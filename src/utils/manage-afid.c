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

static uint32_t *afids_array = NULL;

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
					AFID_SET((uint32_t*)closure, p);
			}
		}
		iter = strstr(iter, key_afm_prefix);
	}
	free(content);
	return 0;
}

static int update_afids(uint32_t *afids)
{
	int rcs, rcu;

	memset(afids, 0, AFID_ACNT * sizeof(uint32_t));
	rcs = units_fs_list(0, get_afid_cb, NULL, 1);
	if (rcs < 0)
		RP_ERROR("troubles while updating system's afids");
	rcu = units_fs_list(1, get_afid_cb, NULL, 1);
	if (rcu < 0)
		RP_ERROR("troubles while updating user's afids");

	return 0; //rcs < 0 ? rcs : rcu;
}

static int first_free_afid(uint32_t *afids)
{
	int afid;

	afid = AFID_MIN;
	while (afid <= AFID_MAX && !~afids[AFID_AIDX(afid)])
		afid += 32;
	while (afid <= AFID_MAX && AFID_TEST(afids, afid))
		afid++;
	if (afid > AFID_MAX) {
		RP_ERROR("Can't compute a valid afid");
		errno = EADDRNOTAVAIL;
		afid = -1;
	}
	return afid;
}

int get_new_afid()
{
	int afid;

	/* ensure existing afid bitmap */
	if (afids_array == NULL) {
		afids_array = malloc(AFID_ACNT * sizeof(uint32_t));
		if (afids_array == NULL || update_afids(afids_array) < 0)
			return -1;
	}

	/* allocates the afid */
	afid = first_free_afid(afids_array);
	if (afid < 0 && errno == EADDRNOTAVAIL) {
		/* no more ids, try to rescan */
		memset(afids_array, 0, AFID_ACNT * sizeof(uint32_t));
		if (update_afids(afids_array) >= 0)
			afid = first_free_afid(afids_array);
	}
	if (afid >= 0)
		AFID_SET(afids_array, afid);

	return afid;
}

