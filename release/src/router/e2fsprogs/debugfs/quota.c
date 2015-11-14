/*
 * quota.c --- debugfs quota commands
 *
 * Copyright (C) 2014 Theodore Ts'o.  This file may be redistributed
 * under the terms of the GNU Public License.
 */

#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int optind;
extern char *optarg;
#endif

#include "debugfs.h"

const char *quota_type[] = { "user", "group", NULL };

static int load_quota_ctx(char *progname)
{
	errcode_t	retval;

	if (check_fs_open(progname))
		return 1;

	if (!EXT2_HAS_RO_COMPAT_FEATURE(current_fs->super,
					EXT4_FEATURE_RO_COMPAT_QUOTA)) {
		com_err(progname, 0, "quota feature not enabled");
		return 1;
	}

	if (current_qctx)
		return 0;

	retval = quota_init_context(&current_qctx, current_fs, -1);
	if (retval) {
		com_err(current_fs->device_name, retval,
			"while trying to load quota information");
		return 1;
	}
	return 0;
}

static int parse_quota_type(const char *cmdname, const char *str)
{
	errcode_t	retval;
	char		*t;
	int		flags = 0;
	int		i;

	for (i = 0; i < MAXQUOTAS; i++) {
		if (strcasecmp(str, quota_type[i]) == 0)
			break;
	}
	if (i >= MAXQUOTAS) {
		i = strtol(str, &t, 0);
		if (*t)
			i = -1;
	}
	if (i < 0 || i >= MAXQUOTAS) {
		com_err(0, 0, "Invalid quota type: %s", str);
		printf("Valid quota types are: ");
		for (i = 0; i < MAXQUOTAS; i++)
			printf("%s ", quota_type[i]);
		printf("\n");
		return -1;
	}

	if (current_fs->flags & EXT2_FLAG_RW)
		flags |= EXT2_FILE_WRITE;

	retval = quota_file_open(current_qctx, NULL, 0, i, -1, flags);
	if (retval) {
		com_err(cmdname, retval,
			"while opening quota inode (type %d)", i);
		return -1;
	}
	return i;
}


static int list_quota_callback(struct dquot *dq, void *cb_data)
{
	printf("%8u   %8lld %8lld %8lld    %8lld %8lld %8lld\n",
	       dq->dq_id, (long long)dq->dq_dqb.dqb_curspace,
	       (long long)dq->dq_dqb.dqb_bsoftlimit,
	       (long long)dq->dq_dqb.dqb_bhardlimit,
	       (long long)dq->dq_dqb.dqb_curinodes,
	       (long long)dq->dq_dqb.dqb_isoftlimit,
	       (long long)dq->dq_dqb.dqb_ihardlimit);
	return 0;
}

void do_list_quota(int argc, char *argv[])
{
	errcode_t	retval;
	int		type;
	struct quota_handle *qh;

	if (load_quota_ctx(argv[0]))
		return;

	if (argc != 2) {
		com_err(0, 0, "Usage: list_quota <quota_type>\n");
		return;
	}

	type = parse_quota_type(argv[0], argv[1]);
	if (type < 0)
		return;

	printf("%8s   %8s %8s %8s    %8s %8s %8s\n",
	       (type == 0) ? "user id" : "group id",
	       "blocks", "quota", "limit", "inodes", "quota", "limit");
	qh = current_qctx->quota_file[type];
	retval = qh->qh_ops->scan_dquots(qh, list_quota_callback, NULL);
	if (retval) {
		com_err(argv[0], retval, "while scanning dquots");
		return;
	}
}

void do_get_quota(int argc, char *argv[])
{
	int		err, type;
	struct quota_handle *qh;
	struct dquot	*dq;
	qid_t		id;

	if (load_quota_ctx(argv[0]))
		return;

	if (argc != 3) {
		com_err(0, 0, "Usage: get_quota <quota_type> <id>\n");
		return;
	}

	type = parse_quota_type(argv[0], argv[1]);
	if (type < 0)
		return;

	id = parse_ulong(argv[2], argv[0], "id", &err);
	if (err)
		return;

	printf("%8s   %8s %8s %8s    %8s %8s %8s\n",
	       (type == 0) ? "user id" : "group id",
	       "blocks", "quota", "limit", "inodes", "quota", "limit");

	qh = current_qctx->quota_file[type];

	dq = qh->qh_ops->read_dquot(qh, id);
	if (dq) {
		list_quota_callback(dq, NULL);
		ext2fs_free_mem(&dq);
	} else {
		com_err(argv[0], 0, "couldn't read quota record");
	}
}
