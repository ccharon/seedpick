// SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
// SPDX-License-Identifier: GPL-2.0-or-later

// Sanitizer builds of the GUI skip leak detection by default. At exit the GPU driver and
// libdbus still own blocks that reach Qt's font and HarfBuzz caches, and the system
// libraries lack frame pointers, so suppressions cannot target those reports.
// The tests keep leak detection; ASAN_OPTIONS=detect_leaks=1 turns it back on here.
#if defined(__SANITIZE_ADDRESS__)
// ECM builds with hidden visibility, the sanitizer runtime only finds exported symbols.
extern "C" __attribute__((visibility("default"), used)) const char *__asan_default_options()
{
    return "detect_leaks=0";
}
#endif
