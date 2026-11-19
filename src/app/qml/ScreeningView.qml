// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

ColumnLayout {
    id: view

    spacing: 0

    Shortcut {
        sequences: ["Right", "Space"]
        enabled: view.enabled
        onActivated: Controller.screenOk()
    }
    Shortcut {
        sequences: ["Down", "Delete"]
        enabled: view.enabled
        onActivated: Controller.screenReject()
    }
    Shortcut {
        sequence: "Left"
        enabled: view.enabled
        onActivated: Controller.showPrevious()
    }

    ImagePane {
        id: pane
        Layout.fillWidth: true
        Layout.fillHeight: true
        source: Controller.currentUrl
        label: Controller.currentFile
        decodeSize: Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))

        Kirigami.Chip {
            anchors {
                top: parent.top
                horizontalCenter: parent.horizontalCenter
                margins: Kirigami.Units.largeSpacing
            }
            visible: Controller.viewingHistory
            closable: false
            checkable: false
            icon.name: Controller.currentRejected ? "dialog-cancel" : "dialog-ok"
            text: Controller.currentRejected ? i18nc("@info", "Already screened: reject") : i18nc("@info", "Already screened: OK")
        }
    }

    Preloader {
        urls: Controller.preloadUrls
        decodeSize: pane.decodeSize
    }

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Error
        visible: Controller.errorMessage.length > 0
        text: Controller.errorMessage
    }

    ReconcileMessage {
        Layout.fillWidth: true
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing

        QQC2.Label {
            text: i18nc("@info:status", "Screened %1 of %2", Controller.screenedCount, Controller.imageCount)
        }
        Item {
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: Controller.viewingHistory ? i18nc("@info:usagetip", "→ back to current image   ← previous") : i18nc("@info:usagetip", "→ / Space: OK   ↓ / Del: reject   ← previous   Backspace: undo")
            opacity: 0.7
        }
    }
}
