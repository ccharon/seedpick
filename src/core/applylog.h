// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "applyplan.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace Seedpick
{
// Record of applied moves (source → target) in the session directory, the basis for undoing
// an apply. Every apply adds an entry, earlier entries stay. The file is rewritten atomically
// after every change.
class ApplyLog
{
public:
    static constexpr int schemaVersion = 1;
    static QString fileName();

    explicit ApplyLog(const QString &directory);

    QString path() const;

    // Loads earlier entries and starts a new one. Fails without touching the file if it
    // exists but cannot be read.
    bool begin(const QDateTime &now, QString *error = nullptr);
    bool recordMove(const FileMove &move, const QDateTime &at, QString *error = nullptr);
    bool recordError(const FileMove &move, const QString &message, const QDateTime &at, QString *error = nullptr);
    bool finish(const QDateTime &now, QString *error = nullptr);

private:
    bool write(QString *error);

    QString m_path;
    QJsonArray m_applies;
    QJsonObject m_current;
    QJsonArray m_moves;
    QJsonArray m_errors;
};
}
