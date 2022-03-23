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
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xmlsave.h>


#include <rp-utils/rp-verbose.h>
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-certs.h"
#include "wgtpkg-xmlsec.h"
#include "wgtpkg-digsig.h"



static const char uri_role_author[] = "http://www.w3.org/ns/widgets-digsig#role-author";
static const char uri_role_distributor[] = "http://www.w3.org/ns/widgets-digsig#role-distributor";
static const char uri_profile[] = "http://www.w3.org/ns/widgets-digsig#profile";


/* global data */
static xmlDocPtr document;  /* the document */

/* facility to get the first element node (skip text nodes) starting with 'node' */
static xmlNodePtr next_element(xmlNodePtr node)
{
	while (node && node->type != XML_ELEMENT_NODE)
		node = node->next;
	return node;
}

/* is the 'node' an element node of 'name'? */
static int is_element(xmlNodePtr node, const char *name)
{
	return node->type == XML_ELEMENT_NODE
		&& !strcmp(name, node->name);
}

/* is the 'node' an element node of 'name'? */
static int is_node(xmlNodePtr node, const char *name)
{
	return node != NULL && is_element(node, name);
}

#if 0
/* facility to get the first element node (skip text nodes) starting with 'node' */
static xmlNodePtr next_element_type(xmlNodePtr node, const char *name)
{
	while (node && node->type != XML_ELEMENT_NODE && strcmp(name, node->name))
		node = node->next;
	return node;
}

/* search the element node of id. NOTE : not optimized at all */
static xmlNodePtr search_for(const char *attrname, const char *value)
{
	char *val;
	xmlNodePtr iter, next;
	xmlNodePtr result;

	result = NULL;
	iter = xmlDocGetRootElement(document);
	while (iter != NULL) {
		val = xmlGetProp(iter, attrname);
		if (val != NULL && !strcmp(val, value)) {
			if (result != NULL) {
				RP_ERROR("duplicated %s %s", attrname, value);
				free(val);
				return NULL;
			}
			result = iter;
		}
		free(val);
		next = next_element(iter->children);
		if (next == NULL) {
			/* no child, try sibling */
			next = next_element(iter->next);
			if (next == NULL) {
				iter = iter->parent;
				while (iter != NULL && next == NULL) {
					next = next_element(iter->next);
					iter = iter->parent;
				}
			}
		}
		iter = next;
	}
	if (result == NULL)
		RP_ERROR("node of %s '%s' not found", attrname, value);
	return result;
}

/* search the element node of id. NOTE : not optimized at all */
static xmlNodePtr search_id(const char *id)
{
	return search_for("Id", id);
}
#endif

/* check the digest of one element */
static int check_one_reference(xmlNodePtr ref)
{
	int rc;
	char *uri;
	xmlURIPtr u;
	struct filedesc *fdesc;

	/* start */
	rc = -1;

	/* get the uri */
	uri = xmlGetProp(ref, "URI");
	if (uri == NULL) {
		RP_ERROR("attribute URI of element <Reference> not found");
		goto error;
	}

	/* parse the uri */
	u = xmlParseURI(uri);
	if (!u) {
		RP_ERROR("error while parsing URI %s", uri);
		goto error2;
	}

	/* check that unexpected parts are not there */
	if (u->scheme || u->opaque || u->authority || u->server || u->user || u->query) {
		RP_ERROR("unexpected uri component in %s", uri);
		goto error3;
	}

	/* check path and fragment */
	if (!u->path && !u->fragment) {
		RP_ERROR("invalid uri %s", uri);
		goto error3;
	}
	if (u->path && u->fragment) {
		RP_ERROR("not allowed to sign foreign fragment in %s", uri);
		goto error3;
	}

	if (u->path) {
		/* check that the path is valid */
		fdesc = file_of_name(u->path);
		if (fdesc == NULL) {
			RP_ERROR("reference to unknown file %s", u->path);
			goto error3;
		}
		if (fdesc->type != type_file) {
			RP_ERROR("reference to directory %s", u->path);
			goto error3;
		}
		if ((fdesc->flags & flag_distributor_signature) != 0) {
			RP_ERROR("reference to signature %s", u->path);
			goto error3;
		}
		fdesc->flags |= flag_referenced;
		rc = 0;
	} else {
		rc = 0;
	}

error3:
	xmlFreeURI(u);
error2:
	xmlFree(uri);
error:
	return rc;
}

