# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(seedpick main.cpp)

ecm_add_qml_module(seedpick URI io.github.ccharon.seedpick GENERATE_PLUGIN_SOURCE)
ecm_target_qml_sources(seedpick SOURCES qml/Main.qml)

target_link_libraries(seedpick PRIVATE
    seedpickcore
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
)

install(TARGETS seedpick ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
