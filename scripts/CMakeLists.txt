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

option(WITH_LEGACY_AFM_UTIL "Really want the really legacy afm-util" OFF)
if(NOT USE_SDK)
	if(WITH_LEGACY_AFM_UTIL)
		configure_file(afm-util-legacy.in afm-util @ONLY)
	else()
		configure_file(afm-util.in afm-util @ONLY)
	endif()
	install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/afm-util DESTINATION ${CMAKE_INSTALL_BINDIR})
	if(AGL_DEVEL)
		configure_file(afm-debug.in afm-debug)
		install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/afm-debug DESTINATION ${CMAKE_INSTALL_BINDIR})
	endif()
endif()

# migration from before 10 to 10
configure_file(afb-appmgr-migration-services.sh.in afb-appmgr-migration-services.sh @ONLY)
install(
	PROGRAMS
		${CMAKE_CURRENT_BINARY_DIR}/afb-appmgr-migration-services.sh
	DESTINATION
		${afm_libexecdir}
)
