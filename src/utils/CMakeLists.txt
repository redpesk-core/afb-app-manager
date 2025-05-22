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

add_library(utils STATIC
	domain-spec.c
	manage-afid.c
	manifest.c
	mime-type.c
	path-entry.c
	path-type.c
	permset.c
	sighup-framework.c
	unit-oper.c
	unit-generator.c
	utils-dir.c
	utils-json.c
	utils-systemd.c
)

target_compile_options(utils PRIVATE ${libjsonc_CFLAGS} ${libsystemd_CFLAGS})
target_include_directories(utils PRIVATE ${libjsonc_INCLUDE_DIRS} ${libsystemd_INCLUDE_DIRS})
target_link_libraries(utils PUBLIC units ${libjsonc_LIBRARIES} ${libsystemd_LIBRARIES})
target_link_directories(utils PUBLIC ${libjsonc_LIBRARY_DIRS} ${libsystemd_LIBRARY_DIRS})
