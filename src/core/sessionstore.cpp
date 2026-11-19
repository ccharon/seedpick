// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sessionstore.h"

#include "filemover.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QTimeZone>

#include <cmath>
#include <limits>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
namespace
{
QString formatTime(const QDateTime &time)
{
    return time.toUTC().toString(Qt::ISODateWithMs);
}

std::optional<QDateTime> parseTime(const QJsonValue &value)
{
    if (!value.isString()) {
        return std::nullopt;
    }
    const QDateTime time = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    if (!time.isValid()) {
        return std::nullopt;
    }
    return time.toUTC();
}

// JSON numbers are doubles. Accept only integral values inside [min, max].
std::optional<qint64> parseInteger(const QJsonValue &value, qint64 min, qint64 max)
{
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const double number = value.toDouble();
    if (number != std::floor(number) || number < double(min) || number > double(max)) {
        return std::nullopt;
    }
    return qint64(number);
}

// Upgrades older schema versions in place. Version 1 is the first one, so there is nothing to convert yet.
QString migrate(QJsonObject &root)
{
    const std::optional<qint64> schema = parseInteger(root.value("schema"_L1), 1, std::numeric_limits<int>::max());
    if (!schema) {
        return u"missing or invalid schema version"_s;
    }
    if (*schema > SessionStore::schemaVersion) {
        return u"session file uses schema %1, this version supports up to %2"_s.arg(*schema).arg(SessionStore::schemaVersion);
    }
    root.insert("schema"_L1, SessionStore::schemaVersion);
    return {};
}

QString parseConfig(const QJsonValue &value, SessionConfig &config)
{
    if (value.isUndefined()) {
        return {};
    }
    if (!value.isObject()) {
        return u"\"config\" is not an object"_s;
    }
    const QJsonObject json = value.toObject();

    auto readInt = [&json](QLatin1StringView key, qint64 min, qint64 max, auto &target) -> QString {
        if (!json.contains(key)) {
            return {};
        }
        const std::optional<qint64> number = parseInteger(json.value(key), min, max);
        if (!number) {
            return u"invalid config value \"%1\""_s.arg(key);
        }
        target = static_cast<std::decay_t<decltype(target)>>(*number);
        return {};
    };

    QString error = readInt("n"_L1, 1, std::numeric_limits<int>::max(), config.topN);
    if (error.isEmpty()) {
        error = readInt("minDuels"_L1, 0, std::numeric_limits<int>::max(), config.minDuels);
    }
    if (error.isEmpty()) {
        error = readInt("rngSeed"_L1, 0, std::numeric_limits<quint32>::max(), config.rngSeed);
    }
    if (error.isEmpty() && json.contains("skipScreening"_L1)) {
        const QJsonValue skip = json.value("skipScreening"_L1);
        if (!skip.isBool()) {
            return u"invalid config value \"skipScreening\""_s;
        }
        config.skipScreening = skip.toBool();
    }
    return error;
}

QString parseImages(const QJsonValue &value, QList<ImageInfo> &images)
{
    if (!value.isArray()) {
        return u"missing or invalid \"images\" array"_s;
    }
    QSet<QString> seen;
    const QJsonArray array = value.toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject json = array.at(i).toObject();
        ImageInfo image;
        image.file = json.value("file"_L1).toString();
        const std::optional<qint64> size = parseInteger(json.value("size"_L1), 0, qint64(1) << 53);
        const std::optional<QDateTime> mtime = parseTime(json.value("mtime"_L1));
        if (!array.at(i).isObject() || image.file.isEmpty() || !size || !mtime) {
            return u"invalid image entry %1"_s.arg(i);
        }
        if (seen.contains(image.file)) {
            return u"duplicate image entry \"%1\""_s.arg(image.file);
        }
        seen.insert(image.file);
        image.size = *size;
        image.mtime = *mtime;
        images.append(image);
    }
    return {};
}

