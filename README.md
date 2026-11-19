<!--
SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
SPDX-License-Identifier: CC0-1.0
-->

# Seedpick

KDE image viewer for picking the n best images from a folder by pairwise comparison.
Two images are shown side by side and the arrow keys choose the better one.
A Bradley-Terry model selects the next pair. The result is moved into `_keep`, `_rest` and `_reject`.

The design is described in [KONZEPT.md](KONZEPT.md) (German).

Status: early development, nothing is usable yet.

## Build

Requirements: CMake 3.22 (3.25 for the presets), Ninja, ECM, Qt 6.6, KF6 6.8 (CoreAddons, I18n, Kirigami).
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

Formatting uses the KDE clang-format style. `ninja -C build/dev clang-format` formats all sources.
A git pre-commit hook that checks formatting is installed at configure time.

## License

GPL-2.0-or-later for code, CC0-1.0 for metadata. The repository follows the [REUSE](https://reuse.software) specification.
