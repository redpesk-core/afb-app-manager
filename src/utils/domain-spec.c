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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "domain-spec.h"

#define STRLEN_MAX 9 /* one more than the maximum length */

/**
 * @brief permission of domains
 */
struct domain_st {
	/** next domain */
	domain_t *next;
	/** permission */
	domain_permission_t permission;
	/** name */
	char name[];
};

/**
 * @brief prefix of domain permissions
 */
static
const char domain_permission_tags[_domain_permission_count_] = {
	0,
	'+',
	'#',
	'@'
};

static
domain_t *
get_domain_len(const domain_spec_t *spec, const char *domain, size_t length)
{
	domain_t *dom = (domain_t *)spec->head;
	while(dom != NULL && (0 != memcmp(dom->name, domain, length) || dom->name[length] != 0))
		dom = dom->next;
	return dom;
}

static
domain_t *
get_domain(const domain_spec_t *spec, const char *domain)
{
	return get_domain_len(spec, domain, strlen(domain));
}

static
int
add_domain_len(domain_spec_t *spec, domain_permission_t perm, const char *domain, size_t length)
{
	domain_t *dom = malloc(length + 1 + sizeof *dom);
	if (dom == NULL)
		return -ENOMEM;
	memcpy(dom->name, domain, length);
	dom->name[length] = 0;
	dom->permission = perm;
	dom->next = spec->head;
	spec->head = dom;
	return 0;
}

static
int
add_domain(domain_spec_t *spec, domain_permission_t perm, const char *domain)
{
	return add_domain_len(spec, perm, domain, strlen(domain));
}

int
domain_spec_set_len(domain_spec_t *spec, domain_permission_t perm, const char *domain, size_t length)
{
	domain_t *dom, **prv;

	/* validate permission */
	if (!is_domain_permission_valid(perm))
		return -EINVAL;

	/* search the domain */
	prv = &spec->head;
	while((dom = *prv) != NULL && (0 != memcmp(dom->name, domain, length) || dom->name[length] != 0))
		prv = &dom->next;

	if (perm == domain_permission_none) {
		if (dom != NULL) {
			*prv = dom->next;
			free(dom);
		}
		return 0;
	}

	if (dom != NULL) {
		dom->permission = perm;
		return 0;
	}

	return add_domain(spec, perm, domain);
}

int
domain_spec_set(domain_spec_t *spec, domain_permission_t perm, const char *domain)
{
	return domain_spec_set_len(spec, perm, domain, strlen(domain));
}

bool is_domain_spec_granting(const domain_spec_t *spec, const char *domain)
{
	const domain_t *dom = get_domain(spec, domain);
	return dom != NULL && dom->permission == domain_permission_grants;
}

int get_string_of_domain_spec(const domain_spec_t *spec, char **result)
{
	const domain_t *dom;
	domain_permission_t pe;
	size_t sz, dsz;
	char *str;

	/* get size */
	for (sz = 0, dom = spec->head ; dom != NULL ; dom = dom->next) {
		pe = dom->permission;
		if (pe > domain_permission_none && pe < _domain_permission_count_)
			sz += strlen(dom->name) + 2;
	}

	/* allocate */
	str = *result = malloc(sz);
	if (str == NULL)
		return -ENOMEM;

	/* compute */
	for (sz = 0, dom = spec->head ; dom != NULL ; dom = dom->next) {
		pe = dom->permission;
		if (pe > domain_permission_none && pe < _domain_permission_count_) {
			if (sz)
				str[sz++] = ',';
			str[sz++] = domain_permission_tags[pe];
			dsz = strlen(dom->name);
			memcpy(&str[sz], dom->name, dsz);
			sz += dsz;
		}
	}
	str[sz] = 0;
	return (int)sz;
}

int get_domain_spec_of_string(const char *str, domain_spec_t *spec)
{
	domain_t *dom;
	size_t pos, end, len;
	int rc;
	char op;
	domain_permission_t pe;

	if (spec->head != NULL)
		return -ENOTEMPTY;
	for (rc = 0, pos = 0 ; rc == 0 && (op = str[pos]) ; pos = end + !!str[end]) {
		for (pe = 0 ; pe < _domain_permission_count_ && domain_permission_tags[pe] != op ; pe++);
		if (pe == _domain_permission_count_)
			rc = -ENOKEY;
		else {
			for (end = ++pos ; str[end] && str[end] != ',' ; end++);
			if ((len = end - pos) == 0)
				rc = -ENODATA;
			else {
				dom = get_domain_len(spec, &str[pos], len);
				if (dom != NULL)
					rc = -EEXIST;
				else
					rc = add_domain_len(spec, pe,  &str[pos], len);
			}
		}
	}
	if (rc != 0)
		domain_spec_reset(spec);
	return rc;
}

bool is_domain_spec_string_valid(const char *str)
{
	domain_spec_t spec = DOMAIN_SPEC_INITIAL;
	int rc = get_domain_spec_of_string(str, &spec);
	domain_spec_reset(&spec);
	return rc == 0;
}


void domain_spec_reset(domain_spec_t *spec)
{
	domain_t *dom;
	while ((dom  = spec->head) != NULL) {
		spec->head = dom->next;
		free(dom);
	}
}

bool is_domain_spec_able_to_sign(const domain_spec_t *auth, const domain_spec_t *target)
{
	const domain_t *domaut, *domtar;
	bool result = true;
	for (domtar = target->head ; result && domtar != NULL ; domtar = domtar->next) {
		domaut = get_domain(auth, domtar->name);
		result = domaut != NULL
		      && (domaut->permission > domtar->permission
			   || domaut->permission == domain_permission_root);
	}
	return result;
}

int domain_spec_add_grantings(domain_spec_t *to, const domain_spec_t *from)
{
	const domain_t *domfr;
	domain_t *domto;
	int rc = 0;
	for (domfr = from->head ; rc == 0 && domfr != NULL ; domfr = domfr->next) {
		if (domfr->permission == domain_permission_grants) {
			domto = get_domain(to, domfr->name);
			if (domto == NULL)
				rc = add_domain(to, domain_permission_grants, domfr->name);
			else
				domto->permission = domain_permission_grants;
		}
	}
	return rc;
}

void domain_spec_enum(const domain_spec_t *spec, void (*func)(const char*, domain_permission_t, void*), void *closure)
{
	const domain_t *dom;
	for (dom = spec->head ; dom != NULL ; dom = dom->next)
		func(dom->name, dom->permission, closure);
}

/************************************************************************/

#if 0
#include <stdio.h>
int main(int ac, char **av)
{
	domain_spec_t sin, sout;
	unsigned it, j, x, n;
	char *str1, *str2;
	int s1, s2, s3;

	for (n = 1, j = 0 ; j < _domain_count_ ; j++, n *= _domain_permission_count_);
	for (it = 0 ; it < n ; it++) {
		for (j = 0, x = it ; j < _domain_count_ ; j++, x /= _domain_permission_count_)
			sin.domains[j] = (domain_permission_t)(x % _domain_permission_count_);
		s1 = get_string_of_domain_spec(&sin, &str1);
		s2 = get_domain_spec_of_string(str1, &sout);
		s3 = get_string_of_domain_spec(&sout, &str2);
		for (j = 0, x = 1 ; j < _domain_count_ && x ; j++)
			x = sin.domains[j] == sout.domains[j];
		printf ("%d: %s %s [%s %d/%d/%d]\n", it, str1, str2, x?"ok":"KO!",s1,s2,s3);
		free(str1);
		free(str2);
	}
	return 0;
}
#endif

