/*
 * machine.h - Linux definitions for /proc-based lsof
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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


/*
 * $Id: machine.h,v 1.33 2008/10/21 16:17:21 abe Exp $
 */


#if	!defined(LSOF_MACHINE_H)
#define	LSOF_MACHINE_H	1


#include <sys/types.h>
#include <sys/param.h>


/*
 * CAN_USE_CLNT_CREATE is defined for those dialects where RPC clnt_create()
 * can be used to obtain a CLIENT handle in lieu of clnttcp_create().
 */

#define	CAN_USE_CLNT_CREATE	1


/*
 * DEVDEV_PATH defines the path to the directory that contains device
 * nodes.
 */

#define	DEVDEV_PATH	"/dev"


/*
 * GET_MAX_FD is defined for those dialects that provide a function other than
 * getdtablesize() to obtain the maximum file descriptor number plus one.
 */

/* #define	GET_MAX_FD	?	*/


/*
 * HASAOPT is defined for those dialects that have AFS support; it specifies
 * that the default path to an alternate AFS kernel name list file may be
 * supplied with the -A <path> option.
 */

/* #define	HASAOPT		1 */


/*
 * HASBLKDEV is defined for those dialects that want block device information
 * recorded in BDevtp[].
 */

/* #define	HASBLKDEV	1 */


/*
 * HASDCACHE is defined for those dialects that support a device cache
 * file.
 *
 * CAUTION!!!  Do not enable HASDCACHE for /proc-based Linux lsof.  The source
 *	       code cannot support it.
 *
 * The presence of NEVER_HASDCACHE in this comment prevents the Customize
 * script from offering to change HASDCACHE.
 *
 *
 * HASENVDC defined the name of an environment variable that contains the
 * device cache file path.  The HASENVDC environment variable is ignored when
 * the lsof process is setuid(root) or its real UID is 0.
 *
 * HASPERSDC defines the format for the last component of a personal device
 * cache file path.  The first will be the home directory of the real UID that
 * executes lsof.
 *
 * HASPERSDCPATH defines the environment variable whose value is the middle
 * component of the personal device cache file path.  The middle component
 * follows the home directory and precedes the results of applying HASPERSDC.
 * The HASPERSDCPATH environment variable is ignored when the lsof process is
 * setuid(root) or its real UID is 0.
 *
 * HASSYSDC defines a public device cache file path.  When it's defined, it's
 * used as the path from which to read the device cache.
 *
 * Consult the 00DCACHE and 00FAQ files of the lsof distribution for more
 * information on device cache file path construction.
 *
 * CAUTION!!!  Do not enable HASDCACHE for /proc-based Linux lsof.  The source
 *	       code cannot support it.
 */

/* #define	HASDCACHE	1  !!!DON'T ENABLE!!! -- see above comment */
/* #define	HASENVDC	"LSOFDEVCACHE" */
/* #define	HASPERSDC	"%h/%p.lsof_%L" */
/* #define	HASPERSDCPATH	"LSOFPERSDCPATH" */
/* #define	HASSYSDC	"/your/choice/of/path" */


/*
 * HASCDRNODE is defined for those dialects that have CD-ROM nodes.
 */

/* #define	HASCDRNODE	1 */


/*
 * HASFIFONODE is defined for those dialects that have FIFO nodes.
 */

/* #define	HASFIFONODE	1 */


/*
 * HASFSINO is defined for those dialects that have the file system
 * inode element, fs_ino, in the lfile structure definition in lsof.h.
 */

/* #define	HASFSINO	1 */


/*
 * HASFSTRUCT is defined if the dialect has a file structure.
 *
 * FSV_DEFAULT defines the default set of file structure values to list.
 * It defaults to zero (0), but may be made up of a combination of the
 * FSV_* symbols from lsof.h.
 *
 *   HASNOFSADDR  -- has no file structure address
 *   HASNOFSFLAGS -- has no file structure flags
 *   HASNOFSCOUNT -- has no file structure count
 *   HASNOFSNADDR -- has no file structure node address
 */

#define	HASFSTRUCT	1
/* #define	FSV_DEFAULT	FSV_? | FSV_? | FSV_? */
#define	HASNOFSADDR	1	/* has no file structure address */
/* #define	HASNOFSFLAGS	1	has no file structure flags */
#define	HASNOFSCOUNT	1	/* has no file structure count */
#define	HASNOFSNADDR	1	/* has no file structure node address */


/*
 * HASGNODE is defined for those dialects that have gnodes.
 */

/* #define	HASGNODE	1 */


