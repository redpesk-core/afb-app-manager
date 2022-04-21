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
#include <gnutls/crypto.h>
#include <gnutls/abstract.h>

#include "path-entry.h"
#include "check-signature.h"
#include "signature-name.h"
#include "digest.h"


#define FIELD_SEPARATOR ' '
#define RECORD_TERMINATOR '\n'

typedef
struct digest_maker
{
	size_t pos;
	gnutls_digest_algorithm_t algorithm;
	char *buffer;
	size_t length;
	size_t prefix;
	int (*wants_node)(const char *path, size_t length);
	int rc;
}
	digest_maker_t;

/**
 * @brief simple node for hash value of a file
 */
typedef
struct hash_node {
	/** next node of the list */
	struct hash_node *next;

	/** a data for the value */
	gnutls_digest_algorithm_t algorithm;

	/* length of the hash */
	unsigned length;

	/** value of the hash (copied) */
	char value[];
} hash_node_t;


static void dispose_hash(void *closure)
{
	hash_node_t *hash = closure, *nxt;
	while (hash != NULL) {
		nxt = hash->next;
		free(hash);
		hash = nxt;
	}
}

static
int entry_hash(
	path_entry_t *entry,
	gnutls_digest_algorithm_t algorithm,
	const char *path,
	hash_node_t **hashnode
) {
	hash_node_t *hash, *prv;
	gnutls_hash_hd_t hctx;
	unsigned hashlen, idxstr;
	FILE *file;
	size_t rlen;
	unsigned char buffer[65500];

	/* search an existing hash */
	prv = NULL;
	hash = path_entry_var(entry, entry_hash);
	while(hash != NULL && hash->algorithm != algorithm) {
		prv = hash;
		hash = hash->next;
	}

	/* create the hash if needed */
	if (hash == NULL) {

		/* open the file */
		file = fopen(path, "rb");
		if (file == NULL)
			return -errno;

		/* allocates the result */
		hashlen = gnutls_hash_get_len(algorithm);
		idxstr = hashlen + hashlen;
		hash = malloc(idxstr + 1 + sizeof *hash);
		if (hash == NULL) {
			fclose(file);
			return -ENOMEM;
		}
		hash->algorithm = algorithm;
		hash->length = idxstr;


		/* link the has to the node */
		hash->next = NULL;
		if (prv != NULL)
			prv->next = hash;
		else {
			int rc = path_entry_var_set(entry, entry_hash, hash, dispose_hash);
			free(hash);
			return rc;
		}


		/* compute the hash */
		gnutls_hash_init(&hctx, algorithm);
		while ((rlen = fread(buffer, 1, sizeof buffer, file)) > 0)
			gnutls_hash(hctx, buffer, rlen);
		gnutls_hash_deinit(hctx, hash->value);

		/* convert to string */
		hash->value[idxstr] = 0;
		while (hashlen) {
			unsigned char high = (unsigned char)hash->value[--hashlen];
			unsigned char low = high & 15;
			hash->value[--idxstr] = (char)(low + (low <= 9 ? '0' : 'a' - 10));
			low = (high >> 4) & 15;
			hash->value[--idxstr] = (char)(low + (low <= 9 ? '0' : 'a' - 10));
		}
	}

	/* return the string if required */
	if (hashnode)
		*hashnode = hash;

	return 0;
}

/* companion function of make_file_list_digest */
static
int digest_maker_cb(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	digest_maker_t *maker = closure;
	int rc;
	size_t len;
	hash_node_t *hashnode;

	/* should the entry be added */
	rc = maker->wants_node ? maker->wants_node(path, length) : 1;
	if (rc) {
		/* get the hash */
		rc = entry_hash(entry, maker->algorithm, path, &hashnode);
		if (rc < 0) {
			if (maker->rc == 0)
				maker->rc = rc;
		}
		else {
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
			len = length - maker->prefix;
			if (maker->pos < maker->length) {
				if (maker->length - maker->pos  < len)
					len = maker->length - maker->pos;
				memcpy(&maker->buffer[maker->pos], &path[maker->prefix] , len);
			}
			maker->pos += length - maker->prefix;

			/* record terminator */
			if (maker->pos < maker->length)
				maker->buffer[maker->pos] = RECORD_TERMINATOR;
			maker->pos++;
		}
	}
	return 0;
}

/**
 * @brief compute the hash file of sha256sum of files of the list
 * (equivalent to the command sha256sum).
 *
 * @param roots the simple list
 * @param algorithm the agorithm to use for computing
 * @param buffer where to store the result
 * @param length length in bytes of the given buffer
 * @param wants_node function called for selecting nodes, returns non zero for selecting
 * @param name_node function call for naming the entry
 * @param open_node function called for opening the nom for reading
 * @param closure closure for the functions
 * @return the length of the production
 */
