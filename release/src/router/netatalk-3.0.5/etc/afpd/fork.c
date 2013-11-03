/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * Copyright (c) 2010      Frank Lahm
 *
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <inttypes.h>

#include <atalk/dsi.h>
#include <atalk/afp.h>
#include <atalk/adouble.h>
#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>
#include <atalk/ea.h>

#include "fork.h"
#include "file.h"
#include "directory.h"
#include "desktop.h"
#include "volume.h"

#ifdef AFS
struct ofork *writtenfork;
#endif

static int getforkparams(const AFPObj *obj, struct ofork *ofork, uint16_t bitmap, char *buf, size_t *buflen)
{
    struct path         path;
    struct stat     *st;

    struct adouble  *adp;
    struct dir      *dir;
    struct vol      *vol;


    /* can only get the length of the opened fork */
    if ( ( (bitmap & ((1<<FILPBIT_DFLEN) | (1<<FILPBIT_EXTDFLEN)))
           && (ofork->of_flags & AFPFORK_RSRC))
         ||
         ( (bitmap & ((1<<FILPBIT_RFLEN) | (1<<FILPBIT_EXTRFLEN)))
           && (ofork->of_flags & AFPFORK_DATA))) {
        return( AFPERR_BITMAP );
    }

    if (! AD_META_OPEN(ofork->of_ad)) {
        adp = NULL;
    } else {
        adp = ofork->of_ad;
    }

    vol = ofork->of_vol;
    dir = dirlookup(vol, ofork->of_did);

    if (NULL == (path.m_name = utompath(vol, of_name(ofork), dir->d_did, utf8_encoding(obj)))) {
        return( AFPERR_MISC );
    }
    path.u_name = of_name(ofork);
    path.id = 0;
    st = &path.st;
    if ( bitmap & ( (1<<FILPBIT_DFLEN) | (1<<FILPBIT_EXTDFLEN) |
                    (1<<FILPBIT_FNUM) | (1 << FILPBIT_CDATE) |
                    (1 << FILPBIT_MDATE) | (1 << FILPBIT_BDATE))) {
        if ( ad_data_fileno( ofork->of_ad ) <= 0 ) {
            /* 0 is for symlink */
            if (movecwd(vol, dir) < 0)
                return( AFPERR_NOOBJ );
            if ( lstat( path.u_name, st ) < 0 )
                return( AFPERR_NOOBJ );
        } else {
            if ( fstat( ad_data_fileno( ofork->of_ad ), st ) < 0 ) {
                return( AFPERR_BITMAP );
            }
        }
    }
    return getmetadata(obj, vol, bitmap, &path, dir, buf, buflen, adp );
}

static off_t get_off_t(char **ibuf, int is64)
{
    uint32_t             temp;
    off_t                 ret;

    ret = 0;
    memcpy(&temp, *ibuf, sizeof( temp ));
    ret = ntohl(temp); /* ntohl is unsigned */
    *ibuf += sizeof(temp);

    if (is64) {
        memcpy(&temp, *ibuf, sizeof( temp ));
        *ibuf += sizeof(temp);
        ret = ntohl(temp)| (ret << 32);
    }
    else {
        ret = (int)ret; /* sign extend */
    }
    return ret;
}

static int set_off_t(off_t offset, char *rbuf, int is64)
{
    uint32_t  temp;
    int        ret;

    ret = 0;
    if (is64) {
        temp = htonl(offset >> 32);
        memcpy(rbuf, &temp, sizeof( temp ));
        rbuf += sizeof(temp);
        ret = sizeof( temp );
        offset &= 0xffffffff;
    }
    temp = htonl(offset);
    memcpy(rbuf, &temp, sizeof( temp ));
    ret += sizeof( temp );

    return ret;
}

static int is_neg(int is64, off_t val)
{
    if (val < 0 || (sizeof(off_t) == 8 && !is64 && (val & 0x80000000U)))
        return 1;
    return 0;
}

static int sum_neg(int is64, off_t offset, off_t reqcount)
{
    if (is_neg(is64, offset +reqcount) )
        return 1;
    return 0;
}

