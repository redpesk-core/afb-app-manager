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

add_library(afmpkg STATIC afmpkg.c afmpkg-request.c afmpkg-std.c)
target_link_libraries(afmpkg units utils)

if(WITH_LEGACY_AFMPKG)
	target_sources(afmpkg PRIVATE afmpkg-legacy.c)
	target_compile_definitions(afmpkg PRIVATE -DWITH_LEGACY_AFMPK=1)
	target_link_libraries(afmpkg secwrp)
endif()
if(WITH_CONFIG_XML)
	target_compile_definitions(afmpkg PRIVATE -DWITH_CONFIG_XML=1)
	target_link_libraries(afmpkg wgt)
endif()
if(NOT SIMULATE_SECMGR)
	target_include_directories(afmpkg PRIVATE ${SECMGR_INCLUDE_DIRS})
	target_link_libraries(afmpkg ${SECMGR_LIBRARIES})
	target_link_directories(afmpkg PUBLIC ${SECMGR_LIBRARY_DIRS})
endif()
