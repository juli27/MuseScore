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

#include "global/io/buffer.h"

#include "engraving/dom/property.h"
#include "engraving/rw/xmlwriter.h"
#include "engraving/types/propertyvalue.h"
#include "engraving/types/types.h"

using namespace muse;
using namespace mu::engraving;

// Functionality of ::muse::XmlStreamWriter is not retested here

class Engraving_RW_XmlWriterTests : public ::testing::Test
{
public:
    Engraving_RW_XmlWriterTests()
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

TEST_F(Engraving_RW_XmlWriterTests, testTag)
{
    XmlWriter writer{ buf() };

    writer.tag("default", 11, 11);
    writer.flush();
    EXPECT_TRUE(bufContent().empty());

    writer.tag("notDefault", 12, 11);
    writer.flush();
    EXPECT_EQ(bufContent(), "<notDefault>12</notDefault>\n");
}

TEST_F(Engraving_RW_XmlWriterTests, testTagProperty)
{
    XmlWriter writer{ buf() };

    writer.tagProperty(Pid::STEM_DIRECTION, PropertyValue { DirectionV::AUTO }, PropertyValue { DirectionV::AUTO });
    writer.flush();
    EXPECT_TRUE(bufContent().empty());

    writer.tagProperty(Pid::STEM_DIRECTION, PropertyValue { DirectionV::UP }, PropertyValue { DirectionV::AUTO });
    writer.tagProperty("customName", PropertyValue { DirectionV::DOWN });
    writer.flush();
    EXPECT_EQ(bufContent(),
              "<StemDirection>up</StemDirection>\n"
              "<customName>down</customName>\n"
              );
}

TEST_F(Engraving_RW_XmlWriterTests, testTagFraction)
{
    XmlWriter writer{ buf() };

    writer.tagFraction("frac", Fraction { 4, 4 }, Fraction { 4, 4 });
    writer.flush();
    EXPECT_TRUE(bufContent().empty());

    writer.tagFraction("frac", Fraction { 12, 8 }, Fraction { 4, 4 });
    writer.flush();
    EXPECT_EQ(bufContent(), "<frac>12/8</frac>\n");
}

TEST_F(Engraving_RW_XmlWriterTests, testTagPoint)
{
    XmlWriter writer{ buf() };

    writer.tagPoint("pt", PointF { 3.14, -2.72 });
    writer.flush();
    EXPECT_EQ(bufContent(), "<pt x=\"3.14\" y=\"-2.72\"/>\n");
}

TEST_F(Engraving_RW_XmlWriterTests, testWriteXml)
{
    XmlWriter writer{ buf() };

    writer.writeXml("raw attr=\"true\"", u"<inner/>");
    writer.flush();
    EXPECT_EQ(bufContent(), "<raw attr=\"true\"><inner/></raw>\n");
}
