/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "midifile.h"

#include "global/containers.h"
#include "global/serialization/binaryreader.h"
#include "global/types/expected.h"
#include "global/types/string.h"

#include "global/log.h"

using namespace mu::engraving;
using namespace muse;

namespace mu::iex::midi {
namespace {
// using ErrorPtr = std::unique_ptr<class Error>;

// class Error
// {
// public:
//     static ErrorPtr create(String message, ErrorPtr cause = {})
//     {
//         return std::make_unique<Error>(std::move(message), std::move(cause));
//     }

//     explicit Error(String message, std::unique_ptr<Error> cause = {})
//         : m_message{std::move(message)}, m_cause{std::move(cause)} {}

//     const String& message() const { return m_message; }

//     const ErrorPtr& cause() const { return m_cause; }

// private:
//     String m_message;
//     ErrorPtr m_cause;
// };

template<typename T>
using Result = ::muse::Expected<T, String>;

// helper template to enable overloaded lambdas
template<class ... Ts>
struct Overloaded : Ts ... {
    using Ts::operator() ...;
};
// explicit deduction guide (not needed as of C++20)
template<class ... Ts>
Overloaded(Ts ...)->Overloaded<Ts...>;

struct ChunkHeader {
    std::array<std::uint8_t, 4> type = {};
    std::uint32_t sizeInBytes = 0;
};

constexpr std::array<std::uint8_t, 4> HEADER_CHUNK_TYPE = { 'M', 'T', 'h', 'd' };
constexpr std::uint32_t HEADER_CHUNK_SIZE_IN_BYTES = 6;
constexpr std::array<std::uint8_t, 4> TRACK_CHUNK_TYPE = { 'M', 'T', 'r', 'k' };

struct TicksPerQuarterNote {
    std::uint16_t value = 0;
};

struct TicksPerFrame {
    std::uint8_t framesPerSecond = 0;
    std::uint8_t ticksPerFrame = 0;
};

using Division = std::variant<TicksPerQuarterNote, TicksPerFrame>;

Division toDivision(const std::uint16_t division)
{
    // ================ Read MIDI division =================
    //
    //                        2 bytes
    //  +-------+---+-------------------+-----------------+
    //  |  bit  |15 | 14              8 | 7             0 |
    //  +-------+---+-------------------------------------+
    //  |       | 0 |       ticks per quarter note        |
    //  | value +---+-------------------------------------+
    //  |       | 1 |  -frames/second   |   ticks/frame   |
    //  +-------+---+-------------------+-----------------+

    if (division & 0x8000) {
        const auto negativeFps = static_cast<std::int8_t>((division & 0xff00) >> 8);
        const auto framesPerSecond = static_cast<std::uint8_t>(-negativeFps);
        const auto ticksPerFrame = static_cast<std::uint8_t>(division & 0x00ff);

        return TicksPerFrame{ framesPerSecond, ticksPerFrame };
    }

    return TicksPerQuarterNote{ division };
}

struct HeaderChunk {
    MidiFile::Format format = {};
    std::uint16_t numTracks = 0;
    Division division = {};
};

bool isStatusByte(const std::uint8_t b)
{
    return b & 0x80;
}

bool isChannelStatusByte(const std::uint8_t b)
{
    return isStatusByte(b) && b <= 0xe0;
}

bool isDataByte(const std::uint8_t b)
{
    return !isStatusByte(b);
}

class MidiFileReader
{
public:
    explicit MidiFileReader(io::IODevice* in)
        : m_in{in} {}

