/*
 * Copyright (c) 2012 Frank Lahm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*!
 * @file
 * Part of Netatalk's AppleDouble implementatation
 * @sa include/atalk/adouble.h
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/adouble.h>
#include <atalk/util.h>
#include <atalk/unix.h>
#include <atalk/ea.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>
#include <atalk/volume.h>

#include "ad_lock.h"

static char emptyfilad[32] = {0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,0,0};

static char emptydirad[32] = {0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,1,0,
                              0,0,0,0,0,0,0,0,
                              0,0,0,0,0,0,0,0};

static int ad_conv_v22ea_hf(const char *path, const struct stat *sp, const struct vol *vol)
{
    EC_INIT;
    struct adouble adv2;
    struct adouble adea;
    int adflags;
    uint32_t ctime, mtime, afpinfo = 0;
    char *emptyad;

    LOG(log_debug, logtype_ad,"ad_conv_v22ea_hf(\"%s\"): BEGIN", fullpathname(path));

    switch (S_IFMT & sp->st_mode) {
    case S_IFREG:
    case S_IFDIR:
        break;
    default:
        return 0;
    }

    ad_init(&adea, vol);
    ad_init_old(&adv2, AD_VERSION2, adea.ad_options);

    adflags = S_ISDIR(sp->st_mode) ? ADFLAGS_DIR : 0;

    /* Open and lock adouble:v2 file */
    EC_ZERO( ad_open(&adv2, path, adflags | ADFLAGS_HF | ADFLAGS_RDWR) );

    EC_NEG1_LOG( ad_tmplock(&adv2, ADEID_RFORK, ADLOCK_WR | ADLOCK_FILELOCK, 0, 0, 0) );
    EC_NEG1_LOG( adv2.ad_ops->ad_header_read(path, &adv2, sp) );

    /* Check if it's a non-empty header */
    if (S_ISREG(sp->st_mode))
        emptyad = &emptyfilad[0];
    else
        emptyad = &emptydirad[0];

    if (ad_getentrylen(&adv2, ADEID_COMMENT) != 0)
        goto copy;
    if (ad_getentryoff(&adv2, ADEID_FINDERI)
        && (ad_getentrylen(&adv2, ADEID_FINDERI) == ADEDLEN_FINDERI)
        && (memcmp(ad_entry(&adv2, ADEID_FINDERI), emptyad, ADEDLEN_FINDERI) != 0))
        goto copy;
    if (ad_getentryoff(&adv2, ADEID_FILEDATESI)) {
        EC_ZERO_LOG( ad_getdate(&adv2, AD_DATE_CREATE | AD_DATE_UNIX, &ctime) );
        EC_ZERO_LOG( ad_getdate(&adv2, AD_DATE_MODIFY | AD_DATE_UNIX, &mtime) );
        if ((ctime != mtime) || (mtime != sp->st_mtime))
            goto copy;
    }
    if (ad_getentryoff(&adv2, ADEID_AFPFILEI)) {
        if (memcmp(ad_entry(&adv2, ADEID_AFPFILEI), &afpinfo, ADEDLEN_AFPFILEI) != 0)
            goto copy;
    }

    LOG(log_debug, logtype_ad,"ad_conv_v22ea_hf(\"%s\"): default adouble", fullpathname(path), ret);
    goto EC_CLEANUP;

copy:
    /* Create a adouble:ea meta EA */
    LOG(log_debug, logtype_ad,"ad_conv_v22ea_hf(\"%s\"): copying adouble", fullpathname(path), ret);
    EC_ZERO_LOGSTR( ad_open(&adea, path, adflags | ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE),
                    "ad_conv_v22ea_hf(\"%s\"): error creating metadata EA: %s",
                    fullpathname(path), strerror(errno));
    EC_ZERO_LOG( ad_copy_header(&adea, &adv2) );
    ad_flush(&adea);

EC_CLEANUP:
    EC_ZERO_LOG( ad_close(&adv2, ADFLAGS_HF | ADFLAGS_SETSHRMD) );
    EC_ZERO_LOG( ad_close(&adea, ADFLAGS_HF | ADFLAGS_SETSHRMD) );
    LOG(log_debug, logtype_ad,"ad_conv_v22ea_hf(\"%s\"): END: %d", fullpathname(path), ret);
    EC_EXIT;
}

