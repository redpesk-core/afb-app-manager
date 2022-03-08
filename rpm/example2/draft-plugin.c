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
#include <rpm/rpmte.h>
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

/* debug te type */
static const char * type_te(rpmte te)
{
	switch (rpmteType(te)) {
		case TR_ADDED: return "TR_ADDED";
		case TR_REMOVED: return "TR_REMOVED";
		case TR_RPMDB: return "TR_RPMDB";
		default: return "UNKNOWN";
	}
}

static void dump_te(rpmte te)
{
	const char *filename;
	const char *name = rpmteN(te);
	rpmfiles files;
	rpmfi fi;

	printf("- TE %p\n", te);
	printf("  type ...: %d = %s\n", (int)rpmteType(te), type_te(te));
	printf("  name (N): %s (%p)\n", name, name);
	files = rpmteFiles(te);
	if (files == NULL)
		printf("  no files\n");
	else {
		printf("  files:\n");
		fi = rpmfilesIter(files, RPMFI_ITER_FWD);
		while (rpmfiNext(fi) >= 0) {
			filename = rpmfiFN(fi);
			printf("    - %s [%s]\n", filename, !access(filename, F_OK) ? "EXISTS" : "NOT FOUND");
		}
		rpmfiFree(fi);
		rpmfilesFree(files);
	}
}

static void dump_ts(rpmts ts)
{
	rpmte te;
	int idx;

	printf("TS %p %d\n", ts, rpmtsGetTid(ts));
	printf("At root %s\n", rpmtsRootDir(ts));
	printf("Elements [%d] by index\n", rpmtsNElements(ts));
	idx = 0;
	while((te = rpmtsElement(ts, idx)) != NULL) {
		printf("\n[TE %d]\n", idx);
		dump_te(te);
		idx++;
	}
}

/***************************************************/

typedef struct record {
	struct record *next;
	rpmts ts;
	rpmte te;
	rpmfiles files;
} record_t;

static record_t *records;


static void apply(rpmts ts, rpmte te, rpmfiles files, int add)
{
	rpmfi fi;
	const char *filename;
	char path[PATH_MAX];
	const char *rootdir = rpmtsRootDir(ts);
	const char *name = rpmteN(te);
	const char *transid = getenv("REDPESK_RPMPLUG_TRANSID");
	printf("\n%s %s (%s)\n", add ? "ADDING" :  "REMOVING", name, rootdir);
	printf("BEGIN %s\n", add ? "ADD" :  "ERASE");
	printf("PACKAGE %s\n", name);
	if (rootdir)
		printf("ROOT %s\n", rootdir);
	if (transid)
		printf("TRANSID %s\n", transid);
	fi = rpmfilesIter(files, RPMFI_ITER_FWD);
	while (rpmfiNext(fi) >= 0) {
		filename = rpmfiFN(fi);
		sprintf(path, "%s/%s", rootdir, filename);
		printf("FILE %s [%s]\n", filename, !access(path, F_OK) ? "OK" : "NOT FOUND");
	}
	rpmfiFree(fi);
	printf("END %s\n", add ? "ADD" :  "ERASE");
}

rpmRC tsm_pre_cb(rpmPlugin plugin, rpmts ts)
{
	record_t *it;
	rpmfiles files;
	rpmte te;
	int idx, count;

	//printf("\n\nTS.PRE\n");

	//dump_ts(ts);

	count = rpmtsNElements(ts);
	for(idx = 0 ; idx < count ; idx++) {
		te = rpmtsElement(ts, idx);
		files = rpmteFiles(te);
		/* can add filtering here */
		switch (rpmteType(te)) {
		case TR_ADDED:
			it = malloc(sizeof *it);
			if (it == NULL) {
				rpmlog(RPMLOG_ERR, "malloc failed");
				return RPMRC_FAIL;
			}
			it->ts = ts;
			it->te = te;
			it->files = files;
			it->next = records;
			records = it;
			break;
		case TR_REMOVED:
			apply(ts, te, files, 0);
			rpmfilesFree(files);
			break;
		default:
			rpmfilesFree(files);
			break;
		}
	}

	return RPMRC_OK;
}

static rpmRC tsm_post_cb(rpmPlugin plugin, rpmts ts, int res)
{
	record_t *it, **prv;

	//printf("\n\nTS.POST %d\n", res);

	//dump_ts(ts);

	prv = &records;
	while((it = *prv) != NULL) {
		if (it->ts != ts)
			prv = &it->next;
		else {
			*prv = it->next;
			if (res == RPMRC_OK)
				apply(ts, it->te, it->files, 1);
			rpmfilesFree(it->files);
			free(it);
		}
	}
	return RPMRC_OK;
}

struct rpmPluginHooks_s draft_plugin_hooks = {
	.tsm_pre = tsm_pre_cb,
	.tsm_post = tsm_post_cb
};

