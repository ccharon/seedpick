// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "applylog.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
namespace
{
QString formatTime(const QDateTime &time)
{
    return time.toUTC().toString(Qt::ISODateWithMs);
}

bool setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}
}

QString ApplyLog::fileName()
{
    return u".seedpick-apply.log.json"_s;
}

ApplyLog::ApplyLog(const QString &directory)
    : m_path(QDir(directory).filePath(fileName()))
{
}

QString ApplyLog::path() const
{
    return m_path;
}

bool ApplyLog::begin(const QDateTime &now, QString *error)
{
    m_applies = {};
    QFile file(m_path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return setError(error, u"cannot read %1: %2"_s.arg(m_path, file.errorString()));
        }
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        const QJsonObject root = document.object();
        if (parseError.error != QJsonParseError::NoError || !document.isObject() || root.value("schema"_L1).toInt() != schemaVersion
            || !root.value("applies"_L1).isArray()) {
            return setError(error, u"%1 is not a valid apply log, it stays untouched"_s.arg(m_path));
        }
        m_applies = root.value("applies"_L1).toArray();
    }
    m_current = QJsonObject{{"startedAt"_L1, formatTime(now)}};
    m_moves = {};
    m_errors = {};
    return write(error);
}

bool ApplyLog::recordMove(const FileMove &move, const QDateTime &at, QString *error)
{
    m_moves.append(QJsonObject{{"source"_L1, move.source}, {"target"_L1, move.target}, {"at"_L1, formatTime(at)}});
    return write(error);
}

bool ApplyLog::recordError(const FileMove &move, const QString &message, const QDateTime &at, QString *error)
{
    m_errors.append(QJsonObject{{"source"_L1, move.source}, {"target"_L1, move.target}, {"error"_L1, message}, {"at"_L1, formatTime(at)}});
    return write(error);
}

bool ApplyLog::finish(const QDateTime &now, QString *error)
{
    m_current.insert("finishedAt"_L1, formatTime(now));
    return write(error);
}

bool ApplyLog::write(QString *error)
{
    QJsonObject current = m_current;
    current.insert("moves"_L1, m_moves);
    current.insert("errors"_L1, m_errors);
    QJsonArray applies = m_applies;
    applies.append(current);
    const QJsonObject root{{"schema"_L1, schemaVersion}, {"applies"_L1, applies}};

    QSaveFile file(m_path);
    const QByteArray data = QJsonDocument(root).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        return setError(error, u"cannot write %1: %2"_s.arg(m_path, file.errorString()));
    }
    return true;
}
}
