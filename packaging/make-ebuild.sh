#!/bin/bash
# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Writes media-gfx/seedpick ebuilds from packaging/templates/seedpick.ebuild.in.
# Usage: make-ebuild.sh 9999 | <version>
set -euo pipefail

version=${1:?usage: make-ebuild.sh 9999|<version>}
here=$(cd "$(dirname "$0")" && pwd)
template=$here/templates/seedpick.ebuild.in
target=$here/gentoo/media-gfx/seedpick/seedpick-$version.ebuild

if [[ $version == 9999 ]]; then
    inherit='inherit ecm git-r3'
    source='EGIT_REPO_URI="https://github.com/ccharon/seedpick.git"'
    keywords=''
elif [[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    inherit='inherit ecm'
    source='SRC_URI="https://github.com/ccharon/seedpick/archive/refs/tags/v${PV}.tar.gz -> ${P}.tar.gz"'
    keywords='KEYWORDS="~amd64"'
else
    echo "version must be 9999 or X.Y.Z" >&2
    exit 1
fi

awk -v inherit="$inherit" -v source="$source" -v keywords="$keywords" '
    $0 == "@INHERIT@" { print inherit; next }
    $0 == "@SOURCE@" { print source; next }
    $0 == "@KEYWORDS@" { if (keywords != "") print keywords; next }
    { print }
' "$template" >"$target"
echo "$target"
