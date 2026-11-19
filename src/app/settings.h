// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "applyplan.h"
#include "sidecars.h"

#include <QString>
#include <QStringList>

#include <optional>

class QCommandLineParser;

// Effective settings of a run: the stored configuration with command line overrides applied.
struct Settings {
    int topN = 5;
    bool skipScreening = false;
    int minDuels = 2;
    double confidenceThreshold = 0.05;
    Seedpick::ResultFolders folders;
    bool rejectToTrash = false;
    QStringList sidecarExtensions = Seedpick::defaultSidecarExtensions();
    bool keepSessionAfterApply = true;

    bool operator==(const Settings &) const = default;
};

// Values given on the command line. They apply to the current run and are never stored.
struct CommandLineOverrides {
    std::optional<int> topN;
    std::optional<QString> keepDir;
    std::optional<QString> restDir;
    std::optional<QString> rejectDir;
    bool rejectToTrash = false;
    bool skipScreening = false;

    // The given options, for example "--top".
    QStringList options() const;
};

void addCommandLineOptions(QCommandLineParser &parser);
// Returns std::nullopt and sets `error` for invalid values.
std::optional<CommandLineOverrides> commandLineOverrides(const QCommandLineParser &parser, QString *error);

Settings applyOverrides(Settings settings, const CommandLineOverrides &overrides);

// A folder name directly inside the image directory: not empty, no path separator, not "." or "..".
bool isValidFolderName(const QString &name);

// Replaces invalid folder names with the defaults (an empty keep folder is valid and keeps the
// images in place), clamps numbers and drops empty or dotted
// sidecar extensions.
Settings sanitized(Settings settings);
