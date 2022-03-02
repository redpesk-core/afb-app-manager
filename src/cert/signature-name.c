/*
 Copyright (C) 2015-2022 IoT.bzh Company

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

#include "signature-name.h"

const char signature_extension[] = ".p7";
const char signature_prefix[] = "signature-";
const char author_name[] = "author";

#define LEN(x)    (sizeof(x)-1)
#define ISEQ(x,y) (0==memcmp((x),(y),LEN(y)))

signature_type_t signature_name_type_length(const char *path, size_t length)
{
	size_t posext, posbeg, len;

	/* Is the path terminating with extension ? */
	if (length >= LEN(signature_extension)) {
		posext = length - LEN(signature_extension);
		if (ISEQ(&path[posext], signature_extension)) {
			/* yes, search start of the file name */
			posbeg = posext;
			while (posbeg > 0 && path[posbeg - 1] != '/')
				posbeg--;

			/* is the filename starting with prefix? */
			len = posext - posbeg;
			if (len >= LEN(signature_prefix) && ISEQ(&path[posbeg], signature_prefix)) {
				/* yes, check the related name */
				posbeg += LEN(signature_prefix);
				len -= LEN(signature_prefix);
				/* is that name author? */
				if (len == LEN(author_name) && ISEQ(&path[posbeg], author_name))
					return Signature_Author;
				return Signature_distributor;
			}
		}
	}
	return Signature_None;
}

signature_type_t signature_name_type(const char *path)
{
	return signature_name_type_length(path, strlen(path));
}

static inline
size_t copy(char *buffer, size_t pos, size_t size, const char *value, size_t length)
{
	if (pos < size)
		memcpy(&buffer[pos], value, pos + length <= size ? length : size - pos);
	return pos + length;
}

size_t make_signature_name_length(const char *name, size_t length, char *buffer, size_t size)
{
	size_t pos = copy(buffer, 0, size, signature_prefix, LEN(signature_prefix));
	pos = copy(buffer, pos, size, name, length);
	pos = copy(buffer, pos, size, signature_extension, LEN(signature_extension));
	if (pos < size)
		buffer[pos] = 0;
	return pos;
}

size_t make_signature_name(const char *name, char *buffer, size_t size)
{
	return make_signature_name_length(name, strlen(name), buffer, size);
}

size_t make_signature_name_number(unsigned number, char *buffer, size_t size)
{
	char scratch[22]; /* enougth for 64 bits */
	size_t idx = sizeof scratch;
	do { scratch[--idx] = (char)('0' + number % 10); } while (number /= 10);
	return make_signature_name_length(&scratch[idx], sizeof scratch - idx, buffer, size);
}

size_t make_signature_name_author(char *buffer, size_t size)
{
	return make_signature_name_length(author_name, LEN(author_name), buffer, size);
}
