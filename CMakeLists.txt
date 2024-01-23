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

cmake_minimum_required(VERSION 3.15)

project(afb-app-manager
        VERSION 12.2.5
        DESCRIPTION "Secured Application framework, Daemons, Tools, Configuratioon"
        HOMEPAGE_URL "https://github.com/redpesk-core/afb-app-manager"
        LANGUAGES C)

include(FindPkgConfig)
include(GNUInstallDirs)
include(CTest)

############################################################
# options and settings
option(ONLY_SDK           "If set, avoids installating system runtime files" OFF)
option(WITH_RPM_REDPESK_PLUGIN "Compile and install the rpm plugin for redpesk" ON)
option(WITH_TOOLS         "Compile and install the tools" ON)
option(REDPESK_DEVEL      "Add developement features" OFF)
option(WITH_CONFIG_XML    "Allow use of legacy config.xml instead of manifest" ON)
option(SIMULATE_SECMGR    "If set, the sec lsm manager is simulated" OFF)
option(ALLOW_NO_SIGNATURE "If set, widgets without signature are accepted" OFF)
option(INSTALL_SAMPLE_KEYS "Install the sample keys and certificates" OFF)

set(afm_name                "afm"                                          CACHE STRING "Name for application framework user")
set(afm_confdir             "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${afm_name}" CACHE STRING "Directory for configuration files")
set(afm_datadir             "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}"    CACHE STRING "Root directory for installions")
set(afm_libexecdir          "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${afm_name}" CACHE STRING "Directory for installing internal binaries")
set(afm_contentdir          "${CMAKE_INSTALL_PREFIX}/redpesk"              CACHE STRING "Directory for managed content")
set(afm_appdir              "${afm_contentdir}"                            CACHE STRING "Directory for installing applications")
set(afm_icondir             "${afm_contentdir}/${afm_name}/icons"          CACHE STRING "Directory for installing icons")
set(afm_prefix              "urn:AGL:"                                     CACHE STRING "Prefix for uniform resource name")
set(afm_units_root          "${afm_datadir}/systemd"                       CACHE STRING "Place where unit files are to be set")
set(crypto_trusted_certs_dir "${afm_confdir}/certs"                        CACHE STRING "Path to internal certificates")
set(crypto_sample_keys_dir  "${afm_datadir}/keys"                          CACHE STRING "Path to internal keys")
set(crypto_sample_certs_dir "${afm_datadir}/certs"                         CACHE STRING "Path to internal certs")
set(afm_platform_rundir     "/run/platform"                                CACHE STRING "Path to location of platform runtime sockets")
set(afm_users_rundir        "/run/user"                                    CACHE STRING "Path to location of users runtime sockets")
set(afm_scope_platform_dir  "/var/scope-platform"                          CACHE STRING "Path to home of scope-platform apps")
set(rpm_plugin_dir          "${CMAKE_INSTALL_FULL_LIBDIR}/rpm-plugins"     CACHE STRING "Path to rpm plugins")
set(rpm_macros_dir          "${CMAKE_INSTALL_PREFIX}/lib/rpm/macros.d"     CACHE STRING "Path to rpm macro files")
set(AFMPKG_SOCKET_ADDRESS   "@afmpkg-installer.socket"                     CACHE STRING "specification of afmpkg installer socket")
set(SYSCONFDIR_DBUS_SYSTEM  "${CMAKE_INSTALL_FULL_SYSCONFDIR}/dbus-1/system.d"  CACHE STRING "Path to dbus system configuration files")
set(SYSCONFDIR_PAMD         "${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d"       CACHE STRING "Path to pam configuration files")
set(UNITDIR_SYSTEM          "${CMAKE_INSTALL_PREFIX}/lib/systemd/system"   CACHE STRING "Path to systemd system unit files")

############################################################
# legacy options
option(WITH_LEGACY_WGTPKG "Supports legacy widget" OFF)
option(WITH_OPENSSL       "Use openssl instead of gnutls" OFF)
option(USE_LIBZIP         "Should try to use libzip?" ON)
option(DISTINCT_VERSIONS  "Should the version of application be distinguished" OFF)
option(WITH_LEGACY_AFM_UTIL "Really want the really legacy afm-util" OFF)
option(WITH_LEGACY_MIGRATION "Installs the legacy migration scripts" OFF)

############################################################
# content
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

