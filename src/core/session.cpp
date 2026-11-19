// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "session.h"

#include <type_traits>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
Session::Session(const SessionConfig &config, const QList<ImageInfo> &images, const QList<Event> &events, const QDateTime &createdAt)
    : m_config(config)
    , m_images(images)
    , m_events(events)
    , m_createdAt(createdAt)
{
    rebuild();
}

const SessionConfig &Session::config() const
{
    return m_config;
}

const QList<ImageInfo> &Session::images() const
{
    return m_images;
}

const QList<Event> &Session::events() const
{
    return m_events;
}

QDateTime Session::createdAt() const
{
    return m_createdAt;
}

void Session::rebuild()
{
    m_index.clear();
    m_index.reserve(m_images.size());
    for (qsizetype i = 0; i < m_images.size(); ++i) {
        m_index.insert(m_images.at(i).file, i);
    }
    m_states = QList<ImageState>(m_images.size());
    m_allDuels.clear();
    m_finished = false;

    m_effective.clear();
    m_effective.reserve(m_events.size());
    for (const Event &event : std::as_const(m_events)) {
        m_effective.append(apply(event));
    }
}

// Replay is lenient: the last screen verdict wins and unknown files are ignored.
// Strict checks for new events happen in validate().
bool Session::apply(const Event &event)
{
    return std::visit(
        [this](const auto &data) -> bool {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, ScreenEvent>) {
                const qsizetype index = indexOf(data.file);
                if (index < 0) {
                    return false;
                }
                m_states[index].screening = data.verdict == ScreenVerdict::Ok ? ScreeningState::Ok : ScreeningState::Reject;
                return true;
            } else if constexpr (std::is_same_v<T, DuelEvent>) {
                const qsizetype left = indexOf(data.left);
                const qsizetype right = indexOf(data.right);
                if (left < 0 || right < 0 || left == right) {
                    return false;
                }
                m_allDuels.append(Duel{left, right, data.result});
                return true;
            } else if constexpr (std::is_same_v<T, RejectEvent>) {
                const qsizetype index = indexOf(data.file);
                if (index < 0) {
                    return false;
                }
                m_states[index].rejectedLater = true;
                return true;
            } else if constexpr (std::is_same_v<T, OverrideEvent>) {
                const qsizetype index = indexOf(data.file);
                if (index < 0) {
                    return false;
                }
                m_states[index].override = data.target;
                return true;
            } else if constexpr (std::is_same_v<T, FinishEvent>) {
                m_finished = true;
                return true;
            }
        },
        event.data);
}

QString Session::validate(const Event &event) const
{
    const Phase current = phase();
    return std::visit(
        [this, current](const auto &data) -> QString {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, ScreenEvent>) {
                const qsizetype index = indexOf(data.file);
                if (index < 0) {
                    return u"unknown file \"%1\""_s.arg(data.file);
                }
                if (current != Phase::Screening) {
                    return u"screening is not active"_s;
                }
                if (m_states.at(index).screening != ScreeningState::Unscreened) {
                    return u"\"%1\" is already screened"_s.arg(data.file);
                }
                return {};
            } else if constexpr (std::is_same_v<T, DuelEvent>) {
                const qsizetype left = indexOf(data.left);
                const qsizetype right = indexOf(data.right);
                if (left < 0 || right < 0) {
                    return u"unknown file in duel"_s;
                }
                if (left == right) {
                    return u"an image cannot duel itself"_s;
                }
                if (current != Phase::Duels) {
                    return u"duels are not active"_s;
                }
                if (!isActive(left) || !isActive(right)) {
                    return u"both images must take part in duels"_s;
                }
                return {};
            } else if constexpr (std::is_same_v<T, RejectEvent>) {
                const qsizetype index = indexOf(data.file);
                if (index < 0) {
                    return u"unknown file \"%1\""_s.arg(data.file);
                }
                if (current != Phase::Duels) {
                    return u"rejecting is only possible during duels"_s;
                }
                if (isRejected(index)) {
                    return u"\"%1\" is already rejected"_s.arg(data.file);
                }
                return {};
            } else if constexpr (std::is_same_v<T, OverrideEvent>) {
                if (indexOf(data.file) < 0) {
                    return u"unknown file \"%1\""_s.arg(data.file);
                }
                if (current != Phase::Summary) {
                    return u"overrides are only possible in the summary"_s;
                }
                return {};
            } else if constexpr (std::is_same_v<T, FinishEvent>) {
                if (current != Phase::Duels) {
                    return u"duels are not active"_s;
                }
                return {};
            }
        },
        event.data);
}

