/*
 * Copyright (c) 1991, 1992 Paul Kranenburg <pk@cs.few.eur.nl>
 * Copyright (c) 1993 Branko Lankester <branko@hacktic.nl>
 * Copyright (c) 1993, 1994, 1995, 1996 Rick Sladkey <jrs@world.std.com>
 * Copyright (c) 1996-1999 Wichert Akkerman <wichert@cistron.nl>
 * Copyright (c) 2005, 2006 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id$
 */

#include "defs.h"

#ifdef LINUX

#include <inttypes.h>

#define SUBCMDMASK  0x00ff
#define SUBCMDSHIFT 8
#define QCMD_CMD(cmd)	((u_int32_t)(cmd) >> SUBCMDSHIFT)
#define QCMD_TYPE(cmd)	((u_int32_t)(cmd) & SUBCMDMASK)

#define OLD_CMD(cmd)	((u_int32_t)(cmd) << 8)
#define NEW_CMD(cmd)	((u_int32_t)(cmd) | 0x800000)
#define XQM_CMD(cmd)	((u_int32_t)(cmd) | ('X'<<8))

#define Q_V1_QUOTAON	OLD_CMD(0x1)
#define Q_V1_QUOTAOFF	OLD_CMD(0x2)
#define Q_V1_GETQUOTA	OLD_CMD(0x3)
#define Q_V1_SETQUOTA	OLD_CMD(0x4)
#define Q_V1_SETUSE	OLD_CMD(0x5)
#define Q_V1_SYNC	OLD_CMD(0x6)
#define Q_SETQLIM	OLD_CMD(0x7)
#define Q_V1_GETSTATS	OLD_CMD(0x8)
#define Q_V1_RSQUASH	OLD_CMD(0x10)

#define Q_V2_GETQUOTA	OLD_CMD(0xD)
#define Q_V2_SETQUOTA	OLD_CMD(0xE)
#define Q_V2_SETUSE	OLD_CMD(0xF)
#define Q_V2_GETINFO	OLD_CMD(0x9)
#define Q_V2_SETINFO	OLD_CMD(0xA)
#define Q_V2_SETGRACE	OLD_CMD(0xB)
#define Q_V2_SETFLAGS	OLD_CMD(0xC)
#define Q_V2_GETSTATS	OLD_CMD(0x11)

#define Q_SYNC		NEW_CMD(0x1)
#define Q_QUOTAON	NEW_CMD(0x2)
#define Q_QUOTAOFF	NEW_CMD(0x3)
#define Q_GETFMT	NEW_CMD(0x4)
#define Q_GETINFO	NEW_CMD(0x5)
#define Q_SETINFO	NEW_CMD(0x6)
#define Q_GETQUOTA	NEW_CMD(0x7)
#define Q_SETQUOTA	NEW_CMD(0x8)

#define Q_XQUOTAON	XQM_CMD(0x1)
#define Q_XQUOTAOFF	XQM_CMD(0x2)
#define Q_XGETQUOTA	XQM_CMD(0x3)
#define Q_XSETQLIM	XQM_CMD(0x4)
#define Q_XGETQSTAT	XQM_CMD(0x5)
#define Q_XQUOTARM	XQM_CMD(0x6)
#define Q_XQUOTASYNC	XQM_CMD(0x7)

