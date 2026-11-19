// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "btmodel.h"

#include <algorithm>
#include <cmath>

namespace Seedpick
{
namespace
{
double sigmoid(double x)
{
    return x >= 0 ? 1.0 / (1.0 + std::exp(-x)) : std::exp(x) / (1.0 + std::exp(x));
}

// log(sigmoid(x)) without overflow.
double logSigmoid(double x)
{
    return x >= 0 ? -std::log1p(std::exp(-x)) : x - std::log1p(std::exp(x));
}

double logPosterior(const std::vector<double> &mu, const std::vector<Comparison> &comparisons, double precision)
{
    double value = 0;
    for (double m : mu) {
        value -= 0.5 * precision * m * m;
    }
    for (const Comparison &c : comparisons) {
        const double d = mu[c.first] - mu[c.second];
        value += c.firstScore * logSigmoid(d) + (1.0 - c.firstScore) * logSigmoid(-d);
    }
    return value;
}

// Negative Hessian of the log posterior: graph Laplacian weighted with p(1-p) plus the prior precision.
struct Precision {
    const std::vector<Comparison> &comparisons;
    std::vector<double> weight;
    double prior;

    void multiply(const std::vector<double> &x, std::vector<double> &out) const
    {
        for (std::size_t i = 0; i < x.size(); ++i) {
            out[i] = prior * x[i];
        }
        for (std::size_t k = 0; k < comparisons.size(); ++k) {
            const Comparison &c = comparisons[k];
            const double flow = weight[k] * (x[c.first] - x[c.second]);
            out[c.first] += flow;
            out[c.second] -= flow;
        }
    }
};

// Conjugate gradient for the symmetric positive definite precision matrix.
std::vector<double> solve(const Precision &a, const std::vector<double> &b)
{
    const std::size_t n = b.size();
    std::vector<double> x(n, 0.0);
    std::vector<double> r = b;
    std::vector<double> p = r;
    std::vector<double> ap(n);
    double rr = 0;
    for (double v : r) {
        rr += v * v;
    }
    const double stop = 1e-24 * std::max(rr, 1e-300);
    for (std::size_t iteration = 0; iteration < 2 * n + 10 && rr > stop; ++iteration) {
        a.multiply(p, ap);
        double pap = 0;
        for (std::size_t i = 0; i < n; ++i) {
            pap += p[i] * ap[i];
        }
        const double alpha = rr / pap;
        double next = 0;
        for (std::size_t i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * ap[i];
            next += r[i] * r[i];
        }
        const double beta = next / rr;
        rr = next;
        for (std::size_t i = 0; i < n; ++i) {
            p[i] = r[i] + beta * p[i];
        }
    }
    return x;
}

// The all-ones vector is an eigenvector of the precision matrix with eigenvalue 1/tau^2, so the
// variance of a common shift of all strengths is tau^2/N. Subtracting it gives the variance
// relative to the mean strength, which is what matters for the ranking.
std::vector<double> exactSigma(const Precision &a, int count)
{
    const std::size_t n = std::size_t(count);
    // Dense lower triangle, row-major.
    std::vector<double> m(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        m[i * n + i] = a.prior;
    }
    for (std::size_t k = 0; k < a.comparisons.size(); ++k) {
        const std::size_t i = std::size_t(a.comparisons[k].first);
        const std::size_t j = std::size_t(a.comparisons[k].second);
        const double w = a.weight[k];
        m[i * n + i] += w;
        m[j * n + j] += w;
        m[std::max(i, j) * n + std::min(i, j)] -= w;
    }
    // Cholesky: m = L L^T, L overwrites the lower triangle.
    for (std::size_t j = 0; j < n; ++j) {
        double diagonal = m[j * n + j];
        for (std::size_t k = 0; k < j; ++k) {
            diagonal -= m[j * n + k] * m[j * n + k];
        }
        diagonal = std::sqrt(diagonal);
        m[j * n + j] = diagonal;
        for (std::size_t i = j + 1; i < n; ++i) {
            double value = m[i * n + j];
            for (std::size_t k = 0; k < j; ++k) {
                value -= m[i * n + k] * m[j * n + k];
            }
            m[i * n + j] = value / diagonal;
        }
    }
    // diag(A^-1)_i is the squared norm of column i of L^-1.
    std::vector<double> variance(n, 0.0);
    std::vector<double> column(n);
    for (std::size_t c = 0; c < n; ++c) {
        std::fill(column.begin(), column.end(), 0.0);
        column[c] = 1.0 / m[c * n + c];
        variance[c] += column[c] * column[c];
        for (std::size_t i = c + 1; i < n; ++i) {
            double value = 0;
            for (std::size_t k = c; k < i; ++k) {
                value -= m[i * n + k] * column[k];
            }
            column[i] = value / m[i * n + i];
            variance[c] += column[i] * column[i];
        }
    }
    const double commonShift = 1.0 / (a.prior * double(n));
    std::vector<double> sigma(n);
    for (std::size_t i = 0; i < n; ++i) {
        sigma[i] = std::sqrt(std::max(variance[i] - commonShift, 0.0));
    }
    return sigma;
}
}

BtFit fitBradleyTerry(int count, const std::vector<Comparison> &comparisons, const BtOptions &options)
{
    BtFit fit;
    fit.strength.assign(std::size_t(count), 0.0);
    const double prior = 1.0 / (options.tau * options.tau);
    Precision precision{comparisons, std::vector<double>(comparisons.size()), prior};
    std::vector<double> &mu = fit.strength;

    auto updateWeights = [&]() {
        for (std::size_t k = 0; k < comparisons.size(); ++k) {
            const double p = sigmoid(mu[comparisons[k].first] - mu[comparisons[k].second]);
            precision.weight[k] = p * (1.0 - p);
        }
    };

    for (fit.iterations = 0; fit.iterations < options.maxIterations; ++fit.iterations) {
        std::vector<double> gradient(mu.size());
        for (std::size_t i = 0; i < mu.size(); ++i) {
            gradient[i] = -prior * mu[i];
        }
        double largest = 0;
        for (const Comparison &c : comparisons) {
            const double residual = c.firstScore - sigmoid(mu[c.first] - mu[c.second]);
            gradient[c.first] += residual;
            gradient[c.second] -= residual;
        }
        for (double g : gradient) {
            largest = std::max(largest, std::abs(g));
        }
        if (largest < options.tolerance) {
            fit.converged = true;
            break;
        }
        updateWeights();
        const std::vector<double> step = solve(precision, gradient);

        // Backtracking keeps every step uphill; full Newton steps are accepted almost always.
        const double before = logPosterior(mu, comparisons, prior);
        std::vector<double> candidate(mu.size());
        double scale = 1.0;
        for (int halving = 0; halving < 30; ++halving, scale *= 0.5) {
            for (std::size_t i = 0; i < mu.size(); ++i) {
                candidate[i] = mu[i] + scale * step[i];
            }
            // The slack absorbs rounding near the optimum, where the true gain is below machine precision.
            if (logPosterior(candidate, comparisons, prior) >= before - 1e-12 * std::abs(before)) {
                break;
            }
        }
        mu = candidate;
    }

    updateWeights();
    if (options.uncertainty == UncertaintyMethod::Exact) {
        fit.sigma = exactSigma(precision, count);
    } else {
        std::vector<double> diagonal(std::size_t(count), prior);
        for (std::size_t k = 0; k < comparisons.size(); ++k) {
            diagonal[std::size_t(comparisons[k].first)] += precision.weight[k];
            diagonal[std::size_t(comparisons[k].second)] += precision.weight[k];
        }
        fit.sigma.resize(std::size_t(count));
        for (std::size_t i = 0; i < diagonal.size(); ++i) {
            fit.sigma[i] = 1.0 / std::sqrt(diagonal[i]);
        }
    }
    return fit;
}
}
