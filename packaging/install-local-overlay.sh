#!/bin/bash
# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copies media-gfx/seedpick into a local Portage repository, owned by root and
# read-only for everybody else, and writes the Manifest for released versions.
# Usage: sudo packaging/install-local-overlay.sh [repository, default /var/db/repos/local]
set -euo pipefail

target_repo=${1:-/var/db/repos/local}
source_dir=$(cd "$(dirname "$0")" && pwd)/gentoo/media-gfx/seedpick
target_dir=$target_repo/media-gfx/seedpick

if [[ $EUID -ne 0 ]]; then
    echo "Run as root: sudo $0 $*" >&2
    exit 1
fi
if [[ ! -f $target_repo/metadata/layout.conf ]]; then
    echo "$target_repo is not a Portage repository (metadata/layout.conf missing)" >&2
    exit 1
fi

mkdir -p "$target_repo/media-gfx"
chown root:root "$target_repo/media-gfx"
chmod 755 "$target_repo/media-gfx"

# --delete removes ebuilds that no longer exist in the project, the Manifest is rebuilt below.
rsync -a --delete --exclude Manifest --chown=root:root --chmod=D755,F644 "$source_dir/" "$target_dir/"

# Live ebuilds have no distfiles. Released versions fetch their tag archive here.
shopt -s nullglob
released=()
for file in "$target_dir"/seedpick-*.ebuild; do
    [[ $file == *-9999.ebuild ]] || released+=("$file")
done
if ((${#released[@]} > 0)); then
    ebuild "${released[0]}" manifest || echo "Manifest failed; is the tag archive on GitHub already?" >&2
else
    rm -f "$target_dir/Manifest"
fi

chown -R root:root "$target_dir"
find "$target_dir" -type d -exec chmod 755 {} +
find "$target_dir" -type f -exec chmod 644 {} +

echo "Installed into $target_dir:"
ls -l "$target_dir"
echo
echo "The live ebuild has no keywords. Accept it with:"
echo "  echo '=media-gfx/seedpick-9999 **' >> /etc/portage/package.accept_keywords/seedpick"
echo "  emerge -av media-gfx/seedpick"
