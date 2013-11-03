/*
 * Copyright (c) 1996 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h> /* works around a bug */
#include <sys/param.h>
#include <errno.h>

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/globals.h>
#include <atalk/fce_api.h>

#include "volume.h"
#include "directory.h"
#include "fork.h"

/* we need to have a hashed list of oforks (by dev inode) */
#define OFORK_HASHSIZE  64
static struct ofork *ofork_table[OFORK_HASHSIZE]; /* forks hashed by dev/inode */
static struct ofork **oforks = NULL;              /* point to allocated table of open forks pointers */
static int          nforks = 0;
static u_short      lastrefnum = 0;


/* OR some of each character for the hash*/
static unsigned long hashfn(const struct file_key *key)
{
    return key->inode & (OFORK_HASHSIZE - 1);
}

static void of_hash(struct ofork *of)
{
    struct ofork **table;

    table = &ofork_table[hashfn(&of->key)];
    if ((of->next = *table) != NULL)
        (*table)->prevp = &of->next;
    *table = of;
    of->prevp = table;
}

static void of_unhash(struct ofork *of)
{
    if (of->prevp) {
        if (of->next)
            of->next->prevp = of->prevp;
        *(of->prevp) = of->next;
    }
}

int of_flush(const struct vol *vol)
{
    int refnum;

    if (!oforks)
        return 0;

    for ( refnum = 0; refnum < nforks; refnum++ ) {
        if (oforks[ refnum ] != NULL && (oforks[refnum]->of_vol == vol) &&
            flushfork( oforks[ refnum ] ) < 0 ) {
            LOG(log_error, logtype_afpd, "of_flush: %s", strerror(errno) );
        }
    }
    return( 0 );
}

int of_rename(const struct vol *vol,
              struct ofork *s_of,
              struct dir *olddir, const char *oldpath _U_,
              struct dir *newdir, const char *newpath)
{
    struct ofork *of, *next;
    int done = 0;

    if (!s_of)
        return AFP_OK;

    next = ofork_table[hashfn(&s_of->key)];
    while ((of = next)) {
        next = next->next; /* so we can unhash and still be all right. */

        if (vol == of->of_vol
            && olddir->d_did == of->of_did
            && s_of->key.dev == of->key.dev
            && s_of->key.inode == of->key.inode ) {
            if (!done) {
                free(of_name(of));
                if ((of_name(of) = strdup(newpath)) == NULL)
                    return AFPERR_MISC;
                done = 1;
            }
            if (newdir != olddir)
                of->of_did = newdir->d_did;
        }
    }

    return AFP_OK;
}

#define min(a,b)    ((a)<(b)?(a):(b))

struct ofork *
of_alloc(struct vol *vol,
         struct dir    *dir,
         char      *path,
         uint16_t     *ofrefnum,
         const int      eid,
         struct adouble *ad,
         struct stat    *st)
{
    struct ofork        *of;
    uint16_t       refnum, of_refnum;

    int         i;

    if (!oforks) {
        nforks = getdtablesize() - 10;
        /* protect against insane ulimit -n */
        nforks = min(nforks, 0xffff);
        oforks = (struct ofork **) calloc(nforks, sizeof(struct ofork *));
        if (!oforks)
            return NULL;
    }

    for ( refnum = ++lastrefnum, i = 0; i < nforks; i++, refnum++ ) {
        /* cf AFP3.0.pdf, File fork page 40 */
        if (!refnum)
            refnum++;
        if ( oforks[ refnum % nforks ] == NULL ) {
            break;
        }
    }
    /* grr, Apple and their 'uniquely identifies'
       the next line is a protection against
       of_alloc()
       refnum % nforks = 3
       lastrefnum = 3
       oforks[3] != NULL
       refnum = 4
       oforks[4] == NULL
       return 4

       close(oforks[4])

       of_alloc()
       refnum % nforks = 4
       ...
       return 4
       same if lastrefnum++ rather than ++lastrefnum.
    */
    lastrefnum = refnum;
    if ( i == nforks ) {
        LOG(log_error, logtype_afpd, "of_alloc: maximum number of forks exceeded.");
        return( NULL );
    }

    of_refnum = refnum % nforks;
    if (( oforks[ of_refnum ] =
          (struct ofork *)malloc( sizeof( struct ofork ))) == NULL ) {
        LOG(log_error, logtype_afpd, "of_alloc: malloc: %s", strerror(errno) );
        return NULL;
    }
    of = oforks[of_refnum];

