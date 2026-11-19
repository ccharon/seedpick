<!--
SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
SPDX-License-Identifier: CC0-1.0
-->

# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
The project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Project skeleton: CMake/ECM, presets, core library, empty Kirigami window, REUSE metadata.
- Core: image scanner, session events, session state with undo, session file storage.
- Core: Bradley-Terry model, pair scheduler and simulation harness.
- GUI: screening and duels with keyboard control, session resume, image prefetching.
- Result page with three columns, drag and drop and context menu corrections.
- Apply: moves images and sidecar files into `_keep`, `_rest` and `_reject` without overwriting, writes `.seedpick-apply.log.json` and archives the session file.
- Ctrl+Q quits.

### Fixed

- Space on the setup page no longer resumes a session that does not exist.
