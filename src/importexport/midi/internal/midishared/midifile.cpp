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

#include <array>
#include <optional>
#include <tuple>
#include <utility>
#include <variant>

#include "global/containers.h"
#include "global/functional.h"
#include "global/io/iodevice.h"
#include "global/serialization/binaryreader.h"
#include "global/types/expected.h"

#include "global/log.h"

using namespace std::literals;

using namespace mu::engraving;
using namespace muse;

namespace mu::iex::midi {
namespace {
using ChunkType = std::array<std::uint8_t, 4>;
constexpr ChunkType HEADER_CHUNK_TYPE = { 'M', 'T', 'h', 'd' };
constexpr ChunkType TRACK_CHUNK_TYPE = { 'M', 'T', 'r', 'k' };

struct ChunkHeader {
    ChunkType chunkType = {};
    std::uint32_t dataSizeInBytes = 0;
};

struct TicksPerQuarterNote {
    std::uint16_t value = 0;
};

struct TicksPerFrame {
    std::uint8_t framesPerSecond = 0;
    std::uint8_t ticksPerFrame = 0;
};

using Division = std::variant<TicksPerQuarterNote, TicksPerFrame>;

constexpr Division toDivision(const std::uint16_t division)
{
    // Spec:
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

struct HeaderData {
    MidiFileFormat format = {};
    std::uint16_t numTracks = 0;
    Division division = {};
};

bool isStatusByte(const std::uint8_t b)
{
    return b & 0x80;
}

bool isDataByte(const std::uint8_t b)
{
    return !isStatusByte(b);
}

bool isChannelStatusByte(const std::uint8_t b)
{
    return isStatusByte(b) && b < 0xf0;
}

class MidiFileReader
{
public:
    using ErrCode = MidiFile::ErrCode;
    using Error = MidiFile::Error;
    template<typename T>
    using Result = MidiFile::Result<T>;

    explicit MidiFileReader(io::IODevice* in)
        : m_in{in} {}

    Result<MidiFile> read()
    {
        // Spec: "A MIDI file always starts with a header chunk, ..."
        Result<HeaderData> headerData = readChunkHeader()
                                        .and_then(bindThis(&MidiFileReader::readHeader, this));
        RETURN_UNEXPECTED(headerData);

        // TODO: read tracks anyway? provide user with choice?
        if (headerData->format == MidiFileFormat::IndependentMultiTrack) {
            return Unexpected(emitError(ErrCode::UnsupportedFileFormat));
        }

        // Spec "... and is followed by one or more track chunks."
        Result<std::vector<MidiTrack> > tracks = readTracks();
        RETURN_UNEXPECTED(tracks);

        if (tracks->empty()) {
            return Unexpected(emitError(ErrCode::NoTracks));
        }

        if (tracks->size() != headerData->numTracks) {
            LOGW() << "expected " << headerData->numTracks << "tracks, but found " << tracks->size();
        }

        const auto convertDivision = Overloaded {
            [](const TicksPerQuarterNote& d) { return std::tuple { d.value, false }; },
            [](const TicksPerFrame& d) {
                const std::uint16_t division = d.framesPerSecond == 29
                                               ? std::round(d.ticksPerFrame * 29.97)
                                               : d.ticksPerFrame * d.framesPerSecond;

                return std::tuple { division, true };
            } };

        // TODO: use Division type in MidiFile instead of converting here
        const auto [division, isDivisionInTps] = std::visit(convertDivision, headerData->division);

        return MidiFile { std::move(*tracks), division, isDivisionInTps };
    }

private:
    BinaryReader m_in;
    int m_click = 0;
    std::optional<std::uint8_t> m_runningStatus;

    Result<std::vector<MidiTrack> > readTracks()
    {
        std::vector<MidiTrack> tracks{};
        while (m_in.device()->isReadable()) {
            // Spec: "Your programs should expect alien chunks and treat them as if they weren't there."
            Result<ChunkHeader> header = readChunkHeader();
            while (header && header->chunkType != TRACK_CHUNK_TYPE) {
                if (m_in.device()->skip(header->dataSizeInBytes) != header->dataSizeInBytes) {
                    // EOF could happen when there's junk (padding) at the end of the file
                    LOGW() << "unexpected EOF while skipping unrecognized chunk";

                    // return what we have so far
                    return tracks;
                }

                header = readChunkHeader();
            }

            if (!header && header.error().code == ErrCode::EndOfFile) {
                const Error& error = header.error();
                // EOF could happen when there's junk (padding) at the end of the file
                LOGW() << "unexpected EOF while reading next chunk header at " << error.devicePos;

                // return what we have so far
                return tracks;
            }
            RETURN_UNEXPECTED(header);

            Result<MidiTrack> track = readTrack(header.value());
            RETURN_UNEXPECTED(track);

            tracks.push_back(std::move(track.value()));
        }

        return tracks;
    }

    Result<MidiTrack> readTrack(const ChunkHeader& chunkHeader)
    {
        // Spec: "<Track Chunk> = 'MTrk' <length> <MTrk event>+"

        if (chunkHeader.chunkType != TRACK_CHUNK_TYPE) {
            return Unexpected(emitError(ErrCode::InvalidChunkType));
        }

        m_runningStatus.reset();
        m_click = 0;

        MidiTrack track{};
        track.setOutPort(0);

        const std::size_t startPos = m_in.device()->pos();
        const auto hasMoreData = [&]() -> bool {
            io::IODevice* device = m_in.device();
            const std::size_t endPos = startPos + chunkHeader.dataSizeInBytes;

            return device->isReadable() && (device->pos() < endPos);
        };

        while (hasMoreData()) {
            Result<MidiEvent> event = readTrackEvent();
            RETURN_UNEXPECTED(event);

            track.insert(m_click, std::move(*event));

            if ((event->type() == ME_META) && (event->metaType() == META_EOT)) {
                break;
            }
        }

        // we could have read more bytes than what is part of this track chunk
        if (m_in.device()->pos() > startPos + chunkHeader.dataSizeInBytes) {
            return Unexpected(emitError(ErrCode::InvalidChunkSize));
        }

        // skip unused bytes in chunk
        const std::size_t numRead = m_in.device()->pos() - startPos;
        const std::size_t numSkipped = m_in.device()->skip(chunkHeader.dataSizeInBytes - numRead);
        if (numSkipped != 0) {
            LOGW() << "track chunk has more data";
        }

        // Spec: "([...] at least one MTrk event must be present)"
        if (track.events().empty()) {
            return Unexpected(emitError(ErrCode::EmptyTrack));
        }

        return track;
    }

    Result<MidiEvent> readTrackEvent()
    {
        // Spec: "<MTrk event> = <delta-time> <event>"
        // Spec: "<event> = <MIDI event> | <sysex event> | <meta-event>"

        Result<std::int32_t> deltaTime = readVarInt();
        RETURN_UNEXPECTED(deltaTime);

        m_click += *deltaTime;

        // status byte or data byte
        Result<std::uint8_t> firstByte = m_in.readByte()
                                         .transform_error(bindThis(&MidiFileReader::mapError, this));
        RETURN_UNEXPECTED(firstByte);

        if (isDataByte(*firstByte)) {
            if (!m_runningStatus) {
                return Unexpected(emitError(ErrCode::NoRunningStatus));
            }

            return readMidiEvent(*m_runningStatus, *firstByte);
        }

        // first byte is status byte

        if (isChannelStatusByte(*firstByte)) {
            m_runningStatus = *firstByte;

            return readMidiEvent(*firstByte);
        }

        if (*firstByte == ME_META) {
            // Spec: "Sysex events and meta-events cancel any running status which was in effect."
            m_runningStatus.reset();

            return readMetaEvent();
        }

        // TODO: these should not be treated the same
        if (*firstByte == ME_SYSEX || *firstByte == ME_ENDSYSEX) {
            // Spec: "Sysex events and meta-events cancel any running status which was in effect."
            m_runningStatus.reset();

            return readSysExEvent();
        }

        return Unexpected(emitError(ErrCode::InvalidStatusByte));
    }

    Result<MidiEvent> readMidiEvent(const std::uint8_t status)
    {
        Result<std::uint8_t> firstDataByte = readDataByte();
        RETURN_UNEXPECTED(firstDataByte);

        return readMidiEvent(status, *firstDataByte);
    }

    Result<MidiEvent> readMidiEvent(const std::uint8_t status, const std::uint8_t firstDataByte)
    {
        const auto type = static_cast<std::uint8_t>(status & 0xf0);
        const auto channel = static_cast<std::uint8_t>(status & 0x0f);

        MidiEvent event {};
        event.setType(type);
        event.setChannel(channel);

        if (type == ME_PROGRAM) {
            event.setValue(firstDataByte);

            return event;
        }

        if (type == ME_AFTERTOUCH) {
            event.setType(ME_CONTROLLER);
            event.setController(CTRL_PRESS);
            event.setValue(firstDataByte);

            return event;
        }

        Result<std::uint8_t> secondDataByte = readDataByte();
        RETURN_UNEXPECTED(secondDataByte);

        switch (type) {
        case ME_NOTEOFF:
        case ME_NOTEON:
        case ME_PITCHBEND:
            event.setDataA(firstDataByte);
            event.setDataB(*secondDataByte);
            break;
        case ME_POLYAFTER:
            event.setType(ME_CONTROLLER);
            event.setController(CTRL_POLYAFTER);
            event.setValue((firstDataByte << 8) + *secondDataByte);
            break;
        case ME_CONTROLLER:
            event.setController(firstDataByte);
            event.setValue(*secondDataByte);
            break;
        default:
            return Unexpected(emitError(ErrCode::InvalidStatusByte));
        }

        return event;
    }

    Result<MidiEvent> readMetaEvent()
    {
        // Spec: "FF <type> <length> <bytes>"

        const auto toReadError = bindThis(&MidiFileReader::mapError, this);

        // Spec: "[...] future meta-events may be designed which may not be known to existing programs,
        //        so programs must properly ignore meta-events which they do not recognize, [...]"
        Result<std::uint8_t> type = m_in.readByte()
                                    .transform_error(toReadError);
        RETURN_UNEXPECTED(type);

        // Spec: "If there is no data, the length is 0."
        Result<std::int32_t> numDataBytes = readVarInt();
        RETURN_UNEXPECTED(numDataBytes);

        Result<std::vector<std::uint8_t> > data = m_in.readNBytes(*numDataBytes)
                                                  .transform_error(toReadError);
        RETURN_UNEXPECTED(data);

        // FIXME: import code expects null termination when data is a string
        data->push_back(0);

        MidiEvent event{};
        event.setType(ME_META);
        event.setMetaType(*type);
        event.setLen(*numDataBytes);
        event.setEData(std::move(*data));

        return event;
    }

    Result<MidiEvent> readSysExEvent()
    {
        Result<std::int32_t> numDataBytes = readVarInt();
        RETURN_UNEXPECTED(numDataBytes);

        Result<std::vector<std::uint8_t> > data = m_in.readNBytes(*numDataBytes)
                                                  .transform_error(bindThis(&MidiFileReader::mapError, this));
        RETURN_UNEXPECTED(data);

        int len = *numDataBytes;
        if (len > 0 && (*data)[len - 1] == ME_ENDSYSEX) {
            // don't count 0xf7
            len--;
        }

        MidiEvent event {};
        event.setType(ME_SYSEX);
        event.setEData(std::move(*data));
        event.setLen(len);

        return event;
    }

    Result<std::uint8_t> readDataByte()
    {
        Result<std::uint8_t> byte = m_in.readByte()
                                    .transform_error(bindThis(&MidiFileReader::mapError, this));
        RETURN_UNEXPECTED(byte);

        if (!isDataByte(*byte)) {
            return Unexpected(emitError(ErrCode::InvalidDataByte));
        }

        return byte;
    }

    // Spec: "Some numbers in MIDI Files are represented in a form called a variable-length quantity."
    //       "These numbers are represented 7 bits per byte, most significant bits first."
    Result<std::int32_t> readVarInt()
    {
        // Spec: "The largest number which is allowed is 0FFFFFFF [...]"
        constexpr int MAX_BYTES = 4;

        std::int32_t value = 0;
        for (int i = 0; i < MAX_BYTES; i++) {
            Result<std::uint8_t> b = m_in.readByte()
                                     .transform_error(bindThis(&MidiFileReader::mapError, this));
            RETURN_UNEXPECTED(b);

            // Spec: "All bytes except the last have bit 7 set, and the last byte has bit 7 clear."
            value += (*b & 0x7f);
            if (!(*b & 0x80)) {
                return value;
            }
            value <<= 7;
        }

        return Unexpected(emitError(ErrCode::InvalidVarInt));
    }

    Result<HeaderData> readHeader(const ChunkHeader& chunkHeader)
    {
        // Spec: "<Header Chunk> = 'MThd' <length> <format> <ntrks> <division>"

        if (chunkHeader.chunkType != HEADER_CHUNK_TYPE) {
            return Unexpected(emitError(ErrCode::InvalidChunkType));
        }

        constexpr std::uint32_t HEADER_CHUNK_SIZE = 6;
        if (chunkHeader.dataSizeInBytes < HEADER_CHUNK_SIZE) {
            return Unexpected(emitError(ErrCode::InvalidChunkSize));
        }

        const auto mapError = bindThis(&MidiFileReader::mapError, this);

        Result<MidiFileFormat> format = m_in.readUInt16BE()
                                        .transform_error(mapError)
                                        .and_then(bindThis(&MidiFileReader::toFormat, this));
        RETURN_UNEXPECTED(format);

        Result<std::uint16_t> numTracks = m_in.readUInt16BE()
                                          .transform_error(mapError);
        RETURN_UNEXPECTED(numTracks);

        Result<Division> division = m_in.readUInt16BE()
                                    .transform_error(mapError)
                                    .transform(&toDivision);
        RETURN_UNEXPECTED(division);

        // Spec: "[...] it is important to read and honor the length, even if it is longer than 6."
        const std::size_t numSkippedBytes = m_in.device()->skip(chunkHeader.dataSizeInBytes - HEADER_CHUNK_SIZE);
        if (numSkippedBytes != 0) {
            LOGW() << "header chunk has more data";
        }

        return HeaderData{ *format, * numTracks, * division };
    }

    Result<ChunkHeader> readChunkHeader()
    {
        const auto mapError = bindThis(&MidiFileReader::mapError, this);

        Result<ChunkType> chunkType = m_in.readNBytes<4>()
                                      .transform_error(mapError);
        RETURN_UNEXPECTED(chunkType);

        Result<std::uint32_t> dataSizeInBytes = m_in.readUInt32BE()
                                                .transform_error(mapError);
        RETURN_UNEXPECTED(dataSizeInBytes);

        return ChunkHeader{ *chunkType, * dataSizeInBytes };
    }

    Error emitError(const ErrCode code)
    {
        return Error{
            code,
            m_in.device()->pos(),
        };
    }

    Error mapError(const BinaryReader::Error& error)
    {
        const ErrCode code = [&] {
            switch (error.code) {
            case BinaryReader::ErrCode::IoError:
                return ErrCode::IoError;

            case BinaryReader::ErrCode::EndOfFile:
                return ErrCode::EndOfFile;

            default:
                return ErrCode::IoError;
            }
        }();

        return Error{ code, error.devicePos };
    }

    Result<MidiFileFormat> toFormat(const std::uint16_t format)
    {
        std::optional<MidiFileFormat> f = toMidiFileFormat(format);
        if (!f.has_value()) {
            return Unexpected(emitError(ErrCode::UnsupportedFileFormat));
        }

        return *f;
    }
};
} // namespace

std::string_view MidiFile::getName(const ErrCode code)
{
    switch (code) {
    case ErrCode::IoError:
        return "IoError"sv;
    case ErrCode::EndOfFile:
        return "EndOfFile"sv;
    case ErrCode::UnsupportedFileFormat:
        return "UnsupportedFileFormat"sv;
    case ErrCode::InvalidChunkType:
        return "InvalidChunkType"sv;
    case ErrCode::InvalidChunkSize:
        return "InvalidChunkSize"sv;
    case ErrCode::NoTracks:
        return "NoTracks"sv;
    case ErrCode::EmptyTrack:
        return "EmptyTrack"sv;
    case ErrCode::NoRunningStatus:
        return "NoRunningStatus"sv;
    case ErrCode::InvalidVarInt:
        return "InvalidVarInt"sv;
    case ErrCode::InvalidDataByte:
        return "InvalidDataByte"sv;
    case ErrCode::InvalidStatusByte:
        return "InvalidStatusByte"sv;
    default:
        UNREACHABLE;
        return "UnknownError"sv;
    }
}

String MidiFile::Error::toString() const
{
    return String(u"%1 at position %2")
           .arg(String::fromUtf8(getName(code)))
           .arg(String::number(devicePos));
}

//---------------------------------------------------------
//   write
//    returns true on error
//---------------------------------------------------------

bool MidiFile::write(QIODevice* out)
{
    fp = out;
    write("MThd", 4);
    writeLong(6);                   // header len
    writeShort(static_cast<int>(_format));
    writeShort(static_cast<int>(_tracks.size()));
    writeShort(_division);
    for (const auto& t: _tracks) {
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
    qint64 lenpos = fp->pos();
    writeLong(0);                   // dummy len

    status   = -1;
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
    qint64 endpos = fp->pos();
    fp->seek(lenpos);
    writeLong(endpos - lenpos - 4); // tracklen
    fp->seek(endpos);
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
    if (_noRunningStatus || (((nstat & 0xf0) != 0xf0) && (nstat != status))) {
        status = nstat;
        put(nstat);
    }
}

MidiFile::Result<MidiFile> MidiFile::read(io::IODevice& in)
{
    TRACEFUNC;
    MidiFileReader reader{ &in };

    return reader.read();
}

MidiFile::MidiFile(std::vector<MidiTrack> tracks, const int division, const bool isDivisionInTps)
    : _tracks{std::move(tracks)}, _division{division}, _isDivisionInTps{isDivisionInTps}
{
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

bool MidiFile::write(const void* p, qint64 len)
{
    qint64 rv = fp->write((char*)p, len);
    if (rv == len) {
        return false;
    }
    LOGD("write midifile failed: %s", fp->errorString().toLatin1().data());
    return true;
}

//---------------------------------------------------------
//   writeShort
//---------------------------------------------------------

void MidiFile::writeShort(int i)
{
    fp->putChar(i >> 8);
    fp->putChar(i);
}

//---------------------------------------------------------
//   writeLong
//---------------------------------------------------------

void MidiFile::writeLong(int i)
{
    fp->putChar(i >> 24);
    fp->putChar(i >> 16);
    fp->putChar(i >> 8);
    fp->putChar(i);
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
    _events.insert({ tick, event });
}

//---------------------------------------------------------
//   setOutChannel
//---------------------------------------------------------

void MidiTrack::setOutChannel(int n)
{
    _outChannel = n;
    if (_outChannel == 9) {
        _drumTrack = true;
    }
}

//---------------------------------------------------------
//   mergeNoteOnOffAndFindMidiType
//    - find matching note on / note off events and merge
//      into a note event with tick duration
//---------------------------------------------------------

void MidiTrack::mergeNoteOnOff()
{
    std::multimap<int, MidiEvent> el;

    int hbank = 0xff;
    int lbank = 0xff;
    int rpnh  = -1;
    int rpnl  = -1;
    int datah = 0;
    int datal = 0;
    int dataType = 0;   // 0 : disabled, 0x20000 : rpn, 0x30000 : nrpn;

    for (auto i = _events.begin(); i != _events.end(); ++i) {
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
                    for (; ii != _events.end(); ++ii) {
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
                // Yamaha
                if (buffer[0] == 0x43) {
                    int type   = buffer[1] & 0xf0;
                    if (type == 0x10) {
                        if (len == 7 && buffer[2] == 0x4c && buffer[3] == 0x08 && buffer[5] == 7) {
                            // part mode
                            // 0 - normal
                            // 1 - DRUM
                            // 2 - DRUM 1
                            // 3 - DRUM 2
                            // 4 - DRUM 3
                            // 5 - DRUM 4
                            if (buffer[6] != 0 && buffer[4] == ev.channel()) {
                                _drumTrack = true;
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
        for (; k != _events.end(); ++k) {
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
        if (k == _events.end()) {
            LOGD("-no note-off for note at %d", tick);
            note.setLen(1);
        }
        el.insert(std::pair<int, MidiEvent>(tick, note));
        ev.setType(ME_INVALID);
    }
    _events = el;
}

//---------------------------------------------------------
//   separateChannel
//    if a track contains events for different midi channels,
//    then split events into separate tracks
//---------------------------------------------------------

void MidiFile::separateChannel()
{
    for (size_t i = 0; i < _tracks.size(); ++i) {
        // create a list of channels used in current track
        std::vector<int> channel;
        MidiTrack& midiTrack = _tracks[i];          // current track
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
            _tracks.insert(_tracks.begin() + i + ii, t);
        }

        //! NOTE: Midi track memory area may be invalid after inserting new elements into tracks
        //!       Let's get the actual track data again
        MidiTrack& actualMidiTrack = _tracks[i];

        // extract all different channel events from current track to inserted tracks
        for (auto ie = actualMidiTrack.events().begin(); ie != actualMidiTrack.events().end();) {
            const MidiEvent& e = ie->second;
            if (e.isChannelEvent()) {
                int ch  = e.channel();
                size_t idx = muse::indexOf(channel, ch);
                MidiTrack& t = _tracks[i + idx];
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
