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
#include <QTimeZone>

#include "applylog.h"
#include "applyplan.h"
#include "filemover.h"
#include "sessionstore.h"
#include "sidecars.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
void writeFile(const QString &path, const QByteArray &data)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Writes each file with its own name as content, so moves can be traced.
void createFiles(const QTemporaryDir &dir, const QStringList &files)
{
    for (const QString &file : files) {
        writeFile(dir.filePath(file), file.toUtf8());
    }
}

QJsonObject readJson(const QString &path)
{
    return QJsonDocument::fromJson(readFile(path)).object();
}

QList<FileMove> allMoves(const ApplyPlan &plan)
{
    QList<FileMove> moves;
    for (const ApplyItem &item : plan.items) {
        moves.append(item.moves);
    }
    return moves;
}

// Fails every move whose source is in `failing`, delegates the rest.
class FailingMover : public FileMover
{
public:
    explicit FailingMover(QStringList failing)
        : m_failing(std::move(failing))
    {
    }

    bool move(const QString &source, const QString &target, QString *error) override
    {
        if (m_failing.contains(QFileInfo(source).fileName())) {
            *error = u"simulated failure"_s;
            return false;
        }
        return m_local.move(source, target, error);
    }

    bool moveToTrash(const QString &source, QString *trashedPath, QString *error) override
    {
        return m_local.moveToTrash(source, trashedPath, error);
    }

private:
    QStringList m_failing;
    LocalFileMover m_local;
};

const QStringList noSidecars;

class EnvironmentOverride
{
public:
    EnvironmentOverride(const char *name, const QString &value)
        : m_name(name)
        , m_previous(qgetenv(name))
        , m_wasSet(qEnvironmentVariableIsSet(name))
    {
        qputenv(name, value.toLocal8Bit());
    }
    ~EnvironmentOverride()
    {
        if (m_wasSet) {
            qputenv(m_name, m_previous);
        } else {
            qunsetenv(m_name);
        }
    }
    Q_DISABLE_COPY_MOVE(EnvironmentOverride)

private:
    const char *m_name;
    QByteArray m_previous;
    bool m_wasSet;
};
}

class TestApplyPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void assignRoles()
    {
        QList<ImageInfo> images;
        for (int i = 0; i < 5; ++i) {
            images.append({u"img%1.png"_s.arg(i), 1, QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC)});
        }
        SessionConfig config;
        config.topN = 2;
        config.skipScreening = true;
        Session session(config, images);
        QVERIFY(session.append({RejectEvent{u"img4.png"_s}, {}}));
        QVERIFY(session.append({FinishEvent{}, {}}));
        QVERIFY(session.append({OverrideEvent{u"img3.png"_s, Role::Keep}, {}}));
        QVERIFY(session.append({OverrideEvent{u"img0.png"_s, Role::Reject}, {}}));

        QList<RankedImage> ranking;
        for (qsizetype image : {1, 0, 2, 3}) {
            ranking.append(RankedImage{image, 0, 0, 0, 0});
        }
        const QList<Role> roles = Seedpick::assignRoles(session, ranking);
        QCOMPARE(roles, (QList<Role>{Role::Reject, Role::Keep, Role::Rest, Role::Keep, Role::Reject}));
    }

    void plansMovesWithSidecars()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"a.png.xmp"_s, u"b.png"_s, u"c.png"_s, u"notes.txt"_s});

        const ApplyPlan plan = buildApplyPlan(dir.path(),
                                              {u"a.png"_s, u"b.png"_s, u"c.png"_s},
                                              {Role::Keep, Role::Rest, Role::Reject},
                                              ResultFolders{},
                                              defaultSidecarExtensions());
        QCOMPARE(plan.items.size(), 3);
        QCOMPARE(plan.items.at(0).moves,
                 (QList<FileMove>{{u"a.png"_s, u"_keep/a.png"_s}, {u"a.png.xmp"_s, u"_keep/a.png.xmp"_s}, {u"a.txt"_s, u"_keep/a.txt"_s}}));
        QCOMPARE(plan.items.at(1).moves, (QList<FileMove>{{u"b.png"_s, u"_rest/b.png"_s}}));
        QCOMPARE(plan.items.at(2).moves, (QList<FileMove>{{u"c.png"_s, u"_reject/c.png"_s}}));
        QCOMPARE(plan.fileCount(), 5);
        // Planning does not touch the directory.
        QCOMPARE(QDir(dir.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot), QStringList{});
    }

    void customFolderNames()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s});
        const ResultFolders folders{u"_auswahl"_s, u"_rest"_s, u"_ausschuss"_s};
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Reject}, folders, noSidecars);
        QCOMPARE(plan.items.at(0).moves.at(0).target, u"_ausschuss/a.png"_s);
    }

    void collisionAddsSuffixToImageAndSidecars()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"a.png.json"_s, u"_keep/a.png"_s});

        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Keep}, ResultFolders{}, defaultSidecarExtensions());
        QCOMPARE(plan.items.at(0).moves,
                 (QList<FileMove>{{u"a.png"_s, u"_keep/a-1.png"_s}, {u"a.png.json"_s, u"_keep/a-1.png.json"_s}, {u"a.txt"_s, u"_keep/a-1.txt"_s}}));
    }

    void takenSidecarNameMovesTheWholeGroup()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Only the caption name is taken, and so is the first suffix.
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"_keep/a.txt"_s, u"_keep/a-1.png"_s});

        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Keep}, ResultFolders{}, defaultSidecarExtensions());
        QCOMPARE(plan.items.at(0).moves, (QList<FileMove>{{u"a.png"_s, u"_keep/a-2.png"_s}, {u"a.txt"_s, u"_keep/a-2.txt"_s}}));
    }

    void collisionsWithinThePlan()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"a-1.png"_s, u"_rest/a.png"_s});

        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s, u"a-1.png"_s}, {Role::Rest, Role::Rest}, ResultFolders{}, noSidecars);
        QCOMPARE(allMoves(plan), (QList<FileMove>{{u"a.png"_s, u"_rest/a-1.png"_s}, {u"a-1.png"_s, u"_rest/a-1-1.png"_s}}));
    }

    void sameNameInDifferentFoldersIsFine()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"_rest/a.png"_s});
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Keep}, ResultFolders{}, noSidecars);
        QCOMPARE(plan.items.at(0).moves.at(0).target, u"_keep/a.png"_s);
    }

    void executeMovesFilesAndWritesLog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"b.png"_s, u"_keep/a.png"_s});
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s, u"b.png"_s}, {Role::Keep, Role::Rest}, ResultFolders{}, defaultSidecarExtensions());

        LocalFileMover mover;
        ApplyLog log(dir.path());
        QList<qsizetype> progress;
        const ApplyReport report = executeApplyPlan(dir.path(), plan, mover, log, [&progress](qsizetype done, qsizetype total) {
            QCOMPARE(total, 3);
            progress.append(done);
        });
        QVERIFY(report.succeeded());
        QCOMPARE(report.moved, 3);
        QCOMPARE(progress, (QList<qsizetype>{1, 2, 3}));

        QCOMPARE(readFile(dir.filePath(u"_keep/a.png"_s)), QByteArray("_keep/a.png"));
        QCOMPARE(readFile(dir.filePath(u"_keep/a-1.png"_s)), QByteArray("a.png"));
        QCOMPARE(readFile(dir.filePath(u"_keep/a-1.txt"_s)), QByteArray("a.txt"));
        QCOMPARE(readFile(dir.filePath(u"_rest/b.png"_s)), QByteArray("b.png"));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files), QStringList{});
        // No reject folder without rejected images.
        QVERIFY(!QFileInfo::exists(dir.filePath(u"_reject"_s)));

        const QJsonObject root = readJson(log.path());
        QCOMPARE(root.value("schema"_L1).toInt(), 1);
        const QJsonArray applies = root.value("applies"_L1).toArray();
        QCOMPARE(applies.size(), 1);
        const QJsonObject apply = applies.first().toObject();
        QVERIFY(apply.contains("startedAt"_L1));
        QVERIFY(apply.contains("finishedAt"_L1));
        const QJsonArray moves = apply.value("moves"_L1).toArray();
        QCOMPARE(moves.size(), 3);
        QCOMPARE(moves.at(0).toObject().value("source"_L1).toString(), u"a.png"_s);
        QCOMPARE(moves.at(0).toObject().value("target"_L1).toString(), u"_keep/a-1.png"_s);
        QVERIFY(moves.at(0).toObject().contains("at"_L1));
        QCOMPARE(apply.value("errors"_L1).toArray().size(), 0);
    }

    void failedImageKeepsSidecarsAndOthersContinue()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"b.png"_s});
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s, u"b.png"_s}, {Role::Keep, Role::Keep}, ResultFolders{}, defaultSidecarExtensions());

        FailingMover mover({u"a.png"_s});
        ApplyLog log(dir.path());
        QList<qsizetype> progress;
        const ApplyReport report = executeApplyPlan(dir.path(), plan, mover, log, [&progress](qsizetype done, qsizetype) {
            progress.append(done);
        });
        QCOMPARE(report.moved, 1);
        QCOMPARE(report.errors.size(), 2);
        QCOMPARE(report.errors.at(0).file, u"a.png"_s);
        QCOMPARE(report.errors.at(1).file, u"a.txt"_s);
        QVERIFY(report.logError.isEmpty());
        QCOMPARE(progress, (QList<qsizetype>{2, 3}));
        QVERIFY(QFileInfo::exists(dir.filePath(u"a.png"_s)));
        QVERIFY(QFileInfo::exists(dir.filePath(u"a.txt"_s)));
        QVERIFY(QFileInfo::exists(dir.filePath(u"_keep/b.png"_s)));

        const QJsonObject apply = readJson(log.path()).value("applies"_L1).toArray().first().toObject();
        const QJsonArray errors = apply.value("errors"_L1).toArray();
        QCOMPARE(errors.size(), 1);
        QCOMPARE(errors.first().toObject().value("error"_L1).toString(), u"simulated failure"_s);
    }

    void fileAppearingAfterPlanningIsNotOverwritten()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s});
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Keep}, ResultFolders{}, noSidecars);
        writeFile(dir.filePath(u"_keep/a.png"_s), "newcomer");

        LocalFileMover mover;
        ApplyLog log(dir.path());
        const ApplyReport report = executeApplyPlan(dir.path(), plan, mover, log);
        QCOMPARE(report.moved, 0);
        QCOMPARE(report.errors.size(), 1);
        QCOMPARE(readFile(dir.filePath(u"_keep/a.png"_s)), QByteArray("newcomer"));
        QCOMPARE(readFile(dir.filePath(u"a.png"_s)), QByteArray("a.png"));
    }

    void secondApplyAppendsToLog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        LocalFileMover mover;
        ApplyLog log(dir.path());
        for (const QString &file : {u"a.png"_s, u"b.png"_s}) {
            createFiles(dir, {file});
            const ApplyPlan plan = buildApplyPlan(dir.path(), {file}, {Role::Keep}, ResultFolders{}, noSidecars);
            QVERIFY(executeApplyPlan(dir.path(), plan, mover, log).succeeded());
        }
        const QJsonArray applies = readJson(log.path()).value("applies"_L1).toArray();
        QCOMPARE(applies.size(), 2);
        QCOMPARE(applies.at(0).toObject().value("moves"_L1).toArray().first().toObject().value("source"_L1).toString(), u"a.png"_s);
        QCOMPARE(applies.at(1).toObject().value("moves"_L1).toArray().first().toObject().value("source"_L1).toString(), u"b.png"_s);
    }

    void unreadableLogStopsBeforeMoving()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        createFiles(dir, {u"a.png"_s});
        writeFile(dir.filePath(ApplyLog::fileName()), "{ broken");
        const ApplyPlan plan = buildApplyPlan(dir.path(), {u"a.png"_s}, {Role::Keep}, ResultFolders{}, noSidecars);

        LocalFileMover mover;
        ApplyLog log(dir.path());
        const ApplyReport report = executeApplyPlan(dir.path(), plan, mover, log);
        QVERIFY(!report.logError.isEmpty());
        QCOMPARE(report.moved, 0);
        QVERIFY(QFileInfo::exists(dir.filePath(u"a.png"_s)));
        QCOMPARE(readFile(dir.filePath(ApplyLog::fileName())), QByteArray("{ broken"));
    }

    void rejectToTrash()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Home and data directory inside the temporary directory: Qt then uses the home trash
        // there instead of a trash at the top of the file system.
        const EnvironmentOverride home("HOME", dir.path());
        const EnvironmentOverride data("XDG_DATA_HOME", dir.filePath(u"data"_s));
        createFiles(dir, {u"a.png"_s, u"a.txt"_s, u"b.png"_s});

        const ApplyPlan plan =
            buildApplyPlan(dir.path(), {u"a.png"_s, u"b.png"_s}, {Role::Reject, Role::Keep}, ResultFolders{}, defaultSidecarExtensions(), true);
        QVERIFY(plan.items.at(0).toTrash);
        QCOMPARE(plan.items.at(0).moves, (QList<FileMove>{{u"a.png"_s, {}}, {u"a.txt"_s, {}}}));
        QVERIFY(!plan.items.at(1).toTrash);

        LocalFileMover mover;
        ApplyLog log(dir.path());
        const ApplyReport report = executeApplyPlan(dir.path(), plan, mover, log);
        QVERIFY2(report.succeeded(), qPrintable(report.errors.isEmpty() ? report.logError : report.errors.first().message));
        QCOMPARE(report.moved, 3);
        const QString trash = dir.filePath(u"data/Trash"_s);
        QCOMPARE(readFile(trash + u"/files/a.png"_s), QByteArray("a.png"));
        QCOMPARE(readFile(trash + u"/files/a.txt"_s), QByteArray("a.txt"));
        QVERIFY(QFileInfo::exists(trash + u"/info/a.png.trashinfo"_s));
        QVERIFY(!QFileInfo::exists(dir.filePath(u"_reject"_s)));

        const QJsonArray moves = readJson(log.path()).value("applies"_L1).toArray().first().toObject().value("moves"_L1).toArray();
        QCOMPARE(QFileInfo(moves.at(0).toObject().value("target"_L1).toString()).canonicalFilePath(), QFileInfo(trash + u"/files/a.png"_s).canonicalFilePath());
    }

    void archiveKeepsEarlierArchives()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SessionStore store(dir.path());
        writeFile(dir.filePath(u".seedpick-session.done.json"_s), "first");
        QVERIFY(store.save(Session(SessionConfig{}, {}), QDateTime::currentDateTimeUtc()));

        QString error;
        const QString archived = store.archive(&error);
        QVERIFY2(!archived.isEmpty(), qPrintable(error));
        QCOMPARE(QFileInfo(archived).fileName(), u".seedpick-session.done-1.json"_s);
        QVERIFY(!store.exists());
        QCOMPARE(readFile(dir.filePath(u".seedpick-session.done.json"_s)), QByteArray("first"));
    }
};

QTEST_GUILESS_MAIN(TestApplyPlan)
#include "tst_applyplan.moc"
