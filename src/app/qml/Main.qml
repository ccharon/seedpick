// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

Kirigami.ApplicationWindow {
    id: root

    title: Controller.directory.length > 0 ? i18nc("@title:window %1 is a folder path", "%1 - Seedpick", Controller.directory) : i18nc("@title:window", "Seedpick")
    width: Kirigami.Units.gridUnit * 70
    height: Kirigami.Units.gridUnit * 45
    minimumWidth: Kirigami.Units.gridUnit * 30
    minimumHeight: Kirigami.Units.gridUnit * 20

    // Page shortcuts stay off while a layer such as the settings covers the page.
    readonly property bool pageKeysActive: pageStack.layers.depth <= 1

    function openSettings() {
        if (root.pageKeysActive) {
            root.pageStack.layers.push(settingsComponent);
        }
    }

    function toggleFullScreen() {
        root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }

    Shortcut {
        sequences: ["Backspace", StandardKey.Undo]
        enabled: Controller.canUndo && root.pageKeysActive
        onActivated: Controller.undo()
    }
    Shortcut {
        sequence: StandardKey.Quit
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "F"
        enabled: root.pageKeysActive
        onActivated: root.toggleFullScreen()
    }
    Shortcut {
        sequence: StandardKey.Preferences
        onActivated: root.openSettings()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (!root.pageKeysActive) {
                root.pageStack.layers.pop();
            } else if (root.visibility === Window.FullScreen) {
                root.visibility = Window.Windowed;
            } else if (Controller.state !== Controller.Welcome) {
                Controller.closeSession();
            }
        }
    }

    pageStack.globalToolBar.style: root.visibility === Window.FullScreen ? Kirigami.ApplicationHeaderStyle.None : Kirigami.ApplicationHeaderStyle.Auto
    pageStack.initialPage: Kirigami.Page {
        id: page

        padding: 0
        title: {
            switch (Controller.state) {
            case Controller.Screening:
                return i18nc("@title", "Screening");
            case Controller.Duels:
                return i18nc("@title", "Duels");
            case Controller.Summary:
                return i18nc("@title", "Result");
            case Controller.Applied:
                return i18nc("@title", "Done");
            default:
                return i18nc("@title", "Seedpick");
            }
        }

        actions: [
            Kirigami.Action {
                icon.name: "edit-undo"
                text: i18nc("@action:button", "Undo")
                enabled: Controller.canUndo
                onTriggered: Controller.undo()
            },
            Kirigami.Action {
                icon.name: "view-fullscreen"
                text: i18nc("@action:button", "Full Screen")
                displayHint: Kirigami.DisplayHint.IconOnly
                onTriggered: root.toggleFullScreen()
            },
            Kirigami.Action {
                icon.name: "media-playback-pause"
                text: i18nc("@action:button", "Pause")
                visible: Controller.state === Controller.Screening || Controller.state === Controller.Duels || Controller.state === Controller.Summary
                enabled: !Controller.applying
                onTriggered: Controller.closeSession()
            },
            Kirigami.Action {
                icon.name: "configure"
                text: i18nc("@action:button", "Settings")
                displayHint: Kirigami.DisplayHint.IconOnly
                onTriggered: root.openSettings()
            }
        ]

        Loader {
            anchors.fill: parent
            enabled: root.pageKeysActive
            sourceComponent: {
                switch (Controller.state) {
                case Controller.Screening:
                    return screeningComponent;
                case Controller.Duels:
                    return duelComponent;
                case Controller.Summary:
                    return summaryComponent;
                case Controller.Applied:
                    return appliedComponent;
                default:
                    return startComponent;
                }
            }
        }

        Component {
            id: startComponent
            StartView {}
        }
        Component {
            id: screeningComponent
            ScreeningView {}
        }
        Component {
            id: duelComponent
            DuelView {}
        }
        Component {
            id: summaryComponent
            SummaryView {}
        }
        Component {
            id: appliedComponent
            AppliedView {}
        }
        Component {
            id: settingsComponent
            SettingsPage {}
        }
    }
}
