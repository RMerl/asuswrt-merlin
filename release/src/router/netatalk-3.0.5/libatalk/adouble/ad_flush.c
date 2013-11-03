/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * Copyright (c) 2010      Frank Lahm
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/adouble.h>
#include <atalk/ea.h>
#include <atalk/logger.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/errchk.h>
#include <atalk/util.h>

#include "ad_lock.h"

static const uint32_t set_eid[] = {
    0,1,2,3,4,5,6,7,8,
    9,10,11,12,13,14,15,
    AD_DEV, AD_INO, AD_SYN, AD_ID
};

#define EID_DISK(a) (set_eid[a])

/*
 * Prepare ad->ad_data buffer from struct adouble for writing on disk
 */
int ad_rebuild_adouble_header_v2(struct adouble *ad)
{
    uint32_t       eid;
    uint32_t       temp;
    uint16_t       nent;
    char        *buf, *nentp;

    LOG(log_debug, logtype_ad, "ad_rebuild_adouble_header_v2");

    buf = ad->ad_data;

    temp = htonl( ad->ad_magic );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl( ad->ad_version );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    buf += sizeof( ad->ad_filler );

    nentp = buf;
    buf += sizeof( nent );
    for ( eid = 0, nent = 0; eid < ADEID_MAX; eid++ ) {
        if (ad->ad_eid[ eid ].ade_off == 0)
            continue;
        temp = htonl( EID_DISK(eid) );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );

        temp = htonl( ad->ad_eid[ eid ].ade_off );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );

        temp = htonl( ad->ad_eid[ eid ].ade_len );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );
        nent++;
    }
    nent = htons( nent );
    memcpy(nentp, &nent, sizeof( nent ));

    return ad_getentryoff(ad, ADEID_RFORK);
}

int ad_rebuild_adouble_header_ea(struct adouble *ad)
{
    uint32_t       eid;
    uint32_t       temp;
    uint16_t       nent;
    char        *buf, *nentp;

    LOG(log_debug, logtype_ad, "ad_rebuild_adouble_header_ea");

    buf = ad->ad_data;

    temp = htonl( ad->ad_magic );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl( ad->ad_version );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    buf += sizeof( ad->ad_filler );

    nentp = buf;
    buf += sizeof( nent );
    for ( eid = 0, nent = 0; eid < ADEID_MAX; eid++ ) {
        if ((ad->ad_eid[ eid ].ade_off == 0) || (eid == ADEID_RFORK))
            continue;
        temp = htonl( EID_DISK(eid) );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );

        temp = htonl( ad->ad_eid[ eid ].ade_off );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );

        temp = htonl( ad->ad_eid[ eid ].ade_len );
        memcpy(buf, &temp, sizeof( temp ));
        buf += sizeof( temp );
        nent++;
    }
    nent = htons( nent );
    memcpy(nentp, &nent, sizeof( nent ));

    return AD_DATASZ_EA;
}

/*!
 * Prepare adbuf buffer from struct adouble for writing on disk
 */
static int ad_rebuild_adouble_header_osx(struct adouble *ad, char *adbuf)
{
    uint32_t       temp;
    uint16_t       nent;
    char           *buf;

    LOG(log_debug, logtype_ad, "ad_rebuild_adouble_header_osx");

    buf = &adbuf[0];

    temp = htonl( ad->ad_magic );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl( ad->ad_version );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    memcpy(buf, "Netatalk        ", 16);
    buf += sizeof( ad->ad_filler );

    nent = htons(ADEID_NUM_OSX);
    memcpy(buf, &nent, sizeof( nent ));
    buf += sizeof( nent );

    /* FinderInfo */
    temp = htonl(EID_DISK(ADEID_FINDERI));
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl(ADEDOFF_FINDERI_OSX);
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl(ADEDLEN_FINDERI);
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    memcpy(adbuf + ADEDOFF_FINDERI_OSX, ad_entry(ad, ADEID_FINDERI), ADEDLEN_FINDERI);

    /* rfork */
    temp = htonl( EID_DISK(ADEID_RFORK) );
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl(ADEDOFF_RFORK_OSX);
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    temp = htonl( ad->ad_rlen);
    memcpy(buf, &temp, sizeof( temp ));
    buf += sizeof( temp );

    return AD_DATASZ_OSX;
}

/* -------------------
 * XXX copy only header with same size or comment
 * doesn't work well for adouble with different version.
 *
 */
