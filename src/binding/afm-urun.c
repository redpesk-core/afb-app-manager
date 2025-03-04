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

#define _GNU_SOURCE

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#include <json-c/json.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "utils-dir.h"
#include "utils-json.h"
#include "utils-systemd.h"
#include "afm-udb.h"
#include "afm-urun.h"

static const char key_unit_d_path[] = "-unit-dpath-";

/**************** get appli basis *********************/

static int get_basis(struct json_object *appli, int *isuser, const char **dpath, int uid)
{
	char userid[40];
	char *dp, *arodot, *nun;
	const char *uname, *uscope;
	struct json_object *odp;
	int rc;

	/* get the scope */
	if (!j_read_string_at(appli, "unit-scope", &uscope)) {
		RP_ERROR("'unit-scope' missing in appli description %s", json_object_get_string(appli));
		goto inval;
	}
	*isuser = strcmp(uscope, "system") != 0;

	/* get dpaths of known users */
	odp = NULL;
	if (json_object_object_get_ex(appli, key_unit_d_path, &odp)) {
		/* try not parametric dpath */
		if (json_object_get_type(odp) == json_type_string) {
			*dpath = json_object_get_string(odp);
			return 0;
		}
		assert(json_object_get_type(odp) == json_type_object);
		/* get userid */
		if (uid < 0) {
			RP_ERROR("unexpected uid %d", uid);
			goto inval;
		}
		rc = snprintf(userid, sizeof userid, "%d", uid);
		assert(rc < (int)(sizeof userid));
		/* try dpath for the user */
		if (j_read_string_at(odp, userid, dpath))
			return 0;
	}

	/* get uname */
	if (!j_read_string_at(appli, "unit-name", &uname)) {
		RP_ERROR("'unit-name' missing in appli description %s", json_object_get_string(appli));
		goto inval;
	}

	/* is user parametric? */
	arodot = strchr(uname, '@');
	if (arodot && *++arodot == '.') {
		if (!odp) {
			/* get userid */
			if (uid < 0) {
				RP_ERROR("unexpected uid %d", uid);
				goto inval;
			}
			rc = snprintf(userid, sizeof userid, "%d", uid);
			assert(rc < (int)(sizeof userid));

			/* create the dpaths of known users */
			odp = json_object_new_object();
			if (!odp)
				goto nomem;
			json_object_object_add(appli, key_unit_d_path, odp);
		}

		/* get dpath of userid */
		nun = alloca((size_t)(arodot - uname) + strlen(userid) + strlen(arodot) + 1);
		stpcpy(stpcpy(stpncpy(nun, uname, (size_t)(arodot - uname)), userid), arodot);
		dp = systemd_unit_dpath_by_name(*isuser, nun, 1);
		if (dp == NULL) {
			RP_ERROR("Can't load unit of name %s for %s: %m", nun, uscope);
			goto error;
		}
		/* record the dpath */
		if (!j_add_string(odp, userid, dp)) {
			free(dp);
			goto nomem;
		}
		free(dp);
		j_read_string_at(odp, userid, dpath);
	} else {
		/* get dpath */
		dp = systemd_unit_dpath_by_name(*isuser, uname, 1);
		if (dp == NULL) {
			RP_ERROR("Can't load unit of name %s for %s: %m", uname, uscope);
			goto error;
		}
		/* record the dpath */
		if (!j_add_string(appli, key_unit_d_path, dp)) {
			free(dp);
			goto nomem;
		}
		free(dp);
		j_read_string_at(appli, key_unit_d_path, dpath);
	}
	return 0;

nomem:
	RP_ERROR("out of memory");
	errno = ENOMEM;
	goto error;

inval:
	errno = EINVAL;
error:
	return -1;
}

static enum SysD_State wait_state_stable(int isuser, const char *dpath, char *job)
{
	int trial;
	enum SysD_State state = SysD_State_INVALID;
	struct timespec tispec;
	const int period_ms = 10;
	const int trial_s = 10;
	const int trial_count = (trial_s * 1000) / period_ms;
	const int period_ns = period_ms * 1000000;