    Result<MidiFile> read()
    {
        Result<HeaderChunk> headerChunk = readHeaderChunk()
                                          .transform_error([](String error) {
            return String(u"expected valid header chunk: %1").arg(std::move(error));
        });
        if (!headerChunk) {
            return Unexpected(std::move(headerChunk).error());
        }

        bool isDivisionInTps = false;
        int division = 0;
        std::visit(Overloaded { [&](const TicksPerQuarterNote& d) {
                division = d.value;
            }, [&](const TicksPerFrame& d) {
                isDivisionInTps = true;
                if (d.framesPerSecond == 29) {
                    division = std::round(d.ticksPerFrame * 29.97);
                } else {
                    division = d.ticksPerFrame * d.framesPerSecond;
                }
            } }, headerChunk->division);

        switch (headerChunk->format) {
        case MidiFile::Format::SingleTrack: {
            Result<MidiTrack> track = readTrack();
            if (!track) {
                return Unexpected(String(u"expected track: &1").arg(std::move(track).error()));
            }

            return MidiFile{ std::vector{ std::move(track).value() }, division, isDivisionInTps };
        }

        case MidiFile::Format::SimultaneousMultiTrack: {
            std::vector<MidiTrack> tracks;
            for (int i = 0; i < headerChunk->numTracks; i++) {
                Result<MidiTrack> track = readTrack();
                if (!track) {
                    return Unexpected(String(u"expected track: &1").arg(std::move(track).error()));
                }
                tracks.push_back(std::move(track).value());
            }

            return MidiFile{ std::move(tracks), division, isDivisionInTps };
        }

        case MidiFile::Format::IndependentMultiTrack:
            return Unexpected(String(u"midi file format %1 not implemented").arg(static_cast<int>(headerChunk->format)));
        }

        UNREACHABLE;

        return Unexpected(String(u"unreachable"));
    }

    Result<HeaderChunk> readHeaderChunk()
    {
        // === Read header_chunk = "MThd" + <header_length> + <format> + <n> + <division>
        //
        // "MThd" 4 bytes
        //    the literal string MThd, or in hexadecimal notation: 0x4d546864.
        //    These four characters at the start of the MIDI file
        //    indicate that this is a MIDI file.
        // <header_length> 4 bytes
        //    length of the header chunk (always =6 bytes long - the size of the next
        //    three fields which are considered the header chunk).
        //    Although the header chunk currently always contains 6 bytes of data,
        //    this should not be assumed, this value should always be read and acted upon,
        //    to allow for possible future extension to the standard.
        // <format> 2 bytes
        //    0 = single track file format
        //    1 = multiple track file format
        //    2 = multiple song file format (i.e., a series of type 0 files)
        // <n> 2 bytes
        //    number of track chunks that follow the header chunk
        // <division> 2 bytes
        //    unit of time for delta timing. If the value is positive, then it represents
        //    the units per beat. For example, +96 would mean 96 ticks per beat.
        //    If the value is negative, delta times are in SMPTE compatible units.

        const auto validateChunkInfo = [](const ChunkHeader& info) -> Result<ChunkHeader> {
            if (info.type != HEADER_CHUNK_TYPE) {
                return Unexpected(String(u"not a header chunk"));
            }
            if (info.sizeInBytes != HEADER_CHUNK_SIZE_IN_BYTES) {
                return Unexpected(String(u"unexpected header chunk size"));
            }

            return info;
        };

        return readChunkHeader()
               .and_then(validateChunkInfo)
               .and_then([&](const ChunkHeader&) {
            return m_in.readUInt16BE()
                   .transform_error(&String::fromStdString)
                   .and_then(&toFileFormat)
                   .and_then([&](const MidiFile::Format format) {
                return m_in.readUInt16BE()
                       .transform_error(&String::fromStdString)
                       .and_then([&](const std::uint16_t numTracks) {
                    return m_in.readUInt16BE()
                           .transform_error(&String::fromStdString)
                           .transform(&toDivision).transform([&](const Division& division) {
                        return HeaderChunk { format, numTracks, division };
                    });
                });
            });
        });
    }

    Result<MidiTrack> readTrack()
    {
        // skip alien chunks until we find a track chunk or EOF is reached
        Result<ChunkHeader> header = readChunkHeader();
        while (header && header->type != TRACK_CHUNK_TYPE) {
            const qint64 numSkipped = m_in.device()->seek(m_in.device()->pos() + header->sizeInBytes);
            if (numSkipped != header->sizeInBytes) {
                return Unexpected(String(u"failed to skip unrecognized chunk"));
            }

            header = readChunkHeader();
        }
        if (!header) {
            throw header.error().toQString();
        }

        // TODO: MidiTrackReader
        m_status = -1;
        m_click = 0;

        MidiTrack track{};
        track.setOutPort(0);
        track.setOutChannel(-1);

        // TODO: check track chunk boundaries
        for (Result<MidiEvent> event = readTrackEvent(); event; event = readTrackEvent()) {
            if ((event->type() == ME_META) && (event->metaType() == META_EOT)) {
                return track;
            }

            track.insert(m_click, std::move(event).value());
        }

        return Unexpected(String(u"expected end of track meta event"));
    }

