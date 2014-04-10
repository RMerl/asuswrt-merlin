/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */


#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifdef ISO
#include <netiso/iso.h>
#endif

#ifdef NFSKERB
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#endif

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "mntopts.h"

#define	ALTF_BG		0x1
#define ALTF_NOCONN	0x2
#define ALTF_DUMBTIMR	0x4
#define ALTF_INTR	0x8
#define ALTF_KERB	0x10
#define ALTF_NFSV3	0x20
#define ALTF_RDIRPLUS	0x40
#define	ALTF_MNTUDP	0x80
#define ALTF_RESVPORT	0x100
#define ALTF_SEQPACKET	0x200
#define ALTF_UDP	0x400
#define ALTF_SOFT	0x800
#define ALTF_TCP	0x1000
#define ALTF_NFSV2	0x2000
#define ALTF_NOLOCKS	0x4000
#define ALTF_ATTRCACHE	0x8000
#define ALTF_RSIZE	0x10000
#define ALTF_WSIZE	0x20000
#define ALTF_READAHEAD	0x40000
#define ALTF_NFSVERS	0x80000
#define ALTF_TIMEO	0x100000
#define ALTF_RETRANS	0x200000

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	{ "bg", 0, ALTF_BG, 1 },
	{ "conn", 1, ALTF_NOCONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
#ifdef NFSKERB
	{ "kerb", 0, ALTF_KERB, 1 },
#endif
	{ "nfsv3", 0, ALTF_NFSV3, 1 },
	{ "rdirplus", 0, ALTF_RDIRPLUS, 1 },
	{ "mntudp", 0, ALTF_MNTUDP, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
#ifdef ISO
	{ "seqpacket", 0, ALTF_SEQPACKET, 1 },
#endif
	{ "udp", 0, ALTF_UDP, 1 },
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ "nfsv2", 0, ALTF_NFSV2, 1 },
	{ "acregmin=", 0, ALTF_ATTRCACHE, 1 },
	{ "acregmax=", 0, ALTF_ATTRCACHE, 1 },
	{ "acdirmin=", 0, ALTF_ATTRCACHE, 1 },
	{ "acdirmax=", 0, ALTF_ATTRCACHE, 1 },
	{ "actimeo=", 0, ALTF_ATTRCACHE, 1 },
	{ "ac", 1, ALTF_ATTRCACHE, 1 },
	{ "locks", 1, ALTF_NOLOCKS, 1 },
	{ "lockd", 1, ALTF_NOLOCKS, 1 },
	{ "lock", 1, ALTF_NOLOCKS, 1 },
	{ "nlm", 1, ALTF_NOLOCKS, 1 },
	{ "rsize=", 0, ALTF_RSIZE, 1 },
	{ "wsize=", 0, ALTF_WSIZE, 1 },
	{ "rwsize=", 0, ALTF_RSIZE|ALTF_WSIZE, 1 },
	{ "readahead=", 0, ALTF_READAHEAD, 1 },
	{ "nfsvers=", 0, ALTF_NFSVERS, 1 },
	{ "vers=", 0, ALTF_NFSVERS, 1 },
	{ "timeo=", 0, ALTF_TIMEO, 1 },
	{ "retrans=", 0, ALTF_RETRANS, 1 },
	{ NULL }
};

struct nfs_args nfsdefargs = {
	NFS_ARGSVERSION,
	(struct sockaddr *)0,
	sizeof (struct sockaddr_in),
	SOCK_DGRAM,
	0,
	(u_char *)0,
	0,
	NFSMNT_NFSV3,
	NFS_WSIZE,
	NFS_RSIZE,
	NFS_READDIRSIZE,
	10,
	NFS_RETRANS,
	NFS_MAXGRPS,
	NFS_DEFRAHEAD,
	0,
	0,
	(char *)0,
	NFS_MINATTRTIMO,
	NFS_MAXATTRTIMO,
	NFS_MINDIRATTRTIMO,
	NFS_MAXDIRATTRTIMO,
};

struct nfhret {
	u_long		stat;
	long		vers;
	long		auth;
	long		fhsize;
	u_char		nfh[NFSX_V3FHMAX];
};
#define	DEF_RETRY	10000
#define	BGRND	1
#define	ISBGRND	2
int retrycnt = DEF_RETRY;
int opflags = 0;
int nfsproto = IPPROTO_UDP;
int mnttcp_ok = 1;
int force2 = 0;
int force3 = 0;

