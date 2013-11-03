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
 *  Research Systems Unix Group
 *  The University of Michigan
 *  c/o Mike Clark
 *  535 W. William Street
 *  Ann Arbor, Michigan
 *  +1-313-763-0525
 *  netatalk@itd.umich.edu
 */

/*!
 * @file
 * @brief Part of Netatalk's AppleDouble implementatation
 */

#ifndef _ATALK_ADOUBLE_H
#define _ATALK_ADOUBLE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <atalk/standards.h>

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/mman.h>

#include <sys/time.h>

#include <atalk/bstrlib.h>

/* version info */
#define AD_VERSION2     0x00020000
#define AD_VERSION_EA   0x00020002

/* default */
#define AD_VERSION      AD_VERSION_EA

/*
 * AppleDouble entry IDs.
 */
#define ADEID_DFORK         1
#define ADEID_RFORK         2
#define ADEID_NAME          3
#define ADEID_COMMENT       4
#define ADEID_ICONBW        5
#define ADEID_ICONCOL       6
#define ADEID_FILEI         7  /* v1, replaced by: */
#define ADEID_FILEDATESI    8  /* this */
#define ADEID_FINDERI       9
#define ADEID_MACFILEI      10 /* we don't use this */
#define ADEID_PRODOSFILEI   11 /* we store prodos info here */
#define ADEID_MSDOSFILEI    12 /* we don't use this */
#define ADEID_SHORTNAME     13
#define ADEID_AFPFILEI      14 /* where the rest of the FILEI info goes */
#define ADEID_DID           15

/* netatalk private note fileid reused DID */
#define ADEID_PRIVDEV       16
#define ADEID_PRIVINO       17
#define ADEID_PRIVSYN       18 /* in synch with database */
#define ADEID_PRIVID        19
#define ADEID_MAX           (ADEID_PRIVID + 1)

/* These are the real ids for these entries, as stored in the adouble file */
#define AD_DEV              0x80444556
#define AD_INO              0x80494E4F
#define AD_SYN              0x8053594E
#define AD_ID               0x8053567E

/* magic */
#define AD_APPLESINGLE_MAGIC 0x00051600
#define AD_APPLEDOUBLE_MAGIC 0x00051607
#define AD_MAGIC             AD_APPLEDOUBLE_MAGIC

/* sizes of relevant entry bits */
#define ADEDLEN_MAGIC       4
#define ADEDLEN_VERSION     4
#define ADEDLEN_FILLER      16
#define ADEDLEN_NENTRIES    2
#define AD_HEADER_LEN       (ADEDLEN_MAGIC + ADEDLEN_VERSION + ADEDLEN_FILLER + ADEDLEN_NENTRIES) /* 26 */
#define AD_ENTRY_LEN        12  /* size of a single entry header */

/* field widths */
#define ADEDLEN_NAME            255
#define ADEDLEN_COMMENT         200
#define ADEDLEN_FILEI           16
#define ADEDLEN_FINDERI         32
#define ADEDLEN_FILEDATESI      16
#define ADEDLEN_SHORTNAME       12 /* length up to 8.3 */
#define ADEDLEN_AFPFILEI        4
#define ADEDLEN_MACFILEI        4
#define ADEDLEN_PRODOSFILEI     8
#define ADEDLEN_MSDOSFILEI      2
#define ADEDLEN_DID             4
#define ADEDLEN_PRIVDEV         8
#define ADEDLEN_PRIVINO         8
#define ADEDLEN_PRIVSYN         8
#define ADEDLEN_PRIVID          4

#define ADEID_NUM_V2            13
#define ADEID_NUM_EA            8
#define ADEID_NUM_OSX           2

#define AD_DATASZ2 (AD_HEADER_LEN + ADEDLEN_NAME + ADEDLEN_COMMENT + ADEDLEN_FILEI + \
                    ADEDLEN_FINDERI + ADEDLEN_DID + ADEDLEN_AFPFILEI + ADEDLEN_SHORTNAME + \
                    ADEDLEN_PRODOSFILEI + ADEDLEN_PRIVDEV + ADEDLEN_PRIVINO + \
                    ADEDLEN_PRIVSYN + ADEDLEN_PRIVID + (ADEID_NUM_V2 * AD_ENTRY_LEN))
#if AD_DATASZ2 != 741
#error bad size for AD_DATASZ2
#endif

