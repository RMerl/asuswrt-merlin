/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
 */

#ifndef ATALK_DIRECTORY_H
#define ATALK_DIRECTORY_H 1

#include <sys/types.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdint.h>

#include <atalk/cnid.h>
#include <atalk/bstrlib.h>
#include <atalk/queue.h>
#include <atalk/unicode.h>

/* setgid directories */
#ifndef DIRBITS
# ifdef AFS
#  define DIRBITS 0
# else /* AFS */
#  define DIRBITS S_ISGID
# endif /* AFS */
#endif /* DIRBITS */

/* reserved directory id's */
#define DIRDID_ROOT_PARENT    htonl(1)  /* parent directory of root */
#define DIRDID_ROOT           htonl(2)  /* root directory */

/* struct dir.d_flags */
#define DIRF_FSMASK	   (3<<0)
#define DIRF_NOFS	   (0<<0)
#define DIRF_AFS	   (1<<0)
#define DIRF_UFS	   (1<<1)
#define DIRF_ISFILE    (1<<3) /* it's cached file, not a directory */
#define DIRF_OFFCNT    (1<<4) /* offsprings count is valid */
#define DIRF_CNID	   (1<<5) /* renumerate id */

struct dir {
    bstring     d_fullpath;          /* complete unix path to dir (or file) */
    bstring     d_m_name;            /* mac name */
    bstring     d_u_name;            /* unix name                                          */
                                     /* be careful here! if d_m_name == d_u_name, d_u_name */
                                     /* will just point to the same storage as d_m_name !! */
    ucs2_t      *d_m_name_ucs2;       /* mac name as UCS2 */
    qnode_t     *qidx_node;           /* pointer to position in queue index */
    time_t      d_ctime;                /* inode ctime, used and modified by reenumeration */

    int         d_flags;              /* directory flags */
    cnid_t      d_pdid;               /* CNID of parent directory */
    cnid_t      d_did;                /* CNID of directory */
    uint32_t    d_offcnt;             /* offspring count */
    uint16_t    d_vid;                /* only needed in the dircache, because
                                         we put all directories in one cache. */
    uint32_t    d_rights_cache;       /* cached rights combinded from mode and possible ACL */

    /* Stuff used in the dircache */
    time_t      dcache_ctime;         /* inode ctime, used and modified by dircache */
    ino_t       dcache_ino;           /* inode number, used to detect changes in the dircache */
};

struct path {
    int         m_type;             /* mac name type (long name, unicode */
    char        *m_name;            /* mac name */
    char        *u_name;            /* unix name */
    cnid_t      id;                 /* file id (only for getmetadata) */
    struct dir  *d_dir;             /* */
    int         st_valid;           /* does st_errno and st set */
    int         st_errno;
    struct stat st;
};

static inline int path_isadir(struct path *o_path)
{
    return o_path->d_dir != NULL;
#if 0
    return o_path->m_name == '\0' || /* we are in a it */
           !o_path->st_valid ||      /* in cache but we can't chdir in it */ 
           (!o_path->st_errno && S_ISDIR(o_path->st.st_mode)); /* not in cache an can't chdir */
#endif
}

#endif /* ATALK_DIRECTORY_H */
