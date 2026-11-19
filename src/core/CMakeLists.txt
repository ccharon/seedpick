# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

configure_file(seedpick_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/seedpick_version.h @ONLY)

add_library(seedpickcore STATIC
    btmodel.cpp
    btmodel.h
    event.cpp
    event.h
    imagescanner.cpp
    imagescanner.h
    pairscheduler.cpp
    pairscheduler.h
    session.cpp
    session.h
    sessionstore.cpp
    sessionstore.h
    version.cpp
    version.h
)

target_include_directories(seedpickcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# The core must stay free of Qt GUI, Quick and KIO so it remains testable headless.
target_link_libraries(seedpickcore PUBLIC Qt6::Core)
