# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

add_subdirectory(core)

if(BUILD_APP)
    add_subdirectory(app)
endif()
