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

#pragma once

#include <stdbool.h>

/************************************************************************/

/**
 * @brief permission of domains
 */
typedef enum domain_permission_et {
	/** no permission */
	domain_permission_none,
	/** permissions of the domain are granted */
	domain_permission_grants,
	/** permissions can deliver certificates that grants */
	domain_permission_auth,
	/** permissions can deliver any certificates */
	domain_permission_root,
	/** count of permissions */
	_domain_permission_count_
} domain_permission_t;

/**
 * @brief permission of domains
 */
typedef struct domain_st domain_t;

/**
 * @brief specification of domains
 */
typedef struct domain_spec_st {
	domain_t *head;
} domain_spec_t;

/**
 * initial value of domain spec
 */
#define DOMAIN_SPEC_INITIAL ((domain_spec_t){ NULL })

/**
 * @brief check if the domain permission is valid
 *
 * @param perm the value to check
 * @return true if valid
 */
static inline
bool is_domain_permission_valid(domain_permission_t perm)
{
	return 0 <= perm && perm < _domain_permission_count_;
}

/**
 * @brief check if the domain spec is valid
 *
 * @param spec the value to check
 * @return true if valid
 */
extern
bool is_domain_spec_valid(const domain_spec_t *spec);

/**
 * @brief check if the string represents a valid domain spec
 *
 * @param str the string to check
 * @return true if valid
 */
extern
bool is_domain_spec_string_valid(const char *str);

/**
 * @brief Get the string representation of a domain spec
 *
 * @param spec spec to translate
 * @param result created and allocated string
 * @return int return the length of the string or -ENOMEM on memory depletion
 */
extern
int get_string_of_domain_spec(const domain_spec_t *spec, char **result);

/**
 * @brief Get the domain spec of string object
 *
 * @param str the string to translate
 * @param spec the resulting spec
 * @return int 0 on success or -EINVAL if the string isn't good
 */
extern
int get_domain_spec_of_string(const char *str, domain_spec_t *spec);

/**
 * @brief Init the domain spec
 *
 * Synonim of domain_spec_fill with value domain_permission_none
 *
 * @param spec the spec
 */
extern
void domain_spec_reset(domain_spec_t *spec);

/**
 * @brief Check if the domain spec is granting the domain
 *
 * @param spec the spec giving the permissions
 * @param domain the domain to check
 * @return int 0 on success or -EINVAL if the string isn't good
 */
extern
bool is_domain_spec_granting(const domain_spec_t *spec, const char *domain);

/**
 * @brief Check if an  authority of domain spec is allowed to sign the other domain spec
 *
 * @param auth the spec giving the permissions
 * @param target the spec to check
 * @return int 0 on success or -EINVAL if the string isn't good or -ENOMEM on out of memory
 */
extern
bool is_domain_spec_able_to_sign(const domain_spec_t *auth, const domain_spec_t *target);

/**
 * @brief Add to spec the granting capabilities of other
 *
 * @param to the spec to update
 * @param from the spec to add
 * @return 0 on success or -EINVAL if the string isn't good or -ENOMEM on out of memory
 */
extern
int domain_spec_add_grantings(domain_spec_t *to, const domain_spec_t *from);

/**
 * @brief call the given function for all items of the domain spec
 *
 * @param spec the spec to inspect
 * @param func the function to call. It receives 3 parameters: the domain name, its
 *             permission and the given closure
 * @param closure closure for the function
 */
extern
void domain_spec_enum(const domain_spec_t *spec, void (*func)(const char*, domain_permission_t, void*), void *closure);

/**
 * @brief set the permission of a domain
 *
 * @param spec the spec to change
 * @param perm the permission to set to the domain
 * @param domain the modified domain
 * @param length length of the domain
 * @return 0 on success or -EINVAL if the string isn't good or -ENOMEM on out of memory
 */
extern
int
domain_spec_set_len(domain_spec_t *spec, domain_permission_t perm, const char *domain, size_t length);

/**
 * @brief set the permission of a domain
 *
 * @param spec the spec to change
 * @param perm the permission to set to the domain
 * @param domain the modified domain
 * @return 0 on success or -EINVAL if the string isn't good or -ENOMEM on out of memory
 */
extern
int
domain_spec_set(domain_spec_t *spec, domain_permission_t perm, const char *domain);
