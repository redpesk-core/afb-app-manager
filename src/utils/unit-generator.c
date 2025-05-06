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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <json-c/json.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>
#include <rp-utils/rp-jsonc.h>

#include "apply-mustach.h"
#include "utils-systemd.h"

#include "unit-generator.h"
#include "unit-utils.h"
#include "unit-process.h"

static const char string_targets[] = "targets";

static int add_metadata(struct json_object *jdesc, const struct unitconf *config)
{
	struct json_object *targets, *targ, *obj;
	int port, afid;
	rp_jsonc_index_t i, n;

	if (json_object_object_get_ex(jdesc, string_targets, &targets)) {
		n = json_object_array_length(targets);
		for (i = 0 ; i < n ; i++) {
			targ = json_object_array_get_idx(targets, i);
			if (!config->new_afid) {
				afid = 0;
				port = 0;
			} else {
				afid = config->new_afid();
				if (afid < 0)
					return afid;
				port = config->base_http_ports + afid;
			}
			if (!rp_jsonc_subobject(targ, "#metatarget", &obj)
			 || !rp_jsonc_add(obj, "afid", json_object_new_int(afid))
			 || !rp_jsonc_add(obj, "http-port", json_object_new_int(port)))
				return -1;
		}
	}

	if (config->metadata != NULL
	 && !rp_jsonc_add(jdesc, "#metadata", json_object_get(config->metadata)))
		return -1;
	return 0;
}

/**
 * Structure used by `unit_process_legacy` in its callback
 * for unit_process_raw
 */
struct for_legacy
{
	/** descriptor to pass in generatdesc */
	struct json_object *jdesc;
	/** config to pass in generatdesc */
	const struct unitconf *config;
	/** callback receiving the generatdesc */
	int (*process)(void *closure, const struct generatedesc *desc);
	/** closure for the callback */
	void *closure;
};

/*
 * Processes all the units of the 'corpus'.
 * Each unit of the corpus is separated and packed and its
 * charactistics are stored in a descriptor.
 * At the end if no error was found, calls the function 'process'
 * with its given 'closure' and the array descripbing the units.
 * Return 0 in case of success or a negative value in case of error.
 */
static
int internal_legacy(
	void *closure,
	char *corpus,
	size_t size
) {
	int rc, nru;
	struct generatedesc gdesc;
	struct unitdesc *units;
	struct for_legacy *fleg = closure;

	/* split the corpus */
	rc = nru = unit_corpus_split(corpus, &units);
	if (rc >= 0) {
		/* call the function that processes the units */
		if (rc > 0 && fleg->process) {
			gdesc.nunits = nru;
			gdesc.units = units;
			gdesc.conf = fleg->config;
			gdesc.desc = fleg->jdesc;
			rc = fleg->process(fleg->closure, &gdesc);
		}

		/* cleanup and frees */
		while(nru) {
			nru--;
			free((void*)(units[nru].name));
			free((void*)(units[nru].wanted_by));
		}
		free(units);
	}

	return rc;
}

/*
 * calls the process with splitted files from template instance using jdesc
 * and some other data
 */
int unit_process_legacy(
	struct json_object *jdesc,
	const struct unitconf *config,
	int (*process)(void *closure, const struct generatedesc *desc),
	void *closure
) {
	struct for_legacy fleg = {
			.jdesc = jdesc,
			.config = config,
			.process = process,
			.closure = closure
		};

	return unit_process_raw(jdesc, internal_legacy, &fleg);
}

/*
 * Applies the object 'jdesc' augmented of meta data coming
 * from 'config' to the current unit generator.
 * The current unit generator will be set to the default one if not unit
 * was previously set using the function 'unit_generator_open_template'.
 * The callback function 'process' is then called with the
 * unit descriptors array and the expected closure.
 * Return what returned process in case of success or a negative
 * error code.
 */
int unit_generator_process(
	struct json_object *jdesc,
	const struct unitconf *config,
	int (*process)(void *closure, const struct generatedesc *desc),
	void *closure
) {
	int rc = add_metadata(jdesc, config);
	if (rc)
		RP_ERROR("can't set the metadata. %m");
	else
		rc = unit_process_legacy(jdesc, config, process, closure);
	return rc;
}

/**************** SPECIALIZED PART *****************************/

/**
 * check the unit: verify that scope, type and name are set
 *
 * @param desc  unit description
 * @param tells output errors if existing
 *
 * @return 0 if the unit is valid or else -1
 */
static int check_unit_desc(const struct unitdesc *desc, int tells)
{
	if (desc->scope != unitscope_unknown && desc->type != unittype_unknown && desc->name != NULL)
		return 0;

	if (tells) {
		if (desc->scope == unitscope_unknown)
			RP_ERROR("unit of unknown scope");
		if (desc->type == unittype_unknown)
			RP_ERROR("unit of unknown type");
		if (desc->name == NULL)
			RP_ERROR("unit of unknown name");
	}
	errno = EINVAL;
	return -1;
}