static int fork_setmode(const AFPObj *obj, struct adouble *adp, int eid, int access, int ofrefnum)
{
    int ret;
    int readset;
    int writeset;
    int denyreadset;
    int denywriteset;

#ifdef HAVE_FSHARE_T
    fshare_t shmd;

    if (obj->options.flags & OPTION_SHARE_RESERV) {
        shmd.f_access = (access & OPENACC_RD ? F_RDACC : 0) | (access & OPENACC_WR ? F_WRACC : 0);
        if (shmd.f_access == 0)
            /* we must give an access mode, otherwise fcntl will complain */
            shmd.f_access = F_RDACC;
        shmd.f_deny = (access & OPENACC_DRD ? F_RDDNY : F_NODNY) | (access & OPENACC_DWR) ? F_WRDNY : 0;
        shmd.f_id = ofrefnum;

        int fd = (eid == ADEID_DFORK) ? ad_data_fileno(adp) : ad_reso_fileno(adp);

        if (fd != -1 && fd != AD_SYMLINK && fcntl(fd, F_SHARE, &shmd) != 0) {
            LOG(log_debug, logtype_afpd, "fork_setmode: fcntl: %s", strerror(errno));
            errno = EACCES;
            return -1;
        }
    }
#endif

    if (! (access & (OPENACC_WR | OPENACC_RD | OPENACC_DWR | OPENACC_DRD))) {
        return ad_lock(adp, eid, ADLOCK_RD | ADLOCK_FILELOCK, AD_FILELOCK_OPEN_NONE, 1, ofrefnum);
    }

    if ((access & (OPENACC_RD | OPENACC_DRD))) {
        if ((readset = ad_testlock(adp, eid, AD_FILELOCK_OPEN_RD)) <0)
            return readset;
        if ((denyreadset = ad_testlock(adp, eid, AD_FILELOCK_DENY_RD)) <0)
            return denyreadset;

        if ((access & OPENACC_RD) && denyreadset) {
            errno = EACCES;
            return -1;
        }
        if ((access & OPENACC_DRD) && readset) {
            errno = EACCES;
            return -1;
        }
        /* boolean logic is not enough, because getforkmode is not always telling the
         * true
         */
        if ((access & OPENACC_RD)) {
            ret = ad_lock(adp, eid, ADLOCK_RD | ADLOCK_FILELOCK, AD_FILELOCK_OPEN_RD, 1, ofrefnum);
            if (ret)
                return ret;
        }
        if ((access & OPENACC_DRD)) {
            ret = ad_lock(adp, eid, ADLOCK_RD | ADLOCK_FILELOCK, AD_FILELOCK_DENY_RD, 1, ofrefnum);
            if (ret)
                return ret;
        }
    }
    /* ------------same for writing -------------- */
    if ((access & (OPENACC_WR | OPENACC_DWR))) {
        if ((writeset = ad_testlock(adp, eid, AD_FILELOCK_OPEN_WR)) <0)
            return writeset;
        if ((denywriteset = ad_testlock(adp, eid, AD_FILELOCK_DENY_WR)) <0)
            return denywriteset;

        if ((access & OPENACC_WR) && denywriteset) {
            errno = EACCES;
            return -1;
        }
        if ((access & OPENACC_DWR) && writeset) {
            errno = EACCES;
            return -1;
        }
        if ((access & OPENACC_WR)) {
            ret = ad_lock(adp, eid, ADLOCK_RD | ADLOCK_FILELOCK, AD_FILELOCK_OPEN_WR, 1, ofrefnum);
            if (ret)
                return ret;
        }
        if ((access & OPENACC_DWR)) {
            ret = ad_lock(adp, eid, ADLOCK_RD | ADLOCK_FILELOCK, AD_FILELOCK_DENY_WR, 1, ofrefnum);
            if (ret)
                return ret;
        }
    }

    return 0;
}

