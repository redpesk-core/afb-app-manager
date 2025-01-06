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

if(WITH_RPM_REDPESK_PLUGIN AND NOT ONLY_SDK)

	MESSAGE(STATUS "Creating rpm plugin for redpesk")

	pkg_check_modules(RPM REQUIRED IMPORTED_TARGET rpm)

	add_library(redpesk SHARED redpesk.c)
	set_target_properties(redpesk PROPERTIES PREFIX "")

	target_include_directories(redpesk PRIVATE ${CMAKE_SOURCE_DIR}/external-includes)
	target_link_directories(redpesk PUBLIC ${RPM_LIBRARY_DIRS})
	target_link_libraries(redpesk LINK_PUBLIC ${RPM_LIBRARIES})
	install(TARGETS redpesk LIBRARY DESTINATION ${rpm_plugin_dir})
	install(FILES macros.afm-main DESTINATION ${rpm_macros_dir})
else()
	MESSAGE(STATUS "Not creating rpm plugin for redpesk")
endif()

