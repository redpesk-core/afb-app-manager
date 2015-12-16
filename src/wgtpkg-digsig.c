/*
 Copyright 2015 IoT.bzh

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


#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <libxml/xmlsave.h>


#include "verbose.h"
#include "wgtpkg.h"



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
				ERROR("duplicated %s %s", attrname, value);
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
		ERROR("node of %s '%s' not found", attrname, value);
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
		ERROR("attribute URI of element <Reference> not found");
		goto error;
	}

	/* parse the uri */
	u = xmlParseURI(uri);
	if (!u) {
		ERROR("error while parsing URI %s", uri);
		goto error2;
	}

	/* check that unexpected parts are not there */
	if (u->scheme || u->opaque || u->authority || u->server || u->user || u->query) {
		ERROR("unexpected uri component in %s", uri);
		goto error3;
	}

	/* check path and fragment */
	if (!u->path && !u->fragment) {
		ERROR("invalid uri %s", uri);
		goto error3;
	}
	if (u->path && u->fragment) {
		ERROR("not allowed to sign foreign fragment in %s", uri);
		goto error3;
	}

	if (u->path) {
		/* check that the path is valid */
		fdesc = file_of_name(u->path);
		if (fdesc == NULL) {
			ERROR("reference to unknown file %s", u->path);
			goto error3;
		}
		if (fdesc->type != type_file) {
			ERROR("reference to directory %s", u->path);
			goto error3;
		}
		if ((fdesc->flags & flag_distributor_signature) != 0) {
			ERROR("reference to signature %s", u->path);
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
				ERROR("file not referenced in signature: %s", f->name);
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
						ERROR("xmlNodeGetContent of X509Certificate failed");
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
		ERROR("root element <Signature> not found");
		goto error;
	}

	sinfo = next_element(rootsig->children);
	if (!is_node(sinfo, "SignedInfo")) {
		ERROR("element <SignedInfo> not found");
		goto error;
	}

	svalue = next_element(sinfo->next);
	if (!is_node(svalue, "SignatureValue")) {
		ERROR("element <SignatureValue> not found");
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
	DEBUG("-- checking file %s",fdesc->name);

	/* reset the flags */
	file_clear_flags();
	clear_certificates();

	/* reads and xml parses the signature file */
	fd = openat(workdirfd, fdesc->name, O_RDONLY);
	if (fd < 0) {
		ERROR("cant't open file %s", fdesc->name);
		return -1;
	}
	document = xmlReadFd(fd, fdesc->name, NULL, 0);
	close(fd);
	if (document == NULL) {
		ERROR("xml parse of file %s failed", fdesc->name);
		return -1;
	}

	res = checkdocument();
	if (res)
		ERROR("previous error was during check of file %s", fdesc->name);

	xmlFreeDoc(document);
	return res;
}

/* check all the signature files */
int check_all_signatures()
{
	int rc, irc;
	unsigned int i, n;
	struct filedesc *fdesc;

	n = signature_count();
	rc = 0;
	for (i = n ; i-- > 0 ; ) {
		fdesc = signature_of_index(i);
		irc = verify_digsig(fdesc);
		if (!irc)
			rc = irc;
	}

	return rc;
}

/* create a signature of 'index' (0 for author, other values for distributors)
using the private 'key' (filename) and the certificates 'certs' (filenames)
as trusted chain */
int create_digsig(int index, const char *key, const char **certs)
{
	struct filedesc *fdesc;
	xmlDocPtr doc;
	int rc, len, fd;
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
		ERROR("cant open %s for write", fdesc->name);
		goto error2;
	}
	ctx = xmlSaveToFd(fd, NULL, XML_SAVE_FORMAT);
	if (!ctx) {
		ERROR("xmlSaveToFd failed for %s", fdesc->name);
		goto error3;
	}
	len = xmlSaveDoc(ctx, doc);
	if (len < 0) {
		ERROR("xmlSaveDoc to %s failed", fdesc->name);
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


