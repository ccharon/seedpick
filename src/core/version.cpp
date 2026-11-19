// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "version.h"

#include "seedpick_version.h"

namespace Seedpick
{
QString versionString()
{
    return QStringLiteral(SEEDPICK_VERSION_STRING);
}

QString projectVersion()
{
    return QStringLiteral(SEEDPICK_PROJECT_VERSION);
}

QString appId()
{
    return QStringLiteral(SEEDPICK_APP_ID);
}
}
