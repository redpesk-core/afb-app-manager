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

#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "check-signature.h"
#include "file-hash-list.h"
#include "signature-name.h"


#define FIELD_SEPARATOR ' '
#define RECORD_TERMINATOR '\n'

typedef
struct digest_maker
{
	size_t pos;
	gnutls_digest_algorithm_t algorithm;
	char *buffer;
	size_t length;
	int (*wants_node)(void*, const file_node_t*);
	int (*name_node)(void*, const file_node_t*, char *, size_t);
	FILE *(*open_node)(void*, const file_node_t*);
	void *closure;
	int rc;
}
	digest_maker_t;

/* companion function of make_file_list_digest */
static
int digest_maker_cb(void *hclosure, file_node_t *node)
{
	digest_maker_t *maker = hclosure;
	int rc;
	size_t len;
	hash_node_t *hashnode;

	/* should the node be added */
	rc = maker->wants_node ? maker->wants_node(maker->closure, node) : 1;
	if (rc) {
		/* get the hash */
		rc = file_node_hash(node, maker->algorithm, maker->open_node, maker->closure, &hashnode);
		if (rc < 0)
			return maker->rc = rc;
		/* copy the hash */
		if (maker->pos < maker->length) {
			len = maker->length - maker->pos;
			if (len > hashnode->length)
				len = hashnode->length;
			memcpy(&maker->buffer[maker->pos], hashnode->value, len);
		}
		maker->pos += hashnode->length;
		/* field separator */
		if (maker->pos < maker->length)
			maker->buffer[maker->pos] = FIELD_SEPARATOR;
		maker->pos++;
		/* get the name */
		rc = maker->name_node(maker->closure, node, &maker->buffer[maker->pos], maker->pos < maker->length ? maker->length - maker->pos : 0);
		if (rc < 0)
			return maker->rc = rc;
		maker->pos += (unsigned)rc;
		/* record terminator */
		if (maker->pos < maker->length)
			maker->buffer[maker->pos] = RECORD_TERMINATOR;
		maker->pos++;
	}
	return 1;
}

/**
 * @brief compute the hash file of sha256sum of files of the list
 * (equivalent to the command sha256sum).
 * 
 * @param files the simple list
 * @param algorithm the agorithm to use for computing
 * @param buffer where to store the result
 * @param length length in bytes of the given buffer
 * @param wants_node function called for selecting nodes, returns non zero for selecting
 * @param name_node function call for naming the node
 * @param open_node function called for opening the nom for reading
 * @param closure closure for the functions
 * @return the length of the production
 */
static
int make_file_list_digest(
	file_list_t *files,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	int (*wants_node)(void*, const file_node_t*),
	int (*name_node)(void*, const file_node_t*, char *, size_t),
	FILE *(*open_node)(void*, const file_node_t*),
	void *closure
) {
	digest_maker_t maker = {
		.pos = 0,
		.algorithm = algorithm,
		.buffer = buffer,
		.length = length,
		.wants_node = wants_node,
		.name_node = name_node,
		.open_node = open_node,
		.closure = closure,
		.rc = 0
	};

	file_list_iterate(files, digest_maker_cb, &maker);
	return (int)maker.pos;
}

/**
 * @brief filter files for author signature
 * 
 * @param closure  not used
 * @param node the node to filter
 * @return 0 if not to be signed
 */
static
int filter_for_author(void *closure, const file_node_t *node)
{
	return Signature_None == signature_name_type_length(node->value, node->length);
}

/**
 * @brief filter files for distributor signature
 * 
 * @param closure  not used
 * @param node the node to filter
 * @return 0 if not to be signed
 */
static
int filter_for_distributor(void *closure, const file_node_t *node)
{
	return Signature_distributor != signature_name_type_length(node->value, node->length);
}

/**
 * @brief the default naming is to give the name of the node
 *
 * @param closure unused
 * @param node the node
 * @param buffer where to store the name
 * @param size size for storing
 * @return the length of the name, copied or not
 */
static
int default_name_node(void *closure, const file_node_t *node, char *buffer, size_t size)
{
	if (size)
		memcpy(buffer, node->value, size > node->length ? node->length : size);
	return node->length;
}

/**
 * @brief the default naming is to give the name of the node
 *
 * @param closure the prefixc to be added
 * @param node the node
 * @return file opened
 */
