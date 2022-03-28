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

project(afb-app-manager C)

include(GNUInstallDirs)
include(CTest)

set(PROJECT_NAME "AFM Main")
set(PROJECT_PRETTY_NAME "Application Framework Main")
set(PROJECT_DESCRIPTION "Secured Application framework")
set(PROJECT_VERSION 10.0.1)
set(PROJECT_URL "https://github.com/redpesk-core/afb-app-manager")

option(DISTINCT_VERSIONS  "Should the version of application be distinguished" OFF)
option(USE_LIBZIP         "Should try to use libzip?" ON)
option(USE_SIMULATION     "If set simulates sec lsm manager" OFF)
option(USE_SDK            "If set, avoids installating system runtime files" OFF)
option(ALLOW_NO_SIGNATURE "If set, widgets without signature are accepted" OFF)
option(INSTALL_SAMPLE_KEYS "Install the sample keys and certificates" OFF)
option(WITH_RPM_REDPESK_PLUGIN "Compile and install the rpm plugin for redpesk" ON)
option(WITH_TOOLS         "Compile and install the tools" ON)
option(ALLOW_WIDGETS      "Allow widgets even if WITH_RPM_REDPESK_PLUGIN is ON" OFF)
option(WITH_OPENSSL       "Use openssl instead of gnutls" ON)

option(SIMULATE_SECMGR    "If set, the sec lsm manager is simulated" OFF)

set(afm_name                "afm" CACHE STRING "Name for application framework user")
set(afm_confdir             "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${afm_name}" CACHE STRING "Directory for configuration files")
set(afm_datadir             "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}" CACHE STRING "Root directory for installions")
set(afm_libexecdir          "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${afm_name}" CACHE STRING "Directory for installing internal binaries")
set(afm_appdir              "${afm_datadir}/applications" CACHE STRING "Directory for installing applications")
set(afm_icondir             "${afm_datadir}/icons" CACHE STRING "Directory for installing icons")
set(afm_prefix              "urn:AGL:" CACHE STRING "Prefix for uniform resource name")
set(systemd_units_root      "${CMAKE_INSTALL_FULL_LIBDIR}/systemd" CACHE STRING "Place where unit files are to be set")
set(wgtpkg_trusted_certs_dir "${afm_confdir}/certs" CACHE STRING "Path to internal certificates")
set(wgtpkg_sample_keys_dir  "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}/keys" CACHE STRING "Path to internal keys")
set(wgtpkg_sample_certs_dir "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}/certs" CACHE STRING "Path to internal certs")
set(afm_platform_rundir     "/run/platform" CACHE STRING "Path to location of platform runtime sockets")
set(afm_users_rundir        "/run/user" CACHE STRING "Path to location of users runtime sockets")
set(afm_scope_platform_dir  "/var/scope-platform" CACHE STRING "Path to home of scope-platform apps")
set(rpm_plugin_dir          "${CMAKE_INSTALL_FULL_LIBDIR}/rpm-plugins" CACHE STRING "Path to rpm plugins")

if(USE_SIMULATION)
    set(SIMULATE_SECMGR ON)
endif(USE_SIMULATION)

if(WITH_RPM_REDPESK_PLUGIN AND NOT ALLOW_WIDGETS)
    set(WITH_TOOLS OFF)
    set(WITH_WIDGETS OFF)
    add_definitions(-DWITH_WIDGETS=0)
else(WITH_RPM_REDPESK_PLUGIN AND NOT ALLOW_WIDGETS)
    set(WITH_WIDGETS ON)
    add_definitions(-DWITH_WIDGETS=1)
endif(WITH_RPM_REDPESK_PLUGIN AND NOT ALLOW_WIDGETS)

add_definitions(
	-DFWK_CONFIG_DIR="${afm_confdir}"
	-DFWK_PREFIX="${afm_prefix}"
	-DFWK_ICON_DIR="${afm_icondir}"
	-DFWK_APP_DIR="${afm_appdir}"
	-DWGTPKG_TRUSTED_CERT_DIR="${wgtpkg_trusted_certs_dir}"
	-DFWK_LAUNCH_CONF="${afm_confdir}/afm-launch.conf"
	-DFWK_UNIT_CONF="${afm_confdir}/afm-unit.conf"
	-DSYSTEMD_UNITS_ROOT="${systemd_units_root}"
	-DAFM_VERSION="${PROJECT_VERSION}"
)
if(ALLOW_NO_SIGNATURE)
	add_definitions(-DALLOW_NO_SIGNATURE=1)
endif(ALLOW_NO_SIGNATURE)
if(DISTINCT_VERSIONS)
	add_definitions(-DDISTINCT_VERSIONS=1)
else(DISTINCT_VERSIONS)
	add_definitions(-DDISTINCT_VERSIONS=0)
endif(DISTINCT_VERSIONS)
if(INSTALL_SAMPLE_KEYS)
	add_definitions(-DWITH_SAMPLE_KEYS=1)
endif(INSTALL_SAMPLE_KEYS)
if(WITH_OPENSSL)
	add_definitions(-DWITH_OPENSSL=1)
endif(WITH_OPENSSL)


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


