// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "btmodel.h"
#include "session.h"

#include <QList>

#include <optional>

namespace Seedpick
{
struct SchedulerOptions {
    double tau = 1.5;
    // Done when the expected share of wrong images in the top n falls below this value.
    double confidenceThreshold = 0.05;
    // Decisive results per pair. Ties do not count.
    int maxRepeatsPerPair = 2;
    // Soft budget in comparisons per active image.
    double budgetFactor = 2.5;
    UncertaintyMethod uncertainty = UncertaintyMethod::Exact;
    // Above this many active images the diagonal approximation is used, since exact is O(N^3).
    int exactUncertaintyLimit = 300;
};

struct RankedImage {
    // Index into Session::images().
    qsizetype image = -1;
    double strength = 0;
    double sigma = 0;
    // Probability of being on the wrong side of the top-n boundary.
    double ambiguity = 0;
    int duels = 0;
};

struct ImagePair {
    qsizetype left = -1;
    qsizetype right = -1;

    bool operator==(const ImagePair &) const = default;
};

struct Plan {
    // Active images, strongest first.
    QList<RankedImage> ranking;
    double boundary = 0;
    int comparisons = 0;
    int budget = 0;
    int estimatedRemaining = 0;
    // Model estimate of the share of the current top n that truly belongs there.
    double expectedHitRate = 1.0;
    // Every image has minDuels and the expected miss share is below the threshold.
    bool done = false;
    // Next pair to show. Also offered when done, so the user may continue.
    std::optional<ImagePair> next;

    bool budgetExceeded() const
    {
        return comparisons >= budget;
    }
};

// Fits the model to the session's duels and picks the next pair.
// Deterministic for a given session: the random stream depends on rngSeed and the log length.
Plan planNext(const Session &session, const SchedulerOptions &options = {});
}
