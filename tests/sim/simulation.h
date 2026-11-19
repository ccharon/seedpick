// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "pairscheduler.h"

#include <QtGlobal>

namespace Seedpick::Sim
{
enum class NoiseModel {
    // Error probability falls with the quality gap. Near-equal images are close to a coin flip.
    Logistic,
    // Every answer is wrong with probability errorRate, independent of the gap.
    Flip,
};

struct Parameters {
    int images = 100;
    int topN = 5;
    int minDuels = 2;
    // Share of wrong answers over random pairs. Errors are logistic: close pairs are confused more often.
    double errorRate = 0.1;
    NoiseModel noise = NoiseModel::Logistic;
    // Added to the logit of choosing the left image.
    double leftBias = 0.0;
    // Keep comparing after the soft budget until done or this many comparisons per image.
    bool ignoreBudget = false;
    double hardLimitFactor = 10.0;
    quint64 seed = 1;
    SchedulerOptions scheduler;
};

struct RunResult {
    // Share of the true top n found.
    double hitRate = 0;
    // The model's own estimate of hitRate at the end of the run.
    double expectedHitRate = 0;
    // Mean true quality of the true top n minus that of the selection, in standard deviations.
    double regret = 0;
    int comparisons = 0;
    // Stopping criterion reached before the budget.
    bool done = false;
};

struct Summary {
    int runs = 0;
    double meanHitRate = 0;
    double meanExpectedHitRate = 0;
    double minHitRate = 0;
    // Share of runs that found the complete top n.
    double perfectRate = 0;
    double meanRegret = 0;
    double meanComparisons = 0;
    int p90Comparisons = 0;
    double doneRate = 0;
};

// Logistic noise scale that yields the given error rate on random pairs of N(0, 1) qualities.
double noiseScaleForErrorRate(double errorRate);

RunResult simulate(const Parameters &parameters);
Summary simulateMany(Parameters parameters, int runs);
}
