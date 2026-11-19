// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "sessionstore.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
const QDateTime created(QDate(2026, 11, 19), QTime(9, 0, 0, 1), QTimeZone::UTC);
const QDateTime updated(QDate(2026, 11, 19), QTime(10, 30, 0, 500), QTimeZone::UTC);

Session makeSession()
{
    SessionConfig config;
    config.topN = 3;
    config.minDuels = 4;
    config.rngSeed = 4000000000U;
    config.skipScreening = true;
    const QList<ImageInfo> images{
        {u"a.png"_s, 10, QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5, 6), QTimeZone::UTC)},
        {u"b.png"_s, 20, QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5, 7), QTimeZone::UTC)},
        {u"c.png"_s, 30, QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5, 8), QTimeZone::UTC)},
        {u"d.png"_s, 40, QDateTime(QDate(2025, 1, 2), QTime(3, 4, 5, 9), QTimeZone::UTC)},
    };
    const QList<Event> events{
        {DuelEvent{u"a.png"_s, u"b.png"_s, DuelResult::Left}, updated},
        {DuelEvent{u"c.png"_s, u"d.png"_s, DuelResult::Tie}, {}},
        {RejectEvent{u"d.png"_s}, updated},
    };
    return Session(config, images, events, created);
}

QJsonObject toObject(const QByteArray &data)
{
    return QJsonDocument::fromJson(data).object();
}

QByteArray toBytes(const QJsonObject &object)
{
    return QJsonDocument(object).toJson();
}

void writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}
}

class TestSessionStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip()
    {
        const Session session = makeSession();
        const LoadResult loaded = SessionStore::deserialize(SessionStore::serialize(session, updated));
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QVERIFY(loaded.session);
        QCOMPARE(loaded.session->config(), session.config());
        QCOMPARE(loaded.session->images(), session.images());
        QVERIFY(loaded.session->events() == session.events());
        QCOMPARE(loaded.session->createdAt(), created);
        QCOMPARE(loaded.updatedAt, updated);
        QCOMPARE(loaded.session->duels(), session.duels());
        QCOMPARE(loaded.session->phase(), session.phase());
    }

    void writesDocumentedKeys()
    {
        const QJsonObject root = toObject(SessionStore::serialize(makeSession(), updated));
        QCOMPARE(root.value("schema"_L1).toInt(), 1);
        QCOMPARE(root.value("createdAt"_L1).toString(), u"2026-11-19T09:00:00.001Z"_s);
        QCOMPARE(root.value("updatedAt"_L1).toString(), u"2026-11-19T10:30:00.500Z"_s);
        const QJsonObject config = root.value("config"_L1).toObject();
        QCOMPARE(config.value("n"_L1).toInt(), 3);
        QCOMPARE(config.value("minDuels"_L1).toInt(), 4);
        QCOMPARE(config.value("rngSeed"_L1).toInteger(), 4000000000LL);
        QCOMPARE(config.value("skipScreening"_L1).toBool(), true);
        const QJsonObject image = root.value("images"_L1).toArray().first().toObject();
        QCOMPARE(image.value("file"_L1).toString(), u"a.png"_s);
        QCOMPARE(image.value("size"_L1).toInteger(), 10);
        QCOMPARE(image.value("mtime"_L1).toString(), u"2025-01-02T03:04:05.006Z"_s);
        QCOMPARE(root.value("events"_L1).toArray().size(), 3);
    }

    void missingCreatedAtUsesSaveTime()
    {
        const Session session(SessionConfig{}, {});
        const QJsonObject root = toObject(SessionStore::serialize(session, updated));
        QCOMPARE(root.value("createdAt"_L1).toString(), root.value("updatedAt"_L1).toString());
    }

    void missingConfigUsesDefaults()
    {
        QJsonObject root = toObject(SessionStore::serialize(makeSession(), updated));
        root.remove("config"_L1);
        const LoadResult loaded = SessionStore::deserialize(toBytes(root));
        QVERIFY2(loaded.session, qPrintable(loaded.error));
        QCOMPARE(loaded.session->config(), SessionConfig{});
    }

    void rejectsInvalidFiles_data()
    {
        QTest::addColumn<QByteArray>("data");
        const QJsonObject valid = toObject(SessionStore::serialize(makeSession(), updated));
        auto modified = [&valid](auto change) {
            QJsonObject root = valid;
            change(root);
            return toBytes(root);
        };

        QTest::newRow("not json") << QByteArray("{ broken");
        QTest::newRow("array root") << QByteArray("[]");
        QTest::newRow("no schema") << modified([](QJsonObject &root) {
            root.remove("schema"_L1);
        });
        QTest::newRow("newer schema") << modified([](QJsonObject &root) {
            root.insert("schema"_L1, 2);
        });
        QTest::newRow("fractional schema") << modified([](QJsonObject &root) {
            root.insert("schema"_L1, 1.5);
        });
        QTest::newRow("bad config value") << modified([](QJsonObject &root) {
            root.insert("config"_L1, QJsonObject{{"n"_L1, 0}});
        });
        QTest::newRow("seed out of range") << modified([](QJsonObject &root) {
            root.insert("config"_L1, QJsonObject{{"rngSeed"_L1, 5000000000.0}});
        });
        QTest::newRow("no images") << modified([](QJsonObject &root) {
            root.remove("images"_L1);
        });
        QTest::newRow("duplicate image") << modified([](QJsonObject &root) {
            QJsonArray images = root.value("images"_L1).toArray();
            images.append(images.first());
            root.insert("images"_L1, images);
        });
        QTest::newRow("image without mtime") << modified([](QJsonObject &root) {
            QJsonArray images = root.value("images"_L1).toArray();
            QJsonObject first = images.first().toObject();
            first.remove("mtime"_L1);
            images.replace(0, first);
            root.insert("images"_L1, images);
        });
        QTest::newRow("unknown event") << modified([](QJsonObject &root) {
            QJsonArray events = root.value("events"_L1).toArray();
            events.append(QJsonObject{{"t"_L1, "teleport"_L1}});
            root.insert("events"_L1, events);
        });
    }

    void rejectsInvalidFiles()
    {
        QFETCH(QByteArray, data);
        const LoadResult loaded = SessionStore::deserialize(data);
        QVERIFY(!loaded.session);
        QVERIFY(!loaded.error.isEmpty());
    }

    void errorNamesBrokenEvent()
    {
        QJsonObject root = toObject(SessionStore::serialize(makeSession(), updated));
        QJsonArray events = root.value("events"_L1).toArray();
        events.replace(1, QJsonObject{{"t"_L1, "duel"_L1}});
        root.insert("events"_L1, events);
        QVERIFY(SessionStore::deserialize(toBytes(root)).error.startsWith("event 1:"_L1));
    }

    void saveAndLoadInDirectory()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SessionStore store(dir.path());
        QCOMPARE(store.path(), QDir(dir.path()).filePath(u".seedpick-session.json"_s));
        QVERIFY(!store.exists());

        QString error;
        QVERIFY2(store.save(makeSession(), updated, &error), qPrintable(error));
        QVERIFY(store.exists());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden), QStringList{SessionStore::fileName()});

        const LoadResult loaded = store.load();
        QVERIFY2(loaded.session, qPrintable(loaded.error));
        QVERIFY(loaded.session->events() == makeSession().events());
    }

    void saveReplacesPreviousFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SessionStore store(dir.path());
        Session session = makeSession();
        QVERIFY(store.save(session, updated));
        QVERIFY(session.undo());
        QVERIFY(store.save(session, updated.addSecs(1)));

        const LoadResult loaded = store.load();
        QVERIFY(loaded.session);
        QCOMPARE(loaded.session->events().size(), 2);
        QCOMPARE(loaded.updatedAt, updated.addSecs(1));
    }

    void failedSaveKeepsPreviousFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SessionStore store(dir.path());
        QVERIFY(store.save(makeSession(), updated));
        QFile::Permissions permissions = QFile::permissions(dir.path());
        QVERIFY(QFile::setPermissions(dir.path(), QFile::ReadOwner | QFile::ExeOwner));

        QString error;
        const bool saved = store.save(Session(SessionConfig{}, {}), updated, &error);
        QFile::setPermissions(dir.path(), permissions);
        if (saved) {
            QSKIP("directory permissions are not enforced (running as root?)");
        }
        QVERIFY(!error.isEmpty());
        const LoadResult loaded = store.load();
        QVERIFY(loaded.session);
        QCOMPARE(loaded.session->events().size(), 3);
    }

    void loadMissingFileFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const LoadResult loaded = SessionStore(dir.path()).load();
        QVERIFY(!loaded.session);
        QVERIFY(!loaded.error.isEmpty());
    }

    void corruptFileIsLeftUntouched()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const SessionStore store(dir.path());
        writeFile(store.path(), QByteArrayLiteral("{ broken"));
        QVERIFY(!store.load().session);
        QFile file(store.path());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArrayLiteral("{ broken"));
    }
};

QTEST_GUILESS_MAIN(TestSessionStore)
#include "tst_sessionstore.moc"
