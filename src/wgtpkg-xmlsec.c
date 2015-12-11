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


#include <syslog.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include <libxml/tree.h>
#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/crypto.h>
#include <xmlsec/templates.h>
#include <xmlsec/errors.h>
#include <xmlsec/io.h>


#include "wgtpkg.h"

static int initstatus;
static int initdone;
static xmlSecKeysMngrPtr keymgr;

#ifndef CA_ROOT_DIRECTORY
#define CA_ROOT_DIRECTORY "./ca-certificates"
#endif

/* checks if a file match  uri (should not be a distributor signature) */
static int file_match_cb(const char *uri)
{
	struct filedesc *fdesc = file_of_name(uri);
	return fdesc != NULL && fdesc->type == type_file && (fdesc->flags & flag_distributor_signature) == 0;
}

/* open the file of uri */
static void *file_open_cb(const char *file)
{
	struct filedesc *fdesc;
	int fd;
	FILE *f;

	fdesc = file_of_name(file);
	if (fdesc == NULL) {
		syslog(LOG_ERR, "shouldn't open uri %s", file);
		return NULL;
	}

	fd = openat(workdirfd, file, O_RDONLY);
	f = fd < 0 ? NULL : fdopen(fd, "r");
	if (f == NULL) {
		syslog(LOG_ERR, "can't open file %s for reading", file);
		if (fd >= 0)
			close(fd);
	} else
		fdesc->flags |= flag_opened;

	return f;
}

/* read the opened file */
static int file_read_cb(void *context, char *buffer, int len)
{
	size_t r = fread(buffer, 1, len, (FILE*)context);
	return r ? (int)r : feof((FILE*)context) ? 0 : - 1;
}

/* close the opened file */
static int file_close_cb(void *context)
{
	return (int)fclose((FILE*)context);
}

/* echo an error message */
static void errors_cb(const char *file, int line, const char *func, const char *errorObject, const char *errorSubject, int reason, const char *msg)
{
	syslog(LOG_ERR, "xmlSec error %3d: %s (subject=\"%s\", object=\"%s\")", reason, msg, errorSubject ? errorSubject : "?", errorObject ? errorObject : "?");
}

/* fills database with trusted keys */
static int fill_trusted_keys()
{
	int err;
	DIR *dir;
	struct dirent *ent;
	char path[PATH_MAX], *e;

	e = stpcpy(path, CA_ROOT_DIRECTORY);
	dir = opendir(path);
	if (!dir) {
		syslog(LOG_ERR, "opendir %s failed in fill_trusted_keys", path);
		return -1;
	}

	*e++ = '/';
	ent = readdir(dir);
	while (ent != NULL) {
		if (ent->d_type == DT_REG) {
			strcpy(e, ent->d_name);
			err = xmlSecCryptoAppKeysMngrCertLoad(keymgr, path, xmlSecKeyDataFormatPem, xmlSecKeyDataTypeTrusted);
			if (err < 0) {
				syslog(LOG_ERR, "xmlSecCryptoAppKeysMngrCertLoadMemory failed for %s", path);
				closedir(dir);
				return -1;
			}
		}
		ent = readdir(dir);
	}

	closedir(dir);
	return 0;
	
}

/* initialisation of access to xmlsec */
int xmlsec_init()
{

	if (initdone)
		goto end;

	initdone = 1;
	initstatus = -1;

	if(xmlSecInit() < 0) {
		syslog(LOG_ERR, "xmlSecInit failed.");
		goto end;
	}

#ifdef XMLSEC_CRYPTO_DYNAMIC_LOADING
	if(xmlSecCryptoDLLoadLibrary(XMLSEC_CRYPTO) < 0) {
		syslog(LOG_ERR, "xmlSecCryptoDLLoadLibrary %s failed.", XMLSEC_CRYPTO);
		goto end;
	}
#endif

	if(xmlSecCryptoAppInit(NULL) < 0) {
		syslog(LOG_ERR, "xmlSecCryptoAppInit failed.");
		goto end;
	}

	if(xmlSecCryptoInit() < 0) {
		syslog(LOG_ERR, "xmlSecCryptoInit failed.");
		goto end;
	}

	xmlSecErrorsSetCallback(errors_cb);

	xmlSecIOCleanupCallbacks();
	if (xmlSecIORegisterCallbacks(file_match_cb,
					file_open_cb, file_read_cb, file_close_cb)) {
		syslog(LOG_ERR, "xmlSecIORegisterCallbacks failed.");
		goto end;
	}

	keymgr = xmlSecKeysMngrCreate();
	if (keymgr == NULL) {
		syslog(LOG_ERR, "xmlSecKeysMngrCreate failed.");
		goto end;
	}

	if(xmlSecCryptoAppDefaultKeysMngrInit(keymgr) < 0) {
		syslog(LOG_ERR, "xmlSecCryptoAppDefaultKeysMngrInit failed.");
		goto end;
	}
	fill_trusted_keys();

	initstatus = 0;
end:
	return initstatus;
}

