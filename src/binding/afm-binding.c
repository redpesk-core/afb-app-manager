/*
 * Copyright (C) 2015-2026 IoT.bzh Company
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

#define AFB_BINDING_VERSION 4
#include <afb/afb-binding.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "utils-systemd.h"
#include "afm-udb.h"
#include "afm-urun.h"
#include "wgt-info.h"

/*
 * constant strings
 */
static const char _all_[]       = "all";
static const char _applstchg_[] = "application-list-changed";
static const char _bad_request_[] = "bad-request";
static const char _cannot_start_[] = "cannot-start";
static const char _detail_[]    = "detail";
static const char _forbidden_[] = "forbidden";
static const char _id_[]	= "id";
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
static const char _uid_[]       = "uid";
static const char _update_[]    = "update";

/*
 * the permissions
 */
#define REDPESK_PREFIX "urn:redpesk:permission:afm:system:"

#define DEF_OR(name, case1, case2) \
		static const struct afb_auth name = { \
			.type = afb_auth_Or, \
			.first = &case1, \
			.next = &case2 };

#define DEF_PREFIX_PERM(name, prefix, suffix) \
		static const struct afb_auth name = { \
			.type = afb_auth_Permission, \
			.text = prefix suffix };

#define DEF_PERM(name, suffix) \
	DEF_PREFIX_PERM(name, REDPESK_PREFIX, suffix)

#if !defined(ADD_AGL_PERMISSIONS)
#define ADD_AGL_PERMISSIONS 1
#endif

#if ADD_AGL_PERMISSIONS

#  define AGL_PREFIX     "urn:AGL:permission:afm:system:"

