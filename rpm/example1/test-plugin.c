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

/***************************************************
* RPM plugins MUST have a NAME following C naming
* conventions: characters, digits, underscores
*
* The library of the plugin MUST be named:
*
*    NAME.so
*
* For transaction (no other case known), the RPM
* environment must include a declaration of the
* form:
*
*   __transaction_NAME %{__plugindir}/NAME.so
*
* RPM searchs for the symbol named:
*
*   NAME_hooks
***************************************************/

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
 
 sequence of UPGRADE and REINSTALL

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

***************************************************/

struct pkgs_cb {
	char *name; //package name
	int toinstall;
	int installed;
	int toremove;
	struct pkgs_cb *next;
};

/* packages list */
static struct pkgs_cb *rpkgs_list = NULL;

/* debug te type */
static const char * type_te(rpmte te)
{
	switch (rpmteType(te)) {
		case TR_ADDED: return "TR_ADDED";
		case TR_REMOVED: return "TR_REMOVED";
		default: return "UNKNOWN";
	}
}

static void print_files(rpmte te)
{
	rpmfiles files = rpmteFiles(te);
	rpmfi fi = rpmfilesIter(files, RPMFI_ITER_FWD);

	rpmlog(RPMLOG_DEBUG, "************ ((FILE %p))\n", files);
	while (rpmfiNext(fi) >= 0) {
		const char *filepath = rpmfiFN(fi);
		rpmlog(RPMLOG_DEBUG, "************ FILE %s\n", filepath);
	}

	rpmfiFree(fi);
	rpmfilesFree(files);
}


static int install(struct pkgs_cb*pkg)
{
	rpmlog(RPMLOG_DEBUG, "************ install\n");
	return 0;
}

static int uninstall(struct pkgs_cb*pkg)
{
	rpmlog(RPMLOG_DEBUG, "************ uninstall\n");
	return 0;
}

static struct pkgs_cb *get_node_from_list(const char* name)
{
	if(!rpkgs_list)
		return NULL;

	struct pkgs_cb *node = rpkgs_list;
	while (node != NULL) {
		if (!strcmp(node->name, name))
			return node;
		node = node->next;
	}
	return NULL;
}

static rpmRC psm_pre_cb(rpmPlugin plugin, rpmte te)
{
	int rc;
	const char *name = rpmteN(te);

	rpmlog(RPMLOG_DEBUG, "************ psm_pre_cb pkg=%s type=%s\n", name, type_te(te));
	print_files(te);

	struct pkgs_cb * pkg = get_node_from_list(name);
	if(!pkg) {



		/* add element pkg to list */
		pkg = (struct pkgs_cb*)malloc(sizeof(struct pkgs_cb));
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
		pkg->toinstall = 0;
		pkg->installed = 0;
		pkg->toremove = rpmteType(te) == TR_REMOVED; //initialised with TR_REMOVED means upgrading if there will be an install
		pkg->next = rpkgs_list;
		rpkgs_list = pkg;
	}

	/* need to install_cb at the end */
	if(rpmteType(te) == TR_ADDED)
		pkg->toinstall = 1;

	/* uninstall */
	if(pkg->toremove) {
		rc = uninstall(pkg);
		if(rc)
			rpmlog(RPMLOG_WARNING, "issue uninstall lsm context: carry on removing package anyway\n");
	}

	return RPMRC_OK;
}

static rpmRC psm_post_cb(rpmPlugin plugin, rpmte te, int res)
{
	rpmRC ret = RPMRC_OK;
	const char *name = rpmteN(te);

	rpmlog(RPMLOG_DEBUG, "************ psm_post_cb pkg=%s type=%s\n", name, type_te(te));
	print_files(te);

	struct pkgs_cb * pkg = get_node_from_list(name);
	if(!pkg) {
		rpmlog(RPMLOG_DEBUG, "No pkg in list for %s\n", name);
		return RPMRC_OK;
	}

	/* if toremove and need to install: install */
	if(!pkg->toremove && !pkg->installed && pkg->toinstall) {
		/* try to install test_plugin */
		pkg->installed = 1;
		if (install(pkg))
			ret = RPMRC_FAIL;
	}

	/* Clearing toremove is needed on upgrade to allow install in later TR_REMOVED */
	if(rpmteType(te) == TR_ADDED)
		pkg->toremove = 0;

	return ret;
}

rpmRC tsm_pre_cb(rpmPlugin plugin, rpmts ts)
{
	rpmlog(RPMLOG_DEBUG, "************ tsm_pre_cb %s\n", getenv("TOTO") ?: "????");
	rpmlog(RPMLOG_DEBUG, "************ tsm_pre_cb %s\n", rpmtsRootDir(ts));
	return RPMRC_OK;
}

static rpmRC tsm_post_cb(rpmPlugin plugin, rpmts ts, int res)
{
	rpmlog(RPMLOG_DEBUG, "************ tsm_post_cb\n");

	/* free list */
	while(rpkgs_list != NULL) {
		struct pkgs_cb *node = rpkgs_list;
		rpkgs_list = node->next;
		free(node->name);
		free(node);
	}
	return RPMRC_OK;
}

rpmRC init_cb(rpmPlugin plugin, rpmts ts)
{
	rpmlog(RPMLOG_DEBUG, "************ init_cb\n");
	return RPMRC_OK;
}

void cleanup_cb(rpmPlugin plugin)
{
	rpmlog(RPMLOG_DEBUG, "************ cleanup_cb\n");
}

struct rpmPluginHooks_s test_plugin_hooks = {
	.init = init_cb,
	.cleanup = cleanup_cb,
	.tsm_pre = tsm_pre_cb,
	.tsm_post = tsm_post_cb,
	.psm_pre = psm_pre_cb,
	.psm_post = psm_post_cb,
};

