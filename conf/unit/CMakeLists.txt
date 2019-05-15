###########################################################################
# Copyright (C) 2015-2019 IoT.bzh
#
# author: José Bollo <jose.bollo@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

cmake_minimum_required(VERSION 2.8)

find_program(M4EXEC m4)
mark_as_advanced(M4EXEC)

file(GLOB in_afm_unit_list
	LIST_DIRECTORIES false
	RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
	CONFIGURE_DEPENDS
	afm-unit *.inc
)
foreach(f IN LISTS in_afm_unit_list)
	configure_file(${f} ${f})
	set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/afm-unit.conf APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${f})
endforeach(f)
if(AGL_DEVEL)
	set(optm4 -DAGL_DEVEL)
endif()
add_custom_command(
	OUTPUT afm-unit.conf
	COMMAND ${M4EXEC} ${optm4} -I. afm-unit > afm-unit.conf
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_target(unitconf ALL DEPENDS afm-unit.conf)

if(NOT USE_SDK)
	install(DIRECTORY DESTINATION ${afm_confdir}/unit.env.d)
	install(DIRECTORY DESTINATION ${afm_confdir}/widget.env.d)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/afm-unit.conf DESTINATION ${afm_confdir})
endif()

