/*
 * store.c - common global storage for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: store.c,v 1.38 2008/10/21 16:21:41 abe Exp $";
#endif


#include "lsof.h"


/*
 * Global storage definitions
 */

#if	defined(HASBLKDEV)
struct l_dev *BDevtp = (struct l_dev *)NULL;
				/* block device table pointer */
int BNdev = 0;			/* number of entries in BDevtp[] */
struct l_dev **BSdev = (struct l_dev **)NULL;
				/* pointer to BDevtp[] pointers, sorted
				 * by device */
#endif	/* defined(HASBLKDEV) */

int CkPasswd = 0;		/* time to check /etc/passwd for change */

#if	defined(HAS_STD_CLONE)
struct clone *Clone = (struct clone *)NULL;
				/* clone device list */
#endif	/* defined(HAS_STD_CLONE) */

int CmdColW;			/* COMMAND column width */
struct str_lst *Cmdl = (struct str_lst *)NULL;
				/* command names selected with -c */
int CmdLim = CMDL;		/* COMMAND column width limit */
int Cmdni = 0;			/* command name inclusions selected with -c */
int Cmdnx = 0;			/* command name exclusions selected with -c */
lsof_rx_t *CmdRx = (lsof_rx_t *)NULL;
				/* command regular expression table */

#if	defined(HASSELINUX)
cntxlist_t *CntxArg = (cntxlist_t *)NULL;
				/* security context arguments supplied with
				 * -Z */
int CntxColW;			/* security context column width */
int CntxStatus = 0;		/* security context status: 0 == disabled,
				 * 1 == enabled */
#endif	/* defined(HASSELINUX) */

#if	defined(HASDCACHE)
unsigned DCcksum;		/* device cache file checksum */
int DCfd = -1;			/* device cache file descriptor */
FILE *DCfs = (FILE *)NULL;	/* stream pointer for DCfd */
char *DCpathArg = (char *)NULL;	/* device cache path from -D[b|r|u]<path> */
char *DCpath[] = {		/* device cache paths, indexed by DCpathX
				 *when it's >= 0 */
	(char *)NULL, (char *)NULL, (char *)NULL, (char *)NULL
};
int DCpathX = -1;		/* device cache path index:
				 *	-1 = path not defined
				 *	 0 = defined via -D
				 *	 1 = defined via HASENVDC
				 *	 2 = defined via HASSYSDC
				 *	 3 = defined via HASPERSDC and
				 *	     HASPERSDCPATH */
int DCrebuilt = 0;		/* an unsafe device cache file has been
				 * rebuilt */
int DCstate = 3;		/* device cache state:
				 *	0 = ignore (-Di)
				 *	1 = build (-Db[path])
				 *	2 = read; don't rebuild (-Dr[path])
				 *	3 = update; read and rebuild if
				 *	    necessary (-Du[path])
				 */
int DCunsafe = 0;		/* device cache file is potentially unsafe,
				 * (The [cm]time check failed.) */
#endif	/* defined(HASDCACHE) */

int DChelp = 0;			/* -D? status */

int DevColW;			/* DEVICE column width */
dev_t DevDev;			/* device number of /dev or its equivalent */
struct l_dev *Devtp = (struct l_dev *)NULL;
				/* device table pointer */


/*
 * Externals for a stkdir(), dumbed-down for older AIX compilers.
 */

char **Dstk = (char **)NULL;	/* the directory stack */
int Dstkx = 0;			/* Dstk[] index */
int Dstkn = 0;			/* Dstk[] entries allocated */

int ErrStat = 0;		/* path stat() error count */
uid_t Euid;			/* effective UID of this lsof process */
int Fand = 0;			/* -a option status */
int Fblock = 0;			/* -b option status */
int Fcntx = 0;			/* -Z option status */
int FdColW;			/* FD column width */
int Ffilesys = 0;		/* -f option status:
				 *    0 = paths may be file systems
				 *    1 = paths are just files
				 *    2 = paths must be file systems */

