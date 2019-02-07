/*
 * Copyright (C) 2015-2019 "IoT.bzh"
 * Author José Bollo <jose.bollo@iot.bzh>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>

#include <json-c/json.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#include "verbose.h"
#include "utils-systemd.h"
#include "afm-udb.h"
#include "afm-urun.h"
#include "wgt-info.h"
#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"
#include "wrap-json.h"

/*
 * constant strings
 */
static const char _added_[]     = "added";
static const char _a_l_c_[]     = "application-list-changed";
static const char _bad_request_[] = "bad-request";
static const char _cannot_start_[] = "cannot-start";
static const char _detail_[]    = "detail";
static const char _id_[]        = "id";
static const char _install_[]   = "install";
static const char _lang_[]      = "lang";
static const char _not_found_[] = "not-found";
static const char _not_running_[] = "not-running";
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
static const char _update_[]    = "update";

/*
 * the permissions
 */
static const struct afb_auth
	auth_perm_widget = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget"
	},
	auth_perm_widget_install = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:install"
	},
	auth_perm_widget_uninstall = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:uninstall"
	},
	auth_perm_widget_preinstall = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:preinstall"
	},
	auth_perm_widget_detail = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:detail"
	},
	auth_perm_widget_start = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:start"
	},
	auth_perm_widget_view_all = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:widget:view-all"
	},
	auth_perm_runner = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:runner"
	},
	auth_perm_runner_state = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:runner:state"
	},
	auth_perm_runner_kill = {
		.type = afb_auth_Permission,
		.text = "urn:AGL:permission:afm:system:runner:kill"
	},

	auth_install = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_install
	},
	auth_uninstall = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_uninstall
	},
	auth_preinstall = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_preinstall
	},
	auth_detail = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_detail
	},
	auth_start = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_start
	},
	auth_view_all = {
		.type = afb_auth_Or,
		.first = &auth_perm_widget,
		.next = &auth_perm_widget_view_all
	},
	auth_state = {
		.type = afb_auth_Or,
		.first = &auth_perm_runner,
		.next = &auth_perm_runner_state
	},
	auth_kill = {
		.type = afb_auth_Or,
		.first = &auth_perm_runner,
		.next = &auth_perm_runner_kill
	}
;

/*
 * default root
 */
static const char *rootdir = FWK_APP_DIR;

/*
 * the internal application database
 */
static struct afm_udb *afudb;

/*
 * the event signaling that application list changed
 */
static afb_event_t applist_changed_event;

/*
 * the preallocated true json_object
 */
static struct json_object *json_true;

/* enforce daemon reload */
static void do_reloads()
{
	systemd_daemon_reload(0);
	systemd_unit_restart_name(0, "sockets.target");
}

/* common bad request reply */
static void bad_request(afb_req_t req)
{
	afb_req_fail(req, _bad_request_, NULL);
}

/* common not found reply */
static void not_found(afb_req_t req)
{
	afb_req_fail(req, _not_found_, NULL);
}

/* common not running reply */
static void not_running(afb_req_t req)
{
	afb_req_fail(req, _not_running_, NULL);
}