static const struct xlat quotacmds[] = {
	{Q_V1_QUOTAON, "Q_V1_QUOTAON"},
	{Q_V1_QUOTAOFF, "Q_V1_QUOTAOFF"},
	{Q_V1_GETQUOTA, "Q_V1_GETQUOTA"},
	{Q_V1_SETQUOTA, "Q_V1_SETQUOTA"},
	{Q_V1_SETUSE, "Q_V1_SETUSE"},
	{Q_V1_SYNC, "Q_V1_SYNC"},
	{Q_SETQLIM, "Q_SETQLIM"},
	{Q_V1_GETSTATS, "Q_V1_GETSTATS"},
	{Q_V1_RSQUASH, "Q_V1_RSQUASH"},

	{Q_V2_GETQUOTA, "Q_V2_GETQUOTA"},
	{Q_V2_SETQUOTA, "Q_V2_SETQUOTA"},
	{Q_V2_SETUSE, "Q_V2_SETUSE"},
	{Q_V2_GETINFO, "Q_V2_GETINFO"},
	{Q_V2_SETINFO, "Q_V2_SETINFO"},
	{Q_V2_SETGRACE, "Q_V2_SETGRACE"},
	{Q_V2_SETFLAGS, "Q_V2_SETFLAGS"},
	{Q_V2_GETSTATS, "Q_V2_GETSTATS"},

	{Q_SYNC, "Q_SYNC"},
	{Q_QUOTAON, "Q_QUOTAON"},
	{Q_QUOTAOFF, "Q_QUOTAOFF"},
	{Q_GETFMT, "Q_GETFMT"},
	{Q_GETINFO, "Q_GETINFO"},
	{Q_SETINFO, "Q_SETINFO"},
	{Q_GETQUOTA, "Q_GETQUOTA"},
	{Q_SETQUOTA, "Q_SETQUOTA"},

	{Q_XQUOTAON, "Q_XQUOTAON"},
	{Q_XQUOTAOFF, "Q_XQUOTAOFF"},
	{Q_XGETQUOTA, "Q_XGETQUOTA"},
	{Q_XSETQLIM, "Q_XSETQLIM"},
	{Q_XGETQSTAT, "Q_XGETQSTAT"},
	{Q_XQUOTARM, "Q_XQUOTARM"},
	{Q_XQUOTASYNC, "Q_XQUOTASYNC"},

	{0, NULL},
};

#define USRQUOTA 0
#define GRPQUOTA 1

static const struct xlat quotatypes[] = {
	{USRQUOTA, "USRQUOTA"},
	{GRPQUOTA, "GRPQUOTA"},
	{0, NULL},
};

/* Quota format identifiers */
#define QFMT_VFS_OLD 1
#define QFMT_VFS_V0  2

static const struct xlat quota_formats[] = {
	{QFMT_VFS_OLD, "QFMT_VFS_OLD"},
	{QFMT_VFS_V0, "QFMT_VFS_V0"},
	{0, NULL},
};

#define XFS_QUOTA_UDQ_ACCT	(1<<0)	/* user quota accounting */
#define XFS_QUOTA_UDQ_ENFD	(1<<1)	/* user quota limits enforcement */
#define XFS_QUOTA_GDQ_ACCT	(1<<2)	/* group quota accounting */
#define XFS_QUOTA_GDQ_ENFD	(1<<3)	/* group quota limits enforcement */

#define XFS_USER_QUOTA		(1<<0)	/* user quota type */
#define XFS_PROJ_QUOTA		(1<<1)	/* (IRIX) project quota type */
#define XFS_GROUP_QUOTA		(1<<2)	/* group quota type */

static const struct xlat xfs_quota_flags[] = {
	{XFS_QUOTA_UDQ_ACCT, "XFS_QUOTA_UDQ_ACCT"},
	{XFS_QUOTA_UDQ_ENFD, "XFS_QUOTA_UDQ_ENFD"},
	{XFS_QUOTA_GDQ_ACCT, "XFS_QUOTA_GDQ_ACCT"},
	{XFS_QUOTA_GDQ_ENFD, "XFS_QUOTA_GDQ_ENFD"},
	{0, NULL}
};

static const struct xlat xfs_dqblk_flags[] = {
	{XFS_USER_QUOTA, "XFS_USER_QUOTA"},
	{XFS_PROJ_QUOTA, "XFS_PROJ_QUOTA"},
	{XFS_GROUP_QUOTA, "XFS_GROUP_QUOTA"},
	{0, NULL}
};

/*
 * Following flags are used to specify which fields are valid
 */
#define QIF_BLIMITS	1
#define QIF_SPACE	2
#define QIF_ILIMITS	4
#define QIF_INODES	8
#define QIF_BTIME	16
#define QIF_ITIME	32

static const struct xlat if_dqblk_valid[] = {
	{QIF_BLIMITS, "QIF_BLIMITS"},
	{QIF_SPACE, "QIF_SPACE"},
	{QIF_ILIMITS, "QIF_ILIMITS"},
	{QIF_INODES, "QIF_INODES"},
	{QIF_BTIME, "QIF_BTIME"},
	{QIF_ITIME, "QIF_ITIME"},
	{0, NULL}
};

struct if_dqblk
{
	u_int64_t dqb_bhardlimit;
	u_int64_t dqb_bsoftlimit;
	u_int64_t dqb_curspace;
	u_int64_t dqb_ihardlimit;
	u_int64_t dqb_isoftlimit;
	u_int64_t dqb_curinodes;
	u_int64_t dqb_btime;
	u_int64_t dqb_itime;
	u_int32_t dqb_valid;
};

