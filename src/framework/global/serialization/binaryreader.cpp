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

#include "global/types/string.h"

namespace muse {
BinaryReader::BinaryReader(io::IODevice* device)
    : m_device(device) {}

BinaryReader::Result<std::uint32_t> BinaryReader::readUInt32BE() const
{
    return readNBytes<4>()
           .transform([](const std::array<std::uint8_t, 4>& bytes) {
        std::uint32_t val = 0;
        for (const std::uint8_t b : bytes) {
            val <<= 8;
            val += (b & 0xff);
        }

        return val;
    });
}

BinaryReader::Result<std::uint16_t> BinaryReader::readUInt16BE() const
{
    return readNBytes<2>()
           .transform([](const std::array<std::uint8_t, 2>& bytes) {
        std::uint16_t val = 0;
        for (const std::uint8_t c : bytes) {
            val <<= 8;
            val += (c & 0xff);
        }

        return val;
    });
}

BinaryReader::Result<std::vector<std::uint8_t> > BinaryReader::readNBytes(std::size_t n) const
{
    std::vector<std::uint8_t> buf(n);
    if (n == 0) {
        return buf;
    }

    return readNBytes(buf.data(), n).transform([&](const std::uint64_t) {
        return std::move(buf);
    });
}

BinaryReader::Result<std::size_t> BinaryReader::readNBytes(std::uint8_t* data, std::size_t n) const
{
    return readBytes(data, n)
           .and_then([&](const std::uint64_t bytesRead) -> Result<std::uint64_t> {
        if (bytesRead != n) {
            return Unexpected(String(u"expected %1 bytes, but only read %2 bytes")
                              .arg(n).arg(bytesRead).toStdString());
        }

        return bytesRead;
    });
}

BinaryReader::Result<std::size_t> BinaryReader::readBytes(std::uint8_t* data, std::size_t max) const
{
    const qint64 bytesRead = m_device->read(data, max);
    if (bytesRead == -1) {
        return Unexpected(m_device->errorString());
    }

    return static_cast<std::uint64_t>(bytesRead);
}

BinaryReader::Result<std::uint8_t> BinaryReader::readByte() const
{
    std::uint8_t b = {};
    const qint64 bytesRead = m_device->read(&b, 1);
    if (bytesRead == -1) {
        return Unexpected(m_device->errorString());
    }
    if (bytesRead != 1) {
        return Unexpected(std::string("expected a byte"));
    }

    return b;
}
}