/* common can't start reply */
static void cant_start(afb_req_t req)
{
	afb_req_fail(req, _cannot_start_, NULL);
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
 * Retrieve the required language from 'req'.
 */
static const char *get_lang(afb_req_t req)
{
	const char *lang;

	/* get the optional language */
	lang = afb_req_value(req, _lang_);

	/* TODO use the req to get the lang of the session (if any) */

	return lang;
}


/*
 * retrieves the 'appid' in parameters received with the
 * request 'req' for the 'method'.
 *
 * Returns 1 in case of success.
 * Otherwise, if the 'appid' can't be retrieved, an error stating
 * the bad request is replied for 'req' and 0 is returned.
 */
static int onappid(afb_req_t req, const char *method, const char **appid)
{
	struct json_object *json;

	/* get the paramaters of the request */
	json = afb_req_json(req);

	/* get the appid if any */
	if (!wrap_json_unpack(json, "s", appid)
	 || !wrap_json_unpack(json, "{ss}", _id_, appid)) {
		/* found */
		INFO("method %s called for %s", method, *appid);
		return 1;
	}

	/* nothing appropriate */
	INFO("bad request method %s: %s", method,
					json_object_to_json_string(json));
	bad_request(req);
	return 0;
}

/*
 * retrieves the 'runid' in parameters received with the
 * request 'req' for the 'method'.
 *
 * Returns 1 in case of success.
 * Otherwise, if the 'runid' can't be retrieved, an error stating
 * the bad request is replied for 'req' and 0 is returned.
 */
static int onrunid(afb_req_t req, const char *method, int *runid)
{
	struct json_object *json;
	const char *appid;

	/* get the paramaters of the request */
	json = afb_req_json(req);

	/* get the runid if any */
	if (!wrap_json_unpack(json, "i", runid)
	 || !wrap_json_unpack(json, "{si}", _runid_, runid)) {
		INFO("method %s called for %d", method, *runid);
		return 1;
	}

	/* get the appid if any */
	if (!onappid(req, method, &appid))
		return 0;

	/* search the runid of the appid */
	*runid = afm_urun_search_runid(afudb, appid, afb_req_get_uid(req));
	if (*runid < 0) {
		/* nothing appropriate */
		INFO("method %s can't get runid for %s: %m", method,
							appid);
		if (errno == ESRCH)
			not_running(req);
		else
			not_found(req);
		return 0;
	}

	/* found */
	INFO("method %s called for %s -> %d", method, appid, *runid);
	return 1;
}

/*
 * Sends the reply 'resp' to the request 'req' if 'resp' is not NULLzero.
 * Otherwise, when 'resp' is NULL replies the error string 'errstr'.
 */
static void reply(afb_req_t req, struct json_object *resp)
{
	if (resp)
		afb_req_reply(req, resp, NULL, NULL);
	else
		afb_req_reply(req, NULL, "failed", strerror(errno));
}

/*
 * Sends the reply "true" to the request 'req' if 'status' is zero.
 * Otherwise, when 'status' is not zero replies the error string 'errstr'.
 */
static void reply_status(afb_req_t req, int status)
{
	reply(req, status ? NULL : json_object_get(json_true));
}

/*
 * On query "runnables"
 */
static void runnables(afb_req_t req)
{
	const char *lang;
	struct json_object *resp;

	/* get the language */
	lang = get_lang(req);

	/* get the details */
	resp = afm_udb_applications_public(afudb, afb_req_get_uid(req), lang);
	afb_req_success(req, resp, NULL);
}

/*
 * On query "detail"
 */
static void detail(afb_req_t req)
{
	const char *lang;
	const char *appid;
	struct json_object *resp;

	/* scan the request */
	if (!onappid(req, _detail_, &appid))
		return;

	/* get the language */
	lang = get_lang(req);

	/* wants details for appid */
	resp = afm_udb_get_application_public(afudb, appid, afb_req_get_uid(req), lang);
	if (resp)
		afb_req_success(req, resp, NULL);
	else
		not_found(req);
}

/*
 * On query "start"
 */
static void start(afb_req_t req)
{
	const char *appid;
	struct json_object *appli, *resp;
	int runid;

	/* scan the request */
	if (!onappid(req, _start_, &appid))
		return;

	/* get the application */
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
static void once(afb_req_t req)
{
	const char *appid;
	struct json_object *appli, *resp;
	int runid;

	/* scan the request */
	if (!onappid(req, _once_, &appid))
		return;

	/* get the application */
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
static void pause(afb_req_t req)
{
	int runid, status;
	if (onrunid(req, "pause", &runid)) {
		status = afm_urun_pause(runid, afb_req_get_uid(req));
		reply_status(req, status);
	}
}

/*
 * On query "resume" from 'smsg' with parameters of 'obj'.
 */
static void resume(afb_req_t req)
{
	int runid, status;
	if (onrunid(req, "resume", &runid)) {
		status = afm_urun_resume(runid, afb_req_get_uid(req));
		reply_status(req, status);
	}
}

/*
 * On query "terminate"
 */
static void terminate(afb_req_t req)
{
	int runid, status;
	if (onrunid(req, "terminate", &runid)) {
		status = afm_urun_terminate(runid, afb_req_get_uid(req));
		reply_status(req, status);
	}
}

/*
 * On query "runners"
 */
static void runners(afb_req_t req)
{
	struct json_object *resp;
	resp = afm_urun_list(afudb, afb_req_get_uid(req));
	afb_req_success(req, resp, NULL);
}

/*
 * On query "state"
 */
static void state(afb_req_t req)
{
	int runid;
	struct json_object *resp;
	if (onrunid(req, "state", &runid)) {
		resp = afm_urun_state(afudb, runid, afb_req_get_uid(req));
		reply(req, resp);
	}
}

/*
 * On querying installation of widget(s)
 */
static void install(afb_req_t req)
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

/*
 * On querying uninstallation of widget(s)
 */
static void uninstall(afb_req_t req)
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

static void onsighup(int signal)
{
	afm_udb_update(afudb);
	application_list_changed(_update_, _update_);
}

static int init(afb_api_t api)
{
	/* create TRUE */
	json_true = json_object_new_boolean(1);

	/* init database */
	afudb = afm_udb_create(1, 0, "afm-appli-");
	if (!afudb) {
		ERROR("afm_udb_create failed");
		return -1;
	}

	signal(SIGHUP, onsighup);

	/* create the event */
	applist_changed_event = afb_api_make_event(api, _a_l_c_);
	return -!afb_event_is_valid(applist_changed_event);
}

static const afb_verb_t verbs[] =
{
	{.verb=_runnables_, .callback=runnables, .auth=&auth_detail,    .info="Get list of runnable applications",          .session=AFB_SESSION_CHECK },
	{.verb=_detail_   , .callback=detail,    .auth=&auth_detail,    .info="Get the details for one application",        .session=AFB_SESSION_CHECK },
	{.verb=_start_    , .callback=start,     .auth=&auth_start,     .info="Start an application",                       .session=AFB_SESSION_CHECK },
	{.verb=_once_     , .callback=once,      .auth=&auth_start,     .info="Start once an application",                  .session=AFB_SESSION_CHECK },
	{.verb=_terminate_, .callback=terminate, .auth=&auth_kill,      .info="Terminate a running application",            .session=AFB_SESSION_CHECK },
	{.verb=_pause_    , .callback=pause,     .auth=&auth_kill,      .info="Pause a running application",                .session=AFB_SESSION_CHECK },
	{.verb=_resume_   , .callback=resume,    .auth=&auth_kill,      .info="Resume a paused application",                .session=AFB_SESSION_CHECK },
	{.verb=_runners_  , .callback=runners,   .auth=&auth_state,     .info="Get the list of running applications",       .session=AFB_SESSION_CHECK },
	{.verb=_state_    , .callback=state,     .auth=&auth_state,     .info="Get the state of a running application",     .session=AFB_SESSION_CHECK },
	{.verb=_install_  , .callback=install,   .auth=&auth_install,   .info="Install an application using a widget file", .session=AFB_SESSION_CHECK },
	{.verb=_uninstall_, .callback=uninstall, .auth=&auth_uninstall, .info="Uninstall an application",                   .session=AFB_SESSION_CHECK },
	{.verb=NULL }
};

const afb_binding_t afbBindingExport = {
	.api = "afm-main",
	.specification = NULL,
	.info = "Application Framework Master Service",
	.verbs = verbs,
	.preinit = NULL,
	.init = init,
	.onevent = NULL,
	.noconcurrency = 1 /* relies on binder for serialization of requests */
};

