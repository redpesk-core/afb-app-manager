/*
 Copyright (C) 2015-2023 IoT.bzh Company

 Author: Clément Bénier <clement.benier@iot.bzh>

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
#include <libgen.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm-plugins/rpmplugin.h>

#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"
#include "afm-udb.h"
#include "sighup-framework.h"
#include "wgt-info.h"

/***************************************************
  sequence of INSTALL

	tsm_pre(CHECK)
	tsm_post(CHECK)
	tsm_pre(APPLY)
 		psm_pre(TR_ADDED)   + files   -> [toinstall,!installed,!toremove,installdir]
		psm_post(TR_ADDED)  + files   -> [toinstall,!installed,!toremove,installdir]
			- install -
		psm_pre(TR_ADDED)             -> [!toinstall,installed,!toremove,installdir]
		psm_post(TR_ADDED)            -> [!toinstall,installed,!toremove,installdir]
	tsm_post(APPLY)

 sequence of REMOVE

	tsm_pre(CHECK)
	tsm_post(CHECK)
	tsm_pre(APPLY)
 		psm_pre(TR_REMOVED)           -> [!toinstall,!installed,toremove,!installdir]
		psm_post(TR_REMOVED)          -> [!toinstall,!installed,toremove,!installdir]
		psm_pre(TR_REMOVED)  + files  -> [!toinstall,!installed,toremove,installdir]
			- remove -
		psm_post(TR_REMOVED) + files  -> [!toinstall,!installed,toremove,installdir]
	tsm_post(APPLY)

 sequence of UPGRADE

	tsm_pre(CHECK)
	tsm_post(CHECK)
	tsm_pre(APPLY)
 		psm_pre(TR_REMOVED)           -> [!toinstall,!installed,toremove,!installdir]
		psm_post(TR_REMOVED)          -> [!toinstall,!installed,toremove,!installdir]
		psm_pre(TR_ADDED)    + files  -> [toinstall,!installed,toremove,installdir]
			- remove -
		psm_post(TR_ADDED)   + files  -> [toinstall,!installed,!toremove,installdir]
		psm_pre(TR_REMOVED)  + files  -> [toinstall,!installed,!toremove,installdir]
		psm_post(TR_REMOVED) + files  -> [toinstall,installed,!toremove,installdir]
			- install -
		psm_pre(TR_ADDED)             -> [toinstall,installed,!toremove,installdir]
		psm_post(TR_ADDED)            -> [toinstall,installed,!toremove,installdir]
	tsm_post(APPLY)

 */

struct redpesk_pkgs {
	char *name; //package name
	char *installdir; //path to install directory
	int toinstall;
	int installed;
	int toremove;
	struct redpesk_pkgs *next;
};

/* packages list */
static struct redpesk_pkgs *rpkgs_list = NULL;

/* debug te type */
static const char * typestring(rpmte te)
{
	switch (rpmteType(te)) {
		case TR_ADDED: return "RPMTYPE TR_ADDED";
		case TR_REMOVED: return "RPMTYPE TR_REMOVED";
		default: return "RPMTYPE UNKNOWN";
	}
}

static int install(const char *dirname)
{
	struct wgt_info *ifo;

	rpmlog(RPMLOG_DEBUG, "[REDPESK]: install %s\n", dirname);
	ifo = install_redpesk(dirname);
	if (!ifo) {
		rpmlog(RPMLOG_ERR, "Fail to install %s\n", dirname);
		return -1;
	}
	else {
		sighup_all();
		/* clean-up */
		wgt_info_unref(ifo);
	}
	return 0;
}

static int uninstall(const char *dirname)
{
	int rc;
	/* uninstall the widget */
	rpmlog(RPMLOG_DEBUG, "[REDPESK]: uninstall %s\n", dirname);
	rc = uninstall_redpesk(dirname);
	if(rc)
		rpmlog(RPMLOG_ERR, "Fail to uninstall %s\n", dirname);
	else
		sighup_afm_main();
	return rc;
}

static int getInstallDir(rpmte te, char **ptrDir)
{
	static const char configxml[] = "/config.xml";
	int result = 0;
	rpmfiles files = rpmteFiles(te);
	rpmfi fi = rpmfilesIter(files, RPMFI_ITER_FWD);

	*ptrDir = NULL;
	while (result == 0 && rpmfiNext(fi) >= 0) {
		const char *filepath = rpmfiFN(fi);
		size_t length = strlen(filepath);
		if (length >= sizeof(configxml) - 1 /* assumes absolute path */
		  && !strcmp(configxml, &filepath[length - sizeof(configxml) + 1])) {
			*ptrDir = strndup(filepath, length - sizeof(configxml) + 1);
			result = *ptrDir != NULL ? 1 : -1;
		}
	}

	rpmfiFree(fi);
	rpmfilesFree(files);
	return result;
}

