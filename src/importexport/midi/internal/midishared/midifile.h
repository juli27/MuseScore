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
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include <QIODevice>

#include "global/functional.h"
#include "global/types/expected.h"
#include "global/types/string.h"

#include "midievent.h"

namespace mu::iex::midi {
class MidiTrack
{
    std::multimap<int, MidiEvent> _events;
    int _outChannel = -1;
    int _outPort = -1;
    bool _drumTrack = false;

public:
    MidiTrack() = default;

    const std::multimap<int, MidiEvent>& events() const { return _events; }
    std::multimap<int, MidiEvent>& events() { return _events; }

    int outChannel() const { return _outChannel; }
    void setOutChannel(int n);
    int outPort() const { return _outPort; }
    void setOutPort(int n) { _outPort = n; }

    bool drumTrack() const { return _drumTrack; }

    void insert(int tick, const MidiEvent&);
    void mergeNoteOnOff();
};

// Spec: "Only three values of <format> are specified:
//      - 0 the file contains a single multi-channel track
//      - 1 the file contains one or more simultaneous tracks (or MIDI outputs) of a sequence
//      - 2 the file contains one or more sequentially independent single-track patterns
enum class MidiFileFormat : std::uint16_t {
    SingleTrack = 0,
    SimultaneousTracks = 1,
    IndependentTracks = 2,
};

constexpr std::optional<MidiFileFormat> toMidiFileFormat(const std::uint16_t format)
{
    if (format > 2) {
        return std::nullopt;
    }

    return MidiFileFormat{ format };
}

constexpr std::uint16_t toMidiData(const MidiFileFormat format)
{
    return static_cast<std::uint16_t>(format);
}

struct TicksPerQuarterNote {
    std::uint16_t value = 0;
};

struct TicksPerFrame {
    std::uint8_t framesPerSecond = 0;
    std::uint8_t ticksPerFrame = 0;
};

// Spec:
//                        2 bytes
//  +-------+---+-------------------+-----------------+
//  |  bit  |15 | 14              8 | 7             0 |
//  +-------+---+-------------------------------------+
//  |       | 0 |       ticks per quarter note        |
//  | value +---+-------------------------------------+
//  |       | 1 |  -frames/second   |   ticks/frame   |
//  +-------+---+-------------------+-----------------+
using Division = std::variant<TicksPerQuarterNote, TicksPerFrame>;

constexpr Division toDivision(const std::uint16_t division)
{
    if (division & 0x8000) {
        const auto negativeFps = static_cast<std::int8_t>((division & 0xff00) >> 8);
        const auto framesPerSecond = static_cast<std::uint8_t>(-negativeFps);
        const auto ticksPerFrame = static_cast<std::uint8_t>(division & 0x00ff);

        return TicksPerFrame{ framesPerSecond, ticksPerFrame };
    }

    return TicksPerQuarterNote{ division };
}

constexpr std::uint16_t toMidiData(const Division division)
{
    const auto visitor = muse::Overloaded {
        [](const TicksPerQuarterNote t) -> std::uint16_t { return t.value; },
        [](const TicksPerFrame t) -> std::uint16_t { return ((-t.framesPerSecond) << 8) | t.ticksPerFrame; }
    };

    return std::visit(visitor, division);
}

class MidiFile
{
    QIODevice* fp = nullptr;
    std::vector<MidiTrack> _tracks;
    int _division = 0;
    bool _isDivisionInTps = false; ///< ticks per second, alternative - ticks per beat
    MidiFileFormat _format = MidiFileFormat::SimultaneousTracks;
    bool _noRunningStatus = false;       ///< do not use running status on output

    // values used during write()
    int status = 0;    ///< running status

    void writeEvent(const MidiEvent& event);

protected:
    // write
    bool write(const void*, qint64);
    void writeShort(int);
    void writeLong(int);
    bool writeTrack(const MidiTrack&);
    void putvl(unsigned);
    void put(unsigned char c) { write(&c, 1); }
    void writeStatus(int type, int channel);

    void resetRunningStatus() { status = -1; }

public:
    using ChunkType = std::array<std::uint8_t, 4>;
    inline static constexpr ChunkType HEADER_CHUNK_TYPE = { 'M', 'T', 'h', 'd' };
    inline static constexpr ChunkType TRACK_CHUNK_TYPE = { 'M', 'T', 'r', 'k' };

    enum class ErrCode : std::uint8_t {
        IoError,
        EndOfFile,
        UnsupportedFileFormat,
        InvalidChunkType,
        InvalidChunkSize,
        NoTracks,
        EmptyTrack,
        NoRunningStatus,
        InvalidVarInt,
        InvalidDataByte,
        InvalidStatusByte,
    };

    static std::string_view getName(ErrCode);

    struct Error {
        ErrCode code;
        std::size_t devicePos;

        muse::String toString() const;
    };

    template<typename T>
    using Result = muse::Expected<T, Error>;

    MidiFile() = default;
    MidiFile(std::vector<MidiTrack>, int division, bool isDivisionInTps);

    bool write(QIODevice*);

    std::vector<MidiTrack>& tracks() { return _tracks; }
    const std::vector<MidiTrack>& tracks() const { return _tracks; }

    MidiFileFormat format() const { return _format; }
    void setFormat(MidiFileFormat fmt) { _format = fmt; }

    int division() const { return _division; }
    bool isDivisionInTps() const { return _isDivisionInTps; }
    void setDivision(int val) { _division = val; }
    void separateChannel();
};
}
