/*
 *	linux.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Linux compatibility layer
 *	Other UNIX like operating systems should also be able to use this
 *	implementation without change.
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
#ifdef LINUX
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/times.h>
#include <time.h>

#include "../osLayer.h"

#if defined(USE_RDTSCLL_TIME) || defined(RDTSC)
#include <asm/timex.h>
/*
	As defined in asm/timex.h for x386:
*/
#ifndef rdtscll
	#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

static sslTime_t	hiresStart; 	/* zero-time */
static sslTime_t	hiresFreq; 		/* tics per second */
#else /* USE_RDTSCLL_TIME */
static uint32		prevTicks; 		/* Check wrap */
static sslTime_t	elapsedTime; 	/* Last elapsed time */
#endif

#ifdef USE_MULTITHREADING
#include <pthread.h>
static pthread_mutexattr_t	attr;
#endif

/* max sure we don't retry reads forever */
#define	MAX_RAND_READS		1024

static 	int32	urandfd = -1;
static 	int32	randfd	= -1;

int32 sslOpenOsdep(void)
{
#if defined(USE_RDTSCLL_TIME) || defined(RDTSC)
	FILE		*cpuInfo;
	double		mhz;
	char		line[80] = "";
	char		*tmpstr;
	int32 		c;
#endif 
/*
	Open /dev/random access non-blocking.
*/
	if ((randfd = open("/dev/random", O_RDONLY | O_NONBLOCK)) < 0) {
		return -1;
	}
	if ((urandfd = open("/dev/urandom", O_RDONLY)) < 0) {
		close(randfd);
		return -1;
	}
/*
	Initialize times
*/
#if defined(USE_RDTSCLL_TIME) || defined(RDTSC)
	if ((cpuInfo = fopen ("/proc/cpuinfo","r")) == NULL) {
		matrixStrDebugMsg("Error opening /proc/cpuinfo\n", NULL);
		return -2;
	}

	while ((!feof(cpuInfo)) && (strncasecmp(line,"cpu MHz",7) != 0)){
		fgets(line,79,cpuInfo);
	}

	if (strncasecmp(line,"cpu MHz",7) == 0){ 
		tmpstr = strchr(line,':');
		tmpstr++;
		c = strspn(tmpstr, " \t");
		tmpstr +=c;
		c = strcspn(tmpstr, " \t\n\r");
		tmpstr[c] = '\0';
		mhz = 1000000 * atof(tmpstr);
		hiresFreq = (sslTime_t)mhz;
		fclose (cpuInfo);	
	} else {
		fclose (cpuInfo);
		hiresStart = 0;
		return -3;
	}
	rdtscll(hiresStart);
#endif /* USE_RDTSCLL_TIME */
/*
	FUTURE - Solaris doesn't support recursive mutexes!
	We don't use them internally anyway, so this is not an issue,
	but we like to set this if we can because it's silly for a thread to lock
	itself, rather than error or recursive lock
*/
#ifdef USE_MULTITHREADING
	pthread_mutexattr_init(&attr);
#ifndef OSX
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
#endif /* !OSX */
#endif /* USE_MULTITHREADING */
	return psOpenMalloc(MAX_MEMORY_USAGE);
}

int32 sslCloseOsdep(void)
{
	psCloseMalloc();
#ifdef USE_MULTITHREADING
	pthread_mutexattr_destroy(&attr);
#endif
	close(randfd);
	close(urandfd);
	return 0;
}

/*
	Read from /dev/random non-blocking first, then from urandom if it would 
	block.  Also, handle file closure case and re-open.
*/

int32 sslGetEntropy(unsigned char *bytes, int32 size)
{
	int32				rc, sanity, retry, readBytes;
	unsigned char 	*where = bytes;

	sanity = retry = rc = readBytes = 0;

	while (size) {
		if ((rc = read(randfd, where, size)) < 0 || sanity > MAX_RAND_READS) {
			if (errno == EINTR) {
				if (sanity > MAX_RAND_READS) {
					return -1;
				}
				sanity++;
				continue;
			} else if (errno == EAGAIN) {
				break;
			} else if (errno == EBADF && retry == 0) {
				close(randfd);
				if ((randfd = open("/dev/random", O_RDONLY | O_NONBLOCK)) < 0) {
					break;
				}
				retry++;
				continue;
			} else {
				break;
			}
		}
		readBytes += rc;
		where += rc;
		size -= rc;
	}


	sanity = retry = 0;	
	while (size) {
		if ((rc = read(urandfd, where, size)) < 0 || sanity > MAX_RAND_READS) {
			if (errno == EINTR) {
				if (sanity > MAX_RAND_READS) {
					return -1;
				}
				sanity++;
				continue;
			} else if (errno == EBADF && retry == 0) {
				close(urandfd);
				if ((urandfd = 
					open("/dev/urandom", O_RDONLY | O_NONBLOCK)) < 0) {
					return -1;
				}
				retry++;
				continue;
			} else {
				return -1;
			}
		}
		readBytes += rc;
		where += rc;
		size -= rc;
	}
	return readBytes;
}