/* ----------------------- */
int afp_openfork(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol      *vol;
    struct dir      *dir;
    struct ofork    *ofork, *opened;
    struct adouble  *adsame = NULL;
    size_t          buflen;
    int             ret, adflags, eid;
    uint32_t        did;
    uint16_t        vid, bitmap, access, ofrefnum;
    char            fork, *path, *upath;
    struct stat     *st;
    uint16_t        bshort;
    struct path     *s_path;

    ibuf++;
    fork = *ibuf++;
    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof(vid);

    *rbuflen = 0;
    if (NULL == ( vol = getvolbyvid( vid ))) {
        return( AFPERR_PARAM );
    }

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof( int );

    if (NULL == ( dir = dirlookup( vol, did ))) {
        return afp_errno;
    }

    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );
    memcpy(&access, ibuf, sizeof( access ));
    access = ntohs( access );
    ibuf += sizeof( access );

    if ((vol->v_flags & AFPVOL_RO) && (access & OPENACC_WR)) {
        return AFPERR_VLOCK;
    }

    if (NULL == ( s_path = cname( vol, dir, &ibuf ))) {
        return get_afp_errno(AFPERR_PARAM);
    }

    if (*s_path->m_name == '\0') {
        /* it's a dir ! */
        return  AFPERR_BADTYPE;
    }

    /* stat() data fork st is set because it's not a dir */
    switch ( s_path->st_errno ) {
    case 0:
        break;
    case ENOENT:
        return AFPERR_NOOBJ;
    case EACCES:
        return (access & OPENACC_WR) ? AFPERR_LOCK : AFPERR_ACCESS;
    default:
        LOG(log_error, logtype_afpd, "afp_openfork(%s): %s", s_path->m_name, strerror(errno));
        return AFPERR_PARAM;
    }

    upath = s_path->u_name;
    path = s_path->m_name;
    st = &s_path->st;

    if (!vol_unix_priv(vol)) {
        if (check_access(obj, vol, upath, access ) < 0) {
            return AFPERR_ACCESS;
        }
    } else {
        if (file_access(obj, vol, s_path, access ) < 0) {
            return AFPERR_ACCESS;
        }
    }

    if ((opened = of_findname(vol, s_path))) {
        adsame = opened->of_ad;
    }

    adflags = ADFLAGS_SETSHRMD;

    if (fork == OPENFORK_DATA) {
        eid = ADEID_DFORK;
        adflags |= ADFLAGS_DF;
    } else {
        eid = ADEID_RFORK;
        adflags |= ADFLAGS_RF;
    }

    if (access & OPENACC_WR) {
        adflags |= ADFLAGS_RDWR;
        if (fork != OPENFORK_DATA)
            /*
             * We only try to create the resource
             * fork if the user wants to open it for write acess.
             */
            adflags |= ADFLAGS_CREATE;
    } else {
        adflags |= ADFLAGS_RDONLY;
    }

    if ((ofork = of_alloc(vol, curdir, path, &ofrefnum, eid, adsame, st)) == NULL)
        return AFPERR_NFILE;

    LOG(log_debug, logtype_afpd, "afp_openfork(\"%s\", %s, %s)",
        fullpathname(s_path->u_name),
        (fork == OPENFORK_DATA) ? "data" : "reso",
        !(access & OPENACC_WR) ? "O_RDONLY" : "O_RDWR");

    ret = AFPERR_NOOBJ;

    /* First ad_open(), opens data or ressource fork */
    if (ad_open(ofork->of_ad, upath, adflags, 0666) < 0) {
        switch (errno) {
        case EROFS:
            ret = AFPERR_VLOCK;
        case EACCES:
            goto openfork_err;
        case ENOENT:
            goto openfork_err;
        case EMFILE :
        case ENFILE :
            ret = AFPERR_NFILE;
            goto openfork_err;
        case EISDIR :
            ret = AFPERR_BADTYPE;
            goto openfork_err;
        default:
            LOG(log_error, logtype_afpd, "afp_openfork(%s): ad_open: %s", s_path->m_name, strerror(errno) );
            ret = AFPERR_PARAM;
            goto openfork_err;
        }
    }

    /*
     * Create metadata if we open rw, otherwise only open existing metadata
     */
    if (access & OPENACC_WR) {
        adflags = ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE;
    } else {
        adflags = ADFLAGS_HF | ADFLAGS_RDONLY;
    }

    if (ad_open(ofork->of_ad, upath, adflags, 0666) == 0) {
        ofork->of_flags |= AFPFORK_META;
        if (ad_get_MD_flags(ofork->of_ad) & O_CREAT) {
            LOG(log_debug, logtype_afpd, "afp_openfork(\"%s\"): setting CNID", upath);
            cnid_t id;
            if ((id = get_id(vol, ofork->of_ad, st, dir->d_did, upath, strlen(upath))) == CNID_INVALID) {
                LOG(log_error, logtype_afpd, "afp_createfile(\"%s\"): CNID error", upath);
                goto openfork_err;
            }
            (void)ad_setid(ofork->of_ad, st->st_dev, st->st_ino, id, dir->d_did, vol->v_stamp);
            ad_flush(ofork->of_ad);
        }
    } else {
        switch (errno) {
        case EACCES:
        case ENOENT:
            /* no metadata? We don't care! */
            break;
        case EROFS:
            ret = AFPERR_VLOCK;
        case EMFILE :
        case ENFILE :
            ret = AFPERR_NFILE;
            goto openfork_err;
        case EISDIR :
            ret = AFPERR_BADTYPE;
            goto openfork_err;
        default:
            LOG(log_error, logtype_afpd, "afp_openfork(%s): ad_open: %s", s_path->m_name, strerror(errno) );
            ret = AFPERR_PARAM;
            goto openfork_err;
        }
    }

    if ((adflags & ADFLAGS_RF) && (ad_get_RF_flags( ofork->of_ad) & O_CREAT)) {
        if (ad_setname(ofork->of_ad, path)) {
            ad_flush( ofork->of_ad );
        }
    }

    if ((ret = getforkparams(obj, ofork, bitmap, rbuf + 2 * sizeof(int16_t), &buflen)) != AFP_OK) {
        ad_close( ofork->of_ad, adflags | ADFLAGS_SETSHRMD);
        goto openfork_err;
    }

    *rbuflen = buflen + 2 * sizeof( uint16_t );
    bitmap = htons( bitmap );
    memcpy(rbuf, &bitmap, sizeof( uint16_t ));
    rbuf += sizeof( uint16_t );

    /* check  WriteInhibit bit if we have a ressource fork
     * the test is done here, after some Mac trafic capture
     */
    if (ad_meta_fileno(ofork->of_ad) != -1) {   /* META */
        ad_getattr(ofork->of_ad, &bshort);
        if ((bshort & htons(ATTRBIT_NOWRITE)) && (access & OPENACC_WR)) {
            ad_close( ofork->of_ad, adflags | ADFLAGS_SETSHRMD);
            of_dealloc(ofork);
            ofrefnum = 0;
            memcpy(rbuf, &ofrefnum, sizeof(ofrefnum));
            return(AFPERR_OLOCK);
        }
    }

    /*
     * synchronization locks:
     */

    /* don't try to lock non-existent rforks. */
    if ((eid == ADEID_DFORK)
        || (ad_reso_fileno(ofork->of_ad) != -1)
        || (ofork->of_ad->ad_vers == AD_VERSION_EA)) {
        ret = fork_setmode(obj, ofork->of_ad, eid, access, ofrefnum);
        /* can we access the fork? */
        if (ret < 0) {
            ofork->of_flags |= AFPFORK_ERROR;
            ret = errno;
            ad_close( ofork->of_ad, adflags | ADFLAGS_SETSHRMD);
            of_dealloc(ofork);
            switch (ret) {
            case EAGAIN: /* return data anyway */
            case EACCES:
            case EINVAL:
                ofrefnum = 0;
                memcpy(rbuf, &ofrefnum, sizeof(ofrefnum));
                return( AFPERR_DENYCONF );
                break;
            default:
                *rbuflen = 0;
                LOG(log_error, logtype_afpd, "afp_openfork(%s): ad_lock: %s", s_path->m_name, strerror(ret) );
                return( AFPERR_PARAM );
            }
        }
        if ((access & OPENACC_WR))
            ofork->of_flags |= AFPFORK_ACCWR;
    }
    /* the file may be open read only without ressource fork */
    if ((access & OPENACC_RD))
        ofork->of_flags |= AFPFORK_ACCRD;

    LOG(log_debug, logtype_afpd, "afp_openfork(\"%s\"): fork: %" PRIu16,
        fullpathname(s_path->m_name), ofork->of_refnum);

    memcpy(rbuf, &ofrefnum, sizeof(ofrefnum));
    return( AFP_OK );

