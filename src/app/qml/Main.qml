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

    function toggleFullScreen() {
        root.visibility = root.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen;
    }

    Shortcut {
        sequences: ["Backspace", StandardKey.Undo]
        enabled: Controller.canUndo
        onActivated: Controller.undo()
    }
    Shortcut {
        sequence: "F"
        onActivated: root.toggleFullScreen()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (root.visibility === Window.FullScreen) {
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
                onTriggered: Controller.closeSession()
            }
        ]

        Loader {
            anchors.fill: parent
            sourceComponent: {
                switch (Controller.state) {
                case Controller.Screening:
                    return screeningComponent;
                case Controller.Duels:
                    return duelComponent;
                case Controller.Summary:
                    return summaryComponent;
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
    }
}
