// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "controller.h"
#include "imageurl.h"
#include "sessionstore.h"

using namespace Qt::Literals::StringLiterals;

namespace
{
const QStringList extensions{u"png"_s};

void createImages(const QString &directory, int count)
{
    for (int i = 0; i < count; ++i) {
        QFile file(QDir(directory).filePath(u"img%1.png"_s.arg(i, 2, 10, u'0')));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
}

QJsonArray storedEvents(const QString &directory)
{
    QFile file(Seedpick::SessionStore(directory).path());
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().value("events"_L1).toArray();
}
}

class TestController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsInWelcome()
    {
        const Controller controller(extensions);
        QCOMPARE(controller.state(), Controller::State::Welcome);
    }

    void screeningThenDuels()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 6);
        Controller controller(extensions);
        controller.openPath(dir.path());
        QCOMPARE(controller.state(), Controller::State::Setup);

        controller.startSession(2, false);
        QCOMPARE(controller.state(), Controller::State::Screening);
        QCOMPARE(controller.currentFile(), u"img00.png"_s);
        QCOMPARE(controller.currentUrl(), ImageUrl::fromPath(QDir(dir.path()).filePath(u"img00.png"_s)));
        QCOMPARE(controller.preloadUrls().size(), 2);

        controller.screenOk();
        controller.screenReject();
        QCOMPARE(controller.screenedCount(), 2);
        QCOMPARE(storedEvents(dir.path()).size(), 2);

        controller.showPrevious();
        QVERIFY(controller.viewingHistory());
        QCOMPARE(controller.currentFile(), u"img01.png"_s);
        QVERIFY(controller.currentRejected());
        controller.screenReject();
        QCOMPARE(controller.screenedCount(), 2);
        controller.screenOk();
        QVERIFY(!controller.viewingHistory());
        QCOMPARE(controller.currentFile(), u"img02.png"_s);

        for (int i = 0; i < 4; ++i) {
            controller.screenOk();
        }
        QCOMPARE(controller.state(), Controller::State::Duels);
        QVERIFY(!controller.leftFile().isEmpty());
        QVERIFY(controller.leftFile() != controller.rightFile());
        QVERIFY(controller.leftFile() != u"img01.png"_s && controller.rightFile() != u"img01.png"_s);
        QVERIFY(!controller.preloadUrls().isEmpty());

        controller.chooseLeft();
        QCOMPARE(controller.comparisons(), 1);
        const QJsonObject last = storedEvents(dir.path()).last().toObject();
        QCOMPARE(last.value("t"_L1).toString(), u"duel"_s);
        QVERIFY(last.contains("at"_L1));
    }

    void undoReturnsToScreening()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 4);
        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.startSession(1, false);
        for (int i = 0; i < 4; ++i) {
            controller.screenOk();
        }
        QCOMPARE(controller.state(), Controller::State::Duels);
        QVERIFY(controller.canUndo());
        controller.undo();
        QCOMPARE(controller.state(), Controller::State::Screening);
        QCOMPARE(controller.currentFile(), u"img03.png"_s);
        QCOMPARE(storedEvents(dir.path()).size(), 3);
    }

    void resumeRestoresState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 5);
        QString left;
        {
            Controller controller(extensions);
            controller.openPath(dir.path());
            controller.startSession(2, true);
            QCOMPARE(controller.state(), Controller::State::Duels);
            controller.chooseRight();
            left = controller.leftFile();
        }
        Controller controller(extensions);
        controller.openPath(dir.path());
        QCOMPARE(controller.state(), Controller::State::ResumePrompt);
        QCOMPARE(controller.storedEventCount(), 1);
        controller.resumeSession();
        QCOMPARE(controller.state(), Controller::State::Duels);
        QCOMPARE(controller.comparisons(), 1);
        // Same log, same seed: the scheduler offers the same pair again.
        QCOMPARE(controller.leftFile(), left);
    }

    void resumeReportsFolderChanges()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 5);
        {
            Controller controller(extensions);
            controller.openPath(dir.path());
            controller.startSession(2, true);
        }
        QVERIFY(QFile::remove(QDir(dir.path()).filePath(u"img04.png"_s)));
        QFile extra(QDir(dir.path()).filePath(u"new.png"_s));
        QVERIFY(extra.open(QIODevice::WriteOnly));
        extra.close();

        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.resumeSession();
        QCOMPARE(controller.removedFiles(), QStringList{u"img04.png"_s});
        QCOMPARE(controller.addedFiles(), QStringList{u"new.png"_s});
        controller.dismissReport();
        QVERIFY(controller.removedFiles().isEmpty());
    }

    void newSessionKeepsPreviousFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 3);
        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.startSession(1, false);
        controller.screenOk();
        controller.closeSession();
        QCOMPARE(controller.state(), Controller::State::Welcome);

        controller.openPath(dir.path());
        QCOMPARE(controller.state(), Controller::State::ResumePrompt);
        controller.startSession(1, false);
        QCOMPARE(controller.state(), Controller::State::Screening);
        QCOMPARE(storedEvents(dir.path()).size(), 0);
        const QStringList backups = QDir(dir.path()).entryList({u".seedpick-session.*.json"_s}, QDir::Files | QDir::Hidden);
        QCOMPARE(backups.size(), 1);
        QVERIFY(controller.errorMessage().isEmpty());
    }

    void finishLeadsToSummary()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 5);
        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.startSession(2, true);
        controller.finish();
        QCOMPARE(controller.state(), Controller::State::Summary);
        QCOMPARE(controller.rankingFiles().size(), 5);
        controller.undo();
        QCOMPARE(controller.state(), Controller::State::Duels);
    }

    void invalidDirectoryIsReported()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Controller controller(extensions);
        controller.openPath(dir.filePath(u"missing"_s));
        QCOMPARE(controller.state(), Controller::State::Welcome);
        QVERIFY(!controller.errorMessage().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestController)
#include "tst_controller.moc"