static
int make_file_list_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	int (*wants_node)(const char *, size_t)
) {
	digest_maker_t maker = {
		.pos = 0,
		.algorithm = algorithm,
		.buffer = buffer,
		.length = length,
		.prefix = path_entry_get_path(root, NULL, 0),
		.wants_node = wants_node,
		.rc = 0
	};

	path_entry_for_each(
			PATH_ENTRY_FORALL_ONLY_ADDED | PATH_ENTRY_FORALL_ABSOLUTE,
			root,
			digest_maker_cb,
			&maker);
	return (int)maker.pos;
}

/**
 * @brief filter files for author signature
 *
 * @param closure  not used
 * @param entry the entry to filter
 * @return 0 if not to be signed
 */
static
int filter_for_author(const char *path, size_t length)
{
	return Signature_None == signature_name_type_length(path, length);
}

/**
 * @brief filter files for distributor signature
 *
 * @param closure  not used
 * @param entry the entry to filter
 * @return 0 if not to be signed
 */
static
int filter_for_distributor(const char *path, size_t length)
{
	return Signature_distributor != signature_name_type_length(path, length);
}

/* compute the hash digest of files of the list */
int create_author_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length
) {
	return make_file_list_digest(
		root,
		algorithm,
		buffer,
		length,
		filter_for_author);
}

/* compute the hash digest of files of the list */
int create_distributor_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length
) {
	return make_file_list_digest(
		root,
		algorithm,
		buffer,
		length,
		filter_for_distributor);
}

/* compute the hash digest of files of the list */
int create_digest(
	path_entry_t *root,
	gnutls_digest_algorithm_t algorithm,
	char *buffer,
	size_t length,
	int isdistributor
) {
	return make_file_list_digest(
		root,
		algorithm,
		buffer,
		length,
		isdistributor ? filter_for_distributor : filter_for_author);
}


/** @brief set the idata of the entry to zero */
static int reset_idata(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	path_entry_var_set(entry, reset_idata, NULL, NULL);
	return 0;
}

struct checking
{
	int isdistributor;
	int nrerrors;
};

/** @brief set the idata of the entry to zero */
static int check_signed(void *closure, path_entry_t *entry, const char *path, size_t length)
{
	struct checking *check = closure;
	signature_type_t sigtyp = signature_name_type_length(path, length);
	int issigned = path_entry_var(entry, reset_idata) != NULL;
	switch (sigtyp) {
	default:
	case Signature_None:
		check->nrerrors += !issigned;
		break;
	case Signature_Author:
		check->nrerrors += !issigned != !check->isdistributor;
		break;
	case Signature_distributor:
		check->nrerrors += issigned;
		break;
	}
	return 0;
}

int check_digest(
	path_entry_t *root,
	const char *buffer,
	size_t length,
	int isdistributor
) {
	size_t posh, posn, it, lenh, lenn;
	struct checking check;
	path_entry_t *entry;
	hash_node_t *hashnode;
	gnutls_digest_algorithm_t algorithm;
	char path[PATH_MAX + 1];
	int rc;

	/* reset the signature flags */
	path_entry_for_each(PATH_ENTRY_FORALL_NO_PATH|PATH_ENTRY_FORALL_ONLY_ADDED, root, reset_idata, NULL);

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

		/* get the entry */
		rc = path_entry_get_length(root, &entry, &buffer[posn], lenn);
		if (rc < 0)
			return rc;

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
		path_entry_get_path(entry, path, sizeof path);
		path[sizeof path - 1] = 0;
		rc = entry_hash(entry, algorithm, path, &hashnode);
		if (rc < 0)
			return rc;

		/* check the hash */
		if (lenh != hashnode->length || 0 != memcmp(&buffer[posh], hashnode->value, lenh))
			return -EBADF;

		/* mark it seen and checked */
		path_entry_var_set(entry, reset_idata, reset_idata, NULL);
	}

	/* check for the signed status */
	check.isdistributor = isdistributor;
	check.nrerrors = 0;

	path_entry_for_each(PATH_ENTRY_FORALL_ONLY_ADDED, root, check_signed, &check);

	return check.nrerrors == 0 ? 0 : -EBADF;
}

/* check if the given digest matches the file list */
int check_author_digest(
	path_entry_t *root,
	const char *digest,
	size_t length
) {
	return check_digest(root, digest, length, 0);
}

/* check if the given digest matches the file list */
int check_distributor_digest(
	path_entry_t *root,
	const char *digest,
	size_t length
) {
	return check_digest(root, digest, length, 1);
}
