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
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <json-c/json.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>
#include <rp-utils/rp-jsonc.h>
#include <rp-utils/rp-yaml.h>

#include <manifest.h>
#include <apply-mustach.h>
#include <normalize-unit-file.h>
#include <unit-process.h>
#include <unit-utils.h>

static const char version[] = "0.2";

/*************************************************************************/
/* the legacy method is anterior to version 0.2 */
static void method_legacy(const char *ftempl, struct json_object *manif)
{
	int rc;
	char *templ, *prod;
	size_t szprod;

	/* read template */
	rc = rp_file_get(ftempl, &templ, NULL);
	if (rc < 0) {
		fprintf(stderr, "can't read template file %s: %s\n",
						ftempl, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* process mustach templating now */
	rc = apply_mustach(templ, manif, &prod, &szprod);
	if (rc < 0) {
		fprintf(stderr, "expansion of template failed\n");
		exit(EXIT_FAILURE);
	}

	/* normalize the result */
	normalize_unit_file(prod);
	fputs(prod, stdout);
}

/*************************************************************************/
/* the modern method is the standard output since version 0.2 */
static int modern_cb(void *closure, char *text, size_t size)
{
	fputs(text, stdout);
	return 0;
}

static void method_modern(const char *ftempl, struct json_object *manif)
{
	int rc;

	/* read template */
	rc = unit_process_open_template(ftempl);
	if (rc < 0) {
		fprintf(stderr, "can't read template file %s: %s\n",
						ftempl, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* process mustach templating now */
	rc = unit_process_raw(manif, modern_cb, NULL);
	if (rc < 0) {
		fprintf(stderr, "expansion of template failed\n");
		exit(EXIT_FAILURE);
	}
}

/*************************************************************************/
/* the split method is here since version 0.2 */

static int runcmdX(const char *argv[])
{
	int rc;
	pid_t pid = vfork();
	if (pid == 0) {
		execv(argv[0], (char**)argv);
		_exit(EXIT_FAILURE);
	}
	waitpid(pid, &rc, 0);
	return (WIFEXITED(rc) && WEXITSTATUS(rc) == 0) - 1;
}

static int runcmd2(const char *cmd, const char *arg1, const char *arg2)
{
	const char *argv[] = { cmd, arg1, arg2, NULL };
	return runcmdX(argv);
}

static int purgedir(const char *dir)
{
	return runcmd2("/usr/bin/rm", "-r", dir);
}

static int makedir(const char *dir)
{
	return runcmd2("/usr/bin/mkdir", "-p", dir);
}

static int makefiledir(char *path)
{
	int rc = 0;
	char *p = strrchr(path, '/');
	if (p) {
		*p = 0;
		rc = makedir(path);
		*p = '/';
	}
	return rc;
}

static int writefile(char *path, const char *content)
{
	int rc;
	rc = makefiledir(path);
	if (rc == 0)
		rc = open(path, O_WRONLY|O_CREAT,
			S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (rc >= 0) {
		size_t len = strlen(content);
		ssize_t wlen = write(rc, content, len);
		close(rc);
		rc = (wlen == (ssize_t)len) - 1;
	}
	return rc;
}

static int writelink(char *path, const char *content)
{
	int rc = makefiledir(path);
	return rc == 0 ? symlink(content, path) : rc;
}

static int split_cb(void *closure, const struct unitdesc *units, int nrunits)
{
	const struct unitdesc *u;
	const char *tmpdir = closure;
	char upath[2000], wpath[2000], targ[2000];
	const char *ext;
	int rc, idx, isuser;
	size_t ldir;

	ldir = strlen(tmpdir);
	strcpy(upath, tmpdir);
	strcpy(wpath, tmpdir);
	upath[ldir] = wpath[ldir] = '/';
	ldir++;
	for (idx = 0 ; idx < nrunits ; idx++) {
		u = &units[idx];
		if (u->name == NULL) {
			fprintf(stderr, "name error, a unit has no name");
			return -1;
		}
		if (u->type == unittype_unknown) {
			fprintf(stderr, "type error, unit %s has unknown type", u->name);
			return -1;
		}
		if (u->scope == unitscope_unknown) {
			fprintf(stderr, "scope error, unit %s has unknown scope", u->name);
			return -1;
		}

		isuser = u->scope == unitscope_user;
		ext = u->type == unittype_socket ? "socket" : "service";
		units_get_afm_unit_path(&upath[ldir], (sizeof upath) - ldir,
					isuser, u->name, ext);
		wpath[ldir] = targ[ldir] = 0;
		if (u->wanted_by) {
			units_get_afm_wants_unit_path(&wpath[ldir], (sizeof wpath) - ldir,
						isuser, u->wanted_by, u->name, ext);
			units_get_wants_target(targ, sizeof targ, u->name, ext);
		}

		rc = writefile(upath, u->content);
		if (rc < 0) {
			fprintf(stderr, "failed to write file %s\n", upath);
			return -1;
		}
		if (u->wanted_by) {
			rc = writelink(wpath, targ);
			if (rc < 0) {
				fprintf(stderr, "failed to write link %s\n", wpath);
				return -1;
			}
		}
	}

	return 0;
}

static void method_split(const char *ftempl, struct json_object *manif)
{
	int rc;
	char tempdir[50];
	const char *argv[8];

	/* read template */
	rc = unit_process_open_template(ftempl);
	if (rc < 0) {
		fprintf(stderr, "can't read template file %s: %s\n",
						ftempl, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* create temporary directory */
	strcpy(tempdir, "./@trXXXXXX");
	if (mkdtemp(tempdir) == NULL) {
		fprintf(stderr, "unable to create temporary directory: %s\n",
					strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* process mustach templating now */
	rc = unit_process_split(manif, split_cb, tempdir);
	if (rc < 0) {
		purgedir(tempdir);
		fprintf(stderr, "expansion of template failed\n");
		exit(EXIT_FAILURE);
	}

	argv[0] = "/usr/bin/tar";
	argv[1] = "-C";
	argv[2] = tempdir;
	argv[3] = "--owner=root:0";
	argv[4] = "--group=root:0";
	argv[5] = "-c";
	argv[6] = ".";
	argv[7] = NULL;

	rc = runcmdX(argv);
	purgedir(tempdir);
	if (rc < 0) {
		fprintf(stderr, "failed to create tar file\n");
		exit(EXIT_FAILURE);
	}
}

/*************************************************************************/

static void usage(const char *name)
{
	printf("usage: %s (-h | --help)\n", name);
	printf("  or   %s (-v | --version)\n", name);
	printf("  or   %s [OPTION...] manifest [meta...]\n", name);
	printf("where OPTION in:\n"
	       " -l        use legacy translation\n"
	       " -m        use modern translation\n"
	       " -s        use split translation (produce tar file)\n"
	       " -o FILE   names the output FILE\n"
	       " -t FILE   use the template FILE (default %s)\n"
	       " -u DIR    unit destination for split (default %s)\n",
		FWK_UNIT_CONF,
		units_set_root_dir(NULL));
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
	const char *ftempl, *me, *unitdir, *fileout;
	enum { Legacy, Modern, Split } method = Modern;

	/* name of current program */
	me = strrchr(av[0], '/') == NULL ? av[0] : 1 + strrchr(av[0], '/');

	/* default settings */
	ftempl = FWK_UNIT_CONF;
	fileout = unitdir = NULL;

	/* scan options */
	while((rc = getopt(ac, av, "?hlmo:st:u:v")) > 0) {
		switch((char)rc) {
		default:
			fprintf(stderr, "unrecognized option '%c'\n", (char)rc);
			/*@fallthrough@*/
		case '?':
		case 'h':
			usage(me);
			break;
		case 'l':
			method = Legacy;
			break;
		case 'm':
			method = Modern;
			break;
		case 'o':
			fileout = optarg;
			break;
		case 's':
			method = Split;
			break;
		case 't':
			ftempl = optarg;
			break;
		case 'u':
			unitdir = optarg;
			break;
		case 'v':
			printf("%s v%s\n", me, version);
			return EXIT_SUCCESS;
		}
	}

	/* check argument count */
	idx = optind;
	if (ac <= idx) {
		fprintf(stderr, "no manifest given\n");
		exit(EXIT_FAILURE);
	}

	/* read the manifest */
	manif = NULL;
	rc = manifest_read_and_check(&manif, av[idx]);
	if (rc < 0) {
		fprintf(stderr, "can't read manifest file %s: %s\n",
						av[idx], strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* read meta data of the manifest and merge them to the manifest */
	while (++idx < ac) {
		meta = NULL;
		rc = rp_yaml_path_to_json_c(&meta, av[idx], NULL);
		if (rc < 0) {
			fprintf(stderr, "can't read meta file %s: %s\n",
						av[idx], strerror(errno));
			exit(EXIT_FAILURE);
		}
		if (meta != NULL)
			add_metadata(manif, meta);
	}

	/* check, opens the output */
	if (fileout != NULL) {
		stdout = freopen(fileout, "w", stdout);
		if (stdout == NULL) {
			fprintf(stderr, "can't create output file %s\n", fileout);
			exit(EXIT_FAILURE);
		}
	}

	/* set the unit dir if required */
	if (unitdir != NULL)
		units_set_root_dir(unitdir);

	/* process */
	if (method == Legacy)
		method_legacy(ftempl, manif);
	else if (method == Split)
		method_split(ftempl, manif);
	else
		method_modern(ftempl, manif);

	return EXIT_SUCCESS;
}


