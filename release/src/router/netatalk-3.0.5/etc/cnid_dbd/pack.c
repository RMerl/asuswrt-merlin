/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <arpa/inet.h>

#include <string.h>
#include <inttypes.h>
#include <sys/param.h>
#include <db.h>

#include <atalk/unicode.h>
#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/volume.h>
#include "pack.h"

static const struct vol *volume;

/* --------------- */
/*
 *  This implementation is portable, but could probably be faster by using htonl
 *  where appropriate. Also, this again doubles code from the cdb backend.
 */
static void pack_devino(unsigned char *buf, dev_t dev, ino_t ino)
{
    buf[CNID_DEV_LEN - 1] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 2] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 3] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 4] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 5] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 6] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 7] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 8] = dev;

    buf[CNID_DEV_LEN + CNID_INO_LEN - 1] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 2] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 3] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 4] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 5] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 6] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 7] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 8] = ino;    
}

/* --------------- */
int didname(DB *dbp _U_, const DBT *pkey _U_, const DBT *pdata, DBT *skey)
{
int len;
 
    memset(skey, 0, sizeof(DBT));
    skey->data = (char *)pdata->data + CNID_DID_OFS;
    /* FIXME: At least DB 4.0.14 and 4.1.25 pass in the correct length of
       pdata.size. strlen is therefore not needed. Also, skey should be zeroed
       out already. */
    len = strlen((char *)skey->data + CNID_DID_LEN);
    skey->size = CNID_DID_LEN + len + 1;
    return (0);
}
 
/* --------------- */
int devino(DB *dbp _U_, const DBT *pkey _U_,  const DBT *pdata, DBT *skey)
{
    memset(skey, 0, sizeof(DBT));
    skey->data = (char *)pdata->data + CNID_DEVINO_OFS;
    skey->size = CNID_DEVINO_LEN;
    return (0);
}

/* --------------- */
int idxname(DB *dbp _U_, const DBT *pkey _U_,  const DBT *pdata, DBT *skey)
{
    static char buffer[MAXPATHLEN +2];
    uint16_t flags = CONV_TOLOWER;
    memset(skey, 0, sizeof(DBT));

    if (convert_charset(volume->v_volcharset,
                        volume->v_volcharset,
                        volume->v_maccharset,
                        (char *)pdata->data + CNID_NAME_OFS,
                        strlen((char *)pdata->data + CNID_NAME_OFS),
                        buffer,
                        MAXPATHLEN,
                        &flags) == (size_t)-1) {
        LOG(log_error, logtype_cnid, "idxname: conversion error");
    }

    skey->data = buffer;
    skey->size = strlen(skey->data);
    return (0);
}

void pack_setvol(const struct vol *vol)
{
    volume = vol;
}

/* The equivalent to make_cnid_data in the cnid library. Non re-entrant. We
   differ from make_cnid_data in that we never return NULL, rqst->name cannot
   ever cause start[] to overflow because name length is checked in libatalk. */

unsigned char *pack_cnid_data(struct cnid_dbd_rqst *rqst)
{
    static unsigned char start[CNID_HEADER_LEN + MAXPATHLEN + 1];
    unsigned char *buf = start +CNID_LEN;
    u_int32_t i;

    pack_devino(buf, rqst->dev, rqst->ino);
    buf += CNID_DEVINO_LEN;

    i = htonl(rqst->type);
    memcpy(buf, &i, sizeof(i));
    buf += sizeof(i);

    /* did is already in network byte order */
    buf = memcpy(buf, &rqst->did, sizeof(rqst->did));
    buf += sizeof(rqst->did);
    buf = memcpy(buf, rqst->name, rqst->namelen);
    *(buf + rqst->namelen) = '\0';

    return start;
}

