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
#include <stack>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "textstream.h"

namespace muse {
namespace io {
class IODevice;
}

class XmlStreamWriter
{
    struct NoDefConstr {
        NoDefConstr() = delete;

        bool operator==(const NoDefConstr&) const { return true; }
        bool operator!=(const NoDefConstr& other) const { return !(*this == other); }
    };

public:
    explicit XmlStreamWriter(io::IODevice* dev);

    virtual ~XmlStreamWriter();
    XmlStreamWriter(const XmlStreamWriter&) = delete;
    XmlStreamWriter(XmlStreamWriter&&) = delete;
    XmlStreamWriter& operator=(const XmlStreamWriter&) = delete;
    XmlStreamWriter& operator=(XmlStreamWriter&&) = delete;

    using Value = std::variant<NoDefConstr, int32_t, uint32_t, int64_t, uint64_t, double, std::string_view, std::string>;
    struct Attribute {
        std::string_view name;
        Value value;

        Attribute(const std::string_view n, const Value v)
            : name{n}, value{v} {}

        // disambiguate std::string_view and std::string
        Attribute(const std::string_view n, const char* v)
            : name{n}, value{std::string_view { v }} {}
    };
    // TODO(C++20): use std::span
    using Attributes = std::vector<Attribute>;

    void flush();

    void startDocument();
    void writeDoctype(std::string_view type);

    void startElement(std::string_view name, const Attributes& attrs = {});
    void endElement();

    // <name attr="value"/>
    void element(std::string_view name, const Attributes& attrs = {});

    // <name attr="value">body</name>
    void element(std::string_view name, const Value& body, const Attributes& attrs = {});
    // disambiguate std::string_view and std::string
    void element(std::string_view name, const char* body, const Attributes& attrs = {});

    void comment(std::string_view);

    static std::string escapeCodePoint(char32_t);
    static std::string escapeString(std::string_view);

protected:
    [[deprecated("please use startElement(name, attr)")]]
    void startElementRaw(std::string_view nameWithAttributes);
    [[deprecated("please use element(name, attrs)")]]
    void elementRaw(std::string_view nameWithAttributes);
    [[deprecated("please use element(name, body, attrs)")]]
    void elementRaw(std::string_view nameWithAttributes, const Value& body);
    // TODO: provide a way to serialize a XML DOM tree
    //[[deprecated]]
    void elementStringRaw(std::string_view nameWithAttributes, const std::string_view rawBody);

private:
    void writeValue(const Value& v);
    void writeIndent();

    using ElementStack = std::stack<std::string, std::vector<std::string>>;
    ElementStack m_elementStack;
    TextStream m_stream;
};
}
