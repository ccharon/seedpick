// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imagescanner.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QTimeZone>

#include <algorithm>

namespace Seedpick
{
ScanResult scanImages(const QString &directory, const QStringList &extensions)
{
    ScanResult result;
    const QDir dir(directory);
    if (!dir.exists()) {
        result.error = QStringLiteral("Directory does not exist: %1").arg(directory);
        return result;
    }
    if (!QFileInfo(directory).isReadable()) {
        result.error = QStringLiteral("Directory is not readable: %1").arg(directory);
        return result;
    }

    QSet<QString> wanted;
    for (const QString &extension : extensions) {
        wanted.insert(extension.toLower());
    }

    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (!wanted.contains(entry.suffix().toLower())) {
            continue;
        }
        QDateTime mtime = entry.lastModified(QTimeZone::UTC);
        // Drop sub-millisecond precision so the value survives an ISO 8601 round trip.
        mtime = QDateTime::fromMSecsSinceEpoch(mtime.toMSecsSinceEpoch(), QTimeZone::UTC);
        result.images.append(ImageInfo{entry.fileName(), entry.size(), mtime});
    }

    sortByFileName(result.images);
    return result;
}

namespace
{
// Compares digit runs by value and everything else case-insensitively.
int naturalCompare(QStringView a, QStringView b)
{
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            qsizetype endA = i;
            qsizetype endB = j;
            while (endA < a.size() && a[endA].isDigit()) {
                ++endA;
            }
            while (endB < b.size() && b[endB].isDigit()) {
                ++endB;
            }
            QStringView numberA = a.sliced(i, endA - i);
            QStringView numberB = b.sliced(j, endB - j);
            while (numberA.size() > 1 && numberA.front() == u'0') {
                numberA = numberA.sliced(1);
            }
            while (numberB.size() > 1 && numberB.front() == u'0') {
                numberB = numberB.sliced(1);
            }
            if (numberA.size() != numberB.size()) {
                return numberA.size() < numberB.size() ? -1 : 1;
            }
            if (const int order = numberA.compare(numberB); order != 0) {
                return order;
            }
            i = endA;
            j = endB;
            continue;
        }
        const char32_t charA = QChar::toCaseFolded(a[i].unicode());
        const char32_t charB = QChar::toCaseFolded(b[j].unicode());
        if (charA != charB) {
            return charA < charB ? -1 : 1;
        }
        ++i;
        ++j;
    }
    if (i < a.size()) {
        return 1;
    }
    if (j < b.size()) {
        return -1;
    }
    return 0;
}
}

void sortByFileName(QList<ImageInfo> &images)
{
    std::sort(images.begin(), images.end(), [](const ImageInfo &a, const ImageInfo &b) {
        const int order = naturalCompare(a.file, b.file);
        return order != 0 ? order < 0 : a.file < b.file;
    });
}
}
