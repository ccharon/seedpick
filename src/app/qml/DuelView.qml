// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

ColumnLayout {
    id: view

    readonly property size decodeSize: Qt.size(Math.round(leftPane.width * Screen.devicePixelRatio), Math.round(leftPane.height * Screen.devicePixelRatio))
    readonly property bool canFinish: Controller.done || Controller.budgetExceeded

    spacing: 0

    // Both images zoom and pan together. A new pair starts unzoomed.
    ZoomState {
        id: zoom
    }
    readonly property string pair: Controller.leftFile + "\n" + Controller.rightFile
    onPairChanged: zoom.reset()

    Shortcut {
        sequences: ["+", "=", StandardKey.ZoomIn]
        enabled: view.enabled
        onActivated: zoom.zoomAt(1.5, 0, 0)
    }
    Shortcut {
        sequences: ["-", StandardKey.ZoomOut]
        enabled: view.enabled
        onActivated: zoom.zoomAt(1 / 1.5, 0, 0)
    }
    Shortcut {
        sequence: "0"
        enabled: view.enabled
        onActivated: zoom.reset()
    }

    Shortcut {
        sequence: "Left"
        enabled: view.enabled
        onActivated: Controller.chooseLeft()
    }
    Shortcut {
        sequence: "Right"
        enabled: view.enabled
        onActivated: Controller.chooseRight()
    }
    Shortcut {
        sequence: "Up"
        enabled: view.enabled
        onActivated: Controller.chooseTie()
    }
    Shortcut {
        sequence: "1"
        enabled: view.enabled
        onActivated: Controller.rejectLeft()
    }
    Shortcut {
        sequence: "2"
        enabled: view.enabled
        onActivated: Controller.rejectRight()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: view.enabled && view.canFinish
        onActivated: Controller.finish()
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Kirigami.Units.smallSpacing

        ImagePane {
            id: leftPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            source: Controller.leftUrl
            label: Controller.leftFile
            decodeSize: view.decodeSize
            zoomState: zoom
            showInfo: Config.showImageInfo
            info: Controller.imageInfo(Controller.leftFile)
            compareInfo: Controller.imageInfo(Controller.rightFile)
        }
        ImagePane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            source: Controller.rightUrl
            label: Controller.rightFile
            decodeSize: view.decodeSize
            zoomState: zoom
            showInfo: Config.showImageInfo
            info: Controller.imageInfo(Controller.rightFile)
            compareInfo: Controller.imageInfo(Controller.leftFile)
        }
    }

    Preloader {
        urls: Controller.preloadUrls
        decodeSize: view.decodeSize
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

    Kirigami.InlineMessage {
        Layout.fillWidth: true
        type: Kirigami.MessageType.Positive
        visible: view.canFinish
        text: Controller.done ? i18n("The selection is settled.") : i18n("Good enough. Finish now?")
        actions: [
            Kirigami.Action {
                icon.name: "dialog-ok"
                text: i18nc("@action:button", "Finish (Enter)")
                onTriggered: Controller.finish()
            }
        ]
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.margins: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.largeSpacing

        QQC2.Label {
            text: Controller.done ? i18nc("@info:status", "%1 comparisons", Controller.comparisons) : i18ncp("@info:status", "%2 comparisons, about one remaining", "%2 comparisons, about %1 remaining", Controller.estimatedRemaining, Controller.comparisons)
        }

        QQC2.Label {
            text: i18nc("@label", "Top %1:", Controller.topN)
            opacity: 0.7
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 3
            orientation: ListView.Horizontal
            spacing: Kirigami.Units.smallSpacing
            interactive: false
            clip: true
            model: Controller.topUrls
            delegate: Image {
                required property url modelData
                height: ListView.view.height
                width: height * 0.75
                source: modelData
                sourceSize: Qt.size(Math.round(width * Screen.devicePixelRatio), Math.round(height * Screen.devicePixelRatio))
                fillMode: Image.PreserveAspectFit
                asynchronous: true
            }
        }

        QQC2.Label {
            text: i18nc("@info:usagetip", "← → choose   ↑ tie   1 / 2 reject   + − 0 zoom   I info   Backspace undo")
            opacity: 0.7
        }
    }
}