	for (trial = 1 ; trial <= trial_count ; trial++) {
		if (job) {
			if (!systemd_job_is_pending(isuser, job))
				job = NULL;
		}
		if (!job) {
			state = systemd_unit_state_of_dpath(isuser, dpath);
			switch (state) {
			case SysD_State_Active:
			case SysD_State_Failed:
			case SysD_State_Inactive:
				return state;
			default:
				break;
			}
		}
		tispec.tv_sec = 0;
		tispec.tv_nsec = period_ns;
		nanosleep(&tispec, NULL);
	}
	return state;
}

/*
 * Creates a json object that describes the state for:
 *  - 'id', the id of the application
 *  - 'runid', its runid
 *  - 'pid', its pid
 *  - 'state', its systemd state
 *
 * Returns the created object or NULL in case of error.
 */
static json_object *mkstate(const char *id, int runid, int pid, enum SysD_State state)
{
	struct json_object *result, *pids;

	/* the structure */
	result = json_object_new_object();
	if (result == NULL)
		goto error;

	/* the runid */
	if (!j_add_integer(result, "runid", runid))
		goto error;

	/* the pids */
	if (pid > 0) {
		pids = j_add_new_array(result, "pids");
		if (!pids)
			goto error;
		if (!j_add_integer(pids, NULL, pid))
			goto error;
	}

	/* the state */
	if (!j_add_string(result, "state", state == SysD_State_Active ? "running" : "terminated"))
		goto error;

	/* the application id */
	if (!j_add_string(result, "id", id))
		goto error;

	/* done */
	return result;

error:
	json_object_put(result);
	errno = ENOMEM;
	return NULL;
}

/**************** API handling ************************/

/*
 * Starts the application described by 'appli' for the 'mode'.
 * In case of remote start, it returns in uri the uri to
 * connect to.
 *
 * A reference to 'appli' is kept during the live of the
 * runner. This is made using json_object_get. Thus be aware
 * that further modifications to 'appli' might create errors.
 *
 * Returns the runid in case of success or -1 in case of error
 */
int afm_urun_start(struct json_object *appli, int uid)
{
	return afm_urun_once(appli, uid);
}

/*
 * Returns the runid of a previously started application 'appli'
 * or if none is running, starts the application described by 'appli'
 * in local mode.
 *
 * A reference to 'appli' is kept during the live of the
 * runner. This is made using json_object_get. Thus be aware
 * that further modifications to 'appli' might create errors.
 *
 * Returns the runid in case of success or -1 in case of error
 */
int afm_urun_once(struct json_object *appli, int uid)
{
	const char *udpath, *uscope, *uname;
	enum SysD_State state;
	int rc, isuser;
	char *job;

	job = NULL;

	/* retrieve basis */
	rc = get_basis(appli, &isuser, &udpath, uid);
	if (rc < 0)
		goto error;

	/* start the unit */
	rc = systemd_unit_start_dpath(isuser, udpath, &job);
	if (rc < 0) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		RP_ERROR("can't start %s unit %s for uid %d", uscope, uname, uid);
		goto error;
	}

	state = wait_state_stable(isuser, udpath, job);
	switch (state) {
	case SysD_State_Active:
	case SysD_State_Inactive:
		break;
	case SysD_State_Failed:
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		RP_ERROR("start error %s unit %s for uid %d: %s", uscope, uname, uid,
							systemd_name_of_state(state));
		goto error;
	default:
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		RP_ERROR("can't wait %s unit %s for uid %d: %m", uscope, uname, uid);
		goto error;
	}

	rc = systemd_unit_pid_of_dpath(isuser, udpath);
	if (rc < 0) {
		j_read_string_at(appli, "unit-scope", &uscope);
		j_read_string_at(appli, "unit-name", &uname);
		RP_ERROR("can't get pid of %s unit %s for uid %d: %m", uscope, uname, uid);
		goto error;
	}

	free(job);
	return rc;

error:
	free(job);
	return -1;
}

static int not_yet_implemented(const char *what)
{
	RP_ERROR("%s isn't yet implemented", what);
	errno = ENOTSUP;
	return -1;
}

/*
 * Terminates the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_terminate(int runid, int uid)
{
	int rc = systemd_unit_stop_pid(1 /* TODO: isuser? */, (unsigned)runid, NULL);
	if (rc < 0)
		rc = systemd_unit_stop_pid(0 /* TODO: isuser? */, (unsigned)runid, NULL);
	return rc < 0 ? rc : 0;
}

