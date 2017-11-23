/*
 * Copyright (C) 2015, 2016, 2017 IoT.bzh
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

#define AFB_BINDING_VERSION 2
#include <afb/afb-binding.h>

#include "verbose.h"
#include "utils-systemd.h"
#include "afm.h"
#include "afm-udb.h"
#include "afm-urun.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"
#include "wrap-json.h"

static const char _added_[]     = "added";
static const char _a_l_c_[]     = "application-list-changed";
static const char _detail_[]    = "detail";
static const char _id_[]        = "id";
static const char _install_[]   = "install";
static const char _not_found_[] = "not-found";
static const char _once_[]      = "once";
static const char _pause_[]     = "pause";
static const char _resume_[]    = "resume";
static const char _runid_[]     = "runid";
static const char _runnables_[] = "runnables";
static const char _runners_[]   = "runners";
static const char _start_[]     = "start";
static const char _state_[]     = "state";
static const char _terminate_[] = "terminate";
static const char _uninstall_[] = "uninstall";

static const char *rootdir = FWK_APP_DIR;
static struct afb_event applist_changed_event;
static struct afm_udb *afudb;
static struct json_object *json_true;

static void do_reloads()
{
	/* enforce daemon reload */
	systemd_daemon_reload(0);
	systemd_unit_restart_name(0, "sockets.target");
}

static void bad_request(struct afb_req req)
{
	afb_req_fail(req, "bad-request", NULL);
}

static void not_found(struct afb_req req)
{
	afb_req_fail(req, _not_found_, NULL);
}

static void cant_start(struct afb_req req)
{
	afb_req_fail(req, "cannot-start", NULL);
}

/*
 * Broadcast the event "application-list-changed".
 * This event is sent was the event "changed" is received from dbus.
 */
static void application_list_changed(const char *operation, const char *data)
{
	struct json_object *e = NULL;
	wrap_json_pack(&e, "{ss ss}", "operation", operation, "data", data);
	afb_event_broadcast(applist_changed_event, e);
}

/*
 * retrieves the 'runid' in parameters received with the
 * request 'req' for the 'method'.
 *
 * Returns 1 in case of success.
 * Otherwise, if the 'runid' can't be retrived, an error stating
 * the bad request is replied for 'req' and 0 is returned.
 */
static int onrunid(struct afb_req req, const char *method, int *runid)
{
	struct json_object *json;

	json = afb_req_json(req);
	if (wrap_json_unpack(json, "i", runid)
		&& wrap_json_unpack(json, "{si}", _runid_, runid)) {
		INFO("bad request method %s: %s", method,
					json_object_to_json_string(json));
		bad_request(req);
		return 0;
	}

	INFO("method %s called for %d", method, *runid);
	return 1;
}

/*
 * Sends the reply 'resp' to the request 'req' if 'resp' is not NULLzero.
 * Otherwise, when 'resp' is NULL replies the error string 'errstr'.
 */
static void reply(struct afb_req req, struct json_object *resp, const char *errstr)
{
	if (!resp)
		afb_req_fail(req, errstr, NULL);
	else
		afb_req_success(req, resp, NULL);
}

/*
 * Sends the reply "true" to the request 'req' if 'status' is zero.
 * Otherwise, when 'status' is not zero replies the error string 'errstr'.
 */
static void reply_status(struct afb_req req, int status, const char *errstr)
{
	reply(req, status ? NULL : json_object_get(json_true), errstr);
}

/*
 * On query "runnables"
 */
static void runnables(struct afb_req req)
{
	struct json_object *resp;
	INFO("method runnables called");
	resp = afm_udb_applications_public(afudb, afb_req_get_uid(req));
	afb_req_success(req, resp, NULL);
}

/*
 * On query "detail"
 */