#  undef DEF_PERM
#  define DEF_PERM(name, suffix) \
	DEF_PREFIX_PERM(name##_redpesk, REDPESK_PREFIX, suffix) \
	DEF_PREFIX_PERM(name##_agl, AGL_PREFIX, suffix) \
	DEF_OR(name, name##_redpesk, name##_agl)

#endif

DEF_PERM(auth_perm_widget,          "widget")
DEF_PERM(auth_perm_widget_detail,   "widget:detail")
DEF_PERM(auth_perm_widget_start,    "widget:start")
DEF_PERM(auth_perm_widget_view_all, "widget:view-all")
DEF_PERM(auth_perm_runner,          "runner")
DEF_PERM(auth_perm_runner_state,    "runner:state")
DEF_PERM(auth_perm_runner_kill,     "runner:kill")
DEF_PERM(auth_perm_set_uid,         "set-uid")

DEF_OR(auth_detail,   auth_perm_widget, auth_perm_widget_detail)
DEF_OR(auth_start,    auth_perm_widget, auth_perm_widget_start)
DEF_OR(auth_view_all, auth_perm_widget, auth_perm_widget_view_all)
DEF_OR(auth_state,    auth_perm_runner, auth_perm_runner_state)
DEF_OR(auth_kill,     auth_perm_runner, auth_perm_runner_kill)

/**
 * Enumerate the possible arguments
 * This is intended to be used as a mask of bits
 * telling what parameter is expected, optional,
 * and, finally, set.
 */
enum {
	Param_UID    = 1,  /* the UID is set for the request */
	Param_All    = 2,  /* get even hidden items */
	Param_Id     = 4,  /* the id of an application */
	Param_RunId  = 8   /* the pid of a process*/
};

/**
 * Enumerate the status code of parameter scanning
 */
enum {
	no_error = 0,
	error_bad_request = 1,
	error_not_found = 2,
	error_not_running = 3,
	error_forbidden = 4
};

/**
 * Records the parameters of verb queries
 */
struct params {
	/** status of the scan */
	int status;
	/** bit mask of the required param */
	unsigned required;
	/** bit mask of the given param */
	unsigned found;
	/** value of param 'all' if set */
	int all;
	/** value of param 'uid' if set */
	int uid;
	/** value of param 'runid' if set */
	int runid;
	/** value of param 'id' if set */
	const char *id;
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
 * creates the data handling the given JSON object
 */
static int json2data(afb_data_t *data, struct json_object *object)
{
	return afb_create_data_raw(data, AFB_PREDEFINED_TYPE_JSON_C,
			object, 0, (void*)json_object_put, object);
}

/* extract the json object of the request */
static struct json_object *get_json_object(afb_req_t req)
{
	afb_data_t data;
	int rc = afb_req_param_convert(req, 0, AFB_PREDEFINED_TYPE_JSON_C, &data);
	return rc == 0 ? afb_data_ro_pointer(data) : NULL;
}

/* reply json object */
static void reply_json_object(afb_req_t req, struct json_object *object)
{
	afb_data_t data;
	json2data(&data, object);
	afb_req_reply(req, 0, 1, &data);
}

/* reply an error code */
static void reply_error(afb_req_t req, const char *text, int errcode)
{
	afb_req_reply(req, errcode, 0, NULL);
}

/*
 * Sends the reply "true" to the request 'req' if 'status' is zero.
 * Otherwise, when 'status' is not zero replies the error string 'errstr'.
 */
static void reply_status(afb_req_t req, int status)
{
	if (status >= 0)
		reply_json_object(req, json_object_new_boolean(1));
	else
		reply_error(req, strerror(errno), AFB_ERRNO_INTERNAL_ERROR);
}

/* common bad request reply */
static void bad_request(afb_req_t req)
{
	RP_INFO("bad request verb %s: %s",
		afb_req_get_called_verb(req),
		json_object_to_json_string(get_json_object(req)));
	reply_error(req, _bad_request_, AFB_ERRNO_INVALID_REQUEST);
}

/* forbidden request reply */
static void forbidden_request(afb_req_t req)
{
	RP_INFO("forbidden request verb %s: %s",
		afb_req_get_called_verb(req),
		json_object_to_json_string(get_json_object(req)));
	reply_error(req, _forbidden_, AFB_ERRNO_FORBIDDEN);
}

/* common not found reply */
static void not_found(afb_req_t req)
{
	reply_error(req, _not_found_, AFB_ERRNO_NO_ITEM);
}

/* common not running reply */
static void not_running(afb_req_t req)
{
	reply_error(req, _not_running_, AFB_ERRNO_BAD_STATE);
}

/* common can't start reply */
static void cant_start(afb_req_t req)
{
	reply_error(req, _cannot_start_, AFB_ERRNO_INTERNAL_ERROR);
}

/* temporarily disable any permission */
static int afb_req_has_permission(afb_req_t req, const char *permission) { return 0; }

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
 * This event is sent when SIGHUP is received
 */
static void application_list_changed(const char *operation, const char *data)
{
	afb_data_t ada;
	struct json_object *e = NULL;
	rp_jsonc_pack(&e, "{ss ss}", "operation", operation, "data", data);
	json2data(&ada, e);
	afb_event_broadcast(applist_changed_event, 1, &ada);
}

/**
 * get the uid of the request
 */
static int get_req_uid(afb_req_t req)
{
#define NOBODY 99
	int uid = NOBODY, val;
	struct json_object *clifo, *juid;
	clifo = afb_req_get_client_info(req);
	if (clifo != NULL
	 && json_object_object_get_ex(clifo, "uid", &juid)
	 && json_object_is_type(juid, json_type_int)) {
		val = json_object_get_int(juid);
		if (val >= 0 && val <= 65534)
			uid = val;
	}
	return uid;
}

/**
 * common routine for getting parameters
 */
static void extract_params(
	afb_req_t req,
	unsigned required,
	unsigned optional,
	struct params *params
) {
	int id, status;
	struct json_object *args, *obj;
	unsigned found, expected;

	/* init */
	memset(params, 0, sizeof *params);
	params->required = required;
	expected = optional|required;
	status = no_error;
	found = 0;
	params->uid = get_req_uid(req);
	args = get_json_object(req);

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
	}

	/* args is a object value: inspect it */
	else if (json_object_is_type(args, json_type_object)) {
		/* get UID */
		if (json_object_object_get_ex(args, _uid_, &obj)) {
			if (!json_object_is_type(obj, json_type_int))
				status = error_bad_request;
			else {
				id = json_object_get_int(obj);
				if (id < 0)
					status = error_bad_request;
				else if (params->uid != id) {
					params->uid = id;
					found |= Param_UID;
				}
			}
		}

		/* get all */
		if ((expected & Param_All)
		&& json_object_object_get_ex(args, _all_, &obj)) {
			params->all = json_object_get_boolean(obj);
			if (params->all)
				found |= Param_All;
		}

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
					status = error_bad_request;
				else {
					params->runid = json_object_get_int(obj);
					found |= Param_RunId;
				}
			}
		}
	}
	params->status = status;
	params->found = found;
	params->args = args;
}

static void check_permissions(afb_req_t req, struct params *params)
{
	if (params->status == no_error) {
		/* check permissions */
		if (params->found & Param_UID) {
			if (!has_auth(req, &auth_perm_set_uid))
				params->status = error_forbidden;
		}
	}
	if (params->status == no_error) {
		if (params->found & Param_All) {
			if (!has_auth(req, &auth_view_all))
				params->status = error_forbidden;
		}
	}
}

static void check_runid(struct params *params)
{
	/* deduce the runid from the uid on need */
	if (params->status == no_error
	 && (params->required & Param_RunId)
	 && (params->found & (Param_RunId | Param_Id)) == Param_Id) {
		int id = afm_urun_search_runid(afudb, params->id, params->uid);
		if (id < 0)
			params->status = errno == ESRCH ? error_not_running : error_not_found;
		else {
			params->runid = id;
			params->found |= Param_RunId;
		}
	}
}

static int check_final_params(afb_req_t req, struct params *params)
{
	/* status check */
	if ((params->status == no_error)
	 && ((params->required & params->found) == params->required))
		return 1;

	/* error reported */
	switch(params->status) {
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

static int get_params(
	afb_req_t req,
	unsigned required,
	unsigned optional,
	struct params *params
) {
	extract_params(req, required, optional, params);
	check_permissions(req, params);
	check_runid(params);
	return check_final_params(req, params);
}


static void with_params(afb_req_t req, unsigned mandatory, unsigned optional,
		void (*action)(afb_req_t req, const struct params *params))
{
	struct params params;
	if (get_params(req, mandatory, optional, &params))
		action(req, &params);
}

/*
 * On query "runnables"
 */
static void a_runnables(afb_req_t req, const struct params *params)
{
	struct json_object *resp;

	/* get the applications */
	resp = afm_udb_applications_public(afudb, params->all, params->uid);
	reply_json_object(req, resp);
}

static void v_runnables(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, 0, Param_All, a_runnables);
}

/*
 * On query "detail"
 */
static void a_detail(afb_req_t req, const struct params *params)
{
	struct json_object *resp;

	/* get the details */
	resp = afm_udb_get_application_public(afudb, params->id, params->uid);
	if (resp)
		reply_json_object(req, resp);
	else
		not_found(req);
}

static void v_detail(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_Id, 0, a_detail);
}

/*
 * On query "start"
 */
static void a_start(afb_req_t req, const struct params *params)
{
	struct json_object *appli, *resp;
	int runid;

	/* get the application */
	appli = afm_udb_get_application_private(afudb, params->id, params->uid);
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_start(appli, params->uid);
	if (runid < 0) {
		cant_start(req);
		return;
	}

	/* returns */
	resp = NULL;
	if (runid)
		rp_jsonc_pack(&resp, "i", runid);
	reply_json_object(req, resp);
}

static void v_start(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_Id, 0, a_start);
}

