/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited
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

#include "midipitch.h"

#include "translation.h"

using namespace muse;

namespace {
struct CommentedString {
    const char* str;
    const char* comment;
};
}

static constexpr CommentedString SHARP_PITCH_CLASS_NAMES[] = {
    QT_TRANSLATE_NOOP3("global", "C", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "C♯", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "D", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "D♯", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "E", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "F", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "F♯", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "G", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "G♯", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "A", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "A♯", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "B", "midi pitch class")
};

static constexpr CommentedString FLAT_PITCH_CLASS_NAMES[] = {
    QT_TRANSLATE_NOOP3("global", "C", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "D♭", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "D", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "E♭", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "E", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "F", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "G♭", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "G", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "A♭", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "A", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "B♭", "midi pitch class"),
    QT_TRANSLATE_NOOP3("global", "B", "midi pitch class")
};

static constexpr CommentedString OCTAVE_NAMES[] = {
    QT_TRANSLATE_NOOP3("global", "-1", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "0", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "1", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "2", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "3", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "4", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "5", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "6", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "7", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "8", "midi octave"),
    QT_TRANSLATE_NOOP3("global", "9", "midi octave"),
};

String MidiPitch::toLocalizedString(const bool addOctave /* = true */, const bool useFlats /* = false */) const
{
    const auto classNames = useFlats ? FLAT_PITCH_CLASS_NAMES : SHARP_PITCH_CLASS_NAMES;
    const CommentedString& rawPitchClass = classNames[pitchClass()];
    String pitchClassName = mtrc("global", rawPitchClass.str, rawPitchClass.comment);

    if (!addOctave) {
        return pitchClassName;
    }

    const std::size_t octaveNameIdx = octave() + 1;
    const CommentedString& untrOctaveName = OCTAVE_NAMES[octaveNameIdx];
    const String octaveName = mtrc("global", untrOctaveName.str, untrOctaveName.comment);

    //: %1 is the pitch class (C, C♯, D♭, etc.)
    //: %2 is the octave designation (-1, 0, 1, etc.)
    const String pitchName = mtrc("global", "%1%2", "pitch name");

    return pitchName.arg(pitchClassName, octaveName);
}
