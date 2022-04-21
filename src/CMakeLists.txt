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

add_compile_options(-fPIC)

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

