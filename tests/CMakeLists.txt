# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

ecm_add_tests(
    tst_event.cpp
    tst_imagescanner.cpp
    tst_version.cpp
    LINK_LIBRARIES seedpickcore Qt6::Test
)
