/*
 * dynix.h
 * 
 * Date Created: Sat Jan 12 10:50:50 BST 2002
 * Author:       Patrick Hess  <phess@hds.com>
 */

#include <net-snmp/system/generic.h>

/*
 * from s/sysv.h 
 */
#define SYSV 1

/*
 * to make these changes work... 
 */
/*
 * maybe I should have used _SEQUENT_ in all this code..  dunno 
 */
#define dynix dynix

#undef TOTAL_MEMORY_SYMBOL
#undef MBSTAT_SYMBOL

/*
 * Not enough alcohool in bloodstream [fmc] 
 */
#ifdef NPROC_SYMBOL
#undef NPROC_SYMBOL
#endif
/*
 * there might be a way to get NPROC...  this might work..  might not 
 */
/*
 * #define NPROC_SYMBOL "procNPROC" 
 */
#ifdef PROC_SYMBOL
#undef PROC_SYMBOL
#endif

/*
 * These definitions date from early BSD-based headers,
 *   and are included in modern NetBSD and OpenBSD distributions.
 * As such, the relevant copyright probably resides with UCB.
 */
#ifndef TCPTV_MIN
#define TCPTV_MIN       (1*PR_SLOWHZ)   /* minimum allowable value */
#endif
#ifndef TCPTV_REXMTMAX
#define TCPTV_REXMTMAX  (64*PR_SLOWHZ)  /* max allowable REXMT value */
#endif

/*
 * some of the system headers wanna include asm code...  let's not 
 */
#define __NO_ASM_MACRO 1

/*
 * Dynix doesn't seem to set this.  Guess I'll set it here 
 */
#ifndef L_SET
#define L_SET   SEEK_SET
#endif


/*
 * configure fails to detect these properly 
 */
/*
 * lives in libnsl.so 
 */
#define HAVE_GETHOSTNAME 1

/*
 * outta place...  lives in /usr/include/sys 
 */
#define  HAVE_NET_IF_DL_H 1

/*
 * got this library...  dunno why configure didn't find it 
 */
#define HAVE_LIBNSL 1

/*
 * My Dynix box has nearly 400 filesystems and well over 50 disks 
 */
/*
 * #define MAXDISKS 500  
 */

/*
 * lives in libsocket.so 
 */
#define HAVE_GETHOSTBYNAME 1

/*
 * Might as well include this here, since a significant
 * number of files seem to need it.  DTS 
 */
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/* define the extra mib modules that are supported */
#define NETSNMP_INCLUDE_HOST_RESOURCES
