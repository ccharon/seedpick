// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <variant>

namespace Seedpick
{
enum class ScreenVerdict {
    Ok,
    Reject,
};

enum class DuelResult {
    Left,
    Right,
    Tie,
};

// Where a file ends up when the result is applied.
enum class Role {
    Keep,
    Rest,
    Reject,
};

struct ScreenEvent {
    QString file;
    ScreenVerdict verdict = ScreenVerdict::Ok;
    bool operator==(const ScreenEvent &) const = default;
};

struct DuelEvent {
    QString left;
    QString right;
    DuelResult result = DuelResult::Tie;
    bool operator==(const DuelEvent &) const = default;
};

// Marks an image as defective after screening, e.g. during a duel.
struct RejectEvent {
    QString file;
    bool operator==(const RejectEvent &) const = default;
};

// Manual correction in the summary phase.
struct OverrideEvent {
    QString file;
    Role target = Role::Keep;
    bool operator==(const OverrideEvent &) const = default;
};

// Ends the duel phase early.
struct FinishEvent {
    bool operator==(const FinishEvent &) const = default;
};

// Written when Apply starts. Holds the confirmed keep set, so a cancelled apply resumes with the same result
// even though the ranking changes once moved images drop out.
struct ApplyEvent {
    QStringList keep;
    bool operator==(const ApplyEvent &) const = default;
};

using EventData = std::variant<ScreenEvent, DuelEvent, RejectEvent, OverrideEvent, FinishEvent, ApplyEvent>;

struct Event {
    EventData data;
    // Invalid when the entry carries no timestamp.
    QDateTime at;

    bool operator==(const Event &) const = default;
};

QJsonObject eventToJson(const Event &event);

// Unknown keys are ignored. Unknown event types and malformed fields are errors.
std::optional<Event> eventFromJson(const QJsonObject &json, QString *error = nullptr);
}
