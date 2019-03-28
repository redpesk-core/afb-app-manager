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

if(AGL_DEVEL)
	configure_file(afm-unit-debug.conf.in afm-unit.conf)
else()
	configure_file(afm-unit.conf.in afm-unit.conf)
endif()

if(NOT USE_SDK)
	install(DIRECTORY DESTINATION ${afm_confdir}/unit.env.d)
	install(DIRECTORY DESTINATION ${afm_confdir}/widget.env.d)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/afm-unit.conf             DESTINATION ${afm_confdir})
endif()