#define AD_DATASZ_EA (AD_HEADER_LEN + (ADEID_NUM_EA * AD_ENTRY_LEN) + \
                      ADEDLEN_FINDERI + ADEDLEN_COMMENT + ADEDLEN_FILEDATESI + ADEDLEN_AFPFILEI + \
                      ADEDLEN_PRIVDEV + ADEDLEN_PRIVINO + ADEDLEN_PRIVSYN + ADEDLEN_PRIVID)

#if AD_DATASZ_EA != 402
#error bad size for AD_DATASZ_EA
#endif

#define AD_DATASZ_OSX (AD_HEADER_LEN + (ADEID_NUM_OSX * AD_ENTRY_LEN) + ADEDLEN_FINDERI)

#if AD_DATASZ_OSX != 82
#error bad size for AD_DATASZ_OSX
#endif

#define AD_DATASZ_MAX   1024

#if AD_VERSION == AD_VERSION2
#define AD_DATASZ   AD_DATASZ2
#elif AD_VERSION == AD_VERSION_EA
#define AD_DATASZ   AD_DATASZ_EA
#endif

/* fallback for ad:ea on filesystems without fds for EAs, like old adouble:osx */
#define ADEDOFF_FINDERI_OSX  (AD_HEADER_LEN + ADEID_NUM_OSX*AD_ENTRY_LEN)
#define ADEDOFF_RFORK_OSX    (ADEDOFF_FINDERI_OSX + ADEDLEN_FINDERI)

/* special fd value used to indicate an open fork file is a (not open) symlink */
#define AD_SYMLINK -2

typedef uint32_t cnid_t;

struct ad_entry {
    off_t     ade_off;
    ssize_t   ade_len;
};

typedef struct adf_lock_t {
    struct flock lock;
    int user;
    int *refcount; /* handle read locks with multiple users */
} adf_lock_t;

struct ad_fd {
    int          adf_fd;        /* -1: invalid, AD_SYMLINK: symlink */
    char         *adf_syml;
    int          adf_flags;
    adf_lock_t   *adf_lock;
    int          adf_refcount, adf_lockcount, adf_lockmax;
};

/* some header protection */
#define AD_INITED  0xad494e54  /* ad"INT" */
#define AD_CLOSED  0xadc10ced

struct adouble;

struct adouble_fops {
    const char *(*ad_path)(const char *, int);
    int  (*ad_mkrf)(const char *);
    int  (*ad_rebuild_header)(struct adouble *);
    int  (*ad_header_read)(const char *, struct adouble *, const struct stat *);
    int  (*ad_header_upgrade)(struct adouble *, const char *);
};

struct adouble {
    uint32_t            ad_magic;         /* Official adouble magic                   */
    uint32_t            ad_version;       /* Official adouble version number          */
    char                ad_filler[16];
    struct ad_entry     ad_eid[ADEID_MAX];

    struct ad_fd        ad_data_fork;     /* the data fork                            */

    struct ad_fd        ad_resource_fork; /* adouble:v2 -> the adouble file           *
                                           * adouble:ea -> the EA fd                  */

    struct ad_fd        *ad_rfp;          /* adouble:v2 -> ad_resource_fork           *
                                           * adouble:ea -> ad_resource_fork           */

    struct ad_fd        *ad_mdp;          /* adouble:v2 -> ad_resource_fork           *
                                           * adouble:ea -> ad_data_fork               */

    int                 ad_vers;          /* Our adouble version info (AD_VERSION*)   */
    int                 ad_adflags;       /* ad_open flags adflags like ADFLAGS_DIR   */
    uint32_t            ad_inited;
    int                 ad_options;
    int                 ad_refcount;       /* multiple forks may open one adouble     */
    int                 ad_data_refcount;
    int                 ad_meta_refcount;
    int                 ad_reso_refcount;
    off_t               ad_rlen;           /* ressource fork len with AFP 3.0         *
                                            * the header parameter size is too small. */
    char                *ad_name;          /* name (UTF8-MAC)                         */
    struct adouble_fops *ad_ops;
    uint16_t            ad_open_forks;     /* open forks (by others)                  */
    char                ad_data[AD_DATASZ_MAX];
};

#define ADFLAGS_DF        (1<<0)
#define ADFLAGS_RF        (1<<1)
#define ADFLAGS_HF        (1<<2)
#define ADFLAGS_DIR       (1<<3)
#define ADFLAGS_NOHF      (1<<4)  /* not an error if no metadata fork */
#define ADFLAGS_NORF      (1<<5)  /* not an error if no ressource fork */
#define ADFLAGS_CHECK_OF  (1<<6)  /* check for open forks from us and other afpd's */
#define ADFLAGS_SETSHRMD  (1<<7)  /* setting share mode must be done with excl fcnt lock,
                                     which implies that the file must be openend rw.
                                     If it can't be opened rw (eg EPERM or EROFS) it will
                                     be opened ro and the fcntl locks will be shared, that
                                     at least prevent other users who have rw access to the
                                     file from placing excl locks. */
