// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick

// Zoom and position shared by image panes. Coordinates are relative to the fitted image,
// so panes with different image sizes show the same region.
QtObject {
    id: state

    // 1 shows the whole image.
    property real zoom: 1
    // Image point at the pane centre, 0..1 on both axes.
    property real centerX: 0.5
    property real centerY: 0.5

    readonly property real maxZoom: 16
    readonly property bool zoomed: zoom > 1.001

    function clampCenter() {
        const half = 0.5 / zoom;
        centerX = Math.min(Math.max(centerX, half), 1 - half);
        centerY = Math.min(Math.max(centerY, half), 1 - half);
    }

    // offsetX/offsetY: distance of the fixed point from the pane centre, in fitted image sizes.
    function zoomAt(factor, offsetX, offsetY) {
        const next = Math.min(Math.max(zoom * factor, 1), maxZoom);
        const pointX = centerX + offsetX / zoom;
        const pointY = centerY + offsetY / zoom;
        zoom = next;
        centerX = pointX - offsetX / next;
        centerY = pointY - offsetY / next;
        clampCenter();
    }

    // dx/dy in fitted image sizes at zoom 1.
    function panBy(dx, dy) {
        centerX -= dx / zoom;
        centerY -= dy / zoom;
        clampCenter();
    }

    function reset() {
        zoom = 1;
        centerX = 0.5;
        centerY = 0.5;
    }
}
