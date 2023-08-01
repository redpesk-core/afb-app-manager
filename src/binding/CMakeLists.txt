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

pkg_check_modules(AFB afb-binding>=4)
if(libsystemd_FOUND AND AFB_FOUND)
	MESSAGE(STATUS "Creating afm-binding")

	add_library(
		afm-binding
		MODULE
		afm-binding.c
		afm-udb.c
		afm-urun.c
	)
	target_compile_options(
		afm-binding
		PRIVATE
		${AFB_CFLAGS}
	)
	target_include_directories(
		afm-binding
		PRIVATE
		${AFB_INCLUDE_DIRS}
	)
	target_link_libraries(
		afm-binding
		secwrp
		utils
		${AFB_LIBRARIES}
	)
	target_link_directories(
		afm-binding
		PRIVATE
		${AFB_LIBRARY_DIRS}
	)
	set_target_properties(
		afm-binding
		PROPERTIES
		PREFIX ""
		LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/afm-binding.export-map"
	)
	install(
		TARGETS afm-binding
		LIBRARY
		DESTINATION ${afm_libexecdir}
	)
else()
	MESSAGE(STATUS "Not creating afm-binding")
endif()

