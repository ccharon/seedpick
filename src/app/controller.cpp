// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "controller.h"

#include "applylog.h"
#include "filemover.h"
#include "imagescanner.h"
#include "imageurl.h"
#include "sessionstore.h"
#include "sidecars.h"

#include <QDir>
#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>
#include <QRandomGenerator>
#include <QThread>
#include <QTimeZone>

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
Controller *s_instance = nullptr;

QDateTime now()
{
    return QDateTime::currentDateTimeUtc();
}

Seedpick::Role toCore(Controller::Role role)
{
    switch (role) {
    case Controller::Role::Keep:
        return Seedpick::Role::Keep;
    case Controller::Role::Rest:
        return Seedpick::Role::Rest;
    case Controller::Role::Reject:
        return Seedpick::Role::Reject;
    }
    Q_UNREACHABLE_RETURN(Seedpick::Role::Keep);
}
}

Controller::Controller(const QStringList &extensions, QObject *parent)
    : QObject(parent)
    , m_extensions(extensions)
{
}

Controller::~Controller()
{
    if (m_applyThread) {
        m_cancelRequested = true;
        m_applyThread->wait();
    }
}

void Controller::setInstance(Controller *controller)
{
    s_instance = controller;
}

Controller *Controller::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_ASSERT(s_instance);
    Q_ASSERT(s_instance->thread() == jsEngine->thread());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

Controller::State Controller::state() const
{
    if (m_directory.isEmpty()) {
        return State::Welcome;
    }
    if (m_applyReport) {
        return State::Applied;
    }
    if (!m_session) {
        return SessionStore(m_directory).exists() ? State::ResumePrompt : State::Setup;
    }
    switch (m_session->phase()) {
    case Phase::Screening:
        return State::Screening;
    case Phase::Duels:
        return State::Duels;
    case Phase::Summary:
        return State::Summary;
    }
    Q_UNREACHABLE_RETURN(State::Welcome);
}

QString Controller::directory() const
{
    return m_directory;
}

QString Controller::errorMessage() const
{
    return m_errorMessage;
}

int Controller::imageCount() const
{
    return m_session ? int(m_session->images().size()) : 0;
}

int Controller::topN() const
{
    return m_session ? m_session->config().topN : 0;
}

bool Controller::canUndo() const
{
    if (!m_session || applying()) {
        return false;
    }
    Session copy = *m_session;
    return copy.undo();
}

QList<QUrl> Controller::preloadUrls() const
{
    return m_preload;
}

const Settings &Controller::settings() const
{
    return m_settings;
}

void Controller::setSettings(const Settings &settings)
{
    if (settings == m_settings) {
        return;
    }
    m_settings = settings;
    refresh();
}

int Controller::defaultTopN() const
{
    return m_settings.topN;
}

bool Controller::defaultSkipScreening() const
{
    return m_settings.skipScreening;
}

QStringList Controller::overriddenSettings() const
{
    return m_overriddenSettings;
}

void Controller::setOverriddenSettings(const QStringList &keys)
{
    m_overriddenSettings = keys;
    Q_EMIT changed();
}

bool Controller::isValidFolderName(const QString &name) const
{
    return ::isValidFolderName(name);
}

int Controller::storedEventCount() const
{
    return m_storedEventCount;
}

QStringList Controller::removedFiles() const
{
    return m_report.removed;
}

QStringList Controller::addedFiles() const
{
    return m_report.added;
}

QStringList Controller::changedFiles() const
{
    return m_report.changed;
}

QUrl Controller::currentUrl() const
{
    return urlFor(m_current);
}

QString Controller::currentFile() const
{
    return m_current >= 0 ? m_session->images().at(m_current).file : QString();
}

int Controller::screenedCount() const
{
    return m_session ? int(m_session->images().size() - m_session->unscreenedCount()) : 0;
}

bool Controller::viewingHistory() const
{
    return m_historyOffset > 0;
}

bool Controller::currentRejected() const
{
    return m_current >= 0 && m_session->screeningState(m_current) == ScreeningState::Reject;
}

QUrl Controller::leftUrl() const
{
    return m_plan.next ? urlFor(m_plan.next->left) : QUrl();
}

QUrl Controller::rightUrl() const
{
    return m_plan.next ? urlFor(m_plan.next->right) : QUrl();
}

QString Controller::leftFile() const
{
    return m_plan.next ? m_session->images().at(m_plan.next->left).file : QString();
}

QString Controller::rightFile() const
{
    return m_plan.next ? m_session->images().at(m_plan.next->right).file : QString();
}

int Controller::comparisons() const
{
    return m_plan.comparisons;
}

int Controller::budget() const
{
    return m_plan.budget;
}

