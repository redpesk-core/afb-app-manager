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
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <assert.h>

#include <json-c/json.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-bus-protocol.h>

#include "utils-jbus.h"

/*
 * max depth of json messages
 */
#define MAX_JSON_DEPTH 10

/*
 * errors messages generated by jbus
 */
static const char out_of_memory_string[] = "out of memory";

/*
 * structure for services
 */
struct jservice {
	struct jservice *next;	/* link to the next service */
	char *method;		/* method name for the service */
	void (*oncall_s) (struct sd_bus_message *, const char *, void *);
				/* string callback */
	void (*oncall_j) (struct sd_bus_message *, struct json_object *, void *);
				/* json callback */
	void *data;		/* closure data for the callbacks */
};

/*
 * structure for signals
 */
struct jsignal {
	struct jsignal *next;	/* link to the next signal */
	char *name;		/* name of the expected signal */
	void (*onsignal_s) (const char *, void *);
				/* string callback */
	void (*onsignal_j) (struct json_object *, void *);
				/* json callback */
	void *data;		/* closure data for the callbacks */
};

/*
 * structure for asynchronous requests
 */
struct jrespw {
	struct jbus *jbus;
	void (*onresp_s) (int, const char *, void *);
				/* string callback */
	void (*onresp_j) (int, struct json_object *, void *);
				/* json callback */
	void *data;		/* closure data for the callbacks */
};

/*
 * structure for handling either client or server jbus on dbus
 */
struct jbus {
	int refcount;			/* referenced how many time */
	struct sd_bus *sdbus;
	struct sd_bus_slot *sservice;
	struct sd_bus_slot *ssignal;
	struct json_tokener *tokener;	/* string to json tokenizer */
	struct jservice *services;	/* first service */
	struct jsignal *signals;	/* first signal */
	char *path;			/* dbus path */
	char *name;			/* dbus name */
};

/*********************** STATIC COMMON METHODS *****************/

static int mkerrno(int rc)
{
	if (rc >= 0)
		return rc;
	errno = -rc;
	return -1;
}

/*
 * Replies the error "out of memory".
 * This function is intended to be used in services when an
 * allocation fails. Thus, it set errno to ENOMEM and
 * returns -1.
 */
static inline int reply_out_of_memory(struct sd_bus_message *smsg)
{
	jbus_reply_error_s(smsg, out_of_memory_string);
	errno = ENOMEM;
	return -1;
}

/*
 * Parses the json-string 'msg' to create a json object stored
 * in 'obj'. It uses the tokener of 'jbus'. This is a small
 * improvement to avoid recreation of tokeners.
 *
 * Returns 1 in case of success and put the result in *'obj'.
 * Returns 0 in case of error and put NULL in *'obj'.
 */
static int jparse(struct jbus *jbus, const char *msg, struct json_object **obj)
{
	json_tokener_reset(jbus->tokener);
	*obj = json_tokener_parse_ex(jbus->tokener, msg, -1);
	if (json_tokener_get_error(jbus->tokener) == json_tokener_success)
		return 1;
	json_object_put(*obj);
	*obj = NULL;
	return 0;
}

static int on_service_call(struct sd_bus_message *smsg, struct jbus *jbus, sd_bus_error *error)
{
	struct jservice *service;
	const char *member, *content;
	struct json_object *obj;

	/* check the type */
	if (!sd_bus_message_has_signature(smsg, "s")
	  || sd_bus_message_read_basic(smsg, 's', &content) < 0) {
		sd_bus_error_set_const(error, "bad signature", "");
		return 1;
	}

	/* dispatch */
	member = sd_bus_message_get_member(smsg);
	service = jbus->services;
	while (service != NULL) {
		if (!strcmp(service->method, member)) {
			sd_bus_message_ref(smsg);
			if (service->oncall_s)
				service->oncall_s(smsg, content, service->data);
			else if (service->oncall_j) {
				if (!jparse(jbus, content, &obj))
					obj = json_object_new_string(content);
				service->oncall_j(smsg, obj, service->data);
				json_object_put(obj);
			}
			return 1;
		}
		service = service->next;
	}
	return 0;
}

/*
 * Adds to 'jbus' a service of name 'method'. The service is
 * performed by one of the callback 'oncall_s' (for string) or
 * 'oncall_j' (for json) that will receive the request and the
 * closure parameter 'data'.
 *
 * returns 0 in case of success or -1 in case of error (ENOMEM).
 */
