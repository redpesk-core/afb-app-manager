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

if(libsystemd_FOUND AND AFB_FOUND AND NOT ONLY_SDK)
	MESSAGE(STATUS "Creating afm-binding")

	pkg_get_variable(VSCRIPT afb-binding version_script)

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
		LINK_FLAGS "-Wl,--version-script=${VSCRIPT}"
	)
	install(
		TARGETS afm-binding
		LIBRARY
		DESTINATION ${afm_libexecdir}
	)
else()
	MESSAGE(STATUS "Not creating afm-binding")
endif()

