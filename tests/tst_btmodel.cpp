// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>

#include "btmodel.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace Seedpick;

namespace
{
double sigmoid(double x)
{
    return 1.0 / (1.0 + std::exp(-x));
}

bool near(double a, double b, double tolerance = 1e-9)
{
    return std::abs(a - b) <= tolerance;
}
}

class TestBtModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void withoutDataStrengthIsZero()
    {
        const BtFit fit = fitBradleyTerry(3, {});
        QVERIFY(fit.converged);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(fit.strength[i], 0.0);
            // Variance relative to the mean of three independent N(0, tau^2) values.
            QVERIFY(near(fit.sigma[i], 1.5 * std::sqrt(2.0 / 3.0)));
        }
    }

    void singleWinIsSymmetricAndFinite()
    {
        const BtFit fit = fitBradleyTerry(2, {{0, 1, 1.0}});
        QVERIFY(fit.converged);
        QVERIFY(fit.strength[0] > 0);
        QVERIFY(near(fit.strength[0], -fit.strength[1]));
    }

    void tieKeepsStrengthsEqual()
    {
        const BtFit fit = fitBradleyTerry(2, {{0, 1, 0.5}, {1, 0, 0.5}});
        QVERIFY(near(fit.strength[0], 0.0));
        QVERIFY(near(fit.strength[1], 0.0));
    }

    void unanimousWinsStayFinite()
    {
        std::vector<Comparison> comparisons(200, Comparison{0, 1, 1.0});
        const BtFit fit = fitBradleyTerry(2, comparisons);
        QVERIFY(fit.converged);
        QVERIFY(std::isfinite(fit.strength[0]));
        QVERIFY(fit.strength[0] < 10);
    }

    void chainIsOrdered()
    {
        const BtFit fit = fitBradleyTerry(4, {{0, 1, 1.0}, {1, 2, 1.0}, {2, 3, 1.0}});
        QVERIFY(fit.strength[0] > fit.strength[1]);
        QVERIFY(fit.strength[1] > fit.strength[2]);
        QVERIFY(fit.strength[2] > fit.strength[3]);
    }

    void satisfiesMapCondition()
    {
        const std::vector<Comparison> comparisons{{0, 1, 1.0}, {1, 2, 0.0}, {2, 0, 0.5}, {3, 1, 1.0}, {0, 3, 1.0}};
        const BtOptions options;
        const BtFit fit = fitBradleyTerry(4, comparisons, options);
        std::vector<double> gradient(4);
        for (int i = 0; i < 4; ++i) {
            gradient[i] = -fit.strength[i] / (options.tau * options.tau);
        }
        for (const Comparison &c : comparisons) {
            const double residual = c.firstScore - sigmoid(fit.strength[c.first] - fit.strength[c.second]);
            gradient[c.first] += residual;
            gradient[c.second] -= residual;
        }
        for (double g : gradient) {
            QVERIFY2(near(g, 0.0, 1e-8), qPrintable(QString::number(g)));
        }
    }

    void orderAndOrientationDoNotMatter()
    {
        std::mt19937 rng(7);
        std::vector<Comparison> comparisons;
        std::uniform_int_distribution<int> pick(0, 9);
        for (int k = 0; k < 60; ++k) {
            const int a = pick(rng);
            int b = pick(rng);
            if (a == b) {
                b = (b + 1) % 10;
            }
            comparisons.push_back({a, b, double(k % 3) / 2.0});
        }
        const BtFit reference = fitBradleyTerry(10, comparisons);

        std::vector<Comparison> shuffled = comparisons;
        std::shuffle(shuffled.begin(), shuffled.end(), rng);
        for (Comparison &c : shuffled) {
            if (rng() % 2) {
                c = Comparison{c.second, c.first, 1.0 - c.firstScore};
            }
        }
        const BtFit other = fitBradleyTerry(10, shuffled);
        for (int i = 0; i < 10; ++i) {
            QVERIFY(near(reference.strength[i], other.strength[i]));
            QVERIFY(near(reference.sigma[i], other.sigma[i]));
        }
    }

    void exactSigmaMatchesClosedForm()
    {
        // Two items, one tie: precision matrix [[q+w, -w], [-w, q+w]] with w = 1/4 at mu = 0.
        BtOptions options;
        options.tau = 2.0;
        const BtFit fit = fitBradleyTerry(2, {{0, 1, 0.5}}, options);
        const double q = 0.25;
        const double w = 0.25;
        const double expected = std::sqrt((q + w) / (q * (q + 2 * w)) - 1.0 / (2 * q));
        QVERIFY(near(fit.sigma[0], expected));
        QVERIFY(near(fit.sigma[1], expected));
    }

    void diagonalSigmaKeepsStrengths()
    {
        const std::vector<Comparison> comparisons{{0, 1, 1.0}, {1, 2, 1.0}, {2, 3, 0.5}, {3, 0, 0.0}, {4, 0, 1.0}};
        BtOptions exact;
        BtOptions diagonal;
        diagonal.uncertainty = UncertaintyMethod::Diagonal;
        const BtFit a = fitBradleyTerry(6, comparisons, exact);
        const BtFit b = fitBradleyTerry(6, comparisons, diagonal);
        for (int i = 0; i < 6; ++i) {
            QVERIFY(near(a.strength[i], b.strength[i]));
        }
        // Item 5 has no comparisons.
        QVERIFY(near(a.sigma[5], 1.5 * std::sqrt(5.0 / 6.0)));
        QVERIFY(near(b.sigma[5], 1.5));
    }

    void recoversKnownStrengths()
    {
        const std::vector<double> truth{1.5, 0.8, 0.0, -0.7, -1.6};
        std::mt19937 rng(1);
        std::uniform_real_distribution<double> uniform(0, 1);
        std::vector<Comparison> comparisons;
        for (int round = 0; round < 400; ++round) {
            for (int i = 0; i < 5; ++i) {
                for (int j = i + 1; j < 5; ++j) {
                    comparisons.push_back({i, j, uniform(rng) < sigmoid(truth[i] - truth[j]) ? 1.0 : 0.0});
                }
            }
        }
        const BtFit fit = fitBradleyTerry(5, comparisons);
        QVERIFY(fit.converged);
        for (int i = 0; i < 5; ++i) {
            QVERIFY2(near(fit.strength[i], truth[i], 0.15), qPrintable(QString::number(fit.strength[i])));
            QVERIFY(fit.sigma[i] < 0.1);
        }
    }
};

QTEST_GUILESS_MAIN(TestBtModel)
#include "tst_btmodel.moc"
