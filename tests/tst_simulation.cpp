// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>

#include "simulation.h"

using namespace Seedpick;

// Acceptance criterion from KONZEPT.md section 3.4.
class TestSimulation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void noiseScaleMatchesErrorRate()
    {
        QCOMPARE(Sim::noiseScaleForErrorRate(0.0), 0.0);
        const double scale = Sim::noiseScaleForErrorRate(0.1);
        QVERIFY(scale > 0.2 && scale < 0.3);
        QVERIFY(Sim::noiseScaleForErrorRate(0.2) > scale);
    }

    void meetsAcceptanceCriterion()
    {
        Sim::Parameters parameters;
        parameters.images = 100;
        parameters.topN = 5;
        parameters.errorRate = 0.1;
#ifdef SEEDPICK_FULL_SIMULATION
        const Sim::Summary summary = Sim::simulateMany(parameters, 100);
        const double minHitRate = 0.85;
        const double maxRegret = 0.03;
#else
        // Sanitizer builds are too slow for the full run. Fewer runs need wider bounds.
        const Sim::Summary summary = Sim::simulateMany(parameters, 10);
        const double minHitRate = 0.75;
        const double maxRegret = 0.06;
#endif
        qInfo("hit rate %.3f, regret %.3f, comparisons %.1f", summary.meanHitRate, summary.meanRegret, summary.meanComparisons);
        QVERIFY(summary.meanHitRate >= minHitRate);
        QVERIFY(summary.meanRegret <= maxRegret);
        QVERIFY(summary.meanComparisons <= 250);
    }

    void leftBiasDoesNotDominate()
    {
        Sim::Parameters parameters;
        parameters.images = 50;
        parameters.topN = 5;
        parameters.leftBias = 0.5;
        const Sim::Summary summary = Sim::simulateMany(parameters, 10);
        QVERIFY(summary.meanHitRate >= 0.7);
    }
};

QTEST_GUILESS_MAIN(TestSimulation)
#include "tst_simulation.moc"
