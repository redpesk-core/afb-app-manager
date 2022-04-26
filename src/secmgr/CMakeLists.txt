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


add_library(secwrp STATIC
	secmgr-wrap.c
	)

if(NOT SIMULATE_SECMGR)
	pkg_check_modules(SECMGR REQUIRED sec-lsm-manager)
endif()

target_compile_options(secwrp PRIVATE ${SECMGR_CFLAGS} -DSIMULATE_SEC_LSM_MANAGER=$<BOOL:${SIMULATE_SECMGR}>)
target_include_directories(secwrp PRIVATE ${SECMGR_INCLUDE_DIRS})
target_link_libraries(secwrp PUBLIC ${SECMGR_LIBRARIES})
target_link_directories(secwrp PUBLIC ${SECMGR_LIBRARY_DIRS})
