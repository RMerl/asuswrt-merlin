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

/*
 * File I/O stubs
 *
 * Linux and other OSs may use open64, lseek64 instead of defaulting off_t to
 * 64-bits like OSX does. This file provides cover functions to always perform
 * 64-bit file I/O.
 */

#ifndef _DKOPEN_H_
#define _DKOPEN_H_

/* Must predefine the large file flags before including sys/types.h */
#if defined (linux)
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#elif defined (__APPLE__)
#else
#error Platform not recognized
#endif

#include <sys/types.h>

/* Typedef off64_t for platforms that don't have it declared */
#if defined (__APPLE__) && !defined (linux)
typedef u_int64_t off64_t;
#endif

int dkopen (const char *path, int flags, int mode);
int dkclose (int filedes);

off64_t dklseek (int fileds, off64_t offset, int whence);

#endif
