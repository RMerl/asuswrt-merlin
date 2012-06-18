/*
 *	osLayer.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Layered header for OS specific functions
 *	Contributors adding new OS support must implement all functions 
 *	externed below.
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#ifndef _h_OS_LAYER
#define _h_OS_LAYER
#define _h_EXPORT_SYMBOLS

#include <stdio.h>
#include <stdlib.h>

#ifndef WINCE
#include <time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

#include "../../matrixCommon.h"
#include "psMalloc.h"

/*
	Functions defined at OS level
*/
extern int32	sslOpenOsdep(void);
extern int32	sslCloseOsdep(void);
extern int32	sslGetEntropy(unsigned char *bytes, int32 size);

/*
	Defines to make library multithreading safe
*/
#ifdef USE_MULTITHREADING

#if WIN32 || WINCE
#include <windows.h>

typedef CRITICAL_SECTION sslMutex_t;
#define sslCreateMutex(M)	InitializeCriticalSection((CRITICAL_SECTION *) M);
#define sslLockMutex(M)		EnterCriticalSection((CRITICAL_SECTION *) M);
#define sslUnlockMutex(M)	LeaveCriticalSection((CRITICAL_SECTION *) M);
#define sslDestroyMutex(M)	DeleteCriticalSection((CRITICAL_SECTION *) M);

#elif LINUX
#include <pthread.h>
#include <string.h>

typedef pthread_mutex_t sslMutex_t;
extern int32	sslCreateMutex(sslMutex_t *mutex);
extern int32	sslLockMutex(sslMutex_t *mutex);
extern int32	sslUnlockMutex(sslMutex_t *mutex);
extern void	sslDestroyMutex(sslMutex_t *mutex);
#elif VXWORKS
#include "semLib.h"

typedef SEM_ID		sslMutex_t; 
extern int32	sslCreateMutex(sslMutex_t *mutex);
extern int32	sslLockMutex(sslMutex_t *mutex);
extern int32	sslUnlockMutex(sslMutex_t *mutex);
extern void	sslDestroyMutex(sslMutex_t *mutex);
#endif /* WIN32 || CE */

#else /* USE_MULTITHREADING */
typedef int32	sslMutex_t;
#define sslCreateMutex(M)
#define sslLockMutex(M)
#define sslUnlockMutex(M)
#define sslDestroyMutex(M)

#endif /* USE_MULTITHREADING */

/*
	Make sslTime_t an opaque time value.
	FUTURE - use high res time instead of time_t
*/
#ifdef LINUX
/*
	On some *NIX versions such as MAC OS X 10.4, CLK_TCK has been deprecated
*/
#ifndef CLK_TCK
#define CLK_TCK		CLOCKS_PER_SEC
#endif /* CLK_TCK */
#endif /* LINUX */

#if defined(WIN32)
#include <windows.h>
typedef LARGE_INTEGER sslTime_t;
#elif VXWORKS
typedef struct {
		long sec;
		long usec;
	} sslTime_t;
#elif (defined(USE_RDTSCLL_TIME) || defined(RDTSC))
typedef unsigned long long LARGE_INTEGER;
typedef LARGE_INTEGER sslTime_t;
#elif WINCE
#include <windows.h>

typedef LARGE_INTEGER sslTime_t;
#else
typedef struct {
		long sec;
		long usec;
	} sslTime_t;
#endif

/******************************************************************************/
/*
	We define our own stat for CE.
*/
#if WINCE

extern int32 stat(char *filename, struct stat *sbuf);

struct stat {
	unsigned long st_size;	/* file size in bytes				*/
	unsigned long st_mode;
	time_t st_atime;		/* time of last access				*/
	time_t st_mtime;		/* time of last data modification	*/
	time_t st_ctime;		/* time of last file status change	*/
};

#define			S_IFREG 0100000
#define			S_IFDIR 0040000

extern time_t time();

#endif /* WINCE */

extern int32		sslInitMsecs(sslTime_t *t);
extern int32		sslCompareTime(sslTime_t a, sslTime_t b);
extern int32		sslDiffSecs(sslTime_t then, sslTime_t now);
extern long		sslDiffMsecs(sslTime_t then, sslTime_t now);


/******************************************************************************/
/*
	Debugging functionality.  
	
	If DEBUG is defined matrixStrDebugMsg and matrixIntDebugMsg messages are
	output to stdout, sslAsserts go to stderror and call psBreak.

	In non-DEBUG builds matrixStrDebugMsg and matrixIntDebugMsg are 
	compiled out.  sslAsserts still go to stderr, but psBreak is not called.

*/

#if DEBUG
extern void psBreak(void);
extern void matrixStrDebugMsg(char *message, char *arg);
extern void matrixIntDebugMsg(char *message, int32 arg);
extern void matrixPtrDebugMsg(char *message, void *arg);
#define sslAssert(C)  if (C) ; else \
	{fprintf(stderr, "%s:%d sslAssert(%s)\n",__FILE__, __LINE__, #C); psBreak(); }
#else
#define matrixStrDebugMsg(x, y)
#define matrixIntDebugMsg(x, y)
#define matrixPtrDebugMsg(x, y)
#define sslAssert(C)
#endif /* DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* _h_OS_LAYER */

/******************************************************************************/
