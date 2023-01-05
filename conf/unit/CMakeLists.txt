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

cmake_minimum_required(VERSION 3.10)

find_program(M4EXEC m4)
mark_as_advanced(M4EXEC)

if(AGL_DEVEL)
	set(optm4 -DAGL_DEVEL)
endif()

set(m4defs
	-DAFM_PERM_PREFIX=${afm_prefix}
	-DAFM_USERS_RUNDIR=${afm_users_rundir}
	-DAFM_PLATFORM_RUNDIR=${afm_platform_rundir}
	-DAFM_SCOPE_PLATFORM_HOMEDIR=${afm_scope_platform_dir}
	-DAFM_CONFDIR=${afm_confdir}
)

set(m4sources
	afm-unit
	provided-api-platform.inc
	provided-binding-platform.inc
	service.inc
	binder.inc
	macros.inc
	provided-api-user.inc
	provided-binding-user.inc
)

add_custom_command(
	OUTPUT afm-unit.conf
	COMMAND ${M4EXEC} ${optm4} ${m4defs} -I${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/afm-unit > afm-unit.conf
	DEPENDS ${m4sources}
)
add_custom_target(unitconf ALL DEPENDS afm-unit.conf)

if(NOT USE_SDK)
	install(DIRECTORY DESTINATION ${afm_confdir}/unit.env.d)
	install(DIRECTORY DESTINATION ${afm_confdir}/widget.env.d)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/afm-unit.conf DESTINATION ${afm_confdir})
endif()

