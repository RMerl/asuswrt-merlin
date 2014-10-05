/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Version: $Id: quota.h,v 2.0 1996/11/17 16:48:14 mvw Exp mvw $
 */

#ifndef _LINUX_QUOTA_
#define _LINUX_QUOTA_

#include <linux/errno.h>
#include <linux/types.h>

#define __DQUOT_VERSION__	"dquot_6.5.1"
#define __DQUOT_NUM_VERSION__	6*10000+5*100+1

typedef __kernel_uid32_t qid_t; /* Type in which we store ids in memory */
typedef __u64 qsize_t;          /* Type in which we store sizes */

/* Size of blocks in which are counted size limits */
#define QUOTABLOCK_BITS 10
#define QUOTABLOCK_SIZE (1 << QUOTABLOCK_BITS)

/* Conversion routines from and to quota blocks */
#define qb2kb(x) ((x) << (QUOTABLOCK_BITS-10))
#define kb2qb(x) ((x) >> (QUOTABLOCK_BITS-10))
#define toqb(x) (((x) + QUOTABLOCK_SIZE - 1) >> QUOTABLOCK_BITS)

#define MAXQUOTAS 2
#define USRQUOTA  0		/* element used for user quotas */
#define GRPQUOTA  1		/* element used for group quotas */

/*
 * Definitions for the default names of the quotas files.
 */
#define INITQFNAMES { \
	"user",    /* USRQUOTA */ \
	"group",   /* GRPQUOTA */ \
	"undefined", \
};

/*
 * Command definitions for the 'quotactl' system call.
 * The commands are broken into a main command defined below
 * and a subcommand that is used to convey the type of
 * quota that is being manipulated (see above).
 */
#define SUBCMDMASK  0x00ff
#define SUBCMDSHIFT 8
#define QCMD(cmd, type)  (((cmd) << SUBCMDSHIFT) | ((type) & SUBCMDMASK))

#define Q_SYNC     0x800001	/* sync disk copy of a filesystems quotas */
#define Q_QUOTAON  0x800002	/* turn quotas on */
#define Q_QUOTAOFF 0x800003	/* turn quotas off */
#define Q_GETFMT   0x800004	/* get quota format used on given filesystem */
#define Q_GETINFO  0x800005	/* get information about quota files */
#define Q_SETINFO  0x800006	/* set information about quota files */
#define Q_GETQUOTA 0x800007	/* get user quota structure */
#define Q_SETQUOTA 0x800008	/* set user quota structure */

/*
 * Quota structure used for communication with userspace via quotactl
 * Following flags are used to specify which fields are valid
 */
#define QIF_BLIMITS	1
#define QIF_SPACE	2
#define QIF_ILIMITS	4
#define QIF_INODES	8
#define QIF_BTIME	16
#define QIF_ITIME	32
#define QIF_LIMITS	(QIF_BLIMITS | QIF_ILIMITS)
#define QIF_USAGE	(QIF_SPACE | QIF_INODES)
#define QIF_TIMES	(QIF_BTIME | QIF_ITIME)
#define QIF_ALL		(QIF_LIMITS | QIF_USAGE | QIF_TIMES)

struct if_dqblk {
	__u64 dqb_bhardlimit;
	__u64 dqb_bsoftlimit;
	__u64 dqb_curspace;
	__u64 dqb_ihardlimit;
	__u64 dqb_isoftlimit;
	__u64 dqb_curinodes;
	__u64 dqb_btime;
	__u64 dqb_itime;
	__u32 dqb_valid;
};

/*
 * Structure used for setting quota information about file via quotactl
 * Following flags are used to specify which fields are valid
 */
#define IIF_BGRACE	1
#define IIF_IGRACE	2
#define IIF_FLAGS	4
#define IIF_ALL		(IIF_BGRACE | IIF_IGRACE | IIF_FLAGS)

struct if_dqinfo {
	__u64 dqi_bgrace;
	__u64 dqi_igrace;
	__u32 dqi_flags;
	__u32 dqi_valid;
};


# /* nodep */ include <sys/cdefs.h>

__BEGIN_DECLS
long quotactl __P ((unsigned int, const char *, int, caddr_t));
__END_DECLS

#endif /* _QUOTA_ */
