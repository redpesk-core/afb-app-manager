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

static const char *afm_system_daemon = "afm-system-daemon";

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
			rpmlog(RPMLOG_INFO, "Found %s, pid = %d, sending SIGHUP", afm_system_daemon, proc_info.tid);
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

	ifo = install_redpesk(dirname);
	if (!ifo) {
		rpmlog(RPMLOG_ERR, "Fail to install %s", dirname);
		return -1;
	}
	else {
		systemd_daemon_reload(0);
		systemd_unit_restart_name(0, "sockets.target");
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
	rc = uninstall_redpesk(dirname);
	if(rc)
		rpmlog(RPMLOG_ERR, "Fail to uninstall %s", dirname);
	else
		sighup_afm_main();
	return rc;
}

static char *lookingForConfigXml(rpmte te)
{
	char *dir = NULL;
	rpmfiles files = rpmteFiles(te);
	rpmfi fi = rpmfilesIter(files, RPMFI_ITER_FWD);

	while (rpmfiNext(fi) >= 0) {
		const char *filepath = rpmfiFN(fi);

		if (!strcmp("config.xml", basename((char *)filepath))) {
			dir = strdup(dirname((char *)filepath));
			rpmlog(RPMLOG_INFO, "config.xml found on %s\n", dir);
			break;
		}
	}

	rpmfiFree(fi);
	rpmfilesFree(files);
	return dir;
}

static rpmRC redpesk_psm_pre(rpmPlugin plugin, rpmte te)
{
	int rc;

	/* if transaction is not REMOVED, nothing to do */
	if (rpmteType(te) != TR_REMOVED)
		return RPMRC_OK;

	char *dirname = lookingForConfigXml(te);

	if (!dirname)
		return RPMRC_OK;

	rc = uninstall(dirname);
	free(dirname);
	if(rc)
		return RPMRC_FAIL;

	return RPMRC_OK;
}

static rpmRC redpesk_psm_post(rpmPlugin plugin, rpmte te, int res)
{
	/* if transaction fail no need to install redpesk */
	if (res != RPMRC_OK)
		return RPMRC_OK;

	/* if transaction is not install, nothing to do */
	if (rpmteType(te) != TR_ADDED)
		return RPMRC_OK;

	/* looking for config.xml directory */
	char *dirname = lookingForConfigXml(te);

	/* if not found, nothing to do */
	if (!dirname)
		return RPMRC_OK;

	/* try to install redpesk */
	if (install(dirname)) {
		free(dirname);
		return RPMRC_FAIL;
	}

	free(dirname);
	return RPMRC_OK;
}

struct rpmPluginHooks_s redpesk_hooks = {
	.psm_pre = redpesk_psm_pre,
	.psm_post = redpesk_psm_post,
};
