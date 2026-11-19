// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// Report after applying.
ColumnLayout {
    spacing: Kirigami.Units.largeSpacing

    Item {
        Layout.fillHeight: true
    }

    Kirigami.PlaceholderMessage {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 40
        Layout.alignment: Qt.AlignHCenter
        icon.name: Controller.applyErrors.length > 0 || Controller.applyCancelled ? "dialog-warning" : "dialog-ok"
        text: Controller.applyCancelled ? i18np("Cancelled after one of %2 files", "Cancelled after %1 of %2 files", Controller.movedCount, Controller.applyTotal) : i18np("Moved one file", "Moved %1 files", Controller.movedCount)
        explanation: Controller.applyCancelled ? i18n("The moved files are listed in .seedpick-apply.log.jsonl. The session file stays. Resuming it ranks the remaining images again.") : Controller.applyErrors.length > 0 ? i18np("One file could not be moved and stays in place.", "%1 files could not be moved and stay in place.", Controller.applyErrors.length) : (Controller.archivedSession.length > 0 ? i18n("The session file was kept as %1.", Controller.archivedSession) : i18n("The session file was deleted."))
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 40
        Layout.preferredHeight: Kirigami.Units.gridUnit * 10
        Layout.alignment: Qt.AlignHCenter
        visible: Controller.applyErrors.length > 0

        ListView {
            model: Controller.applyErrors
            delegate: QQC2.Label {
                required property string modelData
                width: ListView.view.width
                text: modelData
                wrapMode: Text.Wrap
            }
        }
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 40
        Layout.alignment: Qt.AlignHCenter
        type: Kirigami.MessageType.Error
        visible: Controller.errorMessage.length > 0
        text: Controller.errorMessage
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter

        QQC2.Button {
            icon.name: "document-open-folder"
            text: i18nc("@action:button", "Open Folder")
            onClicked: Qt.openUrlExternally(Controller.directoryUrl)
        }
        QQC2.Button {
            icon.name: "dialog-close"
            text: i18nc("@action:button", "Close")
            onClicked: Controller.closeSession()
            Component.onCompleted: forceActiveFocus()
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
