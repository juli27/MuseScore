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
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace muse {
namespace io {
class IODevice;
}

class TextStream
{
public:
    static constexpr size_t BUFFER_SIZE = 16384;

    explicit TextStream(io::IODevice* device);

    ~TextStream();
    TextStream(const TextStream&) = delete;
    TextStream(TextStream&&) = delete;
    TextStream& operator=(const TextStream&) = delete;
    TextStream& operator=(TextStream&&) = delete;

    void flush();

    TextStream& operator<<(char);
    TextStream& operator<<(std::string_view);
    TextStream& operator<<(int32_t);
    TextStream& operator<<(uint32_t);
    TextStream& operator<<(int64_t);
    TextStream& operator<<(uint64_t);
    TextStream& operator<<(double);

private:
    void write(const char* ch, size_t len);

    io::IODevice* m_device = nullptr;
    std::vector<char> m_buf;
};
}