/*
 * HASHSNODE is defined for those dialects that have High Sierra nodes.
 */

/* #define	HASHSNODE	1 */


/*
 * HASINODE is defined for those dialects that have inodes and wish to
 * use readinode() from node.c.
 */

/* #define	HASINODE	1 */


/*
 * HASINTSIGNAL is defined for those dialects whose signal function returns
 * an int.
 */

/* #define	HASINTSIGNAL	1 */


/*
 * HASKERNIDCK is defined for those dialects that support the comparison of
 * the build to running kernel identity.
 */

/* #define	HASKERNIDCK	1	*/


/*
 * HASKOPT is defined for those dialects that support the -k option of
 * reading the kernel's name list from an optional file.
 */

/* #define	HASKOPT	1 */


/*
 * HASLFILEADD is defined for those dialects that need additional elements
 * in struct lfile.  The HASLFILEADD definition is a macro that defines
 * them.  If any of the additional elements need to be preset in the
 * alloc_lfile() function of proc.c, the SETLFILEADD macro may be defined
 * to do that.
 *
 * If any additional elements need to be cleared in alloc_lfile() or in the
 * free_proc() function of proc.c, the CLRLFILEADD macro may be defined to
 * do that.  Note that CLRLFILEADD takes one argument, the pointer to the
 * lfile struct.  The CLRLFILEADD macro is expected to expand to statements
 * that are complete -- i.e., have terminating semi-colons -- so the macro is
 * called without a terminating semicolon by proc.c.
 *
 * The HASXOPT definition may be used to select the conditions under which
 * private lfile elements are used.
 */

/* #define HASLFILEADD int ... */
/* #define CLRLFILEADD(lf)	(lf)->... = (type)NULL;	*/
/* #define SETLFILEADD Lf->... */


/*
 * HASLWP is defined for dialects that have LWP support inside processes.
 */

#define	HASLWP	1


/*
 * HASMNTSTAT indicates the dialect supports the mount stat(2) result option
 * in its l_vfs and mounts structures.
 */

/* #define	HASMNTSTAT	1	*/


/*
 * HASMNTSUP is defined for those dialects that support the mount supplement
 * option.
 */

#define	HASMNTSUP	1


/*
 * HASMOPT is defined for those dialects that support the reading of
 * kernel memory from an alternate file.
 */

/* #define	HASMOPT	1 */


/*
 * HASNCACHE is defined for those dialects that have a kernel name cache
 * that lsof can search.  A value of 1 directs printname() to prefix the
 * cache value with the file system directory name; 2, avoid the prefix.
 *
 * NCACHELDPFX is a set of C commands to execute before calling ncache_load().
 *
 * NCACHELDSFX is a set of C commands to execute after calling ncache_load().
 */

/* #define	HASNCACHE	1 */
/* #define	NCACHELDPFX	??? */
/* #define	NCACHELDSFX	??? */


/*
 * HASNLIST is defined for those dialects that use nlist() to acccess
 * kernel symbols.
 */

/* #define	HASNLIST	1 */


/*
 * HASPIPEFN is defined for those dialects that have a special function to
 * process DTYPE_PIPE file structure entries.  Its value is the name of the
 * function.
 *
 * NOTE: don't forget to define a prototype for this function in dproto.h.
 */

/* #define	HASPIPEFN	process_pipe? */


/*
 * HASPIPENODE is defined for those dialects that have pipe nodes.
 */

/* #define	HASPIPENODE	1 */


/*
 * HASPMAPENABLED is defined when the reporting of portmapper registration
 * info is enabled by default.
 */

/* #define	HASPMAPENABLED	1 */


/*
 * HASPPID is defined for those dialects that support identification of
 * the parent process IDentifier (PPID) of a process.
 */

#define	HASPPID		1


/*
 * HASPRINTDEV, HASPRINTINO, HASPRINTNM, HASPRINTOFF, and HASPRINTSZ
 * define private dialect-specific functions for printing DEVice numbers,
 * INOde numbers, NaMes, file OFFsets, and file SiZes.  The functions are
 * called from print_file().
 */

/* #define	HASPRINTDEV	print_dev?	*/
/* #define	HASPRINTINO	print_ino?	*/
/* #define	HASPRINTNM	print_nm?	*/
/* #define	HASPRINTOFF	print_off?	*/
/* #define	HASPRINTSZ	print_sz?	*/