static int check_references(xmlNodePtr sinfo)
{
	unsigned int i, n, flags;
	struct filedesc *f;
	int result;
	xmlNodePtr elem;

	result = 0;
	elem = sinfo->children;
	while (elem != NULL) {
		if (is_element(elem, "Reference"))
			if (check_one_reference(elem))
				result = -1;
		elem = elem->next;
	}

	n = file_count();
	i = 0;
	while(i < n) {
		f = file_of_index(i++);
		if (f->type == type_file) {
			flags = f->flags;
			if (!(flags & (flag_signature | flag_referenced))) {
				RP_ERROR("file not referenced in signature: %s", f->name);
				result = -1;
			}
		}
	}

	return result;
}


static int get_certificates(xmlNodePtr kinfo)
{
	xmlNodePtr n1, n2;
	char *b;
	int rc;

	n1 = kinfo->children;
	while (n1) {
		if (is_element(n1, "X509Data")) {
			n2 = n1->children;
			while (n2) {
				if (is_element(n2, "X509Certificate")) {
					b = xmlNodeGetContent(n2);
					if (b == NULL) {
						RP_ERROR("xmlNodeGetContent of X509Certificate failed");
						return -1;
					}
					rc = add_certificate_b64(b);
					xmlFree(b);
					if (rc)
						return rc;
				}
				n2 = n2->next;
			}
		}
		n1 = n1->next;
	}
	return 0;
}

/* checks the current document */
static int checkdocument()
{
	int rc;
	xmlNodePtr sinfo, svalue, kinfo, objs, rootsig;

	rc = -1;

	rootsig = xmlDocGetRootElement(document);
	if (!is_node(rootsig, "Signature")) {
		RP_ERROR("root element <Signature> not found");
		goto error;
	}

	sinfo = next_element(rootsig->children);
	if (!is_node(sinfo, "SignedInfo")) {
		RP_ERROR("element <SignedInfo> not found");
		goto error;
	}

	svalue = next_element(sinfo->next);
	if (!is_node(svalue, "SignatureValue")) {
		RP_ERROR("element <SignatureValue> not found");
		goto error;
	}

	kinfo = next_element(svalue->next);
	if (is_node(kinfo, "KeyInfo")) {
		objs = kinfo->next;
	} else {
		objs = kinfo;
		kinfo = NULL;
	}

	rc = check_references(sinfo);
	if (rc)
		goto error;

	rc = xmlsec_verify(rootsig);
	if (rc)
		goto error;

	rc = get_certificates(kinfo);

error:
	return rc;
}

/* verify the digital signature of the file described by 'fdesc' */
int verify_digsig(struct filedesc *fdesc)
{
	int res, fd;

	assert ((fdesc->flags & flag_signature) != 0);
	RP_DEBUG("-- checking file %s", fdesc->name);

	/* reset the flags */
	file_clear_flags();
	clear_certificates();

	/* reads and xml parses the signature file */
	fd = openat(workdirfd, fdesc->name, O_RDONLY);
	if (fd < 0) {
		RP_ERROR("cant't open file %s", fdesc->name);
		return -1;
	}
	document = xmlReadFd(fd, fdesc->name, NULL, 0);
	close(fd);
	if (document == NULL) {
		RP_ERROR("xml parse of file %s failed", fdesc->name);
		return -1;
	}

	res = checkdocument();
	if (res)
		RP_ERROR("previous error was during check of file %s", fdesc->name);

	xmlFreeDoc(document);
	return res;
}

/* check all the signature files */
int check_all_signatures(int allow_none)
{
	int rc, irc;
	unsigned int i, n;
	struct filedesc *fdesc;

	n = signature_count();
	if (n == 0) {
		if (!allow_none) {
			RP_ERROR("no signature found");
			return -1;
		}
		return 0;
	}

	rc = xmlsec_init();
	if (rc < 0) {
		RP_ERROR("can't check signature");
		return rc;
	}

	rc = 0;
	for (i = n ; i ; ) {
		fdesc = signature_of_index(--i);
		irc = verify_digsig(fdesc);
		if (irc < 0)
			rc = irc;
	}

	return rc;
}

