// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "filemover.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

using namespace Qt::Literals::StringLiterals;

namespace Seedpick
{
namespace
{
constexpr qint64 chunkSize = 1024 * 1024;

bool setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

QString systemError(int code)
{
    return QString::fromLocal8Bit(std::strerror(code));
}

bool sameContent(QFile &a, QFile &b)
{
    if (a.size() != b.size() || !a.seek(0) || !b.seek(0)) {
        return false;
    }
    while (!a.atEnd()) {
        const QByteArray left = a.read(chunkSize);
        const QByteArray right = b.read(chunkSize);
        if (left.isEmpty() || left != right) {
            return false;
        }
    }
    return b.atEnd();
}

enum class RenameResult {
    Done,
    Exists,
    OtherFileSystem,
    Failed,
};

RenameResult renameNoReplace(const QString &source, const QString &target, int *code)
{
    const QByteArray from = QFile::encodeName(source);
    const QByteArray to = QFile::encodeName(target);
#ifdef RENAME_NOREPLACE
    if (::renameat2(AT_FDCWD, from.constData(), AT_FDCWD, to.constData(), RENAME_NOREPLACE) == 0) {
        return RenameResult::Done;
    }
    *code = errno;
    if (*code == EEXIST) {
        return RenameResult::Exists;
    }
    if (*code == EXDEV) {
        return RenameResult::OtherFileSystem;
    }
    if (*code != EINVAL && *code != ENOSYS && *code != ENOTSUP) {
        return RenameResult::Failed;
    }
#endif
    // The file system cannot rename without replacing. Check first, rename() would overwrite.
    if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) {
        return RenameResult::Exists;
    }
    if (std::rename(from.constData(), to.constData()) == 0) {
        return RenameResult::Done;
    }
    *code = errno;
    return *code == EXDEV ? RenameResult::OtherFileSystem : RenameResult::Failed;
}
}

FileMover::~FileMover() = default;

LocalFileMover::LocalFileMover(Method method)
    : m_method(method)
{
}

bool LocalFileMover::move(const QString &source, const QString &target, QString *error)
{
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile() || sourceInfo.isSymLink()) {
        return setError(error, u"%1 is not a regular file"_s.arg(source));
    }
    const QFileInfo targetInfo(target);
    if (targetInfo.exists() || targetInfo.isSymLink()) {
        return setError(error, u"%1 already exists"_s.arg(target));
    }
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        return setError(error, u"cannot create %1"_s.arg(targetInfo.absolutePath()));
    }
    if (m_method == Method::Copy) {
        return copyMove(source, target, error);
    }

    int code = 0;
    switch (renameNoReplace(source, target, &code)) {
    case RenameResult::Done:
        return true;
    case RenameResult::Exists:
        return setError(error, u"%1 already exists"_s.arg(target));
    case RenameResult::OtherFileSystem:
        return copyMove(source, target, error);
    case RenameResult::Failed:
        break;
    }
    return setError(error, u"cannot move %1 to %2: %3"_s.arg(source, target, systemError(code)));
}

bool LocalFileMover::copyMove(const QString &source, const QString &target, QString *error) const
{
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly)) {
        return setError(error, u"cannot read %1: %2"_s.arg(source, in.errorString()));
    }
    // NewOnly fails if the target appeared in the meantime.
    QFile out(target);
    if (!out.open(QIODevice::ReadWrite | QIODevice::NewOnly)) {
        return setError(error, u"cannot create %1: %2"_s.arg(target, out.errorString()));
    }
    auto discard = [&out, &error](const QString &message) {
        out.close();
        out.remove();
        return setError(error, message);
    };

    while (!in.atEnd()) {
        const QByteArray chunk = in.read(chunkSize);
        if (chunk.isEmpty() || out.write(chunk) != chunk.size()) {
            return discard(u"cannot copy %1 to %2: %3"_s.arg(source, target, out.errorString()));
        }
    }
    if (!out.flush() || ::fsync(out.handle()) != 0) {
        return discard(u"cannot write %1"_s.arg(target));
    }
    out.setPermissions(in.permissions());
    out.setFileTime(QFileInfo(source).lastModified(), QFileDevice::FileModificationTime);
    if (!sameContent(in, out)) {
        return discard(u"copy of %1 differs from the original"_s.arg(source));
    }
    in.close();
    out.close();
    if (!QFile::remove(source)) {
        // Keep the original rather than two copies.
        QFile::remove(target);
        return setError(error, u"cannot remove %1 after copying"_s.arg(source));
    }
    return true;
}
}
