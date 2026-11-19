// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "event.h"
#include "pairscheduler.h"
#include "session.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace Seedpick
{
class ApplyLog;
class FileMover;

struct ResultFolders {
    QString keep = QStringLiteral("_keep");
    QString rest = QStringLiteral("_rest");
    QString reject = QStringLiteral("_reject");

    QString name(Role role) const;
};

// Role of every image, indexed like Session::images(). An override wins. Otherwise rejected
// images go to Reject, the first topN of the ranking to Keep and all other images to Rest.
QList<Role> assignRoles(const Session &session, const QList<RankedImage> &ranking);

// Paths relative to the session directory.
struct FileMove {
    QString source;
    QString target;

    bool operator==(const FileMove &) const = default;
};

struct ApplyItem {
    QString image;
    Role role = Role::Keep;
    // The files go to the trash. Their targets stay empty, the trash picks the names.
    bool toTrash = false;
    // The first move is the image itself, the others are its sidecars.
    QList<FileMove> moves;
};

struct ApplyPlan {
    QList<ApplyItem> items;

    qsizetype fileCount() const;
};

// Plans moving every image and its sidecars into the folder of its role. `roles` is indexed like
// `images`. A name already taken in the target folder, on disk or earlier in the plan, gets "-1",
// "-2", … before the extension. The image and its sidecars always get the same suffix.
// With `rejectToTrash`, rejected images and their sidecars go to the trash instead.
ApplyPlan buildApplyPlan(const QString &directory,
                         const QStringList &images,
                         const QList<Role> &roles,
                         const ResultFolders &folders,
                         const QStringList &sidecarExtensions,
                         bool rejectToTrash = false);

struct ApplyError {
    // Relative path of the file that was not moved.
    QString file;
    QString message;
};

struct ApplyReport {
    qsizetype moved = 0;
    QList<ApplyError> errors;
    // Set when the apply log cannot be written. Nothing is moved after that.
    QString logError;

    bool succeeded() const
    {
        return errors.isEmpty() && logError.isEmpty();
    }
};

// Moves the files of the plan and records each move in the log before the next one starts.
// Moves into the trash are logged with the absolute path in the trash as target.
// A failed file does not stop the others. When an image fails, its sidecars stay with it.
// `progress` receives the number of handled files and the total.
ApplyReport executeApplyPlan(const QString &directory,
                             const ApplyPlan &plan,
                             FileMover &mover,
                             ApplyLog &log,
                             const std::function<void(qsizetype, qsizetype)> &progress = {});
}