static struct redpesk_pkgs *get_node_from_list(const char* name)
{
	if(!rpkgs_list)
		return NULL;

	struct redpesk_pkgs *node = rpkgs_list;
	while (node != NULL) {
		if (!strcmp(node->name, name))
			return node;
		node = node->next;
	}
	return NULL;
}

static rpmRC redpesk_psm_pre(rpmPlugin plugin, rpmte te)
{
	int rc;
	const char *name = rpmteN(te);
	char *dirname;

	rpmlog(RPMLOG_DEBUG, "[REDPESK]: redpesk_psm_pre pkg=%s type=%s\n", name, typestring(te));

	struct redpesk_pkgs * pkg = get_node_from_list(name);
	if(!pkg) {
		/* add element pkg to list */
		pkg = (struct redpesk_pkgs*)malloc(sizeof(struct redpesk_pkgs));
		if(pkg == NULL) {
			rpmlog(RPMLOG_ERR, "malloc failed for %s", name);
			return RPMRC_FAIL;
		}
		pkg->name = strdup(name);
		if(pkg->name == NULL) {
			free(pkg);
			rpmlog(RPMLOG_ERR, "malloc failed for %s", name);
			return RPMRC_FAIL;
		}
		pkg->installdir = NULL;
		pkg->toinstall = 0;
		pkg->installed = 0;
		pkg->toremove = rpmteType(te) == TR_REMOVED; //initialised with TR_REMOVED means upgrading if there will be an install
		pkg->next = rpkgs_list;
		rpkgs_list = pkg;
	}

	/* get install directory */
	dirname = pkg->installdir;
	if (dirname == NULL) {
		rc = getInstallDir(te, &dirname);
		if (rc == 0)
			return RPMRC_OK;
		if (rc < 0) {
			rpmlog(RPMLOG_ERR, "malloc failed for %s", name);
			return RPMRC_FAIL;
		}
		pkg->installdir = dirname;
	}

	/* need to redpesk_install at the end */
	if(rpmteType(te) == TR_ADDED)
		pkg->toinstall = 1;

	/* uninstall */
	if(pkg->toremove) {
		rpmlog(RPMLOG_DEBUG, "[REDPESK]: uninstall %s\n", dirname);
		rc = uninstall(dirname);
		if(rc)
			rpmlog(RPMLOG_WARNING, "issue uninstall lsm context: carry on removing package anyway\n");
	}

	return RPMRC_OK;
}

static rpmRC redpesk_psm_post(rpmPlugin plugin, rpmte te, int res)
{
	rpmRC ret = RPMRC_OK;
	const char *name = rpmteN(te);

	rpmlog(RPMLOG_DEBUG, "[REDPESK]: redpesk_psm_post pkg=%s type=%s\n", name, typestring(te));

	struct redpesk_pkgs * pkg = get_node_from_list(name);
	if(!pkg) {
		rpmlog(RPMLOG_DEBUG, "No pkg in list for %s\n", name);
		return RPMRC_OK;
	}

	/* if toremove and need to install: install */
	if(!pkg->toremove && !pkg->installed && pkg->toinstall) {
		/* try to install redpesk */
		rpmlog(RPMLOG_DEBUG, "[REDPESK]: install %s\n", pkg->installdir);
		pkg->installed = 1;
		if (install(pkg->installdir))
			ret = RPMRC_FAIL;
	}

	/* Clearing toremove is needed on upgrade to allow install in later TR_REMOVED */
	if(rpmteType(te) == TR_ADDED)
		pkg->toremove = 0;

	return ret;
}

static rpmRC redpesk_tsm_post(rpmPlugin plugin, rpmts ts, int res)
{
	rpmlog(RPMLOG_DEBUG, "[REDPESK]: redpesk_tsm_post\n");

	/* free list */
	while(rpkgs_list != NULL) {
		struct redpesk_pkgs *node = rpkgs_list;
		rpkgs_list = node->next;
		free(node->name);
		free(node->installdir);
		free(node);
	}
	return RPMRC_OK;
}

struct rpmPluginHooks_s redpesk_hooks = {
	.tsm_post = redpesk_tsm_post,
	.psm_pre = redpesk_psm_pre,
	.psm_post = redpesk_psm_post,
};
