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

#include "dkopen.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int dkopen (const char *path, int flags, int mode)
{
#if defined (linux)
	return (open64 (path, flags, mode));
#elif defined (__APPLE__)
	return (open (path, flags, mode));
#endif
}

int dkclose (int filedes)
{
#if defined (linux)
	return (close (filedes));
#elif defined (__APPLE__)
	return (close (filedes));
#endif
}

off64_t dklseek (int filedes, off64_t offset, int whence)
{
#if defined (linux)
	return (lseek64 (filedes, offset, whence));
#elif defined (__APPLE__)
	return (lseek (filedes, offset, whence));
#endif
}