/* shuting down accesses to xmlsec */
void xmlsec_shutdown()
{
	xmlSecKeysMngrDestroy(keymgr);

	xmlSecCryptoShutdown();
	
	xmlSecCryptoAppShutdown();
	
	xmlSecShutdown();
}

/* verify a signature */
int xmlsec_verify(xmlNodePtr node)
{
	int rc;
	xmlSecDSigCtxPtr dsigctx;

	assert(initdone && !initstatus);

	dsigctx = xmlSecDSigCtxCreate(keymgr);
	if (dsigctx == NULL) {
		syslog(LOG_ERR, "xmlSecDSigCtxCreate failed.");
		rc = -1;
	} else {
		rc = xmlSecDSigCtxVerify(dsigctx, node);
		if (rc)
			syslog(LOG_ERR, "xmlSecDSigCtxVerify failed.");
		else if (dsigctx->status != xmlSecDSigStatusSucceeded) {
			syslog(LOG_ERR, "invalid signature.");
			rc = -1;
		}
		xmlSecDSigCtxDestroy(dsigctx);
	}

	return rc;
}

/* templates for properties of signature files */
static const struct { const char *id; const char *xml; } properties[2] = {
	{
		.id = "AuthorSignature", /* template of properties for author signature */
		.xml =
			"<SignatureProperties xmlns:dsp=\"http://www.w3.org/2009/xmldsig-properties\">"
			 "<SignatureProperty Id=\"profile\" Target=\"#AuthorSignature\">"
			  "<dsp:Profile URI=\"http://www.w3.org/ns/widgets-digsig#profile\"></dsp:Profile>"
			 "</SignatureProperty>"
			 "<SignatureProperty Id=\"role\" Target=\"#AuthorSignature\">"
			   "<dsp:Role URI=\"http://www.w3.org/ns/widgets-digsig#role-author\"></dsp:Role>"
			 "</SignatureProperty>"
			 "<SignatureProperty Id=\"identifier\" Target=\"#AuthorSignature\">"
			   "<dsp:Identifier></dsp:Identifier>"
			 "</SignatureProperty>"
			"</SignatureProperties>"
	},
	{
		.id = "DistributorSignature", /* template of properties for distributor signature */
		.xml = 
			"<SignatureProperties xmlns:dsp=\"http://www.w3.org/2009/xmldsig-properties\">"
			 "<SignatureProperty Id=\"profile\" Target=\"#DistributorSignature\">"
			  "<dsp:Profile URI=\"http://www.w3.org/ns/widgets-digsig#profile\"></dsp:Profile>"
			 "</SignatureProperty>"
			 "<SignatureProperty Id=\"role\" Target=\"#DistributorSignature\">"
			   "<dsp:Role URI=\"http://www.w3.org/ns/widgets-digsig#role-distributor\"></dsp:Role>"
			 "</SignatureProperty>"
			 "<SignatureProperty Id=\"identifier\" Target=\"#DistributorSignature\">"
			   "<dsp:Identifier></dsp:Identifier>"
			 "</SignatureProperty>"
			"</SignatureProperties>"
	}
};

