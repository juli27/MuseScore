/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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

#include <string_view>

#include "global/serialization/xmlstreamwriter.h"

#include "engraving/dom/property.h"
#include "engraving/types/propertyvalue.h"
#include "engraving/types/types.h"

namespace muse::io {
class IODevice;
}

namespace mu::engraving {
class EngravingObject;

class XmlWriter : public muse::XmlStreamWriter
{
public:
    XmlWriter(muse::io::IODevice*);

    ~XmlWriter() override = default;
    XmlWriter(const XmlWriter&) = delete;
    XmlWriter(XmlWriter&&) = delete;
    XmlWriter& operator=(const XmlWriter&) = delete;
    XmlWriter& operator=(XmlWriter&&) = delete;

    using XmlStreamWriter::startElement;
    void startElement(const EngravingObject* se, const Attributes& attrs = {});

    void tag(std::string_view name, const Attributes& attrs = {});
    void tag(std::string_view name, const Value& body, const Attributes& attrs = {});
    // disambiguate std::string_view and std::string
    void tag(std::string_view name, const char* body, const Attributes& attrs = {});
    void tag(std::string_view name, const Value& val, const Value& def, const Attributes& attrs = {});

    void tagProperty(Pid, const PropertyValue& data, const PropertyValue& def = PropertyValue());
    void tagProperty(std::string_view name, const PropertyValue& data, const PropertyValue& def = PropertyValue());

    void tagFraction(std::string_view name, const Fraction&, const Fraction& def = Fraction());
    void tagPoint(std::string_view name, const PointF&);

    // TODO: provide a way to serialize a XML DOM tree
    //[[deprecated]]
    void writeXml(std::string_view nameWithAttributes, String s);

    using XmlStreamWriter::startElementRaw;
    [[deprecated("please use element(name, attrs)")]]
    void tagRaw(std::string_view nameWithAttributes);
    [[deprecated("please use element(name, body, attrs)")]]
    void tagRaw(std::string_view nameWithAttributes, const Value& body);
    // disambiguate std::string_view and std::string
    [[deprecated("please use element(name, body, attrs)")]]
    void tagRaw(std::string_view nameWithAttributes, const char* body);

private:
    void tagProperty(std::string_view name, P_TYPE, const PropertyValue& data);
};
}
