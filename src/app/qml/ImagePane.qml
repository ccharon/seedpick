// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Shows one image scaled to fit. Preloading must use the same decodeSize to hit the image cache.
Rectangle {
    id: pane

    property url source
    property string label
    property size decodeSize

    color: "black"

    Image {
        id: image

        anchors.fill: parent
        // Before layout the size is empty and the provider would decode at full resolution.
        source: pane.decodeSize.width > 1 && pane.decodeSize.height > 1 ? pane.source : ""
        sourceSize: pane.decodeSize
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        // The image cache holds decoded images, a second copy in QML's cache would only cost memory.
        cache: false
        smooth: true
        mipmap: true
    }

    QQC2.BusyIndicator {
        anchors.centerIn: parent
        running: image.status === Image.Loading
    }

    QQC2.Label {
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: Kirigami.Units.smallSpacing
        }
        visible: text.length > 0
        text: pane.label
        color: "white"
        style: Text.Outline
        styleColor: "black"
    }
}
