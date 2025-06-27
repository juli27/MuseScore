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
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "global/io/buffer.h"
#include "global/serialization/binaryreader.h"
#include "global/types/bytearray.h"

using namespace muse;

TEST(BinaryReaderTests, testReadIntBE)
{
    ByteArray data{};
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    EXPECT_EQ(reader.readInt16BE().value(), static_cast<std::int16_t>(0x8000));
    EXPECT_EQ(buffer.pos(), 2);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt16BE().value(), 0x8000);
    EXPECT_EQ(buffer.pos(), 2);

    buffer.seek(0);
    EXPECT_EQ(reader.readInt32BE().value(), static_cast<std::int32_t>(0x8000'8000));
    EXPECT_EQ(buffer.pos(), 4);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt32BE().value(), 0x8000'8000);
    EXPECT_EQ(buffer.pos(), 4);

    buffer.seek(0);
    EXPECT_EQ(reader.readInt64BE().value(), static_cast<std::int64_t>(0x8000'8000'8000'8000));
    EXPECT_EQ(buffer.pos(), 8);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt64BE().value(), 0x8000'8000'8000'8000);
    EXPECT_EQ(buffer.pos(), 8);
}

TEST(BinaryReaderTests, testReadIntLE)
{
    ByteArray data{};
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    EXPECT_EQ(reader.readInt16LE().value(), static_cast<std::int16_t>(0x8000));
    EXPECT_EQ(buffer.pos(), 2);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt16LE().value(), 0x8000);
    EXPECT_EQ(buffer.pos(), 2);

    buffer.seek(0);
    EXPECT_EQ(reader.readInt32LE().value(), static_cast<std::int32_t>(0x8000'8000));
    EXPECT_EQ(buffer.pos(), 4);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt32LE().value(), 0x8000'8000);
    EXPECT_EQ(buffer.pos(), 4);

    buffer.seek(0);
    EXPECT_EQ(reader.readInt64LE().value(), static_cast<std::int64_t>(0x8000'8000'8000'8000));
    EXPECT_EQ(buffer.pos(), 8);

    buffer.seek(0);
    EXPECT_EQ(reader.readUInt64LE().value(), 0x8000'8000'8000'8000);
    EXPECT_EQ(buffer.pos(), 8);
}

TEST(BinaryReaderTests, testReadByte)
{
    ByteArray data{};
    data.push_back(0x80);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    EXPECT_EQ(reader.readByte().value(), 0x80);
    EXPECT_EQ(buffer.pos(), 1);
}

TEST(BinaryReaderTests, testReadNBytes)
{
    ByteArray data{};
    data.push_back(0x80);
    data.push_back(0x00);
    data.push_back(0x80);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    EXPECT_TRUE(reader.readNBytes<0>().value().empty());
    EXPECT_THAT(reader.readNBytes<2>().value(), testing::ElementsAre(0x80, 0x00));

    buffer.seek(0);
    EXPECT_TRUE(reader.readNBytes(0).value().empty());
    EXPECT_THAT(reader.readNBytes(2).value(), testing::ElementsAre(0x80, 0x00));

    buffer.seek(0);
    std::vector<std::uint8_t> buf(2);
    EXPECT_TRUE(reader.readNBytes(buf.data(), buf.size()).has_value());
    EXPECT_THAT(buf, testing::ElementsAre(0x80, 0x00));
}

TEST(BinaryReaderTests, testReadBytes)
{
    ByteArray data{};
    data.push_back(0x80);
    data.push_back(0x00);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    std::vector<std::uint8_t> buf(3);
    EXPECT_EQ(reader.readBytes(buf.data(), buf.size()).value(), 2);
    EXPECT_THAT(buf, testing::ElementsAre(0x80, 0x00, 0x00));
}

TEST(BinaryReaderTests, testFailures)
{
    ByteArray data{};
    data.push_back(0x80);
    io::Buffer buffer{ std::move(data) };
    buffer.open();
    BinaryReader reader{ &buffer };

    EXPECT_FALSE(reader.readInt64BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt64BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readInt32BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt32BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readInt16BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt16BE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readInt64LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt64LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readInt32LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt32LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readInt16LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);
    EXPECT_FALSE(reader.readUInt16LE().has_value());
    EXPECT_EQ(buffer.pos(), 0);

    EXPECT_FALSE(reader.readNBytes<2>().has_value());
    EXPECT_EQ(buffer.pos(), 0);

    EXPECT_FALSE(reader.readNBytes(2).has_value());
    EXPECT_EQ(buffer.pos(), 0);

    std::vector<std::uint8_t> buf(2);
    EXPECT_FALSE(reader.readNBytes(buf.data(), buf.size()).has_value());
    EXPECT_EQ(buffer.pos(), 0);
}
