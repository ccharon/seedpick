// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "event.h"

#include <QTimeZone>

#include <array>
#include <utility>

namespace Seedpick
{
namespace
{
template<typename Enum, std::size_t N>
using EnumNames = std::array<std::pair<Enum, QLatin1StringView>, N>;

using namespace Qt::Literals::StringLiterals;

constexpr EnumNames<ScreenVerdict, 2> screenVerdictNames{{{ScreenVerdict::Ok, "ok"_L1}, {ScreenVerdict::Reject, "reject"_L1}}};
constexpr EnumNames<DuelResult, 3> duelResultNames{{{DuelResult::Left, "left"_L1}, {DuelResult::Right, "right"_L1}, {DuelResult::Tie, "tie"_L1}}};
constexpr EnumNames<Role, 3> roleNames{{{Role::Keep, "keep"_L1}, {Role::Rest, "rest"_L1}, {Role::Reject, "reject"_L1}}};

template<typename Enum, std::size_t N>
QString toName(const EnumNames<Enum, N> &names, Enum value)
{
    for (const auto &[candidate, name] : names) {
        if (candidate == value) {
            return QString(name);
        }
    }
    Q_UNREACHABLE_RETURN(QString());
}

template<typename Enum, std::size_t N>
std::optional<Enum> fromName(const EnumNames<Enum, N> &names, const QString &text)
{
    for (const auto &[value, name] : names) {
        if (text == name) {
            return value;
        }
    }
    return std::nullopt;
}

// Collects the first error while reading one JSON object.
class Reader
{
public:
    explicit Reader(const QJsonObject &json)
        : m_json(json)
    {
    }

    QString string(QLatin1StringView key)
    {
        const QJsonValue value = m_json.value(key);
        if (!value.isString() || value.toString().isEmpty()) {
            fail(u"missing or empty string field \"%1\""_s.arg(key));
            return {};
        }
        return value.toString();
    }

    template<typename Enum, std::size_t N>
    Enum enumValue(QLatin1StringView key, const EnumNames<Enum, N> &names)
    {
        const QString text = string(key);
        if (!ok()) {
            return {};
        }
        const std::optional<Enum> value = fromName(names, text);
        if (!value) {
            fail(u"invalid value \"%1\" for field \"%2\""_s.arg(text, key));
            return {};
        }
        return *value;
    }

    bool ok() const
    {
        return m_error.isEmpty();
    }

    void fail(const QString &error)
    {
        if (m_error.isEmpty()) {
            m_error = error;
        }
    }

    QString error() const
    {
        return m_error;
    }

private:
    QJsonObject m_json;
    QString m_error;
};
}

QJsonObject eventToJson(const Event &event)
{
    QJsonObject json;
    std::visit(
        [&json](const auto &data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, ScreenEvent>) {
                json.insert("t"_L1, "screen"_L1);
                json.insert("file"_L1, data.file);
                json.insert("verdict"_L1, toName(screenVerdictNames, data.verdict));
            } else if constexpr (std::is_same_v<T, DuelEvent>) {
                json.insert("t"_L1, "duel"_L1);
                json.insert("left"_L1, data.left);
                json.insert("right"_L1, data.right);
                json.insert("result"_L1, toName(duelResultNames, data.result));
            } else if constexpr (std::is_same_v<T, RejectEvent>) {
                json.insert("t"_L1, "reject"_L1);
                json.insert("file"_L1, data.file);
            } else if constexpr (std::is_same_v<T, OverrideEvent>) {
                json.insert("t"_L1, "override"_L1);
                json.insert("file"_L1, data.file);
                json.insert("target"_L1, toName(roleNames, data.target));
            } else if constexpr (std::is_same_v<T, FinishEvent>) {
                json.insert("t"_L1, "finish"_L1);
            }
        },
        event.data);
    if (event.at.isValid()) {
        json.insert("at"_L1, event.at.toUTC().toString(Qt::ISODateWithMs));
    }
    return json;
}

std::optional<Event> eventFromJson(const QJsonObject &json, QString *error)
{
    Reader reader(json);
    Event event;
    const QString type = reader.string("t"_L1);

    if (!reader.ok()) {
        // Reported below.
    } else if (type == "screen"_L1) {
        ScreenEvent data;
        data.file = reader.string("file"_L1);
        data.verdict = reader.enumValue("verdict"_L1, screenVerdictNames);
        event.data = data;
    } else if (type == "duel"_L1) {
        DuelEvent data;
        data.left = reader.string("left"_L1);
        data.right = reader.string("right"_L1);
        data.result = reader.enumValue("result"_L1, duelResultNames);
        event.data = data;
    } else if (type == "reject"_L1) {
        event.data = RejectEvent{reader.string("file"_L1)};
    } else if (type == "override"_L1) {
        OverrideEvent data;
        data.file = reader.string("file"_L1);
        data.target = reader.enumValue("target"_L1, roleNames);
        event.data = data;
    } else if (type == "finish"_L1) {
        event.data = FinishEvent{};
    } else {
        reader.fail(u"unknown event type \"%1\""_s.arg(type));
    }

    if (reader.ok() && json.contains("at"_L1)) {
        const QJsonValue value = json.value("at"_L1);
        event.at = QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
        if (!value.isString() || !event.at.isValid()) {
            reader.fail(u"invalid timestamp in field \"at\""_s);
        } else {
            event.at = event.at.toUTC();
        }
    }

    if (!reader.ok()) {
        if (error) {
            *error = reader.error();
        }
        return std::nullopt;
    }
    return event;
}
}
