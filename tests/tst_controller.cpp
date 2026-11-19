// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>

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
        QCOMPARE(controller.keepItems().size(), 2);
        QCOMPARE(controller.restItems().size(), 3);
        QCOMPARE(controller.rejectItems().size(), 0);
        controller.undo();
        QCOMPARE(controller.state(), Controller::State::Duels);
    }

    void setRoleOverridesAndUndoes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 5);
        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.startSession(2, true);
        controller.finish();

        const QVariantMap first = controller.restItems().constFirst().toMap();
        const QString file = first.value(u"file"_s).toString();
        QVERIFY(!first.value(u"overridden"_s).toBool());
        controller.setRole(file, Controller::Role::Reject);
        QCOMPARE(controller.restItems().size(), 2);
        QCOMPARE(controller.rejectItems().size(), 1);
        const QVariantMap moved = controller.rejectItems().constFirst().toMap();
        QCOMPARE(moved.value(u"file"_s).toString(), file);
        QVERIFY(moved.value(u"overridden"_s).toBool());
        QCOMPARE(storedEvents(dir.path()).last().toObject().value("t"_L1).toString(), u"override"_s);

        // Setting the current role adds no event.
        const int events = int(storedEvents(dir.path()).size());
        controller.setRole(file, Controller::Role::Reject);
        QCOMPARE(storedEvents(dir.path()).size(), events);

        controller.undo();
        QCOMPARE(controller.rejectItems().size(), 0);
        QCOMPARE(controller.state(), Controller::State::Summary);
    }

    void applyMovesFilesAndArchivesSession()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 5);
        Controller controller(extensions);
        controller.openPath(dir.path());
        controller.startSession(2, true);
        controller.finish();

        const QString rejected = controller.restItems().constFirst().toMap().value(u"file"_s).toString();
        const QString caption = rejected.chopped(4) + u".txt"_s;
        QFile captionFile(QDir(dir.path()).filePath(caption));
        QVERIFY(captionFile.open(QIODevice::WriteOnly));
        captionFile.close();
        controller.setRole(rejected, Controller::Role::Reject);
        const QStringList keep{controller.keepItems().at(0).toMap().value(u"file"_s).toString(),
                               controller.keepItems().at(1).toMap().value(u"file"_s).toString()};

        controller.apply();
        QVERIFY(controller.applying());
        QVERIFY(!controller.canUndo());
        QTRY_COMPARE(controller.state(), Controller::State::Applied);
        QVERIFY(!controller.applying());
        QCOMPARE(controller.movedCount(), 4);
        QCOMPARE(controller.applyErrors(), QStringList{});
        QCOMPARE(controller.applyDone(), 4);

        const QDir result(dir.path());
        QVERIFY(QFile::exists(result.filePath(u"_reject/"_s + rejected)));
        QVERIFY(QFile::exists(result.filePath(u"_reject/"_s + caption)));
        QCOMPARE(QDir(result.filePath(u"_rest"_s)).entryList(QDir::Files).size(), 2);
        // Kept images stay in the folder by default.
        QStringList expectedKeep = keep;
        expectedKeep.sort();
        QCOMPARE(result.entryList(QDir::Files), expectedKeep);
        QVERIFY(!QFileInfo::exists(result.filePath(u"_keep"_s)));
        QVERIFY(!Seedpick::SessionStore(dir.path()).exists());
        QCOMPARE(controller.archivedSession(), u".seedpick-session.done.json"_s);
        QVERIFY(QFile::exists(result.filePath(controller.archivedSession())));
        QVERIFY(QFile::exists(result.filePath(u".seedpick-apply.log.jsonl"_s)));

        controller.closeSession();
        QCOMPARE(controller.state(), Controller::State::Welcome);
    }

    void settingsReachSessionAndApply()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 4);
        Controller controller(extensions);
        Settings settings;
        settings.topN = 3;
        settings.skipScreening = true;
        settings.minDuels = 4;
        settings.folders = {u"_auswahl"_s, u"_rest2"_s, u"_ausschuss"_s};
        settings.keepSessionAfterApply = false;
        controller.setSettings(settings);
        QCOMPARE(controller.defaultTopN(), 3);
        QVERIFY(controller.defaultSkipScreening());
        QCOMPARE(controller.keepFolder(), u"_auswahl"_s);

        controller.openPath(dir.path());
        controller.startSession(controller.defaultTopN(), controller.defaultSkipScreening());
        QFile file(Seedpick::SessionStore(dir.path()).path());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value("config"_L1).toObject().value("minDuels"_L1).toInt(), 4);
        file.close();

        controller.finish();
        controller.apply();
        QTRY_COMPARE(controller.state(), Controller::State::Applied);
        const QDir result(dir.path());
        QCOMPARE(QDir(result.filePath(u"_auswahl"_s)).entryList(QDir::Files).size(), 3);
        QCOMPARE(QDir(result.filePath(u"_rest2"_s)).entryList(QDir::Files).size(), 1);
        QVERIFY(controller.archivedSession().isEmpty());
        QCOMPARE(result.entryList(QDir::Files | QDir::Hidden), QStringList{u".seedpick-apply.log.jsonl"_s});
    }

    void imageInfoReadsPngText()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createImages(dir.path(), 1);
        auto uint32 = [](quint32 value) {
            QByteArray bytes(4, '\0');
            qToBigEndian(value, bytes.data());
            return bytes;
        };
        const QByteArray header = uint32(64) + uint32(32) + QByteArray("\x08\x06\x00\x00\x00", 5);
        const QByteArray text = "parameters\0a rabbit\nSteps: 20, Seed: 7"_ba;
        QFile png(QDir(dir.path()).filePath(u"info.png"_s));
        QVERIFY(png.open(QIODevice::WriteOnly));
        png.write(QByteArray("\x89PNG\r\n\x1a\n", 8) + uint32(13) + "IHDR" + header + uint32(0) + uint32(quint32(text.size())) + "tEXt" + text + uint32(0)
                  + uint32(0) + "IEND" + uint32(0));
        png.close();

        Controller controller(extensions);
        controller.openPath(dir.path());
        const QVariantMap info = controller.imageInfo(u"info.png"_s);
        QCOMPARE(info.value(u"generator"_s).toString(), u"A1111"_s);
        QCOMPARE(info.value(u"prompt"_s).toString(), u"a rabbit"_s);
        QCOMPARE(info.value(u"width"_s).toInt(), 64);
        const QVariantList settings = info.value(u"settings"_s).toList();
        QCOMPARE(settings.size(), 2);
        QCOMPARE(settings.at(1).toMap().value(u"label"_s).toString(), u"Seed"_s);
        QCOMPARE(settings.at(1).toMap().value(u"value"_s).toString(), u"7"_s);
        QVERIFY(controller.imageInfo(u"img00.png"_s).isEmpty());
        QVERIFY(controller.imageInfo(u"missing.png"_s).isEmpty());
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
