// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QJsonDocument>
#include <QTest>
#include <QTimeZone>

#include "event.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

Q_DECLARE_METATYPE(Seedpick::Event)

namespace
{
QJsonObject parse(const QByteArray &text)
{
    return QJsonDocument::fromJson(text).object();
}

const QDateTime timestamp(QDate(2026, 11, 19), QTime(10, 15, 2, 120), QTimeZone::UTC);
}

class TestEvent : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip_data()
    {
        QTest::addColumn<Event>("event");
        QTest::newRow("screen ok") << Event{ScreenEvent{u"a.png"_s, ScreenVerdict::Ok}, timestamp};
        QTest::newRow("screen reject") << Event{ScreenEvent{u"a.png"_s, ScreenVerdict::Reject}, timestamp};
        QTest::newRow("duel left") << Event{DuelEvent{u"a.png"_s, u"b.png"_s, DuelResult::Left}, timestamp};
        QTest::newRow("duel right") << Event{DuelEvent{u"a.png"_s, u"b.png"_s, DuelResult::Right}, timestamp};
        QTest::newRow("duel tie") << Event{DuelEvent{u"a.png"_s, u"b.png"_s, DuelResult::Tie}, timestamp};
        QTest::newRow("reject") << Event{RejectEvent{u"a.png"_s}, timestamp};
        QTest::newRow("override keep") << Event{OverrideEvent{u"a.png"_s, OverrideTarget::Keep}, timestamp};
        QTest::newRow("override rest") << Event{OverrideEvent{u"a.png"_s, OverrideTarget::Rest}, timestamp};
        QTest::newRow("override reject") << Event{OverrideEvent{u"a.png"_s, OverrideTarget::Reject}, timestamp};
        QTest::newRow("finish") << Event{FinishEvent{}, timestamp};
        QTest::newRow("without timestamp") << Event{RejectEvent{u"a.png"_s}, {}};
    }

    void roundTrip()
    {
        QFETCH(Event, event);
        QString error;
        const std::optional<Event> parsed = eventFromJson(eventToJson(event), &error);
        QVERIFY2(parsed.has_value(), qPrintable(error));
        QVERIFY(*parsed == event);
    }

    void writesDocumentedFormat()
    {
        const Event event{DuelEvent{u"a.png"_s, u"b.png"_s, DuelResult::Left}, timestamp};
        const QJsonObject expected = parse(R"({"t":"duel","at":"2026-11-19T10:15:02.120Z","left":"a.png","right":"b.png","result":"left"})");
        QCOMPARE(eventToJson(event), expected);
    }

    void omitsMissingTimestamp()
    {
        QVERIFY(!eventToJson(Event{FinishEvent{}, {}}).contains("at"_L1));
    }

    void normalizesTimestampToUtc()
    {
        const std::optional<Event> parsed = eventFromJson(parse(R"({"t":"finish","at":"2026-11-19T11:15:02.120+01:00"})"));
        QVERIFY(parsed);
        QCOMPARE(parsed->at, timestamp);
        QCOMPARE(parsed->at.timeSpec(), Qt::UTC);
    }

    void ignoresUnknownKeys()
    {
        const std::optional<Event> parsed = eventFromJson(parse(R"({"t":"reject","file":"a.png","future":42})"));
        QVERIFY(parsed);
        QVERIFY(*parsed == (Event{RejectEvent{u"a.png"_s}, {}}));
    }

    void rejectsMalformed_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("no type") << QByteArray(R"({"file":"a.png"})");
        QTest::newRow("unknown type") << QByteArray(R"({"t":"teleport"})");
        QTest::newRow("type not string") << QByteArray(R"({"t":1})");
        QTest::newRow("screen without file") << QByteArray(R"({"t":"screen","verdict":"ok"})");
        QTest::newRow("screen empty file") << QByteArray(R"({"t":"screen","file":"","verdict":"ok"})");
        QTest::newRow("screen bad verdict") << QByteArray(R"({"t":"screen","file":"a.png","verdict":"maybe"})");
        QTest::newRow("duel without right") << QByteArray(R"({"t":"duel","left":"a.png","result":"left"})");
        QTest::newRow("duel bad result") << QByteArray(R"({"t":"duel","left":"a.png","right":"b.png","result":"up"})");
        QTest::newRow("override without target") << QByteArray(R"({"t":"override","file":"a.png"})");
        QTest::newRow("timestamp not string") << QByteArray(R"({"t":"finish","at":5})");
        QTest::newRow("timestamp garbage") << QByteArray(R"({"t":"finish","at":"yesterday"})");
    }

    void rejectsMalformed()
    {
        QFETCH(QByteArray, json);
        QString error;
        QVERIFY(!eventFromJson(parse(json), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestEvent)
#include "tst_event.moc"
