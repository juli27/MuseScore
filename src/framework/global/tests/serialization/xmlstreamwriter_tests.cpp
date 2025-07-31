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
#include "global/serialization/xmlstreamwriter.h"

using namespace muse;

class Global_Ser_XmlStreamWriterTests : public ::testing::Test
{
public:
    Global_Ser_XmlStreamWriterTests()
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

TEST_F(Global_Ser_XmlStreamWriterTests, testInvariants)
{
    // XmlStreamWriter must always have a non-null, open for writing, IODevice
    EXPECT_DEATH(XmlStreamWriter { nullptr }, "m_device");
    EXPECT_DEATH({
        io::Buffer closedBuf {};
        XmlStreamWriter { &closedBuf };
    }, R"(m_device->isWriteable\(\))");
    EXPECT_DEATH({
        io::Buffer readBuf {};
        readBuf.open(io::IODevice::ReadOnly);
        XmlStreamWriter { &readBuf };
    }, R"(m_device->isWriteable\(\))");

    // Data written to a XmlStreamWriter must always end up being written to the IODevice
    {
        XmlStreamWriter stream{ buf() };

        stream.startDocument();
        // stream is destroyed
    }
    EXPECT_EQ(bufContent(), R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n");
}

TEST_F(Global_Ser_XmlStreamWriterTests, testDoctype)
{
    XmlStreamWriter stream{ buf() };

    stream.writeDoctype(R"(score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" )"
                        R"("http://www.musicxml.org/dtds/partwise.dtd")");
    stream.flush();

    EXPECT_EQ(bufContent(), R"(<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" )"
                            R"("http://www.musicxml.org/dtds/partwise.dtd">)" "\n");
}

TEST_F(Global_Ser_XmlStreamWriterTests, testComment)
{
    XmlStreamWriter stream{ buf() };

    stream.comment("hello world, 0 < 1");
    stream.flush();

    EXPECT_EQ(bufContent(), "<!-- hello world, 0 < 1 -->\n");
}

TEST_F(Global_Ser_XmlStreamWriterTests, testElement)
{
    XmlStreamWriter stream{ buf() };

    EXPECT_DEATH(stream.element(""), "empty");
    EXPECT_DEATH(stream.element("naughty tag"), R"(contains\(name, ' '\))");
    EXPECT_DEATH(stream.element("", 42), "empty");
    EXPECT_DEATH(stream.element("naughty tag", 42), R"(contains\(name, ' '\))");

    stream.element("name1");
    stream.element("name2", { { "attr1", 42 } });
    stream.element("name3", R"(name3 body <>&")");
    stream.element("name4", R"(name4 body <>&")", { { "attr2", 3.14 } });
    stream.flush();

    EXPECT_EQ(bufContent(),
              "<name1/>\n"
              "<name2 attr1=\"42\"/>\n"
              "<name3>name3 body &lt;&gt;&amp;&quot;</name3>\n"
              "<name4 attr2=\"3.14\">name4 body &lt;&gt;&amp;&quot;</name4>\n");
}

TEST_F(Global_Ser_XmlStreamWriterTests, testStartEndElement)
{
    XmlStreamWriter stream{ buf() };

    EXPECT_DEATH(stream.startElement(""), "empty");
    EXPECT_DEATH(stream.startElement("naughty tag"), R"(contains\(name, ' '\))");
    EXPECT_DEATH(stream.endElement(), "empty");

    // every call to startElement must be matched with a call to endElement
    EXPECT_DEATH({
        io::Buffer tmp {};
        tmp.open(io::IODevice::WriteOnly);
        XmlStreamWriter stream { &tmp };

        stream.startElement("closeMe");
        // stream is destroyed
    }, "empty");

    stream.startElement("closeMe");
    stream.startElement("closeMe2", { { "attr1", 42 } });
    stream.endElement();
    stream.endElement();
    stream.flush();

    EXPECT_EQ(bufContent(),
              "<closeMe>\n"
              "  <closeMe2 attr1=\"42\">\n"
              "    </closeMe2>\n"
              "  </closeMe>\n");
}
