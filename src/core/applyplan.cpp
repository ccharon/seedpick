// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "applyplan.h"

#include "applylog.h"
#include "filemover.h"
#include "sidecars.h"

#include <QDir>
#include <QHash>
#include <QSet>

#include <algorithm>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
namespace
{
// "img.png" with suffix 2 gives "img-2.png".
QString withSuffix(const QString &file, int suffix)
{
    if (suffix == 0) {
        return file;
    }
    const QString base = baseName(file);
    return u"%1-%2%3"_s.arg(base).arg(suffix).arg(file.mid(base.size()));
}

// Target name of a sidecar when its image is renamed from `image` to `renamed`.
QString renamedSidecar(const QString &sidecar, const QString &image, const QString &renamed)
{
    if (sidecar.startsWith(image + u'.')) {
        return renamed + sidecar.mid(image.size());
    }
    return baseName(renamed) + sidecar.mid(baseName(image).size());
}

QSet<QString> existingNames(const QString &folder)
{
    const QStringList names = QDir(folder).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    return QSet<QString>(names.cbegin(), names.cend());
}
}

QString ResultFolders::name(Role role) const
{
    switch (role) {
    case Role::Keep:
        return keep;
    case Role::Rest:
        return rest;
    case Role::Reject:
        return reject;
    }
    Q_UNREACHABLE_RETURN(keep);
}

QList<Role> assignRoles(const Session &session, const QList<RankedImage> &ranking)
{
    const qsizetype count = session.images().size();
    QList<Role> roles(count, Role::Rest);
    for (qsizetype i = 0; i < count; ++i) {
        if (session.isRejected(i)) {
            roles[i] = Role::Reject;
        }
    }
    for (qsizetype rank = 0; rank < ranking.size() && rank < session.config().topN; ++rank) {
        roles[ranking.at(rank).image] = Role::Keep;
    }
    for (qsizetype i = 0; i < count; ++i) {
        if (const std::optional<Role> role = session.overrideTarget(i)) {
            roles[i] = *role;
        }
    }
    return roles;
}

qsizetype ApplyPlan::fileCount() const
{
    qsizetype count = 0;
    for (const ApplyItem &item : items) {
        count += item.moves.size();
    }
    return count;
}

ApplyPlan buildApplyPlan(const QString &directory,
                         const QStringList &images,
                         const QList<Role> &roles,
                         const ResultFolders &folders,
                         const QStringList &sidecarExtensions,
                         bool rejectToTrash)
{
    Q_ASSERT(images.size() == roles.size());
    const QDir dir(directory);
    const QHash<QString, QStringList> sidecars = findSidecars(images, dir.entryList(QDir::Files), sidecarExtensions);
    QHash<QString, QSet<QString>> taken;

    ApplyPlan plan;
    for (qsizetype i = 0; i < images.size(); ++i) {
        const QString &image = images.at(i);
        if (rejectToTrash && roles.at(i) == Role::Reject) {
            ApplyItem item{image, Role::Reject, true, {{image, {}}}};
            for (const QString &sidecar : sidecars.value(image)) {
                item.moves.append({sidecar, {}});
            }
            plan.items.append(item);
            continue;
        }
        const QString folder = folders.name(roles.at(i));
        if (folder.isEmpty()) {
            continue;
        }
        if (!taken.contains(folder)) {
            taken.insert(folder, existingNames(dir.filePath(folder)));
        }
        QSet<QString> &names = taken[folder];
        const QStringList companions = sidecars.value(image);

        for (int suffix = 0;; ++suffix) {
            const QString renamed = withSuffix(image, suffix);
            QStringList targets{renamed};
            for (const QString &sidecar : companions) {
                targets.append(renamedSidecar(sidecar, image, renamed));
            }
            const bool free = std::none_of(targets.cbegin(), targets.cend(), [&names](const QString &target) {
                return names.contains(target);
            });
            if (!free) {
                continue;
            }

            ApplyItem item{image, roles.at(i), false, {}};
            item.moves.append({image, folder + u'/' + renamed});
            for (qsizetype s = 0; s < companions.size(); ++s) {
                item.moves.append({companions.at(s), folder + u'/' + targets.at(s + 1)});
            }
            for (const QString &target : std::as_const(targets)) {
                names.insert(target);
            }
            plan.items.append(item);
            break;
        }
    }
    return plan;
}

ApplyReport executeApplyPlan(const QString &directory,
                             const ApplyPlan &plan,
                             FileMover &mover,
                             ApplyLog &log,
                             const std::function<void(qsizetype, qsizetype)> &progress,
                             const std::function<bool()> &isCancelled)
{
    ApplyReport report;
    const QDir dir(directory);
    const qsizetype total = plan.fileCount();
    qsizetype handled = 0;
    auto advance = [&](qsizetype files) {
        handled += files;
        if (progress) {
            progress(handled, total);
        }
    };

    if (!log.begin(QDateTime::currentDateTimeUtc(), &report.logError)) {
        return report;
    }
    for (const ApplyItem &item : plan.items) {
        if (isCancelled && isCancelled()) {
            report.cancelled = true;
            break;
        }
        for (qsizetype m = 0; m < item.moves.size(); ++m) {
            FileMove move = item.moves.at(m);
            QString error;
            const bool moved = item.toTrash ? mover.moveToTrash(dir.filePath(move.source), &move.target, &error)
                                            : mover.move(dir.filePath(move.source), dir.filePath(move.target), &error);
            const QDateTime at = QDateTime::currentDateTimeUtc();
            const bool logged = moved ? log.recordMove(move, at, &report.logError) : log.recordError(move, error, at, &report.logError);
            if (moved) {
                ++report.moved;
            } else {
                report.errors.append({move.source, error});
            }
            if (!logged) {
                return report;
            }
            if (!moved && m == 0) {
                // The sidecars stay next to the image that could not be moved.
                for (qsizetype s = 1; s < item.moves.size(); ++s) {
                    report.errors.append({item.moves.at(s).source, u"%1 was not moved"_s.arg(item.image)});
                }
                advance(item.moves.size());
                break;
            }
            advance(1);
        }
    }
    log.finish(QDateTime::currentDateTimeUtc(), report.cancelled, &report.logError);
    return report;
}
}
