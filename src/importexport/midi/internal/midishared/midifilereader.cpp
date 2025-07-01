/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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
#include "midifilereader.h"

#include <utility>

#include "global/functional.h"
#include "global/log.h"
#include "global/io/iodevice.h"
#include "global/types/expected.h"

using namespace muse;
using namespace mu::engraving;

namespace mu::iex::midi {
namespace {
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
} // namespace

MidiFileReader::MidiFileReader(muse::io::IODevice* in)
    : m_in{in} {}

MidiFileReader::Result<MidiFile> MidiFileReader::read()
{
    TRACEFUNC;

    // Spec: "A MIDI file always starts with a header chunk, ..."
    Result<HeaderData> headerData = readChunkHeader()
                                    .and_then(muse::bindThis(&MidiFileReader::readHeader, this));
    RETURN_UNEXPECTED(headerData);

    // TODO: read tracks anyway? provide user with choice?
    if (headerData->format == MidiFileFormat::IndependentTracks) {
        return Unexpected(makeError(ErrCode::UnsupportedFileFormat));
    }

    // Spec: "... and is followed by one or more track chunks."
    Result<std::vector<MidiTrack> > tracks = readTracks(headerData->numTracks);
    RETURN_UNEXPECTED(tracks);

    if (tracks->empty()) {
        return Unexpected(makeError(ErrCode::NoTracks));
    }

    if (tracks->size() != headerData->numTracks) {
        LOGW() << "file contains " << tracks->size() << " tracks, but header claims " << headerData->numTracks;
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

MidiFileReader::Result<std::vector<MidiTrack> > MidiFileReader::readTracks(const std::uint16_t numTracks)
{
    std::vector<MidiTrack> tracks{};

    while (m_in.device()->isReadable() && (tracks.size() < numTracks)) {
        // Spec: "Your programs should expect alien chunks and treat them as if they weren't there."
        Result<ChunkHeader> header = readChunkHeader();
        while (header && header->chunkType != MidiFile::TRACK_CHUNK_TYPE) {
            if (m_in.device()->skip(header->dataSizeInBytes) != header->dataSizeInBytes) {
                return Unexpected(makeError(ErrCode::InvalidChunkSize));
            }

            header = readChunkHeader();
        }
        RETURN_UNEXPECTED(header);

        Result<MidiTrack> track = readTrack(*header);
        RETURN_UNEXPECTED(track);

        tracks.push_back(std::move(*track));
    }

    return tracks;
}

MidiFileReader::Result<MidiTrack> MidiFileReader::readTrack(const ChunkHeader& chunkHeader)
{
    // Spec: "<Track Chunk> = 'MTrk' <length> <MTrk event>+"

    if (chunkHeader.chunkType != MidiFile::TRACK_CHUNK_TYPE) {
        return Unexpected(makeError(ErrCode::InvalidChunkType));
    }

    m_runningStatus.reset();
    m_backupRunningStatus.reset();
    m_currentTicks = 0;

    MidiTrack track{};
    track.setOutPort(0);

    const std::size_t startPos = m_in.device()->pos();
    const auto hasMoreData = [&]() -> bool {
        io::IODevice* device = m_in.device();
        const std::size_t endPos = startPos + chunkHeader.dataSizeInBytes;

        return device->isReadable() && (device->pos() < endPos);
    };

    bool hasEndOfTrackEvent = false;
    while (hasMoreData()) {
        Result<MidiEvent> event = readTrackEvent();
        RETURN_UNEXPECTED(event);

        track.insert(m_currentTicks, std::move(*event));

        if ((event->type() == ME_META) && (event->metaType() == META_EOT)) {
            hasEndOfTrackEvent = true;
            break;
        }
    }

    // we could have read more bytes than what is part of this track chunk
    const std::size_t numBytesRead = m_in.device()->pos() - startPos;
    if (numBytesRead > chunkHeader.dataSizeInBytes) {
        return Unexpected(makeError(ErrCode::InvalidChunkSize));
    }

    if (!hasEndOfTrackEvent) {
        LOGW() << "missing EndOfTrack meta event";
    }

    // skip unused bytes in chunk
    if (numBytesRead < chunkHeader.dataSizeInBytes) {
        const std::size_t numExtraBytes = chunkHeader.dataSizeInBytes - numBytesRead;

        LOGW() << "track chunk claims to have " << numExtraBytes << " additional bytes of data";

        const std::size_t numSkipped = m_in.device()->skip(numExtraBytes);
        if (numSkipped != numExtraBytes) {
            return Unexpected(makeError(ErrCode::InvalidChunkSize));
        }
    }

    // Spec: "([...] at least one MTrk event must be present)"
    if (track.events().empty()) {
        return Unexpected(makeError(ErrCode::EmptyTrack));
    }

    return track;
}

MidiFileReader::Result<MidiEvent> MidiFileReader::readTrackEvent()
{
    // Spec: "<MTrk event> = <delta-time> <event>"
    // Spec: "<event> = <MIDI event> | <sysex event> | <meta-event>"

    Result<std::int32_t> deltaTicks = readVarInt();
    RETURN_UNEXPECTED(deltaTicks);

    m_currentTicks += *deltaTicks;

    // status byte or data byte
    Result<std::uint8_t> firstByte = m_in.readByte()
                                     .transform_error(muse::bindThis(&MidiFileReader::mapError, this));
    RETURN_UNEXPECTED(firstByte);

    if (isDataByte(*firstByte)) {
        if (!m_runningStatus) {
            // we tolerate non-standard running status behaviour
            if (!m_backupRunningStatus) {
                return Unexpected(makeError(ErrCode::NoRunningStatus));
            }

            LOGW() << "no running status. Using backup...";
            m_runningStatus = m_backupRunningStatus;
        }

        return readMidiEvent(*m_runningStatus, *firstByte);
    }

    // first byte is status byte

    if (isChannelStatusByte(*firstByte)) {
        m_runningStatus = *firstByte;
        m_backupRunningStatus = *firstByte;

        return readMidiEvent(*firstByte);
    }

    if (*firstByte == ME_META) {
        // Spec: "Sysex events and meta-events cancel any running status which was in effect."
        m_runningStatus.reset();

        return readMetaEvent();
    }

    // TODO: should these be treated the same?
    if (*firstByte == ME_SYSEX || *firstByte == ME_ENDSYSEX) {
        // Spec: "Sysex events and meta-events cancel any running status which was in effect."
        m_runningStatus.reset();

        return readSysExEvent();
    }

    return Unexpected(makeError(ErrCode::InvalidStatusByte));
}

MidiFileReader::Result<MidiEvent> MidiFileReader::readMidiEvent(const uint8_t status)
{
    Result<std::uint8_t> firstDataByte = readDataByte();
    RETURN_UNEXPECTED(firstDataByte);

    return readMidiEvent(status, *firstDataByte);
}

MidiFileReader::Result<MidiEvent> MidiFileReader::readMidiEvent(const uint8_t status, const uint8_t firstDataByte)
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

    const auto canSafelyClampSecondDataByte = [&]() -> bool {
        return type == ME_NOTEOFF || type == ME_NOTEON || type == ME_POLYAFTER
               || (type == ME_CONTROLLER && firstDataByte == CTRL_VOLUME)
               || (type == ME_CONTROLLER && firstDataByte == CTRL_EXPRESSION)
               || (type == ME_CONTROLLER && firstDataByte == CTRL_CHORUS_SEND);
    };

    Result<std::uint8_t> secondDataByte = readDataByte();
    if (!secondDataByte && secondDataByte.error().code == ErrCode::InvalidDataByte
        && canSafelyClampSecondDataByte()) {
        LOGW() << "clamping invalid data byte at " << m_in.device()->pos();

        // some midi files contain invalid data bytes (> 127)
        // clamp them to 127 when it is safe to do so
        secondDataByte = 127;
    }
    RETURN_UNEXPECTED(secondDataByte);

    switch (type) {
    case ME_NOTEOFF:
    case ME_NOTEON:
    case ME_PITCHBEND:
        event.setDataA(firstDataByte);
        event.setDataB(*secondDataByte);
        return event;

    case ME_POLYAFTER:
        event.setType(ME_CONTROLLER);
        event.setController(CTRL_POLYAFTER);
        event.setValue((firstDataByte << 8) + *secondDataByte);
        return event;

    case ME_CONTROLLER:
        event.setController(firstDataByte);
        event.setValue(*secondDataByte);
        return event;

    default:
        break;
    }

    return Unexpected(makeError(ErrCode::InvalidStatusByte));
}

MidiFileReader::Result<MidiEvent> MidiFileReader::readMetaEvent()
{
    // Spec: "FF <type> <length> <bytes>"

    const auto mapError = muse::bindThis(&MidiFileReader::mapError, this);

    // Spec: "[...] future meta-events may be designed which may not be known to existing programs,
    //        so programs must properly ignore meta-events which they do not recognize, [...]"
    Result<std::uint8_t> type = m_in.readByte()
                                .transform_error(mapError);
    RETURN_UNEXPECTED(type);

    // Spec: "If there is no data, the length is 0."
    Result<std::int32_t> dataSizeInBytes = readVarInt();
    RETURN_UNEXPECTED(dataSizeInBytes);

    Result<std::vector<std::uint8_t> > data = m_in.readNBytes(*dataSizeInBytes)
                                              .transform_error(mapError);
    RETURN_UNEXPECTED(data);

    // FIXME: import code assumes null termination when data is a string
    data->push_back(0);

    MidiEvent event{};
    event.setType(ME_META);
    event.setMetaType(*type);
    event.setLen(*dataSizeInBytes);
    event.setEData(std::move(*data));

    return event;
}

MidiFileReader::Result<MidiEvent> MidiFileReader::readSysExEvent()
{
    // Spec: "F0 <length> <bytes to be transmitted after F0>"
    //       "F7 <length> <all bytes to be transmitted>"

    Result<std::int32_t> dataSizeInBytes = readVarInt();
    RETURN_UNEXPECTED(dataSizeInBytes);

    Result<std::vector<std::uint8_t> > data = m_in.readNBytes(*dataSizeInBytes)
                                              .transform_error(muse::bindThis(&MidiFileReader::mapError, this));
    RETURN_UNEXPECTED(data);

    int len = *dataSizeInBytes;
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

MidiFileReader::Result<uint8_t> MidiFileReader::readDataByte()
{
    Result<std::uint8_t> byte = m_in.readByte()
                                .transform_error(muse::bindThis(&MidiFileReader::mapError, this));
    RETURN_UNEXPECTED(byte);

    if (!isDataByte(*byte)) {
        return Unexpected(makeError(ErrCode::InvalidDataByte));
    }

    return byte;
}

MidiFileReader::Result<int32_t> MidiFileReader::readVarInt()
{
    // Spec: "Some numbers in MIDI Files are represented in a form called a variable-length quantity."
    //       "These numbers are represented 7 bits per byte, most significant bits first."
    //       "The largest number which is allowed is 0FFFFFFF [...]"
    constexpr int MAX_BYTES = 4;

    std::int32_t value = 0;
    for (int i = 0; i < MAX_BYTES; i++) {
        Result<std::uint8_t> b = m_in.readByte()
                                 .transform_error(muse::bindThis(&MidiFileReader::mapError, this));
        RETURN_UNEXPECTED(b);

        // Spec: "All bytes except the last have bit 7 set, and the last byte has bit 7 clear."
        value += (*b & 0x7f);
        if (!(*b & 0x80)) {
            return value;
        }
        value <<= 7;
    }

    return Unexpected(makeError(ErrCode::InvalidVarInt));
}

MidiFileReader::Result<MidiFileReader::HeaderData> MidiFileReader::readHeader(const ChunkHeader& chunkHeader)
{
    // Spec: "<Header Chunk> = 'MThd' <length> <format> <ntrks> <division>"

    if (chunkHeader.chunkType != MidiFile::HEADER_CHUNK_TYPE) {
        return Unexpected(makeError(ErrCode::InvalidChunkType));
    }

    constexpr std::uint32_t HEADER_CHUNK_SIZE = 6;
    if (chunkHeader.dataSizeInBytes < HEADER_CHUNK_SIZE) {
        return Unexpected(makeError(ErrCode::InvalidChunkSize));
    }

    const auto mapError = muse::bindThis(&MidiFileReader::mapError, this);

    Result<MidiFileFormat> format = m_in.readUInt16BE()
                                    .transform_error(mapError)
                                    .and_then(muse::bindThis(&MidiFileReader::toFormat, this));
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

    return HeaderData{ *format, *numTracks, *division };
}

MidiFileReader::Result<MidiFileFormat> MidiFileReader::toFormat(const uint16_t f)
{
    std::optional<MidiFileFormat> format = toMidiFileFormat(f);
    if (!format.has_value()) {
        return Unexpected(makeError(ErrCode::UnsupportedFileFormat));
    }

    return *format;
}

MidiFileReader::Result<MidiFileReader::ChunkHeader> MidiFileReader::readChunkHeader()
{
    const auto mapError = muse::bindThis(&MidiFileReader::mapError, this);

    Result<MidiFile::ChunkType> chunkType = m_in.readNBytes<4>()
                                            .transform_error(mapError);
    RETURN_UNEXPECTED(chunkType);

    Result<std::uint32_t> dataSizeInBytes = m_in.readUInt32BE()
                                            .transform_error(mapError);
    RETURN_UNEXPECTED(dataSizeInBytes);

    return ChunkHeader{ *chunkType, *dataSizeInBytes };
}

MidiFileReader::Error MidiFileReader::makeError(const ErrCode code)
{
    return Error{ code, m_in.device()->pos() };
}

MidiFileReader::Error MidiFileReader::mapError(const BinaryReader::Error& error)
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
} // namespace mu::iex::midi
