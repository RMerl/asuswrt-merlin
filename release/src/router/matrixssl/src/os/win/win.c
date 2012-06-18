/*
 *	win.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Microsoft Windows compatibility layer.
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

#include "../osLayer.h"

#include <windows.h>
#include <wincrypt.h>

#ifndef WINCE
#include <process.h>
#endif

#include <limits.h>

#if defined(WIN32) || defined(WINCE)

#define	MAX_INT			0x7FFFFFFF

static LARGE_INTEGER	hiresStart; /* zero-time */
static LARGE_INTEGER	hiresFreq; /* tics per second */

/* For backwards compatibility */
#ifndef CRYPT_SILENT
#define CRYPT_SILENT 0
#endif

static HCRYPTPROV		hProv;	/* Crypto context for random bytes */

int32 sslOpenOsdep()
{
	int32		rc;

	if ((rc = psOpenMalloc(MAX_MEMORY_USAGE)) < 0) {
		return rc;
	}
/*
	Hires time init
*/
	QueryPerformanceFrequency(&hiresFreq);
	QueryPerformanceCounter(&hiresStart);

	if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, 
			CRYPT_VERIFYCONTEXT))  {
		return -1;
	}
	return 0;
}

int32 sslCloseOsdep()
{
	CryptReleaseContext(hProv, 0);
	psCloseMalloc();
	return 0;
}

int32 sslGetEntropy(unsigned char *bytes, int32 size)
{
	if (CryptGenRandom(hProv, size, bytes)) {
		return size;
	}
	return -1;
}

#ifdef DEBUG
void psBreak()
{
	int32	i = 0; i++;	/* Prevent the compiler optimizing this function away */

	DebugBreak();
}
#endif

int32 sslInitMsecs(sslTime_t *t)
{
	__int64		diff;
	int32			d;

	QueryPerformanceCounter(t);
	diff = t->QuadPart - hiresStart.QuadPart;
	d = (int32)((diff * 1000) / hiresFreq.QuadPart);
	return d;
}

int32 sslDiffSecs(sslTime_t then, sslTime_t now)
{
	__int64	diff;

	diff = now.QuadPart - then.QuadPart;
	return (int32)(diff / hiresFreq.QuadPart);
}

/*
	Time comparison.  1 if 'a' is less than or equal.  0 if 'a' is greater
*/
int32 sslCompareTime(sslTime_t a, sslTime_t b)
{
	if (a.QuadPart <= b.QuadPart) {
		return 1;
	}
	return 0;
}

#ifdef WINCE
/******************************************************************************/
/*
 	Our implementation of wide character stat: get file information.
 
 	NOTES:
 		only gets the file size currently
 */
int32 stat(char *filename, struct stat *sbuf)
{
	DWORD	dwAttributes;
	HANDLE	hFile;
	int32		rc, size;

	unsigned short uniFile[512];

	rc = 0;
	memset(sbuf, 0, sizeof(struct stat));

	MultiByteToWideChar(CP_ACP, 0, filename, -1, uniFile, 256);
	dwAttributes = GetFileAttributes(uniFile);
	if (dwAttributes != 0xFFFFFFFF && dwAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		sbuf->st_mode = S_IFDIR;
		return 0;
	}
	sbuf->st_mode = S_IFREG;

	if ((hFile = CreateFile(uniFile, GENERIC_READ, FILE_SHARE_READ && FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		return -1;
	}
	
/*
 *	 Get the file size.
 */
	size = GetFileSize(hFile, NULL);
	sbuf->st_size = size;
	
	CloseHandle(hFile);
	return rc;
}

/******************************************************************************/
/*
 	The following functions implement a unixlike time() function.
 */

static FILETIME YearToFileTime(WORD wYear)
{	
	SYSTEMTIME sbase;
	FILETIME fbase;

	sbase.wYear         = wYear;
	sbase.wMonth        = 1;
	sbase.wDayOfWeek    = 1; //assumed
	sbase.wDay          = 1;
	sbase.wHour         = 0;
	sbase.wMinute       = 0;
	sbase.wSecond       = 0;
	sbase.wMilliseconds = 0;

	SystemTimeToFileTime( &sbase, &fbase );

	return fbase;
}

time_t time() {

	__int64 time1, time2, iTimeDiff;
	FILETIME fileTime1, fileTime2;
	SYSTEMTIME  sysTime;

/*
	Get 1970's filetime.
*/
	fileTime1 = YearToFileTime(1970);

/*
	Get the current filetime time.
*/
	GetSystemTime(&sysTime);
	SystemTimeToFileTime(&sysTime, &fileTime2);


/* 
	Stuff the 2 FILETIMEs into their own __int64s.
*/	
	time1 = fileTime1.dwHighDateTime;
	time1 <<= 32;				
	time1 |= fileTime1.dwLowDateTime;

	time2 = fileTime2.dwHighDateTime;
	time2 <<= 32;				
	time2 |= fileTime2.dwLowDateTime;

/*
	Get the difference of the two64-bit ints.

	This is he number of 100-nanosecond intervals since Jan. 1970.  So
	we divide by 10000 to get seconds.
 */
	iTimeDiff = (time2 - time1) / 10000000;
	return (int32)iTimeDiff;
}
#endif /* WINCE */



#endif /* WIN32 */

/******************************************************************************/
