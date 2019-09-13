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

cmake_minimum_required(VERSION 3.4.3)

project(afm-main C)

include(GNUInstallDirs)
include(CTest)

set(PROJECT_NAME "AFM Main")
set(PROJECT_PRETTY_NAME "Application Framework Main")
set(PROJECT_DESCRIPTION "Secured Application framework for Automotive Grade Linux")
set(AGLVERSION "8.0.0" CACHE STRING "Version of AGL")
set(PROJECT_VERSION ${AGLVERSION} CACHE STRING "Version of the project can override AGLVERSION")
set(PROJECT_URL "https://gerrit.automotivelinux.org/gerrit/gitweb?p=src/app-framework-main.git;a=summary")

set(USE_LIBZIP     ON  CACHE BOOL "should try to use libzip?")
set(USE_SIMULATION OFF CACHE BOOL "if set simulates security manager and smack")
set(USE_SDK        OFF CACHE BOOL "if set, avoids installating system runtime files")
set(ALLOW_NO_SIGNATURE OFF CACHE BOOL "if set, widgets without signature are accepted")

set(SIMULATE_SECMGR OFF CACHE BOOL "if set, the security manager is simulated")
set(SIMULATE_SMACK  OFF CACHE BOOL "if set, the smack environment is simulated")

set(LEGACY_USER_DAEMON OFF CACHE BOOL "compile and install the legacy afm-user-daemon")

set(afm_name                "afm" CACHE STRING "Name for application framework user")
set(afm_confdir             "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${afm_name}" CACHE STRING "Directory for configuration files")
set(afm_datadir             "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}" CACHE STRING "Root directory for installions")
set(afm_libexecdir          "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${afm_name}" CACHE STRING "Directory for installing internal binaries")
set(afm_appdir              "${afm_datadir}/applications" CACHE STRING "Directory for installing applications")
set(afm_icondir             "${afm_datadir}/icons" CACHE STRING "Directory for installing icons")
set(afm_prefix              "urn:AGL:" CACHE STRING "Prefix for uniform resource name")
set(afm_user_appdir         "app-data" CACHE STRING "User subdirectory for applications")
set(afm_user_appdir_label   "User::App-Shared" CACHE STRING "Smack label of the user subdirectory for applications")
set(systemd_units_root      "${CMAKE_INSTALL_FULL_LIBDIR}/systemd" CACHE STRING "Place where unit files are to be set")
set(wgtpkg_trusted_cert_dir "${afm_confdir}/certs" CACHE STRING "Path to internal certificates")
set(afm_platform_rundir     "/run/platform" CACHE STRING "Path to location of platform runtime sockets")
set(afm_users_rundir        "/run/user" CACHE STRING "Path to location of users runtime sockets")

if(USE_SIMULATION)
    set(SIMULATE_SECMGR ON)
    set(SIMULATE_SMACK  ON)
endif(USE_SIMULATION)

add_definitions(
	-DFWK_CONFIG_DIR="${afm_confdir}"
	-DFWK_PREFIX="${afm_prefix}"
	-DFWK_ICON_DIR="${afm_icondir}"
	-DFWK_APP_DIR="${afm_appdir}"
	-DFWK_USER_APP_DIR="${afm_user_appdir}"
	-DWGTPKG_TRUSTED_CERT_DIR="${wgtpkg_trusted_cert_dir}"
	-DFWK_LAUNCH_CONF="${afm_confdir}/afm-launch.conf"
	-DFWK_UNIT_CONF="${afm_confdir}/afm-unit.conf"
	-DFWK_USER_APP_DIR_LABEL="${afm_user_appdir_label}"
	-DSYSTEMD_UNITS_ROOT="${systemd_units_root}"
	-DAFM_VERSION="${PROJECT_VERSION}"
)
if(ALLOW_NO_SIGNATURE)
	add_definitions(-DDEFAULT_ALLOW_NO_SIGNATURE=1)
else(ALLOW_NO_SIGNATURE)
	add_definitions(-DDEFAULT_ALLOW_NO_SIGNATURE=0)
endif(ALLOW_NO_SIGNATURE)


add_subdirectory(src)
add_subdirectory(conf)
add_subdirectory(scripts)
add_subdirectory(certs)

############################################################
# installs the pkgconfig files

CONFIGURE_FILE(afm-main.pc.in afm-main.pc @ONLY)
INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/afm-main.pc
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )


