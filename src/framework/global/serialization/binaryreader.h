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
#include <utility>
#include <vector>

#include "global/types/expected.h"

namespace muse {
namespace io {
class IODevice;
} //namespace io

class BinaryReader
{
public:
    enum class ErrCode : std::uint8_t {
        IoError,
        EndOfFile,
    };

    struct Error {
        ErrCode code;
        std::size_t devicePos;
    };

    template<typename T>
    using Result = Expected<T, Error>;

    explicit BinaryReader(io::IODevice*);

    io::IODevice* device() const { return m_device; }

    // big-endian
    Result<std::int64_t> readInt64BE();
    Result<std::uint64_t> readUInt64BE();
    Result<std::int32_t> readInt32BE();
    Result<std::uint32_t> readUInt32BE();
    Result<std::int16_t> readInt16BE();
    Result<std::uint16_t> readUInt16BE();

    // little-endian
    Result<std::int64_t> readInt64LE();
    Result<std::uint64_t> readUInt64LE();
    Result<std::int32_t> readInt32LE();
    Result<std::uint32_t> readUInt32LE();
    Result<std::int16_t> readInt16LE();
    Result<std::uint16_t> readUInt16LE();

    Result<std::uint8_t> readByte();

    // read exactly n bytes, fail otherwise
    template<std::size_t N>
    Result<std::array<std::uint8_t, N> > readNBytes()
    {
        std::array<std::uint8_t, N> buf;
        Result<void> result = readNBytes(buf.data(), buf.size());
        RETURN_UNEXPECTED(result);

        return std::move(buf);
    }

    // read exactly n bytes, fail otherwise
    Result<std::vector<std::uint8_t>> readNBytes(std::size_t n);

    // TODO: use span (C++20 or github.com/microsoft/GSL)
    // read exactly n bytes, fail otherwise
    Result<void> readNBytes(std::uint8_t* data, std::size_t n);

    // TODO: use span (C++20 or github.com/microsoft/GSL)
    // returns actual number of bytes read
    Result<std::size_t> readBytes(std::uint8_t* data, std::size_t max);

private:
    io::IODevice* m_device;

    Error makeError(ErrCode);
};
}