static void detail(struct afb_req req)
{
	const char *appid;
	struct json_object *resp, *json;

	/* scan the request */
	json = afb_req_json(req);
	if (wrap_json_unpack(json, "s", &appid)
		&& wrap_json_unpack(json, "{ss}", _id_, &appid)) {
		bad_request(req);
		return;
	}

	/* wants details for appid */
	INFO("method detail called for %s", appid);
	resp = afm_udb_get_application_public(afudb, appid, afb_req_get_uid(req));
	if (resp)
		afb_req_success(req, resp, NULL);
	else
		not_found(req);
}

/*
 * On query "start"
 */
static void start(struct afb_req req)
{
	const char *appid;
	struct json_object *appli, *resp, *json;
	int runid;

	/* scan the request */
	json = afb_req_json(req);
	if (wrap_json_unpack(json, "s", &appid)
		&& wrap_json_unpack(json, "{ss}", _id_, &appid)) {
		bad_request(req);
		return;
	}

	/* get the application */
	INFO("method start called for %s", appid);
	appli = afm_udb_get_application_private(afudb, appid, afb_req_get_uid(req));
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_start(appli, afb_req_get_uid(req));
	if (runid <= 0) {
		cant_start(req);
		return;
	}

	/* returns */
	resp = NULL;
#if 0
	wrap_json_pack(&resp, "{si}", _runid_, runid);
#else
	wrap_json_pack(&resp, "i", runid);
#endif
	afb_req_success(req, resp, NULL);
}

/*
 * On query "once"
 */
static void once(struct afb_req req)
{
	const char *appid;
	struct json_object *appli, *resp, *json;
	int runid;

	/* scan the request */
	json = afb_req_json(req);
	if (wrap_json_unpack(json, "s", &appid)
		&& wrap_json_unpack(json, "{ss}", _id_, &appid)) {
		bad_request(req);
		return;
	}

	/* get the application */
	INFO("method once called for %s", appid);
	appli = afm_udb_get_application_private(afudb, appid, afb_req_get_uid(req));
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_once(appli, afb_req_get_uid(req));
	if (runid <= 0) {
		cant_start(req);
		return;
	}

	/* returns the state */
	resp = afm_urun_state(afudb, runid, afb_req_get_uid(req));
	afb_req_success(req, resp, NULL);
}

/*
 * On query "pause"
 */
static void pause(struct afb_req req)
{
	int runid, status;
	if (onrunid(req, "pause", &runid)) {
		status = afm_urun_pause(runid, afb_req_get_uid(req));
		reply_status(req, status, _not_found_);
	}
}

/*
 * On query "resume" from 'smsg' with parameters of 'obj'.
 */
static void resume(struct afb_req req)
{
	int runid, status;
	if (onrunid(req, "resume", &runid)) {
		status = afm_urun_resume(runid, afb_req_get_uid(req));
		reply_status(req, status, _not_found_);
	}
}

/*
 * On query "terminate"
 */
static void terminate(struct afb_req req)
{
	int runid, status;
	if (onrunid(req, "terminate", &runid)) {
		status = afm_urun_terminate(runid, afb_req_get_uid(req));
		reply_status(req, status, _not_found_);
	}
}

/*
 * On query "runners"
 */
static void runners(struct afb_req req)
{
	struct json_object *resp;
	INFO("method runners called");
	resp = afm_urun_list(afudb, afb_req_get_uid(req));
	afb_req_success(req, resp, NULL);
}

/*
 * On query "state"
 */
static void state(struct afb_req req)
{
	int runid;
	struct json_object *resp;
	if (onrunid(req, "state", &runid)) {
		resp = afm_urun_state(afudb, runid, afb_req_get_uid(req));
		reply(req, resp, _not_found_);
	}
}