openfork_err:
    of_dealloc(ofork);
    if (errno == EACCES)
        return (access & OPENACC_WR) ? AFPERR_LOCK : AFPERR_ACCESS;
    return ret;
}

int afp_setforkparams(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf _U_, size_t *rbuflen)
{
    struct ofork    *ofork;
    off_t       size;
    uint16_t       ofrefnum, bitmap;
    int                 err;
    int                 is64;
    int                 eid;
    off_t       st_size;

    ibuf += 2;

    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));
    ibuf += sizeof( ofrefnum );

    memcpy(&bitmap, ibuf, sizeof(bitmap));
    bitmap = ntohs(bitmap);
    ibuf += sizeof( bitmap );

    *rbuflen = 0;
    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_setforkparams: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    if (ofork->of_vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    if ((ofork->of_flags & AFPFORK_ACCWR) == 0)
        return AFPERR_ACCESS;

    if ( ofork->of_flags & AFPFORK_DATA) {
        eid = ADEID_DFORK;
    } else if (ofork->of_flags & AFPFORK_RSRC) {
        eid = ADEID_RFORK;
    } else
        return AFPERR_PARAM;

    if ( ( (bitmap & ( (1<<FILPBIT_DFLEN) | (1<<FILPBIT_EXTDFLEN) ))
           && eid == ADEID_RFORK
             ) ||
         ( (bitmap & ( (1<<FILPBIT_RFLEN) | (1<<FILPBIT_EXTRFLEN) ))
           && eid == ADEID_DFORK)) {
        return AFPERR_BITMAP;
    }

    is64 = 0;
    if ((bitmap & ( (1<<FILPBIT_EXTDFLEN) | (1<<FILPBIT_EXTRFLEN) ))) {
        if (obj->afp_version >= 30) {
            is64 = 4;
        }
        else
            return AFPERR_BITMAP;
    }

    if (ibuflen < 2+ sizeof(ofrefnum) + sizeof(bitmap) + is64 +4)
        return AFPERR_PARAM ;

    size = get_off_t(&ibuf, is64);

    if (size < 0)
        return AFPERR_PARAM; /* Some MacOS don't return an error they just don't change the size! */


    if (bitmap == (1<<FILPBIT_DFLEN) || bitmap == (1<<FILPBIT_EXTDFLEN)) {
        st_size = ad_size(ofork->of_ad, eid);
        err = -2;
        if (st_size > size &&
            ad_tmplock(ofork->of_ad, eid, ADLOCK_WR, size, st_size -size, ofork->of_refnum) < 0)
            goto afp_setfork_err;

        err = ad_dtruncate( ofork->of_ad, size );
        if (st_size > size)
            ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, size, st_size -size, ofork->of_refnum);
        if (err < 0)
            goto afp_setfork_err;
    } else if (bitmap == (1<<FILPBIT_RFLEN) || bitmap == (1<<FILPBIT_EXTRFLEN)) {
        ad_refresh(NULL, ofork->of_ad );

        st_size = ad_size(ofork->of_ad, eid);
        err = -2;
        if (st_size > size &&
            ad_tmplock(ofork->of_ad, eid, ADLOCK_WR, size, st_size -size, ofork->of_refnum) < 0) {
            goto afp_setfork_err;
        }

        err = ad_rtruncate(ofork->of_ad, mtoupath(ofork->of_vol, of_name(ofork), ofork->of_did, utf8_encoding(obj)), size);
        if (st_size > size)
            ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, size, st_size -size, ofork->of_refnum);
        if (err < 0)
            goto afp_setfork_err;

        if (ad_flush( ofork->of_ad ) < 0) {
            LOG(log_error, logtype_afpd, "afp_setforkparams(%s): ad_flush: %s", of_name(ofork), strerror(errno) );
            return AFPERR_PARAM;
        }
    } else
        return AFPERR_BITMAP;

#ifdef AFS
    if ( flushfork( ofork ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_setforkparams(%s): flushfork: %s", of_name(ofork), strerror(errno) );
    }
#endif /* AFS */

    return( AFP_OK );

afp_setfork_err:
    if (err == -2)
        return AFPERR_LOCK;
    else {
        switch (errno) {
        case EROFS:
            return AFPERR_VLOCK;
        case EPERM:
        case EACCES:
            return AFPERR_ACCESS;
        case EDQUOT:
        case EFBIG:
        case ENOSPC:
            LOG(log_error, logtype_afpd, "afp_setforkparams: DISK FULL");
            return AFPERR_DFULL;
        default:
            return AFPERR_PARAM;
        }
    }
}

/* for this to work correctly, we need to check for locks before each
 * read and write. that's most easily handled by always doing an
 * appropriate check before each ad_read/ad_write. other things
 * that can change files like truncate are handled internally to those
 * functions.
 */
#define ENDBIT(a)  ((a) & 0x80)
#define UNLOCKBIT(a) ((a) & 0x01)