    Result<MidiEvent> readTrackEvent()
    {
        return readVarInt()
               .and_then([&](const std::int32_t deltaTime) {
            m_click += deltaTime;

            return m_in.readByte()
                   .transform_error(&String::fromStdString)
                   .and_then([&](const std::uint8_t firstByte) -> Result<MidiEvent> {
                // TODO: these should not be treated the same
                if (firstByte == ME_SYSEX || firstByte == ME_ENDSYSEX) {
                    m_status = -1;

                    return readSysExEvent();
                }

                if (firstByte == ME_META) {
                    m_status = -1;

                    return readMetaEvent();
                }

                if (isChannelStatusByte(firstByte) || isDataByte(firstByte)) {
                    if (isChannelStatusByte(firstByte)) {
                        m_status = firstByte;
                    } else if (isDataByte(firstByte) && m_status == -1) {
                        if (m_status == -1) {
                            LOGD("readEvent: no running status, read 0x%02x", firstByte);

                            return Unexpected(String(u"no running status"));
                        }
                    }

                    return readMidiEvent(firstByte, m_status);
                }

                return Unexpected(String(u"Unknown track event: %s")
                                  .arg(firstByte & 0xff));
            });
        });
    }

    Result<MidiEvent> readMidiEvent(const std::uint8_t firstByte, const int status)
    {
        const std::uint8_t dataA = isDataByte(firstByte) ? firstByte : m_in.readByte().value_or(0);

        const auto type = static_cast<std::uint8_t>(status & 0xf0);
        const auto channel = static_cast<std::uint8_t>(status & 0x0f);

        std::uint8_t dataB = 0;
        switch (type) {
        case ME_NOTEOFF:
        case ME_NOTEON:
        case ME_POLYAFTER:
        case ME_CONTROLLER:
        case ME_PITCHBEND:
            dataB = m_in.readByte().value_or(0);
            break;
        }

        MidiEvent event {};
        event.setType(type);
        event.setChannel(channel);

        switch (type) {
        case ME_NOTEOFF:
        case ME_NOTEON:
        case ME_PITCHBEND:
            event.setDataA(dataA & 0x7f);
            event.setDataB(dataB & 0x7f);
            break;
        case ME_POLYAFTER:
            event.setType(ME_CONTROLLER);
            event.setController(CTRL_POLYAFTER);
            event.setValue(((dataA & 0x7f) << 8) + (dataB & 0x7f));
            break;
        case ME_CONTROLLER:
            event.setController(dataA & 0x7f);
            event.setValue(dataB & 0x7f);
            break;
        case ME_PROGRAM:
            event.setValue(dataA & 0x7f);
            break;
        case ME_AFTERTOUCH:
            event.setType(ME_CONTROLLER);
            event.setController(CTRL_PRESS);
            event.setValue(dataA & 0x7f);
            break;
        default:
            UNREACHABLE;
            return Unexpected(String(u"unreachable"));
        }

        if (isStatusByte(dataA) || isStatusByte(dataB)) {
            return Unexpected(String(u"expected data byte, got status byte A = %1, B = %2")
                              .arg(dataA & 0xff, dataB & 0xff));
        }

        return event;
    }

    Result<MidiEvent> readMetaEvent()
    {
        return m_in.readByte()
               .transform_error(&String::fromStdString)
               .and_then([&](const std::uint8_t type) {
            return readVarInt()
                   .and_then([&](const std::uint32_t numDataBytes) {
                return m_in.readNBytes(numDataBytes)
                       .transform_error(&String::fromStdString)
                       .transform([&](std::vector<std::uint8_t> data) {
                    MidiEvent event {};
                    event.setType(ME_META);
                    event.setMetaType(type);
                    event.setLen(static_cast<int>(numDataBytes));
                    event.setEData(std::move(data));

                    return event;
                });
            });
        });
    }