#if	defined(HASNCACHE)
int Fncache = 1;		/* -C option status */
int NcacheReload = 1;		/* 1 == call ncache_load() */
#endif	/* defined(HASNCACHE) */

int Ffield = 0;			/* -f and -F status */
int Fhelp = 0;			/* -h option status */
int Fhost = 1;			/* -H option status */
int Fnet = 0;			/* -i option status: 0==none
				 *		     1==find all
				 *		     2==some found*/
int FnetTy = 0;			/* Fnet type request: 0==all
				 *		      4==IPv4
				 *		      6==IPv6 */
int Fnfs = 0;			/* -N option status: 0==none, 1==find all,
				 * 2==some found*/
int Fnlink = 0;			/* -L option status */
int Foffset = 0;		/* -o option status */
int Fovhd = 0;			/* -O option status */
int Fport = 1;			/* -P option status */

#if	defined(HASPMAPENABLED)
int FportMap = 1;		/* +|-M option status */
#else	/* !defined(HASPMAPENABLED) */
int FportMap = 0;		/* +|-M option status */
#endif	/* defined(HASPMAPENABLED) */

int Fpgid = 0;			/* -g option status */
int Fppid = 0;			/* -R option status */
int Fsize = 0;			/* -s option status */
int FcColW;			/* FCT column width */
int FgColW;			/* FILE-FLAG column width */
int FsColW;			/* FSTR-ADDR column width */
int Fsv = FSV_DEFAULT;		/* file struct value selections */
int FsvByf = 0;			/* Fsv was set by +f */
int FsvFlagX = 0;		/* hex format status for FSV_FG */
int NiColW;			/* NODE-ID column width */
char *NiTtl = NITTL;		/* NODE-ID column title */
int Ftcptpi = TCPTPI_STATE;	/* -T option status */
int Fterse = 0;			/* -t option status */
int Funix = 0;			/* -U option status */
int Futol = 1;			/* -l option status */
int Fverbose = 0;		/* -V option status */

#if	defined(WARNINGSTATE)
int Fwarn = 1;			/* +|-w option status */
#else	/* !defined(WARNINGSTATE) */
int Fwarn = 0;			/* +|-w option status */
#endif	/* defined(WARNINGSTATE) */

#if	defined(HASXOPT_VALUE)
int Fxopt = HASXOPT_VALUE;	/* -X option status */
#endif	/* defined(HASXOPT_VALUE) */

int Fxover = 0;			/* -x option value */
int Fzone = 0;			/* -z option status */

struct fd_lst *Fdl = (struct fd_lst *)NULL;
				/* file descriptors selected with -d */
int FdlTy = -1;			/* Fdl[] type: -1 == none
				 *		0 == include
				 *		1 == exclude */