QString parseEvents(const QJsonValue &value, QList<Event> &events)
{
    if (!value.isArray()) {
        return u"missing or invalid \"events\" array"_s;
    }
    const QJsonArray array = value.toArray();
    events.reserve(array.size());
    for (qsizetype i = 0; i < array.size(); ++i) {
        if (!array.at(i).isObject()) {
            return u"event %1 is not an object"_s.arg(i);
        }
        QString error;
        const std::optional<Event> event = eventFromJson(array.at(i).toObject(), &error);
        if (!event) {
            return u"event %1: %2"_s.arg(i).arg(error);
        }
        events.append(*event);
    }
    return {};
}
}

QString SessionStore::fileName()
{
    return u".seedpick-session.json"_s;
}

SessionStore::SessionStore(const QString &directory)
    : m_path(QDir(directory).filePath(fileName()))
{
}

QString SessionStore::path() const
{
    return m_path;
}

bool SessionStore::exists() const
{
    return QFile::exists(m_path);
}

QByteArray SessionStore::serialize(const Session &session, const QDateTime &updatedAt)
{
    const SessionConfig &config = session.config();
    QJsonObject configJson{
        {"n"_L1, config.topN},
        {"minDuels"_L1, config.minDuels},
        {"rngSeed"_L1, qint64(config.rngSeed)},
        {"skipScreening"_L1, config.skipScreening},
    };

    QJsonArray images;
    for (const ImageInfo &image : session.images()) {
        images.append(QJsonObject{
            {"file"_L1, image.file},
            {"size"_L1, image.size},
            {"mtime"_L1, formatTime(image.mtime)},
        });
    }

    QJsonArray events;
    for (const Event &event : session.events()) {
        events.append(eventToJson(event));
    }

    const QDateTime createdAt = session.createdAt().isValid() ? session.createdAt() : updatedAt;
    QJsonObject root{
        {"schema"_L1, schemaVersion},
        {"createdAt"_L1, formatTime(createdAt)},
        {"updatedAt"_L1, formatTime(updatedAt)},
        {"config"_L1, configJson},
        {"images"_L1, images},
        {"events"_L1, events},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

LoadResult SessionStore::deserialize(const QByteArray &data)
{
    LoadResult result;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        result.error = u"invalid JSON at offset %1: %2"_s.arg(parseError.offset).arg(parseError.errorString());
        return result;
    }
    if (!document.isObject()) {
        result.error = u"session file is not a JSON object"_s;
        return result;
    }

    QJsonObject root = document.object();
    if (result.error = migrate(root); !result.error.isEmpty()) {
        return result;
    }

    SessionConfig config;
    QList<ImageInfo> images;
    QList<Event> events;
    if (result.error = parseConfig(root.value("config"_L1), config); !result.error.isEmpty()) {
        return result;
    }
    if (result.error = parseImages(root.value("images"_L1), images); !result.error.isEmpty()) {
        return result;
    }
    if (result.error = parseEvents(root.value("events"_L1), events); !result.error.isEmpty()) {
        return result;
    }

    const QDateTime createdAt = parseTime(root.value("createdAt"_L1)).value_or(QDateTime());
    result.updatedAt = parseTime(root.value("updatedAt"_L1)).value_or(QDateTime());
    result.session.emplace(config, images, events, createdAt);
    return result;
}

LoadResult SessionStore::load() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        LoadResult result;
        result.error = u"cannot read %1: %2"_s.arg(m_path, file.errorString());
        return result;
    }
    return deserialize(file.readAll());
}

bool SessionStore::save(const Session &session, const QDateTime &now, QString *error) const
{
    QSaveFile file(m_path);
    const QByteArray data = serialize(session, now);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error) {
            *error = u"cannot write %1: %2"_s.arg(m_path, file.errorString());
        }
        return false;
    }
    return true;
}

QString SessionStore::archive(QString *error) const
{
    const QDir dir = QFileInfo(m_path).dir();
    LocalFileMover mover;
    for (int suffix = 0;; ++suffix) {
        const QString name = suffix == 0 ? u".seedpick-session.done.json"_s : u".seedpick-session.done-%1.json"_s.arg(suffix);
        const QString target = dir.filePath(name);
        if (QFileInfo::exists(target)) {
            continue;
        }
        if (!mover.move(m_path, target, error)) {
            return {};
        }
        return target;
    }
}
}
