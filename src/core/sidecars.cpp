// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sidecars.h"

#include <QSet>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
QStringList defaultSidecarExtensions()
{
    return {u"txt"_s, u"json"_s, u"yaml"_s, u"yml"_s, u"xmp"_s, u"caption"_s};
}

QString baseName(const QString &file)
{
    const qsizetype dot = file.lastIndexOf(u'.');
    return dot > 0 ? file.left(dot) : file;
}

QHash<QString, QStringList> findSidecars(const QStringList &images, const QStringList &entries, const QStringList &extensions)
{
    QSet<QString> wanted;
    for (const QString &extension : extensions) {
        wanted.insert(extension.toLower());
    }
    const QSet<QString> imageSet(images.cbegin(), images.cend());
    QHash<QString, QStringList> byBase;
    for (const QString &image : images) {
        byBase[baseName(image)].append(image);
    }

    QHash<QString, QStringList> result;
    for (const QString &entry : entries) {
        const qsizetype dot = entry.lastIndexOf(u'.');
        if (dot <= 0 || imageSet.contains(entry) || !wanted.contains(entry.mid(dot + 1).toLower())) {
            continue;
        }
        const QString stem = entry.left(dot);
        if (imageSet.contains(stem)) {
            result[stem].append(entry);
            continue;
        }
        const QStringList owners = byBase.value(stem);
        if (owners.size() == 1) {
            result[owners.constFirst()].append(entry);
        }
    }
    for (QStringList &sidecars : result) {
        std::sort(sidecars.begin(), sidecars.end());
    }
    return result;
}
}