struct fieldsel FieldSel[] = {
    { LSOF_FID_ACCESS, 0,  LSOF_FNM_ACCESS, NULL,     0		 }, /*  0 */
    { LSOF_FID_CMD,    0,  LSOF_FNM_CMD,    NULL,     0		 }, /*  1 */
    { LSOF_FID_CT,     0,  LSOF_FNM_CT,     &Fsv,     FSV_CT 	 }, /*  2 */
    { LSOF_FID_DEVCH,  0,  LSOF_FNM_DEVCH,  NULL,     0		 }, /*  3 */
    { LSOF_FID_DEVN,   0,  LSOF_FNM_DEVN,   NULL,     0		 }, /*  4 */
    { LSOF_FID_FD,     0,  LSOF_FNM_FD,     NULL,     0		 }, /*  5 */
    { LSOF_FID_FA,     0,  LSOF_FNM_FA,     &Fsv,     FSV_FA	 }, /*  6 */
    { LSOF_FID_FG,     0,  LSOF_FNM_FG,     &Fsv,     FSV_FG	 }, /*  7 */
    { LSOF_FID_INODE,  0,  LSOF_FNM_INODE,  NULL,     0		 }, /*  8 */
    { LSOF_FID_NLINK,  0,  LSOF_FNM_NLINK,  &Fnlink,  1		 }, /*  9 */
    { LSOF_FID_LOCK,   0,  LSOF_FNM_LOCK,   NULL,     0		 }, /* 10 */
    { LSOF_FID_LOGIN,  0,  LSOF_FNM_LOGIN,  NULL,     0		 }, /* 11 */
    { LSOF_FID_MARK,   1,  LSOF_FNM_MARK,   NULL,     0		 }, /* 12 */
    { LSOF_FID_NAME,   0,  LSOF_FNM_NAME,   NULL,     0		 }, /* 13 */
    { LSOF_FID_NI,     0,  LSOF_FNM_NI,     &Fsv,     FSV_NI	 }, /* 14 */
    { LSOF_FID_OFFSET, 0,  LSOF_FNM_OFFSET, NULL,     0		 }, /* 15 */
    { LSOF_FID_PID,    1,  LSOF_FNM_PID,    NULL,     0		 }, /* 16 */
    { LSOF_FID_PGID,   0,  LSOF_FNM_PGID,   &Fpgid,   1		 }, /* 17 */
    { LSOF_FID_PROTO,  0,  LSOF_FNM_PROTO,  NULL,     0		 }, /* 18 */
    { LSOF_FID_RDEV,   0,  LSOF_FNM_RDEV,   NULL,     0		 }, /* 19 */
    { LSOF_FID_PPID,   0,  LSOF_FNM_PPID,   &Fppid,   1		 }, /* 20 */
    { LSOF_FID_SIZE,   0,  LSOF_FNM_SIZE,   NULL,     0		 }, /* 21 */
    { LSOF_FID_STREAM, 0,  LSOF_FNM_STREAM, NULL,     0		 }, /* 22 */
    { LSOF_FID_TYPE,   0,  LSOF_FNM_TYPE,   NULL,     0		 }, /* 23 */
    { LSOF_FID_TCPTPI, 0,  LSOF_FNM_TCPTPI, &Ftcptpi, TCPTPI_ALL }, /* 24 */
    { LSOF_FID_UID,    0,  LSOF_FNM_UID,    NULL,     0		 }, /* 25 */
    { LSOF_FID_ZONE,   0,  LSOF_FNM_ZONE,   &Fzone,   1		 }, /* 26 */
    { LSOF_FID_CNTX,   0,  LSOF_FNM_CNTX,   &Fcntx,   1		 }, /* 27 */
    { LSOF_FID_TERM,   0,  LSOF_FNM_TERM,   NULL,     0		 }, /* 28 */

#if	defined(HASFIELDAP1)
    { '1',	       0,  HASFIELDAP1,     NULL,     0		 }, /* TERM+1 */
#endif	/* defined(HASFIELDAP1) */

#if	defined(HASFIELDAP2)
    { '2',	       0,  HASFIELDAP2,     NULL,     0		 }, /* TERM+2 */
#endif	/* defined(HASFIELDAP2) */

#if	defined(HASFIELDAP3)
    { '3',	       0,  HASFIELDAP3,     NULL,     0		 }, /* TERM+3 */
#endif	/* defined(HASFIELDAP3) */

#if	defined(HASFIELDAP4)
    { '4',	       0,  HASFIELDAP4,     NULL,     0		 }, /* TERM+4 */
#endif	/* defined(HASFIELDAP4) */

#if	defined(HASFIELDAP5)
    { '5',	       0,  HASFIELDAP5,     NULL,     0		 }, /* TERM+5 */
#endif	/* defined(HASFIELDAP5) */

#if	defined(HASFIELDAP6)
    { '6',	       0,  HASFIELDAP6,     NULL,     0		 }, /* TERM+6 */
#endif	/* defined(HASFIELDAP6) */

#if	defined(HASFIELDAP7)
    { '7',	       0,  HASFIELDAP7,     NULL,     0		 }, /* TERM+7 */
#endif	/* defined(HASFIELDAP7) */

#if	defined(HASFIELDAP8)
    { '8',	       0,  HASFIELDAP8,     NULL,     0		 }, /* TERM+8 */
#endif	/* defined(HASFIELDAP8) */

#if	defined(HASFIELDAP9)
    { '9',	       0,  HASFIELDAP9,     NULL,     0		 }, /* TERM+9 */
#endif	/* defined(HASFIELDAP9) */

