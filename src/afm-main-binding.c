/*
 * Copyright (C) 2015, 2016 "IoT.bzh"
 * Author "Fulup Ar Foll"
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <json-c/json.h>

#include <afb/afb-binding.h>

#include "utils-jbus.h"

static const char _added_[]     = "added";
static const char _auto_[]      = "auto";
static const char _continue_[]  = "continue";
static const char _changed_[]   = "changed";
static const char _detail_[]    = "detail";
static const char _id_[]        = "id";
static const char _install_[]   = "install";
static const char _local_[]     = "local";
static const char _mode_[]      = "mode";
static const char _remote_[]    = "remote";
static const char _runid_[]     = "runid";
static const char _runnables_[] = "runnables";
static const char _runners_[]   = "runners";
static const char _start_[]     = "start";
static const char _state_[]     = "state";
static const char _stop_[]      = "stop";
static const char _terminate_[] = "terminate";
static const char _uninstall_[] = "uninstall";
static const char _uri_[]       = "uri";

static const struct afb_binding_interface *binder;

static struct jbus *jbus;

/*
 * Structure for asynchronous call handling
 */
struct memo
{
	struct afb_req request;	/* the recorded request */
	const char *method;	/* the called method */
};

/*
 * Creates the memo for the 'request' and the 'method'.
 * Returns the memo in case of success.
 * In case of error, send a failure answer and returns NULL.
 */
static struct memo *memo_create(struct afb_req request, const char *method)
{
	struct memo *memo = malloc(sizeof *memo);
	if (memo == NULL)
		afb_req_fail(request, "failed", "out of memory");
	else {
		memo->request = request;
		memo->method = method;
		afb_req_addref(request);
	}
	return memo;
}

/*
 * Sends the asynchronous failed reply to the request recorded by 'memo'
 * Then fress the resources.
 */
static void memo_fail(struct memo *memo, const char *info)
{
	afb_req_fail(memo->request, "failed", info);
	afb_req_unref(memo->request);
	free(memo);
}

/*
 * Sends the asynchronous success reply to the request recorded by 'memo'
 * Then fress the resources.
 */
static void memo_success(struct memo *memo, struct json_object *obj, const char *info)
{
	afb_req_success(memo->request, obj, info);
	afb_req_unref(memo->request);
	free(memo);
}

/*
 * Broadcast the event "application-list-changed".
 * This event is sent was the event "changed" is received from dbus.
 */
static void application_list_changed(const char *data, void *closure)
{
	afb_daemon_broadcast_event(binder->daemon, "application-list-changed", NULL);
}

/*
 * Builds if possible the json object having one field of name 'tag'
 * whose value is 'obj' like here: { tag: obj } and returns it.
 * In case of error or when 'tag'==NULL or 'obj'==NULL, 'obj' is returned.
 * The reference count of 'obj' is not incremented.
 */
static struct json_object *embed(const char *tag, struct json_object *obj)
{
	struct json_object *result;

	if (obj == NULL || tag == NULL)
		result = obj;
	else {
		result = json_object_new_object();
		if (result == NULL) {
			/* can't embed */
			result = obj;
		}
		else {
			/* TODO why is json-c not returning a status? */
			json_object_object_add(result, tag, obj);
		}
	}
	return result;
}

/*
 * Callback for replies made by 'embed_call_void'.
 */
static void embed_call_void_callback(int status, struct json_object *obj, struct memo *memo)
{
	DEBUG(binder, "(afm-main-binding) %s(true) -> %s\n", memo->method,
			obj ? json_object_to_json_string(obj) : "NULL");

	if (obj == NULL) {
		memo_fail(memo, "framework daemon failure");
	} else {
		memo_success(memo, embed(memo->method, json_object_get(obj)), NULL);
	}
}

/*
 * Calls with DBus the 'method' of the user daemon without arguments.
 */
static void embed_call_void(struct afb_req request, const char *method)
{
	struct memo *memo;

	/* store the request */
	memo = memo_create(request, method);
	if (memo == NULL)
		return;

	if (jbus_call_sj(jbus, method, "true", (void*)embed_call_void_callback, memo) < 0)
		memo_fail(memo, "dbus failure");
}

/*
 * Callback for replies made by 'call_appid' and 'call_runid'.
 */
static void call_xxxid_callback(int status, struct json_object *obj, struct memo *memo)
{
	DEBUG(binder, "(afm-main-binding) %s -> %s\n", memo->method, 
			obj ? json_object_to_json_string(obj) : "NULL");

	if (obj == NULL) {
		memo_fail(memo, "framework daemon failure");
	} else {
		memo_success(memo, json_object_get(obj), NULL);
	}
}

/*
 * Calls with DBus the 'method' of the user daemon with the argument "id".
 */
static void call_appid(struct afb_req request, const char *method)
{
	struct memo *memo;
	char *sid;
	const char *id;

	id = afb_req_value(request, _id_);
	if (id == NULL) {
		afb_req_fail(request, "bad-request", "missing 'id'");
		return;
	}

	memo = memo_create(request, method);
	if (memo == NULL)
		return;

	if (asprintf(&sid, "\"%s\"", id) <= 0) {
		memo_fail(memo, "out of memory");
		return;
	}

	if (jbus_call_sj(jbus, method, sid, (void*)call_xxxid_callback, memo) < 0)
		memo_fail(memo, "dbus failure");

	free(sid);
}

static void call_runid(struct afb_req request, const char *method)
{
	struct memo *memo;
	const char *id;

	id = afb_req_value(request, _runid_);
	if (id == NULL) {
		afb_req_fail(request, "bad-request", "missing 'runid'");
		return;
	}

	memo = memo_create(request, method);
	if (memo == NULL)
		return;

	if (jbus_call_sj(jbus, method, id, (void*)call_xxxid_callback, memo) < 0)
		memo_fail(memo, "dbus failure");
}