#ifdef NFSKERB
char inst[INST_SZ];
char realm[REALM_SZ];
struct {
	u_long		kind;
	KTEXT_ST	kt;
} ktick;
struct nfsrpc_nickverf kverf;
struct nfsrpc_fullblock kin, kout;
NFSKERBKEY_T kivec;
CREDENTIALS kcr;
struct timeval ktv;
NFSKERBKEYSCHED_T kerb_keysched;
#endif

uid_t real_uid, eff_uid;

int	getnfsargs __P((char *, struct nfs_args *));
#ifdef ISO
struct	iso_addr *iso_addr __P((const char *));
#endif
void	set_rpc_maxgrouplist __P((int));
__dead	void usage __P((void));
int	xdr_dir __P((XDR *, char *));
int	xdr_fh __P((XDR *, struct nfhret *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int c;
	register struct nfs_args *nfsargsp;
	struct nfs_args nfsargs;
	struct nfsd_cargs ncd;
	int mntflags, altflags, i, nfssvc_flag, num;
	char name[MAXPATHLEN], *p, *p2, *spec;
#ifdef NFSKERB
	uid_t last_ruid;

	last_ruid = -1;
	(void)strcpy(realm, KRB_REALM);
	if (sizeof (struct nfsrpc_nickverf) != RPCX_NICKVERF ||
	    sizeof (struct nfsrpc_fullblock) != RPCX_FULLBLOCK ||
	    ((char *)&ktick.kt) - ((char *)&ktick) != NFSX_UNSIGNED ||
	    ((char *)ktick.kt.dat) - ((char *)&ktick) != 2 * NFSX_UNSIGNED)
		fprintf(stderr, "Yikes! NFSKERB structs not packed!!\n");
#endif /* NFSKERB */

	/* drop setuid root privs asap */
	eff_uid = geteuid();
	real_uid = getuid();
	seteuid(real_uid); 

	retrycnt = DEF_RETRY;

	mntflags = 0;
	altflags = 0;
	nfsargs = nfsdefargs;
	nfsargsp = &nfsargs;
	while ((c = getopt(argc, argv,
	    "23a:bcdD:g:I:iKLlm:o:PpqR:r:sTt:Uw:x:")) != EOF)
		switch (c) {
		case '3':
			if (force2)
				errx(1, "-2 and -3 are mutually exclusive");
			force3 = 1;
			break;
		case '2':
			if (force3)
				errx(1, "-2 and -3 are mutually exclusive");
			force2 = 1;
			nfsargsp->flags &= ~NFSMNT_NFSV3;
			break;
		case 'a':
			num = strtol(optarg, &p, 10);
			if (*p || num < 0)
				errx(1, "illegal -a value -- %s", optarg);
			nfsargsp->readahead = num;
			nfsargsp->flags |= NFSMNT_READAHEAD;
			break;
		case 'b':
			opflags |= BGRND;
			break;
		case 'c':
			nfsargsp->flags |= NFSMNT_NOCONN;
			break;
		case 'D':
			warnx("NQNFS not supported; -D option deprecated");
			break;
		case 'd':
			nfsargsp->flags |= NFSMNT_DUMBTIMR;
			break;
		case 'g':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -g value -- %s", optarg);
			set_rpc_maxgrouplist(num);
			nfsargsp->maxgrouplist = num;
			nfsargsp->flags |= NFSMNT_MAXGRPS;
			break;
		case 'I':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -I value -- %s", optarg);
			nfsargsp->readdirsize = num;
			nfsargsp->flags |= NFSMNT_READDIRSIZE;
			break;
		case 'i':
			nfsargsp->flags |= NFSMNT_INT;
			break;
#ifdef NFSKERB
		case 'K':
			nfsargsp->flags |= NFSMNT_KERB;
			break;
#endif
		case 'L':
			nfsargsp->flags |= NFSMNT_NOLOCKS;
			break;
		case 'l':
			nfsargsp->flags |= NFSMNT_RDIRPLUS;
			break;
#ifdef NFSKERB
		case 'm':
			(void)strncpy(realm, optarg, REALM_SZ - 1);
			realm[REALM_SZ - 1] = '\0';
			break;
#endif
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &altflags);
			if(altflags & ALTF_BG)
				opflags |= BGRND;
			if(altflags & ALTF_NOCONN)
				nfsargsp->flags |= NFSMNT_NOCONN;
			if(altflags & ALTF_DUMBTIMR)
				nfsargsp->flags |= NFSMNT_DUMBTIMR;
			if(altflags & ALTF_INTR)
				nfsargsp->flags |= NFSMNT_INT;
#ifdef NFSKERB
			if(altflags & ALTF_KERB)
				nfsargsp->flags |= NFSMNT_KERB;
#endif
			if(altflags & ALTF_NFSV3) {
				if (force2)
					errx(1,"conflicting NFS version options");
				force3 = 1;
			}
			if(altflags & ALTF_NFSV2) {
				if (force3)
					errx(1,"conflicting NFS version options");
				force2 = 1;
				nfsargsp->flags &= ~NFSMNT_NFSV3;
			}
			if(altflags & ALTF_RDIRPLUS)
				nfsargsp->flags |= NFSMNT_RDIRPLUS;
			if(altflags & ALTF_MNTUDP)
				mnttcp_ok = 0;
			if(altflags & ALTF_RESVPORT)
				nfsargsp->flags |= NFSMNT_RESVPORT;
#ifdef ISO
			if(altflags & ALTF_SEQPACKET)
				nfsargsp->sotype = SOCK_SEQPACKET;
#endif
			if(altflags & ALTF_SOFT)
				nfsargsp->flags |= NFSMNT_SOFT;
			if(altflags & ALTF_UDP) {
				nfsargsp->sotype = SOCK_DGRAM;
				nfsproto = IPPROTO_UDP;
			}
			if(altflags & ALTF_TCP) {
				nfsargsp->sotype = SOCK_STREAM;
				nfsproto = IPPROTO_TCP;
			}
			if(altflags & ALTF_NOLOCKS)
				nfsargsp->flags |= NFSMNT_NOLOCKS;
			if(altflags & ALTF_ATTRCACHE) {
				if ((p = strcasestr(optarg, "actimeo="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal actimeo value -- %s", p2);
					nfsargsp->acregmin = num;
					nfsargsp->acregmax = nfsargsp->acregmin;
					nfsargsp->acdirmin = nfsargsp->acregmin;
					nfsargsp->acdirmax = nfsargsp->acregmin;
					nfsargsp->flags |= NFSMNT_ACREGMIN;
					nfsargsp->flags |= NFSMNT_ACREGMAX;
					nfsargsp->flags |= NFSMNT_ACDIRMIN;
					nfsargsp->flags |= NFSMNT_ACDIRMAX;
				}
				if ((p = strcasestr(optarg, "acregmin="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal acregmin value -- %s", p2);
					nfsargsp->acregmin = num;
					nfsargsp->flags |= NFSMNT_ACREGMIN;
				}
				if ((p = strcasestr(optarg, "acregmax="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal acregmax value -- %s", p2);
					nfsargsp->acregmax = num;
					nfsargsp->flags |= NFSMNT_ACREGMAX;
				}
				if ((p = strcasestr(optarg, "acdirmin="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal acdirmin value -- %s", p2);
					nfsargsp->acdirmin = num;
					nfsargsp->flags |= NFSMNT_ACDIRMIN;
				}
				if ((p = strcasestr(optarg, "acdirmax="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal acdirmax value -- %s", p2);
					nfsargsp->acdirmax = num;
					nfsargsp->flags |= NFSMNT_ACDIRMAX;
				}
				if ((p = strcasestr(optarg, "noac"))) {
					nfsargsp->acregmin = 0;
					nfsargsp->acregmax = 0;
					nfsargsp->acdirmin = 0;
					nfsargsp->acdirmax = 0;
					nfsargsp->flags |= NFSMNT_ACREGMIN;
					nfsargsp->flags |= NFSMNT_ACREGMAX;
					nfsargsp->flags |= NFSMNT_ACDIRMIN;
					nfsargsp->flags |= NFSMNT_ACDIRMAX;
				}
			}
			if(altflags & ALTF_RSIZE) {
				p = strcasestr(optarg, "rwsize=");
				if (!p)
					p = strcasestr(optarg, "rsize=");
				if (p) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((*p == 'k') || (*p == 'K')) {
						num *= 1024;
						p++;
					}
					if ((num <= 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal rsize value -- %s", p2);
					nfsargsp->rsize = num;
					nfsargsp->flags |= NFSMNT_RSIZE;
				}
			}
			if(altflags & ALTF_WSIZE) {
				p = strcasestr(optarg, "rwsize=");
				if (!p)
					p = strcasestr(optarg, "wsize=");
				if (p) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((*p == 'k') || (*p == 'K')) {
						num *= 1024;
						p++;
					}
					if ((num <= 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal wsize value -- %s", p2);
					nfsargsp->wsize = num;
					nfsargsp->flags |= NFSMNT_WSIZE;
				}
			}
			if(altflags & ALTF_READAHEAD) {
				if ((p = strcasestr(optarg, "readahead="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num < 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal readahead value -- %s", p2);
					nfsargsp->readahead = num;
					nfsargsp->flags |= NFSMNT_READAHEAD;
				}
			}
			if(altflags & ALTF_NFSVERS) {
				if ((p = strcasestr(optarg, "vers="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((*p != '\0') && (*p != ','))
						num = 0;
					switch (num) {
					case 2:
						if (force3)
							errx(1, "conflicting NFS version options");
						force2 = 1;
						nfsargsp->flags &= ~NFSMNT_NFSV3;
						break;
					case 3:
						if (force2)
							errx(1, "conflicting NFS version options");
						force3 = 1;
						break;
					default:
						errx(1, "illegal NFS version value -- %s", p2);
						break;
					}
				}
			}
			if(altflags & ALTF_TIMEO) {
				/* need to make sure not to accidentally grab "actimeo=" */
				p = optarg - 1;
				do {
					p++;
					p = strcasestr(p, "timeo=");
				} while (p && (p > optarg) && (*(p-1) != ','));
				if (p) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num <= 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal timeout value -- %s", p2);
					nfsargsp->timeo = num;
					nfsargsp->flags |= NFSMNT_TIMEO;
				}
			}
			if(altflags & ALTF_RETRANS) {
				if ((p = strcasestr(optarg, "retrans="))) {
					p2 = strchr(p, '=') + 1;
					num = strtol(p2, &p, 10);
					if ((num <= 0) || ((*p != '\0') && (*p != ',')))
						errx(1, "illegal retrans value -- %s", p2);
					nfsargsp->retrans = num;
					nfsargsp->flags |= NFSMNT_RETRANS;
				}
			}
			altflags = 0;
			break;
		case 'P':
			nfsargsp->flags |= NFSMNT_RESVPORT;
			break;
#ifdef ISO
		case 'p':
			nfsargsp->sotype = SOCK_SEQPACKET;
			break;
#endif
		case 'q':
			warnx("NQNFS not supported; -q option deprecated");
			break;
		case 'R':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -R value -- %s", optarg);
			retrycnt = num;
			break;
		case 'r':
			num = strtol(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0)
				errx(1, "illegal -r value -- %s", optarg);
			nfsargsp->rsize = num;
			nfsargsp->flags |= NFSMNT_RSIZE;
			break;
		case 's':
			nfsargsp->flags |= NFSMNT_SOFT;
			break;
		case 'T':
			nfsargsp->sotype = SOCK_STREAM;
			nfsproto = IPPROTO_TCP;
			break;
		case 't':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -t value -- %s", optarg);
			nfsargsp->timeo = num;
			nfsargsp->flags |= NFSMNT_TIMEO;
			break;
		case 'w':
			num = strtol(optarg, &p, 10);
			if ((*p == 'k') || (*p == 'K')) {
				num *= 1024;
				p++;
			}
			if (*p || num <= 0)
				errx(1, "illegal -w value -- %s", optarg);
			nfsargsp->wsize = num;
			nfsargsp->flags |= NFSMNT_WSIZE;
			break;
		case 'x':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -x value -- %s", optarg);
			nfsargsp->retrans = num;
			nfsargsp->flags |= NFSMNT_RETRANS;
			break;
		case 'U':
			mnttcp_ok = 0;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
		usage();
		/* NOTREACHED */
	}

	spec = *argv++;

	if (realpath(*argv, name) == NULL)
		err(1, "realpath %s", name);

	if (!getnfsargs(spec, nfsargsp))
		exit(1);

	if (mount("nfs", name, mntflags, nfsargsp))
		err(1, "%s", name);
	if (nfsargsp->flags & NFSMNT_KERB) {
		if ((opflags & ISBGRND) == 0) {
			if (i = fork()) {
				if (i == -1)
					err(1, "nfskerb 1");
				exit(0);
			}
			(void) setsid();
			(void) close(STDIN_FILENO);
			(void) close(STDOUT_FILENO);
			(void) close(STDERR_FILENO);
			(void) chdir("/");
		}
		openlog("mount_nfs:", LOG_PID, LOG_DAEMON);
		nfssvc_flag = NFSSVC_MNTD;
		ncd.ncd_dirp = name;
		while (nfssvc(nfssvc_flag, (caddr_t)&ncd) < 0) {
			if (errno != ENEEDAUTH) {
				syslog(LOG_ERR, "nfssvc err %m");
				continue;
			}
			nfssvc_flag =
			    NFSSVC_MNTD | NFSSVC_GOTAUTH | NFSSVC_AUTHINFAIL;
#ifdef NFSKERB
			/*
			 * Set up as ncd_authuid for the kerberos call.
			 * Must set ruid to ncd_authuid and reset the
			 * ticket name iff ncd_authuid is not the same
			 * as last time, so that the right ticket file
			 * is found.
			 * Get the Kerberos credential structure so that
			 * we have the seesion key and get a ticket for
			 * this uid.
			 * For more info see the IETF Draft "Authentication
			 * in ONC RPC".
			 */
			if (ncd.ncd_authuid != last_ruid) {
				krb_set_tkt_string("");
				last_ruid = ncd.ncd_authuid;
			}
			setreuid(ncd.ncd_authuid, 0);
			kret = krb_get_cred(NFS_KERBSRV, inst, realm, &kcr);
			if (kret == RET_NOTKT) {
		            kret = get_ad_tkt(NFS_KERBSRV, inst, realm,
				DEFAULT_TKT_LIFE);
			    if (kret == KSUCCESS)
				kret = krb_get_cred(NFS_KERBSRV, inst, realm,
				    &kcr);
			}
			if (kret == KSUCCESS)
			    kret = krb_mk_req(&ktick.kt, NFS_KERBSRV, inst,
				realm, 0);

			/*
			 * Fill in the AKN_FULLNAME authenticator and verfier.
			 * Along with the Kerberos ticket, we need to build
			 * the timestamp verifier and encrypt it in CBC mode.
			 */
			if (kret == KSUCCESS &&
			    ktick.kt.length <= (RPCAUTH_MAXSIZ-3*NFSX_UNSIGNED)
			    && gettimeofday(&ktv, (struct timezone *)0) == 0) {
			    ncd.ncd_authtype = RPCAUTH_KERB4;
			    ncd.ncd_authstr = (u_char *)&ktick;
			    ncd.ncd_authlen = nfsm_rndup(ktick.kt.length) +
				3 * NFSX_UNSIGNED;
			    ncd.ncd_verfstr = (u_char *)&kverf;
			    ncd.ncd_verflen = sizeof (kverf);
			    memmove(ncd.ncd_key, kcr.session,
				sizeof (kcr.session));
			    kin.t1 = htonl(ktv.tv_sec);
			    kin.t2 = htonl(ktv.tv_usec);
			    kin.w1 = htonl(NFS_KERBTTL);
			    kin.w2 = htonl(NFS_KERBTTL - 1);
			    bzero((caddr_t)kivec, sizeof (kivec));

			    /*
			     * Encrypt kin in CBC mode using the session
			     * key in kcr.
			     */
			    XXX

			    /*
			     * Finally, fill the timestamp verifier into the
			     * authenticator and verifier.
			     */
			    ktick.kind = htonl(RPCAKN_FULLNAME);
			    kverf.kind = htonl(RPCAKN_FULLNAME);
			    NFS_KERBW1(ktick.kt) = kout.w1;
			    ktick.kt.length = htonl(ktick.kt.length);
			    kverf.verf.t1 = kout.t1;
			    kverf.verf.t2 = kout.t2;
			    kverf.verf.w2 = kout.w2;
			    nfssvc_flag = NFSSVC_MNTD | NFSSVC_GOTAUTH;
			}
			setreuid(0, 0);
#endif /* NFSKERB */
		}
	}
	exit(0);
}

int
getnfsargs(spec, nfsargsp)
	char *spec;
	struct nfs_args *nfsargsp;
{
	register CLIENT *clp;
	struct hostent *hp;
	static struct sockaddr_in saddr;
#ifdef ISO
	static struct sockaddr_iso isoaddr;
	struct iso_addr *isop;
	int isoflag = 0;
#endif
	struct timeval pertry, try;
	enum clnt_stat clnt_stat;
	int so = RPC_ANYSOCK, i, nfsvers, mntvers, orgcnt;
	char *hostp, *delimp;
#ifdef NFSKERB
	char *cp;
#endif
	u_short tport = 0;
	static struct nfhret nfhret;
	static char nam[MNAMELEN + 1];

	strncpy(nam, spec, MNAMELEN);
	nam[MNAMELEN] = '\0';
	if ((delimp = strchr(spec, '@')) != NULL) {
		hostp = delimp + 1;
	} else if ((delimp = strchr(spec, ':')) != NULL) {
		hostp = spec;
		spec = delimp + 1;
	} else {
		warnx("no <host>:<dirpath> or <dirpath>@<host> spec");
		return (0);
	}
	*delimp = '\0';
	/*
	 * DUMB!! Until the mount protocol works on iso transport, we must
	 * supply both an iso and an inet address for the host.
	 */
#ifdef ISO
	if (!strncmp(hostp, "iso=", 4)) {
		u_short isoport;

		hostp += 4;
		isoflag++;
		if ((delimp = strchr(hostp, '+')) == NULL) {
			warnx("no iso+inet address");
			return (0);
		}
		*delimp = '\0';
		if ((isop = iso_addr(hostp)) == NULL) {
			warnx("bad ISO address");
			return (0);
		}
		memset(&isoaddr, 0, sizeof (isoaddr));
		memmove(&isoaddr.siso_addr, isop, sizeof (struct iso_addr));
		isoaddr.siso_len = sizeof (isoaddr);
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		isoport = htons(NFS_PORT);
		memmove(TSEL(&isoaddr), &isoport, isoaddr.siso_tlen);
		hostp = delimp + 1;
	}
#endif /* ISO */

	/*
	 * Handle an internet host address and reverse resolve it if
	 * doing Kerberos.
	 */
	if (isdigit(*hostp)) {
		if ((saddr.sin_addr.s_addr = inet_addr(hostp)) == -1) {
			warnx("bad net address %s", hostp);
			return (0);
		}
	} else if ((hp = gethostbyname(hostp)) != NULL)
		memmove(&saddr.sin_addr, hp->h_addr, hp->h_length);
	else {
		warnx("can't get net id for host");
		return (0);
        }
#ifdef NFSKERB
	if ((nfsargsp->flags & NFSMNT_KERB)) {
		if ((hp = gethostbyaddr((char *)&saddr.sin_addr.s_addr,
		    sizeof (u_long), AF_INET)) == (struct hostent *)0) {
			warnx("can't reverse resolve net address");
			return (0);
		}
		memmove(&saddr.sin_addr, hp->h_addr, hp->h_length);
		strncpy(inst, hp->h_name, INST_SZ);
		inst[INST_SZ - 1] = '\0';
		if (cp = strchr(inst, '.'))
			*cp = '\0';
	}
#endif /* NFSKERB */

	if (force2) {
		nfsvers = NFS_VER2;
		mntvers = RPCMNT_VER1;
	} else {
		nfsvers = NFS_VER3;
		mntvers = RPCMNT_VER3;
	}
	orgcnt = retrycnt;
tryagain:
	nfhret.stat = EACCES;	/* Mark not yet successful */
	while (retrycnt > 0) {
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(PMAPPORT);
		if ((tport = pmap_getport(&saddr, RPCPROG_NFS,
		    nfsvers, nfsproto)) == 0) {
			if ((opflags & ISBGRND) == 0)
				clnt_pcreateerror("NFS Portmap");
		} else {
			saddr.sin_port = 0;
			pertry.tv_sec = 10;
			pertry.tv_usec = 0;
			/*
			 * temporarily revert to root, to avoid reserved port
			 * number restriction (port# less than 1024)
			 */
			seteuid(eff_uid);
			if (mnttcp_ok && nfsargsp->sotype == SOCK_STREAM)
			    clp = clnttcp_create(&saddr, RPCPROG_MNT, mntvers,
				&so, 0, 0);
			else
			    clp = clntudp_create(&saddr, RPCPROG_MNT, mntvers,
				pertry, &so);
			seteuid(real_uid);
			if (clp == NULL) {
				if ((opflags & ISBGRND) == 0)
					clnt_pcreateerror("Cannot MNT RPC");
			} else {
				clp->cl_auth = authunix_create_default();
				try.tv_sec = 10;
				try.tv_usec = 0;
				if (nfsargsp->flags & NFSMNT_KERB)
				    nfhret.auth = RPCAUTH_KERB4;
				else
				    nfhret.auth = RPCAUTH_UNIX;
				nfhret.vers = mntvers;
				clnt_stat = clnt_call(clp, RPCMNT_MOUNT,
				    xdr_dir, spec, xdr_fh, &nfhret, try);
				switch (clnt_stat) {
				case RPC_PROGVERSMISMATCH:
					if (nfsvers == NFS_VER3 && !force3) {
						retrycnt = orgcnt;
						nfsvers = NFS_VER2;
						mntvers = RPCMNT_VER1;
						nfsargsp->flags &=
							~NFSMNT_NFSV3;
						goto tryagain;
					} else {
						errx(1, "%s", clnt_sperror(clp,
							"MNT RPC"));
					}
				case RPC_SUCCESS:
					auth_destroy(clp->cl_auth);
					clnt_destroy(clp);
					retrycnt = 0;
					break;
				default:
					/* XXX should give up on some errors */
					if ((opflags & ISBGRND) == 0)
						warnx("%s", clnt_sperror(clp,
						    "bad MNT RPC"));
					break;
				}
			}
		}
		if (--retrycnt > 0) {
			if (opflags & BGRND) {
				opflags &= ~BGRND;
				if (i = fork()) {
					if (i == -1)
						err(1, "nfskerb 2");
					exit(0);
				}
				(void) setsid();
				(void) close(STDIN_FILENO);
				(void) close(STDOUT_FILENO);
				(void) close(STDERR_FILENO);
				(void) chdir("/");
				opflags |= ISBGRND;
			}
			sleep(60);
		}
	}
	if (nfhret.stat) {
		if (opflags & ISBGRND)
			exit(1);
		warnx("can't access %s: %s", spec, strerror(nfhret.stat));
		return (0);
	}
	saddr.sin_port = htons(tport);
#ifdef ISO
	if (isoflag) {
		nfsargsp->addr = (struct sockaddr *) &isoaddr;
		nfsargsp->addrlen = sizeof (isoaddr);
	} else
#endif /* ISO */
	{
		nfsargsp->addr = (struct sockaddr *) &saddr;
		nfsargsp->addrlen = sizeof (saddr);
	}
	nfsargsp->fh = nfhret.nfh;
	nfsargsp->fhsize = nfhret.fhsize;
	nfsargsp->hostname = nam;
	return (1);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

int
xdr_fh(xdrsp, np)
	XDR *xdrsp;
	register struct nfhret *np;
{
	register int i;
	long auth, authcnt, authfnd = 0;

	if (!xdr_u_long(xdrsp, &np->stat))
		return (0);
	if (np->stat)
		return (1);
	switch (np->vers) {
	case 1:
		np->fhsize = NFSX_V2FH;
		return (xdr_opaque(xdrsp, (caddr_t)np->nfh, NFSX_V2FH));
	case 3:
		if (!xdr_long(xdrsp, &np->fhsize))
			return (0);
		if (np->fhsize <= 0 || np->fhsize > NFSX_V3FHMAX)
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)np->nfh, np->fhsize))
			return (0);
		if (!xdr_long(xdrsp, &authcnt))
			return (0);
		for (i = 0; i < authcnt; i++) {
			if (!xdr_long(xdrsp, &auth))
				return (0);
			if (auth == np->auth)
				authfnd++;
		}
		/*
		 * Some servers, such as DEC's OSF/1 return a nil authenticator
		 * list to indicate RPCAUTH_UNIX.
		 */
		if (!authfnd && (authcnt > 0 || np->auth != RPCAUTH_UNIX))
			np->stat = EAUTH;
		return (1);
	};
	return (0);
}

__dead void
usage()
{
	(void)fprintf(stderr, "usage: mount_nfs %s\n%s\n%s\n%s\n",
"[-23bcdiKkLlMPsT] [-a maxreadahead]",
"\t[-g maxgroups] [-m realm] [-o options] [-R retrycnt]",
"\t[-r readsize] [-t timeout] [-w writesize] [-x retrans]",
"\trhost:path node");
	exit(1);
}
