/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2002 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.2 (the 'License').  You may not use this file
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
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
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
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MBR_H
#define _MBR_H

#include "part.h"

#ifndef NDOSPART
#define NDOSPART 4
#define DOSPTYP_UNUSED 0 
#define DOSPTYP_EXTEND 5
#define DOSPTYP_EXTENDL 15
#define DOSACTIVE 128
#endif

/* Various constants */
#define MBR_CODE_SIZE 0x1BE
#define MBR_PART_SIZE	0x10
#define MBR_PART_OFF 0x1BE
#define MBR_SIG_OFF 0x1FE
#define MBR_BUF_SIZE 0x200

#define MBR_SIGNATURE 0xAA55

/* MBR type */
typedef struct _mbr_t {
        off_t reloffset; /* the offset of the first extended partition that contains all the rest */
        off_t offset; /* the absolute offset of this partition */
        struct _mbr_t *next; /* pointer to the next MBR in an extended partition chain */
	unsigned char code[MBR_CODE_SIZE];
	prt_t part[NDOSPART];
	unsigned short signature;
        unsigned char buf[MBR_BUF_SIZE];
} mbr_t;

/* Prototypes */
void MBR_print_disk __P((char *));
void MBR_print __P((mbr_t *));
void MBR_print_all __P((mbr_t *));
void MBR_parse __P((disk_t *, off_t, off_t, mbr_t *));
void MBR_make __P((mbr_t *));
void MBR_init __P((disk_t *, mbr_t *));
int MBR_read __P((int, off_t, mbr_t *));
int MBR_write __P((int, mbr_t *));
void MBR_pcopy __P((disk_t *, mbr_t *));
mbr_t * MBR_parse_spec __P((FILE *, disk_t *));
void MBR_dump __P((mbr_t *));
void MBR_dump_all __P((mbr_t *));
mbr_t *MBR_alloc __P((mbr_t *));
void MBR_free __P((mbr_t *));
mbr_t * MBR_read_all __P((disk_t *));
int MBR_write_all __P((disk_t *, mbr_t *));
void MBR_clear __P((mbr_t *));

/* Sanity check */
#include <machine/param.h>
#if (DEV_BSIZE != 512)
#error "DEV_BSIZE != 512, somebody better fix me!"
#endif

#endif /* _MBR_H */