/*
 * Stops (aka pause) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_pause(int runid, int uid)
{
	return not_yet_implemented("pause");
}

/*
 * Continue (aka resume) the runner of 'runid'
 *
 * Returns 0 in case of success or -1 in case of error
 */
int afm_urun_resume(int runid, int uid)
{
	return not_yet_implemented("resume");
}

/*
 * Get the list of the runners.
 *
 * Returns the list or NULL in case of error.
 */
struct json_object *afm_urun_list(struct afm_udb *db, int all, int uid)
{
	rp_jsonc_index_t i, n;
	int isuser, pid;
	const char *udpath;
	const char *id;
	enum SysD_State state;
	struct json_object *desc;
	struct json_object *appli;
	struct json_object *apps;
	struct json_object *result;

	apps = NULL;
	result = json_object_new_array();
	if (result == NULL)
		goto error;

	apps = afm_udb_applications_private(db, all, uid);
	n = json_object_array_length(apps);
	for (i = 0 ; i < n ; i++) {
		appli = json_object_array_get_idx(apps, i);
		if (appli && get_basis(appli, &isuser, &udpath, uid) >= 0) {
			pid = systemd_unit_pid_of_dpath(isuser, udpath);
			if (pid > 0 && j_read_string_at(appli, "id", &id)) {
				state = systemd_unit_state_of_dpath(isuser, udpath);
				if (state == SysD_State_Active) {
					desc = mkstate(id, pid, pid, state);
					if (desc && json_object_array_add(result, desc) == -1) {
						RP_ERROR("can't add desc %s to result", json_object_get_string(desc));
						json_object_put(desc);
					}
				}
			}
		}
	}

error:
	json_object_put(apps);
	return result;
}

/*
 * Get the state of the runner of 'runid'.
 *
 * Returns the state or NULL in case of success
 */
struct json_object *afm_urun_state(struct afm_udb *db, int runid, int uid)
{
	rp_jsonc_index_t i, n;
	int isuser, pid, wasuser;
	char *dpath;
	const char *udpath;
	const char *id;
	enum SysD_State state;
	struct json_object *appli;
	struct json_object *apps;
	struct json_object *result;

	result = NULL;

	/* get the dpath */
	dpath = systemd_unit_dpath_by_pid(wasuser = 1, (unsigned)runid);
	if (!dpath)
		dpath = systemd_unit_dpath_by_pid(wasuser = 0, (unsigned)runid);
	if (!dpath) {
		errno = EINVAL;
		RP_WARNING("searched runid %d not found", runid);
	} else {
		/* search in the base */
		apps = afm_udb_applications_private(db, 1, uid);
		n = json_object_array_length(apps);
		for (i = 0 ; i < n ; i++) {
			appli = json_object_array_get_idx(apps, i);
			if (appli
			 && get_basis(appli, &isuser, &udpath, uid) >= 0
			 && !strcmp(dpath, udpath)
			 && j_read_string_at(appli, "id", &id)) {
				pid = systemd_unit_pid_of_dpath(isuser, udpath);
				state = systemd_unit_state_of_dpath(isuser, dpath);
				if (pid > 0 && state == SysD_State_Active)
					result = mkstate(id, runid, pid, state);
				goto end;
			}
		}
		errno = ENOENT;
		RP_WARNING("searched runid %d of dpath %s isn't an applications", runid, dpath);
end:
		json_object_put(apps);
		free(dpath);
	}

	return result;
}

/*
 * Search the runid, if any, of the application of 'id' for the user 'uid'.
 * Returns the pid (a positive not null number) or -1 in case of error.
 */
int afm_urun_search_runid(struct afm_udb *db, const char *id, int uid)
{
	int isuser, pid;
	const char *udpath;
	struct json_object *appli;

	appli = afm_udb_get_application_private(db, id, uid);
	if (!appli) {
		RP_NOTICE("Unknown appid %s", id);
		errno = ENOENT;
		pid = -1;
	} else if (get_basis(appli, &isuser, &udpath, uid) < 0) {
		pid = -1;
	} else {
		pid = systemd_unit_pid_of_dpath(isuser, udpath);
		if (pid == 0) {
			errno = ESRCH;
			pid = -1;
		}
	}
	return pid;
}