/*
 * On query "once"
 */
static void a_once(afb_req_t req, const struct params *params)
{
	struct json_object *appli, *resp;
	int runid;

	/* get the application */
	appli = afm_udb_get_application_private(afudb, params->id, params->uid);
	if (appli == NULL) {
		not_found(req);
		return;
	}

	/* launch the application */
	runid = afm_urun_once(appli, params->uid);
	if (runid < 0) {
		cant_start(req);
		return;
	}

	/* returns the state */
	resp = runid ? afm_urun_state(afudb, runid, params->uid) : NULL;
	reply_json_object(req, resp);
}

static void v_once(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_Id, 0, a_once);
}

/*
 * On query "pause"
 */
static void a_pause(afb_req_t req, const struct params *params)
{
	int status = afm_urun_pause(params->runid, params->uid);
	reply_status(req, status);
}

static void v_pause(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_RunId, 0, a_pause);
}

/*
 * On query "resume" from 'smsg' with parameters of 'obj'.
 */
static void a_resume(afb_req_t req, const struct params *params)
{
	int status = afm_urun_resume(params->runid, params->uid);
	reply_status(req, status);
}

static void v_resume(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_RunId, 0, a_resume);
}

/*
 * On query "terminate"
 */
static void a_terminate(afb_req_t req, const struct params *params)
{
	int status = afm_urun_terminate(params->runid, params->uid);
	reply_status(req, status);
}