#define ADFLAGS_RDWR      (1<<8)  /* open read/write */
#define ADFLAGS_RDONLY    (1<<9)  /* open read only */
#define ADFLAGS_CREATE    (1<<10)  /* create file, open called with O_CREAT */
#define ADFLAGS_EXCL      (1<<11)  /* exclusive open, open called with O_EXCL */
#define ADFLAGS_TRUNC     (1<<12) /* truncate, open called with O_TRUNC */

#define ADVOL_NODEV      (1 << 0)
#define ADVOL_RO         (1 << 1)
#define ADVOL_UNIXPRIV   (1 << 2) /* adouble unix priv */
#define ADVOL_INVDOTS    (1 << 3) /* dot files (.DS_Store) are invisible) */
#define ADVOL_FOLLO_SYML (1 << 4)

/* lock flags */
#define ADLOCK_CLR      (0)
#define ADLOCK_RD       (1<<0)
#define ADLOCK_WR       (1<<1)
#define ADLOCK_MASK     (ADLOCK_RD | ADLOCK_WR)
#define ADLOCK_UPGRADE  (1<<2)
#define ADLOCK_FILELOCK (1<<3)

/* we use this so that we can use the same mechanism for both byte
 * locks and file synchronization locks. */
#if _FILE_OFFSET_BITS == 64
#define AD_FILELOCK_BASE (UINT64_C(0x7FFFFFFFFFFFFFFF) - 9)
#else
#define AD_FILELOCK_BASE (UINT32_C(0x7FFFFFFF) - 9)
#endif

#define BYTELOCK_MAX (AD_FILELOCK_BASE - 1)

/* datafork and rsrcfork sharemode locks */
#define AD_FILELOCK_OPEN_WR        (AD_FILELOCK_BASE + 0)
#define AD_FILELOCK_OPEN_RD        (AD_FILELOCK_BASE + 1)
#define AD_FILELOCK_RSRC_OPEN_WR   (AD_FILELOCK_BASE + 2)
#define AD_FILELOCK_RSRC_OPEN_RD   (AD_FILELOCK_BASE + 3)

#define AD_FILELOCK_DENY_WR        (AD_FILELOCK_BASE + 4)
#define AD_FILELOCK_DENY_RD        (AD_FILELOCK_BASE + 5)
#define AD_FILELOCK_RSRC_DENY_WR   (AD_FILELOCK_BASE + 6)
#define AD_FILELOCK_RSRC_DENY_RD   (AD_FILELOCK_BASE + 7)

#define AD_FILELOCK_OPEN_NONE      (AD_FILELOCK_BASE + 8)
#define AD_FILELOCK_RSRC_OPEN_NONE (AD_FILELOCK_BASE + 9)

/* time stuff. we overload the bits a little.  */
#define AD_DATE_CREATE         0
#define AD_DATE_MODIFY         4
#define AD_DATE_BACKUP         8
#define AD_DATE_ACCESS        12
#define AD_DATE_MASK          (AD_DATE_CREATE | AD_DATE_MODIFY | \
                              AD_DATE_BACKUP | AD_DATE_ACCESS)
#define AD_DATE_UNIX          (1 << 10)
#define AD_DATE_START         htonl(0x80000000)
#define AD_DATE_DELTA         946684800
#define AD_DATE_FROM_UNIX(x)  htonl((x) - AD_DATE_DELTA)
#define AD_DATE_TO_UNIX(x)    (ntohl(x) + AD_DATE_DELTA)

/* various finder offset and info bits */
#define FINDERINFO_FRTYPEOFF   0
#define FINDERINFO_FRCREATOFF  4
#define FINDERINFO_FRFLAGOFF   8

