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
#include "printmodule.h"

#include <QtEnvironmentVariables>

#include "global/modularity/ioc.h"

#include "internal/qtprintprovider.h"

#ifdef Q_OS_WIN
#include "internal/platform/win/windowsprintprovider.h"
#endif

#include "log.h"

namespace mu::print {
namespace {
IPrintProviderPtr makePrintProvider([[maybe_unused]] const muse::modularity::ContextPtr& ctx)
{
#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("MUSESCORE_FORCE_QTPRINTPROVIDER")
        && WindowsPrintProvider::isAvailable()) {
        return std::make_shared<WindowsPrintProvider>(ctx);
    }

    LOGI() << "Falling back to QtPrintProvider";
#endif

    return std::make_shared<QtPrintProvider>();
}
}

std::string PrintModule::moduleName() const
{
    return "print";
}

void PrintModule::registerExports()
{
    ioc()->registerExport<IPrintProvider>(moduleName(), makePrintProvider(iocContext()));
}
}
