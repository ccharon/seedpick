// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>
#include <QTimeZone>

#include "session.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

Q_DECLARE_METATYPE(Seedpick::Event)

namespace
{
QList<ImageInfo> makeImages(int count)
{
    QList<ImageInfo> images;
    for (int i = 0; i < count; ++i) {
        images.append(ImageInfo{u"img%1.png"_s.arg(i), 100 + i, QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC)});
    }
    return images;
}

QString img(int i)
{
    return u"img%1.png"_s.arg(i);
}

Event screen(int i, ScreenVerdict verdict = ScreenVerdict::Ok)
{
    return Event{ScreenEvent{img(i), verdict}, {}};
}

Event duel(int left, int right, DuelResult result = DuelResult::Left)
{
    return Event{DuelEvent{img(left), img(right), result}, {}};
}

SessionConfig config(int topN, bool skipScreening = false)
{
    SessionConfig result;
    result.topN = topN;
    result.skipScreening = skipScreening;
    return result;
}

void appendOrFail(Session &session, const Event &event)
{
    QString error;
    QVERIFY2(session.append(event, &error), qPrintable(error));
}

void screenAll(Session &session)
{
    for (qsizetype i = 0; i < session.images().size(); ++i) {
        appendOrFail(session, screen(int(i)));
    }
}
}

