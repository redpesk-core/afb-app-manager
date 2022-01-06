###########################################################################
# Copyright (C) 2015-2022 IoT.bzh Company
#
# Author: José Bollo <jose.bollo@iot.bzh>
#
# $RP_BEGIN_LICENSE$
# Commercial License Usage
#  Licensees holding valid commercial IoT.bzh licenses may use this file in
#  accordance with the commercial license agreement provided with the
#  Software or, alternatively, in accordance with the terms contained in
#  a written agreement between you and The IoT.bzh Company. For licensing terms
#  and conditions see https://www.iot.bzh/terms-conditions. For further
#  information use the contact form at https://www.iot.bzh/contact.
#
# GNU General Public License Usage
#  Alternatively, this file may be used under the terms of the GNU General
#  Public license version 3. This license is as published by the Free Software
#  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
#  of this file. Please review the following information to ensure the GNU
#  General Public License requirements will be met
#  https://www.gnu.org/licenses/gpl-3.0.html.
# $RP_END_LICENSE$
###########################################################################

cmake_minimum_required(VERSION 3.10)

if(NOT USE_SDK AND LEGACY_USER_DAEMON)
	configure_file(afm-user-daemon.service.in afm-user-daemon.service @ONLY)

	set(SYSCONFDIR_DBUS_USER   ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/session.d CACHE STRING "Path to dbus session configuration files")
	set(UNITDIR_USER           ${CMAKE_INSTALL_LIBDIR}/systemd/user CACHE STRING "Path to systemd system unit files")

	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/afm-user-daemon.conf      DESTINATION ${SYSCONFDIR_DBUS_USER})
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/afm-user-daemon.service   DESTINATION ${UNITDIR_USER})
endif()