struct v1_dqblk
{
	u_int32_t dqb_bhardlimit;	/* absolute limit on disk blks alloc */
	u_int32_t dqb_bsoftlimit;	/* preferred limit on disk blks */
	u_int32_t dqb_curblocks;	/* current block count */
	u_int32_t dqb_ihardlimit;	/* maximum # allocated inodes */
	u_int32_t dqb_isoftlimit;	/* preferred inode limit */
	u_int32_t dqb_curinodes;	/* current # allocated inodes */
	time_t  dqb_btime;	/* time limit for excessive disk use */
	time_t  dqb_itime;	/* time limit for excessive files */
};

struct v2_dqblk
{
	unsigned int dqb_ihardlimit;
	unsigned int dqb_isoftlimit;
	unsigned int dqb_curinodes;
	unsigned int dqb_bhardlimit;
	unsigned int dqb_bsoftlimit;
	u_int64_t dqb_curspace;
	time_t  dqb_btime;
	time_t  dqb_itime;
};

struct xfs_dqblk
{
	int8_t  d_version;	/* version of this structure */
	int8_t  d_flags;	/* XFS_{USER,PROJ,GROUP}_QUOTA */
	u_int16_t d_fieldmask;	/* field specifier */
	u_int32_t d_id;		/* user, project, or group ID */
	u_int64_t d_blk_hardlimit;	/* absolute limit on disk blks */
	u_int64_t d_blk_softlimit;	/* preferred limit on disk blks */
	u_int64_t d_ino_hardlimit;	/* maximum # allocated inodes */
	u_int64_t d_ino_softlimit;	/* preferred inode limit */
	u_int64_t d_bcount;	/* # disk blocks owned by the user */
	u_int64_t d_icount;	/* # inodes owned by the user */
	int32_t d_itimer;	/* zero if within inode limits */
	int32_t d_btimer;	/* similar to above; for disk blocks */
	u_int16_t d_iwarns;	/* # warnings issued wrt num inodes */
	u_int16_t d_bwarns;	/* # warnings issued wrt disk blocks */
	int32_t d_padding2;	/* padding2 - for future use */
	u_int64_t d_rtb_hardlimit;	/* absolute limit on realtime blks */
	u_int64_t d_rtb_softlimit;	/* preferred limit on RT disk blks */
	u_int64_t d_rtbcount;	/* # realtime blocks owned */
	int32_t d_rtbtimer;	/* similar to above; for RT disk blks */
	u_int16_t d_rtbwarns;	/* # warnings issued wrt RT disk blks */
	int16_t d_padding3;	/* padding3 - for future use */
	char    d_padding4[8];	/* yet more padding */
};

/*
 * Following flags are used to specify which fields are valid
 */
#define IIF_BGRACE	1
#define IIF_IGRACE	2
#define IIF_FLAGS	4

static const struct xlat if_dqinfo_valid[] = {
	{IIF_BGRACE, "IIF_BGRACE"},
	{IIF_IGRACE, "IIF_IGRACE"},
	{IIF_FLAGS, "IIF_FLAGS"},
	{0, NULL}
};

struct if_dqinfo
{
	u_int64_t dqi_bgrace;
	u_int64_t dqi_igrace;
	u_int32_t dqi_flags;
	u_int32_t dqi_valid;
};

struct v2_dqinfo
{
	unsigned int dqi_bgrace;
	unsigned int dqi_igrace;
	unsigned int dqi_flags;
	unsigned int dqi_blocks;
	unsigned int dqi_free_blk;
	unsigned int dqi_free_entry;
};

struct v1_dqstats
{
	u_int32_t lookups;
	u_int32_t drops;
	u_int32_t reads;
	u_int32_t writes;
	u_int32_t cache_hits;
	u_int32_t allocated_dquots;
	u_int32_t free_dquots;
	u_int32_t syncs;
};

struct v2_dqstats
{
	u_int32_t lookups;
	u_int32_t drops;
	u_int32_t reads;
	u_int32_t writes;
	u_int32_t cache_hits;
	u_int32_t allocated_dquots;
	u_int32_t free_dquots;
	u_int32_t syncs;
	u_int32_t version;
};

typedef struct fs_qfilestat
{
	u_int64_t qfs_ino;	/* inode number */
	u_int64_t qfs_nblks;	/* number of BBs 512-byte-blks */
	u_int32_t qfs_nextents;	/* number of extents */
} fs_qfilestat_t;