/* create a signature of 'index' (0 for author, other values for distributors)
using the private 'key' (filename) and the certificates 'certs' (filenames)
as trusted chain */
int create_digsig(unsigned int index, const char *key, const char **certs)
{
	struct filedesc *fdesc;
	xmlDocPtr doc;
	int rc, fd;
	long len;
	xmlSaveCtxtPtr ctx;

	rc = -1;

	/* create the doc */
	doc = xmlsec_create(index, key, certs);
	if (doc == NULL)
		goto error;

	/* instanciate the filename */
	fdesc = create_signature(index);
	if (fdesc == NULL)
		goto error2;

	/* save the doc as file */
	fd = openat(workdirfd, fdesc->name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		RP_ERROR("cant open %s for write", fdesc->name);
		goto error2;
	}
	ctx = xmlSaveToFd(fd, NULL, XML_SAVE_FORMAT);
	if (!ctx) {
		RP_ERROR("xmlSaveToFd failed for %s", fdesc->name);
		goto error3;
	}
	len = xmlSaveDoc(ctx, doc);
	if (len < 0) {
		RP_ERROR("xmlSaveDoc to %s failed", fdesc->name);
		goto error4;
	}

	rc = 0;
error4:
	xmlSaveClose(ctx);
error3:
	close(fd);
error2:
	xmlFreeDoc(doc);
error:
	return rc;
}

/* create a digital signature(s) from environment data */
int create_auto_digsig()
{
	static const char envvar_prefix[] = "WGTPKG_AUTOSIGN_";
	extern char **environ;

	char **enviter;
	char *var;
	char *iter;
	char *equal;
	unsigned int num;
	char *keyfile;
	const char *certfiles[10];
	int ncert;
	int rc;
	int i;

	rc = 0;
	/* enumerate environment variables */
	enviter = environ;
	while  (rc == 0 && (var = *enviter++) != NULL) {
		/* check the prefix */
		if (0 != strncmp(var, envvar_prefix, sizeof(envvar_prefix) - 1))
			continue; /* not an auto sign variable */
		RP_DEBUG("autosign found %s", var);

		/* check the num */
		iter = &var[sizeof(envvar_prefix) - 1];
		if (*iter < '0' || *iter > '9') {
			RP_ERROR("bad autosign key found: %s", var);
			rc = -1;
			continue;
		}

		/* compute the number */
		num = (unsigned int)(*iter++ - '0');
		while (*iter >= '0' && *iter <= '9')
			num = 10 * num + (unsigned int)(*iter++ - '0');

		/* next char must be = */
		if (*iter != '=' || !iter[1]) {
			/* it is not an error to have an empty autosign */
			RP_WARNING("ignoring autosign key %.*s", (int)(iter - var), var);
			continue;
		}

		/* auto signing with num */
		RP_INFO("autosign key %u found", num);

		/* compute key and certificates */
		equal = iter++;
		keyfile = iter;
		*equal = 0;
		ncert = 0;
		while (ncert < (int)((sizeof certfiles / sizeof *certfiles) - 1)
				&& (iter = strchr(iter, ':')) != NULL) {
			*iter++ = 0;
			certfiles[ncert++] = iter;
		}
		certfiles[ncert] = NULL;

		/* check the parameters */
		if (access(keyfile, R_OK) != 0) {
			RP_ERROR("autosign %u can't access private key %s", num, keyfile);
			rc = -1;
		}
		for(i = 0 ; i < ncert ; i++) {
			if (access(certfiles[i], R_OK) != 0) {
				RP_ERROR("autosign %u can't access certificate %s", num, certfiles[i]);
				rc = -1;
			}
		}

		/* sign now */
		if (rc == 0) {
			rc = xmlsec_init();
			if (rc == 0) {
				rc = create_digsig(num, keyfile, certfiles);
			}
		}

		/* restore stolen chars */
		while(ncert)
			*(char*)(certfiles[--ncert] - 1) = ':';
		*equal = '=';
	}
	return rc;
}

