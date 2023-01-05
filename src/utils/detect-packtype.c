/*
 Copyright (C) 2015-2023 IoT.bzh Company

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
#include <string.h>

#include "detect-packtype.h"

/**
 * Checks if the content has to be communicated to the framework
 */
packtype_t detect_packtype(const char *packname, size_t plen, const char *filename, size_t flen, size_t *blen)
{
	/* the path terminated with "/PACKNAME/ONE-OF-THE-SUFFIX-BELOW"
	 * are assumed to need framework action
	 * the list below is a LINEFEED separated list of knwon suffixes
	 */
	static const char names[] =
		"config.xml\n"
		".rpconfig/manifest.yml\n";

	static packtype_t types[] = {
		packtype_Widget,
		packtype_AfmPkg
	};

	size_t idx, tdx, nlen;

	/* iterate over the suffix names */
	for (idx = tdx = 0 ; names[idx] ; idx += nlen + 1, tdx++) {
		/* compute length of the current suffix */
		for (nlen = 0 ; names[idx + nlen] != '\n' ; nlen++);
		/* check if file matches the suffix and its containing directory */
		if (nlen + 1 <= flen
		 && (plen == 0 || (nlen + plen + 2 <= flen
				  && filename[flen - nlen - plen - 2] == '/'
				  && !memcmp(packname, &filename[flen - nlen - plen - 1], plen)))
		 && filename[flen - nlen - 1] == '/'
		 && !memcmp(&names[idx], &filename[flen - nlen], nlen)) {
			if (blen)
				*blen = flen - nlen - 1;
			return types[tdx];
		}
	}
	return packtype_Unknown;
}
