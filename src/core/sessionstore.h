// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "session.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

namespace Seedpick
{
struct LoadResult {
    std::optional<Session> session;
    QDateTime updatedAt;
    // Empty on success.
    QString error;
};

// Reads and writes the session file of one image directory.
class SessionStore
{
public:
    static constexpr int schemaVersion = 1;
    static QString fileName();

    explicit SessionStore(const QString &directory);

    QString path() const;
    bool exists() const;

    LoadResult load() const;
    // Atomic replace. The previous file stays intact if writing fails.
    bool save(const Session &session, const QDateTime &now, QString *error = nullptr) const;

    static QByteArray serialize(const Session &session, const QDateTime &updatedAt);
    static LoadResult deserialize(const QByteArray &data);

private:
    QString m_path;
};
}