#ifdef DEBUG
void psBreak(void)
{
	abort();
}
#endif

/******************************************************************************/

#ifdef USE_MULTITHREADING

int32 sslCreateMutex(sslMutex_t *mutex)
{

	if (pthread_mutex_init(mutex, &attr) != 0) {
		return -1;
	}	
	return 0;
}

int32 sslLockMutex(sslMutex_t *mutex)
{
	if (pthread_mutex_lock(mutex) != 0) {
		return -1;
	}
	return 0;
}

int32 sslUnlockMutex(sslMutex_t *mutex)
{
	if (pthread_mutex_unlock(mutex) != 0) {
		return -1;
	}
	return 0;
}

void sslDestroyMutex(sslMutex_t *mutex)
{
	pthread_mutex_destroy(mutex);
}
#endif /* USE_MULTITHREADING */

/*****************************************************************************/
/*
	Use a platform specific high resolution timer
*/
#if defined(USE_RDTSCLL_TIME) || defined(RDTSC)

int32 sslInitMsecs(sslTime_t *t)
{
	unsigned long long	diff;
	int32					d;

	rdtscll(*t);
	diff = *t - hiresStart;
	d = (int32)((diff * 1000) / hiresFreq);
	return d;
}

/*
	Return the delta in seconds between two time values
*/
long sslDiffMsecs(sslTime_t then, sslTime_t now)
{
	unsigned long long	diff;

	diff = now - then;
	return (long)((diff * 1000) / hiresFreq);
}

/*
	Return the delta in seconds between two time values
*/
int32 sslDiffSecs(sslTime_t then, sslTime_t now)
{
	unsigned long long	diff;

	diff = now - then;
	return (int32)(diff / hiresFreq);
}

/*
	Time comparison.  1 if 'a' is less than or equal.  0 if 'a' is greater
*/
int32	sslCompareTime(sslTime_t a, sslTime_t b)
{
	if (a <= b) {
		return 1;
	}
	return 0;
}

#else /* USE_RDTSCLL_TIME */

int32 sslInitMsecs(sslTime_t *timePtr)
{
	struct tms		tbuff;
	uint32	t, deltat, deltaticks;
                                                                                
/*
 *	times() returns the number of clock ticks since the system
 *	was booted.  If it is less than the last time we did this, the
 *	clock has wrapped around 0xFFFFFFFF, so compute the delta, otherwise
 *	the delta is just the difference between the new ticks and the last
 *	ticks.  Convert the elapsed ticks to elapsed msecs using rounding.
 */
	if ((t = times(&tbuff)) >= prevTicks) {
		deltaticks = t - prevTicks;
	} else {
		deltaticks = (0xFFFFFFFF - prevTicks) + 1 + t;
	}
	deltat = ((deltaticks * 1000) + (CLK_TCK / 2)) / CLK_TCK;
                                                                     
/*
 *	Add the delta to the previous elapsed time.
 */
	elapsedTime.usec += ((deltat % 1000) * 1000);
	if (elapsedTime.usec >= 1000000) {
		elapsedTime.usec -= 1000000;
		deltat += 1000;
	}
	elapsedTime.sec += (deltat / 1000);
	prevTicks = t;
                                                                                
/*
 *	Return the current elapsed time.
 */
	timePtr->usec = elapsedTime.usec;
	timePtr->sec = elapsedTime.sec;
	return (timePtr->usec / 1000) + timePtr->sec * 1000;
}

/*
	Return the delta in seconds between two time values
*/
long sslDiffMsecs(sslTime_t then, sslTime_t now)
{
	return (long)((now.sec - then.sec) * 1000);
}

/*
	Return the delta in seconds between two time values
*/
int32 sslDiffSecs(sslTime_t then, sslTime_t now)
{
	return (int32)(now.sec - then.sec);
}

/*
	Time comparison.  1 if 'a' is less than or equal.  0 if 'a' is greater
*/
int32	sslCompareTime(sslTime_t a, sslTime_t b)
{
	if (a.sec < b.sec) {
		return 1;
	} else if (a.sec == b.sec) {
		if (a.usec <= b.usec) {
			return 1;
		} else {
			return 0;
		}
	}	
	return 0;
}

#endif /* USE_RDTSCLL_TIME */


#endif /* LINUX */

/******************************************************************************/
