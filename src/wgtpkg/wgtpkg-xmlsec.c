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


#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>

#include <xmlsec/xmlsec.h>
#include <xmlsec/crypto.h>
#include <xmlsec/errors.h>
#include <xmlsec/io.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/templates.h>

#include <rp-utils/rp-verbose.h>
#include "wgtpkg-files.h"
#include "wgtpkg-workdir.h"
#include "wgtpkg-xmlsec.h"

static int initstatus;
static int initdone;
static xmlSecKeysMngrPtr keymgr;

static const char trusted_certificates_directory[] = WGTPKG_TRUSTED_CERT_DIR;

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
		RP_ERROR("shouldn't open uri %s", file);
		return NULL;
	}

	fd = openat(workdirfd, file, O_RDONLY);
	f = fd < 0 ? NULL : fdopen(fd, "r");
	if (f == NULL) {
		RP_ERROR("can't open file %s for reading", file);
		if (fd >= 0)
			close(fd);
	} else
		fdesc->flags |= flag_opened;

	return f;
}

/* read the opened file */
static int file_read_cb(void *context, char *buffer, int len)
{
	size_t r = fread(buffer, 1, (unsigned)len, (FILE*)context);
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
	RP_ERROR("xmlSec error %3d: %s (subject=\"%s\", object=\"%s\")", reason, msg, errorSubject ? errorSubject : "?", errorObject ? errorObject : "?");
}

/* fills database with trusted keys */
static int fill_trusted_keys_file(const char *file)
{
	int err = xmlSecCryptoAppKeysMngrCertLoad(keymgr, file, xmlSecKeyDataFormatPem, xmlSecKeyDataTypeTrusted);
	if (err < 0) {
		RP_ERROR("xmlSecCryptoAppKeysMngrCertLoadMemory failed for %s", file);
		return -1;
	}
	return 0;
}