int ad_copy_header(struct adouble *add, struct adouble *ads)
{
    uint32_t       eid;
    uint32_t       len;

    for ( eid = 0; eid < ADEID_MAX; eid++ ) {
        if ( ads->ad_eid[ eid ].ade_off == 0 || add->ad_eid[ eid ].ade_off == 0 )
            continue;

        len = ads->ad_eid[ eid ].ade_len;
        if (!len || len != add->ad_eid[ eid ].ade_len)
            continue;

        switch (eid) {
        case ADEID_COMMENT:
        case ADEID_PRIVDEV:
        case ADEID_PRIVINO:
        case ADEID_PRIVSYN:
        case ADEID_PRIVID:
            continue;
        default:
            ad_setentrylen( add, eid, len );
            memcpy( ad_entry( add, eid ), ad_entry( ads, eid ), len );
        }
    }
    add->ad_rlen = ads->ad_rlen;

    if (((ads->ad_vers == AD_VERSION2) && (add->ad_vers == AD_VERSION_EA))
        || ((ads->ad_vers == AD_VERSION_EA) && (add->ad_vers == AD_VERSION2))) {
        cnid_t id;
        memcpy(&id, ad_entry(add, ADEID_PRIVID), sizeof(cnid_t));
        id = htonl(id);
        memcpy(ad_entry(add, ADEID_PRIVID), &id, sizeof(cnid_t));
    }
    return 0;
}

static int ad_flush_hf(struct adouble *ad)
{
    EC_INIT;
    int len;
    int cwd = -1;

    LOG(log_debug, logtype_ad, "ad_flush_hf(%s)", adflags2logstr(ad->ad_adflags));

    struct ad_fd *adf;

    switch (ad->ad_vers) {
    case AD_VERSION2:
        adf = ad->ad_mdp;
        break;
    case AD_VERSION_EA:
        adf = &ad->ad_data_fork;
        break;
    default:
        LOG(log_error, logtype_ad, "ad_flush: unexpected adouble version");
        return -1;
    }

    if ((adf->adf_flags & O_RDWR)
        || ((ad->ad_adflags & ADFLAGS_DIR) && (ad->ad_adflags & ADFLAGS_RDWR))) {
        if (ad_getentryoff(ad, ADEID_RFORK)) {
            if (ad->ad_rlen > 0xffffffff)
                ad_setentrylen(ad, ADEID_RFORK, 0xffffffff);
            else
                ad_setentrylen(ad, ADEID_RFORK, ad->ad_rlen);
        }
        len = ad->ad_ops->ad_rebuild_header(ad);

        switch (ad->ad_vers) {
        case AD_VERSION2:
            if (adf_pwrite(ad->ad_mdp, ad->ad_data, len, 0) != len) {
                if (errno == 0)
                    errno = EIO;
                return( -1 );
            }
            break;
        case AD_VERSION_EA:
            if (AD_META_OPEN(ad)) {
                if (ad->ad_adflags & ADFLAGS_DIR) {
                    EC_NEG1_LOG( cwd = open(".", O_RDONLY) );
                    EC_NEG1_LOG( fchdir(ad_data_fileno(ad)) );
                    EC_ZERO_LOGSTR( sys_lsetxattr(".", AD_EA_META, ad->ad_data, AD_DATASZ_EA, 0),
                                    "sys_lsetxattr(\"%s\"): %s", fullpathname(".") ,strerror(errno));
                    EC_NEG1_LOG( fchdir(cwd) );
                    EC_NEG1_LOG( close(cwd) );
                    cwd = -1;
                } else {
                    EC_ZERO_LOG( sys_fsetxattr(ad_data_fileno(ad), AD_EA_META, ad->ad_data, AD_DATASZ_EA, 0) );
                }
            }
            break;
        default:
            LOG(log_error, logtype_ad, "ad_flush: unexpected adouble version");
            return -1;
        }
    }

EC_CLEANUP:
    if (cwd != -1) {
        if (fchdir(cwd) != 0) {
            AFP_PANIC("ad_flush: cant fchdir");
        }
        close(cwd);
    }
    EC_EXIT;
}

/* Flush resofork adouble file if any (currently adouble:ea and #ifndef HAVE_EAFD eg Linux) */
static int ad_flush_rf(struct adouble *ad)
{
    ssize_t len;
    char    adbuf[AD_DATASZ_OSX];

#ifdef HAVE_EAFD
    return 0;
#endif
    if (ad->ad_vers != AD_VERSION_EA)
        return 0;

    LOG(log_debug, logtype_ad, "ad_flush_rf(%s)", adflags2logstr(ad->ad_adflags));

    if ((ad->ad_rfp->adf_flags & O_RDWR)) {
        if (ad_getentryoff(ad, ADEID_RFORK)) {
            if (ad->ad_rlen > 0xffffffff)
                ad_setentrylen(ad, ADEID_RFORK, 0xffffffff);
            else
                ad_setentrylen(ad, ADEID_RFORK, ad->ad_rlen);
        }
        len = ad_rebuild_adouble_header_osx(ad, &adbuf[0]);

        if (adf_pwrite(ad->ad_rfp, adbuf, len, 0) != len) {
            if (errno == 0)
                errno = EIO;
            return -1;
        }
    }
    return 0;
}