static int ad_conv_v22ea_rf(const char *path, const struct stat *sp, const struct vol *vol)
{
    EC_INIT;
    struct adouble adv2;
    struct adouble adea;

    LOG(log_debug, logtype_ad,"ad_conv_v22ea_rf(\"%s\"): BEGIN", fullpathname(path));

    switch (S_IFMT & sp->st_mode) {
    case S_IFREG:
        break;
    default:
        return 0;
    }

    if (S_ISDIR(sp->st_mode))
        return 0;

    ad_init(&adea, vol);
    ad_init_old(&adv2, AD_VERSION2, adea.ad_options);

    /* Open and lock adouble:v2 file */
    EC_ZERO( ad_open(&adv2, path, ADFLAGS_HF | ADFLAGS_RF | ADFLAGS_RDWR) );

    if (adv2.ad_rlen > 0) {
        EC_NEG1_LOG( ad_tmplock(&adv2, ADEID_RFORK, ADLOCK_WR | ADLOCK_FILELOCK, 0, 0, 0) );

        /* Create a adouble:ea resource fork */
        EC_ZERO_LOG( ad_open(&adea, path, ADFLAGS_HF | ADFLAGS_RF | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666) );

        EC_ZERO_LOG( copy_fork(ADEID_RFORK, &adea, &adv2) );
        adea.ad_rlen = adv2.ad_rlen;
        ad_flush(&adea);
        fchmod(ad_reso_fileno(&adea), sp->st_mode & 0666);
        fchown(ad_reso_fileno(&adea), sp->st_uid, sp->st_gid);
    }

EC_CLEANUP:
    EC_ZERO_LOG( ad_close(&adv2, ADFLAGS_HF | ADFLAGS_RF) );
    EC_ZERO_LOG( ad_close(&adea, ADFLAGS_HF | ADFLAGS_RF) );
    LOG(log_debug, logtype_ad,"ad_conv_v22ea_rf(\"%s\"): END: %d", fullpathname(path), ret);
    EC_EXIT;
}

static int ad_conv_v22ea(const char *path, const struct stat *sp, const struct vol *vol)
{
    EC_INIT;
    const char *adpath;
    int adflags = S_ISDIR(sp->st_mode) ? ADFLAGS_DIR : 0;

    become_root();

    if (ad_conv_v22ea_hf(path, sp, vol) != 0)
        goto delete;
    if (ad_conv_v22ea_rf(path, sp, vol) != 0)
        goto delete;

delete:
    EC_NULL( adpath = ad_path(path, adflags) );
    LOG(log_debug, logtype_ad,"ad_conv_v22ea_hf(\"%s\"): deleting adouble:v2 file: \"%s\"",
        path, fullpathname(adpath));

    unlink(adpath);

EC_CLEANUP:
    if (errno == ENOENT)
        EC_STATUS(0);

    unbecome_root();

    EC_EXIT;
}

/*!
 * Remove hexencoded dots and slashes (":2e" and ":2f")
 */
static int ad_conv_dehex(const char *path, const struct stat *sp, const struct vol *vol, const char **newpathp)
{
    EC_INIT;
    static char buf[MAXPATHLEN];
    int adflags = S_ISDIR(sp->st_mode) ? ADFLAGS_DIR : 0;
    bstring newpath = NULL;
    static bstring str2e = NULL;
    static bstring str2f = NULL;
    static bstring strdot = NULL;
    static bstring strcolon = NULL;

    if (str2e == NULL) {
        str2e = bfromcstr(":2e");
        str2f = bfromcstr(":2f");
        strdot = bfromcstr(".");
        strcolon = bfromcstr(":");
    }

    LOG(log_debug, logtype_ad,"ad_conv_dehex(\"%s\"): BEGIN", fullpathname(path));

    *newpathp = NULL;

    if (((strstr(path, ":2e")) == NULL) && ((strstr(path, ":2f")) == NULL) )
        goto EC_CLEANUP;

    EC_NULL( newpath = bfromcstr(path) );

    EC_ZERO( bfindreplace(newpath, str2e, strdot, 0) );
    EC_ZERO( bfindreplace(newpath, str2f, strcolon, 0) );
    
    become_root();
    if (adflags != ADFLAGS_DIR)
        rename(vol->ad_path(path, 0), vol->ad_path(bdata(newpath), 0));
    rename(path, bdata(newpath));
    unbecome_root();

    strlcpy(buf, bdata(newpath), sizeof(buf));
    *newpathp = buf;

EC_CLEANUP:
    if (newpath)
        bdestroy(newpath);
    EC_EXIT;
}

/*!
 * AppleDouble and encoding conversion on the fly
 *
 * @param path      (r) path to file or directory
 * @param sp        (r) stat(path)
 * @param vol       (r) volume handle
 * @param newpath   (w) if encoding changed, new name. Can be NULL.
 *
 * @returns         -1 on internal error, otherwise 0. newpath is NULL if no character conversion was done,
 *                  otherwise newpath points to a static string with the converted name
 */
int ad_convert(const char *path, const struct stat *sp, const struct vol *vol, const char **newpath)
{
    EC_INIT;
    const char *p;

    LOG(log_debug, logtype_ad,"ad_convert(\"%s\"): BEGIN", fullpathname(path));

    if (newpath)
        *newpath = NULL;

    if (vol->v_flags & AFPVOL_RO)
        EC_EXIT_STATUS(0);

    if ((vol->v_adouble == AD_VERSION_EA) && !(vol->v_flags & AFPVOL_NOV2TOEACONV))
        EC_ZERO( ad_conv_v22ea(path, sp, vol) );

    if (vol->v_adouble == AD_VERSION_EA) {
        EC_ZERO( ad_conv_dehex(path, sp, vol, &p) );
        if (p && newpath)
            *newpath = p;
    }

EC_CLEANUP:
    LOG(log_debug, logtype_ad,"ad_convert(\"%s\"): END: %d", fullpathname(path), ret);
    EC_EXIT;
}

