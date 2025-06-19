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

namespace mu::iex::midi {
class MidiFile;
class MTrack;
using LyricsTrack = std::multimap<ReducedFraction, std::string>;

namespace MidiLyrics {
std::vector<LyricsTrack> extractLyricsToMidiData(const MidiFile& mf);
void setLyricsToScore(QList<MTrack>& tracks);
QList<std::string> makeLyricsListForUI();
} // namespace MidiLyrics
} // namespace mu::iex::midi
