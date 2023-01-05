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