    Result<MidiEvent> readSysExEvent()
    {
        return readVarInt()
               .and_then([&](const std::uint32_t numDataBytes) {
            return m_in.readNBytes(numDataBytes)
                   .transform([&](std::vector<std::uint8_t> data) {
                int len = static_cast<int>(numDataBytes);
                if (data[numDataBytes - 1] != ME_ENDSYSEX) {
                    LOGD("SYSEX does not end with 0xf7!");
                    // more to come?
                } else {
                    len--;             // don't count 0xf7
                }

                MidiEvent event {};
                event.setType(ME_SYSEX);
                event.setEData(std::move(data));
                event.setLen(len);

                return event;
            }).transform_error(&String::fromStdString);
        });
    }

    Result<ChunkHeader> readChunkHeader()
    {
        return m_in.readNBytes<4>()
               .and_then([&](const std::array<std::uint8_t, 4>& type) {
            return m_in.readUInt32BE()
                   .transform([&](const std::uint32_t sizeInBytes) {
                return ChunkHeader { type, sizeInBytes };
            });
        }).transform_error(&String::fromStdString);
    }

    // Read variable-length number (7 bits per byte, MSB first)
    // 0x0FFFFFFF is the maximum value
    Result<std::int32_t> readVarInt()
    {
        constexpr int MAX_BYTES = 4;

        std::int32_t value = 0;
        for (int i = 0; i < MAX_BYTES; i++) {
            BinaryReader::Result<std::uint8_t> b = m_in.readByte();
            if (!b) {
                return Unexpected(String::fromStdString(std::move(b).error()));
            }

            value += (*b & 0x7f);
            if (!(*b & 0x80)) {
                return value;
            }
            value <<= 7;
        }

        return Unexpected(String(u"invalid variable-length quantity"));
    }

private:
    BinaryReader m_in;
    int m_click;
    int m_status;

