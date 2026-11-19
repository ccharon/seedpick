// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// One result folder of the summary. Tiles can be dragged to another column or moved via the context menu.
QQC2.Frame {
    id: column

    required property int role
    required property string title
    required property string folder
    required property var items
    // The SummaryView that tracks the drag.
    required property Item board

    readonly property bool dropTarget: board.dragFile.length > 0 && board.hoverRole === role && board.dragRole !== role
    readonly property int thumbSize: Kirigami.Units.gridUnit * 7

    padding: Kirigami.Units.smallSpacing

    background: Rectangle {
        color: column.dropTarget ? Kirigami.Theme.highlightColor : Kirigami.Theme.alternateBackgroundColor
        opacity: column.dropTarget ? 0.3 : 1
        radius: Kirigami.Units.cornerRadius
        border.color: column.dropTarget ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                level: 3
                text: column.title
            }
            QQC2.Label {
                text: column.items.length
                font.bold: true
            }
            Item {
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: column.folder
                opacity: 0.7
                font: Kirigami.Theme.smallFont
            }
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: grid

                clip: true
                model: column.items
                cellWidth: Math.max(column.thumbSize, width / Math.max(1, Math.floor(width / column.thumbSize)))
                cellHeight: column.thumbSize + Kirigami.Units.gridUnit * 1.5

                delegate: Item {
                    id: tile

                    required property var modelData

                    width: grid.cellWidth
                    height: grid.cellHeight
                    opacity: column.board.dragFile === modelData.file ? 0.3 : 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing
                        spacing: 0

                        Image {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            source: tile.modelData.url
                            sourceSize: Qt.size(Math.round(column.thumbSize * Screen.devicePixelRatio), Math.round(column.thumbSize * Screen.devicePixelRatio))
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false

                            Kirigami.Icon {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                width: Kirigami.Units.iconSizes.small
                                height: width
                                visible: tile.modelData.overridden
                                source: "hand"
                                QQC2.ToolTip.text: i18nc("@info:tooltip", "Moved by hand")
                                QQC2.ToolTip.visible: overriddenHover.hovered
                                HoverHandler {
                                    id: overriddenHover
                                }
                            }
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: tile.modelData.file
                            elide: Text.ElideMiddle
                            horizontalAlignment: Text.AlignHCenter
                            font: Kirigami.Theme.smallFont
                        }
                    }

                    DragHandler {
                        id: dragHandler
                        target: null
                        onActiveChanged: {
                            if (active) {
                                column.board.beginDrag(tile.modelData.file, tile.modelData.url, column.role, centroid.scenePosition);
                            } else {
                                column.board.endDrag();
                            }
                        }
                        onCentroidChanged: {
                            if (active) {
                                column.board.moveDrag(centroid.scenePosition);
                            }
                        }
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: menu.popup()
                    }
                    TapHandler {
                        acceptedDevices: PointerDevice.TouchScreen
                        onLongPressed: menu.popup()
                    }

                    QQC2.Menu {
                        id: menu
                        QQC2.MenuItem {
                            icon.name: "starred-symbolic"
                            text: i18nc("@action:inmenu", "Move to Keep")
                            enabled: column.role !== Controller.Keep
                            onTriggered: Controller.setRole(tile.modelData.file, Controller.Keep)
                        }
                        QQC2.MenuItem {
                            icon.name: "folder"
                            text: i18nc("@action:inmenu", "Move to Rest")
                            enabled: column.role !== Controller.Rest
                            onTriggered: Controller.setRole(tile.modelData.file, Controller.Rest)
                        }
                        QQC2.MenuItem {
                            icon.name: "edit-delete"
                            text: i18nc("@action:inmenu", "Move to Reject")
                            enabled: column.role !== Controller.Reject
                            onTriggered: Controller.setRole(tile.modelData.file, Controller.Reject)
                        }
                    }
                }
            }
        }
    }
}
