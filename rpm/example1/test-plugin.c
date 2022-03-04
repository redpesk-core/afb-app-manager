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

/***************************************************


When entering (pre) the transactions set, TS, the transaction
elements, TE, are enumerated.

The TE are of two interesting case for us:
 - TR_ADDED: files are added
 - TR_REMOVED: files are removed

The comment on the function 'rpmtsOrder' clearly indicates that
removed items are put after added ones.

Bercause at end, when leaving TS (post), the TE don't anymore wear
the files, it is of interest to memorize it.

  sequence of INSTALL
 
	tsm_pre(APPLY)
 		psm_pre(TR_ADDED)   + files
		psm_post(TR_ADDED)  + files
		psm_pre(TR_ADDED)
		psm_post(TR_ADDED)
	tsm_post(APPLY)
 
 sequence of REMOVE

	tsm_pre(CHECK)
	tsm_post(CHECK)
	tsm_pre(APPLY)
 		psm_pre(TR_REMOVED)
		psm_post(TR_REMOVED)
		psm_pre(TR_REMOVED)  + files
		psm_post(TR_REMOVED) + files
	tsm_post(APPLY)
 
 sequence of UPGRADE and REINSTALL

	tsm_pre(APPLY)
 		psm_pre(TR_REMOVED)
		psm_post(TR_REMOVED)
		psm_pre(TR_ADDED)    + files
		psm_post(TR_ADDED)   + files
		psm_pre(TR_REMOVED)  + files
		psm_post(TR_REMOVED) + files
		psm_pre(TR_ADDED)
		psm_post(TR_ADDED)
	tsm_post(APPLY)

***************************************************/

typedef struct record {
	struct record *next;
	rpmts ts;
	rpmte te;
	rpmfiles files;
	char *seen;
	int nrfiles;
	int iter;
} record_t;

static record_t *records;

static void unrecordfiles(rpmts ts)
{
	record_t *it, **prv = &records;
	while((it = *prv) != NULL) {
		if (it->ts != ts)
			prv = &it->next;
		else {
			*prv = it->next;
			free(it->seen);
			rpmfilesFree(it->files);
			free(it);
		}
	}
}

static rpmRC recordfiles(rpmts ts)
{
	int idx, count = rpmtsNElements(ts);
	for(idx = 0 ; idx < count ; idx++) {
		record_t *it = malloc(sizeof *it);
		if (it == NULL) {
			unrecordfiles(ts);
			rpmlog(RPMLOG_ERR, "malloc failed");
			return RPMRC_FAIL;
		}
		it->ts = ts;
		it->te = rpmtsElement(ts, idx);
		it->files = rpmteFiles(it->te);
		it->seen = NULL;
		it->iter = 0;
		it->nrfiles = 0;
		it->next = records;
		records = it;
	}
	return RPMRC_OK;
}

static record_t *getrecord(rpmte te)
{
	record_t *it = records;
	while(it != NULL && it->te != te)
		it = it->next;
	return it;
}

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

static rpmRC question()
{
	for(;;) {
		char line[128];
		printf("ok [y/n]? ");
		fgets(line, sizeof line, stdin);
		if ((line[0] | ' ') == 'y')
			return RPMRC_OK;
		if ((line[0] | ' ') == 'n') {
			rpmlog(RPMLOG_ERR, "refused\n");
			return RPMRC_FAIL;
		}
		printf("sorry I haven't understood\n");
	}
}

static int apply(record_t *record, int add)
{
	rpmfi fi;
	const char *filename;
	const char *rootdir = rpmtsRootDir(record->ts);
	const char *name = rpmteN(record->te);
	printf("\n%s %s (%s)\n", add ? "ADDING" :  "REMOVING", name, rootdir);
	fi = rpmfilesIter(record->files, RPMFI_ITER_FWD);
	while (rpmfiNext(fi) >= 0) {
		filename = rpmfiFN(fi);
		printf("    - %s [%s]\n", filename, !access(filename, F_OK) ? "OK" : "NOT FOUND");
	}
	rpmfiFree(fi);
	return question();
}

static record_t *tracked = NULL;

static rpmRC track(record_t *record)
{
	rpmfi fi = rpmfilesIter(record->files, RPMFI_ITER_FWD);
	record->nrfiles = 0;
	while (rpmfiNext(fi) >= 0)
		record->nrfiles++;
	rpmfiFree(fi);
	if (record->seen == NULL) {
		record->seen = malloc(record->nrfiles * sizeof *record->seen);
		if (record->seen == NULL)
			return RPMRC_FAIL;
	}
	memset(record->seen, 0, record->nrfiles * sizeof *record->seen);
	tracked = record;
	return RPMRC_OK;
}

static void cleartrack()
{
	tracked = NULL;
}

static rpmRC fsm_file_prepare_cb(rpmPlugin plugin, rpmfi fi_,
#if defined(RPM_V_4_18)
                int fd,
#endif
		const char* path,
		const char *dest,
		mode_t file_mode,
		rpmFsmOp op)
{
	rpmRC rc = RPMRC_OK;

	printf("file prepare %s %s (%d -- %d -- %p)\n", path, dest, file_mode, op, fi_);

	if (tracked != NULL) {
		rpmfi fi = rpmfilesIter(tracked->files, RPMFI_ITER_FWD);
		int idx = 0;
		while (rpmfiNext(fi) >= 0) {
			if (strcmp(rpmfiFN(fi), dest) == 0) {
				if (tracked->seen[idx] == 0) {
					tracked->seen[idx] = 1;
					tracked->nrfiles--;
					if (tracked->nrfiles == 0)
						rc = apply(tracked, 1);
				}
				break;
			}
			idx++;
		}
		rpmfiFree(fi);
	}
	return rc;
}

static rpmRC psm_pre_cb(rpmPlugin plugin, rpmte te)
{
	record_t *record;
	rpmRC rc = RPMRC_OK;

	printf("\nTE.PRE\n");
	dump_te(te);

	record = getrecord(te);
	if (record != NULL) {
		record->iter++;
		if (record->iter == 1) {
			if (rpmteType(te) == TR_REMOVED)
				rc = apply(record, 0);
			else if (rpmteType(te) == TR_ADDED)
				rc = track(record);
		}
	}
	return rc;
}

static rpmRC psm_post_cb(rpmPlugin plugin, rpmte te, int res)
{
	record_t *record;
	rpmRC rc = RPMRC_OK;
	
	printf("\nTE.POST %d\n", res);
	dump_te(te);

	record = getrecord(te);
	if (record != NULL) {
		printf("iteration %d, ", record->iter);
		//return question();
	}
	cleartrack();

	return rc;
}

rpmRC tsm_pre_cb(rpmPlugin plugin, rpmts ts)
{
	printf("\n\nTS.PRE\n");
	//dump_ts(ts);
	return recordfiles(ts);
}

static rpmRC tsm_post_cb(rpmPlugin plugin, rpmts ts, int res)
{
	printf("\n\nTS.POST %d\n", res);
	//dump_ts(ts);
	unrecordfiles(ts);
	return RPMRC_OK;
}

rpmRC init_cb(rpmPlugin plugin, rpmts ts)
{
	return RPMRC_OK;
}

void cleanup_cb(rpmPlugin plugin)
{
}

struct rpmPluginHooks_s test_plugin_hooks = {
	.init = init_cb,
	.cleanup = cleanup_cb,
	.tsm_pre = tsm_pre_cb,
	.tsm_post = tsm_post_cb,
	.psm_pre = psm_pre_cb,
	.psm_post = psm_post_cb,
	.fsm_file_prepare = fsm_file_prepare_cb
};