    { ' ',	       0,  NULL,	    NULL,     0		 }
};

int Hdr = 0;			/* header print status */
char *InodeFmt_d = (char *) NULL;
				/* INODETYPE decimal printf specification */
char *InodeFmt_x = (char *) NULL;
				/* INODETYPE hexadecimal printf specification */
struct lfile *Lf = (struct lfile *)NULL;
				/* current local file structure */
struct lproc *Lp = (struct lproc *)NULL;
				/* current local process table entry */
struct lproc *Lproc = (struct lproc *)NULL;
				/* local process table */
char *Memory = (char *)NULL;	/* core file path */
int MntSup = 0;			/* mount supplement state: 0 == none
				 *			   1 == create
				 *			   2 == read */
char *MntSupP = (char *)NULL;	/* mount supplement path -- if MntSup == 2 */

#if	defined(HASPROCFS)
struct mounts *Mtprocfs = (struct mounts *)NULL;
				/* /proc mount entry */
#endif	/* defined(HASPROCFS) */

int Mxpgid = 0;			/* maximum process group ID table entries */
int Mxpid = 0;			/* maximum PID table entries */
int Mxuid = 0;			/* maximum UID table entries */
gid_t Mygid;			/* real GID of this lsof process */
int Mypid;			/* lsof's process ID */
uid_t Myuid;			/* real UID of this lsof process */
char *Namech = (char *)NULL;	/* name characters for printing */
size_t Namechl = (size_t)0;	/* sizeof(Namech) */
int NCmdRxU = 0;		/* number of CmdRx[] entries */
int Ndev = 0;			/* number of entries in Devtp[] */

#if	defined(HASNLIST)
struct NLIST_TYPE *Nl = (struct NLIST_TYPE *)NULL;
				/* kernel name list */
int Nll = 0;			/* Nl calloc'd length */
#endif	/* defined(HASNLIST) */

long Nlink = 0l;		/* report nlink values below this number
				 * (0 = report all nlink values) */
int Nlproc = 0;			/* number of entries in Lproc[] */
int NlColW;			/* NLINK column width */
int NmColW;			/* NAME column width */
char *Nmlst = (char *)NULL;	/* namelist file path */
int NodeColW;			/* NODE column width */
int Npgid = 0;			/* -g option count */
int Npgidi = 0;			/* -g option inclusion count */
int Npgidx = 0;			/* -g option exclusion count */
int Npid = 0;			/* -p option count */
int Npidi = 0;			/* -p option inclusion count */
int Npidx = 0;			/* -p option exclusion count */
int Npuns;			/* number of unselected PIDs (starts at Npid) */
int Ntype;			/* node type (see N_* symbols) */
int Nuid = 0;			/* -u option count */
int Nuidexcl = 0;		/* -u option count of UIDs excluded */
int Nuidincl = 0;		/* -u option count of UIDs included */
struct nwad *Nwad = (struct nwad *)NULL;
				/* list of network addresses */
int OffDecDig = OFFDECDIG;	/* offset decimal form (0t...) digit limit */
int OffColW;			/* OFFSET column width */
int PgidColW;			/* PGID column width */
int PidColW;			/* PID column width */
struct lfile *Plf = (struct lfile *)NULL;
				/* previous local file structure */
char *Pn;			/* program name */
int PpidColW;			/* PPID column width */

