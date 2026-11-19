// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

// URLs for the cached image provider. Base64url keeps any file name intact through QML's URL handling.
namespace ImageUrl
{
inline constexpr QLatin1StringView providerId{"seedpick"};

inline QUrl fromPath(const QString &path)
{
    const QByteArray id = path.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QUrl(QStringLiteral("image://") + providerId + u'/' + QString::fromLatin1(id));
}

inline QString pathFromId(const QString &id)
{
    return QString::fromUtf8(QByteArray::fromBase64(id.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// Empty when the URL does not belong to the provider.
inline QString pathFromUrl(const QUrl &url)
{
    if (url.scheme() != QLatin1StringView("image") || url.host() != providerId) {
        return {};
    }
    return pathFromId(url.path().mid(1));
}
}
