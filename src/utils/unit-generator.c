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

#include <errno.h>

#include <json-c/json.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-jsonc.h>

#include "unit-generator.h"
#include "unit-process.h"
#include "unit-oper.h"

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
 * Structure used by `internal_legacy` in its callback
 * for unit_process_raw
 */
struct for_legacy
{
	/** descriptor to pass in generatdesc */
	struct json_object *jdesc;
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
	const struct unitdesc *units,
	int nrunits
) {
	struct generatedesc gdesc;
	struct for_legacy *fleg = closure;
	int rc = 0;

	/* call the function that processes the units */
	if (fleg->process) {
		gdesc.nunits = nrunits;
		gdesc.units = units;
		gdesc.desc = fleg->jdesc;
		rc = fleg->process(fleg->closure, &gdesc);
	}

	return rc;
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
	else {
		struct for_legacy fleg = {
			.jdesc = jdesc,
			.process = process,
			.closure = closure
		};

		rc = unit_process_split(jdesc, internal_legacy, &fleg);
	}
	return rc;
}

/**************** SPECIALIZED PART *****************************/

static int do_uninstall_units(void *closure, const struct generatedesc *desc)
{
	int i, rc, rc2;

	rc = 0;
	for (i = 0 ; i < desc->nunits ; i++) {
		rc2 = unit_oper_uninstall(&desc->units[i]);
		if (rc2 < 0 && rc == 0)
			rc = rc2;
	}
	return rc;
}

static int do_install_units(void *closure, const struct generatedesc *desc)
{
	int i, rc;

	/* check that no file is overwritten by the installation */
	for (i = 0 ; i < desc->nunits ; i++) {
		rc = unit_oper_check_files(&desc->units[i], 0);
		if (rc < 0)
			return rc;
	}

	/* install the units */
	for (i = 0 ; i < desc->nunits ; i++) {
		rc = unit_oper_install(&desc->units[i]);
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
