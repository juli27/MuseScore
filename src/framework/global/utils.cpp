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

namespace {
struct CommentedString {
    const char* str;
    const char* comment;
};
}

static constexpr CommentedString PITCH_CLASS_NAMES[] = {
    QT_TRANSLATE_NOOP3("global", "C", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "C♯", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "D", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "D♯", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "E", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "F", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "F♯", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "G", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "G♯", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "A", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "A♯", "midi pitch"),
    QT_TRANSLATE_NOOP3("global", "B", "midi pitch")
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

String muse::midiPitchToLocalizedString(int pitch)
{
    if (pitch < 0 || pitch > 127) {
        return String{};
    }

    const auto pitchClassIdx = static_cast<std::size_t>(pitch % 12);
    const CommentedString& untrPitchClassName = PITCH_CLASS_NAMES[pitchClassIdx];
    const String pitchClassName = mtrc("global", untrPitchClassName.str, untrPitchClassName.comment);

    const int octave = (pitch / 12) - 1;
    const auto octaveNameIdx = static_cast<std::size_t>(octave + 1);
    const CommentedString& untrOctaveName = OCTAVE_NAMES[octaveNameIdx];
    const String octaveName = mtrc("global", untrOctaveName.str, untrOctaveName.comment);

    //: %1 is the pitch class (C, C♯, D♭, etc.)
    //: %2 is the octave designation (-1, 0, 1, etc.)
    const String pitchName = mtrc("global", "%1%2", "midi pitch");

    return pitchName.arg(pitchClassName, octaveName);
}
