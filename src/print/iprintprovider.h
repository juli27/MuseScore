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

#include <memory>

#include "global/modularity/imoduleinterface.h"
#include "global/types/ret.h"

#include "notation/inotation.h"

namespace mu::print {
class IPrintProvider;
using IPrintProviderPtr = std::shared_ptr<IPrintProvider>;

class IPrintProvider : MODULE_EXPORT_INTERFACE
{
    INTERFACE_ID(IPrintProvider)

public:
    virtual ~IPrintProvider() = default;

    virtual muse::Ret printNotation(notation::INotationPtr notation) = 0;
};
}
