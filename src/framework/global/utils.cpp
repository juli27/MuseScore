/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
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
#include "utils.h"

#include "translation.h"

using namespace muse;

static constexpr const char* SHARP_PITCH_CLASS_NAMES[] = {
    QT_TRANSLATE_NOOP("global", "C"),
    QT_TRANSLATE_NOOP("global", "C♯"),
    QT_TRANSLATE_NOOP("global", "D"),
    QT_TRANSLATE_NOOP("global", "D♯"),
    QT_TRANSLATE_NOOP("global", "E"),
    QT_TRANSLATE_NOOP("global", "F"),
    QT_TRANSLATE_NOOP("global", "F♯"),
    QT_TRANSLATE_NOOP("global", "G"),
    QT_TRANSLATE_NOOP("global", "G♯"),
    QT_TRANSLATE_NOOP("global", "A"),
    QT_TRANSLATE_NOOP("global", "A♯"),
    QT_TRANSLATE_NOOP("global", "B")
};

static constexpr const char* FLAT_PITCH_CLASS_NAMES[] = {
    QT_TRANSLATE_NOOP("global", "C"),
    QT_TRANSLATE_NOOP("global", "D♭"),
    QT_TRANSLATE_NOOP("global", "D"),
    QT_TRANSLATE_NOOP("global", "E♭"),
    QT_TRANSLATE_NOOP("global", "E"),
    QT_TRANSLATE_NOOP("global", "F"),
    QT_TRANSLATE_NOOP("global", "G♭"),
    QT_TRANSLATE_NOOP("global", "G"),
    QT_TRANSLATE_NOOP("global", "A♭"),
    QT_TRANSLATE_NOOP("global", "A"),
    QT_TRANSLATE_NOOP("global", "B♭"),
    QT_TRANSLATE_NOOP("global", "B")
};

namespace {
struct CommentedString {
    const char* str;
    const char* comment;
};
}

static constexpr CommentedString OCTAVE_NAMES[] = {
    QT_TRANSLATE_NOOP3("global", "-1", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "0", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "1", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "2", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "3", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "4", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "5", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "6", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "7", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "8", "octave designation"),
    QT_TRANSLATE_NOOP3("global", "9", "octave designation"),
};

String muse::midiPitchToLocalizedString(int pitch, bool addOctave /* = true */, bool useFlats /* = false */)
{
    if (pitch < 0 || pitch > 127) {
        return String{};
    }

    const auto classNames = useFlats ? FLAT_PITCH_CLASS_NAMES : SHARP_PITCH_CLASS_NAMES;
    const std::size_t pitchClass = pitch % 12;
    String pitchClassName = mtrc("global", classNames[pitchClass]);

    if (!addOctave) {
        return pitchClassName;
    }

    const int octave = (pitch / 12) - 1;
    const std::size_t octaveNameIdx = octave + 1;
    const CommentedString& untrOctaveName = OCTAVE_NAMES[octaveNameIdx];
    const String octaveName = mtrc("global", untrOctaveName.str, untrOctaveName.comment);

    //: %1 is the pitch class (C, C♯, D♭, etc.)
    //: %2 is the octave designation (-1, 0, 1, etc.)
    const String pitchName = mtrc("global", "%1%2", "pitch name");

    return pitchName.arg(pitchClassName, octaveName);
}
