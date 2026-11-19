# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

ecm_add_tests(
    tst_btmodel.cpp
    tst_event.cpp
    tst_imagescanner.cpp
    tst_pairscheduler.cpp
    tst_session.cpp
    tst_sessionstore.cpp
    tst_version.cpp
    LINK_LIBRARIES seedpickcore Qt6::Test
)
