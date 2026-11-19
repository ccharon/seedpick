// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "settings.h"

#include <KLocalizedString>

#include <QCommandLineParser>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace
{
const QString topOption = u"top"_s;
const QString keepDirOption = u"keep-dir"_s;
const QString restDirOption = u"rest-dir"_s;
const QString rejectDirOption = u"reject-dir"_s;
const QString rejectToTrashOption = u"reject-to-trash"_s;
const QString skipScreeningOption = u"skip-screening"_s;

constexpr int maxTopN = 9999;
}

QStringList CommandLineOverrides::options() const
{
    QStringList result;
    if (topN) {
        result.append(u"--"_s + topOption);
    }
    if (keepDir) {
        result.append(u"--"_s + keepDirOption);
    }
    if (restDir) {
        result.append(u"--"_s + restDirOption);
    }
    if (rejectDir) {
        result.append(u"--"_s + rejectDirOption);
    }
    if (rejectToTrash) {
        result.append(u"--"_s + rejectToTrashOption);
    }
    if (skipScreening) {
        result.append(u"--"_s + skipScreeningOption);
    }
    return result;
}

void addCommandLineOptions(QCommandLineParser &parser)
{
    parser.addOption({{u"n"_s, topOption}, i18n("Number of images to keep."), i18nc("@info:shell value name", "count")});
    parser.addOption({keepDirOption, i18n("Folder for the kept images."), i18nc("@info:shell value name", "name")});
    parser.addOption({restDirOption, i18n("Folder for the remaining images."), i18nc("@info:shell value name", "name")});
    parser.addOption({rejectDirOption, i18n("Folder for rejected images."), i18nc("@info:shell value name", "name")});
    parser.addOption({rejectToTrashOption, i18n("Move rejected images to the trash.")});
    parser.addOption({skipScreeningOption, i18n("Start new sessions without screening.")});
}

std::optional<CommandLineOverrides> commandLineOverrides(const QCommandLineParser &parser, QString *error)
{
    CommandLineOverrides overrides;
    if (parser.isSet(topOption)) {
        bool ok = false;
        const int topN = parser.value(topOption).toInt(&ok);
        if (!ok || topN < 1 || topN > maxTopN) {
            *error = i18n("--top expects a number between 1 and %1.", QString::number(maxTopN));
            return std::nullopt;
        }
        overrides.topN = topN;
    }
    const std::pair<const QString &, std::optional<QString> &> folders[] = {
        {keepDirOption, overrides.keepDir},
        {restDirOption, overrides.restDir},
        {rejectDirOption, overrides.rejectDir},
    };
    for (const auto &[option, target] : folders) {
        if (!parser.isSet(option)) {
            continue;
        }
        const QString name = parser.value(option);
        if (!isValidFolderName(name)) {
            *error = i18n("--%1 expects a folder name without \"/\".", option);
            return std::nullopt;
        }
        target = name;
    }
    overrides.rejectToTrash = parser.isSet(rejectToTrashOption);
    overrides.skipScreening = parser.isSet(skipScreeningOption);
    return overrides;
}

Settings applyOverrides(Settings settings, const CommandLineOverrides &overrides)
{
    if (overrides.topN) {
        settings.topN = *overrides.topN;
    }
    if (overrides.keepDir) {
        settings.folders.keep = *overrides.keepDir;
    }
    if (overrides.restDir) {
        settings.folders.rest = *overrides.restDir;
    }
    if (overrides.rejectDir) {
        settings.folders.reject = *overrides.rejectDir;
    }
    settings.rejectToTrash = settings.rejectToTrash || overrides.rejectToTrash;
    settings.skipScreening = settings.skipScreening || overrides.skipScreening;
    return settings;
}

bool isValidFolderName(const QString &name)
{
    return !name.trimmed().isEmpty() && name == name.trimmed() && !name.contains(u'/') && name != "."_L1 && name != ".."_L1;
}

Settings sanitized(Settings settings)
{
    const Settings defaults;
    for (auto member : {&Seedpick::ResultFolders::keep, &Seedpick::ResultFolders::rest, &Seedpick::ResultFolders::reject}) {
        if (!isValidFolderName(settings.folders.*member)) {
            settings.folders.*member = defaults.folders.*member;
        }
    }
    settings.topN = std::clamp(settings.topN, 1, maxTopN);
    settings.minDuels = std::clamp(settings.minDuels, 1, 10);
    settings.confidenceThreshold = std::clamp(settings.confidenceThreshold, 0.001, 0.5);

    QStringList extensions;
    for (const QString &extension : std::as_const(settings.sidecarExtensions)) {
        const QString trimmed = extension.trimmed();
        if (!trimmed.isEmpty() && !trimmed.contains(u'.') && !trimmed.contains(u'/') && !extensions.contains(trimmed, Qt::CaseInsensitive)) {
            extensions.append(trimmed);
        }
    }
    settings.sidecarExtensions = extensions;
    return settings;
}
