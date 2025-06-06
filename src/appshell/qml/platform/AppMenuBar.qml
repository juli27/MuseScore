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
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents 1.0
import MuseScore.AppShell 1.0

Row {
    id: root

    property alias appWindow: appMenuModel.appWindow

    Component.onCompleted: {
        appMenuModel.load()
    }

    AppMenuModel {
        id: appMenuModel

        appMenuAreaRect: Qt.rect(root.x, root.y, root.width, root.height)
        openedMenuAreaRect: prv.openedArea(menuLoader)

        onOpenMenuRequested: function(menuId) {
            prv.openMenu(menuId)
        }

        onCloseOpenedMenuRequested: {
            menuLoader.close()
        }
    }

    AccessibleItem {
        id: panelAccessibleInfo

        visualItem: root
        role: MUAccessible.Panel
        name: qsTrc("appshell", "Application menu")
    }

    QtObject {
        id: prv

        property var openedMenu: null
        property bool needRestoreNavigationAfterClose: false
        property string lastOpenedMenuId: ""

        function openMenu(menuId, byHover = false) {
            for (var i = 0; i < menus.count; ++i) {
            // for (const item in root.children) {
                var item = menus.itemAt(i)
                if (item?.menuId === menuId) {
                    needRestoreNavigationAfterClose = true
                    lastOpenedMenuId = menuId

                    if (!byHover) {
                        if (menuLoader.isMenuOpened && menuLoader.parent === item) {
                            menuLoader.close()
                            return
                        }
                    }

                    menuLoader.menuId = menuId
                    menuLoader.parent = item
                    menuLoader.accessibleName = item.title

                    Qt.callLater(menuLoader.open, item.item.subitems)

                    return
                }
            }
        }

        function openedArea(menuLoader) {
            if (menuLoader.isMenuOpened) {
                if (menuLoader.menu.subMenuLoader && menuLoader.menu.subMenuLoader.isMenuOpened) {
                    return openedArea(menuLoader.menu.subMenuLoader)
                }
                return Qt.rect(menuLoader.menu.x, menuLoader.menu.y, menuLoader.menu.width, menuLoader.menu.height)
            }
            return Qt.rect(0, 0, 0, 0)
        }

        function hasNavigatedItem() {
            return appMenuModel.highlightedMenuId !== ""
        }
    }

    Repeater {
        id: menus

        model: appMenuModel

        delegate: FlatButton {
            id: menuButton

            required property QtObject model
            required property int index

            readonly property QtObject item: model.itemRole
            readonly property string menuId: item.id
            readonly property string title: item.title
            readonly property string titleWithMnemonicUnderline: item.titleWithMnemonicUnderline

            readonly property bool isMenuOpened: menuLoader.isMenuOpened && menuLoader.parent === this
            readonly property bool highlight: appMenuModel.highlightedMenuId === menuId

            property int viewIndex: index

            buttonType: FlatButton.TextOnly
            isNarrow: true
            margins: 8
            drawFocusBorderInsideRect: true

            transparent: !isMenuOpened
            accentButton: isMenuOpened

            navigation.accessible.ignored: true

            AccessibleItem {
                id: accessibleInfo

                accessibleParent: panelAccessibleInfo
                visualItem: menuButton
                role: MUAccessible.Button
                name: menuButton.title

                property bool active: menuButton.highlight && !menuButton.isMenuOpened
                onActiveChanged: {
                    if (active) {
                        forceActiveFocus()
                        accessibleInfo.readInfo()
                    } else {
                        accessibleInfo.resetFocus()
                    }
                }

                function readInfo() {
                    accessibleInfo.ignored = false
                    accessibleInfo.focused = true
                }

                function resetFocus() {
                    accessibleInfo.focused = false
                    accessibleInfo.ignored = true
                }
            }

            contentItem: StyledTextLabel {
                id: textLabel

                text: appMenuModel.isNavigationStarted ? menuButton.titleWithMnemonicUnderline : menuButton.title
                textFormat: Text.RichText
                font: ui.theme.defaultFont
            }

            backgroundItem: AppButtonBackground {
                mouseArea: menuButton.mouseArea

                highlight: menuButton.highlight

                color: menuButton.normalColor
            }

            mouseArea.onHoveredChanged: {
                if (!mouseArea.containsMouse) {
                    return
                }

                if (menuLoader.isMenuOpened && menuLoader.parent !== this) {
                    appMenuModel.openMenu(menuId, true)
                }
            }

            onClicked: {
                appMenuModel.openMenu(menuId, false)
            }
        }
    }

    StyledMenuLoader {
        id: menuLoader

        property string menuId: ""
        property bool hasSiblingMenus: true

        onHandleMenuItem: function(itemId) {
            Qt.callLater(appMenuModel.handleMenuItem, itemId)
        }

        onOpenPrevMenu: {
            appMenuModel.openPrevMenu()
        }

        onOpenNextMenu: {
            appMenuModel.openNextMenu()
        }

        onOpened: {
            appMenuModel.openedMenuId = menuLoader.menuId
        }

        onClosed: {
            appMenuModel.openedMenuId = ""
        }
    }
}