/* ---------------------- */
static int byte_lock(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen, int is64)
{
    struct ofork    *ofork;
    off_t               offset, length;
    int                 eid;
    uint16_t       ofrefnum;
    uint8_t            flags;
    int                 lockop;

    *rbuflen = 0;

    /* figure out parameters */
    ibuf++;
    flags = *ibuf; /* first bit = endflag, lastbit = lockflag */
    ibuf++;
    memcpy(&ofrefnum, ibuf, sizeof(ofrefnum));
    ibuf += sizeof(ofrefnum);

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "byte_lock: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    if ( ofork->of_flags & AFPFORK_DATA) {
        eid = ADEID_DFORK;
    } else if (ofork->of_flags & AFPFORK_RSRC) {
        eid = ADEID_RFORK;
    } else
        return AFPERR_PARAM;

    offset = get_off_t(&ibuf, is64);
    length = get_off_t(&ibuf, is64);

    if (length == -1)
        length = BYTELOCK_MAX;
    else if (!length || is_neg(is64, length)) {
        return AFPERR_PARAM;
    } else if ((length >= AD_FILELOCK_BASE) && -1 == (ad_reso_fileno(ofork->of_ad))) { /* HF ?*/
        return AFPERR_LOCK;
    }

    if (ENDBIT(flags)) {
        offset += ad_size(ofork->of_ad, eid);
        /* FIXME what do we do if file size > 2 GB and
           it's not byte_lock_ext?
        */
    }
    if (offset < 0)    /* error if we have a negative offset */
        return AFPERR_PARAM;

    /* if the file is a read-only file, we use read locks instead of
     * write locks. that way, we can prevent anyone from initiating
     * a write lock. */
    lockop = UNLOCKBIT(flags) ? ADLOCK_CLR : ADLOCK_WR;
    if (ad_lock(ofork->of_ad, eid, lockop, offset, length,
                ofork->of_refnum) < 0) {
        switch (errno) {
        case EACCES:
        case EAGAIN:
            return UNLOCKBIT(flags) ? AFPERR_NORANGE : AFPERR_LOCK;
            break;
        case ENOLCK:
            return AFPERR_NLOCK;
            break;
        case EINVAL:
            return UNLOCKBIT(flags) ? AFPERR_NORANGE : AFPERR_RANGEOVR;
            break;
        case EBADF:
        default:
            return AFPERR_PARAM;
            break;
        }
    }
    *rbuflen = set_off_t (offset, rbuf, is64);
    return( AFP_OK );
}

/* --------------------------- */
int afp_bytelock(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return byte_lock ( obj, ibuf, ibuflen, rbuf, rbuflen , 0);
}

/* --------------------------- */
int afp_bytelock_ext(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return byte_lock ( obj, ibuf, ibuflen, rbuf, rbuflen , 1);
}

#undef UNLOCKBIT

/*!
 * Read *rbuflen bytes from fork at offset
 *
 * @param ofork    (r)  fork handle
 * @param eid      (r)  data fork or ressource fork entry id
 * @param offset   (r)  offset
 * @param rbuf     (r)  data buffer
 * @param rbuflen  (rw) in: number of bytes to read, out: bytes read
 *
 * @return         AFP status code
 */
static int read_file(const struct ofork *ofork, int eid, off_t offset, char *rbuf, size_t *rbuflen)
{
    ssize_t cc;

    cc = ad_read(ofork->of_ad, eid, offset, rbuf, *rbuflen);
    if ( cc < 0 ) {
        LOG(log_error, logtype_afpd, "afp_read(%s): ad_read: %s", of_name(ofork), strerror(errno) );
        *rbuflen = 0;
        return( AFPERR_PARAM );
    }
    *rbuflen = cc;

    if ((size_t)cc < *rbuflen)
        return AFPERR_EOF;
    return AFP_OK;
}

static int read_fork(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen, int is64)
{
    DSI          *dsi = obj->dsi;
    struct ofork *ofork;
    off_t        offset, saveoff, reqcount, savereqcount, size;
    ssize_t      cc, err;
    int          eid;
    uint16_t     ofrefnum;

    /* we break the AFP spec here by not supporting nlmask and nlchar anymore */

    ibuf += 2;
    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));
    ibuf += sizeof( u_short );

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_read: of_find(%d) could not locate fork", ofrefnum );
        err = AFPERR_PARAM;
        goto afp_read_err;
    }

    if ((ofork->of_flags & AFPFORK_ACCRD) == 0) {
        err = AFPERR_ACCESS;
        goto afp_read_err;
    }

    if ( ofork->of_flags & AFPFORK_DATA) {
        eid = ADEID_DFORK;
    } else if (ofork->of_flags & AFPFORK_RSRC) {
        eid = ADEID_RFORK;
    } else { /* fork wasn't opened. this should never really happen. */
        err = AFPERR_ACCESS;
        goto afp_read_err;
    }

    offset   = get_off_t(&ibuf, is64);
    reqcount = get_off_t(&ibuf, is64);

    /* zero request count */
    err = AFP_OK;
    if (!reqcount) {
        goto afp_read_err;
    }

    AFP_READ_START((long)reqcount);

    /* reqcount isn't always truthful. we need to deal with that. */
    size = ad_size(ofork->of_ad, eid);

    LOG(log_debug, logtype_afpd,
        "afp_read(fork: %" PRIu16 " [%s], off: %" PRIu64 ", len: %" PRIu64 ", size: %" PRIu64 ")",
        ofork->of_refnum, (ofork->of_flags & AFPFORK_DATA) ? "data" : "reso", offset, reqcount, size);

    if (offset >= size) {
        err = AFPERR_EOF;
        goto afp_read_err;
    }

    /* subtract off the offset */
    if (reqcount + offset > size) {
        reqcount = size - offset;
        err = AFPERR_EOF;
    }

    savereqcount = reqcount;
    saveoff = offset;

    if (reqcount < 0 || offset < 0) {
        err = AFPERR_PARAM;
        goto afp_read_err;
    }

    if (obj->options.flags & OPTION_AFP_READ_LOCK) {
        if (ad_tmplock(ofork->of_ad, eid, ADLOCK_RD, offset, reqcount, ofork->of_refnum) < 0) {
            err = AFPERR_LOCK;
            goto afp_read_err;
        }
    }

