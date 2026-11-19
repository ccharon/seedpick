// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace Seedpick
{
QStringList defaultSidecarExtensions();

// Maps each image to the sidecar files among `entries`, the file names in the same directory.
// A sidecar is "<image>.<ext>" (img.png.xmp) or "<base>.<ext>" (img.txt), where base is the image
// name without its last extension and ext is one of `extensions`, compared case-insensitively.
// Entries listed in `images` are never sidecars. A "<base>.<ext>" file whose base matches several
// images (img.png and img.jpg) belongs to none of them. Images without sidecars have no key.
QHash<QString, QStringList> findSidecars(const QStringList &images, const QStringList &entries, const QStringList &extensions);

// File name without its last extension. "a.b.png" gives "a.b", a name without a dot stays as is.
QString baseName(const QString &file);
}
