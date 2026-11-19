// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>

namespace Seedpick
{
// Full version including the git suffix for development builds.
QString versionString();

// Plain x.y.z from project(), used for comparisons and packaging.
QString projectVersion();

QString appId();
}
