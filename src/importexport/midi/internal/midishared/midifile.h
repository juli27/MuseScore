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
#include <vector>

#include <QIODevice>

#include "global/io/iodevice.h"
#include "global/types/expected.h"
#include "global/types/string.h"

#include "midievent.h"

namespace mu::iex::midi {
enum class MidiType : char {
    UNKNOWN = 0, GM = 1, GS = 2, XG = 4
};

class MidiTrack
{
public:
    MidiTrack() = default;

    bool empty() const;
    const std::multimap<int, MidiEvent>& events() const { return m_events; }
    std::multimap<int, MidiEvent>& events() { return m_events; }

    int outChannel() const { return m_outChannel; }
    void setOutChannel(int n);
    int outPort() const { return m_outPort; }
    void setOutPort(int n) { m_outPort = n; }

    bool drumTrack() const { return m_drumTrack; }

    void insert(int tick, const MidiEvent&);
    void mergeNoteOnOffAndFindMidiType(MidiType* mt);

private:
    std::multimap<int, MidiEvent> m_events;
    int m_outChannel = -1;
    int m_outPort = -1;
    bool m_drumTrack = false;
};

class MidiFile
{
public:
    enum class Format : std::uint16_t {
        SingleTrack = 0,
        SimultaneousMultiTrack = 1,
        IndependentMultiTrack = 2,
    };

    static muse::Expected<MidiFile, muse::String> read(muse::io::IODevice&);

    MidiFile() = default;
    MidiFile(std::vector<MidiTrack>, int division, bool isDivisionInTps);

    bool write(QIODevice*);

    std::vector<MidiTrack>& tracks() { return m_tracks; }
    const std::vector<MidiTrack>& tracks() const { return m_tracks; }

    MidiType midiType() const { return m_midiType; }
    void setMidiType(MidiType mt) { m_midiType = mt; }

    Format format() { return m_format; }
    void setFormat(Format fmt) { m_format = fmt; }

    int division() const { return m_division; }
    bool isDivisionInTps() const { return m_isDivisionInTps; }
    void setDivision(int val) { m_division = val; }
    void separateChannel();

protected:
    // write
    bool write(const void*, qint64);
    void writeShort(int);
    void writeLong(int);
    bool writeTrack(const MidiTrack&);
    void putvl(unsigned);
    void put(unsigned char c) { write(&c, 1); }
    void writeStatus(int type, int channel);

    void resetRunningStatus() { m_status = -1; }

private:
    QIODevice* m_fp = nullptr;
    std::vector<MidiTrack> m_tracks;
    int m_division = 0;
    bool m_isDivisionInTps = false; ///< ticks per second, alternative - ticks per beat
    Format m_format = Format::SimultaneousMultiTrack;
    MidiType m_midiType = MidiType::UNKNOWN;

    // values used during write()
    int m_status = 0;    ///< running status

    void writeEvent(const MidiEvent& event);
};
}
