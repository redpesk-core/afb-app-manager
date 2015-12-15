/*
 Copyright 2015 IoT.bzh

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <json.h>
#include <dbus/dbus.h>

#include "utils-jbus.h"

struct jreq;
struct jservice;
struct jbus;

struct jreq {
	DBusConnection *connection;
	DBusMessage *reply;
};

struct jservice {
	struct jservice *next;
	char *method;
	void (*oncall)(struct jreq *, struct json_object *);
};

struct jrespw {
	struct jrespw *next;
	dbus_uint32_t serial;
	void *data;
	void (*onresp)(int status, struct json_object *response, void *data);
};

struct jbus {
	int refcount;
	struct jservice *services;
	DBusConnection *connection;
	struct jrespw *waiters;
	char *path;
	char *name;
};

static const char reply_out_of_memory[] = "{\"status\":\"out of memory\"}";
static const char reply_invalid[] = "{\"status\":\"invalid request\"}";
static const char interface_jbus[] = "org.jbus";

static int send_reply(struct jreq *jreq, const char *reply)
{
	int rc = -1;
	if (dbus_message_append_args(jreq->reply, DBUS_TYPE_STRING, &reply, DBUS_TYPE_INVALID)) {
		if (dbus_connection_send(jreq->connection, jreq->reply, NULL))
			rc = 0;
	}
	dbus_message_unref(jreq->reply);
	dbus_connection_unref(jreq->connection);
	free(jreq);
	return rc;
}

static DBusHandlerResult incoming_resp(DBusConnection *connection, DBusMessage *message, struct jbus *jbus)
{
	int status;
	const char *str;
	struct jrespw *jrw, **prv;
	struct json_object *reply;
	dbus_uint32_t serial;

	/* search for the waiter */
	serial = dbus_message_get_serial(message);
	prv = &jbus->waiters;
	while ((jrw = *prv) != NULL && jrw->serial != serial)
		prv = &jrw->next;
	if (jrw == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	*prv = jrw->next;

	/* retrieve the json value */
	if (!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID)) {
		status = -1;
		reply = NULL;
	} else {
		reply = json_tokener_parse(str);
		status = reply ? 0 : -1;
	}

	/* treat it */
	jrw->onresp(status, reply, jrw->data);
	free(jrw);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static int matchitf(DBusMessage *message)
{
	const char *itf = dbus_message_get_interface(message);
	return itf != NULL && !strcmp(itf, interface_jbus);
}

