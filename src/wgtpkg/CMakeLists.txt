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

add_library(wgtpkg STATIC
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

target_compile_options(wgtpkg PRIVATE ${libxml_CFLAGS} ${libjsonc_CFLAGS} ${xmlsec_CFLAGS})
target_include_directories(wgtpkg PRIVATE ${libxml_INCLUDE_DIRS} ${libjsonc_INCLUDE_DIRS} ${xmlsec_INCLUDE_DIRS})
target_link_libraries(wgtpkg PUBLIC ${libjsonc_LIBRARIES} ${libxml_LIBRARIES} ${xmlsec_LIBRARIES})
target_link_directories(wgtpkg PUBLIC ${libjsonc_LIBRARY_DIRS} ${libxml_LIBRARY_DIRS} ${xmlsec_LIBRARY_DIRS})

if(USE_LIBZIP)
	target_sources(wgtpkg PRIVATE wgtpkg-zip.c)
	target_compile_options(wgtpkg PRIVATE ${libzip_CFLAGS})
	target_include_directories(wgtpkg PRIVATE ${libzip_INCLUDE_DIRS})
	target_link_libraries(wgtpkg PUBLIC ${libzip_LIBRARIES})
	target_link_directories(wgtpkg PUBLIC ${libzip_LIBRARY_DIRS})
endif(USE_LIBZIP)

# library for handling the content of the widgets
add_library(wgt STATIC
	wgt-config.c
	wgt-info.c
	wgt-strings.c
	wgt-json.c
	wgt.c
	)

target_compile_options(wgt PRIVATE ${libxml_CFLAGS} ${libjsonc_CFLAGS})
target_include_directories(wgt PRIVATE ${libxml_INCLUDE_DIRS} ${libjsonc_INCLUDE_DIRS})
target_link_libraries(wgt PUBLIC ${libjsonc_LIBRARIES} ${libxml_LIBRARIES})
target_link_directories(wgt PUBLIC ${libjsonc_LIBRARY_DIRS} ${libxml_LIBRARY_DIRS})
