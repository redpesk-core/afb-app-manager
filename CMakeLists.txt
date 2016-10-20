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

project(afm-main C)

cmake_minimum_required(VERSION 2.8)

include(GNUInstallDirs)

set(PROJECT_NAME "AFM Main")
set(PROJECT_PRETTY_NAME "Application Framework Main")
set(PROJECT_DESCRIPTION "Secured Application framework")
set(PROJECT_VERSION "1.0")

set(USE_LIBZIP     ON  CACHE BOOL "should try to use libzip?")
set(USE_SIMULATION OFF CACHE BOOL "if set simulates security manager and smack")
set(USE_SDK        OFF CACHE BOOL "if set, avoids installating system runtime files")

set(SIMULATE_SECMGR OFF CACHE BOOL "if set, the security manager is simulated")
set(SIMULATE_SMACK  OFF CACHE BOOL "if set, the smack environment is simulated")

set(afm_name                "afm" CACHE STRING "Name for application framework user")
set(afm_confdir             "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${afm_name}" CACHE STRING "Directory for configuration files")
set(afm_datadir             "${CMAKE_INSTALL_FULL_DATADIR}/${afm_name}" CACHE STRING "Root directory for installions")
set(afm_appdir              "${afm_datadir}/applications" CACHE STRING "Directory for installing applications")
set(afm_icondir             "${afm_datadir}/icons" CACHE STRING "Directory for installing icons")
set(afm_prefix              "urn:AGL:" CACHE STRING "Prefix for uniform resource name")
set(afm_prefix_binding      "${afm_prefix}binding:" CACHE STRING "URN for bindings")
set(afm_user_appdir         "app-data" CACHE STRING "User subdirectory for applications")
set(wgtpkg_trusted_cert_dir "${afm_confdir}/certs" CACHE STRING "Path to internal certificates")

if(USE_SIMULATION)
    set(SIMULATE_SECMGR ON)
    set(SIMULATE_SMACK  ON)
endif(USE_SIMULATION)

add_definitions(
	-DFWK_CONFIG_DIR="${afm_confdir}"
	-DFWK_PREFIX="${afm_prefix}"
	-DFWK_PREFIX_BINDING="${afm_prefix_binding}"
	-DFWK_ICON_DIR="${afm_icondir}"
	-DFWK_APP_DIR="${afm_appdir}"
	-DFWK_USER_APP_DIR="${afm_user_appdir}"
	-DWGTPKG_TRUSTED_CERT_DIR="${wgtpkg_trusted_cert_dir}"
	-DFWK_LAUNCH_CONF="${afm_confdir}/afm-launch.conf"
)

add_subdirectory(src)
add_subdirectory(conf)
add_subdirectory(scripts)
add_subdirectory(certs)


