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
#ifndef MUSE_API_NAVIGATIONAPI_H
#define MUSE_API_NAVIGATIONAPI_H

#include <QString>
#include <QJSValue>

#include "api/apiobject.h"
#include "modularity/ioc.h"
#include "actions/iactionsdispatcher.h"
#include "ui/inavigationcontroller.h"

namespace muse::api {
class NavigationApi : public api::ApiObject
{
    Q_OBJECT

    Inject<actions::IActionsDispatcher> dispatcher = { this };
    Inject<ui::INavigationController> navigation = { this };

public:
    explicit NavigationApi(api::IApiEngine* e);
    ~NavigationApi();

    Q_INVOKABLE void nextPanel();
    Q_INVOKABLE void prevPanel();
    Q_INVOKABLE void right();
    Q_INVOKABLE void left();
    Q_INVOKABLE void up();
    Q_INVOKABLE void down();
    Q_INVOKABLE void escape();
    Q_INVOKABLE bool goToControl(const QString& section, const QString& panel, const QJSValue& controlNameOrIndex);
    Q_INVOKABLE void trigger();
    Q_INVOKABLE bool triggerControl(const QString& section, const QString& panel, const QJSValue& controlNameOrIndex);

    Q_INVOKABLE QString activeSection() const;
    Q_INVOKABLE QString activePanel() const;
    Q_INVOKABLE QString activeControl() const;

    Q_INVOKABLE QJSValue sections() const;                          // array of sections
    Q_INVOKABLE QJSValue panels(const QString& sectionName) const;  // array of panels
    Q_INVOKABLE QJSValue controls(const QString& sectionName, const QString& panelName) const;  // array of controls

    Q_INVOKABLE void dump() const;
};
}

#endif // MUSE_API_NAVIGATIONAPI_H