/* create a signature of 'index' (0 for author, other values for distributors)
using the private 'key' (filename) and the certificates 'certs' (filenames)
as trusted chain */
xmlDocPtr xmlsec_create(int index, const char *key, const char **certs)
{
	unsigned int i, fc, mask;
	struct filedesc *fdesc;
	xmlNodePtr sign, obj, ref, kinfo, props;
	xmlDocPtr doc;
	int rc;
	xmlSecDSigCtxPtr dsigctx;

	assert(initdone && !initstatus);

	/* create the document */
	doc = xmlNewDoc("1.0");
	if (doc == NULL) {
		syslog(LOG_ERR, "xmlNewDoc failed");
		goto error;
	}

	/* create the root signature node */
	sign = xmlSecTmplSignatureCreate(doc, xmlSecTransformInclC14N11Id, xmlSecTransformRsaSha256Id, properties[!!index].id);
	if (sign == NULL) {
		syslog(LOG_ERR, "xmlSecTmplSignatureCreate failed");
		goto error2;
	}
	xmlDocSetRootElement(doc, sign);

	/* create the object and its reference */
	obj = xmlSecTmplSignatureAddObject(sign, "prop", NULL, NULL);
	if (obj == NULL) {
		syslog(LOG_ERR, "xmlSecTmplSignatureAddObject failed");
		goto error2;
	}
	rc = xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, properties[!!index].xml, &props);
	if (rc) {
		syslog(LOG_ERR, "xmlParseBalancedChunkMemory failed");
		goto error2;
	}
	if (NULL == xmlAddChild(obj, props)) {
		syslog(LOG_ERR, "filling object node failed");
		xmlFreeNode(obj);
		goto error2;
	}

	/* create references to files */
	mask = index ? flag_distributor_signature : flag_signature;
	fc = file_count();
	for (i = 0 ; i < fc ; i++) {
		fdesc = file_of_index(i);
		if (fdesc->type == type_file && (fdesc->flags & mask) == 0) {
			ref = xmlSecTmplSignatureAddReference(sign, xmlSecTransformSha256Id, NULL, fdesc->name, NULL);
			if (ref == NULL) {
				syslog(LOG_ERR, "creation of reference to %s failed", fdesc->name);
				goto error2;
			}
		}
	}

	/* create reference to object having properties */
	ref =  xmlSecTmplSignatureAddReference(sign, xmlSecTransformSha256Id, NULL, "#prop", NULL);
	if (ref == NULL) {
		syslog(LOG_ERR, "creation of reference to #prop failed");
		goto error2;
	}
	if (NULL == xmlSecTmplReferenceAddTransform(ref, xmlSecTransformInclC14N11Id)) {
		syslog(LOG_ERR, "setting transform reference to #prop failed");
		goto error2;
	}

	/* adds the X509 data */
	kinfo = xmlSecTmplSignatureEnsureKeyInfo(sign, NULL);
	if (kinfo == NULL) {
		syslog(LOG_ERR, "xmlSecTmplSignatureEnsureKeyInfo failed");
		goto error2;
	}
	if (NULL == xmlSecTmplKeyInfoAddX509Data(kinfo)) {
		syslog(LOG_ERR, "xmlSecTmplKeyInfoAddX509Data failed");
		goto error2;
	}

	/* sign now */
	dsigctx = xmlSecDSigCtxCreate(keymgr);
	if (dsigctx == NULL) {
		syslog(LOG_ERR, "xmlSecDSigCtxCreate failed.");
		goto error3;
	}
	dsigctx->signKey = xmlSecCryptoAppKeyLoad(key, xmlSecKeyDataFormatPem, NULL, NULL, NULL);
	if (dsigctx->signKey == NULL) {
		syslog(LOG_ERR, "loading key %s failed.", key);
		goto error3;
	}
	while (*certs) {
		if(xmlSecCryptoAppKeyCertLoad(dsigctx->signKey, *certs, xmlSecKeyDataFormatPem) < 0) {
			syslog(LOG_ERR, "loading certificate %s failed.", *certs);
			goto error3;
		}
		certs++;
	}
	if(xmlSecDSigCtxSign(dsigctx, sign) < 0) {
		syslog(LOG_ERR, "signing the document failed.");
		goto error3;
	}
	xmlSecDSigCtxDestroy(dsigctx);
	return doc;

error3:
	xmlSecDSigCtxDestroy(dsigctx);
error2:
	xmlFreeDoc(doc);
error:
	return NULL;
}

