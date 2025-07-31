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
#include "xmlstreamwriter.h"

#include <global/thirdparty/utfcpp/utf8.h>

#include "global/stringutils.h"

#include "global/log.h"

using namespace std::literals;

namespace muse {
XmlStreamWriter::XmlStreamWriter(io::IODevice* const dev)
    : m_stream{dev} {}

XmlStreamWriter::~XmlStreamWriter()
{
    IF_ASSERT_FAILED_X(m_elementStack.empty(), "missing calls to endElement()") {
        do {
            endElement();
        } while (!m_elementStack.empty());
    }
}

void XmlStreamWriter::flush()
{
    m_stream.flush();
}

void XmlStreamWriter::startDocument()
{
    m_stream << R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n"sv;
}

void XmlStreamWriter::writeDoctype(const std::string_view type)
{
    m_stream << "<!DOCTYPE "sv << type << ">\n"sv;
}

void XmlStreamWriter::startElement(const std::string_view name, const Attributes& attrs)
{
    DO_ASSERT(!name.empty() && !strings::contains(name, ' '));

    writeIndent();
    m_stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_stream << ' ' << a.name << "=\""sv;
        writeValue(a.value);
        m_stream << '\"';
    }
    m_stream << ">\n"sv;

    m_elementStack.emplace(name);
}

void XmlStreamWriter::endElement()
{
    IF_ASSERT_FAILED(!m_elementStack.empty()) {
        return;
    }

    writeIndent();
    m_stream << "</"sv << m_elementStack.top() << ">\n"sv;
    m_elementStack.pop();

    // TODO: is this necessary?
    flush();
}

void XmlStreamWriter::element(const std::string_view name, const Attributes& attrs)
{
    DO_ASSERT(!name.empty() && !strings::contains(name, ' '));

    writeIndent();
    m_stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_stream << ' ' << a.name << "=\""sv;
        writeValue(a.value);
        m_stream << '\"';
    }
    m_stream << "/>\n"sv;
}

void XmlStreamWriter::element(const std::string_view name, const Value& body, const Attributes& attrs)
{
    DO_ASSERT(!name.empty() && !strings::contains(name, ' '));

    writeIndent();
    m_stream << '<' << name;
    for (const Attribute& a : attrs) {
        m_stream << ' ' << a.name << "=\""sv;
        writeValue(a.value);
        m_stream << '\"';
    }
    m_stream << '>';
    writeValue(body);
    m_stream << "</"sv << name << ">\n"sv;
}

void XmlStreamWriter::comment(const std::string_view text)
{
    writeIndent();
    m_stream << "<!-- "sv << text << " -->\n"sv;
}

std::string XmlStreamWriter::escapeCodePoint(const char32_t c)
{
    switch (c) {
    case U'<':
        return "&lt;"s;
    case U'>':
        return "&gt;"s;
    case U'&':
        return "&amp;"s;
    case U'\"':
        return "&quot;"s;
    default:
        // ignore invalid characters in xml 1.0
        if ((c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D)) {
            return std::string();
        }
        return utf8::utf32to8(std::u32string_view(&c, 1));
    }
}

std::string XmlStreamWriter::escapeString(const std::string_view str)
{
    std::string escaped{};
    auto it = str.begin();
    while (it != str.end()) {
        const char32_t c = utf8::next(it, str.end());
        escaped += escapeCodePoint(c);
    }

    return escaped;
}

void XmlStreamWriter::startElementRaw(const std::string_view nameWithAttributes)
{
    writeIndent();
    m_stream << '<' << nameWithAttributes << ">\n"sv;

    const auto [name, attributes] = strings::splitFirst(nameWithAttributes, ' ');
    m_elementStack.emplace(name);
}

void XmlStreamWriter::elementRaw(std::string_view nameWithAttributes)
{
    writeIndent();
    m_stream << '<' << nameWithAttributes << "/>\n"sv;
}

void XmlStreamWriter::elementRaw(const std::string_view nameWithAttributes, const Value& body)
{
    writeIndent();
    m_stream << '<' << nameWithAttributes << '>';
    writeValue(body);

    const auto [name, attributes] = strings::splitFirst(nameWithAttributes, ' ');
    m_stream << "</" << name << ">\n"sv;
}

void XmlStreamWriter::elementStringRaw(std::string_view nameWithAttributes, const std::string_view rawBody)
{
    if (rawBody.empty()) {
        elementRaw(nameWithAttributes);
        return;
    }

    writeIndent();
    m_stream << '<' << nameWithAttributes << '>';
    m_stream << rawBody;

    const auto [name, attributes] = strings::splitFirst(nameWithAttributes, ' ');
    m_stream << "</" << name << '>' << '\n';
}

void XmlStreamWriter::writeValue(const Value& v)
{
    switch (v.index()) {
    case 1: m_stream << std::get<int32_t>(v);
        break;
    case 2: m_stream << std::get<uint32_t>(v);
        break;
    case 3: m_stream << std::get<int64_t>(v);
        break;
    case 4: m_stream << std::get<uint64_t>(v);
        break;
    case 5: m_stream << std::get<double>(v);
        break;
    case 6: m_stream << escapeString(std::get<std::string_view>(v));
        break;
    default:
        LOGE() << "invalid value index: " << v.index();
        UNREACHABLE;
        break;
    }
}

void XmlStreamWriter::writeIndent()
{
    const size_t level = m_elementStack.size();
    for (size_t i = 0; i < level; ++i) {
        m_stream << "  "sv;
    }
}
} // namespace muse