    /* see if we need to allocate space for the adouble struct */
    if (!ad) {
        ad = malloc( sizeof( struct adouble ) );
        if (!ad) {
            LOG(log_error, logtype_afpd, "of_alloc: malloc: %s", strerror(errno) );
            free(of);
            oforks[ of_refnum ] = NULL;
            return NULL;
        }

        /* initialize to zero. This is important to ensure that
           ad_open really does reinitialize the structure. */
        ad_init(ad, vol);
        if ((ad->ad_name = strdup(path)) == NULL) {
            LOG(log_error, logtype_afpd, "of_alloc: malloc: %s", strerror(errno) );
            free(ad);
            free(of);
            oforks[ of_refnum ] = NULL;
            return NULL;
        }
    } else {
        /* Increase the refcount on this struct adouble. This is
           decremented again in oforc_dealloc. */
        ad_ref(ad);
    }

    of->of_ad = ad;
    of->of_vol = vol;
    of->of_did = dir->d_did;

    *ofrefnum = refnum;
    of->of_refnum = refnum;
    of->key.dev = st->st_dev;
    of->key.inode = st->st_ino;
    if (eid == ADEID_DFORK)
        of->of_flags = AFPFORK_DATA;
    else
        of->of_flags = AFPFORK_RSRC;

    of_hash(of);
    return( of );
}

struct ofork *of_find(const uint16_t ofrefnum )
{
    if (!oforks || !nforks)
        return NULL;

    return( oforks[ ofrefnum % nforks ] );
}

/* -------------------------- */
int of_stat(const struct vol *vol, struct path *path)
{
    int ret;

    path->st_errno = 0;
    path->st_valid = 1;

    if ((ret = ostat(path->u_name, &path->st, vol_syml_opt(vol))) < 0) {
        LOG(log_debug, logtype_afpd, "of_stat('%s/%s': %s)",
            cfrombstr(curdir->d_fullpath), path->u_name, strerror(errno));
    	path->st_errno = errno;
    }

    return ret;
}


#ifdef HAVE_ATFUNCS
int of_fstatat(int dirfd, struct path *path)
{
    int ret;

    path->st_errno = 0;
    path->st_valid = 1;

    if ((ret = fstatat(dirfd, path->u_name, &path->st, AT_SYMLINK_NOFOLLOW)) < 0)
    	path->st_errno = errno;

   return ret;
}
#endif /* HAVE_ATFUNCS */

/* -------------------------- 
   stat the current directory.
   stat(".") works even if "." is deleted thus
   we have to stat ../name because we want to know if it's there
*/
int of_statdir(struct vol *vol, struct path *path)
{
    static char pathname[ MAXPATHLEN + 1] = "../";
    int ret;
    size_t len;
    struct dir *dir;

    if (*path->m_name) {
        /* not curdir */
        return of_stat(vol, path);
    }
    path->st_errno = 0;
    path->st_valid = 1;
    /* FIXME, what about: we don't have r-x perm anymore ? */
    len = blength(path->d_dir->d_u_name);
    if (len > (MAXPATHLEN - 3))
        len = MAXPATHLEN - 3;
    strncpy(pathname + 3, cfrombstr(path->d_dir->d_u_name), len + 1);

    LOG(log_debug, logtype_afpd, "of_statdir: stating: '%s'", pathname);

    if (!(ret = ostat(pathname, &path->st, vol_syml_opt(vol))))
        return 0;

    path->st_errno = errno;

    /* hmm, can't stat curdir anymore */
    if (errno == EACCES && (dir = dirlookup(vol, curdir->d_pdid))) {
       if (movecwd(vol, dir)) 
           return -1;
       path->st_errno = 0;

       if ((ret = ostat(cfrombstr(path->d_dir->d_u_name), &path->st, vol_syml_opt(vol))) < 0) 
           path->st_errno = errno;
    }

    return ret;
}

/* -------------------------- */
struct ofork *of_findname(const struct vol *vol, struct path *path)
{
    struct ofork *of;
    struct file_key key;

    if (!path->st_valid) {
        of_stat(vol, path);
    }

    if (path->st_errno)
        return NULL;

    key.dev = path->st.st_dev;
    key.inode = path->st.st_ino;

    for (of = ofork_table[hashfn(&key)]; of; of = of->next) {
        if (key.dev == of->key.dev && key.inode == of->key.inode ) {
            return of;
        }
    }

    return NULL;
}

/*!
 * @brief Search for open fork by dirfd/name
 *
 * Function call of_fstatat with dirfd and path and uses dev and ino
 * to search the open fork table.
 *
 * @param dirfd     (r) directory fd
 * @param path      (rw) pointer to struct path
 */
#ifdef HAVE_ATFUNCS
struct ofork *of_findnameat(int dirfd, struct path *path)
{
    struct ofork *of;
    struct file_key key;
    
