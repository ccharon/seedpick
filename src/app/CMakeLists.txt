# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

add_executable(seedpick
    main.cpp
    settings.cpp
    settings.h
    controller.cpp
    controller.h
    imagecache.cpp
    imagecache.h
    imageurl.h
    asanoptions.cpp
)

kconfig_target_kcfg_file(seedpick
    FILE seedpick.kcfg
    CLASS_NAME Config
    SINGLETON
    MUTATORS
    NOTIFIERS
    GENERATE_PROPERTIES
    DEFAULT_VALUE_GETTERS
    GENERATE_MOC
    QML_REGISTRATION
)

ecm_add_qml_module(seedpick URI io.github.ccharon.seedpick GENERATE_PLUGIN_SOURCE)
ecm_target_qml_sources(seedpick SOURCES
    qml/Main.qml
    qml/AppliedView.qml
    qml/DuelView.qml
    qml/ImagePane.qml
    qml/Preloader.qml
    qml/ReconcileMessage.qml
    qml/ScreeningView.qml
    qml/SettingsPage.qml
    qml/StartView.qml
    qml/SummaryColumn.qml
    qml/SummaryView.qml
    qml/ZoomState.qml
)

target_link_libraries(seedpick PRIVATE
    seedpickcore
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::CoreAddons
    KF6::I18n
    KF6::I18nQml
)

install(TARGETS seedpick ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
