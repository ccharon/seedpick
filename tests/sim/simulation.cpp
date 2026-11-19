// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simulation.h"

#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick::Sim
{
namespace
{
double sigmoid(double x)
{
    return 1.0 / (1.0 + std::exp(-x));
}

// Expected error on a random pair: E[sigmoid(-|d| / scale)] with d ~ N(0, 2).
double expectedError(double scale)
{
    constexpr int steps = 4000;
    constexpr double range = 12.0;
    const double sd = std::sqrt(2.0);
    double sum = 0;
    for (int k = 0; k < steps; ++k) {
        const double d = (k + 0.5) * range / steps;
        const double density = 2.0 * std::exp(-0.5 * (d / sd) * (d / sd)) / (sd * std::sqrt(2.0 * std::numbers::pi));
        sum += sigmoid(-d / scale) * density * (range / steps);
    }
    return sum;
}
}

double noiseScaleForErrorRate(double errorRate)
{
    if (errorRate <= 0) {
        return 0;
    }
    double low = 1e-6;
    double high = 100.0;
    for (int iteration = 0; iteration < 100; ++iteration) {
        const double middle = 0.5 * (low + high);
        (expectedError(middle) < errorRate ? low : high) = middle;
    }
    return 0.5 * (low + high);
}

RunResult simulate(const Parameters &parameters)
{
    std::mt19937_64 rng(parameters.seed);
    std::normal_distribution<double> normal;
    std::uniform_real_distribution<double> uniform;

    std::vector<double> quality(std::size_t(parameters.images));
    for (double &q : quality) {
        q = normal(rng);
    }
    const double scale = noiseScaleForErrorRate(parameters.errorRate);

    QList<ImageInfo> images;
    for (int i = 0; i < parameters.images; ++i) {
        images.append(ImageInfo{u"img%1.png"_s.arg(i), 1, QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC)});
    }
    SessionConfig config;
    config.topN = parameters.topN;
    config.minDuels = parameters.minDuels;
    config.rngSeed = quint32(rng());
    config.skipScreening = true;
    Session session(config, images);

    const int hardLimit = int(std::ceil(parameters.hardLimitFactor * parameters.images));
    Plan plan;
    RunResult result;
    while (true) {
        plan = planNext(session, parameters.scheduler);
        if (plan.done) {
            result.done = true;
            break;
        }
        if (!plan.next || plan.comparisons >= hardLimit || (!parameters.ignoreBudget && plan.budgetExceeded())) {
            break;
        }
        const ImagePair pair = *plan.next;
        const double d = quality[std::size_t(pair.left)] - quality[std::size_t(pair.right)];
        double logit = 0;
        if (parameters.noise == NoiseModel::Flip) {
            const double correct = std::log((1.0 - parameters.errorRate) / std::max(parameters.errorRate, 1e-12));
            logit = d > 0 ? correct : -correct;
        } else {
            logit = scale > 0 ? d / scale : (d > 0 ? 1e9 : -1e9);
        }
        logit += parameters.leftBias;
        const DuelResult answer = uniform(rng) < sigmoid(logit) ? DuelResult::Left : DuelResult::Right;
        session.append(Event{DuelEvent{images.at(pair.left).file, images.at(pair.right).file, answer}, {}});
    }
    result.comparisons = plan.comparisons;
    result.expectedHitRate = plan.expectedHitRate;

    std::vector<int> byQuality(std::size_t(parameters.images));
    for (int i = 0; i < parameters.images; ++i) {
        byQuality[std::size_t(i)] = i;
    }
    std::sort(byQuality.begin(), byQuality.end(), [&quality](int a, int b) {
        return quality[std::size_t(a)] > quality[std::size_t(b)];
    });
    QSet<qsizetype> truth;
    for (int r = 0; r < parameters.topN; ++r) {
        truth.insert(byQuality[std::size_t(r)]);
    }
    int hits = 0;
    for (int r = 0; r < parameters.topN && r < plan.ranking.size(); ++r) {
        hits += truth.contains(plan.ranking[r].image) ? 1 : 0;
        result.regret += quality[std::size_t(byQuality[std::size_t(r)])] - quality[std::size_t(plan.ranking[r].image)];
    }
    result.regret /= parameters.topN;
    result.hitRate = double(hits) / double(parameters.topN);
    return result;
}

Summary simulateMany(Parameters parameters, int runs)
{
    Summary summary;
    summary.runs = runs;
    summary.minHitRate = 1.0;
    std::vector<int> comparisons;
    const quint64 base = parameters.seed;
    for (int run = 0; run < runs; ++run) {
        parameters.seed = base * 1000003ULL + quint64(run);
        const RunResult result = simulate(parameters);
        summary.meanHitRate += result.hitRate;
        summary.meanExpectedHitRate += result.expectedHitRate;
        summary.minHitRate = std::min(summary.minHitRate, result.hitRate);
        summary.perfectRate += result.hitRate == 1.0 ? 1 : 0;
        summary.meanRegret += result.regret;
        summary.meanComparisons += result.comparisons;
        summary.doneRate += result.done ? 1 : 0;
        comparisons.push_back(result.comparisons);
    }
    summary.meanHitRate /= runs;
    summary.meanExpectedHitRate /= runs;
    summary.perfectRate /= runs;
    summary.meanRegret /= runs;
    summary.meanComparisons /= runs;
    summary.doneRate /= runs;
    std::sort(comparisons.begin(), comparisons.end());
    summary.p90Comparisons = comparisons[std::min(comparisons.size() - 1, std::size_t(std::ceil(0.9 * runs)) - 1)];
    return summary;
}
}
