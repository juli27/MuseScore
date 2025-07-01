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
#pragma once

#include <cstdint>

#include "global/serialization/binaryreader.h"

#include "midievent.h"
#include "midifile.h"

namespace muse::io {
class IODevice;
}

namespace mu::iex::midi {
class MidiFileReader
{
public:
    using ErrCode = MidiFile::ErrCode;
    using Error = MidiFile::Error;
    template<typename T>
    using Result = MidiFile::Result<T>;

    explicit MidiFileReader(muse::io::IODevice*);

    Result<MidiFile> read();

private:
    struct ChunkHeader {
        MidiFile::ChunkType chunkType = {};
        std::uint32_t dataSizeInBytes = 0;
    };

    muse::BinaryReader m_in;
    int m_currentTicks = 0;
    std::optional<std::uint8_t> m_runningStatus;
    std::optional<std::uint8_t> m_backupRunningStatus;

    Result<std::vector<MidiTrack> > readTracks(std::uint16_t numTracks);
    Result<MidiTrack> readTrack(const ChunkHeader&);

    Result<MidiEvent> readTrackEvent();
    Result<MidiEvent> readMidiEvent(std::uint8_t status);
    Result<MidiEvent> readMidiEvent(std::uint8_t status, std::uint8_t firstDataByte);
    Result<MidiEvent> readMetaEvent();
    Result<MidiEvent> readSysExEvent();

    Result<std::uint8_t> readDataByte();
    Result<std::int32_t> readVarInt();

    struct HeaderData {
        MidiFileFormat format = {};
        std::uint16_t numTracks = 0;
        Division division = {};
    };

    Result<HeaderData> readHeader(const ChunkHeader&);
    Result<MidiFileFormat> toFormat(std::uint16_t);

    Result<ChunkHeader> readChunkHeader();

    Error makeError(ErrCode);
    Error mapError(const muse::BinaryReader::Error&);
};
} // namespace mu::iex::midi