bool Session::append(const Event &event, QString *error)
{
    const QString problem = validate(event);
    if (!problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return false;
    }
    m_events.append(event);
    m_effective.append(apply(event));
    return true;
}

bool Session::undo()
{
    const qsizetype last = m_effective.lastIndexOf(true);
    if (last < 0) {
        return false;
    }
    m_events.resize(last);
    rebuild();
    return true;
}

ReconcileReport Session::reconcile(const QList<ImageInfo> &scanned)
{
    ReconcileReport report;
    QHash<QString, const ImageInfo *> current;
    for (const ImageInfo &image : scanned) {
        current.insert(image.file, &image);
    }
    for (const ImageInfo &stored : std::as_const(m_images)) {
        const ImageInfo *found = current.value(stored.file);
        if (!found) {
            report.removed.append(stored.file);
        } else if (found->size != stored.size || found->mtime != stored.mtime) {
            report.changed.append(stored.file);
        }
    }
    for (const ImageInfo &image : scanned) {
        if (!m_index.contains(image.file)) {
            report.added.append(image.file);
        }
    }

    m_images = scanned;
    rebuild();
    return report;
}

bool Session::isUnscreenedActive(const ImageState &state) const
{
    return state.screening == ScreeningState::Unscreened && m_config.skipScreening;
}

Phase Session::phase() const
{
    if (!m_config.skipScreening && unscreenedCount() > 0) {
        return Phase::Screening;
    }
    if (m_finished || activeCount() <= m_config.topN) {
        return Phase::Summary;
    }
    return Phase::Duels;
}

qsizetype Session::indexOf(const QString &file) const
{
    return m_index.value(file, -1);
}

ScreeningState Session::screeningState(qsizetype index) const
{
    return m_states.at(index).screening;
}

bool Session::isRejected(qsizetype index) const
{
    const ImageState &state = m_states.at(index);
    return state.screening == ScreeningState::Reject || state.rejectedLater;
}

bool Session::isActive(qsizetype index) const
{
    const ImageState &state = m_states.at(index);
    if (isRejected(index)) {
        return false;
    }
    return state.screening == ScreeningState::Ok || isUnscreenedActive(state);
}

std::optional<Role> Session::overrideTarget(qsizetype index) const
{
    return m_states.at(index).override;
}

bool Session::isFinished() const
{
    return m_finished;
}

qsizetype Session::activeCount() const
{
    qsizetype count = 0;
    for (qsizetype i = 0; i < m_states.size(); ++i) {
        count += isActive(i) ? 1 : 0;
    }
    return count;
}

qsizetype Session::unscreenedCount() const
{
    qsizetype count = 0;
    for (const ImageState &state : m_states) {
        count += state.screening == ScreeningState::Unscreened ? 1 : 0;
    }
    return count;
}

std::optional<qsizetype> Session::nextUnscreened() const
{
    for (qsizetype i = 0; i < m_states.size(); ++i) {
        if (m_states.at(i).screening == ScreeningState::Unscreened) {
            return i;
        }
    }
    return std::nullopt;
}

QList<Duel> Session::duels() const
{
    QList<Duel> result;
    result.reserve(m_allDuels.size());
    for (const Duel &duel : m_allDuels) {
        if (isActive(duel.left) && isActive(duel.right)) {
            result.append(duel);
        }
    }
    return result;
}

int Session::duelCount(qsizetype index) const
{
    int count = 0;
    for (const Duel &duel : duels()) {
        count += (duel.left == index || duel.right == index) ? 1 : 0;
    }
    return count;
}
}
