// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace Seedpick
{
// Moves single files. Implementations never overwrite an existing target.
class FileMover
{
public:
    virtual ~FileMover();
    // Absolute paths. Creates missing parent directories of the target.
    virtual bool move(const QString &source, const QString &target, QString *error) = 0;
};

// Moves within the local file system: rename, or copy + verify + remove across file systems.
class LocalFileMover : public FileMover
{
public:
    enum class Method {
        // Rename, and copy only where renaming is impossible (another file system).
        Rename,
        // Always copy + verify + remove.
        Copy,
    };

    explicit LocalFileMover(Method method = Method::Rename);
    bool move(const QString &source, const QString &target, QString *error) override;

private:
    bool copyMove(const QString &source, const QString &target, QString *error) const;

    Method m_method;
};
}
