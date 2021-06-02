#include <stdio.h>
#include <libgen.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <proc/readproc.h>
#include <string.h>
#include <signal.h>

#include <rpm/rpmlog.h>
#include <rpm/rpmts.h>
#include <rpm-plugins/rpmplugin.h>

#include "wgtpkg-install.h"
#include "wgtpkg-uninstall.h"
#include "afm-udb.h"
#include "utils-systemd.h"
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
static const char *afm_system_daemon = "afm-system-daemon";

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
		case TR_RPMDB: return "RPMTYPE TR_RPMDB";
		default: return "RPMTYPE UNKNOWN";
	}
}

static int sighup_afm_main()
{
	int rc = 0;
	PROCTAB* proc;
	proc_t proc_info;
	memset(&proc_info, 0, sizeof(proc_info));

	/* get processes: flag to fill cmdline */
	proc = openproc(PROC_FILLCOM);
	if(!proc)
		return -1;

	/* looking for cmdline with afm-system-daemon */
	while (readproc(proc, &proc_info)) {
		if(!proc_info.cmdline)
			continue;

		if(strstr(*proc_info.cmdline, afm_system_daemon)) {
			rpmlog(RPMLOG_INFO, "Found %s, pid = %d, sending SIGHUP\n", afm_system_daemon, proc_info.tid);
			kill(proc_info.tid, SIGHUP);
			goto done;
		}
	}
	rc = -2;

done:
	closeproc(proc);
	return rc;
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
		systemd_daemon_reload(0);
		systemd_unit_restart_name(0, "sockets.target", NULL);
		sighup_afm_main();

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
