# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

cmake_minimum_required(VERSION 3.22)

project(seedpick VERSION 0.1.0 LANGUAGES CXX)

set(SEEDPICK_APP_ID "io.github.ccharon.seedpick")
set(SEEDPICK_DISPLAY_NAME "Seedpick")

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.8.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEClangFormat)
include(KDEGitCommitHooks)
include(ECMAddTests)
include(ECMQmlModule)
include(FeatureSummary)

option(BUILD_APP "Build the Kirigami application (off builds only the core library and tests)" ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Test)
if(BUILD_APP)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Gui Widgets Qml Quick QuickControls2)
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS Config CoreAddons I18n)
    find_package(KF6Kirigami ${KF_MIN_VERSION} REQUIRED)
endif()

# Git builds get "-git+<shorthash>" appended. The hash is taken at configure time.
set(SEEDPICK_VERSION_STRING "${PROJECT_VERSION}")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SEEDPICK_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(SEEDPICK_GIT_HASH)
            set(SEEDPICK_VERSION_STRING "${PROJECT_VERSION}-git+${SEEDPICK_GIT_HASH}")
        endif()
    endif()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(BUILD_APP)
    add_subdirectory(data)
endif()

file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES CONFIGURE_DEPENDS
    src/*.cpp src/*.h tests/*.cpp tests/*.h
)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
