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

if(libxml_FOUND AND WITH_LEGACY_WGTPKG)
	# check availability of libzip
	if(USE_LIBZIP)
		pkg_check_modules(libzip libzip>=0.11)
		set(USE_LIBZIP ${libzip_FOUND})
	endif()

	# check availability of openssl
	if(WITH_OPENSSL)
		pkg_check_modules(openssl openssl)
		set(WITH_OPENSSL ${openssl_FOUND})
	endif()

	# definitions
	add_definitions(
		-DWITH_OPENSSL=$<BOOL:${WITH_OPENSSL}>
		-DUSE_LIBZIP=$<BOOL:${USE_LIBZIP}>
	)

	if(WITH_OPENSSL)
		pkg_check_modules(xmlsec xmlsec1-openssl)
	else()
		pkg_check_modules(xmlsec xmlsec1-gnutls)
	endif()
	add_library(wgtpkg STATIC
		wgtpkg-certs.c
		wgtpkg-digsig.c
		wgtpkg-files.c
		wgtpkg-install.c
		wgtpkg-permissions.c
		wgtpkg-uninstall.c
		wgtpkg-unit.c
		wgtpkg-workdir.c
		wgtpkg-xmlsec.c
		wgtpkg-zip.c
		)

	target_compile_options(wgtpkg PUBLIC ${libxml_CFLAGS} ${libjsonc_CFLAGS} ${xmlsec_CFLAGS})
	target_include_directories(wgtpkg PUBLIC ${libxml_INCLUDE_DIRS} ${libjsonc_INCLUDE_DIRS} ${xmlsec_INCLUDE_DIRS})
	target_link_libraries(wgtpkg PUBLIC wgt ${libjsonc_LIBRARIES} ${libxml_LIBRARIES} ${xmlsec_LIBRARIES})
	target_link_directories(wgtpkg PUBLIC ${libjsonc_LIBRARY_DIRS} ${libxml_LIBRARY_DIRS} ${xmlsec_LIBRARY_DIRS})

	if(USE_LIBZIP)
		target_compile_options(wgtpkg PRIVATE ${libzip_CFLAGS})
		target_include_directories(wgtpkg PRIVATE ${libzip_INCLUDE_DIRS})
		target_link_libraries(wgtpkg PUBLIC ${libzip_LIBRARIES})
		target_link_directories(wgtpkg PUBLIC ${libzip_LIBRARY_DIRS})
	endif(USE_LIBZIP)

	if(NOT ONLY_SDK)
		MESSAGE(STATUS "Creating legacy installer for legacy widgets")
		add_executable(wgtpkg-install main-wgtpkg-install.c)
		target_link_libraries(wgtpkg-install wgtpkg wgt secwrp utils)
		install(TARGETS wgtpkg-install DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()

	if(WITH_TOOLS AND WITH_LEGACY_WGTPKG)
		MESSAGE(STATUS "Creating legacy tools for legacy widgets")

		add_executable(wgtpkg-sign main-wgtpkg-sign.c)
		target_link_libraries(wgtpkg-sign wgtpkg utils)
		install(TARGETS wgtpkg-sign DESTINATION ${CMAKE_INSTALL_BINDIR})

		add_executable(wgtpkg-pack main-wgtpkg-pack.c)
		target_link_libraries(wgtpkg-pack wgtpkg utils)
		install(TARGETS wgtpkg-pack DESTINATION ${CMAKE_INSTALL_BINDIR})

		add_executable(wgtpkg-info main-wgtpkg-info.c)
		target_link_libraries(wgtpkg-info wgtpkg wgt utils)
		install(TARGETS wgtpkg-info DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()

add_subdirectory(test-unit)
