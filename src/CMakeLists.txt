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

include(FindPkgConfig)

pkg_check_modules(EXTRAS REQUIRED
	libzip>=0.11
	libxml-2.0
	openssl
	xmlsec1 xmlsec1-openssl
	json-c
	dbus-1
	)

add_compile_options(${EXTRAS_CFLAGS})
include_directories(${EXTRAS_INCLUDE_DIRS})
link_libraries(${EXTRAS_LIBRARIES})

###########################################################################

include_directories(simulation)

###########################################################################

add_compile_options(-Wall -Wno-pointer-sign)
add_compile_options(-ffunction-sections -fdata-sections)
add_compile_options(-fPIC)
add_compile_options(-Wl,--gc-sections)

set(CMAKE_C_FLAGS_PROFILING    "-g -O0 -pg -Wp,-U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_DEBUG        "-g -O0 -ggdb -Wp,-U_FORTIFY_SOURCE")
set(CMAKE_C_FLAGS_RELEASE      "-g -O2")
set(CMAKE_C_FLAGS_CCOV         "-g -O2 --coverage")

###########################################################################

add_library(wgtpkg
	wgtpkg-base64.c
	wgtpkg-certs.c
	wgtpkg-digsig.c
	wgtpkg-files.c
	wgtpkg-install.c
	wgtpkg-permissions.c
	wgtpkg-workdir.c
	wgtpkg-xmlsec.c
	wgtpkg-zip.c
	)

add_library(utils
	utils-dir.c
	utils-jbus.c
	verbose.c
	)

add_library(wgt
	wgt-config.c
	wgt-info.c
	wgt.c
	)

add_library(secwrp
	secmgr-wrap.c
	)

add_library(afm
	af-db.c
	af-launch.c
	af-run.c
	)

add_executable(wgtpkg-sign wgtpkg-sign.c)
target_link_libraries(wgtpkg-sign wgtpkg utils)


add_executable(wgtpkg-pack wgtpkg-pack.c)
target_link_libraries(wgtpkg-pack wgtpkg utils)


add_executable(wgtpkg-info wgtpkg-info.c)
target_link_libraries(wgtpkg-info wgtpkg wgt utils)


add_executable(wgtpkg-installer wgtpkg-installer.c)
target_link_libraries(wgtpkg-installer wgtpkg wgt secwrp utils)


add_executable(af-usrd af-usrd.c)
target_link_libraries(af-usrd afm secwrp wgt utils)

install(TARGETS wgtpkg-sign wgtpkg-pack wgtpkg-info wgtpkg-installer DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
install(TARGETS af-usrd DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

