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

#include <map>
#include <string>
#include <vector>

#include <QList>

#include "importmidi_fraction.h"
#include "importmidi_operations.h"

namespace mu::iex::midi {
class MidiFile;
class MTrack;
using LyricsTrack = std::multimap<ReducedFraction, std::string>;

namespace MidiLyrics {
struct TrackMapping {
    int trackIdx;
    int lyricsTrackIdx;
};

std::vector<LyricsTrack> extractLyricsToMidiData(const MidiFile& mf);

std::vector<TrackMapping> setInitialLyricsFromMidiData(const QList<MTrack>&, const QList<LyricsTrack>&);

void setLyricsFromOperations(
    const QList<MTrack>&, const QList<LyricsTrack>&, const MidiOperations::TrackOp<int>& lyricsTrackIdx);

QList<std::string> makeLyricsListForUI(const QList<LyricsTrack>&);
} // namespace MidiLyrics
} // namespace mu::iex::midi