    if ( ! path->st_valid) {
        of_fstatat(dirfd, path);
    }
    	
    if (path->st_errno)
        return NULL;

    key.dev = path->st.st_dev;
    key.inode = path->st.st_ino;

    for (of = ofork_table[hashfn(&key)]; of; of = of->next) {
        if (key.dev == of->key.dev && key.inode == of->key.inode ) {
            return of;
        }
    }

    return NULL;
}
#endif

void of_dealloc(struct ofork *of)
{
    if (!oforks)
        return;

    of_unhash(of);
    oforks[ of->of_refnum % nforks ] = NULL;

    /* decrease refcount */
    of->of_ad->ad_refcount--;

    if ( of->of_ad->ad_refcount <= 0) {
        free( of->of_ad->ad_name );
        free( of->of_ad);
    }

    free( of );
}

/* --------------------------- */
int of_closefork(const AFPObj *obj, struct ofork *ofork)
{
    struct timeval      tv;
    int         adflags = 0;
    int                 ret;

    adflags = 0;
    if (ofork->of_flags & AFPFORK_DATA)
        adflags |= ADFLAGS_DF;
    if (ofork->of_flags & AFPFORK_META)
        adflags |= ADFLAGS_HF;
    if (ofork->of_flags & AFPFORK_RSRC) {
        adflags |= ADFLAGS_RF;
        /* Only set the rfork's length if we're closing the rfork. */
        ad_refresh(NULL, ofork->of_ad );
        if ((ofork->of_flags & AFPFORK_DIRTY) && !gettimeofday(&tv, NULL)) {
            ad_setdate(ofork->of_ad, AD_DATE_MODIFY | AD_DATE_UNIX,tv.tv_sec);
            ad_flush( ofork->of_ad );
        }
    }

    /* Somone has used write_fork, we assume file was changed, register it to file change event api */
    if (ofork->of_flags & AFPFORK_MODIFIED) {
        struct dir *dir =  dirlookup(ofork->of_vol, ofork->of_did);
        bstring forkpath = bformat("%s/%s", bdata(dir->d_fullpath), of_name(ofork));
        fce_register(FCE_FILE_MODIFY, bdata(forkpath), NULL, fce_file);
        bdestroy(forkpath);
    }

    ad_unlock(ofork->of_ad, ofork->of_refnum, ofork->of_flags & AFPFORK_ERROR ? 0 : 1);

#ifdef HAVE_FSHARE_T
    if (obj->options.flags & OPTION_SHARE_RESERV) {
        fshare_t shmd;
        shmd.f_id = ofork->of_refnum;
        if (AD_DATA_OPEN(ofork->of_ad))
            fcntl(ad_data_fileno(ofork->of_ad), F_UNSHARE, &shmd);
        if (AD_RSRC_OPEN(ofork->of_ad))
            fcntl(ad_reso_fileno(ofork->of_ad), F_UNSHARE, &shmd);
    }
#endif

    ret = 0;
    if ( ad_close( ofork->of_ad, adflags | ADFLAGS_SETSHRMD) < 0 ) {
        ret = -1;
    }

    of_dealloc(ofork);

    return ret;
}

/* ----------------------

 */
struct adouble *of_ad(const struct vol *vol, struct path *path, struct adouble *ad)
{
    struct ofork        *of;
    struct adouble      *adp;

    if ((of = of_findname(vol, path))) {
        adp = of->of_ad;
    } else {
        ad_init(ad, vol);
        adp = ad;
    }
    return adp;
}

/* ----------------------
   close all forks for a volume
*/
void of_closevol(const AFPObj *obj, const struct vol *vol)
{
    int refnum;

    if (!oforks)
        return;

    for ( refnum = 0; refnum < nforks; refnum++ ) {
        if (oforks[ refnum ] != NULL && oforks[refnum]->of_vol == vol) {
            if (of_closefork(obj, oforks[ refnum ]) < 0 ) {
                LOG(log_error, logtype_afpd, "of_closevol: %s", strerror(errno) );
            }
        }
    }
    return;
}

/* ----------------------
   close all forks for a volume
*/
void of_close_all_forks(const AFPObj *obj)
{
    int refnum;

    if (!oforks)
        return;

    for ( refnum = 0; refnum < nforks; refnum++ ) {
        if (oforks[ refnum ] != NULL) {
            if (of_closefork(obj, oforks[ refnum ]) < 0 ) {
                LOG(log_error, logtype_afpd, "of_close_all_forks: %s", strerror(errno) );
            }
        }
    }
    return;
}