static
FILE *default_open_node(void *closure, const file_node_t *node)
{
	char path[PATH_MAX];
	const char *prefix = closure;
	const char *filename;

	filename = node->value;
	if (prefix != NULL) {
		while(*filename == '/')
			filename++;
		snprintf(path, sizeof path, "%s/%s", prefix, filename);
		filename = path;
	}
	return fopen(filename, "rb");
}

/* compute the hash digest of files of the list */
int create_author_digest(
	file_list_t *files,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	const char *prefix
) {
	return make_file_list_digest(
		files,
		algorithm,
		buffer,
		length,
		filter_for_author,
		default_name_node,
		default_open_node,
		(void*)prefix);
}

/* compute the hash digest of files of the list */
int create_distributor_digest(
	file_list_t *files,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	const char *prefix
) {
	return make_file_list_digest(
		files,
		algorithm,
		buffer,
		length,
		filter_for_distributor,
		default_name_node,
		default_open_node,
		(void*)prefix);
}

/* compute the hash digest of files of the list */
int create_digest(
	file_list_t *files,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	const char *prefix,
	int isdistributor
) {
	return make_file_list_digest(
		files,
		algorithm,
		buffer,
		length,
		isdistributor ? filter_for_distributor : filter_for_author,
		default_name_node,
		default_open_node,
		(void*)prefix);
}


/** @brief set the idata of the node to zero */
static int reset_idata(void *closure, file_node_t *node)
{
	node->idata = 0;
	return 0;
}

struct checking
{
	int isdistributor;
	int nrerrors;
};

/** @brief set the idata of the node to zero */
static int check_signed(void *closure, file_node_t *node)
{
	struct checking *check = closure;
	signature_type_t sigtyp = signature_name_type_length(node->value, node->length);
	switch (sigtyp) {
	default:
	case Signature_None:
		check->nrerrors += node->idata == 0;
		break;
	case Signature_Author:
		check->nrerrors += (node->idata == 0) != (check->isdistributor == 0);
		break;
	case Signature_distributor:
		check->nrerrors += node->idata != 0;
		break;
	}
	return 0;
}

int check_digest(
	file_list_t *files,
	const char *buffer,
	size_t length,
	const char *prefix,
	int isdistributor
) {
	size_t posh, posn, it, lenh, lenn;
	struct checking check;
	file_node_t *node;
	hash_node_t *hashnode;
	gnutls_digest_algorithm_t algorithm;
	int rc;

	/* reset the signature flags */
	file_list_iterate(files, reset_idata, NULL);

	/* iterate over the lines */
	it = 0;
	while (it < length) {
		/* extract first field: HASH */
		posh = it;
		while (it < length && buffer[it] != FIELD_SEPARATOR)
			it++;
		lenh = it - posh;
		if (it < length)
			it++;

		/* extract last field: NAME */
		posn = it;
		while (it < length && buffer[it] != RECORD_TERMINATOR)
			it++;
		lenn = it - posn;
		if (it < length)
			it++;

		/* check validity lengths */
		if (lenh == 0 || lenn == 0)
			return -EINVAL;

		/* get the node */
		node = file_node_find_length(files, &buffer[posn], lenn);
		if (node == NULL)
			return -ENOENT;

		/* deduce algorithm */
		switch (lenh) {
		case 56: algorithm = GNUTLS_DIG_SHA224; break;
		case 64: algorithm = GNUTLS_DIG_SHA256; break;
		case 96: algorithm = GNUTLS_DIG_SHA384; break;
		case 128: algorithm = GNUTLS_DIG_SHA512; break;
		default:
			return -EINVAL;
		}

		/* get the hash */
		rc = file_node_hash(node, algorithm, default_open_node, (void*)prefix, &hashnode);
		if (rc < 0)
			return rc;

		/* check the hash */
		if (lenh != hashnode->length || 0 != memcmp(&buffer[posh], hashnode->value, lenh))
			return -EBADF;
		
		/* mark it seen and checked */
		node->idata = 1;
	}

	/* check for the signed status */
	check.isdistributor = isdistributor;
	check.nrerrors = 0;
	file_list_iterate(files, check_signed, &check);
	return check.nrerrors == 0 ? 0 : -EBADF;
}

/* check if the given digest matches the file list */
int check_author_digest(
	file_list_t *files,
	const char *digest,
	size_t length,
	const char *prefix
) {
	return check_digest(files, digest, length, prefix, 0);
}

/* check if the given digest matches the file list */
int check_distributor_digest(
	file_list_t *files,
	const char *digest,
	size_t length,
	const char *prefix
) {
	return check_digest(files, digest, length, prefix, 1);
}
