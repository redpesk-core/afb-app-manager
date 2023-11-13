###########################################################################
# Copyright (C) 2015-2023 IoT.bzh Company
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

# library for handling the content of the widgets

if(libxml_FOUND)
	add_library(wgt STATIC
		wgt-config.c
		wgt-info.c
		wgt-strings.c
		wgt-json.c
		wgt.c
		)

	target_compile_options(wgt PUBLIC ${libxml_CFLAGS} ${libjsonc_CFLAGS})
	target_include_directories(wgt PUBLIC ${libxml_INCLUDE_DIRS} ${libjsonc_INCLUDE_DIRS})
	target_link_libraries(wgt PUBLIC ${libjsonc_LIBRARIES} ${libxml_LIBRARIES} utils)
	target_link_directories(wgt PUBLIC ${libjsonc_LIBRARY_DIRS} ${libxml_LIBRARY_DIRS})
endif(libxml_FOUND)

add_subdirectory(legacy)
