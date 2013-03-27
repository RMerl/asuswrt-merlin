/*
 * node.c - common node reading functions for lsof
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: node.c,v 1.5 2000/08/01 17:08:05 abe Exp $";
#endif


#include "lsof.h"


/*
 * print_kptr() - print kernel pointer
 */

char *
print_kptr(kp, buf, bufl)
	KA_T kp;			/* kernel pointer address */
	char *buf;			/* optional destination buffer */
	size_t bufl;			/* size of buf[] */
{
	static char dbuf[32];

	(void) snpf(buf ? buf : dbuf,
		    buf ? bufl : sizeof(dbuf),
		    KA_T_FMT_X, kp);
	return(buf ? buf : dbuf);
}


#if	defined(HASCDRNODE)
/*
 * readcdrnode() - read CD-ROM node
 */

int
readcdrnode(ca, c)
	KA_T ca;			/* cdrnode kernel address */
	struct cdrnode *c;		/* cdrnode buffer */
{
	if (kread((KA_T)ca, (char *)c, sizeof(struct cdrnode))) {
	    (void) snpf(Namech, Namechl, "can't read cdrnode at %s",
		print_kptr(ca, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASCDRNODE) */


#if	defined(HASFIFONODE)
/*
 * readfifonode() - read fifonode
 */

int
readfifonode(fa, f)
	KA_T fa;			/* fifonode kernel address */
	struct fifonode *f;		/* fifonode buffer */
{
	if (kread((KA_T)fa, (char *)f, sizeof(struct fifonode))) {
	    (void) snpf(Namech, Namechl, "can't read fifonode at %s",
		print_kptr(fa, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASFIFONODE) */


#if	defined(HASGNODE)
/*
 * readgnode() - read gnode
 */

int
readgnode(ga, g)
	KA_T ga;			/* gnode kernel address */
	struct gnode *g;		/* gnode buffer */
{
	if (kread((KA_T)ga, (char *)g, sizeof(struct gnode))) {
	    (void) snpf(Namech, Namechl, "can't read gnode at %s",
		print_kptr(ga, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASGNODE) */


#if	defined(HASHSNODE)
/*
 * readhsnode() - read High Sierra file system node
 */

int
readhsnode(ha, h)
	KA_T ha;			/* hsnode kernel address */
	struct hsnode *h;		/* hsnode buffer */
{
	if (kread((KA_T)ha, (char *)h, sizeof(struct hsnode))) {
	    (void) snpf(Namech, Namechl, "can't read hsnode at %s",
		print_kptr(ha, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASHSNODE) */


#if	defined(HASINODE)
/*
 * readinode() - read inode
 */

int
readinode(ia, i)
	KA_T ia;			/* inode kernel address */
	struct inode *i;		/* inode buffer */
{
	if (kread((KA_T)ia, (char *)i, sizeof(struct inode))) {
	    (void) snpf(Namech, Namechl, "can't read inode at %s",
		print_kptr(ia, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASINODE) */


#if	defined(HASPIPENODE)
/*
 * readpipenode() - read pipe node
 */

int
readpipenode(pa, p)
	KA_T pa;			/* pipe node kernel address */
	struct pipenode *p;		/* pipe node buffer */
{
	if (kread((KA_T)pa, (char *)p, sizeof(struct pipenode))) {
	    (void) snpf(Namech, Namechl, "can't read pipenode at %s",
		print_kptr(pa, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASPIPENODE) */


#if	defined(HASRNODE)
/*
 * readrnode() - read rnode
 */

int
readrnode(ra, r)
	KA_T ra;			/* rnode kernel space address */
	struct rnode *r;		/* rnode buffer pointer */
{
	if (kread((KA_T)ra, (char *)r, sizeof(struct rnode))) {
	    (void) snpf(Namech, Namechl, "can't read rnode at %s",
		print_kptr(ra, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASRNODE) */


#if	defined(HASSNODE)
/*
 * readsnode() - read snode
 */

int
readsnode(sa, s)
	KA_T sa;			/* snode kernel space address */
	struct snode *s;		/* snode buffer pointer */
{
	if (kread((KA_T)sa, (char *)s, sizeof(struct snode))) {
	    (void) snpf(Namech, Namechl, "can't read snode at %s",
		print_kptr(sa, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASSNODE) */


#if	defined(HASTMPNODE)
/*
 * readtnode() - read tmpnode
 */

int
readtnode(ta, t)
	KA_T ta;			/* tmpnode kernel space address */
	struct tmpnode *t;		/* tmpnode buffer pointer */
{
	if (kread((KA_T)ta, (char *)t, sizeof(struct tmpnode))) {
	    (void) snpf(Namech, Namechl, "can't read tmpnode at %s",
		print_kptr(ta, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASTMPNODE) */


#if	defined(HASVNODE)
/*
 * readvnode() - read vnode
 */

int
readvnode(va, v)
	KA_T va;			/* vnode kernel space address */
	struct vnode *v;		/* vnode buffer pointer */
{
	if (kread((KA_T)va, (char *)v, sizeof(struct vnode))) {
	    (void) snpf(Namech, Namechl, "can't read vnode at %s",
		print_kptr(va, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* defined(HASVNODE) */
