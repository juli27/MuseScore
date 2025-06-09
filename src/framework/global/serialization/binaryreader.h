/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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
#include <string>
#include <vector> // TODO: replace with ByteArray

#include "global/io/iodevice.h"
#include "global/types/expected.h"

namespace muse {
class BinaryReader
{
public:
    template<typename T>
    using Result = Expected<T, std::string>;

    explicit BinaryReader(io::IODevice* device);

    io::IODevice* device() const { return m_device; }

    Result<std::uint32_t> readUInt32BE() const;

    Result<std::uint16_t> readUInt16BE() const;

    // read exactly n bytes, fail otherwise
    template<std::size_t N>
    Result<std::array<std::uint8_t, N> > readNBytes() const
    {
        std::array<std::uint8_t, N> buf;

        return readNBytes(buf.data(), N)
               .transform([&](const std::uint64_t) {
            return buf;
        });
    }

    // read exactly n bytes, fail otherwise
    Result<std::vector<std::uint8_t>> readNBytes(std::size_t n) const;

    // read exactly n bytes, fail otherwise
    Result<std::size_t> readNBytes(std::uint8_t* data, std::size_t n) const;

    // TODO: use span (C++20 or gh:microsoft/GSL)
    // returns actual number of bytes read
    Result<std::size_t> readBytes(std::uint8_t* data, std::size_t max) const;

    Result<std::uint8_t> readByte() const;

private:
    io::IODevice* m_device;
};
}
