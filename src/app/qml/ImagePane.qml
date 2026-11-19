// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// Shows one image scaled to fit, zoomable with the wheel, by dragging and by double click.
// Preloading must use the same decodeSize to hit the image cache.
Rectangle {
    id: pane

    property url source
    property string label
    property size decodeSize
    // Panes that share a ZoomState zoom and pan together.
    property ZoomState zoomState: ZoomState {}

    readonly property real aspect: base.implicitHeight > 0 ? base.implicitWidth / base.implicitHeight : 1
    readonly property real fitWidth: Math.min(width, height * aspect)
    readonly property real fitHeight: fitWidth / aspect

    color: "black"
    clip: true

    Item {
        id: canvas

        width: pane.fitWidth * pane.zoomState.zoom
        height: pane.fitHeight * pane.zoomState.zoom
        x: pane.width / 2 - pane.zoomState.centerX * width
        y: pane.height / 2 - pane.zoomState.centerY * height

        Image {
            id: base

            anchors.fill: parent
            // Before layout the size is empty and the provider would decode at full resolution.
            source: pane.decodeSize.width > 1 && pane.decodeSize.height > 1 ? pane.source : ""
            sourceSize: pane.decodeSize
            asynchronous: true
            // The image cache holds decoded images, a second copy in QML's cache would only cost memory.
            cache: false
            smooth: true
            mipmap: true
        }

        // Full resolution, loaded once the user zooms in. Covers the fitted image when ready.
        Image {
            id: detail

            anchors.fill: parent
            visible: status === Image.Ready
            source: pane.zoomState.zoomed && base.status === Image.Ready ? pane.source : ""
            asynchronous: true
            cache: false
            smooth: true
            mipmap: true
        }
    }

    // Distance of a pane position from the centre, in fitted image sizes.
    function offsetX(x) {
        return (x - width / 2) / fitWidth;
    }
    function offsetY(y) {
        return (y - height / 2) / fitHeight;
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            pane.zoomState.zoomAt(Math.pow(1.0015, event.angleDelta.y), pane.offsetX(point.position.x), pane.offsetY(point.position.y));
        }
    }

    DragHandler {
        id: panHandler

        property point last

        target: null
        enabled: pane.zoomState.zoomed
        onActiveChanged: last = Qt.point(0, 0)
        onTranslationChanged: {
            pane.zoomState.panBy((translation.x - last.x) / pane.fitWidth, (translation.y - last.y) / pane.fitHeight);
            last = translation;
        }
    }

    TapHandler {
        onDoubleTapped: (eventPoint, button) => {
            if (pane.zoomState.zoomed) {
                pane.zoomState.reset();
            } else {
                pane.zoomState.zoomAt(2, pane.offsetX(eventPoint.position.x), pane.offsetY(eventPoint.position.y));
            }
        }
    }

    HoverHandler {
        cursorShape: panHandler.active ? Qt.ClosedHandCursor : pane.zoomState.zoomed ? Qt.OpenHandCursor : Qt.ArrowCursor
    }

    QQC2.BusyIndicator {
        anchors.centerIn: parent
        running: base.status === Image.Loading || detail.status === Image.Loading
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

    QQC2.Label {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: Kirigami.Units.smallSpacing
        }
        visible: pane.zoomState.zoomed
        text: i18nc("@info:status zoom factor", "×%1", pane.zoomState.zoom.toFixed(1))
        color: "white"
        style: Text.Outline
        styleColor: "black"
    }
}