/* fills database with trusted keys */
static int fill_trusted_keys_dir(const char *directory)
{
	int err;
	DIR *dir;
	struct dirent *ent;
	char path[PATH_MAX], *e;

	e = stpcpy(path, directory);
	dir = opendir(path);
	if (!dir) {
		RP_ERROR("opendir %s failed in fill_trusted_keys_dir", path);
		return -1;
	}

	*e++ = '/';
	ent = readdir(dir);
	while (ent != NULL) {
		if (ent->d_type == DT_REG) {
			strcpy(e, ent->d_name);
			err = fill_trusted_keys_file(path);
			if (err < 0) {
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
		RP_ERROR("xmlSecInit failed.");
		goto end;
	}

#ifdef XMLSEC_CRYPTO_DYNAMIC_LOADING
	if(xmlSecCryptoDLLoadLibrary(XMLSEC_CRYPTO) < 0) {
		RP_ERROR("xmlSecCryptoDLLoadLibrary %s failed.", XMLSEC_CRYPTO);
		goto end;
	}
#endif

	if(xmlSecCryptoAppInit(NULL) < 0) {
		RP_ERROR("xmlSecCryptoAppInit failed.");
		goto end;
	}

	if(xmlSecCryptoInit() < 0) {
		RP_ERROR("xmlSecCryptoInit failed.");
		goto end;
	}

	xmlSecErrorsSetCallback(errors_cb);

	xmlSecIOCleanupCallbacks();
	if (xmlSecIORegisterCallbacks(file_match_cb,
					file_open_cb, file_read_cb, file_close_cb)) {
		RP_ERROR("xmlSecIORegisterCallbacks failed.");
		goto end;
	}

	keymgr = xmlSecKeysMngrCreate();
	if (keymgr == NULL) {
		RP_ERROR("xmlSecKeysMngrCreate failed.");
		goto end;
	}

	if(xmlSecCryptoAppDefaultKeysMngrInit(keymgr) < 0) {
		RP_ERROR("xmlSecCryptoAppDefaultKeysMngrInit failed.");
		goto end;
	}
	fill_trusted_keys_dir(trusted_certificates_directory);

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
		RP_ERROR("xmlSecDSigCtxCreate failed.");
		rc = -1;
	} else {
		rc = xmlSecDSigCtxVerify(dsigctx, node);
		if (rc)
			RP_ERROR("xmlSecDSigCtxVerify failed.");
		else if (dsigctx->status != xmlSecDSigStatusSucceeded) {
			RP_ERROR("invalid signature.");
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
xmlDocPtr xmlsec_create(unsigned int index, const char *key, const char **certs)
{
	unsigned int i, fc, mask;
	struct filedesc *fdesc;
	xmlNodePtr sign, obj, ref, kinfo, props;
	xmlDocPtr doc;
	int rc;
	xmlSecDSigCtxPtr dsigctx;

	assert(initdone && !initstatus);

	/* create the document */
	doc = xmlNewDoc((const xmlChar*)"1.0");
	if (doc == NULL) {
		RP_ERROR("xmlNewDoc failed");
		goto error;
	}

	/* create the root signature node */
	sign = xmlSecTmplSignatureCreate(doc, xmlSecTransformInclC14N11Id, xmlSecTransformRsaSha1Id, (const xmlChar*)properties[!!index].id);
	if (sign == NULL) {
		RP_ERROR("xmlSecTmplSignatureCreate failed");
		goto error2;
	}
	xmlDocSetRootElement(doc, sign);

	/* create the object and its reference */
	obj = xmlSecTmplSignatureAddObject(sign, (const xmlChar*)"prop", NULL, NULL);
	if (obj == NULL) {
		RP_ERROR("xmlSecTmplSignatureAddObject failed");
		goto error2;
	}
	rc = xmlParseBalancedChunkMemory(doc, NULL, NULL, 0, (const xmlChar*)properties[!!index].xml, &props);
	if (rc) {
		RP_ERROR("xmlParseBalancedChunkMemory failed");
		goto error2;
	}
	if (NULL == xmlAddChild(obj, props)) {
		RP_ERROR("filling object node failed");
		xmlFreeNode(obj);
		goto error2;
	}

	/* create references to files */
	mask = index ? flag_distributor_signature : flag_signature;
	fc = file_count();
	for (i = 0 ; i < fc ; i++) {
		fdesc = file_of_index(i);
		if (fdesc->type == type_file && (fdesc->flags & mask) == 0) {
			ref = xmlSecTmplSignatureAddReference(sign, xmlSecTransformSha1Id, NULL, (const xmlChar*)fdesc->name, NULL);
			if (ref == NULL) {
				RP_ERROR("creation of reference to %s failed", fdesc->name);
				goto error2;
			}
		}
	}

	/* create reference to object having properties */
	ref =  xmlSecTmplSignatureAddReference(sign, xmlSecTransformSha1Id, NULL, (const xmlChar*)"#prop", NULL);
	if (ref == NULL) {
		RP_ERROR("creation of reference to #prop failed");
		goto error2;
	}
	if (NULL == xmlSecTmplReferenceAddTransform(ref, xmlSecTransformInclC14N11Id)) {
		RP_ERROR("setting transform reference to #prop failed");
		goto error2;
	}

	/* adds the X509 data */
	kinfo = xmlSecTmplSignatureEnsureKeyInfo(sign, NULL);
	if (kinfo == NULL) {
		RP_ERROR("xmlSecTmplSignatureEnsureKeyInfo failed");
		goto error2;
	}
	if (NULL == xmlSecTmplKeyInfoAddX509Data(kinfo)) {
		RP_ERROR("xmlSecTmplKeyInfoAddX509Data failed");
		goto error2;
	}

	/* sign now */
	dsigctx = xmlSecDSigCtxCreate(keymgr);
	if (dsigctx == NULL) {
		RP_ERROR("xmlSecDSigCtxCreate failed.");
		goto error3;
	}
	dsigctx->signKey = xmlSecCryptoAppKeyLoad(key, xmlSecKeyDataFormatPem, NULL, NULL, NULL);
	if (dsigctx->signKey == NULL) {
		RP_ERROR("loading key %s failed.", key);
		goto error3;
	}
	while (*certs) {
		if(xmlSecCryptoAppKeyCertLoad(dsigctx->signKey, *certs, xmlSecKeyDataFormatPem) < 0) {
			RP_ERROR("loading certificate %s failed.", *certs);
			goto error3;
		}
		certs++;
	}
	if(xmlSecDSigCtxSign(dsigctx, sign) < 0) {
		RP_ERROR("signing the document failed.");
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

