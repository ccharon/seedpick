# SPDX-FileCopyrightText: 2026 Christian Charon <ccharon@mailbox.org>
# SPDX-License-Identifier: GPL-2.0-or-later

install(PROGRAMS ${SEEDPICK_APP_ID}.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${SEEDPICK_APP_ID}.metainfo.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES icons/${SEEDPICK_APP_ID}.svg DESTINATION ${KDE_INSTALL_FULL_ICONDIR}/hicolor/scalable/apps)
