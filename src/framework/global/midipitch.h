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

#pragma once

#include "log.h"
#include "types/string.h"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace muse {
class MidiPitch
{
public:
    static constexpr MidiPitch min()
    {
        return MidiPitch{ MIN_VALUE };
    }

    static constexpr MidiPitch max()
    {
        return MidiPitch{ MAX_VALUE };
    }

    static constexpr std::optional<MidiPitch> parse(const int pitch)
    {
        if (!isValidPitch(pitch)) {
            return std::nullopt;
        }

        return MidiPitch{ static_cast<std::uint8_t>(pitch) };
    }

    static constexpr MidiPitch clamp(const int pitchClass, const int octave)
    {
        return clamp(pitchClass + octave * 12);
    }

    static constexpr MidiPitch clamp(const int pitch)
    {
        const int value = std::clamp(pitch, int(MIN_VALUE), int(MAX_VALUE));

        return MidiPitch { static_cast<std::uint8_t>(value) };
    }

    static MidiPitch fromInt(const int pitch)
    {
        IF_ASSERT_FAILED(isValidPitch(pitch)) {
            return clamp(pitch);
        }

        return MidiPitch{ static_cast<std::uint8_t>(pitch) };
    }

    // default construct middle-c (value = 60)
    constexpr MidiPitch() = default;

    constexpr std::uint8_t value() const
    {
        return m_value;
    }

    // pitch class from 0 to 11 (C, C#, D, etc.)
    constexpr std::uint8_t pitchClass() const
    {
        return m_value % 12;
    }

    // octave designation from -1 to 9
    constexpr std::uint8_t octave() const
    {
        return m_value / 12 - 1;
    }

    String toLocalizedString(bool addOctave = true, bool useFlats = false) const;

    // TODO: remove after transition
    constexpr operator int() const {
        return m_value;
    }

    static constexpr bool isValidPitch(const int pitch)
    {
        return pitch >= MIN_VALUE && pitch <= MAX_VALUE;
    }

private:
    static constexpr std::uint8_t MIN_VALUE = 0;
    static constexpr std::uint8_t MAX_VALUE = 127;
    static constexpr std::uint8_t MIDDLE_C = 60;

    std::uint8_t m_value = MIDDLE_C;

    constexpr explicit MidiPitch(const std::uint8_t value)
        : m_value{value}
    {}
};

// inline namespace literals {
// constexpr MidiPitch operator"" _midi_pitch(unsigned long long value)
// {

//     return MidiPitch::parse()
// }
// }
}
