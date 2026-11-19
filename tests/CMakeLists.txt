# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

ecm_add_tests(
    tst_btmodel.cpp
    tst_event.cpp
    tst_imagescanner.cpp
    tst_pairscheduler.cpp
    tst_session.cpp
    tst_sessionstore.cpp
    tst_sidecars.cpp
    tst_version.cpp
    LINK_LIBRARIES seedpickcore Qt6::Test
)

add_subdirectory(sim)

if(BUILD_APP)
    # The controller is compiled into the test directly, it only needs the QML registration header.
    ecm_add_test(tst_controller.cpp ${CMAKE_SOURCE_DIR}/src/app/controller.cpp ${CMAKE_SOURCE_DIR}/src/app/controller.h
        TEST_NAME tst_controller
        LINK_LIBRARIES seedpickcore Qt6::Qml Qt6::Test
    )
    target_include_directories(tst_controller PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
endif()
