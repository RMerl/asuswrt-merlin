/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/attr.h>
#include <sys/quota.h>
#include <sys/vnode.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "quotacheck.h"


/*
 * Query is based on uid
 */
#define QQ_COMMON	(ATTR_CMN_OWNERID)

struct quota_query {
	u_long		qq_len;
	uid_t		qq_uid;
};

/*
 * Desired atttributes for quota checking
 */
#define QA_COMMON	(ATTR_CMN_OBJTYPE | ATTR_CMN_OWNERID | ATTR_CMN_GRPID)
#define QA_FILE		(ATTR_FILE_ALLOCSIZE)
#define QA_DIR		(0)

struct quota_attr {
	unsigned long	qa_attrlen;
	fsobj_type_t	qa_type;
	uid_t		qa_uid;
	gid_t		qa_gid;
	off_t		qa_bytes;
};

#define ITEMS_PER_SEARCH  2500

void collectdata(const char*, struct quotaname *);


/*
 * Scan an HFS filesystem to check quota(s) present on it.
 */
int
chkquota_hfs(fsname, mntpt, qnp)
	char *fsname, *mntpt;
	register struct quotaname *qnp;
{
	int errs = 0;
	
	sync();

	collectdata(mntpt, qnp);

	if (qnp->flags & HASUSR)
		errs = update(mntpt, qnp->usrqfname, USRQUOTA);
	if (qnp->flags & HASGRP)
		errs = update(mntpt, qnp->grpqfname, GRPQUOTA);

	return (errs);
}


void
collectdata(const char* path, struct quotaname *qnp)
{
	struct fssearchblock searchblk = {0};
	struct attrlist	attrlist = {0};
	struct searchstate state;
	struct quota_attr *qap;
	struct quota_query query;
	u_long nummatches;
	u_long options;
	int i;
	int result;
	int vntype;
	off_t filebytes;
	register struct fileusage *fup;

	qap = malloc(ITEMS_PER_SEARCH * sizeof(struct quota_attr));
	if (qap == NULL)
		err(1, "%s", strerror(errno));

	options = SRCHFS_START |SRCHFS_MATCHDIRS | SRCHFS_MATCHFILES;

	/* Search for items with uid != 0 */
	query.qq_len = sizeof(struct quota_query);
	query.qq_uid = 0;
	options |= SRCHFS_NEGATEPARAMS | SRCHFS_SKIPLINKS;
	searchblk.searchattrs.bitmapcount = ATTR_BIT_MAP_COUNT;
	searchblk.searchattrs.commonattr = QQ_COMMON;
	searchblk.searchparams1 = &query;
	searchblk.searchparams2 = &query;
	searchblk.sizeofsearchparams1 = sizeof(query);
	searchblk.sizeofsearchparams2 = sizeof(query);

	/* Ask for type, uid, gid and file bytes */
	searchblk.returnattrs = &attrlist;
	attrlist.bitmapcount  = ATTR_BIT_MAP_COUNT;
	attrlist.commonattr   = QA_COMMON;
	attrlist.dirattr      = QA_DIR;
	attrlist.fileattr     = QA_FILE;
	
	/* Collect 2,500 items at a time (~60K) */
 	searchblk.returnbuffer = qap;									
	searchblk.returnbuffersize = ITEMS_PER_SEARCH * sizeof(struct quota_attr);
	searchblk.maxmatches = ITEMS_PER_SEARCH;

	for (;;) {
		nummatches = 0;
		result = searchfs(path, &searchblk, &nummatches, 0, options, &state);
		if (result && errno != EAGAIN && errno != EBUSY) {
			fprintf(stderr, "%d \n", errno);
			err(1, "searchfs %s", strerror(errno));
		}
		if (result == 0 && nummatches == 0)
			break;  /* all done */
		options &= ~SRCHFS_START;

		for (i = 0; i < nummatches; ++i) {
			vntype = qap[i].qa_type;
			filebytes = (vntype == VDIR) ? 0 : qap[i].qa_bytes;
	
			if (qnp->flags & HASGRP) {
				fup = addid(qap[i].qa_gid, GRPQUOTA);
				fup->fu_curinodes++;
				if (vntype == VREG || vntype == VDIR || vntype == VLNK)
					fup->fu_curbytes += filebytes;
			}
			if (qnp->flags & HASUSR) {
				fup = addid(qap[i].qa_uid, USRQUOTA);
				fup->fu_curinodes++;
				if (vntype == VREG || vntype == VDIR || vntype == VLNK)
					fup->fu_curbytes += filebytes;
			}
		}
	}

	free(qap);
}
