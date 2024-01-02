###########################################################################
# Copyright (C) 2015-2024 IoT.bzh Company
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

if(NOT ONLY_SDK)
	MESSAGE(STATUS "Creating afmpkg-installer and afmpkg-status")
	add_executable(afmpkg-installer main-afmpkg-installer.c)
	target_compile_options(afmpkg-installer PRIVATE -DNO_SEND_SIGHUP_ALL=$<BOOL:${SIMULATE_SECMGR}>)
	target_link_libraries(afmpkg-installer afmpkg pthread)
	install(TARGETS afmpkg-installer DESTINATION ${CMAKE_INSTALL_BINDIR})

	add_executable(afmpkg-status main-afmpkg-status.c)
	install(TARGETS afmpkg-status DESTINATION ${CMAKE_INSTALL_BINDIR})

	if(libsystemd_FOUND AND AFB_FOUND)
		MESSAGE(STATUS "Creating afm-user-session")

		add_executable(afm-user-session main-afm-user-session.c)
		target_compile_options(afm-user-session PRIVATE ${libsystemd_CFLAGS})
		target_include_directories(afm-user-session PRIVATE ${libsystemd_INCLUDE_DIRS})
		target_link_libraries(afm-user-session PUBLIC ${libsystemd_LIBRARIES})
		target_link_directories(afm-user-session PUBLIC ${libsystemd_LIBRARY_DIRS})
		install(TARGETS afm-user-session DESTINATION ${afm_libexecdir}
				PERMISSIONS SETUID OWNER_READ OWNER_WRITE OWNER_EXECUTE
						GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

	add_executable(afm-check-pkg main-afmpkg-check.c)
	target_link_libraries(afm-check-pkg afmpkg)
	install(TARGETS afm-check-pkg DESTINATION ${CMAKE_INSTALL_BINDIR})
	else()
		MESSAGE("WARNING, Not creating afm-user-session!")
	endif()
endif()

if(WITH_TOOLS)
	MESSAGE(STATUS "Creating tools")
	add_executable(afm-domain-spec main-domain-spec.c)
	target_link_libraries(afm-domain-spec utils)
	install(TARGETS afm-domain-spec DESTINATION ${CMAKE_INSTALL_BINDIR})

	if(${gnutls_FOUND})
		# TODO: these programs are compiled but not installed
		# because their names are not 100% good and they are not
		# needed today
		add_executable(afm-check-signature main-check-signature.c)
		target_link_libraries(afm-check-signature ${gnutls_LINK_LIBRARIES} cert utils)

		add_executable(afm-signed-digest main-signed-digest.c)
		target_link_libraries(afm-signed-digest ${gnutls_LINK_LIBRARIES} cert utils)

		add_executable(afm-digest main-digest.c)
		target_link_libraries(afm-digest ${gnutls_LINK_LIBRARIES} cert utils)

		install(TARGETS afm-check-signature afm-signed-digest afm-digest DESTINATION ${CMAKE_INSTALL_BINDIR})
	else()
		MESSAGE("WARNING, No gnutls, not creating afm cryptographic tools!")
	endif()

	if(libxml_FOUND)
		MESSAGE(STATUS "Creating tools for migrating widgets")
		add_executable(wgt-migrate main-migrate-config.c)
		target_link_libraries(wgt-migrate wgt utils)
		install(TARGETS wgt-migrate DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()
