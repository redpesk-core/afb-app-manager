/*
 Copyright 2015, 2016 IoT.bzh

 author: José Bollo <jose.bollo@iot.bzh>

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


#include <stdlib.h>
#include <string.h>

#include "verbose.h"
#include "wgtpkg-base64.h"

static char tob64(char x)
{
	if (x < 26)
		return (char)('A' + x);
	if (x < 52)
		return (char)('a' + x - 26);
	if (x < 62)
		return (char)('0' + x - 52);
	return x == 62 ? '+' : '/';
}

char *base64encw(const char *buffer, size_t length, unsigned width)
{
	size_t remain, in, out;
	char *result;

	if (width == 0 || (width & 3) != 0) {
		ERROR("bad width in base64enc");
		return NULL;
	}
	result = malloc(2 + 4 * ((length + 2) / 3) + (length / width));
	if (result == NULL) {
		ERROR("malloc failed in base64enc");
		return NULL;
	}
	in = out = 0;
	remain = length;
	while (remain >= 3) {
		if (out % (width + 1) == width)
			result[out++] = '\n'; 
		result[out] = tob64((buffer[in] >> 2) & '\x3f');
		result[out+1] = tob64((char)(((buffer[in] << 4) & '\x30')
					| ((buffer[in+1] >> 4) & '\x0f')));
		result[out+2] = tob64((char)(((buffer[in+1] << 2) & '\x3c')
					| ((buffer[in+2] >> 6) & '\x03')));
		result[out+3] = tob64(buffer[in+2] & '\x3f');
		remain -= 3;
		in += 3;
		out += 4;
	}
	if (remain != 0) {
		if (out % (width + 1) == width)
			result[out++] = '\n'; 
		result[out] = tob64((buffer[in] >> 2) & '\x3f');
		if (remain == 1) {
			result[out+1] = tob64((buffer[in] << 4) & '\x30');
			result[out+2] = '=';
		} else {
			result[out+1] = tob64((char)(((buffer[in] << 4) & '\x30')
						| ((buffer[in+1] >> 4) & '\x0f')));
			result[out+2] = tob64((buffer[in+1] << 2) & '\x3c');
		}
		result[out+3] = '=';
		out += 4;
	}
	result[out] = 0;
	return result;
}

char *base64enc(const char *buffer, size_t length)
{
	return base64encw(buffer, length, 76);
}

static char fromb64(char x)
{
	if ('A' <= x && x <= 'Z')
		return (char)(x - 'A');
	if ('a' <= x && x <= 'z')
		return (char)(x - 'a' + 26);
	if ('0' <= x && x <= '9')
		return (char)(x - '0' + 52);
	if (x == '+')
		return (char)62;
	if (x == '/')
		return (char)63;
	if (x == '=')
		return '@';
	return 'E';
}

ssize_t base64dec(const char *buffer, char **output)
{
	size_t len, in;
	ssize_t out;
	char *result;
	unsigned char x0, x1, x2, x3;

	len = strlen(buffer);
	result = malloc(3 * ((3 + len) / 4));
	if (result == NULL) {
		ERROR("malloc failed in base64dec");
		return -1;
	}
	in = 0;
	out = 0;
	while (buffer[in] == '\r' || buffer[in] == '\n')
		in++;
	while (buffer[in]) {
		if (in + 4 > len) {
			ERROR("unexpected input size in base64dec");
			free(result);
			return -1;
		}
		x0 = (unsigned char)fromb64(buffer[in]);
		x1 = (unsigned char)fromb64(buffer[in+1]);
		x2 = (unsigned char)fromb64(buffer[in+2]);
		x3 = (unsigned char)fromb64(buffer[in+3]);
		in += 4;
		if (x0 == 'E' || x1 == 'E' || x2 == 'E' || x3 == 'E') {
			ERROR("unexpected input character in base64dec");
			free(result);
			return -1;
		}
		if (x0 == '@' || x1 == '@' || (x2 == '@' && x3 != '@')) {
			ERROR("unexpected termination character in base64dec");
			free(result);
			return -1;
		}
		result[out] = (char)((x0 << 2) | (x1 >> 4));
		result[out+1] = (char)((x1 << 4) | ((x2 >> 2) & 15));
		result[out+2] = (char)((x2 << 6) | (x3 & 63));
		while (buffer[in] == '\r' || buffer[in] == '\n')
			in++;
		if (x3 != '@')
			out += 3;
		else if (!buffer[in])
			out += 1 + (unsigned)(x2 != '@');
		else {
			ERROR("unexpected continuation in base64dec");
			free(result);
			return -1;
		}
		if (out < 0) {
			ERROR("output too big in base64dec");
			free(result);
			return -1;
		}
	}
	*output = result;
	return out;
}

int base64eq(const char *buf1, const char *buf2)
{
	for(;;) {
		while(*buf1 == '\n' || *buf1 == '\r')
			buf1++;
		while(*buf2 == '\n' || *buf2 == '\r')
			buf2++;
		if (*buf1 != *buf2)
			return 0;
		if (!*buf1)
			return 1;
		buf1++;
		buf2++;
	}
}

#ifdef TESTBASE64
#include <fcntl.h>
#include <string.h>

int main(int ac, char **av)
{
	char buffer[32768];
	int fd, l0, l1, l2;
	char *p1, *p2;

	while(*++av) {
		fd = open(*av, O_RDONLY);
		if (fd < 0) continue;
		l0 = read(fd, buffer, sizeof buffer);
		if (l0 <= 0) continue;
		close(fd);
		p1 = base64enc(buffer, l0);
		if (!p1) continue;
		l1 = strlen(p1);
		l2 = base64dec(p1, &p2);
		if (l2 <= 0) continue;
printf("[[[%.*s]]]\n",l2,p2);
		if (l0 != l2) printf("length mismatch\n");
		else if (memcmp(buffer, p2, l0)) printf("content mismatch\n");
		free(p1);
		free(p2);
	}
	return 0;
}

#endif
