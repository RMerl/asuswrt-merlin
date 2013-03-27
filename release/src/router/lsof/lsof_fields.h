/*
 * lsof_field.h - field ID characters for lsof output that can be parsed
 *		  (selected with -f or -F)
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * $Id: lsof_fields.h,v 1.11 2006/09/15 18:53:21 abe Exp $
 */


#if	!defined(LSOF_FORMAT_H)
#define	LSOF_FORMAT_H	1

/*
 * Codes for output fields:
 *
 *	LSOF_FID_*	ID character
 *	LSOF_FIX_*	ID index
 *	LSOF_FNM_*	name
 *
 * A field is displayed in the form:
 *		<ID_character><data><field_terminator>
 *	
 * Output fields are normally terminated with a NL ('\n'), but the field
 * terminator can be set to NUL with the -0 (zero) option to lsof.
 *
 * Field sets -- process-specific information or information specific
 * to a single file descriptor -- are terminated with NL when the field
 * terminator is NUL.
 */

#define	LSOF_FID_ACCESS		'a'
#define	LSOF_FIX_ACCESS		0
#define	LSOF_FNM_ACCESS		"access: r = read; w = write; u = read/write"

#define	LSOF_FID_CMD		'c'
#define	LSOF_FIX_CMD		1
#define	LSOF_FNM_CMD		"command name"

#define	LSOF_FID_CT		'C'
#define	LSOF_FIX_CT		2
#define	LSOF_FNM_CT		"file struct share count"

#define	LSOF_FID_DEVCH		'd'
#define	LSOF_FIX_DEVCH		3
#define	LSOF_FNM_DEVCH		"device character code"

#define	LSOF_FID_DEVN		'D'
#define	LSOF_FIX_DEVN		4
#define	LSOF_FNM_DEVN		"major/minor device number as 0x<hex>"

#define	LSOF_FID_FD		'f'
#define	LSOF_FIX_FD		5
#define	LSOF_FNM_FD		"file descriptor"

#define	LSOF_FID_FA		'F'
#define	LSOF_FIX_FA		6
#define	LSOF_FNM_FA		"file struct address as 0x<hex>"

#define	LSOF_FID_FG		'G'
#define	LSOF_FIX_FG		7
#define	LSOF_FNM_FG		"file flaGs"

#define	LSOF_FID_INODE		'i'
#define	LSOF_FIX_INODE		8
#define	LSOF_FNM_INODE		"inode number"

#define	LSOF_FID_NLINK		'k'
#define	LSOF_FIX_NLINK		9
#define	LSOF_FNM_NLINK		"link count"

#define	LSOF_FID_LOCK		'l'
#define	LSOF_FIX_LOCK		10
#define	LSOF_FNM_LOCK		"lock: r/R = read; w/W = write; u = read/write"

#define	LSOF_FID_LOGIN		'L'
#define	LSOF_FIX_LOGIN		11
#define	LSOF_FNM_LOGIN		"login name"

#define	LSOF_FID_MARK		'm'
#define	LSOF_FIX_MARK		12
#define	LSOF_FNM_MARK		"marker between repeated output"

#define	LSOF_FID_NAME		'n'
#define	LSOF_FIX_NAME		13
#define	LSOF_FNM_NAME		"comment, name, Internet addresses"

#define	LSOF_FID_NI		'N'
#define	LSOF_FIX_NI		14
#define	LSOF_FNM_NI		"file struct node ID as 0x<hex>"

#define	LSOF_FID_OFFSET		'o'
#define	LSOF_FIX_OFFSET		15
#define	LSOF_FNM_OFFSET		"file offset as 0t<dec> or 0x<hex>"

#define	LSOF_FID_PID		'p'
#define	LSOF_FIX_PID		16
#define	LSOF_FNM_PID		"process ID (PID)"

#define	LSOF_FID_PGID		'g'
#define	LSOF_FIX_PGID		17
#define	LSOF_FNM_PGID		"process group ID (PGID)"

#define	LSOF_FID_PROTO		'P'
#define	LSOF_FIX_PROTO		18
#define	LSOF_FNM_PROTO		"protocol name"

#define	LSOF_FID_RDEV		'r'
#define	LSOF_FIX_RDEV		19
#define	LSOF_FNM_RDEV		"raw device number as 0x<hex>"

#define	LSOF_FID_PPID		'R'
#define	LSOF_FIX_PPID		20
#define	LSOF_FNM_PPID		"paRent PID"

#define	LSOF_FID_SIZE		's'
#define	LSOF_FIX_SIZE		21
#define	LSOF_FNM_SIZE		"file size"

#define	LSOF_FID_STREAM		'S'
#define	LSOF_FIX_STREAM		22
#define	LSOF_FNM_STREAM		"stream module and device names"

#define	LSOF_FID_TYPE		't'
#define	LSOF_FIX_TYPE		23
#define	LSOF_FNM_TYPE		"file type"

#define	LSOF_FID_TCPTPI		'T'
#define	LSOF_FIX_TCPTPI		24
#define	LSOF_FNM_TCPTPI		"TCP/TPI info"

#define	LSOF_FID_UID		'u'
#define	LSOF_FIX_UID		25
#define	LSOF_FNM_UID		"user ID (UID)"

#define	LSOF_FID_ZONE		'z'
#define	LSOF_FIX_ZONE		26
#define	LSOF_FNM_ZONE		"zone name"

#define	LSOF_FID_CNTX		'Z'
#define	LSOF_FIX_CNTX		27
#define	LSOF_FNM_CNTX		"security context"

#define	LSOF_FID_TERM		'0'
#define	LSOF_FIX_TERM		28
#define	LSOF_FNM_TERM		"(zero) use NUL field terminator instead of NL"

#endif	/* !defined(LSOF_FORMAT_H) */
