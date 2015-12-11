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

#include <errno.h>
#include <syslog.h>
#include <string.h>

#include "verbose.h"
#include "wgtpkg.h"
#include "wgt.h"
#include "wgt-info.h"

static int check_temporary_constraints(const struct wgt_desc *desc)
{
	if (!desc->icons) {
		syslog(LOG_ERR, "widget has not icon defined (temporary constraints)");
		errno = EINVAL;
		return -1;
	}
	if (desc->icons->next) {
		syslog(LOG_ERR, "widget has more than one icon defined (temporary constraints)");
		errno = EINVAL;
		return -1;
	}
	if (!desc->content_src) {
		syslog(LOG_ERR, "widget has not content defined (temporary constraints)");
		errno = EINVAL;
		return -1;
	}
	if (!desc->content_type) {
		syslog(LOG_ERR, "widget has not type for its content (temporary constraints)");
		errno = EINVAL;
		return -1;
	}
	return 0;
}

static int check_permissions(const char *name, int required)
{
	if (permission_exists(name)) {
		if (request_permission(name)) {
			debug("granted permission: %s", name);
		} else if (required) {
			syslog(LOG_ERR, "ungranted permission required: %s", name);
			errno = EPERM;
			return 0;
		} else {
			notice("ungranted permission optional: %s", name);
		}
	}
	return 1;
}

static int check_widget(const struct wgt_desc *desc)
{
	int result;
	const struct wgt_desc_feature *feature;
	const char *name;

	result = check_temporary_constraints(desc);
	feature = desc->features;
	while(feature) {
		name = feature->name;
		if (0 == strcmp(name, AGLWIDGET)) {
			
		} else {
			if (!check_permissions(feature->name, feature->required))
				result = -1;
		}
		feature = feature->next;
	}
	return result;
}

static int place(const char *root, const char *appid, const char *version, int force)
{
	char newdir[PATH_MAX];
	int rc;

	rc = snprintf(newdir, sizeof newdir, "%s/%s/%s", root, appid, version);
	if (rc >= sizeof newdir) {
		syslog(LOG_ERR, "path to long: %s/%s/%s", root, appid, version);
		errno = EINVAL;
		return -1;
	}

	rc = move_workdir(newdir, 1, force);
	return rc;
}

/* install the widget of the file */
void install_widget(const char *wgtfile, const char *root, int force)
{
	struct wgt_info *ifo;
	const struct wgt_desc *desc;

	notice("-- INSTALLING widget %s --", wgtfile);

	/* workdir */
	if (make_workdir_base(root, "UNPACK", 0)) {
		syslog(LOG_ERR, "failed to create a working directory");
		goto error1;
	}

	if (zread(wgtfile, 0))
		goto error2;

	if (check_all_signatures())
		goto error2;

	ifo = wgt_info_createat(workdirfd, NULL, 1, 1, 1);
	if (!ifo)
		goto error2;

	desc = wgt_info_desc(ifo);
	if (check_widget(desc))
		goto error3;

/*
	if (check_and_place())
		goto error2;
*/	
	return;

error3:
	wgt_info_unref(ifo);

error2:
	remove_workdir();

error1:
	return;
}