#ifdef WITH_SENDFILE
    if (!(eid == ADEID_DFORK && ad_data_fileno(ofork->of_ad) == AD_SYMLINK) &&
        !(obj->options.flags & OPTION_NOSENDFILE)) {
        int fd = ad_readfile_init(ofork->of_ad, eid, &offset, 0);
        if (dsi_stream_read_file(dsi, fd, offset, reqcount, err) < 0) {
            LOG(log_error, logtype_afpd, "afp_read(%s): ad_readfile: %s",
                of_name(ofork), strerror(errno));
            goto afp_read_exit;
        }
        goto afp_read_done;
    }
#endif

    *rbuflen = MIN(reqcount, dsi->server_quantum);

    cc = read_file(ofork, eid, offset, ibuf, rbuflen);
    if (cc < 0) {
        err = cc;
        goto afp_read_done;
    }

    LOG(log_debug, logtype_afpd,
        "afp_read(name: \"%s\", offset: %jd, reqcount: %jd): got %jd bytes from file",
        of_name(ofork), (intmax_t)offset, (intmax_t)reqcount, (intmax_t)*rbuflen);

    offset += *rbuflen;

    /*
     * dsi_readinit() returns size of next read buffer. by this point,
     * we know that we're sending some data. if we fail, something
     * horrible happened.
     */
    if ((cc = dsi_readinit(dsi, ibuf, *rbuflen, reqcount, err)) < 0)
        goto afp_read_exit;
    *rbuflen = cc;

    while (*rbuflen > 0) {
        /*
         * This loop isn't really entered anymore, we've already
         * sent the whole requested block in dsi_readinit().
         */
        cc = read_file(ofork, eid, offset, ibuf, rbuflen);
        if (cc < 0)
            goto afp_read_exit;

        offset += *rbuflen;
        /* dsi_read() also returns buffer size of next allocation */
        cc = dsi_read(dsi, ibuf, *rbuflen); /* send it off */
        if (cc < 0)
            goto afp_read_exit;
        *rbuflen = cc;
    }
    dsi_readdone(dsi);
    goto afp_read_done;

afp_read_exit:
    LOG(log_error, logtype_afpd, "afp_read(%s): %s", of_name(ofork), strerror(errno));
    dsi_readdone(dsi);
   if (obj->options.flags & OPTION_AFP_READ_LOCK)
       ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, savereqcount, ofork->of_refnum);
    obj->exit(EXITERR_CLNT);

afp_read_done:
    if (obj->options.flags & OPTION_AFP_READ_LOCK)
        ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, savereqcount, ofork->of_refnum);

    AFP_READ_DONE();
    return err;

afp_read_err:
    *rbuflen = 0;
    return err;
}

/* ---------------------- */
int afp_read(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return read_fork(obj, ibuf, ibuflen, rbuf, rbuflen, 0);
}

/* ---------------------- */
int afp_read_ext(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return read_fork(obj, ibuf, ibuflen, rbuf, rbuflen, 1);
}

/* ---------------------- */
int afp_flush(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol *vol;
    uint16_t vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    of_flush(vol);
    return( AFP_OK );
}

int afp_flushfork(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct ofork    *ofork;
    uint16_t       ofrefnum;

    *rbuflen = 0;
    ibuf += 2;
    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_flushfork: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    LOG(log_debug, logtype_afpd, "afp_flushfork(fork: %s)",
        (ofork->of_flags & AFPFORK_DATA) ? "d" : "r");

    if ( flushfork( ofork ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_flushfork(%s): %s", of_name(ofork), strerror(errno) );
    }

    return( AFP_OK );
}

/*
  FIXME
  There is a lot to tell about fsync, fdatasync, F_FULLFSYNC.
  fsync(2) on OSX is implemented differently than on other platforms.
  see: http://mirror.linux.org.au/pub/linux.conf.au/2007/video/talks/278.pdf.
*/
int afp_syncfork(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct ofork        *ofork;
    uint16_t           ofrefnum;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&ofrefnum, ibuf, sizeof(ofrefnum));
    ibuf += sizeof( ofrefnum );

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afpd_syncfork: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    LOG(log_debug, logtype_afpd, "afp_syncfork(fork: %s)",
        (ofork->of_flags & AFPFORK_DATA) ? "d" : "r");

    if ( flushfork( ofork ) < 0 ) {
        LOG(log_error, logtype_afpd, "flushfork(%s): %s", of_name(ofork), strerror(errno) );
        return AFPERR_MISC;
    }

    return( AFP_OK );
}

