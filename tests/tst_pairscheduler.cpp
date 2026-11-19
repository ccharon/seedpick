// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QHash>
#include <QSet>
#include <QTest>
#include <QTimeZone>

#include "pairscheduler.h"

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
Session makeSession(int count, int topN, quint32 seed = 1)
{
    QList<ImageInfo> images;
    for (int i = 0; i < count; ++i) {
        images.append(ImageInfo{u"img%1.png"_s.arg(i), 1, QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC)});
    }
    SessionConfig config;
    config.topN = topN;
    config.rngSeed = seed;
    config.skipScreening = true;
    return Session(config, images);
}

// Lower index is better. The user never errs.
void answer(Session &session, const ImagePair &pair)
{
    const DuelResult result = pair.left < pair.right ? DuelResult::Left : DuelResult::Right;
    const Event event{DuelEvent{session.images().at(pair.left).file, session.images().at(pair.right).file, result}, {}};
    QString error;
    QVERIFY2(session.append(event, &error), qPrintable(error));
}

QList<ImagePair> play(Session &session, int steps)
{
    QList<ImagePair> pairs;
    for (int step = 0; step < steps; ++step) {
        const Plan plan = planNext(session);
        if (!plan.next) {
            break;
        }
        pairs.append(*plan.next);
        answer(session, *plan.next);
    }
    return pairs;
}
}

class TestPairScheduler : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void isDeterministic()
    {
        Session a = makeSession(10, 3, 42);
        Session b = makeSession(10, 3, 42);
        QCOMPARE(play(a, 20), play(b, 20));
    }

    void seedChangesPairs()
    {
        Session a = makeSession(10, 3, 1);
        Session b = makeSession(10, 3, 2);
        QVERIFY(play(a, 5) != play(b, 5));
    }

    void undoRestoresSamePair()
    {
        Session session = makeSession(10, 3);
        play(session, 7);
        const Plan before = planNext(session);
        QVERIFY(before.next);
        answer(session, *before.next);
        QVERIFY(session.undo());
        QCOMPARE(planNext(session).next, before.next);
    }

    void startRoundPairsEveryImageOnce()
    {
        Session session = makeSession(10, 3);
        play(session, 5);
        const Plan plan = planNext(session);
        for (const RankedImage &image : plan.ranking) {
            QCOMPARE(image.duels, 1);
        }
    }

    void noImageInConsecutiveDuels()
    {
        Session session = makeSession(12, 3);
        const QList<ImagePair> pairs = play(session, 40);
        for (qsizetype k = 1; k < pairs.size(); ++k) {
            const QSet<qsizetype> previous{pairs[k - 1].left, pairs[k - 1].right};
            QVERIFY(!previous.contains(pairs[k].left));
            QVERIFY(!previous.contains(pairs[k].right));
        }
    }

    void decisivePairsRepeatAtMostTwice()
    {
        Session session = makeSession(12, 3);
        const QList<ImagePair> pairs = play(session, 40);
        QHash<QPair<qsizetype, qsizetype>, int> seen;
        for (const ImagePair &pair : pairs) {
            const QPair<qsizetype, qsizetype> key{std::min(pair.left, pair.right), std::max(pair.left, pair.right)};
            QVERIFY(++seen[key] <= 2);
        }
    }

    void sidesAreBalanced()
    {
        Session session = makeSession(20, 5);
        const QList<ImagePair> pairs = play(session, 80);
        int betterOnLeft = 0;
        for (const ImagePair &pair : pairs) {
            betterOnLeft += pair.left < pair.right ? 1 : 0;
        }
        const double share = double(betterOnLeft) / double(pairs.size());
        QVERIFY2(share > 0.3 && share < 0.7, qPrintable(QString::number(share)));
    }

    void findsTrueTopNWithinBudget()
    {
        Session session = makeSession(16, 4);
        Plan plan = planNext(session);
        while (!plan.done && !plan.budgetExceeded()) {
            QVERIFY(plan.next);
            answer(session, *plan.next);
            plan = planNext(session);
        }
        QSet<qsizetype> top;
        for (int r = 0; r < 4; ++r) {
            top.insert(plan.ranking[r].image);
        }
        QCOMPARE(top, (QSet<qsizetype>{0, 1, 2, 3}));
        for (const RankedImage &image : plan.ranking) {
            QVERIFY(image.duels >= session.config().minDuels);
        }
    }

    void doneWhenEvidenceIsOverwhelming()
    {
        Session session = makeSession(6, 2);
        for (int round = 0; round < 15; ++round) {
            for (int i = 0; i < 6; ++i) {
                for (int j = i + 1; j < 6; ++j) {
                    answer(session, ImagePair{i, j});
                }
            }
        }
        const Plan plan = planNext(session);
        QVERIFY2(plan.done, qPrintable(QString::number(plan.expectedHitRate)));
        QVERIFY(plan.expectedHitRate > 0.95);
        QCOMPARE(plan.estimatedRemaining, 0);
        QCOMPARE(plan.ranking[0].image, 0);
        QCOMPARE(plan.ranking[1].image, 1);
    }

    void notDoneBeforeMinDuels()
    {
        Session session = makeSession(6, 2);
        answer(session, ImagePair{0, 5});
        const Plan plan = planNext(session);
        QVERIFY(!plan.done);
        QVERIFY(plan.estimatedRemaining > 0);
    }

    void rejectedImagesAreNotScheduled()
    {
        Session session = makeSession(8, 2);
        QVERIFY(session.append(Event{RejectEvent{u"img3.png"_s}, {}}));
        for (const ImagePair &pair : play(session, 20)) {
            QVERIFY(pair.left != 3 && pair.right != 3);
        }
        QCOMPARE(planNext(session).ranking.size(), 7);
    }

    void fewImagesNeedNoDuels()
    {
        const Session session = makeSession(3, 3);
        const Plan plan = planNext(session);
        QVERIFY(plan.done);
        QVERIFY(!plan.next);
        QCOMPARE(plan.ranking.size(), 3);
    }

    void twoImagesCanRepeat()
    {
        Session session = makeSession(2, 1);
        answer(session, ImagePair{0, 1});
        const Plan plan = planNext(session);
        // Decisive repeats are a last resort, used only when nothing else is left.
        if (!plan.done) {
            QVERIFY(plan.next);
        }
    }

    void budgetIsTwoAndAHalfPerImage()
    {
        const Session session = makeSession(20, 5);
        QCOMPARE(planNext(session).budget, 50);
    }
};

QTEST_GUILESS_MAIN(TestPairScheduler)
#include "tst_pairscheduler.moc"