int ad_flush(struct adouble *ad)
{
    EC_INIT;

    LOG(log_debug, logtype_ad, "ad_flush(%s)", adflags2logstr(ad->ad_adflags));

    if (AD_META_OPEN(ad)) {
        EC_ZERO( ad_flush_hf(ad) );
    }

    if (AD_RSRC_OPEN(ad)) {
        EC_ZERO( ad_flush_rf(ad) );
    }

EC_CLEANUP:
    EC_EXIT;
}

static int ad_data_closefd(struct adouble *ad)
{
    int ret = 0;

    if (ad_data_fileno(ad) == AD_SYMLINK) {
        free(ad->ad_data_fork.adf_syml);
        ad->ad_data_fork.adf_syml = NULL;
    } else {
        if (close(ad_data_fileno(ad)) < 0)
            ret = -1;
    }
    ad_data_fileno(ad) = -1;
    return ret;
}

/*!
 * Close a struct adouble freeing all resources
 */
int ad_close(struct adouble *ad, int adflags)
{
    int err = 0;

    if (ad == NULL)
        return err;


    LOG(log_debug, logtype_ad,
        "ad_close(%s): BEGIN: {d: %d, m: %d, r: %d} "
        "[dfd: %d (ref: %d), mfd: %d (ref: %d), rfd: %d (ref: %d)]",
        adflags2logstr(adflags),
        ad->ad_data_refcount, ad->ad_meta_refcount, ad->ad_reso_refcount,
        ad_data_fileno(ad), ad->ad_data_fork.adf_refcount,
        ad_meta_fileno(ad), ad->ad_mdp->adf_refcount,
        ad_reso_fileno(ad), ad->ad_rfp->adf_refcount);

    if (adflags & (ADFLAGS_SETSHRMD | ADFLAGS_CHECK_OF)) {
        /* sharemode locks are stored in the data fork, adouble:v2 needs this extra handling */
        adflags |= ADFLAGS_DF;
    }

    if ((ad->ad_vers == AD_VERSION2) && (adflags & ADFLAGS_RF))
        adflags |= ADFLAGS_HF;

    if ((adflags & ADFLAGS_DF) && (ad_data_fileno(ad) >= 0 || ad_data_fileno(ad) == AD_SYMLINK)) {        
        if (ad->ad_data_refcount)
            if (--ad->ad_data_refcount == 0)
                adf_lock_free(&ad->ad_data_fork);                
        if (--ad->ad_data_fork.adf_refcount == 0) {
            if (ad_data_closefd(ad) < 0)
                err = -1;
        }
    }

    if ((adflags & ADFLAGS_HF) && (ad_meta_fileno(ad) != -1)) {
        if (ad->ad_meta_refcount)
            ad->ad_meta_refcount--;
        if (!(--ad->ad_mdp->adf_refcount)) {
            if (close( ad_meta_fileno(ad)) < 0)
                err = -1;
            ad_meta_fileno(ad) = -1;
        }
    }

    if (adflags & ADFLAGS_RF) {
        if (ad->ad_reso_refcount)
            if (--ad->ad_reso_refcount == 0)
                adf_lock_free(ad->ad_rfp);
        if (ad->ad_vers == AD_VERSION_EA) {
            if ((ad_reso_fileno(ad) != -1)
                && !(--ad->ad_rfp->adf_refcount)) {
                if (close(ad->ad_rfp->adf_fd) < 0)
                    err = -1;
                ad->ad_rlen = 0;
                ad_reso_fileno(ad) = -1;
            }
        }
    }

    LOG(log_debug, logtype_ad,
        "ad_close(%s): END: %d {d: %d, m: %d, r: %d} "
        "[dfd: %d (ref: %d), mfd: %d (ref: %d), rfd: %d (ref: %d)]",
        adflags2logstr(adflags), err,
        ad->ad_data_refcount, ad->ad_meta_refcount, ad->ad_reso_refcount,
        ad_data_fileno(ad), ad->ad_data_fork.adf_refcount,
        ad_meta_fileno(ad), ad->ad_mdp->adf_refcount,
        ad_reso_fileno(ad), ad->ad_rfp->adf_refcount);

    return err;
}
