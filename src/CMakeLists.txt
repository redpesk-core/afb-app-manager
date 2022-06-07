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

add_compile_options(-fPIC -ffunction-sections -fdata-sections)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)
add_link_options(-Wl,--as-needed -Wl,--gc-sections)

# rputils
pkg_check_modules(rputils REQUIRED librp-utils>=0.0.1)
add_compile_options(${rputils_CFLAGS})
include_directories(${rputils_INCLUDE_DIRS})
link_libraries(${rputils_LIBRARIES})
link_directories(${rputils_LIBRARY_DIRS})

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

# check other modules
pkg_check_modules(gnutls gnutls)
pkg_check_modules(libsystemd libsystemd>=222)
pkg_check_modules(libxml libxml-2.0)
pkg_check_modules(libjsonc json-c)
if(WITH_OPENSSL)
	pkg_check_modules(xmlsec xmlsec1-openssl)
else()
	pkg_check_modules(xmlsec xmlsec1-gnutls)
endif()

# definitions
add_definitions(
	-DFWK_CONFIG_DIR="${afm_confdir}"
	-DFWK_PREFIX="${afm_prefix}"
	-DFWK_ICON_DIR="${afm_icondir}"
	-DFWK_APP_DIR="${afm_appdir}"
	-DWGTPKG_TRUSTED_CERT_DIR="${wgtpkg_trusted_certs_dir}"
	-DFWK_LAUNCH_CONF="${afm_confdir}/afm-launch.conf"
	-DFWK_UNIT_CONF="${afm_confdir}/afm-unit.conf"
	-DAFM_UNITS_ROOT="${afm_units_root}"
	-DAFM_VERSION="${PROJECT_VERSION}"
	-DAFMPKG_SOCKET_ADDRESS="${AFMPKG_SOCKET_ADDRESS}"
	-DWITH_WIDGETS=$<BOOL:${WITH_WIDGETS}>
	-DALLOW_NO_SIGNATURE=$<BOOL:${ALLOW_NO_SIGNATURE}>
	-DDISTINCT_VERSIONS=$<BOOL:${DISTINCT_VERSIONS}>
	-DWITH_OPENSSL=$<BOOL:${WITH_OPENSSL}>
	-DUSE_LIBZIP=$<BOOL:${USE_LIBZIP}>
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
add_subdirectory(wgtpkg)

