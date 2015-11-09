/*
 Copyright 2015 IoT.bzh

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
#include <syslog.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/uri.h>


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
static xmlNodePtr search_id(const char *id)
{
	char *val;
	xmlNodePtr iter, next;
	xmlNodePtr result;

	result = NULL;
	iter = xmlDocGetRootElement(document);
	while (iter != NULL) {
		val = xmlGetProp(iter, "Id");
		if (val != NULL && !strcmp(val, id)) {
			if (result != NULL) {
				syslog(LOG_ERR, "duplicated Id %s", id);
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
		syslog(LOG_ERR, "node of Id '%s' not found", id);
	return result;
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
		syslog(LOG_ERR, "attribute URI of element <Reference> not found");
		goto error;
	}

	/* parse the uri */
	u = xmlParseURI(uri);
	if (u == NULL) {
		syslog(LOG_ERR, "error while parsing URI %s", uri);
		goto error2;
	}

	if (u->scheme || u->opaque || u->authority || u->server || u->user || u->query) {
		syslog(LOG_ERR, "unexpected uri component in %s", uri);
		goto error3;
	}

	if (u->path && u->fragment) {
		syslog(LOG_ERR, "not allowed to sign foreign fragment in %s", uri);
		goto error3;
	}

	if (u->path) {
		fdesc = file_of_name(u->path);
		if (fdesc == NULL) {
			syslog(LOG_ERR, "reference to unknown file %s", u->path);
			goto error3;
		}
		if (fdesc->type != type_file) {
			syslog(LOG_ERR, "reference to directory %s", u->path);
			goto error3;
		}
		if ((fdesc->flags & flag_distributor_signature) != 0) {
			syslog(LOG_ERR, "reference to signature %s", u->path);
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
	xmlNodePtr elem;

	elem = sinfo->children;
	while (elem != NULL) {
		if (is_element(elem, "Reference"))
			if (check_one_reference(elem))
				return -1;
		elem = elem->next;
	}
	return 0;
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
						syslog(LOG_ERR, "xmlNodeGetContent of X509Certificate failed");
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

static int checkdocument()
{
	int rc;
	xmlNodePtr sinfo, svalue, kinfo, objs, rootsig;

	rc = -1;

	rootsig = xmlDocGetRootElement(document);
	if (!is_node(rootsig, "Signature")) {
		syslog(LOG_ERR, "root element <Signature> not found");
		goto error;
	}

	sinfo = next_element(rootsig->children);
	if (!is_node(sinfo, "SignedInfo")) {
		syslog(LOG_ERR, "element <SignedInfo> not found");
		goto error;
	}

	svalue = next_element(sinfo->next);
	if (!is_node(svalue, "SignatureValue")) {
		syslog(LOG_ERR, "element <SignatureValue> not found");
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

int verify_digsig(struct filedesc *fdesc)
{
	int res;

	assert ((fdesc->flags & flag_signature) != 0);
printf("\n\nchecking file %s\n\n",fdesc->name);

	/* reset the flags */
	file_clear_flags();
	clear_certificates();

	/* reads and xml parses the signature file */
	document = xmlReadFile(fdesc->name, NULL, 0);
	if (document == NULL) {
		syslog(LOG_ERR, "xml parse of file %s failed", fdesc->name);
		return -1;
	}

	res = checkdocument();
	if (res)
		syslog(LOG_ERR, "previous error was during check of file %s", fdesc->name);

	xmlFreeDoc(document);
	return res;
}

int check_all_signatures()
{
	int rc, irc;
	unsigned int i, n;
	struct filedesc *fdesc;

	n = signature_count();
	rc = 0;
	for (i = n ; i-- > 0 ; ) {
		fdesc = signature_of_index(i);
		assert ((fdesc->flags & flag_signature) != 0);
		irc = verify_digsig(fdesc);
		if (!irc)
			rc = irc;
	}

	return rc;
}

int create_digsig(int index, const char *key, const char **certs)
{
	struct filedesc *fdesc;
	xmlDocPtr doc;
	int rc, len;

	rc = -1;
	doc = xmlsec_create(index, key, certs);
	if (doc == NULL)
		goto error;

	fdesc = create_signature(index);
	if (fdesc == NULL)
		goto error2;

	len = xmlSaveFormatFileEnc(fdesc->name, doc, NULL, 0);
	if (len < 0) {
		syslog(LOG_ERR, "xmlSaveFormatFileEnc to %s failed", fdesc->name);
		goto error2;
	}

	rc = 0;
error2:
	xmlFreeDoc(doc);
error:
	return rc;
}