/*
 * HASPRIVFILETYPE and PRIVFILETYPE are defined for dialects that have a
 * file structure type that isn't defined by a DTYPE_* symbol.  They are
 * used in lib/prfp.c to select the type's processing.
 *
 * PRIVFILETYPE is the definition of the f_type value in the file struct.
 *
 * HASPRIVFILETYPE is the name of the processing function.
 */

/* #define	HASPRIVFILETYPE	process_shmf?	*/
/* #define	PRIVFILETYPE	??	*/


/*
 * HASPRIVNMCACHE is defined for dialects that have a private method for
 * printing cached NAME column values for some files.  HASPRIVNAMECACHE
 * is defined to be the name of the function.
 *
 * The function takes one argument, a struct lfile pointer to the file, and
 * returns non-zero if it prints a name to stdout.
 */

/* #define	HASPRIVNMCACHE	<function name>	*/


/*
 * HASPRIVPRIPP is defined for dialects that have a private function for
 * printing IP protocol names.  When HASPRIVPRIPP isn't defined, the
 * IP protocol name printing function defaults to printiprto().
 */

/* #define	HASPRIVPRIPP	1	*/


/*
 * HASPROCFS is defined for those dialects that have a proc file system --
 * usually /proc and usually in SYSV4 derivatives.
 *
 * HASFSTYPE is defined as 1 for those systems that have a file system type
 * string, st_fstype, in the stat() buffer; 2, for those systems that have a
 * file system type integer in the stat() buffer, named MOUNTS_STAT_FSTYPE;
 * 0, for systems whose stat(2) structure has no file system type member.  The
 * additional symbols MOUNTS_FSTYPE, RMNT_FSTYPE, and RMNT_STAT_FSTYPE may be
 * defined in dlsof.h to direct how the readmnt() function in lib/rmnt.c
 * preserves these stat(2) and getmntent(3) buffer values in the local mounts
 * structure.
 *
 * The defined value is the string that names the file system type.
 *
 * The HASPROCFS definition usually must be accompanied by the HASFSTYPE
 * definition and the providing of an fstype element in the local mounts
 * structure (defined in dlsof.h).
 *
 * The HASPROCFS definition may be accompanied by the HASPINODEN definition.
 * HASPINODEN specifies that searching for files in HASPROCFS is to be done
 * by inode number.
 */

/* #define	HASPROCFS	"proc?" */
/* #define	HASFSTYPE	1 */
/* #define	HASPINODEN	1 */


/*
 * HASRNODE is defined for those dialects that have rnodes.
 */

/* #define	HASRNODE	1 */


/*
 * Define HASSECURITY to restrict the listing of all open files to the
 * root user.  When HASSECURITY is defined, the non-root user may list
 * only files whose processes have the same user ID as the real user ID
 * (the one that its user logged on with) of the lsof process.
 */

/* #define	HASSECURITY	1 */


/*
 * If HASSECURITY is defined, define HASNOSOCKSECURITY to allow users
 * restricted by HASSECURITY to list any open socket files, provide their
 * listing is selected by the "-i" option.
 */

/* #define	HASNOSOCKSECURITY	1	*/


/*
 * HASSETLOCALE is defined for those dialects that have <locale.h> and
 * setlocale().
 *
 * If the dialect also has wide character support for language locales,
 * HASWIDECHAR activates lsof's wide character support and WIDECHARINCL
 * defines the header file (if any) that must be #include'd to use the
 * mblen() and mbtowc() functions.
 */

#define	HASSETLOCALE	1
#define	HASWIDECHAR	1
#define	WIDECHARINCL	<wctype.h>


/*
 * HASSNODE is defined for those dialects that have snodes.
 */

/* #define	HASSNODE	1 */


/*
 * HASSOOPT, HASSOSTATE and HASTCPOPT define the availability of information
 * on socket options (SO_* symbols), socket states (SS_* symbols) and TCP
 * options.
 */

/* #define	HASSOOPT	1	has socket option information */
/* #define	HASSOSTATE	1	has socket state information */
/* #define	HASTCPOPT	1	has TCP options or flags */


/*
 * Define HASSPECDEVD to be the name of a function that handles the results
 * of a successful stat(2) of a file name argument.
 *
 * For example, HASSPECDEVD() for Darwin makes sure that st_dev is set to
 * what stat("/dev") returns -- i.e., what's in DevDev.
 *
 * The function takes two arguments:
 *
 *	1: pointer to the full path name of file
 *	2: pointer to the stat(2) result
 *
 * The function returns void.
 */

/* #define	HASSPECDEVD	process_dev_stat */


/*
 * HASSTREAMS is defined for those dialects that support streams.
 */

/* #define	HASSTREAMS	1 */


