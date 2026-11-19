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
- Apply: kept images stay in the folder, the others move with their sidecar files into `_rest` and `_reject` without overwriting. A keep folder can be set. Apply can be cancelled, writes `.seedpick-apply.log.jsonl` (one line per move) and archives the session file.
- Ctrl+Q quits.
- Settings page and `seedpickrc` configuration for all options, command line options for a single run.
- Rejected images can go to the trash instead of a folder.
- Gentoo: live ebuild, ebuild template, local overlay installer and release script.

### Fixed

- Space on the setup page no longer resumes a session that does not exist.
