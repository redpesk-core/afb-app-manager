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

MESSAGE(STATUS "Creating afmpkg-daemon and afmpkg-status")
add_executable(afmpkg-daemon main-afmpkg-daemon.c)
target_link_libraries(afmpkg-daemon afmpkg wgtpkg wgt secwrp utils pthread)
install(TARGETS afmpkg-daemon DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(afmpkg-status main-afmpkg-status.c)
install(TARGETS afmpkg-status DESTINATION ${CMAKE_INSTALL_BINDIR})

if(libsystemd_FOUND AND AFB_FOUND)
	MESSAGE(STATUS "Creating afm-user-session")

	add_executable(afm-user-session main-afm-user-session.c)
	install(TARGETS afm-user-session DESTINATION ${afm_libexecdir}
			PERMISSIONS SETUID OWNER_READ OWNER_WRITE OWNER_EXECUTE
					GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
else()
	MESSAGE("WARNING, Not creating afm-user-session!")
endif()

if(WITH_TOOLS)

	MESSAGE(STATUS "Creating tools for legacy widgets")
	add_executable(afm-domain-spec main-domain-spec.c)
	target_link_libraries(afm-domain-spec utils)

	if(${gnutls_FOUND})
		add_executable(afm-check-signature main-check-signature.c)
		target_link_libraries(afm-check-signature ${gnutls_LINK_LIBRARIES} cert utils)

		add_executable(afm-signed-digest main-signed-digest.c)
		target_link_libraries(afm-signed-digest ${gnutls_LINK_LIBRARIES} cert utils)

		add_executable(afm-digest main-digest.c)
		target_link_libraries(afm-digest ${gnutls_LINK_LIBRARIES} cert utils)
	else()
		MESSAGE("WARNING, No gnutls, not creating afm cryptographic tools!")
	endif()

	if(WITH_WIDGETS)
		MESSAGE(STATUS "Creating tools for legacy widgets")

		add_executable(wgtpkg-sign main-wgtpkg-sign.c)
		target_link_libraries(wgtpkg-sign wgtpkg utils)
		install(TARGETS wgtpkg-sign DESTINATION ${CMAKE_INSTALL_BINDIR})

		add_executable(wgtpkg-pack main-wgtpkg-pack.c)
		target_link_libraries(wgtpkg-pack wgtpkg utils)
		install(TARGETS wgtpkg-pack DESTINATION ${CMAKE_INSTALL_BINDIR})

		add_executable(wgtpkg-info main-wgtpkg-info.c)
		target_link_libraries(wgtpkg-info wgtpkg wgt utils)
		install(TARGETS wgtpkg-info DESTINATION ${CMAKE_INSTALL_BINDIR})

		add_executable(wgtpkg-install main-wgtpkg-install.c)
		target_link_libraries(wgtpkg-install wgtpkg wgt secwrp utils)
		install(TARGETS wgtpkg-install DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()
