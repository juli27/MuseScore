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
#include <gmock/gmock.h>

#include <cstdint>
#include <limits>

#include "global/io/buffer.h"
#include "global/serialization/textstream.h"

using ::testing::StartsWith;
using namespace muse;

class Global_Ser_TextStreamTests : public ::testing::Test
{
public:
    Global_Ser_TextStreamTests()
    {
        m_buf.open(io::IODevice::WriteOnly);
    }

    io::Buffer* buf()
    {
        return &m_buf;
    }

    std::string_view bufContent() const
    {
        return m_buf.data().viewAsUtf8();
    }

private:
    io::Buffer m_buf{};
};

TEST_F(Global_Ser_TextStreamTests, testInvariants)
{
    // TextStream must always have a non-null, open for writing, IODevice
    EXPECT_DEATH(TextStream { nullptr }, "m_device");
    EXPECT_DEATH({
        io::Buffer closedBuf {};
        TextStream { &closedBuf };
    }, R"(m_device->isWriteable\(\))");
    EXPECT_DEATH({
        io::Buffer readBuf {};
        readBuf.open(io::IODevice::ReadOnly);
        TextStream { &readBuf };
    }, R"(m_device->isWriteable\(\))");

    // Data written to a TextStream must always end up being written to the IODevice
    {
        TextStream stream{ buf() };

        stream << "hello";
        // stream is destroyed
    }
    EXPECT_EQ(bufContent(), "hello");
}

TEST_F(Global_Ser_TextStreamTests, testNumberMin)
{
    TextStream stream{ buf() };

    stream << std::numeric_limits<int32_t>::lowest();
    stream << std::numeric_limits<uint32_t>::lowest();
    stream << std::numeric_limits<int64_t>::lowest();
    stream << std::numeric_limits<uint64_t>::lowest();
    stream << std::numeric_limits<double>::lowest();
    stream.flush();

    EXPECT_EQ(bufContent(), "-2147483648"
                            "0"
                            "-9223372036854775808"
                            "0"
                            "-1.79769e+308");
}

TEST_F(Global_Ser_TextStreamTests, testNumberMax)
{
    TextStream stream{ buf() };

    stream << std::numeric_limits<int32_t>::max();
    stream << std::numeric_limits<uint32_t>::max();
    stream << std::numeric_limits<int64_t>::max();
    stream << std::numeric_limits<uint64_t>::max();
    stream << std::numeric_limits<double>::max();
    stream.flush();

    EXPECT_EQ(bufContent(), "2147483647"
                            "4294967295"
                            "9223372036854775807"
                            "18446744073709551615"
                            "1.79769e+308");
}

TEST_F(Global_Ser_TextStreamTests, testDoubleSpecial)
{
    TextStream stream{ buf() };
    size_t prevSize = 0;

    stream << 0.0;
    stream << std::numeric_limits<double>::min();
    stream << std::numeric_limits<double>::denorm_min();
    stream.flush();
    {
        const std::string_view str = bufContent();
        prevSize += str.size();
        EXPECT_EQ(str, "0"
                       "2.22507e-308"
                       "4.94066e-324");
    }

    // the exact string representations of these is implementation defined
    stream << std::numeric_limits<double>::infinity();
    stream.flush();
    {
        const std::string_view str = bufContent().substr(prevSize);
        prevSize += str.size();
        EXPECT_THAT(str, StartsWith("inf"));
    }

    stream << std::numeric_limits<double>::quiet_NaN();
    stream.flush();
    {
        const std::string_view str = bufContent().substr(prevSize);
        EXPECT_THAT(str, StartsWith("nan"));
    }
}