class TestSession : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startsInScreening()
    {
        const Session session(config(2), makeImages(4));
        QCOMPARE(session.phase(), Phase::Screening);
        QCOMPARE(session.unscreenedCount(), 4);
        QCOMPARE(session.nextUnscreened(), 0);
        QCOMPARE(session.activeCount(), 0);
    }

    void screeningLeadsToDuels()
    {
        Session session(config(2), makeImages(4));
        appendOrFail(session, screen(0));
        appendOrFail(session, screen(1, ScreenVerdict::Reject));
        QCOMPARE(session.nextUnscreened(), 2);
        appendOrFail(session, screen(2));
        appendOrFail(session, screen(3));

        QCOMPARE(session.phase(), Phase::Duels);
        QCOMPARE(session.activeCount(), 3);
        QVERIFY(session.isRejected(1));
        QVERIFY(!session.isActive(1));
        QCOMPARE(session.screeningState(1), ScreeningState::Reject);
    }

    void skipScreeningMakesAllActive()
    {
        const Session session(config(2, true), makeImages(4));
        QCOMPARE(session.phase(), Phase::Duels);
        QCOMPARE(session.activeCount(), 4);
    }

    void fewImagesSkipDuels()
    {
        Session session(config(3), makeImages(4));
        appendOrFail(session, screen(0));
        appendOrFail(session, screen(1));
        appendOrFail(session, screen(2));
        appendOrFail(session, screen(3, ScreenVerdict::Reject));
        QCOMPARE(session.phase(), Phase::Summary);
    }

    void duelsAreRecorded()
    {
        Session session(config(1, true), makeImages(4));
        appendOrFail(session, duel(0, 1, DuelResult::Left));
        appendOrFail(session, duel(2, 0, DuelResult::Tie));
        QCOMPARE(session.duels(), (QList<Duel>{{0, 1, DuelResult::Left}, {2, 0, DuelResult::Tie}}));
        QCOMPARE(session.duelCount(0), 2);
        QCOMPARE(session.duelCount(3), 0);
    }

    void rejectRemovesImageFromDuels()
    {
        Session session(config(1, true), makeImages(4));
        appendOrFail(session, duel(0, 1));
        appendOrFail(session, duel(2, 3));
        appendOrFail(session, Event{RejectEvent{img(1)}, {}});
        QVERIFY(session.isRejected(1));
        QCOMPARE(session.duels(), (QList<Duel>{{2, 3, DuelResult::Left}}));
        QCOMPARE(session.activeCount(), 3);
    }

    void finishEndsDuels()
    {
        Session session(config(1, true), makeImages(4));
        appendOrFail(session, Event{FinishEvent{}, {}});
        QVERIFY(session.isFinished());
        QCOMPARE(session.phase(), Phase::Summary);
        appendOrFail(session, Event{OverrideEvent{img(2), OverrideTarget::Keep}, {}});
        QCOMPARE(session.overrideTarget(2), OverrideTarget::Keep);
        QCOMPARE(session.overrideTarget(1), std::nullopt);
    }

    void validationRejectsInvalidEvents_data()
    {
        QTest::addColumn<bool>("skipScreening");
        QTest::addColumn<Event>("event");
        QTest::newRow("screen unknown file") << false << Event{ScreenEvent{u"other.png"_s, ScreenVerdict::Ok}, {}};
        QTest::newRow("screen during duels") << true << screen(0);
        QTest::newRow("duel during screening") << false << duel(0, 1);
        QTest::newRow("duel with itself") << true << duel(0, 0);
        QTest::newRow("duel unknown file") << true << Event{DuelEvent{img(0), u"other.png"_s, DuelResult::Left}, {}};
        QTest::newRow("reject during screening") << false << Event{RejectEvent{img(0)}, {}};
        QTest::newRow("override during duels") << true << Event{OverrideEvent{img(0), OverrideTarget::Keep}, {}};
        QTest::newRow("finish during screening") << false << Event{FinishEvent{}, {}};
    }

    void validationRejectsInvalidEvents()
    {
        QFETCH(bool, skipScreening);
        QFETCH(Event, event);
        Session session(config(1, skipScreening), makeImages(4));
        QString error;
        QVERIFY(!session.append(event, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(session.events().isEmpty());
    }

    void cannotScreenTwice()
    {
        Session session(config(1), makeImages(4));
        appendOrFail(session, screen(0));
        QVERIFY(!session.append(screen(0, ScreenVerdict::Reject)));
    }

    void cannotDuelRejectedImage()
    {
        Session session(config(1, true), makeImages(4));
        appendOrFail(session, Event{RejectEvent{img(0)}, {}});
        QVERIFY(!session.append(duel(0, 1)));
        QVERIFY(!session.append(Event{RejectEvent{img(0)}, {}}));
    }

    void undoRemovesLastEvent()
    {
        Session session(config(2), makeImages(4));
        screenAll(session);
        appendOrFail(session, duel(0, 1));
        QCOMPARE(session.phase(), Phase::Duels);

        QVERIFY(session.undo());
        QVERIFY(session.duels().isEmpty());
        QVERIFY(session.undo());
        QCOMPARE(session.phase(), Phase::Screening);
        QCOMPARE(session.nextUnscreened(), 3);
        QCOMPARE(session.events().size(), 3);
    }

    void undoOnEmptyLogFails()
    {
        Session session(config(2), makeImages(4));
        QVERIFY(!session.undo());
    }

    void undoReopensDuelsAfterFinish()
    {
        Session session(config(1, true), makeImages(4));
        appendOrFail(session, Event{FinishEvent{}, {}});
        QVERIFY(session.undo());
        QCOMPARE(session.phase(), Phase::Duels);
        QVERIFY(!session.isFinished());
    }

    void undoSkipsIneffectiveEvents()
    {
        // The duel against the vanished image has no effect and is dropped together with the screen event before it.
        const QList<Event> log{screen(0), screen(1), Event{DuelEvent{img(0), u"gone.png"_s, DuelResult::Left}, {}}};
        Session session(config(1), makeImages(3), log);
        QVERIFY(session.undo());
        QCOMPARE(session.events(), (QList<Event>{screen(0)}));
    }

    void replayGivesSameState()
    {
        Session live(config(2, true), makeImages(5));
        appendOrFail(live, duel(0, 1, DuelResult::Left));
        appendOrFail(live, duel(2, 3, DuelResult::Right));
        appendOrFail(live, Event{RejectEvent{img(4)}, {}});
        appendOrFail(live, duel(1, 2, DuelResult::Tie));

        const Session replayed(live.config(), live.images(), live.events());
        QCOMPARE(replayed.duels(), live.duels());
        QCOMPARE(replayed.phase(), live.phase());
        QCOMPARE(replayed.activeCount(), live.activeCount());
        for (qsizetype i = 0; i < live.images().size(); ++i) {
            QCOMPARE(replayed.isRejected(i), live.isRejected(i));
        }
    }

    void undoEqualsReplayWithoutLastEvent()
    {
        Session session(config(2, true), makeImages(5));
        appendOrFail(session, duel(0, 1));
        appendOrFail(session, Event{RejectEvent{img(1)}, {}});
        const Session before(session.config(), session.images(), session.events().first(1));
        QVERIFY(session.undo());
        QCOMPARE(session.duels(), before.duels());
        QCOMPARE(session.isRejected(1), false);
    }

    void reconcileReportsChanges()
    {
        QList<ImageInfo> stored = makeImages(3);
        Session session(config(1), stored, {screen(0), screen(2)});

        QList<ImageInfo> scanned = stored;
        scanned.removeAt(2);
        scanned[1].size += 1;
        scanned.append(ImageInfo{u"new.png"_s, 1, stored.first().mtime});

        const ReconcileReport report = session.reconcile(scanned);
        QCOMPARE(report.removed, QStringList{img(2)});
        QCOMPARE(report.changed, QStringList{img(1)});
        QCOMPARE(report.added, QStringList{u"new.png"_s});
        QCOMPARE(session.images(), scanned);

        // The event for the missing file stays in the log without effect.
        QCOMPARE(session.events().size(), 2);
        QCOMPARE(session.screeningState(session.indexOf(img(0))), ScreeningState::Ok);
        QCOMPARE(session.screeningState(session.indexOf(u"new.png"_s)), ScreeningState::Unscreened);
        QCOMPARE(session.phase(), Phase::Screening);
    }

    void reconcileWithoutChangesIsEmpty()
    {
        Session session(config(1), makeImages(3));
        QVERIFY(session.reconcile(makeImages(3)).isEmpty());
    }

    void returningFileGetsItsStateBack()
    {
        Session session(config(1), makeImages(3), {screen(2, ScreenVerdict::Reject)});
        session.reconcile(makeImages(2));
        QCOMPARE(session.indexOf(img(2)), -1);
        session.reconcile(makeImages(3));
        QVERIFY(session.isRejected(2));
    }
};

QTEST_GUILESS_MAIN(TestSession)
#include "tst_session.moc"
