<!--
SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
SPDX-License-Identifier: CC0-1.0
-->

# Seedpick

KDE image viewer for picking the n best images from a folder by pairwise comparison.
Two images are shown side by side and the arrow keys choose the better one.
A Bradley-Terry model selects the next pair. The selected images stay in the folder, the others move into `_rest` and `_reject`.

The design is described in [KONZEPT.md](KONZEPT.md) (German). Simulation results are in [docs/simulation.md](docs/simulation.md).

Status: early development, nothing is usable yet.

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

Until the repository is on GitHub, build the live ebuild from the local clone:
`sudo EGIT_OVERRIDE_REPO_CCHARON_SEEDPICK=file://$PWD emerge -av media-gfx/seedpick`.

Releases: `packaging/release.sh X.Y.Z [--push]` sets the version, updates the changelog and the AppStream release list, writes the versioned ebuild, commits and tags.

## Development

Formatting uses the KDE clang-format style. `ninja -C build/dev clang-format` formats all sources.
A git pre-commit hook that checks formatting is installed at configure time.

## License

GPL-2.0-or-later for code, CC0-1.0 for metadata. The repository follows the [REUSE](https://reuse.software) specification.
