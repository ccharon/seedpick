// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "applyplan.h"
#include "pairscheduler.h"
#include "session.h"
#include "settings.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <optional>

class QQmlEngine;
class QJSEngine;
class QThread;

// Binds the core session to QML. All state is derived from the session after every change.
class Controller : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY changed)
    Q_PROPERTY(QString directory READ directory NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(int imageCount READ imageCount NOTIFY changed)
    Q_PROPERTY(int topN READ topN NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(QList<QUrl> preloadUrls READ preloadUrls NOTIFY changed)

    // Settings of the run.
    Q_PROPERTY(int defaultTopN READ defaultTopN NOTIFY changed)
    Q_PROPERTY(bool defaultSkipScreening READ defaultSkipScreening NOTIFY changed)
    // Command line options that override the settings for this run.
    Q_PROPERTY(QStringList overriddenSettings READ overriddenSettings NOTIFY changed)

    // Resume prompt and reconcile report.
    Q_PROPERTY(int storedEventCount READ storedEventCount NOTIFY changed)
    // The session file in the folder could be read, so it can be resumed.
    Q_PROPERTY(bool canResume READ canResume NOTIFY changed)
    Q_PROPERTY(QStringList removedFiles READ removedFiles NOTIFY changed)
    Q_PROPERTY(QStringList addedFiles READ addedFiles NOTIFY changed)
    Q_PROPERTY(QStringList changedFiles READ changedFiles NOTIFY changed)

    // Screening.
    Q_PROPERTY(QUrl currentUrl READ currentUrl NOTIFY changed)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY changed)
    Q_PROPERTY(int screenedCount READ screenedCount NOTIFY changed)
    Q_PROPERTY(bool viewingHistory READ viewingHistory NOTIFY changed)
    Q_PROPERTY(bool currentRejected READ currentRejected NOTIFY changed)

    // Duels.
    Q_PROPERTY(QUrl leftUrl READ leftUrl NOTIFY changed)
    Q_PROPERTY(QUrl rightUrl READ rightUrl NOTIFY changed)
    Q_PROPERTY(QString leftFile READ leftFile NOTIFY changed)
    Q_PROPERTY(QString rightFile READ rightFile NOTIFY changed)
    Q_PROPERTY(int comparisons READ comparisons NOTIFY changed)
    Q_PROPERTY(int budget READ budget NOTIFY changed)
    Q_PROPERTY(int estimatedRemaining READ estimatedRemaining NOTIFY changed)
    Q_PROPERTY(bool done READ done NOTIFY changed)
    Q_PROPERTY(bool budgetExceeded READ budgetExceeded NOTIFY changed)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY changed)
    Q_PROPERTY(QList<QUrl> topUrls READ topUrls NOTIFY changed)

    // Summary. Each item is a map with file, url and overridden.
    Q_PROPERTY(QVariantList keepItems READ keepItems NOTIFY changed)
    Q_PROPERTY(QVariantList restItems READ restItems NOTIFY changed)
    Q_PROPERTY(QVariantList rejectItems READ rejectItems NOTIFY changed)
    Q_PROPERTY(QString keepFolder READ keepFolder NOTIFY changed)
    Q_PROPERTY(QString restFolder READ restFolder NOTIFY changed)
    Q_PROPERTY(QString rejectFolder READ rejectFolder NOTIFY changed)
    Q_PROPERTY(bool rejectToTrash READ rejectToTrash NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(int applyDone READ applyDone NOTIFY changed)
    Q_PROPERTY(int applyTotal READ applyTotal NOTIFY changed)

    // Report after applying.
    Q_PROPERTY(int movedCount READ movedCount NOTIFY changed)
    // Applying stopped on request. The session file stays.
    Q_PROPERTY(bool applyCancelled READ applyCancelled NOTIFY changed)
    Q_PROPERTY(QStringList applyErrors READ applyErrors NOTIFY changed)
    // File name the session file was renamed to, empty if it was deleted.
    Q_PROPERTY(QString archivedSession READ archivedSession NOTIFY changed)
    Q_PROPERTY(QUrl directoryUrl READ directoryUrl NOTIFY changed)

public:
    enum class State {
        Welcome,
        Setup,
        ResumePrompt,
        Screening,
        Duels,
        Summary,
        Applied,
    };
    Q_ENUM(State)

    // Mirrors Seedpick::Role for QML.
    enum class Role {
        Keep,
        Rest,
        Reject,
    };
    Q_ENUM(Role)

    explicit Controller(const QStringList &extensions, QObject *parent = nullptr);
    ~Controller() override;

    // The application creates the instance before loading QML.
    static void setInstance(Controller *controller);
    static Controller *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    State state() const;
    QString directory() const;
    QString errorMessage() const;
    int imageCount() const;
    int topN() const;
    bool canUndo() const;
    QList<QUrl> preloadUrls() const;

    const Settings &settings() const;
    void setSettings(const Settings &settings);
    int defaultTopN() const;
    bool defaultSkipScreening() const;
    QStringList overriddenSettings() const;
    void setOverriddenSettings(const QStringList &keys);
    Q_INVOKABLE bool isValidFolderName(const QString &name) const;
    // Generation info of an image in the current directory: generator, prompt, negativePrompt,
    // settings (list of {label, value}), width and height. Empty for files without PNG text.
    Q_INVOKABLE QVariantMap imageInfo(const QString &file) const;

    int storedEventCount() const;
    bool canResume() const;
    QStringList removedFiles() const;
    QStringList addedFiles() const;
    QStringList changedFiles() const;

    QUrl currentUrl() const;
    QString currentFile() const;
    int screenedCount() const;
    bool viewingHistory() const;
    bool currentRejected() const;

    QUrl leftUrl() const;
    QUrl rightUrl() const;
    QString leftFile() const;
    QString rightFile() const;
    int comparisons() const;
    int budget() const;
    int estimatedRemaining() const;
    bool done() const;
    bool budgetExceeded() const;
    int activeCount() const;
    QList<QUrl> topUrls() const;

    QVariantList keepItems() const;
    QVariantList restItems() const;
    QVariantList rejectItems() const;
    QString keepFolder() const;
    QString restFolder() const;
    QString rejectFolder() const;
    bool rejectToTrash() const;
    bool applying() const;
    int applyDone() const;
    int applyTotal() const;

    int movedCount() const;
    bool applyCancelled() const;
    QStringList applyErrors() const;
    QString archivedSession() const;
    QUrl directoryUrl() const;

    Q_INVOKABLE void openDirectory(const QUrl &url);
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void startSession(int topN, bool skipScreening);
    Q_INVOKABLE void resumeSession();
    Q_INVOKABLE void closeSession();
    Q_INVOKABLE void dismissReport();

    Q_INVOKABLE void screenOk();
    Q_INVOKABLE void screenReject();
    Q_INVOKABLE void showPrevious();
    Q_INVOKABLE void showNext();

    Q_INVOKABLE void chooseLeft();
    Q_INVOKABLE void chooseRight();
    Q_INVOKABLE void chooseTie();
    Q_INVOKABLE void rejectLeft();
    Q_INVOKABLE void rejectRight();
    Q_INVOKABLE void finish();

    Q_INVOKABLE void setRole(const QString &file, Role role);
    // Moves the files in a worker thread. The state turns to Applied when done.
    Q_INVOKABLE void apply();
    // Stops applying after the current image and its sidecars.
    Q_INVOKABLE void cancelApply();

    Q_INVOKABLE void undo();

Q_SIGNALS:
    void changed();

private:
    void refresh();
    bool append(const Seedpick::EventData &data);
    void choose(Seedpick::DuelResult result);
    // Shows a translated sentence, followed by the untranslated technical detail from the core.
    void fail(const QString &message, const QString &detail = {});
    QStringList screeningHistory() const;
    QUrl urlFor(qsizetype index) const;
    QList<QUrl> pairUrls(const Seedpick::Session &session) const;
    Seedpick::SchedulerOptions schedulerOptions() const;
    QVariantList itemsFor(Seedpick::Role role) const;
    void finishApply(const Seedpick::ApplyReport &report);

    QStringList m_extensions;
    QString m_directory;
    QString m_errorMessage;
    std::optional<Seedpick::Session> m_session;
    Seedpick::Plan m_plan;
    Seedpick::ReconcileReport m_report;
    int m_storedEventCount = 0;
    bool m_canResume = false;
    // 0 shows the next unscreened image, k > 0 the k-th last screened one.
    int m_historyOffset = 0;
    qsizetype m_current = -1;
    QList<QUrl> m_preload;

    Settings m_settings;
    QStringList m_overriddenSettings;
    mutable QHash<QString, QVariantMap> m_imageInfo;
    // Indices into Session::images() in display order: ranking first, then the rest.
    QList<qsizetype> m_summaryOrder;
    QList<Seedpick::Role> m_roles;
    QPointer<QThread> m_applyThread;
    std::atomic_bool m_cancelRequested = false;
    qsizetype m_applyDone = 0;
    qsizetype m_applyTotal = 0;
    std::optional<Seedpick::ApplyReport> m_applyReport;
    QString m_archivedSession;
};