/*
 * HASTCPTPIQ is defined for dialects where it is possible to report the
 * TCP/TPI Recv-Q and Send-Q values produced by netstat.
 */

#define	HASTCPTPIQ	1


/*
 * HASTCPTPIW is defined for dialects where it is possible to report the
 * TCP/TPI send and receive window sizes produced by netstat.
 */

/* #define	HASTCPTPIW	1 */


/*
 * HASTCPUDPSTATE is defined for dialects that have TCP and UDP state
 * support -- i.e., for the "-stcp|udp:state" option and its associated
 * speed improvements.
 */

#define	HASTCPUDPSTATE	1


/*
 * HASTMPNODE is defined for those dialects that have tmpnodes.
 */

/* #define	HASTMPNODE	1 */


/*
 * HASVNODE is defined for those dialects that use the Sun virtual file system
 * node, the vnode.  BSD derivatives usually do; System V derivatives prior to
 * R4 usually don't.
 * doesn't.
 */

/* #define	HASVNODE	1 */


/*
 * HASXOPT is defined for those dialects that have an X option.  It
 * defines the text for the usage display.  HASXOPT_VALUE defines the
 * option's default binary value -- 0 or 1.
 */

#define	HASXOPT		"skip TCP&UDP* files"
#define	HASXOPT_VALUE	0


/*
 * INODETYPE and INODEPSPEC define the internal node number type and its
 * printf specification modifier.  These need not be defined and lsof.h
 * can be allowed to define defaults.
 *
 * These are defined here, because they must be used in dlsof.h.
 */

#define	INODETYPE	unsigned long long
					/* inode number internal storage type */
#define	INODEPSPEC	"ll"	 	/* INODETYPE printf specification
					 * modifier */


/*
 * UID_ARG defines the size of a User ID number when it is passed
 * as a function argument.
 */

#define	UID_ARG	u_int


/*
 * Each USE_LIB_<function_name> is defined for dialects that use the
 * <function_name> in the lsof library.
 *
 * Note: other definitions and operations may be required to condition the
 * library function source code.  They may be found in the dialect dlsof.h
 * header files.
 */

/* #define	USE_LIB_CKKV			1	   ckkv.c */
/* #define	USE_LIB_COMPLETEVFS		1	   cvfs.c */
/* #define	USE_LIB_FIND_CH_INO		1	   fino.c */
#define	USE_LIB_IS_FILE_NAMED			1	/* isfn.c */
/* #define	USE_LIB_LKUPDEV			1	   lkud.c */
/* #define	USE_LIB_PRINTDEVNAME		1	   pdvn.c */
/* #define	USE_LIB_PROCESS_FILE		1	   prfp.c */
/* #define	USE_LIB_PRINT_TCPTPI		1	   ptti.c */
/* #define	USE_LIB_READDEV			1	   rdev.c */
/* #define	USE_LIB_READMNT			1	   rmnt.c */
/* #define	USE_LIB_REGEX			1	   regex.c */
/* #define	USE_LIB_RNAM			1	   rnam.c */
/* #define	USE_LIB_RNCH			1	   rnch.c */
/* #define	USE_LIB_RNMH			1	   rnmh.c */
/* #define	USE_LIB_SNPF			1	   snpf.c */
#define	snpf	snprintf	   /* use the system's snprintf() */


/*
 * WARNDEVACCESS is defined for those dialects that should issue a warning
 * when lsof can't access /dev (or /device) or one of its sub-directories.
 * The warning can be inhibited by the lsof caller with the -w option.
 *
 * CAUTION!!!  Don't enable the WARNDEVACCESS definiton for /proc-based Linux
 *	       lsof; it doesn't process /dev at all.
 *
 * The presence of NEVER_WARNDEVACCESS in this comment prevents the Customize
 * script from offering to change WARNDEVACCESS.
 */

/* #define	WARNDEVACCESS	1  DON'T ENABLE!!! -- see above comment */


/*
 * WARNINGSTATE is defined for those dialects that want to suppress all lsof
 * warning messages.
 */

/* #define	WARNINGSTATE	1	warnings are enabled by default */


/*
 * WILLDROPGID is defined for those dialects whose lsof executable runs
 * setgid(not_real_GID) and whose setgid power can be relinquished after
 * the dialect's initialize() function has been executed.
 */

/* #define	WILLDROPGID	1 */


/*
 * zeromem is a macro that uses bzero or memset.
 */

#define	zeromem(a, l)	bzero(a, l)

#endif	/* !defined(LSOF_MACHINE_H) */
