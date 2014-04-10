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
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fsck.h	8.4 (Berkeley) 5/9/95
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define	MAXDUP		10	/* limit on dup blks (per inode) */
#define	MAXBAD		10	/* limit on bad blks (per inode) */
#define	MAXBUFSPACE	40*1024	/* maximum space to allocate to buffers */
#define	INOBUFSIZE	56*1024	/* size of buffer to read inodes in pass1 */

#ifndef BUFSIZ
#define BUFSIZ 1024
#endif

#define	USTATE	01		/* inode not allocated */
#define	FSTATE	02		/* inode is file */
#define	DSTATE	03		/* inode is directory */
#define	DFOUND	04		/* directory found during descent */
#define	DCLEAR	05		/* directory is to be cleared */
#define	FCLEAR	06		/* file is to be cleared */

/*
 * buffer cache structure.
 */
struct bufarea {
	struct bufarea *b_next;		/* free list queue */
	struct bufarea *b_prev;		/* free list queue */
	ufs_daddr_t b_bno;
	int b_size;
	int b_errs;
	int b_flags;
	union {
		char *b_buf;			/* buffer space */
		ufs_daddr_t *b_indir;		/* indirect block */
		struct fs *b_fs;		/* super block */
		struct cg *b_cg;		/* cylinder group */
		struct dinode *b_dinode;	/* inode block */
	} b_un;
	char b_dirty;
};

#define	B_INUSE 1

#define	MINBUFS		5	/* minimum number of buffers required */
extern struct bufarea bufhead;		/* head of list of other blks in filesys */
extern struct bufarea sblk;		/* file system superblock */
extern struct bufarea cgblk;		/* cylinder group blocks */
extern struct bufarea *pdirbp;		/* current directory contents */
extern struct bufarea *pbp;		/* current inode block */

#define	dirty(bp)	(bp)->b_dirty = 1
#define	initbarea(bp) \
	(bp)->b_dirty = 0; \
	(bp)->b_bno = (ufs_daddr_t)-1; \
	(bp)->b_flags = 0;

#define	sbdirty()	sblk.b_dirty = 1
#define	cgdirty()	cgblk.b_dirty = 1
#define	sblock		(*sblk.b_un.b_fs)
#define	cgrp		(*cgblk.b_un.b_cg)

enum fixstate {DONTKNOW, NOFIX, FIX, IGNORE};

struct inodesc {
	enum fixstate id_fix;	/* policy on fixing errors */
	int (*id_func)();	/* function to be applied to blocks of inode */
	ino_t id_number;	/* inode number described */
	ino_t id_parent;	/* for DATA nodes, their parent */
	ufs_daddr_t id_blkno;	/* current block number being examined */
	int id_numfrags;	/* number of frags contained in block */
	quad_t id_filesize;	/* for DATA nodes, the size of the directory */
	int id_loc;		/* for DATA nodes, current location in dir */
	int id_entryno;		/* for DATA nodes, current entry number */
	struct direct *id_dirp;	/* for DATA nodes, ptr to current entry */
	char *id_name;		/* for DATA nodes, name to find or enter */
	char id_type;		/* type of descriptor, DATA or ADDR */
};
/* file types */
#define	DATA	1
#define	ADDR	2

/*
 * Linked list of duplicate blocks.
 * 
 * The list is composed of two parts. The first part of the
 * list (from duplist through the node pointed to by muldup)
 * contains a single copy of each duplicate block that has been 
 * found. The second part of the list (from muldup to the end)
 * contains duplicate blocks that have been found more than once.
 * To check if a block has been found as a duplicate it is only
 * necessary to search from duplist through muldup. To find the 
 * total number of times that a block has been found as a duplicate
 * the entire list must be searched for occurences of the block
 * in question. The following diagram shows a sample list where
 * w (found twice), x (found once), y (found three times), and z
 * (found once) are duplicate block numbers:
 *
 *    w -> y -> x -> z -> y -> w -> y
 *    ^		     ^
 *    |		     |
 * duplist	  muldup
 */
struct dups {
	struct dups *next;
	ufs_daddr_t dup;
};
extern struct dups *duplist;		/* head of dup list */
extern struct dups *muldup;		/* end of unique duplicate dup block numbers */

/*
 * Linked list of inodes with zero link counts.
 */
struct zlncnt {
	struct zlncnt *next;
	ino_t zlncnt;
};
extern struct zlncnt *zlnhead;		/* head of zero link count list */

/*
 * Inode cache data structures.
 */
struct inoinfo {
	struct	inoinfo *i_nexthash;	/* next entry in hash chain */
	ino_t	i_number;		/* inode number of this entry */
	ino_t	i_parent;		/* inode number of parent */
	ino_t	i_dotdot;		/* inode number of `..' */
	size_t	i_isize;		/* size of inode */
	u_int	i_numblks;		/* size of block array in bytes */
	ufs_daddr_t i_blks[1];		/* actually longer */
} ;
extern struct inoinfo **inphead, **inpsort;
extern long numdirs, listmax, inplast;

