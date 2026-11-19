// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// Changes are stored right away.
Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title", "Settings")

    function saveFolder(field, setter) {
        if (Controller.isValidFolderName(field.text)) {
            setter(field.text);
            Config.save();
        }
    }

    actions: [
        Kirigami.Action {
            icon.name: "edit-reset"
            text: i18nc("@action:button", "Defaults")
            onTriggered: {
                Config.setDefaults();
                Config.save();
            }
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: Controller.overriddenSettings.length > 0
            type: Kirigami.MessageType.Information
            text: i18n("Command line options override these settings for this run: %1", Controller.overriddenSettings.join(", "))
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "New Sessions")
            }
            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Images to keep:")
                from: 1
                to: 9999
                editable: true
                value: Config.topN
                onValueModified: {
                    Config.topN = value;
                    Config.save();
                }
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Skip screening")
                checked: Config.skipScreening
                onToggled: {
                    Config.skipScreening = checked;
                    Config.save();
                }
            }
            QQC2.SpinBox {
                Kirigami.FormData.label: i18nc("@label:spinbox", "Minimum duels per image:")
                from: 1
                to: 10
                value: Config.minDuels
                onValueModified: {
                    Config.minDuels = value;
                    Config.save();
                }
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Duels")
            }
            QQC2.SpinBox {
                id: thresholdBox
                Kirigami.FormData.label: i18nc("@label:spinbox", "Done below:")
                from: 1
                to: 50
                value: Math.round(Config.confidenceThreshold * 100)
                textFromValue: (value, locale) => i18nc("@item:valuesuffix percentage", "%1 %", value)
                valueFromText: (text, locale) => parseInt(text)
                onValueModified: {
                    Config.confidenceThreshold = value / 100;
                    Config.save();
                }
            }
            QQC2.Label {
                Layout.maximumWidth: Kirigami.Units.gridUnit * 20
                text: i18n("Expected share of wrong picks in the selection. Lower values need more duels.")
                wrapMode: Text.Wrap
                font: Kirigami.Theme.smallFont
                opacity: 0.7
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18nc("@title:group", "Apply")
            }
            QQC2.TextField {
                id: keepField
                Kirigami.FormData.label: i18nc("@label:textbox", "Folder for kept images:")
                text: Config.keepDirName
                onEditingFinished: page.saveFolder(keepField, name => Config.keepDirName = name)
            }
            QQC2.TextField {
                id: restField
                Kirigami.FormData.label: i18nc("@label:textbox", "Folder for the rest:")
                text: Config.restDirName
                onEditingFinished: page.saveFolder(restField, name => Config.restDirName = name)
            }
            QQC2.TextField {
                id: rejectField
                Kirigami.FormData.label: i18nc("@label:textbox", "Folder for rejected images:")
                enabled: !Config.rejectToTrash
                text: Config.rejectDirName
                onEditingFinished: page.saveFolder(rejectField, name => Config.rejectDirName = name)
            }
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                visible: !Controller.isValidFolderName(keepField.text) || !Controller.isValidFolderName(restField.text) || !Controller.isValidFolderName(rejectField.text)
                text: i18n("A folder name must not be empty, contain \"/\" or start or end with a space.")
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Move rejected images to the trash")
                checked: Config.rejectToTrash
                onToggled: {
                    Config.rejectToTrash = checked;
                    Config.save();
                }
            }
            QQC2.TextField {
                id: sidecarField
                Kirigami.FormData.label: i18nc("@label:textbox", "Sidecar file extensions:")
                text: Config.sidecarExtensions.join(", ")
                onEditingFinished: {
                    Config.sidecarExtensions = sidecarField.text.split(",").map(item => item.trim()).filter(item => item.length > 0);
                    Config.save();
                }
            }
            QQC2.CheckBox {
                text: i18nc("@option:check", "Keep the session file after applying")
                checked: Config.keepSessionAfterApply
                onToggled: {
                    Config.keepSessionAfterApply = checked;
                    Config.save();
                }
            }
        }
    }
}
