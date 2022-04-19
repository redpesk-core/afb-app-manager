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

###########################################################################

link_libraries(-Wl,--as-needed -Wl,--gc-sections)

add_compile_options(-Wall -Wextra -Wconversion)
add_compile_options(-Wno-unused-parameter) # frankly not using a parameter does it care?
add_compile_options(-Werror=maybe-uninitialized)
add_compile_options(-Werror=implicit-function-declaration)
add_compile_options(-Wno-pointer-sign) # for XmlChar handling
add_compile_options(-ffunction-sections -fdata-sections)
add_compile_options(-Wl,--as-needed -Wl,--gc-sections)
add_compile_options(-fPIC)
#add_definitions(-DNDEBUG)

set(CMAKE_C_FLAGS_PROFILING    "-g -O0 -pg -Wp,-U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_DEBUG        "-g -O0 -ggdb -Wp,-U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_RELEASE      "-g -O2")
set(CMAKE_C_FLAGS_CCOV         "-g -O2 --coverage")

###########################################################################

find_package(PkgConfig REQUIRED)

if(WITH_OPENSSL)
	set(xsec openssl xmlsec1-openssl)
else(WITH_OPENSSL)
	set(xsec gnutls xmlsec1-gnutls)
endif(WITH_OPENSSL)

pkg_check_modules(EXTRAS REQUIRED libxml-2.0 ${xsec} json-c librp-utils>=0.0.1)
add_compile_options(${EXTRAS_CFLAGS})
include_directories(${EXTRAS_INCLUDE_DIRS})
link_libraries(${EXTRAS_LIBRARIES})
link_directories(${EXTRAS_LIBRARY_DIRS})

if(WITH_WIDGETS)
	pkg_check_modules(libzip libzip>=0.11)
	if(libzip_FOUND AND USE_LIBZIP)
		add_compile_options(${libzip_CFLAGS})
		include_directories(${libzip_INCLUDE_DIRS})
		link_libraries(${libzip_LIBRARIES})
		link_directories(${libzip_LIBRARY_DIRS})
		add_definitions(-DUSE_LIBZIP=1)
	else()
		add_definitions(-DUSE_LIBZIP=0)
	endif()
else(WITH_WIDGETS)
	set(USE_LIBZIP OFF)
	add_definitions(-DUSE_LIBZIP=0)
endif(WITH_WIDGETS)

pkg_check_modules(libsystemd libsystemd>=222)
if(libsystemd_FOUND)
	add_compile_options(${libsystemd_CFLAGS})
	include_directories(${libsystemd_INCLUDE_DIRS})
	link_libraries(${libsystemd_LIBRARIES})
	link_directories(${libsystemd_LIBRARY_DIRS})
else()
	add_definitions(-DNO_LIBSYSTEMD)
endif()

pkg_check_modules(AFB afb-binding>=4)
if(AFB_FOUND)
	add_compile_options(${AFB_CFLAGS})
	include_directories(${AFB_INCLUDE_DIRS})
	link_libraries(${AFB_LIBRARIES})
	link_directories(${AFB_LIBRARY_DIRS})
endif()

###########################################################################

if(SIMULATE_SECMGR)
	add_definitions(-DSIMULATE_SEC_LSM_MANAGER=1)
else(SIMULATE_SECMGR)
	pkg_check_modules(SECMGR REQUIRED sec-lsm-manager)
	add_compile_options(${SECMGR_CFLAGS})
	include_directories(${SECMGR_INCLUDE_DIRS})
	link_libraries(${SECMGR_LIBRARIES})
	link_directories(${SECMGR_LIBRARY_DIRS})
	add_definitions(-DSIMULATE_SEC_LSM_MANAGER=0)
endif(SIMULATE_SECMGR)

###########################################################################

set(wgtpkg_sources
	wgtpkg-certs.c
	wgtpkg-digsig.c
	wgtpkg-files.c
	wgtpkg-install.c
	wgtpkg-mustach.c
	wgtpkg-permissions.c
	wgtpkg-uninstall.c
	wgtpkg-unit.c
	wgtpkg-workdir.c
	wgtpkg-xmlsec.c
	)

if(WITH_WIDGETS)
	set(wgtpkg_sources ${wgtpkg_sources} wgtpkg-zip.c)
endif(WITH_WIDGETS)

add_library(wgtpkg STATIC ${wgtpkg_sources})

set(afmpkg_sources
	afmpkg.c
	)
add_library(afmpkg STATIC ${afmpkg_sources})

add_library(utils STATIC
	detect-packtype.c
	manage-afid.c
	manifest.c
	mime-type.c
	mustach.c
	normalize-unit-file.c
	path-entry.c
	path-type.c
	permset.c
	sighup-framework.c
	utils-dir.c
	utils-json.c
	utils-systemd.c
	)

# library for handling the content of the widgets
add_library(wgt STATIC
	wgt-config.c
	wgt-info.c
	wgt-strings.c
	wgt-json.c
	wgt.c
	)

add_library(secwrp STATIC
	secmgr-wrap.c
	)

add_library(afm STATIC
	afm-udb.c
	afm-urun.c
	)

###########################################################################
# off line tools tools

if(WITH_TOOLS)
	MESSAGE(STATUS "Creating packaging tools")

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

	add_executable(afmpkg-status main-afmpkg-status.c)
	install(TARGETS afmpkg-status DESTINATION ${CMAKE_INSTALL_BINDIR})

else()
	MESSAGE(STATUS "Not creating packaging tools")
endif()

add_executable(afmpkg-daemon main-afmpkg-daemon.c)
target_link_libraries(afmpkg-daemon afmpkg wgtpkg wgt secwrp utils pthread)
install(TARGETS afmpkg-daemon DESTINATION ${CMAKE_INSTALL_BINDIR})

###########################################################################
# dynamic tool daemons

if(libsystemd_FOUND AND AFB_FOUND)
	MESSAGE(STATUS "Creating daemons")

	add_library(afm-binding MODULE afm-binding.c)
	target_link_libraries(afm-binding wgtpkg wgt secwrp afm utils)
	set_target_properties(afm-binding PROPERTIES
		PREFIX ""
		LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/afm-binding.export-map"
	)
	install(TARGETS afm-binding LIBRARY DESTINATION ${afm_libexecdir})

	add_executable(afm-user-session afm-user-session.c)
	install(TARGETS afm-user-session DESTINATION ${afm_libexecdir}
			PERMISSIONS SETUID OWNER_READ OWNER_WRITE OWNER_EXECUTE
					GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
else()
	MESSAGE(STATUS "Not creating daemons")
endif()

###########################################################################
# the tests

add_subdirectory(tests)

###########################################################################
# rpm plugin

if(WITH_RPM_REDPESK_PLUGIN)
	MESSAGE(STATUS "Creating rpm plugin for redpesk")

	pkg_check_modules(RPM REQUIRED IMPORTED_TARGET rpm)

	add_library(redpesk SHARED rpm-redpesk.c)
	set_target_properties(redpesk PROPERTIES PREFIX "")

	target_link_directories(redpesk PUBLIC ${RPM_LIBRARY_DIRS})
	target_link_libraries(redpesk LINK_PUBLIC ${RPM_LIBRARIES} wgtpkg wgt secwrp utils afm)
	install(TARGETS redpesk LIBRARY DESTINATION ${rpm_plugin_dir})
else()
	MESSAGE(STATUS "Not creating rpm plugin for redpesk")
endif()

###########################################################################
# certification tools

add_subdirectory(cert)