/* this is very similar to closefork */
int flushfork(struct ofork *ofork)
{
    struct timeval tv;

    int err = 0, doflush = 0;

    if ( ad_data_fileno( ofork->of_ad ) != -1 &&
         fsync( ad_data_fileno( ofork->of_ad )) < 0 ) {
        LOG(log_error, logtype_afpd, "flushfork(%s): dfile(%d) %s",
            of_name(ofork), ad_data_fileno(ofork->of_ad), strerror(errno) );
        err = -1;
    }

    if ( ad_reso_fileno( ofork->of_ad ) != -1 &&  /* HF */
         (ofork->of_flags & AFPFORK_RSRC)) {

        /* read in the rfork length */
        ad_refresh(NULL, ofork->of_ad);

        /* set the date if we're dirty */
        if ((ofork->of_flags & AFPFORK_DIRTY) && !gettimeofday(&tv, NULL)) {
            ad_setdate(ofork->of_ad, AD_DATE_MODIFY|AD_DATE_UNIX, tv.tv_sec);
            ofork->of_flags &= ~AFPFORK_DIRTY;
            doflush++;
        }

        /* flush the header */
        if (doflush && ad_flush(ofork->of_ad) < 0)
            err = -1;

        if (fsync( ad_reso_fileno( ofork->of_ad )) < 0)
            err = -1;

        if (err < 0)
            LOG(log_error, logtype_afpd, "flushfork(%s): hfile(%d) %s",
                of_name(ofork), ad_reso_fileno(ofork->of_ad), strerror(errno) );
    }

    return( err );
}

int afp_closefork(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct ofork    *ofork;
    uint16_t       ofrefnum;

    *rbuflen = 0;
    ibuf += 2;
    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_closefork: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    LOG(log_debug, logtype_afpd, "afp_closefork(fork: %" PRIu16 " [%s])",
        ofork->of_refnum, (ofork->of_flags & AFPFORK_DATA) ? "data" : "rsrc");

    if (of_closefork(obj, ofork) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_closefork: of_closefork: %s", strerror(errno) );
        return( AFPERR_PARAM );
    }

    return( AFP_OK );
}


static ssize_t write_file(struct ofork *ofork, int eid,
                          off_t offset, char *rbuf,
                          size_t rbuflen)
{
    ssize_t cc;

    LOG(log_debug, logtype_afpd, "write_file(off: %ju, size: %zu)",
        (uintmax_t)offset, rbuflen);

    if (( cc = ad_write(ofork->of_ad, eid, offset, 0,
                        rbuf, rbuflen)) < 0 ) {
        switch ( errno ) {
        case EDQUOT :
        case EFBIG :
        case ENOSPC :
            LOG(log_error, logtype_afpd, "write_file: DISK FULL");
            return( AFPERR_DFULL );
        case EACCES:
            return AFPERR_ACCESS;
        default :
            LOG(log_error, logtype_afpd, "afp_write(%s): ad_write: %s", of_name(ofork), strerror(errno) );
            return( AFPERR_PARAM );
        }
    }

    return cc;
}


/*
 * FPWrite. NOTE: on an error, we always use afp_write_err as
 * the client may have sent us a bunch of data that's not reflected
 * in reqcount et al.
 */
static int write_fork(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen, int is64)
{
    struct ofork    *ofork;
    off_t           offset, saveoff, reqcount, oldsize, newsize;
    int             endflag, eid, err = AFP_OK;
    uint16_t        ofrefnum;
    ssize_t         cc;
    DSI             *dsi = obj->dsi;
    char            *rcvbuf = (char *)dsi->commands;
    size_t          rcvbuflen = dsi->server_quantum;

    /* figure out parameters */
    ibuf++;
    endflag = ENDBIT(*ibuf);
    ibuf++;
    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));
    ibuf += sizeof( ofrefnum );

    offset   = get_off_t(&ibuf, is64);
    reqcount = get_off_t(&ibuf, is64);

    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_write: of_find(%d) could not locate fork", ofrefnum );
        err = AFPERR_PARAM;
        goto afp_write_err;
    }

    LOG(log_debug, logtype_afpd, "afp_write(fork: %" PRIu16 " [%s], off: %" PRIu64 ", size: %" PRIu64 ")",
        ofork->of_refnum, (ofork->of_flags & AFPFORK_DATA) ? "data" : "reso", offset, reqcount);

    if ((ofork->of_flags & AFPFORK_ACCWR) == 0) {
        err = AFPERR_ACCESS;
        goto afp_write_err;
    }

#ifdef AFS
    writtenfork = ofork;
