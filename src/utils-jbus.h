/*
 Copyright (C) 2015-2022 IoT.bzh Company

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

struct sd_bus;

struct sd_bus_message;
struct jbus;

extern struct jbus *create_jbus(struct sd_bus *sdbus, const char *path);

extern void jbus_addref(struct jbus *jbus);
extern void jbus_unref(struct jbus *jbus);

/* verbs for the clients */
extern int jbus_call_ss(
		struct jbus *jbus,
		const char *method,
		const char *query,
		void (*onresp) (int, const char *, void *),
		void *data);

extern int jbus_call_js(
		struct jbus *jbus,
		const char *method,
		struct json_object *query,
		void (*onresp) (int, const char *, void *),
		void *data);

extern int jbus_call_sj(
		struct jbus *jbus,
		const char *method,
		const char *query,
		void (*onresp) (int, struct json_object *, void *),
		void *data);

extern int jbus_call_jj(
		struct jbus *jbus,
		const char *method,
		struct json_object *query,
		void (*onresp) (int, struct json_object *, void *),
		void *data);

extern char *jbus_call_ss_sync(
		struct jbus *jbus,
		const char *method,
		const char *query);

extern char *jbus_call_js_sync(
		struct jbus *jbus,
		const char *method,
		struct json_object *query);

extern struct json_object *jbus_call_sj_sync(
		struct jbus *jbus,
		const char *method,
		const char *query);

extern struct json_object *jbus_call_jj_sync(
		struct jbus *jbus,
		const char *method,
		struct json_object *query);

extern int jbus_on_signal_s(
		struct jbus *jbus,
		const char *name,
		void (*onsignal) (const char *, void *),
		void *data);

extern int jbus_on_signal_j(
		struct jbus *jbus,
		const char *name,
		void (*onsignal) (struct json_object *, void *),
		void *data);

/* verbs for servers */
extern int jbus_reply_s(
		struct sd_bus_message *smsg,
		const char *reply);

extern int jbus_reply_j(
		struct sd_bus_message *smsg,
		struct json_object *reply);

extern int jbus_reply_error_s(
		struct sd_bus_message *smsg,
		const char *reply);

extern int jbus_reply_error_j(
		struct sd_bus_message *smsg,
		struct json_object *reply);

extern int jbus_add_service_s(
		struct jbus *jbus,
		const char *method,
		void (*oncall) (struct sd_bus_message *, const char *, void *),
		void *data);

extern int jbus_add_service_j(
		struct jbus *jbus,
		const char *method,
		void (*oncall) (struct sd_bus_message *, struct json_object *, void *),
		void *data);

extern int jbus_start_serving(
		struct jbus *jbus);

extern int jbus_send_signal_s(
		struct jbus *jbus,
		const char *name,
		const char *content);

extern int jbus_send_signal_j(
		struct jbus *jbus,
		const char *name,
		struct json_object *content);

