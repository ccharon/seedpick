// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "pairscheduler.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Seedpick
{
namespace
{
// SplitMix64. Standard library distributions differ between implementations, this does not.
class Random
{
public:
    Random(quint64 seed, quint64 stream)
        : m_state(seed ^ (stream * 0x9E3779B97F4A7C15ULL))
    {
        next();
    }

    quint64 next()
    {
        quint64 z = (m_state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    std::size_t below(std::size_t bound)
    {
        return std::size_t(next() % bound);
    }

    template<typename T>
    void shuffle(std::vector<T> &values)
    {
        for (std::size_t i = values.size(); i > 1; --i) {
            std::swap(values[i - 1], values[below(i)]);
        }
    }

private:
    quint64 m_state;
};

double normalCdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double sigmoid(double x)
{
    return 1.0 / (1.0 + std::exp(-x));
}

quint64 pairKey(int a, int b)
{
    return (quint64(std::min(a, b)) << 32) | quint64(std::max(a, b));
}
}

Plan planNext(const Session &session, const SchedulerOptions &options)
{
    Plan plan;
    const SessionConfig &config = session.config();

    // Local indices 0..n-1 over the active images.
    std::vector<qsizetype> images;
    QHash<qsizetype, int> local;
    for (qsizetype i = 0; i < session.images().size(); ++i) {
        if (session.isActive(i)) {
            local.insert(i, int(images.size()));
            images.push_back(i);
        }
    }
    const int count = int(images.size());
    const std::size_t size = images.size();

    const QList<Duel> duels = session.duels();
    std::vector<Comparison> comparisons;
    comparisons.reserve(std::size_t(duels.size()));
    std::vector<int> duelCount(size, 0);
    QHash<quint64, int> decisive;
    for (const Duel &duel : duels) {
        const int a = local.value(duel.left);
        const int b = local.value(duel.right);
        const double score = duel.result == DuelResult::Left ? 1.0 : duel.result == DuelResult::Right ? 0.0 : 0.5;
        comparisons.push_back({a, b, score});
        ++duelCount[std::size_t(a)];
        ++duelCount[std::size_t(b)];
        if (duel.result != DuelResult::Tie) {
            ++decisive[pairKey(a, b)];
        }
    }

    BtOptions btOptions;
    btOptions.tau = options.tau;
    btOptions.uncertainty = count > options.exactUncertaintyLimit ? UncertaintyMethod::Diagonal : options.uncertainty;
    const BtFit fit = fitBradleyTerry(count, comparisons, btOptions);

    std::vector<int> order(size);
    for (int i = 0; i < count; ++i) {
        order[std::size_t(i)] = i;
    }
    std::sort(order.begin(), order.end(), [&fit](int a, int b) {
        const double sa = fit.strength[std::size_t(a)];
        const double sb = fit.strength[std::size_t(b)];
        return sa != sb ? sa > sb : a < b;
    });
    std::vector<int> rank(size);
    for (int r = 0; r < count; ++r) {
        rank[std::size_t(order[std::size_t(r)])] = r;
    }

    const int topN = config.topN;
    const bool contested = count > topN;
    std::vector<double> ambiguity(size, 0.0);
    if (contested) {
        plan.boundary = 0.5 * (fit.strength[std::size_t(order[std::size_t(topN - 1)])] + fit.strength[std::size_t(order[std::size_t(topN)])]);
        for (int i = 0; i < count; ++i) {
            const double distance = std::abs(fit.strength[std::size_t(i)] - plan.boundary);
            const double sigma = fit.sigma[std::size_t(i)];
            ambiguity[std::size_t(i)] = sigma > 0 ? normalCdf(-distance / sigma) : (distance == 0 ? 0.5 : 0.0);
        }
    }

    for (int r = 0; r < count; ++r) {
        const auto i = std::size_t(order[std::size_t(r)]);
        plan.ranking.append(RankedImage{images[i], fit.strength[i], fit.sigma[i], ambiguity[i], duelCount[i]});
    }

    plan.comparisons = int(duels.size());
    plan.budget = int(std::ceil(options.budgetFactor * count));

    double missing = 0;
    int unsure = 0;
    double expectedMisses = 0;
    for (int i = 0; i < count; ++i) {
        missing += std::max(0, config.minDuels - duelCount[std::size_t(i)]);
        unsure += ambiguity[std::size_t(i)] >= options.confidenceThreshold ? 1 : 0;
        if (rank[std::size_t(i)] < topN) {
            expectedMisses += ambiguity[std::size_t(i)];
        }
    }
    if (contested) {
        plan.expectedHitRate = 1.0 - expectedMisses / topN;
    }
    plan.done = !contested || (missing == 0 && 1.0 - plan.expectedHitRate < options.confidenceThreshold);
    plan.estimatedRemaining = plan.done ? 0 : std::min(std::max(0, plan.budget - plan.comparisons), int(std::ceil(missing / 2.0)) + 2 * unsure);

    if (!contested || count < 2) {
        return plan;
    }

    Random random(config.rngSeed, quint64(session.events().size()));

    // No image appears in two consecutive duels.
    std::vector<bool> blocked(size, false);
    if (count >= 4 && !comparisons.empty()) {
        blocked[std::size_t(comparisons.back().first)] = true;
        blocked[std::size_t(comparisons.back().second)] = true;
    }

    std::vector<int> candidates;
    for (int i = 0; i < count; ++i) {
        if (!blocked[std::size_t(i)]) {
            candidates.push_back(i);
        }
    }
    // Random order makes every remaining tie-break random.
    random.shuffle(candidates);

    auto unplayed = [&](int i) {
        return duelCount[std::size_t(i)] == 0;
    };

    // Priority for the first image: unplayed images (random start round), then the fairness rule,
    // then the highest ambiguity. The shuffle above makes remaining ties random.
    std::stable_sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        const int da = std::min(duelCount[std::size_t(a)], config.minDuels);
        const int db = std::min(duelCount[std::size_t(b)], config.minDuels);
        if (da != db) {
            return da < db;
        }
        return ambiguity[std::size_t(a)] * fit.sigma[std::size_t(a)] > ambiguity[std::size_t(b)] * fit.sigma[std::size_t(b)];
    });

    // Constraints are relaxed only when no first image has a partner under the stricter level.
    int first = -1;
    int second = -1;
    for (int level = 0; level < 3 && second < 0; ++level) {
        for (int candidate : candidates) {
            std::vector<int> partners;
            for (int i = 0; i < count; ++i) {
                if (i == candidate) {
                    continue;
                }
                if (level < 1 && blocked[std::size_t(i)]) {
                    continue;
                }
                if (level < 2 && decisive.value(pairKey(candidate, i)) >= options.maxRepeatsPerPair) {
                    continue;
                }
                partners.push_back(i);
            }
            if (partners.empty()) {
                continue;
            }
            random.shuffle(partners);
            first = candidate;

            if (unplayed(first)) {
                const auto other = std::find_if(partners.begin(), partners.end(), unplayed);
                if (other != partners.end()) {
                    second = *other;
                    break;
                }
            }

            const bool firstInTop = rank[std::size_t(first)] < topN;
            double bestScore = -std::numeric_limits<double>::infinity();
            for (int i : partners) {
                const double p = sigmoid(fit.strength[std::size_t(first)] - fit.strength[std::size_t(i)]);
                double score = 4.0 * p * (1.0 - p) + ambiguity[std::size_t(i)];
                if ((rank[std::size_t(i)] < topN) != firstInTop) {
                    score += 0.5;
                }
                if (duelCount[std::size_t(i)] < config.minDuels) {
                    score += 0.5;
                }
                if (score > bestScore) {
                    bestScore = score;
                    second = i;
                }
            }
            break;
        }
    }

    if (second < 0) {
        return plan;
    }
    if (random.next() & 1) {
        std::swap(first, second);
    }
    plan.next = ImagePair{images[std::size_t(first)], images[std::size_t(second)]};
    return plan;
}
}
