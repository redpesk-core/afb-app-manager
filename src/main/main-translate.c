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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <json-c/json.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>
#include <rp-utils/rp-jsonc.h>
#include <rp-utils/rp-yaml.h>

#include <manifest.h>
#include <apply-mustach.h>
#include <normalize-unit-file.h>

static void usage(const char *name)
{
	name = strrchr(name, '/') == NULL ? name : 1 + strrchr(name, '/');
	printf("usage: %s [-t template] manifest [meta...]\n", name);
	printf("(default template is %s)\n", FWK_UNIT_CONF);
	exit(EXIT_FAILURE);
}

static void mergadd(struct json_object *dest, const char *name, struct json_object *obj)
{
	struct json_object *fld;

	if (json_object_object_get_ex(dest, name, &fld))
		rp_jsonc_object_merge(fld, obj, rp_jsonc_merge_option_replace);
	else
		json_object_object_add(dest, name, rp_jsonc_clone(obj));
}

static void add_metadata(struct json_object *manif, struct json_object *meta)
{
	struct json_object *metaglob;
	struct json_object *metatarg, *manitarg, *name, *targ, *mtar;
	unsigned idx, length;

	/* if global metadata is given */
	if (json_object_object_get_ex(meta, "#metadata", &metaglob))
		mergadd(manif, "#metadata", metaglob);

	/* if target metadata is given */
	if (json_object_object_get_ex(meta, "#metatarget", &metatarg)
	 && json_object_object_get_ex(manif, MANIFEST_TARGETS, &manitarg)) {
		/* iterate over targets */
		length = (unsigned)json_object_array_length(manitarg);
		for (idx = 0 ; idx < length ; idx++) {
			targ = json_object_array_get_idx(manitarg, idx);
			if (json_object_object_get_ex(targ,
						MANIFEST_SHARP_TARGET, &name)
			 && json_object_is_type(name, json_type_string)
			 && json_object_object_get_ex(metatarg,
				json_object_get_string(name), &mtar))
				mergadd(targ, "#metatarget", mtar);
		}
	}
}

int main(int ac, char **av)
{
	int rc, idx;
	struct json_object *manif, *meta;
	const char *ftempl;
	char *templ, *prod;
	size_t szprod;

	/* compute template name */
	if (ac > 1 && strcmp(av[1], "-t") != 0) {
		idx = 1;
		ftempl = FWK_UNIT_CONF;
	}
	else {
		idx = 3;
		ftempl = av[2];
	}

	/* check argument count */
	if (ac < idx)
		usage(av[0]);

	/* read template */
	rc = rp_file_get(ftempl, &templ, NULL);
	if (rc < 0) {
		fprintf(stderr, "can't read template file %s: %s", ftempl, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* read the manifest */
	manif = NULL;
	rc = manifest_read_and_check(&manif, av[idx]);
	if (rc < 0) {
		fprintf(stderr, "can't read manifest file %s: %s", av[idx], strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* read meta data of the manifest and merge them to the manifest */
	while (++idx < ac) {
		meta = NULL;
		rc = rp_yaml_path_to_json_c(&meta, av[idx], NULL);
		if (rc < 0) {
			fprintf(stderr, "can't read meta file %s: %s", av[idx], strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (meta != NULL)
			add_metadata(manif, meta);
	}

	/* process mustach templating now */
	rc = apply_mustach(templ, manif, &prod, &szprod);
	if (rc < 0) {
		fprintf(stderr, "expansion of template failed");
		exit(EXIT_FAILURE);
	}

	/* normalize the result */
	normalize_unit_file(prod);
	fputs(prod, stdout);

	return EXIT_SUCCESS;
}


