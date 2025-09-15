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
#include "qtprintprovider.h"

#include <string>

#include <QPrinter>
#include <QPrintDialog>

#include "draw/painter.h"

#include "notation/inotation.h"
#include "notation/inotationpainting.h"

#include "log.h"

using namespace std::literals;
using namespace muse;
using namespace muse::async;
using namespace muse::draw;
using namespace mu::notation;

namespace mu::print {
Promise<Ret> QtPrintProvider::printNotation(const INotationPtr notation)
{
    return Promise<Ret>([&](const auto resolve) {
        IF_ASSERT_FAILED(notation) {
            return resolve(muse::make_ret(Ret::Code::InternalError, "can't print null notation"s));
        }

        const INotationPaintingPtr painting = notation->painting();

        SizeF pageSizeInch = painting->pageSizeInch();
        auto printer = std::make_unique<QPrinter>(QPrinter::HighResolution);
        QPageSize ps(QPageSize::id(pageSizeInch.toQSizeF(), QPageSize::Inch));
        printer->setPageSize(ps);
        printer->setPageOrientation(pageSizeInch.width() > pageSizeInch.height() ? QPageLayout::Landscape : QPageLayout::Portrait);

        //printer->setCreator("MuseScore Studio Version: " VERSION);
        printer->setFullPage(true);
        if (!printer->setPageMargins(QMarginsF())) {
            LOGW() << "unable to clear printer margins";
        }

        printer->setDocName(notation->projectWorkTitleAndPartName());
        printer->setOutputFormat(QPrinter::NativeFormat);
        printer->setFromTo(1, painting->pageCount());

        auto* printDialog = new QPrintDialog(printer.get());

        QObject::connect(printDialog, &QPrintDialog::finished,
                         [=, printer = std::move(printer)] (const int dlgCode) {
            printDialog->deleteLater();

            if (dlgCode == QDialog::Rejected) {
                (void)resolve(make_ret(Ret::Code::Cancel));
                return;
            }

            DO_ASSERT(dlgCode == QDialog::Accepted);

            Painter painter(printer.get(), "print");

            INotationPainting::Options opt;
            opt.fromPage = printer->fromPage() - 1;
            opt.toPage = printer->toPage() - 1;
            // See https://doc.qt.io/qt-5/qprinter.html#supportsMultipleCopies
            opt.copyCount = printer->supportsMultipleCopies() ? 1 : printer->copyCount();
            opt.deviceDpi = printer->logicalDpiX();
            opt.onNewPage = [&]() { printer->newPage(); };

            painting->paintPrint(&painter, opt);

            painter.endDraw();

            (void)resolve(muse::make_ok());
        });

        printDialog->open();

        return Promise<Ret>::dummy_result();
    }, PromiseType::AsyncByBody);
}
}