struct xfs_dqstats
{
	int8_t  qs_version;	/* version number for future changes */
	u_int16_t qs_flags;	/* XFS_QUOTA_{U,P,G}DQ_{ACCT,ENFD} */
	int8_t  qs_pad;		/* unused */
	fs_qfilestat_t qs_uquota;	/* user quota storage information */
	fs_qfilestat_t qs_gquota;	/* group quota storage information */
	u_int32_t qs_incoredqs;	/* number of dquots incore */
	int32_t qs_btimelimit;	/* limit for blks timer */
	int32_t qs_itimelimit;	/* limit for inodes timer */
	int32_t qs_rtbtimelimit;	/* limit for rt blks timer */
	u_int16_t qs_bwarnlimit;	/* limit for num warnings */
	u_int16_t qs_iwarnlimit;	/* limit for num warnings */
};

static void
decode_cmd_data(struct tcb *tcp, u_int32_t cmd, unsigned long data)
{
	switch (cmd)
	{
		case Q_GETQUOTA:
		case Q_SETQUOTA:
		{
			struct if_dqblk dq;

			if (cmd == Q_GETQUOTA && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{bhardlimit=%" PRIu64 ", ", dq.dqb_bhardlimit);
			tprintf("bsoftlimit=%" PRIu64 ", ", dq.dqb_bsoftlimit);
			tprintf("curspace=%" PRIu64 ", ", dq.dqb_curspace);
			tprintf("ihardlimit=%" PRIu64 ", ", dq.dqb_ihardlimit);
			tprintf("isoftlimit=%" PRIu64 ", ", dq.dqb_isoftlimit);
			tprintf("curinodes=%" PRIu64 ", ", dq.dqb_curinodes);
			if (!abbrev(tcp))
			{
				tprintf("btime=%" PRIu64 ", ", dq.dqb_btime);
				tprintf("itime=%" PRIu64 ", ", dq.dqb_itime);
				tprintf("valid=");
				printflags(if_dqblk_valid,
					   dq.dqb_valid, "QIF_???");
				tprintf("}");
			} else
				tprintf("...}");
			break;
		}
		case Q_V1_GETQUOTA:
		case Q_V1_SETQUOTA:
		{
			struct v1_dqblk dq;

			if (cmd == Q_V1_GETQUOTA && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{bhardlimit=%u, ", dq.dqb_bhardlimit);
			tprintf("bsoftlimit=%u, ", dq.dqb_bsoftlimit);
			tprintf("curblocks=%u, ", dq.dqb_curblocks);
			tprintf("ihardlimit=%u, ", dq.dqb_ihardlimit);
			tprintf("isoftlimit=%u, ", dq.dqb_isoftlimit);
			tprintf("curinodes=%u, ", dq.dqb_curinodes);
			tprintf("btime=%lu, ", dq.dqb_btime);
			tprintf("itime=%lu}", dq.dqb_itime);
			break;
		}
		case Q_V2_GETQUOTA:
		case Q_V2_SETQUOTA:
		{
			struct v2_dqblk dq;

			if (cmd == Q_V2_GETQUOTA && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{ihardlimit=%u, ", dq.dqb_ihardlimit);
			tprintf("isoftlimit=%u, ", dq.dqb_isoftlimit);
			tprintf("curinodes=%u, ", dq.dqb_curinodes);
			tprintf("bhardlimit=%u, ", dq.dqb_bhardlimit);
			tprintf("bsoftlimit=%u, ", dq.dqb_bsoftlimit);
			tprintf("curspace=%" PRIu64 ", ", dq.dqb_curspace);
			tprintf("btime=%lu, ", dq.dqb_btime);
			tprintf("itime=%lu}", dq.dqb_itime);
			break;
		}
		case Q_XGETQUOTA:
		case Q_XSETQLIM:
		{
			struct xfs_dqblk dq;

			if (cmd == Q_XGETQUOTA && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{version=%d, ", dq.d_version);
			tprintf("flags=");
			printflags(xfs_dqblk_flags,
				   dq.d_flags, "XFS_???_QUOTA");
			tprintf(", fieldmask=%#x, ", dq.d_fieldmask);
			tprintf("id=%u, ", dq.d_id);
			tprintf("blk_hardlimit=%" PRIu64 ", ", dq.d_blk_hardlimit);
			tprintf("blk_softlimit=%" PRIu64 ", ", dq.d_blk_softlimit);
			tprintf("ino_hardlimit=%" PRIu64 ", ", dq.d_ino_hardlimit);
			tprintf("ino_softlimit=%" PRIu64 ", ", dq.d_ino_softlimit);
			tprintf("bcount=%" PRIu64 ", ", dq.d_bcount);
			tprintf("icount=%" PRIu64 ", ", dq.d_icount);
			if (!abbrev(tcp))
			{
				tprintf("itimer=%d, ", dq.d_itimer);
				tprintf("btimer=%d, ", dq.d_btimer);
				tprintf("iwarns=%u, ", dq.d_iwarns);
				tprintf("bwarns=%u, ", dq.d_bwarns);
				tprintf("rtbcount=%" PRIu64 ", ", dq.d_rtbcount);
				tprintf("rtbtimer=%d, ", dq.d_rtbtimer);
				tprintf("rtbwarns=%u}", dq.d_rtbwarns);
			} else
				tprintf("...}");
			break;
		}
		case Q_GETFMT:
		{
			u_int32_t fmt;

			if (syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &fmt) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{");
			printxval(quota_formats, fmt, "QFMT_VFS_???");
			tprintf("}");
			break;
		}
		case Q_GETINFO:
		case Q_SETINFO:
		{
			struct if_dqinfo dq;

			if (cmd == Q_GETINFO && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{bgrace=%" PRIu64 ", ", dq.dqi_bgrace);
			tprintf("igrace=%" PRIu64 ", ", dq.dqi_igrace);
			tprintf("flags=%#x, ", dq.dqi_flags);
			tprintf("valid=");
			printflags(if_dqinfo_valid, dq.dqi_valid, "IIF_???");
			tprintf("}");
			break;
		}
		case Q_V2_GETINFO:
		case Q_V2_SETINFO:
		{
			struct v2_dqinfo dq;

			if (cmd == Q_V2_GETINFO && syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{bgrace=%u, ", dq.dqi_bgrace);
			tprintf("igrace=%u, ", dq.dqi_igrace);
			tprintf("flags=%#x, ", dq.dqi_flags);
			tprintf("blocks=%u, ", dq.dqi_blocks);
			tprintf("free_blk=%u, ", dq.dqi_free_blk);
			tprintf("free_entry=%u}", dq.dqi_free_entry);
			break;
		}
		case Q_V1_GETSTATS:
		{
			struct v1_dqstats dq;

			if (syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{lookups=%u, ", dq.lookups);
			tprintf("drops=%u, ", dq.drops);
			tprintf("reads=%u, ", dq.reads);
			tprintf("writes=%u, ", dq.writes);
			tprintf("cache_hits=%u, ", dq.cache_hits);
			tprintf("allocated_dquots=%u, ", dq.allocated_dquots);
			tprintf("free_dquots=%u, ", dq.free_dquots);
			tprintf("syncs=%u}", dq.syncs);
			break;
		}
		case Q_V2_GETSTATS:
		{
			struct v2_dqstats dq;

			if (syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{lookups=%u, ", dq.lookups);
			tprintf("drops=%u, ", dq.drops);
			tprintf("reads=%u, ", dq.reads);
			tprintf("writes=%u, ", dq.writes);
			tprintf("cache_hits=%u, ", dq.cache_hits);
			tprintf("allocated_dquots=%u, ", dq.allocated_dquots);
			tprintf("free_dquots=%u, ", dq.free_dquots);
			tprintf("syncs=%u, ", dq.syncs);
			tprintf("version=%u}", dq.version);
			break;
		}
		case Q_XGETQSTAT:
		{
			struct xfs_dqstats dq;

			if (syserror(tcp))
			{
				tprintf("%#lx", data);
				break;
			}
			if (umove(tcp, data, &dq) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{version=%d, ", dq.qs_version);
			if (abbrev(tcp))
			{
				tprintf("...}");
				break;
			}
			tprintf("flags=");
			printflags(xfs_quota_flags,
				   dq.qs_flags, "XFS_QUOTA_???");
			tprintf(", incoredqs=%u, ", dq.qs_incoredqs);
			tprintf("u_ino=%" PRIu64 ", ", dq.qs_uquota.qfs_ino);
			tprintf("u_nblks=%" PRIu64 ", ", dq.qs_uquota.qfs_nblks);
			tprintf("u_nextents=%u, ", dq.qs_uquota.qfs_nextents);
			tprintf("g_ino=%" PRIu64 ", ", dq.qs_gquota.qfs_ino);
			tprintf("g_nblks=%" PRIu64 ", ", dq.qs_gquota.qfs_nblks);
			tprintf("g_nextents=%u, ", dq.qs_gquota.qfs_nextents);
			tprintf("btimelimit=%d, ", dq.qs_btimelimit);
			tprintf("itimelimit=%d, ", dq.qs_itimelimit);
			tprintf("rtbtimelimit=%d, ", dq.qs_rtbtimelimit);
			tprintf("bwarnlimit=%u, ", dq.qs_bwarnlimit);
			tprintf("iwarnlimit=%u}", dq.qs_iwarnlimit);
			break;
		}
		case Q_XQUOTAON:
		{
			u_int32_t flag;

			if (umove(tcp, data, &flag) < 0)
			{
				tprintf("{???} %#lx", data);
				break;
			}
			tprintf("{");
			printflags(xfs_quota_flags, flag, "XFS_QUOTA_???");
			tprintf("}");
			break;
		}
		default:
			tprintf("%#lx", data);
			break;
	}
}

int
sys_quotactl(struct tcb *tcp)
{
	/*
	 * The Linux kernel only looks at the low 32 bits of command and id
	 * arguments, but on some 64-bit architectures (s390x) this word
	 * will have been sign-extended when we see it.  The high 1 bits
	 * don't mean anything, so don't confuse the output with them.
	 */
	u_int32_t qcmd = tcp->u_arg[0];
	u_int32_t cmd = QCMD_CMD(qcmd);
	u_int32_t type = QCMD_TYPE(qcmd);
	u_int32_t id = tcp->u_arg[2];

	if (!verbose(tcp))
		return printargs(tcp);

	if (entering(tcp))
	{
		printxval(quotacmds, cmd, "Q_???");
		tprintf("|");
		printxval(quotatypes, type, "???QUOTA");
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
		tprintf(", ");
		switch (cmd)
		{
			case Q_V1_QUOTAON:
			case Q_QUOTAON:
				printxval(quota_formats, id, "QFMT_VFS_???");
				break;
			case Q_V1_GETQUOTA:
			case Q_V2_GETQUOTA:
			case Q_GETQUOTA:
			case Q_V1_SETQUOTA:
			case Q_V2_SETQUOTA:
			case Q_V1_SETUSE:
			case Q_V2_SETUSE:
			case Q_SETQLIM:
			case Q_SETQUOTA:
			case Q_XGETQUOTA:
			case Q_XSETQLIM:
				tprintf("%u", id);
				break;
			default:
				tprintf("%#lx", tcp->u_arg[2]);
				break;
		}
		tprintf(", ");
	} else
	{
		if (!tcp->u_arg[3])
			tprintf("NULL");
		else
			decode_cmd_data(tcp, cmd, tcp->u_arg[3]);
	}
	return 0;
}

#endif /* Linux */

#if defined(SUNOS4) || defined(FREEBSD)

#ifdef SUNOS4
#include <ufs/quota.h>
#endif

#ifdef FREEBSD
#include <ufs/ufs/quota.h>
#endif

static const struct xlat quotacmds[] = {
	{Q_QUOTAON, "Q_QUOTAON"},
	{Q_QUOTAOFF, "Q_QUOTAOFF"},
	{Q_GETQUOTA, "Q_GETQUOTA"},
	{Q_SETQUOTA, "Q_SETQUOTA"},
#ifdef Q_SETQLIM
	{Q_SETQLIM, "Q_SETQLIM"},
#endif
#ifdef Q_SETUSE
	{Q_SETUSE, "Q_SETUSE"},
#endif
	{Q_SYNC, "Q_SYNC"},
	{0, NULL},
};

int
sys_quotactl(struct tcb *tcp)
{
	/* fourth arg (addr) not interpreted here */
	if (entering(tcp))
	{
#ifdef SUNOS4
		printxval(quotacmds, tcp->u_arg[0], "Q_???");
		tprintf(", ");
		printstr(tcp, tcp->u_arg[1], -1);
#endif
#ifdef FREEBSD
		printpath(tcp, tcp->u_arg[0]);
		tprintf(", ");
		printxval(quotacmds, tcp->u_arg[1], "Q_???");
#endif
		tprintf(", %lu, %#lx", tcp->u_arg[2], tcp->u_arg[3]);
	}
	return 0;
}

#endif /* SUNOS4 || FREEBSD */