static int add_service(
		struct jbus *jbus,
		const char *method,
		void (*oncall_s) (struct sd_bus_message *, const char *, void *),
		void (*oncall_j) (struct sd_bus_message *, struct json_object *, void *),
		void *data)
{
	int rc;
	struct jservice *srv;

	/* connection of the service */
	if (jbus->sservice == NULL) {
		rc = sd_bus_add_object(jbus->sdbus, &jbus->sservice, jbus->path, (void*)on_service_call, jbus);
		if (rc < 0) {
			errno = -rc;
			goto error;
		}
	}

	/* allocation */
	srv = malloc(sizeof *srv);
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
	srv->oncall_s = oncall_s;
	srv->oncall_j = oncall_j;
	srv->data = data;
	srv->next = jbus->services;
	jbus->services = srv;

	return 0;

 error2:
	free(srv);
 error:
	return -1;
}

static int on_signal_event(struct sd_bus_message *smsg, struct jbus *jbus, sd_bus_error *error)
{
	struct jsignal *signal;
	const char *member, *content;
	struct json_object *obj;

	/* check the type */
	if (!sd_bus_message_has_signature(smsg, "s")
	  || sd_bus_message_read_basic(smsg, 's', &content) < 0)
		return 0;

	/* dispatch */
	member = sd_bus_message_get_member(smsg);
	signal = jbus->signals;
	while (signal != NULL) {
		if (!strcmp(signal->name, member)) {
			if (signal->onsignal_s)
				signal->onsignal_s(content, signal->data);
			else if (signal->onsignal_j) {
				if (!jparse(jbus, content, &obj))
					obj = json_object_new_string(content);
				signal->onsignal_j(obj, signal->data);
				json_object_put(obj);
			}
		}
		signal = signal->next;
	}
	return 0;
}

/*
 * Adds to 'jbus' a handler for the signal of 'name' emmited by
 * the sender and the interface that 'jbus' is linked to.
 * The signal is handled by one of the callback 'onsignal_s'
 * (for string) or 'onsignal_j' (for json) that will receive
 * parameters associated with the signal and the closure
 * parameter 'data'.
 *
 * returns 0 in case of success or -1 in case of error (ENOMEM).
 */
static int add_signal(
		struct jbus *jbus,
		const char *name,
		void (*onsignal_s) (const char *, void *),
		void (*onsignal_j) (struct json_object *, void *),
		void *data)
{
	int rc;
	struct jsignal *sig;
	char *match;

	/* connection of the signal */
	if (jbus->ssignal == NULL) {
		rc = asprintf(&match, "type='signal',path='%s',interface='%s'", jbus->path, jbus->name);
		if (rc < 0) {
			errno = ENOMEM;
			goto error;
		}
		rc = sd_bus_add_match(jbus->sdbus, &jbus->ssignal, match, (void*)on_signal_event, jbus);
		free(match);
		if (rc < 0) {
			errno = -rc;
			goto error;
		}
	}

	/* allocation */
	sig = malloc(sizeof *sig);
	if (sig == NULL) {
		errno = ENOMEM;
		goto error;
	}
	sig->name = strdup(name);
	if (!sig->name) {
		errno = ENOMEM;
		goto error2;
	}

	/* record the signal */
	sig->onsignal_s = onsignal_s;
	sig->onsignal_j = onsignal_j;
	sig->data = data;
	sig->next = jbus->signals;
	jbus->signals = sig;

	return 0;

 error2:
	free(sig);
 error:
	return -1;
}

static int on_reply(struct sd_bus_message *smsg, struct jrespw *jrespw, sd_bus_error *error)
{
	struct json_object *obj;
	const char *reply;
	int iserror;

	/* check the type */
	if (!sd_bus_message_has_signature(smsg, "s")
	  || sd_bus_message_read_basic(smsg, 's', &reply) < 0) {
		sd_bus_error_set_const(error, "bad signature", "");
		goto end;
	}
	iserror = sd_bus_message_is_method_error(smsg, NULL);

	/* dispatch string? */
	if (jrespw->onresp_s != NULL) {
		jrespw->onresp_s(iserror, reply, jrespw->data);
		goto end;
	}

	/* dispatch json */
	if (!jparse(jrespw->jbus, reply, &obj))
		obj = json_object_new_string(reply);
	jrespw->onresp_j(iserror, obj, jrespw->data);
	json_object_put(obj);

 end:
	free(jrespw);
	return 1;
}

