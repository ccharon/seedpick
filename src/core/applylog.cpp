// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "applylog.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

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
    return u".seedpick-apply.log.jsonl"_s;
}

ApplyLog::ApplyLog(const QString &directory)
    : m_file(QDir(directory).filePath(fileName()))
{
}

QString ApplyLog::path() const
{
    return m_file.fileName();
}

bool ApplyLog::begin(const QDateTime &now, QString *error)
{
    if (!m_file.isOpen() && !m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return setError(error, u"cannot open %1: %2"_s.arg(path(), m_file.errorString()));
    }
    return append({{"t"_L1, "start"_L1}, {"at"_L1, formatTime(now)}}, error);
}

bool ApplyLog::recordMove(const FileMove &move, const QDateTime &at, QString *error)
{
    return append({{"t"_L1, "move"_L1}, {"source"_L1, move.source}, {"target"_L1, move.target}, {"at"_L1, formatTime(at)}}, error);
}

bool ApplyLog::recordError(const FileMove &move, const QString &message, const QDateTime &at, QString *error)
{
    return append({{"t"_L1, "error"_L1}, {"source"_L1, move.source}, {"target"_L1, move.target}, {"error"_L1, message}, {"at"_L1, formatTime(at)}}, error);
}

bool ApplyLog::finish(const QDateTime &now, bool cancelled, QString *error)
{
    QJsonObject line{{"t"_L1, "finish"_L1}, {"at"_L1, formatTime(now)}};
    if (cancelled) {
        line.insert("cancelled"_L1, true);
    }
    const bool written = append(line, error);
    m_file.close();
    return written;
}

bool ApplyLog::append(const QJsonObject &line, QString *error)
{
    if (!m_file.isOpen()) {
        return setError(error, u"%1 is not open"_s.arg(path()));
    }
    const QByteArray data = QJsonDocument(line).toJson(QJsonDocument::Compact) + '\n';
    if (m_file.write(data) != data.size() || !m_file.flush()) {
        return setError(error, u"cannot write %1: %2"_s.arg(path(), m_file.errorString()));
    }
    return true;
}
}
