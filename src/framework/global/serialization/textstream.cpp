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
#include "textstream.h"

#include <array>
#include <charconv>

#if !defined(_MSC_VER)
#include <sstream>
#endif

#include "global/io/iodevice.h"

#include "global/log.h"

namespace muse {
TextStream::TextStream(io::IODevice* const device)
    : m_device(device)
{
    DO_ASSERT(m_device && m_device->isWriteable());

    m_buf.reserve(BUFFER_SIZE);
}

TextStream::~TextStream()
{
    flush();
}

void TextStream::flush()
{
    if (m_buf.empty()) {
        return;
    }

    const size_t numBytesToWrite = m_buf.size();
    const size_t numBytesWritten = m_device->write(reinterpret_cast<const uint8_t*>(m_buf.data()), numBytesToWrite);
    DO_ASSERT(numBytesWritten == numBytesToWrite);

    m_buf.clear();
}

TextStream& TextStream::operator<<(const char ch)
{
    write(&ch, 1);
    return *this;
}

TextStream& TextStream::operator<<(const std::string_view str)
{
    write(str.data(), str.size());
    return *this;
}

TextStream& TextStream::operator<<(const int32_t val)
{
    // ceil(log_10(2^31)) = 10 (+ sign)
    std::array<char, 11> buf{};
    const auto [last, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    IF_ASSERT_FAILED(ec == std::errc {}) {
        return *this;
    }

    write(buf.data(), static_cast<size_t>(last - buf.data()));

    return *this;
}

TextStream& TextStream::operator<<(const uint32_t val)
{
    // ceil(log_10(2^32)) = 10
    std::array<char, 10> buf{};
    const auto [last, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    IF_ASSERT_FAILED(ec == std::errc {}) {
        return *this;
    }

    write(buf.data(), static_cast<size_t>(last - buf.data()));
    return *this;
}

TextStream& TextStream::operator<<(const int64_t val)
{
    // ceil(log_10(2^63)) = 20 (+ sign)
    std::array<char, 21> buf{};
    const auto [last, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    IF_ASSERT_FAILED(ec == std::errc {}) {
        return *this;
    }

    write(buf.data(), static_cast<size_t>(last - buf.data()));

    return *this;
}

TextStream& TextStream::operator<<(const uint64_t val)
{
    // ceil(log_10(2^64)) = 20
    std::array<char, 20> buf{};
    const auto [last, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val);
    IF_ASSERT_FAILED(ec == std::errc {}) {
        return *this;
    }

    write(buf.data(), static_cast<size_t>(last - buf.data()));

    return *this;
}

TextStream& TextStream::operator<<(const double val)
{
    // macOS: requires macOS 13.3 at runtime
    // linux: requires at least libstdc++ 11 (we compile with libstdc++ 10) or libc++ 14
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130300) \
    || (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160300) \
    || defined(_MSC_VER) \
    || (!defined(__APPLE__) && defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 14000) \
    || (defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 11)
    std::array<char, 24> buf{};
    // emulate std::stringstream's behaviour
    const auto [last, ec] = std::to_chars(buf.data(), buf.data() + buf.size(), val, std::chars_format::general, 6);
    IF_ASSERT_FAILED(ec == std::errc {}) {
        return *this;
    }
    write(buf.data(), static_cast<size_t>(last - buf.data()));
#else
    std::stringstream ss;
    ss << val;
    const std::string buf = ss.str();
    write(buf.data(), buf.size());
#endif

    return *this;
}

void TextStream::write(const char* ch, const size_t len)
{
    m_buf.insert(m_buf.end(), ch, ch + len);
    if (m_buf.size() >= BUFFER_SIZE) {
        flush();
    }
}
} // namespace muse
