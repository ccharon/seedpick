#!/bin/bash
# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Prepares a release: version in CMake, changelog, AppStream release entry,
# versioned ebuild, commit and annotated tag. Pushes only with --push.
# Usage: packaging/release.sh <X.Y.Z> [--push]
set -euo pipefail

version=${1:-}
push=false
[[ ${2:-} == --push ]] && push=true
if [[ ! $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "usage: packaging/release.sh <X.Y.Z> [--push]" >&2
    exit 1
fi

repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
cd "$repo"
tag=v$version
today=$(date -u +%Y-%m-%d)
metainfo=data/io.github.ccharon.seedpick.metainfo.xml

fail() {
    echo "release: $*" >&2
    exit 1
}

[[ -z $(git status --porcelain) ]] || fail "the working tree has uncommitted changes"
[[ $(git branch --show-current) == main ]] || fail "releases are made from main"
git rev-parse -q --verify "refs/tags/$tag" >/dev/null && fail "tag $tag exists already"
grep -q '^## \[Unreleased\]' CHANGELOG.md || fail "CHANGELOG.md has no [Unreleased] section"
# The Unreleased section must list at least one change.
awk '/^## \[Unreleased\]/ { inside = 1; next } /^## / { inside = 0 } inside && /^- / { found = 1 } END { exit !found }' CHANGELOG.md \
    || fail "the [Unreleased] section of CHANGELOG.md is empty"

sed -i -E "s/^(project\(seedpick VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1$version/" CMakeLists.txt
grep -q "project(seedpick VERSION $version" CMakeLists.txt || fail "cannot set the version in CMakeLists.txt"

sed -i "s/^## \[Unreleased\]$/## [Unreleased]\n\n## [$version] - $today/" CHANGELOG.md

if grep -q "<release version=\"$version\"" "$metainfo"; then
    sed -i -E "s|<release version=\"$version\"[^/]*/>|<release version=\"$version\" date=\"$today\"/>|" "$metainfo"
else
    sed -i "s|^\(\s*\)<releases>$|&\n\1  <release version=\"$version\" date=\"$today\"/>|" "$metainfo"
fi
if command -v appstreamcli >/dev/null; then
    appstreamcli validate --no-net "$metainfo" || fail "$metainfo does not validate"
fi

ebuild=$(packaging/make-ebuild.sh "$version")

git add CMakeLists.txt CHANGELOG.md "$metainfo" "$ebuild"
git commit -m "chore(release): $tag"
git tag -a "$tag" -m "Seedpick $version"

if $push; then
    git push origin main "$tag"
else
    echo "Tagged $tag. Publish with: git push origin main $tag"
fi
echo "The Manifest for ${ebuild##*/} needs the tag archive on GitHub:"
echo "  sudo packaging/install-local-overlay.sh"
