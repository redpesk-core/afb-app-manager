###########################################################################
# Copyright (C) 2015-2025 IoT.bzh Company
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

add_compile_options(-fPIC -ffunction-sections -fdata-sections)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
add_link_options(-Wl,--as-needed -Wl,--gc-sections)

# rputils
pkg_check_modules(rputils REQUIRED
	librp-utils-socket-static>=0.2
	librp-utils-file-static>=0.2
	librp-utils-json-c-static>=0.2
	librp-utils-yaml-static>=0.2
)
add_compile_options(${rputils_CFLAGS})
include_directories(${rputils_INCLUDE_DIRS})
link_libraries(${rputils_LIBRARIES})
link_directories(${rputils_LIBRARY_DIRS})

# check other modules
pkg_check_modules(AFB REQUIRED afb-binding>=4)
pkg_check_modules(gnutls REQUIRED gnutls)
pkg_check_modules(libsystemd libsystemd>=222)
pkg_check_modules(libjsonc REQUIRED json-c)
if(WITH_CONFIG_XML OR WITH_LEGACY_WGTPKG)
	pkg_check_modules(libxml REQUIRED libxml-2.0)
else()
	pkg_check_modules(libxml libxml-2.0)
endif()

# definitions
add_definitions(
	-DFWK_CONFIG_DIR="${afm_confdir}"
	-DFWK_REDPESK_PREFIX="${afm_redpesk_prefix}"
	-DFWK_AGL_PREFIX="${afm_agl_prefix}"
	-DFWK_ICON_DIR="${afm_icondir}"
	-DFWK_APP_DIR="${afm_appdir}"
	-DWGTPKG_TRUSTED_CERT_DIR="${crypto_trusted_certs_dir}"
	-DFWK_LAUNCH_CONF="${afm_confdir}/afm-launch.conf"
	-DFWK_UNIT_CONF="${afm_confdir}/afm-unit.conf"
	-DAFM_UNITS_ROOT="${afm_units_root}"
	-DAFM_VERSION="${PROJECT_VERSION}"
	-DAFMPKG_SOCKET_ADDRESS="${AFMPKG_SOCKET_ADDRESS}"
	-DALLOW_NO_SIGNATURE=$<BOOL:${ALLOW_NO_SIGNATURE}>
	-DDISTINCT_VERSIONS=$<BOOL:${DISTINCT_VERSIONS}>
	-DNO_LIBSYSTEMD=$<BOOL:$<NOT:$<BOOL:${libsystemd_FOUND}>>>
)

###########################################################################

include_directories(. afmpkg cert utils secmgr wgtpkg)

add_subdirectory(afmpkg)
add_subdirectory(binding)
add_subdirectory(cert)
add_subdirectory(main)
add_subdirectory(rpm-plugin)
add_subdirectory(secmgr)
add_subdirectory(tests)
add_subdirectory(utils)
add_subdirectory(mustach)
add_subdirectory(wgtpkg)

