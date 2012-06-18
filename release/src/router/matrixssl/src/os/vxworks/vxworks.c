/*
 *	vxworks.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	VXWORKS compatibility layer
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

#ifdef VXWORKS
#include <fcntl.h>
#include <errno.h>

#include "../../matrixInternal.h"

#include <sys/times.h>
#include <time.h>

static unsigned	int prevTicks; 		/* Check wrap */
static int			tickspersec;	/* system clock rate */
static sslTime_t	elapsedTime; 	/* Last elapsed time */

/******************************************************************************/
/*
	OS dependent Open.
 */
int sslOpenOsdep(void)
{
	tickspersec = sysClkRateGet();

	psOpenMalloc(MAX_MEMORY_USAGE);
	return 0;
}

/******************************************************************************/
/*
	sslClose
 */
int sslCloseOsdep(void)
{
	psCloseMalloc();
	return 0;
}

/******************************************************************************/
/*
	TODO:  hardware dependent entropy function.  Implement 
	depending on platform.
 */

int sslGetEntropy(unsigned char *bytes, int size)
{
	return 0;
}

/******************************************************************************/
/*
	debug break.
 */
void sslBreak(void)
{
	abort();
}

/******************************************************************************/
/*
	Init a time structure.
 */
int sslInitMsecs(sslTime_t *timePtr)
{
	unsigned int	t, deltat, deltaticks;

/*
 	tickGet returns the number of clock ticks since the system
 	was booted. If it is less than the last time we did this, the
 	clock has wrapped around 0xFFFFFFFF, so compute the delta, otherwise
 	the delta is just the difference between the new ticks and the last
 	ticks. Convert the elapsed ticks to elapsed msecs using rounding.
 */
	if ((t = tickGet()) >= prevTicks) {
		deltaticks = t - prevTicks;
	} else {
		deltaticks = (0xFFFFFFFF - prevTicks) + 1 + t;
	}
	deltat = ((deltaticks * 1000) + (tickspersec / 2)) / tickspersec;

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
}

int sslDiffSecs(sslTime_t then, sslTime_t now)
{
	return (int)(now.sec - then.sec); 
}

/******************************************************************************/
/*
	Time comparison.  1 if 'a' is less than or equal.  0 if 'a' is greater
*/
int	sslCompareTime(sslTime_t a, sslTime_t b)
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

/******************************************************************************/
/*
 	Initialize a mutex structure.
 */

int sslCreateMutex(sslMutex_t *mutex)
{
	memset (mutex,0x0,sizeof(sslMutex_t));

/*
 *	Create and initialize a mutual-exclusion semaphore
 */
	*mutex = semMCreate(SEM_Q_PRIORITY | SEM_DELETE_SAFE | SEM_INVERSION_SAFE);
	return 0;
}

/******************************************************************************/
/*
 	Lock a mutex structure.
 */

int sslLockMutex(sslMutex_t *mutex)
{
	STATUS		stat;
	int			err;

/*
	Currently waits forever until semaphore is released.
*/
	if ((stat = semTake((SEM_ID) mutex, WAIT_FOREVER)) == ERROR) {
		return -1;
	}
	return stat;
}

/******************************************************************************/
/*
 	Unlock a mutex structure.
 */

int sslUnlockMutex(sslMutex_t *mutex)
{
	if (mutex != NULL) {
		if (semGive((SEM_ID) mutex) == ERROR) {
			return -1;
		}
	}
	return 0;
}

/******************************************************************************/
/*
	Destroy mutex.
 */

void sslDestroyMutex(sslMutex_t *mutex)
{
	if (mutex == NULL) {
		return;
	}
	sslLockMutex(mutex);
	semDelete((SEM_ID) mutex);
}

#endif /* VXWORKS */

/******************************************************************************/
