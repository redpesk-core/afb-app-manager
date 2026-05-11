/*
 * Copyright (C) 2015-2026 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * $RP_BEGIN_LICENSE$
 * Commercial License Usage
 *  Licensees holding valid commercial IoT.bzh licenses may use this file in
 *  accordance with the commercial license agreement provided with the
 *  Software or, alternatively, in accordance with the terms contained in
 *  a written agreement between you and The IoT.bzh Company. For licensing terms
 *  and conditions see https://www.iot.bzh/terms-conditions. For further
 *  information use the contact form at https://www.iot.bzh/contact.
 *
 * GNU General Public License Usage
 *  Alternatively, this file may be used under the terms of the GNU General
 *  Public license version 3. This license is as published by the Free Software
 *  Foundation and appearing in the file LICENSE.GPLv3 included in the packaging
 *  of this file. Please review the following information to ensure the GNU
 *  General Public License requirements will be met
 *  https://www.gnu.org/licenses/gpl-3.0.html.
 * $RP_END_LICENSE$
 */

#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <errno.h>

#define AFB_BINDING_VERSION 4
#include <afb/afb-binding.h>

#include <rp-utils/rp-verbose.h>

#include "auth.h"

#if !defined(CKAUTH_STACK_DEPTH)
#define CKAUTH_STACK_DEPTH 8
#endif

/**
 * structure for checking temporarily check permission synchronously */
struct ckauth {
	afb_req_t req;
	int idx;
	const afb_auth_t *authstack[CKAUTH_STACK_DEPTH];
	void (*callback)(void *closure, int status, void *extra);
	void *closure;
	void *extra;
};

/* predeclaration */
static void cka_push(struct ckauth *cka, const afb_auth_t *auth);


static void cka_end(struct ckauth *cka, int status)
{
	void (*callback)(void*,int,void*) = cka->callback;
	void *closure = cka->closure;
	void *extra = cka->extra;
	free(cka);
	callback(closure, status, extra);
}

static void cka_pop(struct ckauth *cka, int status)
{
	if (cka->idx <= 0)
		cka_end(cka, status);
	else {
		const afb_auth_t *auth = cka->authstack[--cka->idx];
		switch (auth->type) {
		case afb_auth_Or:
			if (status <= 0) {
				cka_push(cka, auth->next);
				return;
			}
			break;
		case afb_auth_And:
			if (status > 0) {
				cka_push(cka, auth->next);
				return;
			}
			break;
		case afb_auth_Not:
			status = status <= 0;
			break;
		default:
			break;
		}
		cka_pop(cka, status);
	}
}

static void cka_perm_cb(void *closure, int status, afb_req_t req)
{
	struct ckauth *cka = (struct ckauth*)closure;
	cka_pop(cka, status);
}

static void cka_push(struct ckauth *cka, const afb_auth_t *auth)
{
	if (cka->idx >= (int)(sizeof cka->authstack / sizeof cka->authstack[0])) {
		AFB_ERROR("CKA stack too small");
		cka_end(cka, -E2BIG);
	}
	else {
		cka->authstack[cka->idx++] = auth;
		switch (auth->type) {
		case afb_auth_Permission:
			afb_req_check_permission(cka->req, auth->text, cka_perm_cb, cka);
			break;
		case afb_auth_Or:
		case afb_auth_And:
		case afb_auth_Not:
			cka_push(cka, auth->first);
			break;
		case afb_auth_Yes:
			cka_pop(cka, 1);
			break;
		case afb_auth_No:
		case afb_auth_Token:
		case afb_auth_LOA:
		default:
			cka_pop(cka, 0);
			break;
		}
	}
}

void auth_check(
	afb_req_t req,
	const afb_auth_t *auth,
	void (*callback)(void *closure, int status, void *extra),
	void *closure,
	void *extra
) {
	struct ckauth *cka = malloc(sizeof *cka);
	if (cka == NULL)
		callback(closure, -ENOMEM, extra);
	else {
		cka->req = req;
		cka->idx = 0;
		cka->callback = callback;
		cka->closure = closure;
		cka->extra = extra;
		cka_push(cka, auth);
	}
}

