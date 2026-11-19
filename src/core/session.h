// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "event.h"
#include "imagescanner.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QStringList>

#include <optional>

namespace Seedpick
{
struct SessionConfig {
    int topN = 5;
    int minDuels = 2;
    quint32 rngSeed = 0;
    bool skipScreening = false;

    bool operator==(const SessionConfig &) const = default;
};

enum class Phase {
    Screening,
    Duels,
    Summary,
};

enum class ScreeningState {
    Unscreened,
    Ok,
    Reject,
};

// A duel between two active images, as indices into Session::images().
struct Duel {
    qsizetype left = -1;
    qsizetype right = -1;
    DuelResult result = DuelResult::Tie;

    bool operator==(const Duel &) const = default;
};

struct ReconcileReport {
    QStringList removed;
    QStringList added;
    // Size or mtime differ from the stored values.
    QStringList changed;

    bool isEmpty() const
    {
        return removed.isEmpty() && added.isEmpty() && changed.isEmpty();
    }
};

// Event log plus the state derived from it. The log is the only source of truth.
// Events that reference files missing from images() stay in the log without effect.
class Session
{
public:
    Session(const SessionConfig &config, const QList<ImageInfo> &images, const QList<Event> &events = {}, const QDateTime &createdAt = {});

    const SessionConfig &config() const;
    const QList<ImageInfo> &images() const;
    const QList<Event> &events() const;
    QDateTime createdAt() const;

    // Returns an empty string when the event is allowed in the current state.
    QString validate(const Event &event) const;
    // Appends the event if validate() accepts it.
    bool append(const Event &event, QString *error = nullptr);
    // Removes the last effective event and any ineffective events after it.
    // Returns false when no effective event exists.
    bool undo();

    // Replaces the image list with a fresh scan and recomputes the state.
    ReconcileReport reconcile(const QList<ImageInfo> &scanned);

    Phase phase() const;
    qsizetype indexOf(const QString &file) const;
    ScreeningState screeningState(qsizetype index) const;
    // Rejected in screening or later.
    bool isRejected(qsizetype index) const;
    // Takes part in duels.
    bool isActive(qsizetype index) const;
    std::optional<Role> overrideTarget(qsizetype index) const;
    bool isFinished() const;

    qsizetype activeCount() const;
    qsizetype unscreenedCount() const;
    // First unscreened image in images() order.
    std::optional<qsizetype> nextUnscreened() const;

    // Duels between images that are still active.
    QList<Duel> duels() const;
    int duelCount(qsizetype index) const;

private:
    struct ImageState {
        ScreeningState screening = ScreeningState::Unscreened;
        bool rejectedLater = false;
        std::optional<Role> override;
    };

    void rebuild();
    bool apply(const Event &event);
    bool isUnscreenedActive(const ImageState &state) const;

    SessionConfig m_config;
    QList<ImageInfo> m_images;
    QList<Event> m_events;
    QDateTime m_createdAt;

    QHash<QString, qsizetype> m_index;
    QList<ImageState> m_states;
    QList<Duel> m_allDuels;
    QList<bool> m_effective;
    bool m_finished = false;
};
}