static DBusHandlerResult incoming_call(DBusConnection *connection, DBusMessage *message, struct jbus *jbus)
{
	struct jservice *srv;
	struct jreq *jreq;
	const char *str;
	const char *method;
	struct json_object *query;

	/* search for the service */
	if (!matchitf(message))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	method = dbus_message_get_member(message);
	if (method == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	srv = jbus->services;
	while(srv != NULL && strcmp(method, srv->method))
		srv = srv->next;
	if (srv == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	/* handle the message */
	jreq = malloc(sizeof * jreq);
	if (jreq == NULL)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	jreq->reply = dbus_message_new_method_return(message);
	if (jreq->reply == NULL) {
		free(jreq);
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}
	jreq->connection = dbus_connection_ref(jbus->connection);
	
	/* retrieve the json value */
	if (!dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID)) {
		send_reply(jreq, reply_invalid);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	query = json_tokener_parse(str);
	if (query == NULL) {
		send_reply(jreq, reply_invalid);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	/* treat it */
	srv->oncall(jreq, query);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult incoming(DBusConnection *connection, DBusMessage *message, void *data)
{
	switch(dbus_message_get_type(message)) {
	case DBUS_MESSAGE_TYPE_METHOD_CALL:
		return incoming_call(connection, message, (struct jbus*)data);
	case  DBUS_MESSAGE_TYPE_METHOD_RETURN:
		return incoming_resp(connection, message, (struct jbus*)data);
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


struct jbus *create_jbus(int session, const char *path)
{
	struct jbus *jbus;
	char *name;

	/* create the context and connect */
	jbus = calloc(1, sizeof * jbus);
	if (jbus == NULL) {
		errno = ENOMEM;
		goto error;
	}
	jbus->refcount = 1;
	jbus->path = strdup(path);
	jbus->name = NULL;
	if (jbus->path == NULL) {
		errno = ENOMEM;
		goto error2;
	}
	while(*path == '/') path++;
	jbus->name = name = strdup(path);
	if (name == NULL) {
		errno = ENOMEM;
		goto error2;
	}
	while(*name) {
		if (*name == '/')
			*name = '.';
		name++;
	}
	name--;
	while (name >= jbus->name && *name == '.')
		*name-- = 0;
	if (!*jbus->name) {
		errno = EINVAL;
		goto error2;
	}

	/* connect */
	jbus->connection = dbus_bus_get(session ? DBUS_BUS_SESSION : DBUS_BUS_SYSTEM, NULL);
	if (jbus->connection == NULL) {
		goto error2;
	}
	if (!dbus_connection_add_filter(jbus->connection, incoming, jbus, NULL)) {
		goto error2;
	}

	return jbus;

error2:
	jbus_unref(jbus);
error:
	return NULL;
}

void jbus_addref(struct jbus *jbus)
{
	jbus->refcount++;
}

void jbus_unref(struct jbus *jbus)
{
	struct jservice *srv;
	if (!--jbus->refcount) {
		dbus_connection_unref(jbus->connection);
		while((srv = jbus->services) != NULL) {
			jbus->services = srv->next;
			free(srv->method);
			free(srv);
		}
		free(jbus->name);
		free(jbus->path);
		free(jbus);
	}
}

int jbus_reply(struct jreq *jreq, struct json_object *reply)
{
	const char *str = json_object_to_json_string(reply);
	return send_reply(jreq, str ? str : reply_out_of_memory);
}

int jbus_add_service(struct jbus *jbus, const char *method, void (*oncall)(struct jreq *jreq, struct json_object *request))
{
	struct jservice *srv;

	/* allocation */
	srv = malloc(sizeof * srv);
	if (srv == NULL) {
		errno = ENOMEM;
		goto error;
	}
	srv->method = strdup(method);
	if (!srv->method) {
		errno = ENOMEM;
		goto error2;
	}

	/* record the service */
	srv->oncall = oncall;
	srv->next = jbus->services;
	jbus->services = srv;

	return 0;

error3:
	free(srv->method);
error2:
	free(srv);
error:
	return -1;
}

int jbus_start_serving(struct jbus *jbus)
{
	int status = dbus_bus_request_name(jbus->connection, jbus->name, DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL);
	switch (status) {
	case DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER:
	case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
		return 0;
	case DBUS_REQUEST_NAME_REPLY_EXISTS:
	case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
		errno = EADDRINUSE;
		return -1;
	}
}

int jbus_callj(struct jbus *jbus, const char *method, const char *query, void (*onresp)(int status, struct json_object *response, void *data), void *data)
{
	int rc;
	DBusMessage *msg;
	struct jrespw *resp;

	resp = malloc(sizeof * resp);
	if (resp == NULL) {
		errno = ENOMEM;
		goto error;
	}

	msg = dbus_message_new_method_call(jbus->name, jbus->path, interface_jbus, method);
	if (msg == NULL) {
		errno = ENOMEM;
		goto error2;
	}

	if (!dbus_message_append_args(msg, DBUS_TYPE_STRING, &query, DBUS_TYPE_INVALID)) {
		errno = ENOMEM;
		goto error3;
	}

	if (!dbus_connection_send(jbus->connection, msg, &resp->serial)) {
		goto error3;
	}

	dbus_message_unref(msg);
	resp->data = data;
	resp->onresp = onresp;
	resp->next = jbus->waiters;
	jbus->waiters = resp;
	return 0;

error3:
	dbus_message_unref(msg);
error2:
	free(resp);
error:
	return -1;
}


int jbus_call(struct jbus *jbus, const char *method, struct json_object *query, void (*onresp)(int status, struct json_object *response, void *data), void *data)
{
	const char *str = json_object_to_json_string(query);
	if (str == NULL) {
		errno = ENOMEM;
		return -1;
	}
	return jbus_callj(jbus, method, str, onresp, data);
}

#ifdef SERVER
#include <stdio.h>
#include <unistd.h>
void ping(struct jreq *jreq, struct json_object *request)
{
printf("ping(%s) -> %s\n",json_object_to_json_string(request),json_object_to_json_string(request));
	jbus_reply(jreq, request);
	json_object_put(request);
}
void incr(struct jreq *jreq, struct json_object *request)
{
	static int counter = 0;
	struct json_object *res = json_object_new_int(++counter);
printf("incr(%s) -> %s\n",json_object_to_json_string(request),json_object_to_json_string(res));
	jbus_reply(jreq, res);
	json_object_put(res);
	json_object_put(request);
}
int main()
{
	struct jbus *jbus = create_jbus(1, "/bzh/iot/jdbus");
	int s1 = jbus_add_service(jbus, "ping", ping);
	int s2 = jbus_add_service(jbus, "incr", incr);
	int s3 = jbus_start_serving(jbus);
	printf("started %d %d %d\n", s1, s2, s3);
	while (dbus_connection_read_write_dispatch (jbus->connection, -1))
		;
}
#endif
#ifdef CLIENT
#include <stdio.h>
#include <unistd.h>
void onresp(int status, struct json_object *response, void *data)
{
	printf("resp: %d, %s, %s\n",status,(char*)data,json_object_to_json_string(response));
	json_object_put(response);
}
int main()
{
	struct jbus *jbus = create_jbus(1, "/bzh/iot/jdbus");
	int i = 10;
	while(i--) {
		jbus_callj(jbus, "ping", "{\"toto\":[1,2,3,4,true,\"toto\"]}", onresp, "ping");
		jbus_callj(jbus, "incr", "{\"doit\":\"for-me\"}", onresp, "incr");
		dbus_connection_read_write_dispatch (jbus->connection, 1);
	}
	while (dbus_connection_read_write_dispatch (jbus->connection, -1))
		;
}
#endif