extern char	*cdevname;		/* name of device being checked */
extern long	dev_bsize;		/* computed value of DEV_BSIZE */
extern long	secsize;		/* actual disk sector size */
extern char	nflag;			/* assume a no response */
extern char	yflag;			/* assume a yes response */
extern int	bflag;			/* location of alternate super block */
#ifdef __APPLE__
extern int qflag;			/* quick check return zero if clean, nonzero if dirty */
#endif /* __APPLE__ */
extern int	debug;			/* output debugging info */
extern int	cvtlevel;		/* convert to newer file system format */
extern int	doinglevel1;		/* converting to new cylinder group format */
extern int	doinglevel2;		/* converting to new inode format */
extern int	newinofmt;		/* filesystem has new inode format */
extern char	preen;			/* just fix normal inconsistencies */
extern char	hotroot;		/* checking root device */
extern char	havesb;			/* superblock has been read */
extern int	fsmodified;		/* 1 => write done to file system */
extern int	fsreadfd;		/* file descriptor for reading file system */
extern int	fswritefd;		/* file descriptor for writing file system */

extern ufs_daddr_t maxfsblock;		/* number of blocks in the file system */
extern char	*blockmap;		/* ptr to primary blk allocation map */
extern ino_t	maxino;			/* number of inodes in file system */
extern ino_t	lastino;		/* last inode in use */
extern char	*statemap;		/* ptr to inode state table */
extern u_char	*typemap;		/* ptr to inode type table */
extern short	*lncntp;		/* ptr to link count table */

extern ino_t	lfdir;			/* lost & found directory inode number */
extern char	*lfname;		/* lost & found directory name */
extern int	lfmode;			/* lost & found directory creation mode */

extern ufs_daddr_t n_blks;		/* number of blocks in use */
extern ufs_daddr_t n_files;		/* number of files in use */

#define	clearinode(dp)	(*(dp) = zino)
extern struct	dinode zino;

#define	setbmap(blkno)	setbit(blockmap, blkno)
#define	testbmap(blkno)	isset(blockmap, blkno)
#define	clrbmap(blkno)	clrbit(blockmap, blkno)

#define	STOP	0x01
#define	SKIP	0x02
#define	KEEPON	0x04
#define	ALTERED	0x08
#define	FOUND	0x10

#define	EEXIT	8		/* Standard error exit. */

#ifdef __APPLE__			/* define qflag exit codes */
#define CLEANEXIT  0		/* Filesystem Clean, no checks */
#define DIRTYEXIT  3		/* Filesystem Dirty, no checks */
#endif /* __APPLE__ */

struct fstab;

void		adjust __P((struct inodesc *, int lcnt));
ufs_daddr_t	allocblk __P((long frags));
ino_t		allocdir __P((ino_t parent, ino_t request, int mode));
ino_t		allocino __P((ino_t request, int type));
void		blkerror __P((ino_t ino, char *type, ufs_daddr_t blk));
char	       *blockcheck __P((char *name));
int		bread __P((int fd, char *buf, ufs_daddr_t blk, long size));
void		bufinit __P((void));
void		bwrite __P((int fd, char *buf, ufs_daddr_t blk, long size));
void		cacheino __P((struct dinode *dp, ino_t inumber));
void		catch __P((int));
void		catchquit __P((int));
int		changeino __P((ino_t dir, char *name, ino_t newnum));
int		checkfstab __P((int preen, int maxrun,
			int (*docheck)(struct fstab *),
			int (*chkit)(char *, char *, long, int)));
int		chkrange __P((ufs_daddr_t blk, int cnt));
void		ckfini __P((int markclean));
int		ckinode __P((struct dinode *dp, struct inodesc *));
void		clri __P((struct inodesc *, char *type, int flag));
void		direrror __P((ino_t ino, char *errmesg));
int		dirscan __P((struct inodesc *));
int		dofix __P((struct inodesc *, char *msg));
extern void		ffs_clrblock __P((struct fs *, u_char *, ufs_daddr_t));
extern void		ffs_fragacct __P((struct fs *, int, int32_t [], int));
extern int		ffs_isblock __P((struct fs *, u_char *, ufs_daddr_t));
extern void		ffs_setblock __P((struct fs *, u_char *, ufs_daddr_t));
extern void		fileerror __P((ino_t cwd, ino_t ino, char *errmesg));
extern int		findino __P((struct inodesc *));
int		findname __P((struct inodesc *));
void		flush __P((int fd, struct bufarea *bp));
void		freeblk __P((ufs_daddr_t blkno, long frags));
void		freeino __P((ino_t ino));
void		freeinodebuf __P((void));
int		ftypeok __P((struct dinode *dp));
void		getblk __P((struct bufarea *bp, ufs_daddr_t blk, long size));
struct bufarea *getdatablk __P((ufs_daddr_t blkno, long size));
struct inoinfo *getinoinfo __P((ino_t inumber));
struct dinode  *getnextinode __P((ino_t inumber));
void		getpathname __P((char *namebuf, ino_t curdir, ino_t ino));
struct dinode  *ginode __P((ino_t inumber));
void		inocleanup __P((void));
void		inodirty __P((void));
int		linkup __P((ino_t orphan, ino_t parentdir));
int		makeentry __P((ino_t parent, ino_t ino, char *name));
void		panic __P((const char *fmt, ...));
void		pass1 __P((void));
void		pass1b __P((void));
int		pass1check __P((struct inodesc *));
void		pass2 __P((void));
void		pass3 __P((void));
void		pass4 __P((void));
int		pass4check __P((struct inodesc *));
void		pass5 __P((void));
void		pfatal __P((const char *fmt, ...));
void		pinode __P((ino_t ino));
void		propagate __P((void));
void		pwarn __P((const char *fmt, ...));
int		reply __P((char *question));
void		resetinodebuf __P((void));
int		setup __P((char *dev));
void		voidquit __P((int));
