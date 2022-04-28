/*
 * Copyright (C) 2015-2022 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
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

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "utils-systemd.h"
#include "afm-udb.h"
#include "afm-urun.h"
#include "wgt-info.h"

#if WITH_WIDGETS
# include "wgtpkg-install.h"
# include "wgtpkg-uninstall.h"
#endif

/*
 * constant strings
 */
static const char _all_[]       = "all";
static const char _a_l_c_[]     = "application-list-changed";
static const char _bad_request_[] = "bad-request";
static const char _cannot_start_[] = "cannot-start";
static const char _detail_[]    = "detail";
static const char _forbidden_[] = "forbidden";
static const char _force_[]     = "force";
static const char _id_[]	= "id";
static const char _lang_[]      = "lang";
static const char _not_found_[] = "not-found";
static const char _not_running_[] = "not-running";
static const char _once_[]      = "once";
static const char _pause_[]     = "pause";
static const char _reload_[]    = "reload";
static const char _resume_[]    = "resume";
static const char _root_[]      = "root";
static const char _runid_[]     = "runid";
static const char _runnables_[] = "runnables";
static const char _runners_[]   = "runners";
static const char _start_[]     = "start";
static const char _state_[]     = "state";
static const char _terminate_[] = "terminate";
static const char _uid_[]       = "uid";
static const char _update_[]    = "update";

/*
 * the permissions
 */
static const struct afb_auth
	auth_perm_widget = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget"
	},
	auth_perm_widget_detail = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget:detail"
	},
	auth_perm_widget_start = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget:start"
	},
	auth_perm_widget_view_all = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget:view-all"
	},
	auth_perm_runner = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:runner"
	},
	auth_perm_runner_state = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:runner:state"
	},
	auth_perm_runner_kill = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:runner:kill"
	},
	auth_perm_set_uid = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:set-uid"
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

#if WITH_WIDGETS
static const char _added_[]     = "added";
static const char _install_[]   = "install";
static const char _uninstall_[] = "uninstall";
static const char _wgt_[]       = "wgt";

static const struct afb_auth
	auth_perm_widget_install = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget:install"
	},
	auth_perm_widget_uninstall = {
		.type = afb_auth_Permission,
		.text = FWK_PREFIX"permission:afm:system:widget:uninstall"
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
	};

/*
 * default root
 */
static const char rootdir[] = FWK_APP_DIR;
#endif

/* add legacy widget's verbs if needed */
#if !WITH_WIDGETS && !defined(WITH_LEGACY_WIDGET_VERBS)
#  define WITH_LEGACY_WIDGET_VERBS 0
#endif
#if WITH_LEGACY_WIDGET_VERBS
static const char _install_[]   = "install";
static const char _uninstall_[] = "uninstall";
#endif

/**
 * Enumerate the possible arguments
 * This is intended to be used as a mask of bits
 * telling what parameter is expected, optional,
 * and, finally, set.
 */
enum {
	Param_Lang   = 1,
	Param_All    = 2,
	Param_Force  = 4,
	Param_Reload = 8,
	Param_Id     = 16,
	Param_RunId  = 32,
#if WITH_WIDGETS
	Param_WGT    = 64,
#endif
	Param_Root   = 128
};

/**
 * Records the parameters of verb queries
 */
struct params {
	/** bit mask of the given param */
	unsigned found;
	/** value of param 'all' if set */
	int all;
	/** value of param 'force' if set */
	int force;
	/** value of param 'reload' if set */
	int reload;
	/** value of param 'uid' if set */
	int uid;
	/** value of param 'runid' if set */
	int runid;
	/** value of param 'lang' if set */
	const char *lang;
	/** value of param 'id' if set */
	const char *id;
	/** value of param 'wgt' if set */
	const char *wgt;
	/** value of param 'root' if set */
	const char *root;
	/** object value of parameters */
	struct json_object *args;
};

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

/* common bad request reply */
static void bad_request(afb_req_t req)
{
	RP_INFO("bad request verb %s: %s",
		afb_req_get_called_verb(req),
		json_object_to_json_string(afb_req_json(req)));
	afb_req_fail(req, _bad_request_, NULL);
}

