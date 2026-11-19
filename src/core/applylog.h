// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "applyplan.h"

#include <QDateTime>
#include <QFile>
#include <QString>

namespace Seedpick
{
// Record of applied moves (source → target) in the session directory, one JSON object per line.
// Every apply appends a "start" line, one line per file and a "finish" line. Lines are
// appended and flushed one by one, so earlier content is never rewritten and a crash loses
// at most the line being written.
class ApplyLog
{
public:
    static QString fileName();

    explicit ApplyLog(const QString &directory);

    QString path() const;

    bool begin(const QDateTime &now, QString *error = nullptr);
    bool recordMove(const FileMove &move, const QDateTime &at, QString *error = nullptr);
    bool recordError(const FileMove &move, const QString &message, const QDateTime &at, QString *error = nullptr);
    // Closes the file. `cancelled` marks an apply that stopped before all files were handled.
    bool finish(const QDateTime &now, bool cancelled = false, QString *error = nullptr);

private:
    bool append(const QJsonObject &line, QString *error);

    QFile m_file;
};
}