/* FinderInfo Flags, char in `ad ls`, valid for files|dirs */
#define FINDERINFO_ISONDESK      (1)     /* "d", fd */
#define FINDERINFO_COLOR         (0x0e)
#define FINDERINFO_HIDEEXT       (1<<4)  /* "e", fd */
#define FINDERINFO_ISHARED       (1<<6)  /* "m", f  */
#define FINDERINFO_HASNOINITS    (1<<7)  /* "n", f  */
#define FINDERINFO_HASBEENINITED (1<<8)  /* "i", fd */
#define FINDERINFO_HASCUSTOMICON (1<<10) /* "c", fd */
#define FINDERINFO_ISSTATIONNERY (1<<11) /* "t", f  */
#define FINDERINFO_NAMELOCKED    (1<<12) /* "s", fd */
#define FINDERINFO_HASBUNDLE     (1<<13) /* "b", fd */
#define FINDERINFO_INVISIBLE     (1<<14) /* "v", fd */
#define FINDERINFO_ISALIAS       (1<<15) /* "a", fd */

#define FINDERINFO_FRVIEWOFF  14
#define FINDERINFO_CUSTOMICON 0x4
#define FINDERINFO_CLOSEDVIEW 0x100

/*
  The "shared" and "invisible" attributes are opaque and stored and
  retrieved from the FinderFlags. This fixes Bug #2802236:
  <https://sourceforge.net/tracker/?func=detail&aid=2802236&group_id=8642&atid=108642>
*/

/* AFP attributes, char in `ad ls`, valid for files|dirs */
#define ATTRBIT_INVISIBLE (1<<0)  /* opaque from FinderInfo */
#define ATTRBIT_MULTIUSER (1<<1)  /* file: opaque, dir: see below */
#define ATTRBIT_SYSTEM    (1<<2)  /* "y", fd */
#define ATTRBIT_DOPEN     (1<<3)  /* data fork already open. Not stored, computed on the fly */
#define ATTRBIT_ROPEN     (1<<4)  /* resource fork already open. Not stored, computed on the fly */
#define ATTRBIT_NOWRITE   (1<<5)  /* "w", f, write inhibit(v2)/read-only(v1) bit */
#define ATTRBIT_BACKUP    (1<<6)  /* "p", fd */
#define ATTRBIT_NORENAME  (1<<7)  /* "r", fd */
#define ATTRBIT_NODELETE  (1<<8)  /* "l", fd */
#define ATTRBIT_NOCOPY    (1<<10) /* "o", f */
#define ATTRBIT_SETCLR    (1<<15) /* set/clear bit (d) */

/* AFP attributes for dirs. These should probably be computed on the fly.
 * We don't do that, nor does e.g. OS S X 10.5 Server */
#define ATTRBIT_EXPFLDR   (1<<1)  /* Folder is a sharepoint */
#define ATTRBIT_MOUNTED   (1<<3)  /* Directory is mounted by a user */
#define ATTRBIT_SHARED    (1<<4)  /* Shared area, called IsExpFolder in spec */

/* private AFPFileInfo bits */
#define AD_AFPFILEI_OWNER       (1 << 0) /* any owner */
#define AD_AFPFILEI_GROUP       (1 << 1) /* ignore group */
#define AD_AFPFILEI_BLANKACCESS (1 << 2) /* blank access permissions */

#define ad_data_fileno(ad)  ((ad)->ad_data_fork.adf_fd)
#define ad_reso_fileno(ad)  ((ad)->ad_rfp->adf_fd)
#define ad_meta_fileno(ad)  ((ad)->ad_mdp->adf_fd)

/* -1: not open, AD_SYMLINK (-2): it's a symlink */
#define AD_DATA_OPEN(ad) (((ad)->ad_data_refcount) && (ad_data_fileno(ad) >= 0))
#define AD_META_OPEN(ad) (((ad)->ad_meta_refcount) && (ad_meta_fileno(ad) >= 0))
#define AD_RSRC_OPEN(ad) (((ad)->ad_reso_refcount) && (ad_reso_fileno(ad) >= 0))

#define ad_getversion(ad)   ((ad)->ad_version)

#define ad_getentrylen(ad,eid)     ((ad)->ad_eid[(eid)].ade_len)
#define ad_setentrylen(ad,eid,len) ((ad)->ad_eid[(eid)].ade_len = (len))
#define ad_setentryoff(ad,eid,off) ((ad)->ad_eid[(eid)].ade_off = (off))
#define ad_entry(ad,eid)           ((caddr_t)(ad)->ad_data + (ad)->ad_eid[(eid)].ade_off)

#define ad_get_RF_flags(ad) ((ad)->ad_rfp->adf_flags)
#define ad_get_MD_flags(ad) ((ad)->ad_mdp->adf_flags)

/* Refcounting open forks using one struct adouble */
#define ad_ref(ad)   (ad)->ad_refcount++
#define ad_unref(ad) --((ad)->ad_refcount)