/* forbidden request reply */
static void forbidden_request(afb_req_t req)
{
	RP_INFO("forbidden request verb %s: %s",
		afb_req_get_called_verb(req),
		json_object_to_json_string(afb_req_json(req)));
	afb_req_fail(req, _forbidden_, NULL);
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

/* emulate missing function */
static int has_auth(afb_req_t req, const struct afb_auth *auth)
{
	switch (auth->type) {
	case afb_auth_Permission:
		return afb_req_has_permission(req, auth->text);
	case afb_auth_Or:
		return has_auth(req, auth->first) || has_auth(req, auth->next);
	case afb_auth_And:
		return has_auth(req, auth->first) && has_auth(req, auth->next);
	case afb_auth_Not:
		return !has_auth(req, auth->first);
	case afb_auth_Yes:
		return 1;
	case afb_auth_No:
	case afb_auth_Token:
	case afb_auth_LOA:
	default:
		return 0;
	}
}

/*
 * Broadcast the event "application-list-changed".
 * This event is sent was the event "changed" is received from dbus.
 */
static void application_list_changed(const char *operation, const char *data)
{
	struct json_object *e = NULL;
	rp_jsonc_pack(&e, "{ss ss}", "operation", operation, "data", data);
	afb_event_broadcast(applist_changed_event, e);
}

/**
 * common routine for getting parameters
 */
static int get_params(afb_req_t req, unsigned mandatory, unsigned optional, struct params *params)
{
	enum {
		no_error = 0,
		error_bad_request = 1,
		error_not_found = 2,
		error_not_running = 3,
		error_forbidden = 4
	};

	int id, error;
	struct json_object *args, *obj;
	unsigned found, expected;

	/* init */
	expected = optional|mandatory;
	memset(params, 0, sizeof *params);
	error = no_error;
	found = 0;
	params->uid = afb_req_get_uid(req);
	args = afb_req_json(req);

	/* args is a numeric value: a run id */
	if (json_object_is_type(args, json_type_int)) {
		if (expected & Param_RunId) {
			params->runid = json_object_get_int(args);
			found |= Param_RunId;
		}
	}

	/* args is a string value: either an ID or a widget path */
	else if (json_object_is_type(args, json_type_string)) {
		if (expected & (Param_Id | Param_RunId)) {
			params->id = json_object_get_string(args);
			found |= Param_Id;
		}
#if WITH_WIDGETS
		else if (expected & Param_WGT) {
			params->wgt = json_object_get_string(args);
			found |= Param_WGT;
		}
#endif
	}

	/* args is a object value: inspect it */
	else if (json_object_is_type(args, json_type_object)) {
		/* get UID */
		if (json_object_object_get_ex(args, _uid_, &obj)) {
			if (!json_object_is_type(obj, json_type_int))
				error = error_bad_request;
			else {
				id = json_object_get_int(obj);
				if (id < 0)
					error = error_bad_request;
				else if (params->uid != id) {
					if (!afb_req_has_permission(req, auth_perm_set_uid.text))
						error = error_forbidden;
					else {
						params->uid = id;
					}
				}
			}
		}

		/* get all */
		if ((expected & Param_All)
		&& json_object_object_get_ex(args, _all_, &obj)) {
			params->all = json_object_get_boolean(obj);
			if (params->all && !has_auth(req, &auth_view_all))
				error = error_forbidden;
			else
				found |= Param_All;
		}

		/* get force */
		if ((expected & Param_Force)
		&& json_object_object_get_ex(args, _force_, &obj)) {
			params->force = json_object_get_boolean(obj);
			found |= Param_Force;
		}

		/* get reload */
		if ((expected & Param_Reload)
		&& json_object_object_get_ex(args, _reload_, &obj)) {
			params->reload = json_object_get_boolean(obj);
			found |= Param_Reload;
		}

		/* get languages */
		if ((expected & Param_Lang)
		&& json_object_object_get_ex(args, _lang_, &obj)) {
			params->lang = json_object_get_string(obj);
			found |= Param_Lang;
		}

		/* get root */
		if ((expected & Param_Root)
		&& json_object_object_get_ex(args, _root_, &obj)) {
			params->root = json_object_get_string(obj);
			found |= Param_Root;
		}

#if WITH_WIDGETS
		/* get WGT */
		if (expected & Param_WGT) {
			if (json_object_object_get_ex(args, _wgt_, &obj)) {
				params->wgt = json_object_get_string(obj);
				found |= Param_WGT;
			}
		}
#endif

		/* get appid */
		if (expected & (Param_Id | Param_RunId)) {
			if (json_object_object_get_ex(args, _id_, &obj)) {
				params->id = json_object_get_string(obj);
				found |= Param_Id;
			}
		}

		/* get runid */
		if (expected & Param_RunId) {
			if (json_object_object_get_ex(args, _runid_, &obj)) {
				if (!json_object_is_type(obj, json_type_int))
					error = error_bad_request;
				else {
					params->runid = json_object_get_int(obj);
					found |= Param_RunId;
				}
			}
		}
	}

	/* deduce the runid from the uid on need */
	if ((mandatory & Param_RunId) && !(found & Param_RunId) && (found & Param_Id)) {
		id = afm_urun_search_runid(afudb, params->id, params->uid);
		if (id > 0) {
			params->runid = id;
			found |= Param_RunId;
		}
		else if (errno == ESRCH)
			error = error_not_running;
		else
			error = error_not_found;
	}

	/* check all mandatory are here */
	if (error != no_error || (mandatory & found) != mandatory) {
		switch(error) {
		case error_not_found:
			not_found(req);
			break;
		case error_not_running:
			not_running(req);
			break;
		case error_forbidden:
			forbidden_request(req);
			break;
		case error_bad_request:
		default:
			bad_request(req);
			break;
		}
		return 0;
	}

	params->args = args;
	params->found = found;
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
	struct params params;
	struct json_object *resp;

	/* scan the request */
	if (!get_params(req, 0, Param_Lang|Param_All, &params))
		return;

	/* get the applications */
	resp = afm_udb_applications_public(afudb, params.all, params.uid, params.lang);
	afb_req_success(req, resp, NULL);
}

/*
 * On query "detail"
 */
static void detail(afb_req_t req)
{
	struct params params;
	struct json_object *resp;

	/* scan the request */
	if (!get_params(req, Param_Id, Param_Lang, &params))
		return;

	/* get the details */
	resp = afm_udb_get_application_public(afudb, params.id, params.uid, params.lang);
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
	struct params params;
	struct json_object *appli, *resp;
	int runid;

	/* scan the request */
	if (!get_params(req, Param_Id, 0, &params))
		return;

	/* get the application */
	appli = afm_udb_get_application_private(afudb, params.id, params.uid);
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_start(appli, params.uid);
	if (runid < 0) {
		cant_start(req);
		return;
	}

	/* returns */
	resp = NULL;
#if 0
	rp_jsonc_pack(&resp, "{si}", _runid_, runid);
#else
	if (runid)
		rp_jsonc_pack(&resp, "i", runid);
#endif
	afb_req_success(req, resp, NULL);
}

/*
 * On query "once"
 */
static void once(afb_req_t req)
{
	struct params params;
	struct json_object *appli, *resp;
	int runid;

	/* scan the request */
	if (!get_params(req, Param_Id, 0, &params))
		return;

	/* get the application */
	appli = afm_udb_get_application_private(afudb, params.id, params.uid);
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_once(appli, params.uid);
	if (runid < 0) {
		cant_start(req);
		return;
	}

	/* returns the state */
	resp = runid ? afm_urun_state(afudb, runid, params.uid) : NULL;
	afb_req_success(req, resp, NULL);
}

/*
 * On query "pause"
 */
static void pause(afb_req_t req)
{
	struct params params;
	int status;

	/* scan the request */
	if (get_params(req, Param_RunId, 0, &params)) {
		status = afm_urun_pause(params.runid, params.uid);
		reply_status(req, status);
	}
}

/*
 * On query "resume" from 'smsg' with parameters of 'obj'.
 */
static void resume(afb_req_t req)
{
	struct params params;
	int status;

	/* scan the request */
	if (get_params(req, Param_RunId, 0, &params)) {
		status = afm_urun_resume(params.runid, params.uid);
		reply_status(req, status);
	}
}

/*
 * On query "terminate"
 */
static void terminate(afb_req_t req)
{
	struct params params;
	int status;

	/* scan the request */
	if (get_params(req, Param_RunId, 0, &params)) {
		status = afm_urun_terminate(params.runid, params.uid);
		reply_status(req, status);
	}
}

/*
 * On query "runners"
 */
static void runners(afb_req_t req)
{
	struct params params;
	struct json_object *resp;

	/* scan the request */
	if (!get_params(req, 0, Param_All, &params))
		return;

	resp = afm_urun_list(afudb, params.all, params.uid);
	afb_req_success(req, resp, NULL);
}

/*
 * On query "state"
 */
static void state(afb_req_t req)
{
	struct params params;
	struct json_object *resp;

	/* scan the request */
	if (get_params(req, Param_RunId, 0, &params)) {
		resp = afm_urun_state(afudb, params.runid, params.uid);
		reply(req, resp);
	}
}

#if WITH_WIDGETS
/* enforce daemon reload */
static void do_reloads()
{
	systemd_daemon_reload(0);
	systemd_unit_restart_name(0, "sockets.target", NULL);
}

/*
 * On querying installation of widget(s)
 */
static void install(afb_req_t req)
{
	struct params params;
	struct wgt_info *ifo;
	struct json_object *resp;

	/* scan the request */
	if (!get_params(req, Param_WGT, Param_Root|Param_Force|Param_Reload, &params))
		return;

	/* check if force is allowed */
	if (params.force) {
		if (!has_auth(req, &auth_uninstall)) {
			forbidden_request(req);
			return;
		 }
	}

	/* supply default values */
	if (!(params.found & Param_Reload))
		params.reload = 1;
	if (!(params.found & Param_Root))
		params.root = rootdir;

	/* install the widget */
	ifo = install_widget(params.wgt, params.root, params.force);
	if (ifo == NULL)
		afb_req_fail_f(req, "failed", "installation failed: %m");
	else {
		afm_udb_update(afudb);
		/* reload if needed */
		if (params.reload)
			do_reloads();

		/* build the response */
		rp_jsonc_pack(&resp, "{ss}", _added_, wgt_info_desc(ifo)->idaver);
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
	struct params params;
	int rc;

	/* scan the request */
	if (!get_params(req, Param_Id, Param_Root|Param_Reload, &params))
		return;
	if (!(params.found & Param_Reload))
		params.reload = 1;
	if (!(params.found & Param_Root))
		params.root = rootdir;

	/* install the widget */
	rc = uninstall_widget(params.id, params.root);
	if (rc)
		afb_req_fail_f(req, "failed", "uninstallation failed: %m");
	else {
		afm_udb_update(afudb);
		afb_req_success(req, NULL, NULL);
		application_list_changed(_uninstall_, params.id);
	}
}
#endif

#if WITH_LEGACY_WIDGET_VERBS
static void __unimplemented_legacy__(afb_req_t req)
	{ afb_req_fail(req, "unimplemented-legacy", NULL); }
static void install(afb_req_t req) __attribute__((alias("__unimplemented_legacy__")));
static void uninstall(afb_req_t req) __attribute__((alias("__unimplemented_legacy__")));
#endif

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
	afudb = afm_udb_create(1, 0, "afm-");
	if (!afudb) {
		RP_ERROR("afm_udb_create failed");
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
#if WITH_WIDGETS
	{.verb=_install_  , .callback=install,   .auth=&auth_install,   .info="Install an application using a widget file", .session=AFB_SESSION_CHECK },
	{.verb=_uninstall_, .callback=uninstall, .auth=&auth_uninstall, .info="Uninstall an application",                   .session=AFB_SESSION_CHECK },
#endif
#if WITH_LEGACY_WIDGET_VERBS
	{.verb=_install_  , .callback=install,   .auth=NULL,            .info="Install a widget (legacy, unimplmented)",    .session=0 },
	{.verb=_uninstall_, .callback=uninstall, .auth=NULL,            .info="Install an application (legacy, unimplmented)", .session=0 },
#endif
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

