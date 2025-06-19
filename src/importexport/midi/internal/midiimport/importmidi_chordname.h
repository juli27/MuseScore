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

#include <QList>
#include <QString>

#include "internal/midishared/midifile.h"

#include "importmidi_fraction.h"

namespace mu::iex::midi {
class MTrack;

namespace MidiChordName {
std::multimap<ReducedFraction, QString> findChordNames(const MidiFile& tracks);
void setChordNames(const std::multimap<ReducedFraction, QString>& chordNames, QList<MTrack>& tracks);
} // namespace MidiChordName
} // namespace mu::iex::midi
