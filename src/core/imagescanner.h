// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace Seedpick
{
struct ImageInfo {
    // File name relative to the session directory.
    QString file;
    qint64 size = 0;
    // UTC, millisecond precision.
    QDateTime mtime;

    bool operator==(const ImageInfo &) const = default;
};

struct ScanResult {
    QList<ImageInfo> images;
    // Empty on success.
    QString error;
};

// Lists the image files directly inside a directory.
// Subdirectories (including the result folders) and hidden files are skipped.
// The core cannot query QImageReader, so the caller passes the supported extensions.
ScanResult scanImages(const QString &directory, const QStringList &extensions);

// Natural, case-insensitive order ("img2" before "img10"), stable across locales.
void sortByFileName(QList<ImageInfo> &images);
}
