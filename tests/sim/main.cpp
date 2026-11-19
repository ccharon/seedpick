// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "simulation.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

using namespace Seedpick;
using namespace Qt::Literals::StringLiterals;

namespace
{
QList<int> parseList(const QString &text)
{
    QList<int> values;
    for (const QString &part : text.split(u',', Qt::SkipEmptyParts)) {
        values.append(part.trimmed().toInt());
    }
    return values;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(u"Simulates a user comparing images and reports how well the top n is found."_s);
    parser.addHelpOption();
    const QCommandLineOption imagesOption(u"images"_s, u"Comma-separated image counts N."_s, u"list"_s, u"20,50,100,300"_s);
    const QCommandLineOption topOption(u"top"_s, u"Comma-separated values of n."_s, u"list"_s, u"1,3,5,10"_s);
    const QCommandLineOption runsOption(u"runs"_s, u"Runs per configuration."_s, u"count"_s, u"50"_s);
    const QCommandLineOption errorOption(u"error-rate"_s, u"Share of wrong answers on random pairs."_s, u"rate"_s, u"0.1"_s);
    const QCommandLineOption biasOption(u"left-bias"_s, u"Logit bias towards the left image."_s, u"logit"_s, u"0"_s);
    const QCommandLineOption tauOption(u"tau"_s, u"Prior standard deviation."_s, u"value"_s, u"1.5"_s);
    const QCommandLineOption minDuelsOption(u"min-duels"_s, u"Minimum duels per image."_s, u"count"_s, u"2"_s);
    const QCommandLineOption thresholdOption(u"threshold"_s, u"Confidence threshold."_s, u"value"_s, u"0.05"_s);
    const QCommandLineOption repeatsOption(u"max-repeats"_s, u"Decisive results per pair."_s, u"count"_s, u"2"_s);
    const QCommandLineOption budgetOption(u"budget"_s, u"Soft budget in comparisons per image."_s, u"factor"_s, u"2.5"_s);
    const QCommandLineOption noiseOption(u"noise"_s, u"Noise model: logistic or flip."_s, u"name"_s, u"logistic"_s);
    const QCommandLineOption diagonalOption(u"diagonal"_s, u"Use the diagonal uncertainty approximation."_s);
    const QCommandLineOption ignoreBudgetOption(u"ignore-budget"_s, u"Continue after the soft budget, up to 10 N."_s);
    const QCommandLineOption seedOption(u"seed"_s, u"Base seed."_s, u"value"_s, u"1"_s);
    parser.addOptions({imagesOption,
                       topOption,
                       runsOption,
                       errorOption,
                       biasOption,
                       tauOption,
                       minDuelsOption,
                       thresholdOption,
                       repeatsOption,
                       noiseOption,
                       budgetOption,
                       diagonalOption,
                       ignoreBudgetOption,
                       seedOption});
    parser.process(app);

    Sim::Parameters parameters;
    parameters.errorRate = parser.value(errorOption).toDouble();
    parameters.noise = parser.value(noiseOption) == "flip"_L1 ? Sim::NoiseModel::Flip : Sim::NoiseModel::Logistic;
    parameters.leftBias = parser.value(biasOption).toDouble();
    parameters.minDuels = parser.value(minDuelsOption).toInt();
    parameters.ignoreBudget = parser.isSet(ignoreBudgetOption);
    parameters.seed = parser.value(seedOption).toULongLong();
    parameters.scheduler.tau = parser.value(tauOption).toDouble();
    parameters.scheduler.confidenceThreshold = parser.value(thresholdOption).toDouble();
    parameters.scheduler.maxRepeatsPerPair = parser.value(repeatsOption).toInt();
    parameters.scheduler.budgetFactor = parser.value(budgetOption).toDouble();
    parameters.scheduler.uncertainty = parser.isSet(diagonalOption) ? UncertaintyMethod::Diagonal : UncertaintyMethod::Exact;
    const int runs = parser.value(runsOption).toInt();

    QTextStream out(stdout);
    out << "| N | n | mean hit rate | expected hit rate | min hit rate | perfect | regret | mean comparisons | p90 comparisons | comparisons/N | done before "
           "budget | ms/run |\n";
    out << "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    for (int images : parseList(parser.value(imagesOption))) {
        for (int topN : parseList(parser.value(topOption))) {
            if (topN >= images) {
                continue;
            }
            parameters.images = images;
            parameters.topN = topN;
            QElapsedTimer timer;
            timer.start();
            const Sim::Summary summary = Sim::simulateMany(parameters, runs);
            out << "| " << images << " | " << topN << " | " << QString::number(summary.meanHitRate, 'f', 3) << " | "
                << QString::number(summary.meanExpectedHitRate, 'f', 3) << " | " << QString::number(summary.minHitRate, 'f', 2) << " | "
                << QString::number(summary.perfectRate, 'f', 2) << " | " << QString::number(summary.meanRegret, 'f', 3) << " | "
                << QString::number(summary.meanComparisons, 'f', 1) << " | " << summary.p90Comparisons << " | "
                << QString::number(summary.meanComparisons / images, 'f', 2) << " | " << QString::number(summary.doneRate, 'f', 2) << " | "
                << timer.elapsed() / runs << " |\n";
            out.flush();
        }
    }
    return 0;
}