    static Result<MidiFile::Format> toFileFormat(const std::uint16_t format)
    {
        if (format > 2) {
            return Unexpected(String(u"unknown file format"));
        }

        return MidiFile::Format { format };
    }
};
}

static const uchar gmOnMsg[] = {
    0x7e,         // Non-Real Time header
    0x7f,         // ID of target device (7f = all devices)
    0x09,
    0x01
};
static const uchar gsOnMsg[] = {
    0x41,         // roland id
    0x10,         // Id of target device (default = 10h for roland)
    0x42,         // model id (42h = gs devices)
    0x12,         // command id (12h = data set)
    0x40,         // address & value
    0x00,
    0x7f,
    0x00,
    0x41          // checksum?
};
static const uchar xgOnMsg[] = {
    0x43,         // yamaha id
    0x10,         // device number (0)
    0x4c,         // model id
    0x00,         // address (high, mid, low)
    0x00,
    0x7e,
    0x00          // data
};

const int gmOnMsgLen = sizeof(gmOnMsg);
const int gsOnMsgLen = sizeof(gsOnMsg);
const int xgOnMsgLen = sizeof(xgOnMsg);

//---------------------------------------------------------
//   write
//    returns true on error
//---------------------------------------------------------

bool MidiFile::write(QIODevice* out)
{
    m_fp = out;
    write("MThd", 4);
    writeLong(6);                   // header len
    writeShort(static_cast<int>(m_format));
    writeShort(static_cast<int>(m_tracks.size()));
    writeShort(m_division);
    for (const auto& t: m_tracks) {
        if (writeTrack(t)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void MidiFile::writeEvent(const MidiEvent& event)
{
    switch (event.type()) {
    case ME_NOTEON:
        writeStatus(ME_NOTEON, event.channel());
        put(event.pitch());
        put(event.velo());
        break;

    case ME_NOTEOFF:
        writeStatus(ME_NOTEOFF, event.channel());
        put(event.pitch());
        put(event.velo());
        break;

    case ME_PITCHBEND:
        writeStatus(ME_PITCHBEND, event.channel());
        put(event.dataA());
        put(event.dataB());
        break;

    case ME_CONTROLLER:
        switch (event.controller()) {
        case CTRL_PROGRAM:
            writeStatus(ME_PROGRAM, event.channel());
            put(event.value() & 0x7f);
            break;
        case CTRL_PRESS:
            writeStatus(ME_AFTERTOUCH, event.channel());
            put(event.value() & 0x7f);
            break;
        default:
            writeStatus(ME_CONTROLLER, event.channel());
            put(event.controller());
            put(event.value() & 0x7f);
            break;
        }
        break;

    case ME_META:
        put(ME_META);
        put(event.metaType());
        // Don't null terminate text meta events
        if (event.metaType() >= 0x1 && event.metaType() <= 0x14) {
            putvl(event.len() - 1);
            write(event.edata(), event.len() - 1);
        } else {
            putvl(event.len());
            write(event.edata(), event.len());
        }
        resetRunningStatus();               // really ?!
        break;

    case ME_SYSEX:
        put(ME_SYSEX);
        putvl(event.len() + 1);            // including 0xf7
        write(event.edata(), event.len());
        put(ME_ENDSYSEX);
        resetRunningStatus();
        break;
    }
}

//---------------------------------------------------------
//   writeTrack
//---------------------------------------------------------

bool MidiFile::writeTrack(const MidiTrack& t)
{
    write("MTrk", 4);
    qint64 lenpos = m_fp->pos();
    writeLong(0);                   // dummy len

    m_status   = -1;
    int tick = 0;
    for (const auto& i : t.events()) {
        int ntick = i.first;
        putvl(ntick - tick);        // write tick delta
        //
        // if track channel != -1, then use this
        //    channel for all events in this track
        //
        if (t.outChannel() != -1) {
            writeEvent(i.second);
        }
        tick = ntick;
    }

    //---------------------------------------------------
    //    write "End Of Track" Meta
    //    write Track Len
    //

    putvl(1);
    put(0xff);          // Meta
    put(0x2f);          // EOT
    putvl(0);           // len 0
    qint64 endpos = m_fp->pos();
    m_fp->seek(lenpos);
    writeLong(endpos - lenpos - 4); // tracklen
    m_fp->seek(endpos);
    return false;
}

//---------------------------------------------------------
//   writeStatus
//---------------------------------------------------------

void MidiFile::writeStatus(int nstat, int c)
{
    nstat |= (c & 0xf);
    //
    //  running status; except for Sysex- and Meta Events
    //
    if (((nstat & 0xf0) != 0xf0) && (nstat != m_status)) {
        m_status = nstat;
        put(nstat);
    }
}

muse::Expected<MidiFile, muse::String> MidiFile::read(io::IODevice& in)
{
    MidiFileReader reader{ &in };

    return reader.read();
}

MidiFile::MidiFile(std::vector<MidiTrack> tracks, const int division, const bool isDivisionInTps)
    : m_tracks{std::move(tracks)}, m_division{division}, m_isDivisionInTps{isDivisionInTps}
{
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool MidiFile::write(const void* p, qint64 len)
{
    qint64 rv = m_fp->write((char*)p, len);
    if (rv == len) {
        return false;
    }
    LOGD("write midifile failed: %s", m_fp->errorString().toLatin1().data());
    return true;
}

//---------------------------------------------------------
//   writeShort
//---------------------------------------------------------

void MidiFile::writeShort(int i)
{
    m_fp->putChar(i >> 8);
    m_fp->putChar(i);
}

//---------------------------------------------------------
//   writeLong
//---------------------------------------------------------

void MidiFile::writeLong(int i)
{
    m_fp->putChar(i >> 24);
    m_fp->putChar(i >> 16);
    m_fp->putChar(i >> 8);
    m_fp->putChar(i);
}

/*---------------------------------------------------------
 *    putvl
 *    Write variable-length number (7 bits per byte, MSB first)
 *---------------------------------------------------------*/

void MidiFile::putvl(unsigned val)
{
    unsigned long buf = val & 0x7f;
    while ((val >>= 7) > 0) {
        buf <<= 8;
        buf |= 0x80;
        buf += (val & 0x7f);
    }
    for (;;) {
        put(buf);
        if (buf & 0x80) {
            buf >>= 8;
        } else {
            break;
        }
    }
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

void MidiTrack::insert(int tick, const MidiEvent& event)
{
    m_events.insert({ tick, event });
}

//---------------------------------------------------------
//   setOutChannel
//---------------------------------------------------------

void MidiTrack::setOutChannel(int n)
{
    m_outChannel = n;
    if (m_outChannel == 9) {
        m_drumTrack = true;
    }
}

//---------------------------------------------------------
//   mergeNoteOnOffAndFindMidiType
//    - find matching note on / note off events and merge
//      into a note event with tick duration
//    - find MIDI type
//---------------------------------------------------------

void MidiTrack::mergeNoteOnOffAndFindMidiType(MidiType* mt)
{
    std::multimap<int, MidiEvent> el;

    int hbank = 0xff;
    int lbank = 0xff;
    int rpnh  = -1;
    int rpnl  = -1;
    int datah = 0;
    int datal = 0;
    int dataType = 0;   // 0 : disabled, 0x20000 : rpn, 0x30000 : nrpn;

    for (auto i = m_events.begin(); i != m_events.end(); ++i) {
        MidiEvent& ev = i->second;
        if (ev.type() == ME_INVALID) {
            continue;
        }
        if ((ev.type() != ME_NOTEON) && (ev.type() != ME_NOTEOFF)) {
            if (ev.type() == ME_CONTROLLER) {
                int val  = ev.value();
                int cn   = ev.controller();
                if (cn == CTRL_HBANK) {
                    hbank = val;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LBANK) {
                    lbank = val;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_HDATA) {
                    datah = val;

                    // check if a CTRL_LDATA follows
                    // e.g. wie have a 14 bit controller:
                    auto ii = i;
                    ++ii;
                    bool found = false;
                    for (; ii != m_events.end(); ++ii) {
                        MidiEvent& ev1 = ii->second;
                        if (ev1.type() == ME_CONTROLLER) {
                            if (ev1.controller() == CTRL_LDATA) {
                                // handle later
                                found = true;
                            }
                            break;
                        }
                    }
                    if (!found) {
                        if (rpnh == -1 || rpnl == -1) {
                            LOGD("parameter number not defined, data 0x%x", datah);
                            ev.setType(ME_INVALID);
                            continue;
                        } else {
                            ev.setController(dataType | (rpnh << 8) | rpnl);
                            ev.setValue(datah);
                        }
                    } else {
                        ev.setType(ME_INVALID);
                        continue;
                    }
                } else if (cn == CTRL_LDATA) {
                    datal = val;

                    if (rpnh == -1 || rpnl == -1) {
                        LOGD("parameter number not defined, data 0x%x 0x%x, tick %d, channel %d",
                             datah, datal, i->first, ev.channel());
                        continue;
                    }
                    // assume that the sequence is always
                    //    CTRL_HDATA - CTRL_LDATA
                    // eg. that LDATA is always send last

                    // 14 Bit RPN/NRPN
                    ev.setController((dataType + 0x30000) | (rpnh << 8) | rpnl);
                    ev.setValue((datah << 7) | datal);
                } else if (cn == CTRL_HRPN) {
                    rpnh = val;
                    dataType = 0x20000;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LRPN) {
                    rpnl = val;
                    dataType = 0x20000;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_HNRPN) {
                    rpnh = val;
                    dataType = 0x30000;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_LNRPN) {
                    rpnl = val;
                    dataType = 0x30000;
                    ev.setType(ME_INVALID);
                    continue;
                } else if (cn == CTRL_PROGRAM) {
                    ev.setValue((hbank << 16) | (lbank << 8) | ev.value());
                    // TODO el.insert(ev);
                    ev.setType(ME_INVALID);
                    continue;
                }
            } else if (ev.type() == ME_SYSEX) {
                int len = ev.len();
                const uchar* buffer = ev.edata();
                if ((len == gmOnMsgLen) && memcmp(buffer, gmOnMsg, gmOnMsgLen) == 0) {
                    *mt = MidiType::GM;
                    ev.setType(ME_INVALID);
                    continue;
                }
                if ((len == gsOnMsgLen) && memcmp(buffer, gsOnMsg, gsOnMsgLen) == 0) {
                    *mt = MidiType::GS;
                    ev.setType(ME_INVALID);
                    continue;
                }
                if ((len == xgOnMsgLen) && memcmp(buffer, xgOnMsg, xgOnMsgLen) == 0) {
                    *mt = MidiType::XG;
                    ev.setType(ME_INVALID);
                    continue;
                }
                if (buffer[0] == 0x43) {            // Yamaha
                    *mt = MidiType::XG;
                    int type   = buffer[1] & 0xf0;
                    if (type == 0x10) {
//TODO                                    if (buffer[1] != 0x10) {
//                                          buffer[1] = 0x10;    // fix to Device 1
//                                          }
                        if ((len == xgOnMsgLen) && memcmp(buffer, xgOnMsg, xgOnMsgLen) == 0) {
                            *mt = MidiType::XG;
                            ev.setType(ME_INVALID);
                            continue;
                        }
                        if (len == 7 && buffer[2] == 0x4c && buffer[3] == 0x08 && buffer[5] == 7) {
                            // part mode
                            // 0 - normal
                            // 1 - DRUM
                            // 2 - DRUM 1
                            // 3 - DRUM 2
                            // 4 - DRUM 3
                            // 5 - DRUM 4
                            if (buffer[6] != 0 && buffer[4] == ev.channel()) {
                                m_drumTrack = true;
                            }
                            ev.setType(ME_INVALID);
                            continue;
                        }
                    }
                }
            }
            el.insert(std::pair<int, MidiEvent>(i->first, ev));
            ev.setType(ME_INVALID);
            continue;
        }
        int tick = i->first;
        if (ev.type() == ME_NOTEOFF || ev.velo() == 0) {
            LOGD("-extra note off at %d", tick);
            ev.setType(ME_INVALID);
            continue;
        }
        MidiEvent note(ME_NOTE, ev.channel(), ev.dataA(), ev.dataB());

        auto k = i;
        ++k;
        for (; k != m_events.end(); ++k) {
            MidiEvent& e = k->second;
            if (e.type() != ME_NOTEON && e.type() != ME_NOTEOFF) {
                continue;
            }
            if ((e.type() == ME_NOTEOFF || (e.type() == ME_NOTEON && e.velo() == 0))
                && (e.pitch() == note.pitch())) {
                int t = k->first - tick;
                if (t <= 0) {
                    t = 1;
                }
                note.setLen(t);
                e.setType(ME_INVALID);
                break;
            }
        }
        if (k == m_events.end()) {
            LOGD("-no note-off for note at %d", tick);
            note.setLen(1);
        }
        el.insert(std::pair<int, MidiEvent>(tick, note));
        ev.setType(ME_INVALID);
    }
    m_events = el;
}

//---------------------------------------------------------
//   separateChannel
//    if a track contains events for different midi channels,
//    then split events into separate tracks
//---------------------------------------------------------

void MidiFile::separateChannel()
{
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        // create a list of channels used in current track
        std::vector<int> channel;
        MidiTrack& midiTrack = m_tracks[i];          // current track
        for (const auto& ie : midiTrack.events()) {
            const MidiEvent& e = ie.second;
            if (e.isChannelEvent() && !muse::contains(channel, static_cast<int>(e.channel()))) {
                channel.push_back(e.channel());
            }
        }
        midiTrack.setOutChannel(channel.empty() ? 0 : channel[0]);
        size_t nn = channel.size();
        if (nn <= 1) {
            continue;
        }
        std::sort(channel.begin(), channel.end());
        // -- split --
        // insert additional tracks, assign to them found channels
        for (size_t ii = 1; ii < nn; ++ii) {
            MidiTrack t;
            t.setOutChannel(channel[ii]);
            m_tracks.insert(m_tracks.begin() + i + ii, t);
        }

        //! NOTE: Midi track memory area may be invalid after inserting new elements into tracks
        //!       Let's get the actual track data again
        MidiTrack& actualMidiTrack = m_tracks[i];

        // extract all different channel events from current track to inserted tracks
        for (auto ie = actualMidiTrack.events().begin(); ie != actualMidiTrack.events().end();) {
            const MidiEvent& e = ie->second;
            if (e.isChannelEvent()) {
                int ch  = e.channel();
                size_t idx = muse::indexOf(channel, ch);
                MidiTrack& t = m_tracks[i + idx];
                if (&t != &actualMidiTrack) {
                    t.insert(ie->first, e);
                    ie = actualMidiTrack.events().erase(ie);
                    continue;
                }
            }
            ++ie;
        }
        i += nn - 1;
    }
}
} // namespace mu::iex::midi
