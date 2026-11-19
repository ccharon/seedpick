// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "pairscheduler.h"
#include "session.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <optional>

class QQmlEngine;
class QJSEngine;

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

    // Resume prompt and reconcile report.
    Q_PROPERTY(int storedEventCount READ storedEventCount NOTIFY changed)
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

    // Summary placeholder until the summary page exists.
    Q_PROPERTY(QStringList rankingFiles READ rankingFiles NOTIFY changed)

public:
    enum class State {
        Welcome,
        Setup,
        ResumePrompt,
        Screening,
        Duels,
        Summary,
    };
    Q_ENUM(State)

    explicit Controller(const QStringList &extensions, QObject *parent = nullptr);

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

    int storedEventCount() const;
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

    QStringList rankingFiles() const;

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

    Q_INVOKABLE void undo();

Q_SIGNALS:
    void changed();

private:
    void refresh();
    bool append(const Seedpick::EventData &data);
    void choose(Seedpick::DuelResult result);
    void fail(const QString &message);
    QStringList screeningHistory() const;
    QUrl urlFor(qsizetype index) const;
    QList<QUrl> pairUrls(const Seedpick::Session &session) const;

    QStringList m_extensions;
    QString m_directory;
    QString m_errorMessage;
    std::optional<Seedpick::Session> m_session;
    Seedpick::Plan m_plan;
    Seedpick::ReconcileReport m_report;
    int m_storedEventCount = 0;
    // 0 shows the next unscreened image, k > 0 the k-th last screened one.
    int m_historyOffset = 0;
    qsizetype m_current = -1;
    QList<QUrl> m_preload;
};