#define ad_get_syml_opt(ad) (((ad)->ad_options & ADVOL_FOLLO_SYML) ? 0 : O_NOFOLLOW)

/* ad_flush.c */
extern int ad_rebuild_adouble_header_v2(struct adouble *);
extern int ad_rebuild_adouble_header_ea(struct adouble *);
extern int ad_copy_header (struct adouble *, struct adouble *);
extern int ad_flush (struct adouble *);
extern int ad_close (struct adouble *, int);
extern int fsetrsrcea(struct adouble *ad, int fd, const char *eaname, const void *value, size_t size, int flags);

/* ad_lock.c */
extern int ad_testlock      (struct adouble *adp, int eid, off_t off);
extern uint16_t ad_openforks(struct adouble *adp, uint16_t);
extern int ad_lock(struct adouble *, uint32_t eid, int type, off_t off, off_t len, int fork);
extern void ad_unlock(struct adouble *, int fork, int unlckbrl);
extern int ad_tmplock(struct adouble *, uint32_t eid, int type, off_t off, off_t len, int fork);

/* ad_open.c */
extern off_t ad_getentryoff(const struct adouble *ad, int eid);
extern const char *adflags2logstr(int adflags);
extern int ad_setfuid     (const uid_t );
extern uid_t ad_getfuid   (void );
extern char *ad_dir       (const char *);
extern const char *ad_path      (const char *, int);
extern const char *ad_path_ea   (const char *, int);
extern const char *ad_path_osx  (const char *path, int adflags);
extern int ad_mode        (const char *, mode_t);
extern int ad_mkdir       (const char *, mode_t);
struct vol;
extern void ad_init       (struct adouble *, const struct vol * restrict);
extern void ad_init_old   (struct adouble *ad, int flags, int options);
extern int ad_init_offsets(struct adouble *ad);
extern int ad_open        (struct adouble *ad, const char *path, int adflags, ...);
extern int ad_openat      (struct adouble *, int dirfd, const char *path, int adflags, ...);
extern int ad_refresh     (const char *path, struct adouble *);
extern int ad_stat        (const char *, struct stat *);
extern int ad_metadata    (const char *, int, struct adouble *);
extern int ad_metadataat  (int, const char *, int, struct adouble *);
extern mode_t ad_hf_mode(mode_t mode);
extern int ad_valid_header_osx(const char *path);
extern off_t ad_reso_size(const char *path, int adflags, struct adouble *ad);

/* ad_conv.c */
extern int ad_convert(const char *path, const struct stat *sp, const struct vol *vol, const char **newpath);

/* ad_read.c/ad_write.c */
extern int     sys_ftruncate(int fd, off_t length);
extern ssize_t ad_read(struct adouble *, uint32_t, off_t, char *, size_t);
extern ssize_t ad_pread(struct ad_fd *, void *, size_t, off_t);
extern ssize_t ad_write(struct adouble *, uint32_t, off_t, int, const char *, size_t);
extern ssize_t adf_pread(struct ad_fd *, void *, size_t, off_t);
extern ssize_t adf_pwrite(struct ad_fd *, const void *, size_t, off_t);
extern int     ad_dtruncate(struct adouble *, off_t);
extern int     ad_rtruncate(struct adouble *, const char *, off_t);
extern int     copy_fork(int eid, struct adouble *add, struct adouble *ads);

/* ad_size.c */
extern off_t ad_size (const struct adouble *, uint32_t );

/* ad_mmap.c */
extern void *ad_mmapread(struct adouble *, uint32_t, off_t, size_t);
extern void *ad_mmapwrite(struct adouble *, uint32_t, off_t, int, size_t);
#define ad_munmap(buf, len)  (munmap((buf), (len)))

/* ad_date.c */
extern int ad_setdate(struct adouble *, unsigned int, uint32_t);
extern int ad_getdate(const struct adouble *, unsigned int, uint32_t *);

/* ad_attr.c */
extern int       ad_setattr(const struct adouble *, const uint16_t);
extern int       ad_getattr(const struct adouble *, uint16_t *);
extern int       ad_setname(struct adouble *, const char *);
extern int       ad_setid(struct adouble *, dev_t dev, ino_t ino, uint32_t, uint32_t, const void *);
extern uint32_t  ad_getid(struct adouble *, dev_t, ino_t, cnid_t, const void *);
extern uint32_t  ad_forcegetid(struct adouble *adp);

#ifdef WITH_SENDFILE
extern int ad_readfile_init(const struct adouble *ad, int eid, off_t *off, int end);
#endif

#endif /* _ATALK_ADOUBLE_H */