#endif /* AFS */

    if ( ofork->of_flags & AFPFORK_DATA) {
        eid = ADEID_DFORK;
    } else if (ofork->of_flags & AFPFORK_RSRC) {
        eid = ADEID_RFORK;
    } else {
        err = AFPERR_ACCESS; /* should never happen */
        goto afp_write_err;
    }

    oldsize = ad_size(ofork->of_ad, eid);
    if (endflag)
        offset += oldsize;

    /* handle bogus parameters */
    if (reqcount < 0 || offset < 0) {
        err = AFPERR_PARAM;
        goto afp_write_err;
    }

    newsize = ((offset + reqcount) > oldsize) ? (offset + reqcount) : oldsize;

    /* offset can overflow on 64-bit capable filesystems.
     * report disk full if that's going to happen. */
    if (sum_neg(is64, offset, reqcount)) {
        LOG(log_error, logtype_afpd, "write_fork: DISK FULL");
        err = AFPERR_DFULL;
        goto afp_write_err;
    }

    if (!reqcount) { /* handle request counts of 0 */
        err = AFP_OK;
        *rbuflen = set_off_t (offset, rbuf, is64);
        goto afp_write_err;
    }

    AFP_WRITE_START((long)reqcount);

    saveoff = offset;
    if (obj->options.flags & OPTION_AFP_READ_LOCK) {
        if (ad_tmplock(ofork->of_ad, eid, ADLOCK_WR, saveoff, reqcount, ofork->of_refnum) < 0) {
            err = AFPERR_LOCK;
            goto afp_write_err;
        }
    }

    /* find out what we have already */
    cc = dsi_writeinit(dsi, rcvbuf, rcvbuflen);

    if (!cc || (cc = write_file(ofork, eid, offset, rcvbuf, cc)) < 0) {
        dsi_writeflush(dsi);
        *rbuflen = 0;
        if (obj->options.flags & OPTION_AFP_READ_LOCK)
            ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, reqcount, ofork->of_refnum);
        return cc;
    }

    offset += cc;

#if 0 /*def HAVE_SENDFILE_WRITE*/
    if ((cc = ad_writefile(ofork->of_ad, eid, dsi->socket, offset, dsi->datasize)) < 0) {
        switch (errno) {
        case EDQUOT:
        case EFBIG:
        case ENOSPC:
            cc = AFPERR_DFULL;
            break;
        default:
            LOG(log_error, logtype_afpd, "afp_write: ad_writefile: %s", strerror(errno) );
            goto afp_write_loop;
        }
        dsi_writeflush(dsi);
        *rbuflen = 0;
        if (obj->options.flags & OPTION_AFP_READ_LOCK)
            ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, reqcount,  ofork->of_refnum);
        return cc;
    }

    offset += cc;
    goto afp_write_done;
#endif /* 0, was HAVE_SENDFILE_WRITE */

    /* loop until everything gets written. currently
     * dsi_write handles the end case by itself. */
    while ((cc = dsi_write(dsi, rcvbuf, rcvbuflen))) {

        if ((cc = write_file(ofork, eid, offset, rcvbuf, cc)) < 0) {
            dsi_writeflush(dsi);
            *rbuflen = 0;
            if (obj->options.flags & OPTION_AFP_READ_LOCK)
                ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, reqcount,  ofork->of_refnum);
            return cc;
        }

        LOG(log_debug, logtype_afpd, "afp_write: wrote: %jd, offset: %jd",
            (intmax_t)cc, (intmax_t)offset);

        offset += cc;
    }

    if (obj->options.flags & OPTION_AFP_READ_LOCK)
        ad_tmplock(ofork->of_ad, eid, ADLOCK_CLR, saveoff, reqcount,  ofork->of_refnum);
    if ( ad_meta_fileno( ofork->of_ad ) != -1 ) /* META */
        ofork->of_flags |= AFPFORK_DIRTY;

    /* we have modified any fork, remember until close_fork */
    ofork->of_flags |= AFPFORK_MODIFIED;

    /* update write count */
    ofork->of_vol->v_appended += (newsize > oldsize) ? (newsize - oldsize) : 0;

    *rbuflen = set_off_t (offset, rbuf, is64);
    AFP_WRITE_DONE();
    return( AFP_OK );

afp_write_err:
    dsi_writeinit(dsi, rcvbuf, rcvbuflen);
    dsi_writeflush(dsi);

    if (err != AFP_OK) {
        *rbuflen = 0;
    }
    return err;
}

/* ---------------------------- */
int afp_write(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return write_fork(obj, ibuf, ibuflen, rbuf, rbuflen, 0);
}

/* ----------------------------
 * FIXME need to deal with SIGXFSZ signal
 */
int afp_write_ext(AFPObj *obj, char *ibuf, size_t ibuflen, char *rbuf, size_t *rbuflen)
{
    return write_fork(obj, ibuf, ibuflen, rbuf, rbuflen, 1);
}

/* ---------------------------- */
int afp_getforkparams(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct ofork    *ofork;
    int             ret;
    uint16_t       ofrefnum, bitmap;
    size_t          buflen;
    ibuf += 2;
    memcpy(&ofrefnum, ibuf, sizeof( ofrefnum ));
    ibuf += sizeof( ofrefnum );
    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    *rbuflen = 0;
    if (NULL == ( ofork = of_find( ofrefnum )) ) {
        LOG(log_error, logtype_afpd, "afp_getforkparams: of_find(%d) could not locate fork", ofrefnum );
        return( AFPERR_PARAM );
    }

    if (AD_META_OPEN(ofork->of_ad)) {
        if ( ad_refresh(NULL, ofork->of_ad ) < 0 ) {
            LOG(log_error, logtype_afpd, "getforkparams(%s): ad_refresh: %s", of_name(ofork), strerror(errno) );
            return( AFPERR_PARAM );
        }
    }

    if (AFP_OK != (ret = getforkparams(obj, ofork, bitmap, rbuf + sizeof( u_short ), &buflen ))) {
        return( ret );
    }

    *rbuflen = buflen + sizeof( u_short );
    bitmap = htons( bitmap );
    memcpy(rbuf, &bitmap, sizeof( bitmap ));
    return( AFP_OK );
}

