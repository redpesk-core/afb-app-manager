/*
 Copyright (C) 2015-2025 IoT.bzh Company

 Author: José Bollo <jose.bollo@iot.bzh>

 $RP_BEGIN_LICENSE$
 Commercial License Usage
  Licensees holding valid commercial IoT.bzh licenses may use this file in
  accordance with the commercial license agreement provided with the
  Software or, alternatively, in accordance with the terms contained in
  a written agreement between you and The IoT.bzh Company. For licensing terms
  and conditions see https://www.iot.bzh/terms-conditions. For further
  information use the contact form at https://www.iot.bzh/contact.

 GNU General Public License Usage
  Alternatively, this file may be used under the terms of the GNU General
  Public license version 3. This license is as published by the Free Software
  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
  of this file. Please review the following information to ensure the GNU
  General Public License requirements will be met
  https://www.gnu.org/licenses/gpl-3.0.html.
 $RP_END_LICENSE$
*/

#pragma once

#define AFMPKG_CLIENT_BUFFER0_SIZE 4050

typedef
enum afmpkg_operation_e {
	afmpkg_operation_Add,
	afmpkg_operation_Remove,
	afmpkg_operation_Check_Add,
	afmpkg_operation_Check_Remove
}
	afmpkg_operation_t;

typedef
enum afmpkg_client_state_e {
	afmpkg_client_state_None,
	afmpkg_client_state_Started,
	afmpkg_client_state_Ready
}
	afmpkg_client_state_t;

typedef
struct afmpkg_client_s {
	char   *buffer;
	size_t  length;
	size_t  size;
	afmpkg_operation_t operation;
	afmpkg_client_state_t state;
	char    memo;
	char    buffer0[AFMPKG_CLIENT_BUFFER0_SIZE];
}
	afmpkg_client_t;

void afmpkg_client_init(afmpkg_client_t *client);
void afmpkg_client_release(afmpkg_client_t *client);
int afmpkg_client_begin(
		afmpkg_client_t *client,
		afmpkg_operation_t operation,
		const char *package_name,
		int index,
		int count);
int afmpkg_client_end(afmpkg_client_t *client);
int afmpkg_client_put_file(afmpkg_client_t *client, const char *value);
int afmpkg_client_put_rootdir(afmpkg_client_t *client, const char *value);
int afmpkg_client_put_transid(afmpkg_client_t *client, const char *value);
int afmpkg_client_put_redpakid(afmpkg_client_t *client, const char *value);
int afmpkg_client_dial(afmpkg_client_t *client, char **errstr);