static void v_terminate(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_RunId, 0, a_terminate);
}

/*
 * On query "runners"
 */
static void a_runners(afb_req_t req, const struct params *params)
{
	struct json_object *resp = afm_urun_list(afudb, params->all, params->uid);
	reply_json_object(req, resp);
}

static void v_runners(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, 0, Param_All, a_runners);
}

/*
 * On query "state"
 */
static void a_state(afb_req_t req, const struct params *params)
{
	struct json_object *resp = afm_urun_state(afudb, params->runid, params->uid);
	if (resp != NULL)
		reply_json_object(req, resp);
	else
		reply_error(req, NULL, AFB_ERRNO_INTERNAL_ERROR);
}

static void v_state(afb_req_t req, unsigned nargs, afb_data_t const *args)
{
	with_params(req, Param_RunId, 0, a_state);
}

static void onsighup(int signal)
{
	afm_udb_update(afudb);
	application_list_changed(_update_, _update_);
}

static int init(afb_api_t api)
{
	/* init database */
	afudb = afm_udb_create(1, 0, "afm-");
	if (!afudb) {
		RP_ERROR("afm_udb_create failed");
		return -1;
	}

	signal(SIGHUP, onsighup);

	/* create the event */
	return afb_api_new_event(api, _applstchg_, &applist_changed_event);
}

static int mainctl(afb_api_t api, afb_ctlid_t ctlid, afb_ctlarg_t ctlarg, void *userdata)
{
	return ctlid == afb_ctlid_Init ? init(api) : 0;
}

static const afb_verb_t verbs[] =
{
	{.verb=_runnables_, .callback=v_runnables, .auth=&auth_detail,    .info="Get list of runnable applications",          .session=AFB_SESSION_CHECK },
	{.verb=_detail_   , .callback=v_detail,    .auth=&auth_detail,    .info="Get the details for one application",        .session=AFB_SESSION_CHECK },
	{.verb=_start_    , .callback=v_start,     .auth=&auth_start,     .info="Start an application",                       .session=AFB_SESSION_CHECK },
	{.verb=_once_     , .callback=v_once,      .auth=&auth_start,     .info="Start once an application",                  .session=AFB_SESSION_CHECK },
	{.verb=_terminate_, .callback=v_terminate, .auth=&auth_kill,      .info="Terminate a running application",            .session=AFB_SESSION_CHECK },
	{.verb=_pause_    , .callback=v_pause,     .auth=&auth_kill,      .info="Pause a running application",                .session=AFB_SESSION_CHECK },
	{.verb=_resume_   , .callback=v_resume,    .auth=&auth_kill,      .info="Resume a paused application",                .session=AFB_SESSION_CHECK },
	{.verb=_runners_  , .callback=v_runners,   .auth=&auth_state,     .info="Get the list of running applications",       .session=AFB_SESSION_CHECK },
	{.verb=_state_    , .callback=v_state,     .auth=&auth_state,     .info="Get the state of a running application",     .session=AFB_SESSION_CHECK },
	{.verb=NULL }
};

const afb_binding_t afbBindingExport = {
	.api = "afm-main",
	.specification = NULL,
	.info = "Application Framework Master Service",
	.verbs = verbs,
	.mainctl = mainctl,
	.noconcurrency = 1 /* relies on binder for serialization of requests */
};

