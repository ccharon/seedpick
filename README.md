<!--
SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
SPDX-License-Identifier: CC0-1.0
-->

# Seedpick

[![CI](https://github.com/ccharon/seedpick/actions/workflows/ci.yml/badge.svg)](https://github.com/ccharon/seedpick/actions/workflows/ci.yml)

KDE image viewer for picking the n best images from a folder by pairwise comparison.

Seedpick is made for sorting batches of generated images, where many variants of one prompt differ only in details.
Instead of rating every image, you compare two images at a time. A Bradley-Terry model picks the pair whose outcome tells it the most and stops once the top n are certain.

The design is described in [KONZEPT.md](KONZEPT.md) (German). Simulation results are in [docs/simulation.md](docs/simulation.md).

Status: usable, no release yet. Install from git (see [Gentoo](#gentoo)) or build it yourself.

## How it works

1. **Screening:** Every image is shown once. Keep it or reject the obvious failures.
2. **Duels:** Two images side by side, choose the better one. Seedpick decides when it has seen enough.
3. **Result:** Kept, remaining and rejected images in three columns. Drag an image or use its context menu to move it to another column.
4. **Apply:** Only now does Seedpick touch the folder. Kept images stay in place, the others move into `_rest` and `_reject` (or the trash). Files are never overwritten, a name collision gets a `-1` suffix.

Sidecar files with the same base name (`.txt`, `.json`, `.yaml`, `.yml`, `.xmp`, `.caption`) move together with their image.
Every move is written to `.seedpick-apply.log.jsonl` in the folder.

Progress is stored in `.seedpick-session.json` after every decision, so a session can be paused and resumed at any time, even after a crash.
Files added or removed in the meantime are picked up when the session is resumed.

For PNG files from ComfyUI and AUTOMATIC1111 (and its forks), Seedpick shows the prompt and the generation settings. In duels, settings that differ between both images are highlighted.

### Keys

| Key | Screening | Duels |
|---|---|---|
| Right / Space | Keep | Right image is better |
| Left | Previous image | Left image is better |
| Up | | Both equal |
| Down / Delete | Reject | |
| 1 / 2 | | Reject left / right image |
| Return | | Finish (once the ranking is certain) |
| Mouse wheel, + / − / 0 | Zoom, reset | Zoom both images together, reset |
| I | Show prompt and settings | Show prompt and settings |

In both phases: Backspace or Ctrl+Z undoes the last decision, F toggles full screen, Esc pauses the session.

## Build

Requirements: CMake 3.22 (3.25 for the presets), Ninja, ECM, Qt 6.6, KF6 6.8 (Config, CoreAddons, I18n, Kirigami).
Runtime: kimageformats, qqc2-desktop-style.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/seedpick [directory]
```

| Preset | Build type | Notes |
|---|---|---|
| `dev` | Debug | ASan and UBSan enabled |
| `release` | RelWithDebInfo | |

| CMake option | Type | Default | Effect |
|---|---|---|---|
| `BUILD_APP` | bool | `ON` | `OFF` builds only the core library and its tests |
| `BUILD_TESTING` | bool | `ON` | Builds the unit tests |

## Usage

```sh
seedpick [options] [directory]
```

| Option | Effect for this run |
|---|---|
| `-n, --top <count>` | Number of images to keep |
| `--keep-dir <name>` | Folder for the kept images (default: they stay in place) |
| `--rest-dir <name>` | Folder for the remaining images (default `_rest`) |
| `--reject-dir <name>` | Folder for rejected images (default `_reject`) |
| `--reject-to-trash` | Move rejected images to the trash |
| `--skip-screening` | Start new sessions without screening |

The settings page (gear icon) stores the defaults in `~/.config/seedpickrc`. Command line options are never stored.

## Gentoo

The ebuilds live in `packaging/gentoo`, which also works as an overlay on its own.

```sh
sudo packaging/install-local-overlay.sh            # copies media-gfx/seedpick to /var/db/repos/local
echo '=media-gfx/seedpick-9999 **' | sudo tee /etc/portage/package.accept_keywords/seedpick
sudo emerge -av media-gfx/seedpick
```

To build the live ebuild from a local clone instead of GitHub:
`sudo EGIT_OVERRIDE_REPO_CCHARON_SEEDPICK=file://$PWD emerge -av media-gfx/seedpick`.

Releases: `packaging/release.sh X.Y.Z [--push]` sets the version, updates the changelog and the AppStream release list, writes the versioned ebuild, commits and tags.

## Translations

Available: German.

`po/extract.sh` extracts the UI strings and merges them into `po/<lang>/seedpick.po`. To try a translation from the build tree:
`LANGUAGE=de XDG_DATA_DIRS=$PWD/build/dev:/usr/share ./build/dev/bin/seedpick`.

## Development

Formatting uses the KDE clang-format style. `ninja -C build/dev clang-format` formats all sources.
A git pre-commit hook that checks formatting is installed at configure time.

`tests/sim` holds a simulation that runs the pairing and stopping logic against synthetic ratings. `seedpick-sim --help` lists its options.

GitHub Actions builds and tests every push with ASan and UBSan in a Fedora container and checks formatting (clang-format 22) and REUSE compliance.

## License

GPL-2.0-or-later for code, CC0-1.0 for metadata. The repository follows the [REUSE](https://reuse.software) specification.
