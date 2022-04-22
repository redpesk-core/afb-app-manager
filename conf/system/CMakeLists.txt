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

if(AGL_DEVEL)
  set(create_platform_debug "true")
endif()

configure_file(afm-system-daemon.service.in afm-system-daemon.service  @ONLY)
configure_file(afm-system-daemon.socket.in  afm-system-daemon.socket   @ONLY)
configure_file(afm-system-setup.service.in  afm-system-setup.service   @ONLY)
configure_file(afm-system-setup.sh.in       afm-system-setup.sh        @ONLY)
configure_file(afm-api-afm-main@.service.in afm-api-afm-main@.service  @ONLY)
configure_file(afm-create-user.sh.in        afm-create-user.sh         @ONLY)
configure_file(afm-user-session@.service.in afm-user-session@.service  @ONLY)
configure_file(afm-user-setup@.service.in   afm-user-setup@.service    @ONLY)
configure_file(afm-user-setup.sh.in         afm-user-setup.sh          @ONLY)
configure_file(afmpkg-installer.service.in  afmpkg-installer.service   @ONLY)
configure_file(afmpkg-installer.socket.in   afmpkg-installer.socket    @ONLY)

if(NOT USE_SDK)
	set(SYSCONFDIR_DBUS_SYSTEM ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d CACHE STRING "Path to dbus system configuration files")
	set(SYSCONFDIR_PAMD        ${CMAKE_INSTALL_SYSCONFDIR}/pam.d           CACHE STRING "Path to pam configuration files")
	set(UNITDIR_SYSTEM         ${CMAKE_INSTALL_LIBDIR}/systemd/system      CACHE STRING "Path to systemd system unit files")

	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/afm-system-daemon.conf    DESTINATION ${SYSCONFDIR_DBUS_SYSTEM})
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/afm-user-session.pamd     DESTINATION ${SYSCONFDIR_PAMD} RENAME afm-user-session)
	install(
		FILES
			${CMAKE_CURRENT_BINARY_DIR}/afm-api-afm-main@.service
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-daemon.service
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-daemon.socket
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-setup.service
			${CMAKE_CURRENT_BINARY_DIR}/afm-user-session@.service
			${CMAKE_CURRENT_SOURCE_DIR}/afm-user-session@.target
			${CMAKE_CURRENT_BINARY_DIR}/afm-user-setup@.service
			${CMAKE_CURRENT_BINARY_DIR}/afmpkg-installer.service
			${CMAKE_CURRENT_BINARY_DIR}/afmpkg-installer.socket
		DESTINATION
			${UNITDIR_SYSTEM}
	)
	install(
		DIRECTORY
			${CMAKE_CURRENT_SOURCE_DIR}/user-runtime-dir@.service.wants
		DESTINATION
			${UNITDIR_SYSTEM}
	)
	install(
		PROGRAMS
			${CMAKE_CURRENT_BINARY_DIR}/afm-create-user.sh
			${CMAKE_CURRENT_BINARY_DIR}/afm-user-setup.sh
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-setup.sh
		DESTINATION
			${afm_libexecdir}
	)
endif()

