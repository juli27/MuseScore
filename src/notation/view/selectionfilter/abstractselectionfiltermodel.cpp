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

#include "abstractselectionfiltermodel.h"

#include "log.h"
#include "translation.h"

using namespace mu::notation;

AbstractSelectionFilterModel::AbstractSelectionFilterModel(QObject* parent)
    : QAbstractListModel(parent), muse::Injectable(muse::iocCtxForQmlObject(this))
{
}

void AbstractSelectionFilterModel::load()
{
    TRACEFUNC;

    beginResetModel();
    loadTypes();
    endResetModel();

    onNotationChanged();

    globalContext()->currentNotationChanged().onNotify(this, [this]() {
        onNotationChanged();
    });

    emit maskStatesChanged();
}

void AbstractSelectionFilterModel::selectAll()
{
    setFiltered(getAllMask(), true);
    notifyAboutDataChanged(this->index(0), getAllMask());
}

void AbstractSelectionFilterModel::clearAll()
{
    setFiltered(getAllMask(), false);
    notifyAboutDataChanged(this->index(0), getAllMask());
}

QVariant AbstractSelectionFilterModel::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row < 0 || row >= rowCount()) {
        return {};
    }

    const SelectionFilterTypesVariant type = m_types.at(row);

    switch (role) {
    case IsAllowedRole:
        return isAllowed(type);

    case TitleRole:
        return titleForType(type);

    case IsSelectedRole:
        return isFiltered(type);

    case IsIndeterminateRole:
        return isIndeterminate(type);
    }

    return {};
}

bool AbstractSelectionFilterModel::setData(const QModelIndex& index, const QVariant& data, int role)
{
    if (role != IsSelectedRole) {
        return false;
    }

    if (!currentNotationSelectionFilter()) {
        return false;
    }

    int row = index.row();
    if (row < 0 || row >= rowCount()) {
        return false;
    }

    const SelectionFilterTypesVariant type = m_types.at(row);
    const bool filtered = data.toBool();

    setFiltered(type, filtered);
    notifyAboutDataChanged(index, type);
    return true;
}

int AbstractSelectionFilterModel::rowCount(const QModelIndex&) const
{
    return m_types.size();
}

QHash<int, QByteArray> AbstractSelectionFilterModel::roleNames() const
{
    return {
        { IsAllowedRole, "isAllowed" },
        { TitleRole, "title" },
        { IsSelectedRole, "isSelected" },
        { IsIndeterminateRole, "isIndeterminate" }
    };
}

bool AbstractSelectionFilterModel::enabled() const
{
    return currentNotation() != nullptr;
}

bool AbstractSelectionFilterModel::isAllSelected() const
{
    return isFiltered(getAllMask());
}

bool AbstractSelectionFilterModel::isNoneSelected() const
{
    return isFiltered(getNoneMask());
}

INotationPtr AbstractSelectionFilterModel::currentNotation() const
{
    return globalContext()->currentNotation();
}

INotationInteractionPtr AbstractSelectionFilterModel::currentNotationInteraction() const
{
    return currentNotation() ? currentNotation()->interaction() : nullptr;
}

INotationSelectionFilterPtr AbstractSelectionFilterModel::currentNotationSelectionFilter() const
{
    return currentNotationInteraction() ? currentNotationInteraction()->selectionFilter() : nullptr;
}

bool AbstractSelectionFilterModel::isFiltered(const SelectionFilterTypesVariant& variant) const
{
    return currentNotationSelectionFilter() ? currentNotationSelectionFilter()->isSelectionTypeFiltered(variant) : false;
}

void AbstractSelectionFilterModel::setFiltered(const SelectionFilterTypesVariant& variant, bool filtered)
{
    if (currentNotationSelectionFilter()) {
        currentNotationSelectionFilter()->setSelectionTypeFiltered(variant, filtered);
    }
}

bool AbstractSelectionFilterModel::isIndeterminate(const SelectionFilterTypesVariant& variant) const
{
    if (variant == getAllMask()) {
        return !isFiltered(getAllMask()) && !isFiltered(getNoneMask());
    }
    return false;
}

void AbstractSelectionFilterModel::onSelectionChanged()
{
    if (!currentNotation()) {
        return;
    }
    emit dataChanged(index(0), index(rowCount() - 1), { IsSelectedRole, IsIndeterminateRole });
    emit enabledChanged();
}

void AbstractSelectionFilterModel::onNotationChanged()
{
    onSelectionChanged();

    const INotationInteractionPtr interaction = currentNotationInteraction();
    if (!interaction) {
        return;
    }
    interaction->selectionChanged().onNotify(this, [this]() {
        onSelectionChanged();
    });
}

void AbstractSelectionFilterModel::notifyAboutDataChanged(const QModelIndex& index, const SelectionFilterTypesVariant& variant)
{
    if (variant == getAllMask()) {
        emit dataChanged(this->index(0), this->index(rowCount() - 1), { IsSelectedRole, IsIndeterminateRole });
        emit maskStatesChanged();
        return;
    }

    emit dataChanged(this->index(0), this->index(0), { IsSelectedRole, IsIndeterminateRole });
    emit dataChanged(index, index, { IsSelectedRole });
    emit maskStatesChanged();
}
