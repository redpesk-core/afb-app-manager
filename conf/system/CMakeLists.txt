###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
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

configure_file(afm-system-daemon.service.in afm-system-daemon.service)
configure_file(afm-system-daemon.socket.in afm-system-daemon.socket)

if(NOT USE_SDK)
	set(SYSCONFDIR_DBUS_SYSTEM ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d CACHE STRING "Path to dbus system configuration files")
	set(SYSCONFDIR_PAMD        ${CMAKE_INSTALL_SYSCONFDIR}/pam.d           CACHE STRING "Path to pam configuration files")
	set(UNITDIR_SYSTEM         ${CMAKE_INSTALL_LIBDIR}/systemd/system      CACHE STRING "Path to systemd system unit files")

	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/afm-system-daemon.conf    DESTINATION ${SYSCONFDIR_DBUS_SYSTEM})
	install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/afm-user-session.pamd     DESTINATION ${SYSCONFDIR_PAMD} RENAME afm-user-session)
	install(
		FILES
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-daemon.service
			${CMAKE_CURRENT_BINARY_DIR}/afm-system-daemon.socket
			${CMAKE_CURRENT_SOURCE_DIR}/afm-system-daemon.conf
			${CMAKE_CURRENT_SOURCE_DIR}/afm-user-session@.service
			${CMAKE_CURRENT_SOURCE_DIR}/afm-user-session@.target
		DESTINATION
			${UNITDIR_SYSTEM}
	)
endif()

