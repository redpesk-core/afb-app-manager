###########################################################################
# Copyright (C) 2015-2019 IoT.bzh
#
# author: José Bollo <jose.bollo@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

cmake_minimum_required(VERSION 2.8)

if(AGL_DEVEL)
  set(create_platform_debug "dodir ${afm_platform_rundir}/debug")
endif()

configure_file(afm-system-daemon.service.in afm-system-daemon.service)
configure_file(afm-system-daemon.socket.in  afm-system-daemon.socket)
configure_file(afm-system-setup.service.in  afm-system-setup.service)
configure_file(afm-system-setup.sh.in       afm-system-setup.sh)
configure_file(afm-api-afm-main@.service.in afm-api-afm-main@.service)
configure_file(afm-user-session@.service.in afm-user-session@.service)
configure_file(afm-user-setup@.service.in   afm-user-setup@.service)
configure_file(afm-user-setup.sh.in         afm-user-setup.sh)

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
		DESTINATION
			${UNITDIR_SYSTEM}
	)
	install(
		PROGRAMS
			${CMAKE_CURRENT_BINARY_DIR}/afm-user-setup.sh
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-setup.sh
		DESTINATION
			${afm_libexecdir}
	)
endif()