static int get_unit_path(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = units_get_afm_unit_path(
			path, pathlen, desc->scope == unitscope_user,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		RP_ERROR("can't get the unit path for %s", desc->name);

	return rc;
}

static int get_wants_path(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = units_get_afm_wants_unit_path(
			path, pathlen, desc->scope == unitscope_user, desc->wanted_by,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		RP_ERROR("can't get the wants path for %s and %s", desc->name, desc->wanted_by);

	return rc;
}

static int get_wants_target(char *path, size_t pathlen, const struct unitdesc *desc)
{
	int rc = units_get_wants_target(
			path, pathlen,
			desc->name, desc->type == unittype_socket ? "socket" : "service");

	if (rc < 0)
		RP_ERROR("can't get the wants target for %s", desc->name);

	return rc;
}

static void stop_that_unit_cb(void *closure, struct SysD_ListUnitItem *lui)
{
	int rc, isuser;

	switch (systemd_state_of_name(lui->active_state)) {
	case SysD_State_Activating:
	case SysD_State_Active:
	case SysD_State_Reloading:
		isuser = (int)(intptr_t)closure;
		rc = systemd_unit_stop_dpath(isuser, lui->opath, 0);
		if (rc < 0)
			RP_ERROR("can't stop %s", lui->name);
		break;
	case SysD_State_INVALID:
	case SysD_State_Inactive:
	case SysD_State_Deactivating:
	case SysD_State_Failed:
	default:
		break;
	}
}

static int stop_unit(char *buffer, size_t buflen, const struct unitdesc *desc)
{
	int rc;
	int isuser = desc->scope == unitscope_user;
	char star[2] = { '*', 0 };

	rc = (int)strnlen(desc->name, buflen);
	rc = snprintf(buffer, buflen, "%s%s.%s",
		desc->name , &star[rc <= 0 || desc->name[rc - 1] != '@'],
		desc->type == unittype_socket ? "socket" : "service");

	rc = systemd_list_unit_pattern(isuser, buffer, stop_that_unit_cb, (void*)(intptr_t)isuser);
	if (rc < 0)
		RP_ERROR("can't get the unit path for %s", desc->name);

	return rc;
}


static int do_uninstall_units(void *closure, const struct generatedesc *desc)
{
	int rc, rc2;
	int i;
	char path[PATH_MAX];
	const struct unitdesc *u;

	rc = 0;
	for (i = 0 ; i < desc->nunits ; i++) {
		u = &desc->units[i];
		rc2 = check_unit_desc(u, 0);
		if (rc2 == 0) {
			stop_unit(path, sizeof path, u);
			rc2 = get_unit_path(path, sizeof path, u);
			if (rc2 >= 0) {
				rc2 = unlink(path);
			}
			if (rc2 < 0 && rc == 0)
				rc = rc2;
			if (u->wanted_by != NULL) {
				rc2 = get_wants_path(path, sizeof path, u);
				if (rc2 >= 0)
					rc2 = unlink(path);
			}
		}
		if (rc2 < 0 && rc == 0)
			rc = rc2;
	}
	return rc;
}

static int do_install_units(void *closure, const struct generatedesc *desc)
{
	int rc;
	int i;
	char path[PATH_MAX + 1], target[PATH_MAX + 1];
	const struct unitdesc *u;

	i = 0;
	while (i < desc->nunits) {
		u = &desc->units[i];
		rc = check_unit_desc(u, 1);
		if (!rc) {
			rc = get_unit_path(path, sizeof path, u);
			if (rc >= 0) {
				RP_INFO("installing unit %s", path);
				rc = rp_file_put(path, u->content, u->content_length);
				if (rc >= 0 && u->wanted_by != NULL) {
					rc = get_wants_path(path, sizeof path, u);
					if (rc >= 0) {
						rc = get_wants_target(target, sizeof target, u);
						if (rc >= 0) {
							unlink(path); /* TODO? check? */
							rc = symlink(target, path);
						}
					}
				}
				i++;
			}
		}
		if (rc < 0)
			goto error;
	}
	return 0;
error:
	i = errno;
	do_uninstall_units(closure, desc);
	errno = i;
	return rc;
}

/* installs the unit files accordingly to manifest and config */
int unit_generator_install(json_object *manifest, const struct unitconf *config)
{
	return unit_generator_process(manifest, config, do_install_units, NULL);
}

/* uninstalls the unit files accordingly to manifest and config */
int unit_generator_uninstall(json_object *manifest, const struct unitconf *config)
{
	return unit_generator_process(manifest, config, do_uninstall_units, NULL);
}
