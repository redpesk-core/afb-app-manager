###########################################################################
# Copyright 2015 IoT.bzh
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

if(NOT USE_SDK)
	setc(SYSCONFDIR_DBUS_SYSTEM ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d)
	setc(SYSCONFDIR_DBUS_USER   ${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/session.d)
	setc(UNITDIR_SYSTEM         ${CMAKE_INSTALL_LIBDIR}/systemd/system)
	setc(UNITDIR_USER           ${CMAKE_INSTALL_LIBDIR}/systemd/user)

	install(FILES afm-system-daemon.conf    DESTINATION ${SYSCONFDIR_DBUS_SYSTEM})
	install(FILES afm-system-daemon.service DESTINATION ${UNITDIR_SYSTEM})
	install(FILES afm-user-daemon.conf      DESTINATION ${SYSCONFDIR_DBUS_USER})
	install(FILES afm-user-daemon.service   DESTINATION ${UNITDIR_USER})
	install(FILES afm-launch.conf           DESTINATION ${afm_confdir})
endif()

