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

#include <string.h>
#include "normalize-unit-file.h"

/*
 * normalize unit files: remove comments, remove heading blanks,
 * make single lines
 */
size_t normalize_unit_file(char *content)
{
	return normalize_unit_file_cmtchrs(content, ";#");
}

size_t normalize_unit_file_cmtchrs(char *content, const char *cmtchrs)
{
	char *read, *write, c;

	cmtchrs = cmtchrs ? cmtchrs : "";
	read = write = content;
	c = *read++;
	while (c) {
		switch (c) {
		case '\n':
		case ' ':
		case '\t':
			/* remove blank lines and blanks at line begin */
			c = *read++;
			break;
		default:
			if (strchr(cmtchrs, c)) {
				/* remove comments until end of line */
				do { c = *read++; } while(c && c != '\n');
				break;
			}
			/* read the line */
			*write++ = c;
			do { *write++ = c = *read++; } while(c && c != '\n');
			if (write - content >= 2 && write[-2] == '\\')
				(--write)[-1] = ' ';
			break;
		}
	}
	*write = c;
	return (size_t)(write - content);
}