/*
 * Creates a message for 'method' with one string parameter being 'query'
 * and sends it to the destination, object and interface linked to 'jbus'.
 *
 * Adds to 'jbus' the response handler defined by the callbacks 'onresp_s'
 * (for string) and 'onresp_j' (for json) and the closure parameter 'data'.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
static int call(
		struct jbus *jbus,
		const char *method,
		const char *query,
		void (*onresp_s) (int, const char *, void *),
		void (*onresp_j) (int, struct json_object *, void *),
		void *data)
{
	int rc;
	struct jrespw *resp;

	/* allocates the response structure */
	resp = malloc(sizeof *resp);
	if (resp == NULL) {
		errno = ENOMEM;
		goto error;
	}

	/* fulfill the response structure */
	resp->jbus = jbus;
	resp->onresp_s = onresp_s;
	resp->onresp_j = onresp_j;
	resp->data = data;

	rc = sd_bus_call_method_async(jbus->sdbus, NULL, jbus->name, jbus->path, jbus->name, method, (void*)on_reply, resp, "s", query);
	if (rc < 0) {
		errno = -rc;
		goto error2;
	}

	return 0;

 error2:
	free(resp);
 error:
	return -1;
}

/********************* MAIN FUNCTIONS *****************************************/

/*
 * Creates a 'jbus' bound the 'path' and it derived names and linked
 * either to the DBUS SYSTEM when 'session' is nul or to the DBUS SESSION
 * if 'session' is not nul.
 *
 * The parameter 'path' is intended to be the path of a DBUS single object.
 * Single means that it exists only one instance of the object on the
 * given bus. That path implies 2 derived DBUS names:
 *   1. the destination name of the program that handles the object
 *   2. the interface name of the object
 * These names are derived by removing the heading slash (/) and
 * by replacing all occurences of slashes by dots.
 * For example, passing path = /a/b/c means that the object /a/b/c is
 * handled by the destination a.b.c and replies to the interface a.b.c
 *
 * Returns the created jbus or NULL in case of error.
 */
struct jbus *create_jbus(struct sd_bus *sdbus, const char *path)
{
	struct jbus *jbus;
	char *name;

	/* create the jbus object */
	jbus = calloc(1, sizeof *jbus);
	if (jbus == NULL) {
		errno = ENOMEM;
		goto error;
	}
	jbus->refcount = 1;

	/* create the tokener */
	jbus->tokener = json_tokener_new_ex(MAX_JSON_DEPTH);
	if (jbus->tokener == NULL) {
		errno = ENOMEM;
		goto error2;
	}

	/* records the path */
	jbus->path = strdup(path);
	if (jbus->path == NULL) {
		errno = ENOMEM;
		goto error2;
	}

