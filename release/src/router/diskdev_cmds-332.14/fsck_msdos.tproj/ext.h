/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the 'License').  You may not use this file
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
 * Copyright (C) 1995, 1996, 1997 Wolfgang Solfrank
 * Copyright (c) 1995 Martin Husemann
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Martin Husemann
 *	and Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EXT_H
#define EXT_H

#include <sys/types.h>

#include "dosfs.h"

#define	LOSTDIR	"LOST.DIR"

/*
 * Options:
 */
extern int alwaysno;	/* assume "no" for all questions */
extern int alwaysyes;	/* assume "yes" for all questions */
extern int preen;	/* we are preening */
extern int rdonly;	/* device is opened read only (supersedes above) */
extern int quick;	/* set to quickly check if volume is dirty */
extern int quiet;	/* set to suppress most messages */

extern char *fname;	/* filesystem currently checked */

extern struct dosDirEntry *rootDir;

/*
 * function declarations
 */
int ask __P((int, const char *, ...));

/*
 * Check filesystem given as arg
 */
int checkfilesys __P((const char *));

/*
 * Return values of various functions
 */
#define	FSOK		0		/* Check was OK */
#define	FSBOOTMOD	1		/* Boot block was modified */
#define	FSDIRMOD	2		/* Some directory was modified */
#define	FSFATMOD	4		/* The FAT was modified */
#define	FSERROR		8		/* Some unrecovered error remains */
#define	FSFATAL		16		/* Some unrecoverable error occured */
#define FSDIRTY		32		/* File system is dirty */
#define FSFIXFAT	64		/* Fix file system FAT */

/*
 * read a boot block in a machine independend fashion and translate
 * it into our struct bootblock.
 */
int readboot __P((int, struct bootblock *));

/*
 * Correct the FSInfo block.
 */
int writefsinfo __P((int, struct bootblock *));

/*
 * Determine whether a volume is dirty, without reading the entire FAT.
 */
int isdirty(int fs, struct bootblock *boot, int fat);

/*
 * Read one of the FAT copies and return a pointer to the new
 * allocated array holding our description of it.
 */
int readfat __P((int, struct bootblock *, int, struct fatEntry **));

/*
 * Check two FAT copies for consistency and merge changes into the
 * first if neccessary.
 */
int comparefat __P((struct bootblock *, struct fatEntry *, struct fatEntry *, int));

/*
 * Check a FAT
 */
int checkfat __P((struct bootblock *, struct fatEntry *));

/*
 * Write back FAT entries
 */
int writefat __P((int, struct bootblock *, struct fatEntry *, int));

/*
 * Read a directory
 */
int resetDosDirSection __P((struct bootblock *, struct fatEntry *));
void finishDosDirSection __P((void));
int handleDirTree __P((int, struct bootblock *, struct fatEntry *));

/*
 * Cross-check routines run after everything is completely in memory
 */
/*
 * Determine the length of a cluster chain by following its links in the FAT
 */
cl_t chainlength(struct bootblock *boot, struct fatEntry *fat, cl_t head);

/*
 * Check for lost cluster chains
 */
int checklost __P((int, struct bootblock *, struct fatEntry *));

/*
 * Try to reconnect a lost cluster chain
 */
int reconnect __P((int, struct bootblock *, struct fatEntry *, cl_t));
void finishlf __P((void));

/*
 * Small helper functions
 */
/*
 * Return the type of a reserved cluster as text
 */
char *rsrvdcltype __P((cl_t));

/*
 * Clear a cluster chain in a FAT
 */
void clearchain __P((struct bootblock *, struct fatEntry *, cl_t));

#endif
