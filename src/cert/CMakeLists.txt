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


add_executable(
	domain-spec-tool
		domain-spec-tool.c
		domain-spec.c)

pkg_check_modules(gnutls gnutls)
if(${gnutls_FOUND})
	add_executable(
		check-signature-tool
			check-signature-tool.c
			domain-spec.c
			check-signature.c
			common-cert.c)
	target_link_libraries(
		check-signature-tool
			${gnutls_LINK_LIBRARIES})

	add_executable(
		signed-digest-tool
			signed-digest-tool.c
			signed-digest.c
			check-signature.c
			make-signature.c
			domain-spec.c
			digest.c
			common-cert.c
			signature-name.c
			../path-entry.c)
	target_link_libraries(
		signed-digest-tool
			${gnutls_LINK_LIBRARIES})

	add_executable(
		digest-tool
			digest-tool.c
			digest.c
			signed-digest.c
			signature-name.c
			../path-entry.c
			common-cert.c)
	target_link_libraries(
		digest-tool
			${gnutls_LINK_LIBRARIES})

endif()
