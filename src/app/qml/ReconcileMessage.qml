// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// Reports how the folder changed since the session was saved.
Kirigami.InlineMessage {
    type: Kirigami.MessageType.Warning
    visible: Controller.removedFiles.length + Controller.addedFiles.length + Controller.changedFiles.length > 0
    showCloseButton: true
    onVisibleChanged: {
        if (!visible) {
            Controller.dismissReport();
        }
    }
    text: {
        let parts = [];
        if (Controller.removedFiles.length > 0) {
            parts.push(i18np("One image is missing and no longer counts.", "%1 images are missing and no longer count.", Controller.removedFiles.length));
        }
        if (Controller.addedFiles.length > 0) {
            parts.push(i18np("One new image was added.", "%1 new images were added.", Controller.addedFiles.length));
        }
        if (Controller.changedFiles.length > 0) {
            parts.push(i18np("One image changed since the last session: %2", "%1 images changed since the last session: %2", Controller.changedFiles.length, Controller.changedFiles.join(", ")));
        }
        return parts.join(" ");
    }
}
