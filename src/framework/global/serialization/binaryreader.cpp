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
#include "binaryreader.h"

#include "global/io/iodevice.h"

using namespace std::literals;

namespace muse {
namespace {
template<typename T>
T toIntBE(const std::array<std::uint8_t, sizeof(T)>& bytes)
{
    T val = 0;
    for (const std::uint8_t b : bytes) {
        val <<= 8;
        val |= b;
    }

    return val;
}

template<typename T>
T toIntLE(const std::array<std::uint8_t, sizeof(T)>& bytes)
{
    T val = 0;
    for (std::size_t i = 0; i < bytes.size(); i++) {
        val |= std::uint64_t{ bytes[i] } << (8 * i);
    }

    return val;
}

template<typename T>
BinaryReader::Result<T> readIntBE(BinaryReader& reader)
{
    return reader.readNBytes<sizeof(T)>()
           .transform(&toIntBE<T>);
}

template<typename T>
BinaryReader::Result<T> readIntLE(BinaryReader& reader)
{
    return reader.readNBytes<sizeof(T)>()
           .transform(&toIntLE<T>);
}
} // namespace

BinaryReader::BinaryReader(io::IODevice* device)
    : m_device(device) {}

BinaryReader::Result<std::int64_t> BinaryReader::readInt64BE()
{
    return readIntBE<std::int64_t>(*this);
}

BinaryReader::Result<std::uint64_t> BinaryReader::readUInt64BE()
{
    return readIntBE<std::uint64_t>(*this);
}

BinaryReader::Result<std::int32_t> BinaryReader::readInt32BE()
{
    return readIntBE<std::int32_t>(*this);
}

BinaryReader::Result<std::uint32_t> BinaryReader::readUInt32BE()
{
    return readIntBE<std::uint32_t>(*this);
}

BinaryReader::Result<std::int16_t> BinaryReader::readInt16BE()
{
    return readIntBE<std::int16_t>(*this);
}

BinaryReader::Result<std::uint16_t> BinaryReader::readUInt16BE()
{
    return readIntBE<std::uint16_t>(*this);
}

BinaryReader::Result<std::int64_t> BinaryReader::readInt64LE()
{
    return readIntLE<std::int64_t>(*this);
}

BinaryReader::Result<std::uint64_t> BinaryReader::readUInt64LE()
{
    return readIntLE<std::uint64_t>(*this);
}

BinaryReader::Result<std::int32_t> BinaryReader::readInt32LE()
{
    return readIntLE<std::int32_t>(*this);
}

BinaryReader::Result<std::uint32_t> BinaryReader::readUInt32LE()
{
    return readIntLE<std::uint32_t>(*this);
}

BinaryReader::Result<std::int16_t> BinaryReader::readInt16LE()
{
    return readIntLE<std::int16_t>(*this);
}

BinaryReader::Result<std::uint16_t> BinaryReader::readUInt16LE()
{
    return readIntLE<std::uint16_t>(*this);
}

BinaryReader::Result<std::uint8_t> BinaryReader::readByte()
{
    std::uint8_t b = {};
    Result<void> result = readNBytes(&b, 1);
    RETURN_UNEXPECTED(result);

    return b;
}

BinaryReader::Result<std::vector<std::uint8_t> > BinaryReader::readNBytes(std::size_t n)
{
    std::vector<std::uint8_t> buf(n);

    Result<void> result = readNBytes(buf.data(), n);
    RETURN_UNEXPECTED(result);

    return std::move(buf);
}

BinaryReader::Result<void> BinaryReader::readNBytes(std::uint8_t* data, std::size_t n)
{
    return readBytes(data, n)
           .and_then([&](const std::size_t bytesRead) -> Result<void> {
        if (bytesRead != n) {
            // revert pos back to what it was
            m_device->seek(m_device->pos() - bytesRead);

            return Unexpected(makeError(ErrCode::EndOfFile));
        }

        return Result<void> {};
    });
}

BinaryReader::Result<std::size_t> BinaryReader::readBytes(std::uint8_t* data, std::size_t max)
{
    const std::size_t bytesRead = m_device->read(data, max);
    if (m_device->hasError()) {
        return Unexpected(makeError(ErrCode::IoError));
    }

    return static_cast<std::size_t>(bytesRead);
}

BinaryReader::Error BinaryReader::makeError(const ErrCode code)
{
    return Error{ code, m_device->pos() };
}
} // namespace muse