static void install(struct afb_req req)
{
	const char *wgtfile;
	const char *root;
	int force;
	int reload;
	struct wgt_info *ifo;
	struct json_object *json;
	struct json_object *resp;

	/* default settings */
	root = rootdir;
	force = 0;
	reload = 1;

	/* scan the request */
	json = afb_req_json(req);
	if (wrap_json_unpack(json, "s", &wgtfile)
		&& wrap_json_unpack(json, "{ss s?s s?b s?b}",
				"wgt", &wgtfile,
				"root", &root,
				"force", &force,
				"reload", &reload)) {
		return bad_request(req);
	}

	/* install the widget */
	ifo = install_widget(wgtfile, root, force);
	if (ifo == NULL)
		afb_req_fail_f(req, "failed", "installation failed: %m");
	else {
		afm_udb_update(afudb);
		/* reload if needed */
		if (reload)
			do_reloads();

		/* build the response */
		wrap_json_pack(&resp, "{ss}", _added_, wgt_info_desc(ifo)->idaver);
		afb_req_success(req, resp, NULL);
		application_list_changed(_install_, wgt_info_desc(ifo)->idaver);

		/* clean-up */
		wgt_info_unref(ifo);
	}
}

static void uninstall(struct afb_req req)
{
	const char *idaver;
	const char *root;
	struct json_object *json;
	int rc;

	/* default settings */
	root = rootdir;

	/* scan the request */
	json = afb_req_json(req);
	if (wrap_json_unpack(json, "s", &idaver)
		&& wrap_json_unpack(json, "{ss s?s}",
				_id_, &idaver,
				"root", &root)) {
		return bad_request(req);
	}

	/* install the widget */
	rc = uninstall_widget(idaver, root);
	if (rc)
		afb_req_fail_f(req, "failed", "uninstallation failed: %m");
	else {
		afm_udb_update(afudb);
		afb_req_success(req, NULL, NULL);
		application_list_changed(_uninstall_, idaver);
	}
}

static int init()
{
	/* init database */
	afudb = afm_udb_create(1, 0, "afm-appli-");
	if (!afudb) {
		ERROR("afm_udb_create failed");
		return -1;
	}

	/* create TRUE */
	json_true = json_object_new_boolean(1);

	/* create the event */
	applist_changed_event = afb_daemon_make_event(_a_l_c_);
	return -!afb_event_is_valid(applist_changed_event);
}

static const struct afb_auth
	auth_install = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:install"
	},
	auth_uninstall = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:uninstall"
	}
;

static const struct afb_verb_v2 verbs[] =
{
	{_runnables_, runnables, NULL, "Get list of runnable applications",          AFB_SESSION_CHECK_V2 },
	{_detail_   , detail,    NULL, "Get the details for one application",        AFB_SESSION_CHECK_V2 },
	{_start_    , start,     NULL, "Start an application",                       AFB_SESSION_CHECK_V2 },
	{_once_     , once,      NULL, "Start once an application",                  AFB_SESSION_CHECK_V2 },
	{_terminate_, terminate, NULL, "Terminate a running application",            AFB_SESSION_CHECK_V2 },
	{_pause_    , pause,     NULL, "Pause a running application",                AFB_SESSION_CHECK_V2 },
	{_resume_   , resume,    NULL, "Resume a paused application",                AFB_SESSION_CHECK_V2 },
	{_runners_  , runners,   NULL, "Get the list of running applications",       AFB_SESSION_CHECK_V2 },
	{_state_    , state,     NULL, "Get the state of a running application",     AFB_SESSION_CHECK_V2 },
	{_install_  , install,   NULL, "Install an application using a widget file", AFB_SESSION_CHECK_V2 },
	{_uninstall_, uninstall, NULL, "Uninstall an application",                   AFB_SESSION_CHECK_V2 },
	{ NULL, NULL, NULL, NULL, 0 }
};

const struct afb_binding_v2 afbBindingV2 = {
	.api = "afm-main",
	.specification = NULL,
	.info = "Application Framework Master Service",
	.verbs = verbs,
	.preinit = NULL,
	.init = init,
	.onevent = NULL,
	.noconcurrency = 1 /* relies on binder for serialisation of requests */
};