#if	defined(HASPROCFS)
int Procfind = 0;		/* 1 when searching for an proc file system
				 * file and one was found */
struct procfsid *Procfsid = (struct procfsid *)NULL;
				/* proc file system PID search table */
int Procsrch = 0;		/* 1 if searching for any proc file system
				 * file */
#endif	/* defined(HASPROCFS) */

int PrPass = 0;			/* print pass: 0 = compute column widths
				 *	       1 = print */
int RptTm = 0;			/* repeat time -- set by -r */
struct l_dev **Sdev = (struct l_dev **)NULL;
				/* pointer to Devtp[] pointers, sorted
				 * by device */
int Selall = 1;			/* all processes are selected (default) */
int Selflags = 0;		/* selection flags -- see SEL* in lsof.h */
int Setgid = 0;			/* setgid state */
int Selinet = 0;		/* select only Internet socket files */
int Setuidroot = 0;		/* setuid-root state */
struct sfile *Sfile = (struct sfile *)NULL;
				/* chain of files to search for */
struct int_lst *Spgid = (struct int_lst *)NULL;
				/* process group IDs to search for */
struct int_lst *Spid = (struct int_lst *)NULL;
				/* Process IDs to search for */
struct seluid *Suid = (struct seluid *)NULL;
				/* User IDs to include or exclude */
int SzColW;			/* SIZE column width */
int SzOffColW;			/* SIZE/OFF column width */
char *SzOffFmt_0t = (char *)NULL;
				/* SZOFFTYPE 0t%u printf specification */
char *SzOffFmt_d = (char *)NULL;
				/* SZOFFTYPE %d printf  specification */
char *SzOffFmt_dv = (char *)NULL;
				/* SZOFFTYPE %*d printf  specification */
char *SzOffFmt_x = (char *)NULL;
				/* SZOFFTYPE %#x printf  specification */
int TcpStAlloc = 0;		/* allocated (possibly unused) entries in TCP 
				 * state tables */
unsigned char *TcpStI = (unsigned char *)NULL;
				/* included TCP states */
int TcpStIn = 0;		/* number of entries in TcpStI[] */
int TcpStOff = 0;		/* offset for TCP state number to adjust
				 * negative numbers to an index into TcpSt[],
				 * TcpStI[] and TcpStX[] */
unsigned char *TcpStX = (unsigned char *)NULL;
				/* excluded TCP states */
int TcpStXn = 0;		/* number of entries in TcpStX[] */
int TcpNstates = 0;		/* number of TCP states -- either in
				 * tcpstates[] or TcpSt[] */
char **TcpSt = (char **)NULL;	/* local TCP state names, indexed by system
				 * state value */
char Terminator = '\n';		/* output field terminator */
int TmLimit = TMLIMIT;		/* Readlink() and stat() timeout (seconds) */
int TypeColW;			/* TYPE column width */
int UdpStAlloc = 0;		/* allocated (possibly unused) entries in UDP 
				 * state tables */
unsigned char *UdpStI = (unsigned char *)NULL;
				/* included UDP states */
int UdpStIn = 0;		/* number of entries in UdpStI[] */
int UdpStOff = 0;		/* offset for UDP state number to adjust
				 * negative numbers to an index into UdpSt[],
				 * UdpStI[] and UdpStX[] */
unsigned char *UdpStX = (unsigned char *)NULL;
				/* excluded UDP states */
int UdpStXn = 0;		/* number of entries in UdpStX[] */
int UdpNstates = 0;		/* number of UDP states  in UdpSt[] */
char **UdpSt = (char **)NULL;	/* local UDP state names, indexed by system
				 * state number */
int UserColW;			/* USER column width */

#if	defined(HASZONES)
znhash_t **ZoneArg = (znhash_t **)NULL;
				/* zone arguments supplied with -z */
#endif	/* defined(HASZONES) */

int ZoneColW;			/* ZONE column width */
