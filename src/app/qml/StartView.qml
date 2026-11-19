// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

Item {
    id: view

    property bool showSetup: Controller.state === Controller.Setup

    // Return confirms the visible default action.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: Controller.state === Controller.ResumePrompt || view.showSetup
        onActivated: {
            if (view.showSetup) {
                Controller.startSession(topNBox.value, skipBox.checked);
            } else {
                Controller.resumeSession();
            }
        }
    }

    FolderDialog {
        id: folderDialog
        title: i18nc("@title:window", "Open Image Folder")
        onAccepted: Controller.openDirectory(folderDialog.selectedFolder)
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 30)
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            visible: Controller.errorMessage.length > 0
            text: Controller.errorMessage
        }

        Kirigami.PlaceholderMessage {
            Layout.fillWidth: true
            visible: Controller.state === Controller.Welcome
            icon.name: "folder-pictures"
            text: i18n("Open a folder with similar images")
            explanation: i18n("Pick the best images by comparing them in pairs.")
            helpfulAction: Kirigami.Action {
                icon.name: "document-open-folder"
                text: i18nc("@action:button", "Open Folder…")
                onTriggered: folderDialog.open()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: Controller.state === Controller.ResumePrompt && !view.showSetup
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                text: i18n("Continue the previous session?")
                wrapMode: Text.Wrap
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: i18np("The folder contains a session with one decision.", "The folder contains a session with %1 decisions.", Controller.storedEventCount)
                wrapMode: Text.Wrap
            }
            RowLayout {
                QQC2.Button {
                    icon.name: "media-playback-start"
                    text: i18nc("@action:button", "Continue")
                    onClicked: Controller.resumeSession()
                }
                QQC2.Button {
                    icon.name: "document-new"
                    text: i18nc("@action:button", "Start New Session")
                    onClicked: view.showSetup = true
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true
            visible: view.showSetup

            QQC2.Label {
                Kirigami.FormData.label: i18nc("@label", "Folder:")
                text: Controller.directory
                elide: Text.ElideMiddle
                Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            }
            QQC2.SpinBox {
                id: topNBox
                Kirigami.FormData.label: i18nc("@label:spinbox", "Images to keep:")
                from: 1
                to: 9999
                value: 5
            }
            QQC2.CheckBox {
                id: skipBox
                text: i18nc("@option:check", "Skip screening")
            }
            QQC2.Label {
                visible: Controller.state === Controller.ResumePrompt
                text: i18n("The previous session file is kept as a backup.")
                font: Kirigami.Theme.smallFont
            }
            QQC2.Button {
                icon.name: "media-playback-start"
                text: i18nc("@action:button", "Start")
                onClicked: Controller.startSession(topNBox.value, skipBox.checked)
            }
        }
    }
}
