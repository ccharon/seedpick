// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.ccharon.seedpick

// Ranking only. Moving files belongs to the summary page.
ColumnLayout {
    spacing: Kirigami.Units.largeSpacing

    Kirigami.Heading {
        Layout.margins: Kirigami.Units.largeSpacing
        text: i18ncp("@title", "Best image", "Best %1 images", Controller.topN)
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true

        ListView {
            model: Controller.rankingFiles
            delegate: QQC2.ItemDelegate {
                required property int index
                required property string modelData
                width: ListView.view.width
                text: (index + 1) + ". " + modelData
                font.bold: index < Controller.topN
            }
        }
    }
}
