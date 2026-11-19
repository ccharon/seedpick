// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import io.github.ccharon.seedpick

// Decodes upcoming images into the image cache at the size they will be shown.
QtObject {
    id: preloader

    property list<url> urls
    property size decodeSize

    function prefetch() {
        ImageCache.prefetch(preloader.urls, preloader.decodeSize);
    }

    onUrlsChanged: prefetch()
    onDecodeSizeChanged: prefetch()
}
