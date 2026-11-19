// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// Phase 3: the result in three columns, manual corrections and Apply.
Item {
    id: view

    // Drag state shared by the columns.
    property string dragFile: ""
    property url dragUrl
    property int dragRole: -1
    property int hoverRole: -1
    property point dragPosition

    readonly property var columns: [keepColumn, restColumn, rejectColumn]

    function roleAt(scenePosition) {
        for (const column of columns) {
            const local = column.mapFromItem(null, scenePosition.x, scenePosition.y);
            if (column.contains(local)) {
                return column.role;
            }
        }
        return -1;
    }

    function beginDrag(file, url, role, scenePosition) {
        dragFile = file;
        dragUrl = url;
        dragRole = role;
        moveDrag(scenePosition);
    }

    function moveDrag(scenePosition) {
        dragPosition = view.mapFromItem(null, scenePosition.x, scenePosition.y);
        hoverRole = roleAt(scenePosition);
    }

    function endDrag() {
        const file = dragFile;
        const role = hoverRole;
        const changed = role >= 0 && role !== dragRole;
        dragFile = "";
        dragRole = -1;
        hoverRole = -1;
        if (changed) {
            Controller.setRole(file, role);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            type: Kirigami.MessageType.Error
            visible: Controller.errorMessage.length > 0
            text: Controller.errorMessage
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing
            enabled: !Controller.applying

            SummaryColumn {
                id: keepColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                board: view
                role: Controller.Keep
                title: i18nc("@title result column", "Keep")
                folder: Controller.keepFolder
                items: Controller.keepItems
            }
            SummaryColumn {
                id: restColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                board: view
                role: Controller.Rest
                title: i18nc("@title result column", "Rest")
                folder: Controller.restFolder
                items: Controller.restItems
            }
            SummaryColumn {
                id: rejectColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                board: view
                role: Controller.Reject
                title: i18nc("@title result column", "Reject")
                folder: Controller.rejectToTrash ? i18nc("@info result folder", "Trash") : Controller.rejectFolder
                items: Controller.rejectItems
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.largeSpacing

            QQC2.Label {
                Layout.fillWidth: true
                visible: !Controller.applying
                text: i18nc("@info:usagetip", "Drag images to another column or right-click them. Nothing is moved before Apply.")
                opacity: 0.7
                wrapMode: Text.Wrap
            }
            QQC2.ProgressBar {
                Layout.fillWidth: true
                visible: Controller.applying
                from: 0
                to: Math.max(1, Controller.applyTotal)
                value: Controller.applyDone
            }
            QQC2.Button {
                icon.name: "dialog-ok-apply"
                text: i18nc("@action:button", "Apply…")
                enabled: !Controller.applying
                onClicked: confirmDialog.open()
            }
        }
    }

    Image {
        x: view.dragPosition.x - width / 2
        y: view.dragPosition.y - height / 2
        width: Kirigami.Units.gridUnit * 5
        height: width
        visible: view.dragFile.length > 0
        source: view.dragFile.length > 0 ? view.dragUrl : ""
        sourceSize: Qt.size(Math.round(Kirigami.Units.gridUnit * 7 * Screen.devicePixelRatio), Math.round(Kirigami.Units.gridUnit * 7 * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        opacity: 0.8
        z: 10
    }

    Kirigami.PromptDialog {
        id: confirmDialog
        title: i18nc("@title:dialog", "Apply the Result?")
        subtitle: [i18ncp("@info", "Keep: one image → %2", "Keep: %1 images → %2", Controller.keepItems.length, Controller.keepFolder),
                   i18ncp("@info", "Rest: one image → %2", "Rest: %1 images → %2", Controller.restItems.length, Controller.restFolder),
                   i18ncp("@info", "Reject: one image → %2", "Reject: %1 images → %2", Controller.rejectItems.length, Controller.rejectToTrash ? i18nc("@info result folder", "Trash") : Controller.rejectFolder),
                   "",
                   i18n("Sidecar files move along. Existing files are never overwritten.")].join("\n")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                icon.name: "dialog-ok-apply"
                text: i18nc("@action:button", "Move Files")
                onTriggered: {
                    confirmDialog.close();
                    Controller.apply();
                }
            }
        ]
    }
}
