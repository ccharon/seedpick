// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

namespace Seedpick
{
struct Comparison {
    int first = 0;
    int second = 0;
    // 1 when first wins, 0 when second wins, 0.5 for a tie.
    double firstScore = 0.5;
};

enum class UncertaintyMethod {
    // Diagonal of the inverse Hessian via Cholesky, O(N^3), relative to the mean strength.
    Exact,
    // 1 / sqrt(H_ii), ignores correlations between items.
    Diagonal,
};

struct BtOptions {
    // Standard deviation of the Gaussian prior on each strength.
    double tau = 1.5;
    UncertaintyMethod uncertainty = UncertaintyMethod::Exact;
    int maxIterations = 100;
    // Stop when the largest gradient component falls below this value.
    double tolerance = 1e-10;
};

struct BtFit {
    std::vector<double> strength;
    std::vector<double> sigma;
    int iterations = 0;
    bool converged = false;
};

// MAP estimate of a Bradley-Terry model with prior N(0, tau^2), fitted from scratch by Newton's method.
// The result depends only on the multiset of comparisons, never on their order.
BtFit fitBradleyTerry(int count, const std::vector<Comparison> &comparisons, const BtOptions &options = {});
}
