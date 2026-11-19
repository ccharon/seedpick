#!/bin/bash
# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Extracts the UI strings into po/seedpick.pot and merges them into every po/<lang>/seedpick.po.
set -euo pipefail
cd "$(dirname "$0")/.."

keywords=(
    --keyword=i18n:1 --keyword=i18nc:1c,2 --keyword=i18np:1,2 --keyword=i18ncp:1c,2,3
    --keyword=ki18n:1 --keyword=ki18nc:1c,2 --keyword=ki18np:1,2 --keyword=ki18ncp:1c,2,3
    --keyword=xi18n:1 --keyword=xi18nc:1c,2 --keyword=xi18np:1,2 --keyword=xi18ncp:1c,2,3
)
pot=po/seedpick.pot
common=(--from-code=UTF-8 --package-name=seedpick --msgid-bugs-address=https://github.com/ccharon/seedpick/issues "${keywords[@]}")

mapfile -t cpp < <(git ls-files 'src/*.cpp')
mapfile -t qml < <(git ls-files 'src/*.qml')
xgettext "${common[@]}" -C --kde -o "$pot" "${cpp[@]}"
xgettext "${common[@]}" -L JavaScript -j -o "$pot" "${qml[@]}"

for po in po/*/seedpick.po; do
    [[ -e $po ]] || continue
    msgmerge --quiet --update --backup=none "$po" "$pot"
done
echo "$pot: $(grep -c '^msgid ' "$pot") strings"
