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

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include <QIODevice>

#include "global/io/iodevice.h"
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

enum class MidiFileFormat : std::uint16_t {
    SingleTrack = 0,
    SimultaneousMultiTrack = 1,
    IndependentMultiTrack = 2,
};

constexpr std::optional<MidiFileFormat> toFileFormat(const std::uint16_t format)
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

class MidiFile
{
    QIODevice* fp = nullptr;
    std::vector<MidiTrack> _tracks;
    int _division = 0;
    bool _isDivisionInTps = false; ///< ticks per second, alternative - ticks per beat
    MidiFileFormat _format = MidiFileFormat::SimultaneousMultiTrack;
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
    enum class ReadErrCode : std::uint8_t {
        IoError,
        MalformedFile,
    };

    struct ReadError {
        ReadErrCode code;
        std::size_t devicePos;
        muse::String message;
    };

    using ReadResult = muse::Expected<MidiFile, ReadError>;

    static ReadResult read(muse::io::IODevice&);

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
