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

include(FindPkgConfig)

pkg_check_modules(EXTRAS REQUIRED libxml-2.0 openssl xmlsec1 xmlsec1-openssl)
add_compile_options(${EXTRAS_CFLAGS})
include_directories(${EXTRAS_INCLUDE_DIRS})
link_libraries(${EXTRAS_LIBRARIES})

pkg_check_modules(libzip libzip>=0.11)
if(libzip_FOUND AND USE_LIBZIP)
	add_compile_options(${libzip_CFLAGS})
	include_directories(${libzip_INCLUDE_DIRS})
	link_libraries(${libzip_LIBRARIES})
	add_definitions(-DUSE_LIBZIP=1)
else()
	add_definitions(-DUSE_LIBZIP=0)
endif()

###########################################################################

if(SIMULATE_SECMGR)
	add_definitions(-DSIMULATE_SECURITY_MANAGER=1)
else(SIMULATE_SECMGR)
	pkg_check_modules(SECMGR REQUIRED security-manager)
	add_compile_options(${SECMGR_CFLAGS})
	include_directories(${SECMGR_INCLUDE_DIRS})
	link_libraries(${SECMGR_LIBRARIES})
	add_definitions(-DSIMULATE_SECURITY_MANAGER=0)
endif(SIMULATE_SECMGR)

if(SIMULATE_SMACK)
	add_definitions(-DSIMULATE_LIBSMACK=1)
else(SIMULATE_SMACK)
	pkg_check_modules(SMACK REQUIRED libsmack)
	add_compile_options(${SMACK_CFLAGS})
	include_directories(${SMACK_INCLUDE_DIRS})
	link_libraries(${SMACK_LIBRARIES})
	add_definitions(-DSIMULATE_LIBSMACK=0)
endif(SIMULATE_SMACK)

###########################################################################

add_library(wgtpkg STATIC
	wgtpkg-base64.c
	wgtpkg-certs.c
	wgtpkg-digsig.c
	wgtpkg-files.c
	wgtpkg-install.c
	wgtpkg-permissions.c
	wgtpkg-uninstall.c
	wgtpkg-workdir.c
	wgtpkg-xmlsec.c
	wgtpkg-zip.c
	)

add_library(utils STATIC
	utils-dir.c
	verbose.c
	)

add_library(wgt STATIC
	wgt-config.c
	wgt-info.c
	wgt.c
	)

add_library(secwrp STATIC
	secmgr-wrap.c
	)

###########################################################################
# packaging tools

MESSAGE(STATUS "Creating packaging tools")

add_executable(wgtpkg-sign wgtpkg-sign.c)
target_link_libraries(wgtpkg-sign wgtpkg utils)

add_executable(wgtpkg-pack wgtpkg-pack.c)
target_link_libraries(wgtpkg-pack wgtpkg utils)

add_executable(wgtpkg-info wgtpkg-info.c)
target_link_libraries(wgtpkg-info wgtpkg wgt utils)

add_executable(wgtpkg-installer wgtpkg-installer.c)
target_link_libraries(wgtpkg-installer wgtpkg wgt secwrp utils)

install(TARGETS wgtpkg-sign wgtpkg-pack wgtpkg-info wgtpkg-installer DESTINATION ${CMAKE_INSTALL_BINDIR})

###########################################################################
# the targeted

pkg_check_modules(EXTRA2 json-c libsystemd)
if(EXTRA2_FOUND)
	add_compile_options(${EXTRA2_CFLAGS})
	include_directories(${EXTRA2_INCLUDE_DIRS})
	link_libraries(${EXTRA2_LIBRARIES})

	add_library(utils2 STATIC
		utils-jbus.c
		utils-json.c
		)

	add_library(afm STATIC
		afm-db.c
		afm-launch.c
		afm-launch-mode.c
		afm-run.c
		)

	###########################################################################
	# the daemons
	MESSAGE(STATUS "Creating daemons")

	add_executable(afm-user-daemon afm-user-daemon.c)
	target_link_libraries(afm-user-daemon afm secwrp wgt utils utils2)
	install(TARGETS afm-user-daemon DESTINATION ${CMAKE_INSTALL_BINDIR})

	add_executable(afm-system-daemon afm-system-daemon.c)
	target_link_libraries(afm-system-daemon wgtpkg afm secwrp wgt utils utils2)
	install(TARGETS afm-system-daemon DESTINATION ${CMAKE_INSTALL_BINDIR})

	###########################################################################
	# the binding for afb

	pkg_check_modules(afb afb-daemon)
	if(afb_FOUND)
		message(STATUS "Creation afm-main-binding for AFB-DAEMON")
		###############################################################
		#pkg_get_variable(afb_binding_install_dir afb-daemon binding_install_dir)
		execute_process(
			COMMAND pkg-config --variable binding_install_dir afb-daemon
			OUTPUT_VARIABLE afb_binding_install_dir OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		###############################################################
		add_library(afm-main-binding MODULE afm-main-binding.c)
		target_compile_options(afm-main-binding PRIVATE ${afb_CFLAGS})
		target_include_directories(afm-main-binding PRIVATE ${afb_INCLUDE_DIRS})
		target_link_libraries(afm-main-binding utils utils2 ${afb_LIBRARIES})
		set_target_properties(afm-main-binding PROPERTIES
			PREFIX ""
			LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/afm-main-binding.export-map"
		)
		install(TARGETS afm-main-binding LIBRARY DESTINATION ${afb_binding_install_dir})
	else()
		message(STATUS "Not creating the binding for AFB-DAEMON")
	endif()

endif(EXTRA2_FOUND)