int Controller::estimatedRemaining() const
{
    return m_plan.estimatedRemaining;
}

bool Controller::done() const
{
    return m_plan.done;
}

bool Controller::budgetExceeded() const
{
    return m_plan.budgetExceeded();
}

int Controller::activeCount() const
{
    return m_session ? int(m_session->activeCount()) : 0;
}

QList<QUrl> Controller::topUrls() const
{
    QList<QUrl> urls;
    if (!m_session) {
        return urls;
    }
    for (qsizetype r = 0; r < m_plan.ranking.size() && r < m_session->config().topN; ++r) {
        urls.append(urlFor(m_plan.ranking.at(r).image));
    }
    return urls;
}

QVariantList Controller::keepItems() const
{
    return itemsFor(Seedpick::Role::Keep);
}

QVariantList Controller::restItems() const
{
    return itemsFor(Seedpick::Role::Rest);
}

QVariantList Controller::rejectItems() const
{
    return itemsFor(Seedpick::Role::Reject);
}

QString Controller::keepFolder() const
{
    return m_settings.folders.keep;
}

QString Controller::restFolder() const
{
    return m_settings.folders.rest;
}

QString Controller::rejectFolder() const
{
    return m_settings.folders.reject;
}

bool Controller::rejectToTrash() const
{
    return m_settings.rejectToTrash;
}

bool Controller::applying() const
{
    return m_applyThread != nullptr;
}

int Controller::applyDone() const
{
    return int(m_applyDone);
}

int Controller::applyTotal() const
{
    return int(m_applyTotal);
}

int Controller::movedCount() const
{
    return m_applyReport ? int(m_applyReport->moved) : 0;
}

bool Controller::applyCancelled() const
{
    return m_applyReport && m_applyReport->cancelled;
}

QStringList Controller::applyErrors() const
{
    QStringList lines;
    if (!m_applyReport) {
        return lines;
    }
    if (!m_applyReport->logError.isEmpty()) {
        lines.append(m_applyReport->logError);
    }
    for (const ApplyError &error : m_applyReport->errors) {
        lines.append(u"%1: %2"_s.arg(error.file, error.message));
    }
    return lines;
}

QString Controller::archivedSession() const
{
    return m_archivedSession;
}

QUrl Controller::directoryUrl() const
{
    return m_directory.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_directory);
}