	/* makes the name from the path */
	while (*path == '/')
		path++;
	jbus->name = name = strdup(path);
	if (name == NULL) {
		errno = ENOMEM;
		goto error2;
	}
	while (*name) {
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

	/* connect and init */
	jbus->sdbus = sd_bus_ref(sdbus);

	return jbus;

 error2:
	jbus_unref(jbus);
 error:
	return NULL;
}

/*
 * Adds one reference to 'jbus'.
 */
void jbus_addref(struct jbus *jbus)
{
	jbus->refcount++;
}

/*
 * Removes one reference to 'jbus'. Destroys 'jbus' and it related
 * data if the count of references decrease to zero.
 */
void jbus_unref(struct jbus *jbus)
{
	struct jservice *srv;
	struct jsignal *sig;
	if (!--jbus->refcount) {
		while ((srv = jbus->services) != NULL) {
			jbus->services = srv->next;
			free(srv->method);
			free(srv);
		}
		while ((sig = jbus->signals) != NULL) {
			jbus->signals = sig->next;
			free(sig->name);
			free(sig);
		}
		if (jbus->sservice != NULL)
			sd_bus_slot_unref(jbus->sservice);
		if (jbus->ssignal != NULL)
			sd_bus_slot_unref(jbus->ssignal);
		if (jbus->tokener != NULL)
			json_tokener_free(jbus->tokener);
		sd_bus_unref(jbus->sdbus);
		free(jbus->name);
		free(jbus->path);
		free(jbus);
	}
}

/*
 * Replies an error of string 'error' to the request handled by 'smsg'.
 * Also destroys the request 'smsg' that must not be used later.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_reply_error_s(struct sd_bus_message *smsg, const char *error)
{
	int rc = sd_bus_reply_method_errorf(smsg, SD_BUS_ERROR_FAILED, "%s", error);
	sd_bus_message_unref(smsg);
	return mkerrno(rc);
}

/*
 * Replies an error of json 'reply' to the request handled by 'smsg'.
 * Also destroys the request 'smsg' that must not be used later.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_reply_error_j(struct sd_bus_message *smsg, struct json_object *reply)
{
	const char *str = json_object_to_json_string(reply);
	return str ? jbus_reply_error_s(smsg, str) : reply_out_of_memory(smsg);
}

/*
 * Replies normally the string 'reply' to the request handled by 'smsg'.
 * Also destroys the request 'smsg' that must not be used later.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_reply_s(struct sd_bus_message *smsg, const char *reply)
{
	int rc = sd_bus_reply_method_return(smsg, "s", reply);
	sd_bus_message_unref(smsg);	
	return mkerrno(rc);
}

/*
 * Replies normally the json 'reply' to the request handled by 'smsg'.
 * Also destroys the request 'smsg' that must not be used later.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_reply_j(struct sd_bus_message *smsg, struct json_object *reply)
{
	const char *str = json_object_to_json_string(reply);
	return str ? jbus_reply_s(smsg, str) : reply_out_of_memory(smsg);
}

/*
 * Sends from 'jbus' the signal of 'name' handling the string 'content'.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_send_signal_s(struct jbus *jbus, const char *name, const char *content)
{
	return mkerrno(sd_bus_emit_signal(jbus->sdbus, jbus->path, jbus->name, name, "s", content));
}

/*
 * Sends from 'jbus' the signal of 'name' handling the json 'content'.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_send_signal_j(struct jbus *jbus, const char *name,
		       struct json_object *content)
{
	const char *str = json_object_to_json_string(content);
	if (str == NULL) {
		errno = ENOMEM;
		return -1;
	}
	return jbus_send_signal_s(jbus, name, str);
}

/*
 * Adds to 'jbus' a service handling calls to the 'method' using
 * the "string" callback 'oncall' and the closure value 'data'.
 *
 * The callback 'oncall' is invoked for handling incoming method
 * calls. It receives 3 parameters:
 *   1. struct sd_bus_message *: a handler to data to be used for replying
 *   2. const char *: the received string
 *   3. void *: the closure 'data' set by this function
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_add_service_s(
		struct jbus *jbus,
		const char *method,
		void (*oncall) (struct sd_bus_message *, const char *, void *),
		void *data)
{
	return add_service(jbus, method, oncall, NULL, data);
}

/*
 * Adds to 'jbus' a service handling calls to the 'method' using
 * the "json" callback 'oncall' and the closure value 'data'.
 *
 * The callback 'oncall' is invoked for handling incoming method
 * calls. It receives 3 parameters:
 *   1. struct sd_bus_message *: a handler to data to be used for replying
 *   2. struct json_object *: the received json
 *   3. void *: the closure 'data' set by this function
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_add_service_j(
		struct jbus *jbus,
		const char *method,
		void (*oncall) (struct sd_bus_message *, struct json_object *, void *),
		void *data)
{
	return add_service(jbus, method, NULL, oncall, data);
}

/*
 * Start to serve: activate services declared for 'jbus'.
 * This function, in fact, declares 'jbus' as the receiver
 * for calls to the destination derived from the path set at
 * 'jbus' creation.
 * It also allows 'jbus' to emit signals of that origin.
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_start_serving(struct jbus *jbus)
{
	return mkerrno(sd_bus_request_name(jbus->sdbus, jbus->name, 0));
}

/*
 * Asynchronous call to 'method' of 'jbus' passing the string 'query'.
 * On response, the function 'onresp' is called with the returned string
 * value and the closure 'data'.
 * The function 'onresp' is invoked with 3 parameters:
 *   1. int: 0 if no error or -1 if error.
 *   2. const char *: the returned string (might be NULL if error)
 *   3. void *: the closure 'data'
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_call_ss(
		struct jbus *jbus,
		const char *method,
		const char *query,
		void (*onresp) (int, const char *, void *),
		void *data)
{
	return call(jbus, method, query, onresp, NULL, data);
}

/*
 * Asynchronous call to 'method' of 'jbus' passing the string 'query'.
 * On response, the function 'onresp' is called with the returned json
 * value and the closure 'data'.
 * The function 'onresp' is invoked with 3 parameters:
 *   1. int: 0 if no error or -1 if error.
 *   2. const char *: the returned json (might be NULL if error)
 *   3. void *: the closure 'data'
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_call_sj(
		struct jbus *jbus,
		const char *method,
		const char *query,
		void (*onresp) (int, struct json_object *, void *),
		void *data)
{
	return call(jbus, method, query, NULL, onresp, data);
}

/*
 * Asynchronous call to 'method' of 'jbus' passing the json 'query'.
 * On response, the function 'onresp' is called with the returned string
 * value and the closure 'data'.
 * The function 'onresp' is invoked with 3 parameters:
 *   1. int: 0 if no error or -1 if error.
 *   2. const char *: the returned string (might be NULL if error)
 *   3. void *: the closure 'data'
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_call_js(
		struct jbus *jbus,
		const char *method,
		struct json_object *query,
		void (*onresp) (int, const char *, void *),
		void *data)
{
	const char *str = json_object_to_json_string(query);
	if (str == NULL) {
		errno = ENOMEM;
		return -1;
	}
	return call(jbus, method, str, onresp, NULL, data);
}

/*
 * Asynchronous call to 'method' of 'jbus' passing the json 'query'.
 * On response, the function 'onresp' is called with the returned json
 * value and the closure 'data'.
 * The function 'onresp' is invoked with 3 parameters:
 *   1. int: 0 if no error or -1 if error.
 *   2. const char *: the returned json (might be NULL if error)
 *   3. void *: the closure 'data'
 *
 * Returns 0 in case of success or -1 in case of error.
 */
int jbus_call_jj(
		struct jbus *jbus,
		const char *method,
		struct json_object *query,
		void (*onresp) (int, struct json_object *, void *),
		void *data)
{
	const char *str = json_object_to_json_string(query);
	if (str == NULL) {
		errno = ENOMEM;
		return -1;
	}
	return call(jbus, method, str, NULL, onresp, data);
}

/*
 * Synchronous call to 'method' of 'jbus' passing the string 'query'.
 * The returned string response is returned.
 *
 * Returns the string response or NULL in case of error.
 */
char *jbus_call_ss_sync(
		struct jbus *jbus,
		const char *method,
		const char *query)
{
	sd_bus_message *smsg = NULL;
        sd_bus_error error = SD_BUS_ERROR_NULL;
	char *result = NULL;
	const char *reply;

	/* makes the call */
	if (mkerrno(sd_bus_call_method(jbus->sdbus, jbus->name, jbus->path, jbus->name, method, &error, &smsg, "s", query)) < 0)
		goto error;

	/* check if error */
	if (sd_bus_message_is_method_error(smsg, NULL))
		goto error;

	/* check the returned type */
	if (!sd_bus_message_has_signature(smsg, "s")
	  || sd_bus_message_read_basic(smsg, 's', &reply) < 0)
		goto error;

	/* get the result */
	result = strdup(reply);

error:
	sd_bus_message_unref(smsg);
	sd_bus_error_free(&error);
	return result;
}

/*
 * Synchronous call to 'method' of 'jbus' passing the string 'query'.
 * The returned json response is returned.
 *
 * Returns the json response or NULL in case of error.
 */
struct json_object *jbus_call_sj_sync(
		struct jbus *jbus,
		const char *method,
		const char *query)
{
	struct json_object *obj;
	char *str = jbus_call_ss_sync(jbus, method, query);
	if (str == NULL)
		obj = NULL;
	else {
		jparse(jbus, str, &obj);
		free(str);
	}
	return obj;
}

/*
 * Synchronous call to 'method' of 'jbus' passing the json 'query'.
 * The returned string response is returned.
 *
 * Returns the string response or NULL in case of error.
 */
char *jbus_call_js_sync(
		struct jbus *jbus,
		const char *method,
		struct json_object *query)
{
	const char *str = json_object_to_json_string(query);
	if (str == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	return jbus_call_ss_sync(jbus, method, str);
}

/*
 * Synchronous call to 'method' of 'jbus' passing the json 'query'.
 * The returned json response is returned.
 *
 * Returns the json response or NULL in case of error.
 */
struct json_object *jbus_call_jj_sync(
		struct jbus *jbus,
		const char *method,
		struct json_object *query)
{
	const char *str = json_object_to_json_string(query);
	if (str == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	return jbus_call_sj_sync(jbus, method, str);
}

/*
 * Records for 'jbus' the string signal handler 'onsig' with closure 'data'
 * for the signal of 'name'.
 * The callback handler is called with 2 arguments:
 *   1. char *: the string parameter associated to the signal
 *   2. void *: the closure data.
 *
 * Returns 0 in case of success or -1 otherwise.
 */
int jbus_on_signal_s(
		struct jbus *jbus,
		const char *name,
		void (*onsig) (const char *, void *),
		void *data)
{
	return add_signal(jbus, name, onsig, NULL, data);
}

/*
 * Records for 'jbus' the json signal handler 'onsig' with closure 'data'
 * for the signal of 'name'.
 * The callback handler is called with 2 arguments:
 *   1. struct json_object *: the json parameter associated to the signal
 *   2. void *: the closure data.
 *
 * Returns 0 in case of success or -1 otherwise.
 */
int jbus_on_signal_j(
		struct jbus *jbus,
		const char *name,
		void (*onsig) (struct json_object *, void *),
		void *data)
{
	return add_signal(jbus, name, NULL, onsig, data);
}

/****************** FEW LITTLE TESTS *****************************************/

#if defined(SERVER)||defined(CLIENT)
#include <stdio.h>
#include <unistd.h>

static struct sd_bus *msbus()
{
	static struct sd_bus *r = NULL;
	if (r == NULL) {
		static sd_event *e;
		sd_event_default(&e);
		sd_bus_open_user(&r);
		sd_bus_attach_event(r, e, 0);
	}
	return r;
}

static sd_event *events()
{
	static sd_event *ev = NULL;
	if (ev == NULL)
		ev = sd_bus_get_event(msbus());
	return ev;
}

static int mwait(int timeout, void *closure)
{
	sd_event_run(events(), -1);
	return 0;
}

static struct jbus *jbus;

#ifdef SERVER
void ping(struct sd_bus_message *smsg, struct json_object *request, void *unused)
{
	printf("ping(%s) -> %s\n", json_object_to_json_string(request),
	       json_object_to_json_string(request));
	jbus_reply_j(smsg, request);
	json_object_put(request);
}

void incr(struct sd_bus_message *smsg, struct json_object *request, void *unused)
{
	static int counter = 0;
	struct json_object *res = json_object_new_int(++counter);
	printf("incr(%s) -> %s\n", json_object_to_json_string(request),
	       json_object_to_json_string(res));
	jbus_reply_j(smsg, res);
	jbus_send_signal_j(jbus, "incremented", res);
	json_object_put(res);
	json_object_put(request);
}

int main()
{
	int s1, s2, s3;
	jbus = create_jbus(msbus(), "/bzh/iot/jdbus");
	s1 = jbus_add_service_j(jbus, "ping", ping, NULL);
	s2 = jbus_add_service_j(jbus, "incr", incr, NULL);
	s3 = jbus_start_serving(jbus);
	printf("started %d %d %d\n", s1, s2, s3);
	while (!mwait(-1,jbus)) ;
	return 0;
}
#endif

#ifdef CLIENT
void onresp(int status, struct json_object *response, void *data)
{
	printf("resp: %d, %s, %s\n", status, (char *)data,
	       json_object_to_json_string(response));
	json_object_put(response);
}

void signaled(const char *content, void *data)
{
	printf("signaled with {%s}/%s\n", content, (char*)data);
}

int main()
{
	int i = 1;
	jbus = create_jbus(msbus(), "/bzh/iot/jdbus");
	jbus_on_signal_s(jbus, "incremented", signaled, "closure-signal");
	while (i--) {
		jbus_call_sj(jbus, "ping", "{\"toto\":[1,2,3,4,true,\"toto\"]}",
			     onresp, "ping");
		jbus_call_sj(jbus, "incr", "{\"doit\":\"for-me\"}", onresp,
			     "incr");
		mwait(-1,jbus);
	}
	printf("[[[%s]]]\n",
	       jbus_call_ss_sync(jbus, "ping", "\"formidable!\""));
	while (!mwait(-1,jbus)) ;
	return 0;
}
#endif
#endif

