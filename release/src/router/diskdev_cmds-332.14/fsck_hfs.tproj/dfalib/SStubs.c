/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* SStubs.c */


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>

#include "Scavenger.h"

extern char *stat_msg[];
extern char *err_msg[];

/*
 *	This is the straight GMT conversion constant:
 *	00:00:00 January 1, 1970 - 00:00:00 January 1, 1904
 *	(3600 * 24 * ((365 * (1970 - 1904)) + (((1970 - 1904) / 4) + 1)))
 */
#define MAC_GMT_FACTOR		2082844800UL

/*
 * GetTimeUTC - get the GMT Mac OS time (in seconds since 1/1/1904)
 *
 */
UInt32 GetTimeUTC(void)
{
	struct timeval time;
	struct timezone zone;

	(void) gettimeofday(&time, &zone);

	return (time.tv_sec + MAC_GMT_FACTOR);
}

/*
 * GetTimeLocal - get the local Mac OS time (in seconds since 1/1/1904)
 *
 */
UInt32 GetTimeLocal(Boolean forHFS)
{
	struct timeval time;
	struct timezone zone;
	UInt32 localTime;

	(void) gettimeofday(&time, &zone);
	localTime = time.tv_sec + MAC_GMT_FACTOR - (zone.tz_minuteswest * 60);

	if (forHFS && zone.tz_dsttime)
		localTime += 3600;

	return localTime;
}


OSErr FlushVol(ConstStr63Param volName, short vRefNum)
{
	sync();
	
	return (0);
}


OSErr MemError()
{
	return (0);
}

void DebugStr(ConstStr255Param debuggerMsg)
{
}


UInt32 TickCount()
{
	return (0);
}


OSErr GetVolumeFeatures( SGlobPtr GPtr )
{
	GPtr->volumeFeatures = supportsTrashVolumeCacheFeatureMask + supportsHFSPlusVolsFeatureMask;

	return( noErr );
}


Handle NewHandleClear(Size byteCount)
{
	return NewHandle(byteCount);
}

Handle NewHandle(Size byteCount)
{
	Handle h;
	Ptr p = NULL;

	if (byteCount < 0)
		return NULL;
		
	if (!(h = malloc(sizeof(Ptr) + sizeof(Size))))
		return NULL;
		
	if (byteCount)
		if (!(p = calloc(1, byteCount)))
		{
			free(h);
			return NULL;
		}
	
	*h = p;
	
	*((Size *)(h + 1)) = byteCount;	
	
	return h;
}

void DisposeHandle(Handle h)
{
	if (h)
	{
		if (*h)
			free(*h);
		free(h);
	}
}

Size GetHandleSize(Handle h)
{
	return h ? *((Size *)(h + 1)) : 0;
}

void SetHandleSize(Handle h, Size newSize)
{
	Ptr p = NULL;

	if (!h || newSize < 0)
		return;

	if ((p = realloc(*h, newSize)))
	{
		*h = p;
		*((Size *)(h + 1)) = newSize;
	}
}


OSErr PtrAndHand(const void *ptr1, Handle hand2, long size)
{
	Ptr p = NULL;
	Size old_size = 0;

	if (!hand2)
		return -109;
	
	if (!ptr1 || size < 1)
		return 0;
		
	old_size = *((Size *)(hand2 + 1));

	if (!(p = realloc(*hand2, size + old_size)))
		return -108;

	*hand2 = p;
	*((Size *)(hand2 + 1)) = size + old_size;
	
	memcpy(*hand2 + old_size, ptr1, size);
	
	return 0;
}


/* deprecated call, use PrintError instead */
void WriteError( SGlobPtr GPtr, short msgID, UInt32 tarID, UInt64 tarBlock )  
{
	PrintError(GPtr, msgID, 0);

	if (GPtr->logLevel > 0 && !GPtr->guiControl && (tarID | tarBlock) != 0)
		printf("(%ld, %qd)\n", (long)tarID, tarBlock);
}

/* deprecated call, use PrintStatus instead */
void WriteMsg( SGlobPtr GPtr, short messageID, short messageType )
{
	PrintStatus(GPtr, messageID, 0);
}


/*
 * Print a localizable message to stdout
 *
 * output looks something like...
 *
 * (S, "Checking Catalog file", 0)
 */
static void
print_localized(const char *type, const char *message, int vargc, va_list ap)
{
	int i;
	char string[256];
	static char sub[] = {"%@"};

	switch (vargc) {
	case 1:	sprintf(string, message, sub);
		break;
	case 2:	sprintf(string, message, sub, sub);
		break;
	case 3:	sprintf(string, message, sub, sub, sub);
		break;
	default:
		strcpy(string, message);
	}

	fprintf(stdout, "(%s,\"%s\",%d)\n", type, string, vargc);
	for (i = 0; i < vargc; i++)
		fprintf(stdout, "%s\n", (char *)va_arg(ap, char *));
	fflush(stdout);
}


/* Print an eror message to stdout */
void
PrintError(SGlobPtr GPtr, short error, int vargc, ...)
{
	int index;
	va_list ap;

	index = abs(error) - E_FirstError;

	if (GPtr->logLevel > 0 && index >= 0) {
		va_start(ap, vargc);

		if (!GPtr->guiControl) {
			printf("   ");
			vprintf(err_msg[index], ap);
			printf("\n");
		} else {
			print_localized("E", err_msg[index], vargc, ap);
		}

		va_end(ap);
	}
}


/* Print a status message to stdout */
void
PrintStatus(SGlobPtr GPtr, short status, int vargc, ...)
{
	int index;
	va_list ap;

	index = status - M_FirstMessage;

	if (GPtr->logLevel > 1 && index >= 0) {
		va_start(ap, vargc);

		if (!GPtr->guiControl) {
			printf("** ");
			vprintf(stat_msg[index], ap);
			printf("\n");
		} else {
			print_localized("S", stat_msg[index], vargc, ap);
		}

		va_end(ap);
	}
}


