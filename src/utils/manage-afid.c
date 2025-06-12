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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rp-utils/rp-verbose.h>
#include <rp-utils/rp-file.h>

#include "normalize-unit-file.h"
#include "unit-fs.h"

static const char key_afm_prefix[] = "X-AFM-";
static const char key_afid[] = "ID";

#define AFID_MIN		1
#define AFID_MAX		1999
#define AFID_IS_VALID(afid)	(AFID_MIN <= (afid) && (afid) <= AFID_MAX)
#define AFID_COUNT		(AFID_MAX - AFID_MIN + 1)
#define AFID_ACNT		((AFID_COUNT + 31) >> 5)
#define AFID_ASFT(afid)		(((afid) - AFID_MIN) & 31)
#define AFID_AIDX(afid)		(((afid) - AFID_MIN) >> 5)
#define AFID_TEST(array,afid)	((((array)[AFID_AIDX(afid)]) >> AFID_ASFT(afid)) & 1)
#define AFID_SET(array,afid)	(((array)[AFID_AIDX(afid)]) |= (((uint32_t)1) << AFID_ASFT(afid)))

static bool init_done = false;
static uint32_t afids_array[AFID_ACNT];

static int get_afid_cb(void *closure, const char *name, const char *path, int isuser)
{
	char *iter;
	char *content;
	size_t length;
	int rc, p;

	/* reads the file */
	rc = rp_file_get(path, &content, &length);
	if (rc < 0)
		return rc;

	/* normalize the unit file */
	normalize_unit_file(content);

	/* process the file */
	iter = strstr(content, key_afm_prefix);
	while (iter) {
		iter += sizeof key_afm_prefix - 1;
		if (*iter == '-')
			iter++;
		if (!strncmp(iter, key_afid, sizeof key_afid - 1)) {
			iter += sizeof key_afid - 1;
			while(*iter && *iter != '=' && *iter != '\n')
				iter++;
			if (*iter == '=') {
				while(*++iter == ' ');
				p = atoi(iter);
				if (AFID_IS_VALID(p))
					AFID_SET(afids_array, p);
			}
		}
		iter = strstr(iter, key_afm_prefix);
	}
	free(content);
	return 0;
}

static int update_afids(bool strict)
{
	int rcs, rcu, loglvl;

	loglvl = strict ? rp_Log_Level_Error : rp_Log_Level_Warning;
	memset(afids_array, 0, sizeof afids_array);
	rcs = units_fs_list(0, get_afid_cb, NULL, 1);
	if (rcs < 0)
		_RP_VERBOSE_(loglvl, "troubles while updating system's afids");
	rcu = units_fs_list(1, get_afid_cb, NULL, 1);
	if (rcu < 0)
		_RP_VERBOSE_(loglvl, "troubles while updating user's afids");

	return !strict ? 0 : rcs < 0 ? rcs : rcu;
}

static int first_free_afid()
{
	int afid;

	afid = AFID_MIN;
	while (afid <= AFID_MAX && !~afids_array[AFID_AIDX(afid)])
		afid += 32;
	while (afid <= AFID_MAX && AFID_TEST(afids_array, afid))
		afid++;
	if (afid > AFID_MAX) {
		RP_ERROR("Can't get a valid afid");
		errno = EADDRNOTAVAIL;
		afid = -1;
	}
	return afid;
}

int init_afid_manager(int strict)
{
	/* is already done? */
	if (init_done)
		return 0;

	/* initialize */
	init_done = true;
	return update_afids(strict != 0);
}

int get_new_afid()
{
	int rc, afid;

	/* lazy init */
	rc = init_afid_manager(true);
	if (rc < 0)
		return rc;

	/* allocates the afid */
	afid = first_free_afid();
	if (afid < 0) {
		/* no more ids, try to rescan */
		rc = update_afids(true);
		if (rc < 0)
			return rc;
		afid = first_free_afid();
	}

	/* record allocation */
	if (afid >= 0)
		AFID_SET(afids_array, afid);

	return afid;
}