/************************** entries ******************************/

static void runnables(struct afb_req request)
{
	embed_call_void(request, _runnables_);
}

static void detail(struct afb_req request)
{
	call_appid(request, _detail_);
}

static void start_callback(int status, struct json_object *obj, struct memo *memo)
{
	DEBUG(binder, "(afm-main-binding) %s -> %s\n", memo->method, 
			obj ? json_object_to_json_string(obj) : "NULL");

	if (obj == NULL) {
		memo_fail(memo, "framework daemon failure");
	} else {
		obj = json_object_get(obj);
		if (json_object_get_type(obj) == json_type_int)
			obj = embed(_runid_, obj);
		memo_success(memo, obj, NULL);
	}
}

static void start(struct afb_req request)
{
	struct memo *memo;
	const char *id, *mode;
	char *query;
	int rc;

	/* get the id */
	id = afb_req_value(request, _id_);
	if (id == NULL) {
		afb_req_fail(request, "bad-request", "missing 'id'");
		return;
	}

	/* get the mode */
	mode = afb_req_value(request, _mode_);
	if (mode == NULL || !strcmp(mode, _auto_)) {
		mode = binder->mode == AFB_MODE_REMOTE ? _remote_ : _local_;
	}

	/* prepares asynchronous request */
	memo = memo_create(request, _start_);
	if (memo == NULL)
		return;

	/* create the query */
	rc = asprintf(&query, "{\"id\":\"%s\",\"mode\":\"%s\"}", id, mode);
	if (rc < 0) {
		memo_fail(memo, "out of memory");
		return;
	}

	/* calls the service asynchronously */
	if (jbus_call_sj(jbus, _start_, query, (void*)start_callback, memo) < 0)
		memo_fail(memo, "dbus failure");
	free(query);
}

static void terminate(struct afb_req request)
{
	call_runid(request, _terminate_);
}

static void stop(struct afb_req request)
{
	call_runid(request, _stop_);
}

static void continue_(struct afb_req request)
{
	call_runid(request, _continue_);
}

static void runners(struct afb_req request)
{
	embed_call_void(request, _runners_);
}

static void state(struct afb_req request)
{
	call_runid(request, _state_);
}

static void install_callback(int status, struct json_object *obj, struct memo *memo)
{
	struct json_object *added;

	if (obj == NULL) {
		memo_fail(memo, "framework daemon failure");
	} else {
		if (json_object_object_get_ex(obj, _added_, &added))
			obj = added;
		obj = json_object_get(obj);
		obj = embed(_id_, obj);
		memo_success(memo, obj, NULL);
	}
}
static void install(struct afb_req request)
{
	struct memo *memo;
	char *query;
	const char *filename;
	struct afb_arg arg;

	/* get the argument */
	arg = afb_req_get(request, "widget");
	filename = arg.path;
	if (filename == NULL) {
		afb_req_fail(request, "bad-request", "missing 'widget' file");
		return;
	}

	/* prepares asynchronous request */
	memo = memo_create(request, _install_);
	if (memo == NULL)
		return;

	/* makes the query */
	if (0 >= asprintf(&query, "\"%s\"", filename)) {
		afb_req_fail(request, "server-error", "out of memory");
		return;
	}

	/* calls the service asynchronously */
	if (jbus_call_sj(jbus, _install_, query, (void*)install_callback, memo) < 0)
		memo_fail(memo, "dbus failure");
	free(query);
}

static void uninstall(struct afb_req request)
{
	call_appid(request, _uninstall_);
}

static const struct afb_verb_desc_v1 verbs[] =
{
	{_runnables_, AFB_SESSION_CHECK, runnables,  "Get list of runnable applications"},
	{_detail_   , AFB_SESSION_CHECK, detail, "Get the details for one application"},
	{_start_    , AFB_SESSION_CHECK, start, "Start an application"},
	{_terminate_, AFB_SESSION_CHECK, terminate, "Terminate a running application"},
	{_stop_     , AFB_SESSION_CHECK, stop, "Stop (pause) a running application"},
	{_continue_ , AFB_SESSION_CHECK, continue_, "Continue (resume) a stopped application"},
	{_runners_  , AFB_SESSION_CHECK, runners,  "Get the list of running applications"},
	{_state_    , AFB_SESSION_CHECK, state, "Get the state of a running application"},
	{_install_  , AFB_SESSION_CHECK, install,  "Install an application using a widget file"},
	{_uninstall_, AFB_SESSION_CHECK, uninstall, "Uninstall an application"},
	{ NULL, 0, NULL, NULL }
};

static const struct afb_binding plug_desc = {
	.type = AFB_BINDING_VERSION_1,
	.v1 = {
		.info = "Application Framework Master Service",
		.prefix = "afm-main",
		.verbs = verbs
	}
};

const struct afb_binding *afbBindingV1Register(const struct afb_binding_interface *itf)
{
	int rc;
	struct sd_bus *sbus;

	/* records the interface */
	assert (binder == NULL);
	binder = itf;

	/* creates the jbus for accessing afm-user-daemon */
	sbus = afb_daemon_get_user_bus(itf->daemon);
	if (sbus == NULL)
		return NULL;
	jbus = create_jbus(sbus, "/org/AGL/afm/user");
        if (jbus == NULL)
		return NULL;

	/* records the signal handler */
	rc = jbus_on_signal_s(jbus, _changed_, application_list_changed, NULL);
	if (rc < 0) {
		jbus_unref(jbus);
		return NULL;
	}

	return &plug_desc;
}

