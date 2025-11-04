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

MESSAGE(STATUS "Creation of afm-create-user, afm-system-setup, afm-user-setup")

add_compile_definitions(AFM_USERS_RUNDIR="${afm_users_rundir}")
add_compile_definitions(AFM_PLATFORM_RUNDIR="${afm_platform_rundir}")
add_compile_definitions(AFM_PLATFORM_DATADIR="${afm_platform_datadir}")
add_compile_definitions(AFM_PLATFORM_USER="${afm_platform_user}")
if(REDPESK_DEVEL)
	add_compile_definitions(WITH_PLATFORM_DEBUG=1)
endif()

add_library(setup-utils OBJECT setup-utils.c)

add_executable(afm-system-setup main-afm-system-setup.c setup-utils)
add_executable(afm-user-setup   main-afm-user-setup.c   setup-utils)

install(
	TARGETS
		afm-system-setup
		afm-user-setup
	DESTINATION
		${afm_libexecdir}
)