void Controller::openDirectory(const QUrl &url)
{
    openPath(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void Controller::openPath(const QString &path)
{
    if (applying()) {
        return;
    }
    const QFileInfo info(path);
    if (!info.isDir()) {
        fail(u"Not a directory: %1"_s.arg(path));
        Q_EMIT changed();
        return;
    }
    m_session.reset();
    m_report = {};
    m_applyReport.reset();
    m_archivedSession.clear();
    m_errorMessage.clear();
    m_directory = info.canonicalFilePath();
    m_storedEventCount = 0;

    const SessionStore store(m_directory);
    if (store.exists()) {
        const LoadResult loaded = store.load();
        if (loaded.session) {
            m_storedEventCount = int(loaded.session->events().size());
        } else {
            fail(loaded.error);
        }
    }
    refresh();
}

void Controller::startSession(int topN, bool skipScreening)
{
    if (m_directory.isEmpty()) {
        return;
    }
    m_errorMessage.clear();
    const SessionStore store(m_directory);
    if (store.exists()) {
        // Keep the previous session instead of overwriting it.
        const QString backup = QDir(m_directory).filePath(u".seedpick-session.%1.json"_s.arg(now().toString(u"yyyyMMdd-HHmmss"_s)));
        if (!QFile::rename(store.path(), backup)) {
            fail(u"Cannot move the previous session file to %1"_s.arg(backup));
            Q_EMIT changed();
            return;
        }
    }

    const ScanResult scan = scanImages(m_directory, m_extensions);
    if (!scan.error.isEmpty()) {
        fail(scan.error);
        Q_EMIT changed();
        return;
    }
    SessionConfig config;
    config.topN = std::max(1, topN);
    config.skipScreening = skipScreening;
    config.minDuels = m_settings.minDuels;
    config.rngSeed = QRandomGenerator::global()->generate();
    m_session.emplace(config, scan.images, QList<Event>{}, now());
    m_report = {};

    QString error;
    if (!store.save(*m_session, now(), &error)) {
        fail(error);
    }
    refresh();
}

void Controller::resumeSession()
{
    const SessionStore store(m_directory);
    LoadResult loaded = store.load();
    if (!loaded.session) {
        fail(loaded.error);
        Q_EMIT changed();
        return;
    }
    const ScanResult scan = scanImages(m_directory, m_extensions);
    if (!scan.error.isEmpty()) {
        fail(scan.error);
        Q_EMIT changed();
        return;
    }
    m_errorMessage.clear();
    m_session = std::move(loaded.session);
    m_report = m_session->reconcile(scan.images);

    QString error;
    if (!store.save(*m_session, now(), &error)) {
        fail(error);
    }
    refresh();
}

void Controller::closeSession()
{
    if (applying()) {
        return;
    }
    m_session.reset();
    m_report = {};
    m_applyReport.reset();
    m_archivedSession.clear();
    m_errorMessage.clear();
    m_directory.clear();
    refresh();
}

void Controller::dismissReport()
{
    m_report = {};
    Q_EMIT changed();
}

void Controller::screenOk()
{
    if (viewingHistory()) {
        showNext();
        return;
    }
    if (m_current >= 0) {
        append(ScreenEvent{currentFile(), ScreenVerdict::Ok});
    }
}

void Controller::screenReject()
{
    if (!viewingHistory() && m_current >= 0) {
        append(ScreenEvent{currentFile(), ScreenVerdict::Reject});
    }
}

void Controller::showPrevious()
{
    if (m_historyOffset < screeningHistory().size()) {
        ++m_historyOffset;
        refresh();
    }
}

void Controller::showNext()
{
    if (m_historyOffset > 0) {
        --m_historyOffset;
        refresh();
    }
}

void Controller::chooseLeft()
{
    choose(DuelResult::Left);
}

void Controller::chooseRight()
{
    choose(DuelResult::Right);
}

void Controller::chooseTie()
{
    choose(DuelResult::Tie);
}

void Controller::rejectLeft()
{
    if (m_plan.next) {
        append(RejectEvent{leftFile()});
    }
}

void Controller::rejectRight()
{
    if (m_plan.next) {
        append(RejectEvent{rightFile()});
    }
}

void Controller::finish()
{
    append(FinishEvent{});
}

void Controller::undo()
{
    if (applying() || !m_session || !m_session->undo()) {
        return;
    }
    QString error;
    if (!SessionStore(m_directory).save(*m_session, now(), &error)) {
        fail(error);
    }
    m_historyOffset = 0;
    refresh();
}

void Controller::setRole(const QString &file, Role role)
{
    const qsizetype index = m_session ? m_session->indexOf(file) : -1;
    if (index < 0 || index >= m_roles.size() || m_roles.at(index) == toCore(role)) {
        return;
    }
    append(OverrideEvent{file, toCore(role)});
}

void Controller::apply()
{
    if (applying() || state() != State::Summary) {
        return;
    }
    QStringList files;
    for (const ImageInfo &image : m_session->images()) {
        files.append(image.file);
    }
    const ApplyPlan plan = buildApplyPlan(m_directory, files, m_roles, m_settings.folders, m_settings.sidecarExtensions, m_settings.rejectToTrash);
    m_applyDone = 0;
    m_applyTotal = plan.fileCount();
    m_cancelRequested = false;

    const QString directory = m_directory;
    m_applyThread = QThread::create([this, plan, directory]() {
        LocalFileMover mover;
        ApplyLog log(directory);
        const ApplyReport report = executeApplyPlan(
            directory,
            plan,
            mover,
            log,
            [this](qsizetype done, qsizetype) {
                QMetaObject::invokeMethod(
                    this,
                    [this, done]() {
                        m_applyDone = done;
                        Q_EMIT changed();
                    },
                    Qt::QueuedConnection);
            },
            [this]() {
                return m_cancelRequested.load();
            });
        QMetaObject::invokeMethod(
            this,
            [this, report]() {
                finishApply(report);
            },
            Qt::QueuedConnection);
    });
    connect(m_applyThread, &QThread::finished, m_applyThread, &QObject::deleteLater);
    m_applyThread->start();
    Q_EMIT changed();
}

void Controller::cancelApply()
{
    m_cancelRequested = true;
}

void Controller::finishApply(const ApplyReport &report)
{
    // The worker posts this as its last step, so waiting is short.
    if (m_applyThread) {
        m_applyThread->wait();
    }
    m_applyThread = nullptr;
    if (report.moved == 0 && !report.logError.isEmpty()) {
        // Nothing moved, the session stays so the user can try again.
        fail(report.logError);
        refresh();
        return;
    }
    const SessionStore store(m_directory);
    if (report.cancelled) {
        // The session stays. Resuming it drops the moved images from the ranking.
    } else if (m_settings.keepSessionAfterApply) {
        QString error;
        const QString archived = store.archive(&error);
        if (archived.isEmpty()) {
            fail(error);
        }
        m_archivedSession = QFileInfo(archived).fileName();
    } else if (!QFile::remove(store.path())) {
        fail(u"Cannot delete %1"_s.arg(store.path()));
    }
    m_applyReport = report;
    m_session.reset();
    refresh();
}

SchedulerOptions Controller::schedulerOptions() const
{
    SchedulerOptions options;
    options.confidenceThreshold = m_settings.confidenceThreshold;
    return options;
}

QVariantList Controller::itemsFor(Seedpick::Role role) const
{
    QVariantList items;
    if (!m_session) {
        return items;
    }
    for (qsizetype index : m_summaryOrder) {
        if (m_roles.at(index) != role) {
            continue;
        }
        items.append(QVariantMap{
            {u"file"_s, m_session->images().at(index).file},
            {u"url"_s, urlFor(index)},
            {u"overridden"_s, m_session->overrideTarget(index).has_value()},
        });
    }
    return items;
}

void Controller::choose(DuelResult result)
{
    if (m_plan.next) {
        append(DuelEvent{leftFile(), rightFile(), result});
    }
}

bool Controller::append(const EventData &data)
{
    if (!m_session || applying()) {
        return false;
    }
    QString error;
    if (!m_session->append(Event{data, now()}, &error)) {
        fail(error);
        Q_EMIT changed();
        return false;
    }
    if (!SessionStore(m_directory).save(*m_session, now(), &error)) {
        fail(error);
    } else {
        m_errorMessage.clear();
    }
    m_historyOffset = 0;
    refresh();
    return true;
}

void Controller::fail(const QString &message)
{
    m_errorMessage = message;
}

QStringList Controller::screeningHistory() const
{
    QStringList files;
    if (!m_session) {
        return files;
    }
    for (const Event &event : m_session->events()) {
        if (const auto *screen = std::get_if<ScreenEvent>(&event.data); screen && m_session->indexOf(screen->file) >= 0) {
            files.append(screen->file);
        }
    }
    return files;
}

QUrl Controller::urlFor(qsizetype index) const
{
    if (!m_session || index < 0) {
        return {};
    }
    return ImageUrl::fromPath(QDir(m_directory).filePath(m_session->images().at(index).file));
}

QList<QUrl> Controller::pairUrls(const Session &session) const
{
    const Plan plan = planNext(session, schedulerOptions());
    if (!plan.next) {
        return {};
    }
    return {urlFor(plan.next->left), urlFor(plan.next->right)};
}

void Controller::refresh()
{
    m_plan = {};
    m_current = -1;
    m_preload.clear();
    m_roles.clear();
    m_summaryOrder.clear();

    if (m_session) {
        const State current = state();
        if (current != State::Screening) {
            m_historyOffset = 0;
        }
        if (current == State::Screening) {
            const QStringList history = screeningHistory();
            m_historyOffset = std::min<int>(m_historyOffset, int(history.size()));
            if (m_historyOffset > 0) {
                m_current = m_session->indexOf(history.at(history.size() - m_historyOffset));
            } else {
                m_current = m_session->nextUnscreened().value_or(-1);
            }
            // The next two unscreened images after the shown one.
            int queued = 0;
            const qsizetype start = m_historyOffset > 0 ? 0 : m_current + 1;
            for (qsizetype i = start; i < m_session->images().size() && queued < 2; ++i) {
                if (i != m_current && m_session->screeningState(i) == ScreeningState::Unscreened) {
                    m_preload.append(urlFor(i));
                    ++queued;
                }
            }
        } else {
            m_plan = planNext(*m_session, schedulerOptions());
        }

        if (current == State::Summary) {
            m_roles = assignRoles(*m_session, m_plan.ranking);
            QList<bool> ranked(m_session->images().size(), false);
            for (const RankedImage &image : m_plan.ranking) {
                m_summaryOrder.append(image.image);
                ranked[image.image] = true;
            }
            for (qsizetype i = 0; i < m_session->images().size(); ++i) {
                if (!ranked.at(i)) {
                    m_summaryOrder.append(i);
                }
            }
        }

        // Pairs that follow each possible answer, so the next pair is decoded before the key press.
        if (current == State::Duels && m_plan.next) {
            for (DuelResult result : {DuelResult::Left, DuelResult::Right, DuelResult::Tie}) {
                Session branch = *m_session;
                if (branch.append(Event{DuelEvent{leftFile(), rightFile(), result}, {}})) {
                    m_preload.append(pairUrls(branch));
                }
            }
        } else if (current == State::Screening && m_session->unscreenedCount() == 1 && m_historyOffset == 0 && m_current >= 0) {
            Session branch = *m_session;
            if (branch.append(Event{ScreenEvent{currentFile(), ScreenVerdict::Ok}, {}})) {
                m_preload.append(pairUrls(branch));
            }
        }
    }
    Q_EMIT changed();
}
