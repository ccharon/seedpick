# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(seedpicksim STATIC simulation.cpp simulation.h)
target_include_directories(seedpicksim PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(seedpicksim PUBLIC seedpickcore)

add_executable(seedpick-sim main.cpp)
target_link_libraries(seedpick-sim PRIVATE seedpicksim)

ecm_add_test(${CMAKE_CURRENT_SOURCE_DIR}/../tst_simulation.cpp
    TEST_NAME tst_simulation
    LINK_LIBRARIES seedpicksim Qt6::Test
)

# The full acceptance run needs an optimized build without sanitizers.
if(NOT ECM_ENABLE_SANITIZERS)
    target_compile_definitions(tst_simulation PRIVATE SEEDPICK_FULL_SIMULATION)
endif()
