###########################################################################
# Copyright (C) 2015-2020 IoT.bzh
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

if(NOT USE_SDK)
	install(DIRECTORY DESTINATION ${wgtpkg_trusted_certs_dir})
endif()

if(INSTALL_SAMPLE_KEYS)
	if(NOT USE_SDK)
		#foreach(f root) TODO: install only root certificate
		foreach(f root developer partner platform public)
			install(FILES sample/${f}.cert.pem
			        DESTINATION ${wgtpkg_trusted_certs_dir}
			        RENAME sample-cert-${f}.pem
			)
		endforeach(f)
	endif()
	foreach(f developer partner platform public)
		install(FILES sample/${f}.cert.pem
		        DESTINATION ${wgtpkg_sample_certs_dir}
		)
		install(FILES sample/${f}.key.pem
		        DESTINATION ${wgtpkg_sample_keys_dir}
		)
	endforeach(f)
endif()


