/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/volume.h>
#include <atalk/directory.h>
#include <atalk/queue.h>
#include <atalk/bstrlib.h>
#include <atalk/globals.h>

#include "file.h"
#include "filedir.h"
#include "directory.h"
#include "dircache.h"
#include "hash.h"
#include "afp_config.h"
#include "volume.h"

#include "test.h"
#include "subtests.h"


#define rbufsize 128000
static char rbuf[rbufsize];
static size_t rbuflen;

#define ADD(a, b, c) (a) += (c); \
                         (b) += (c)

#define PUSHBUF(p, val, size, len) \
    memcpy((p), (val), (size));    \
    (p) += (size);                 \
    (len) += (size)

#define PUSHVAL(p, type, val, len)         \
    { \
        type type = val;                          \
        memcpy(p, &type, sizeof(type));           \
        (p) += sizeof(type);                      \
        (len) += sizeof(type);                    \
    }

static int push_path(char **bufp, const char *name)
{
    int len = 0;
    int slen = strlen(name);
    char *p = *bufp;

    PUSHVAL(p, uint8_t, 3, len); /* path type */
    PUSHVAL(p, uint32_t, kTextEncodingUTF8, len); /* text encoding hint */
    PUSHVAL(p, uint16_t, htons(slen), len);
    if (slen) {
        for (int i = 0; i < slen; i++) {
            if (name[i] == '/')
                p[i] = 0;
            else
                p[i] = name[i];
        }
        len += slen;
    }

    *bufp += len;
    return len;
}

/***********************************************************************************
 * Interface
 ***********************************************************************************/

char **cnamewrap(const char *name)
{
    static char buf[256];
    static char *p = buf;
    int len = 0;

    PUSHVAL(p, uint8_t, 3, len); /* path type */
    PUSHVAL(p, uint32_t, kTextEncodingUTF8, len); /* text encoding hint */
    PUSHVAL(p, uint16_t, ntohs(strlen(name)), len);
    strcpy(p, name);

    p = buf;
    return &p;
}

int getfiledirparms(AFPObj *obj, uint16_t vid, cnid_t did, const char *name)
{
    const int bufsize = 256;
    char buf[bufsize];
    char *p = buf;
    int len = 0;

    ADD(p, len , 2);

    PUSHVAL(p, uint16_t, vid, len);
    PUSHVAL(p, cnid_t, did, len);
    PUSHVAL(p, uint16_t, htons(FILPBIT_FNUM | FILPBIT_PDINFO), len);
    PUSHVAL(p, uint16_t, htons(DIRPBIT_DID | DIRPBIT_PDINFO), len);

    len += push_path(&p, name);

    return afp_getfildirparams(obj, buf, len, rbuf, &rbuflen);
}

int createdir(AFPObj *obj, uint16_t vid, cnid_t did, const char *name)
{
    const int bufsize = 256;
    char buf[bufsize];
    char *p = buf;
    int len = 0;

    ADD(p, len , 2);

    PUSHVAL(p, uint16_t, vid, len);
    PUSHVAL(p, cnid_t, did, len);
    len += push_path(&p, name);

    return afp_createdir(obj, buf, len, rbuf, &rbuflen);
}

int createfile(AFPObj *obj, uint16_t vid, cnid_t did, const char *name)
{
    const int bufsize = 256;
    char buf[bufsize];
    char *p = buf;
    int len = 0;

    PUSHVAL(p, uint16_t, htons(128), len); /* hard create */
    PUSHVAL(p, uint16_t, vid, len);
    PUSHVAL(p, cnid_t, did, len);
    len += push_path(&p, name);

    return afp_createfile(obj, buf, len, rbuf, &rbuflen);
}

int delete(AFPObj *obj, uint16_t vid, cnid_t did, const char *name)
{
    const int bufsize = 256;
    char buf[bufsize];
    char *p = buf;
    int len = 0;

    PUSHVAL(p, uint16_t, htons(128), len); /* hard create */
    PUSHVAL(p, uint16_t, vid, len);
    PUSHVAL(p, cnid_t, did, len);
    len += push_path(&p, name);

    return afp_delete(obj, buf, len, rbuf, &rbuflen);
}

int enumerate(AFPObj *obj, uint16_t vid, cnid_t did)
{
    const int bufsize = 256;
    char buf[bufsize];
    char *p = buf;
    int len = 0;
    int ret;

    ADD(p, len , 2);

    PUSHVAL(p, uint16_t, vid, len);
    PUSHVAL(p, cnid_t, did, len);
    PUSHVAL(p, uint16_t, htons(FILPBIT_PDID | FILPBIT_FNUM | FILPBIT_PDINFO), len);
    PUSHVAL(p, uint16_t, htons(DIRPBIT_PDID | DIRPBIT_DID | DIRPBIT_PDINFO), len);
    PUSHVAL(p, uint16_t, htons(20), len);       /* reqcount */
    PUSHVAL(p, uint32_t, htonl(1), len);        /* startindex */
    PUSHVAL(p, uint32_t, htonl(rbufsize), len); /* max replysize */

    len += push_path(&p, "");

    ret = afp_enumerate_ext2(obj, buf, len, rbuf, &rbuflen);

    if (ret != AFPERR_NOOBJ && ret != AFP_OK)
        return -1;
    return 0;
}

uint16_t openvol(AFPObj *obj, const char *name)
{
    int ret;
    uint16_t bitmap;
    uint16_t vid;
    const int bufsize = 32;
    char buf[bufsize];
    char *p = buf;
    char len = strlen(name);

    memset(p, 0, bufsize);
    p += 2;

    /* bitmap */
    bitmap = htons(1<<VOLPBIT_VID);
    memcpy(p, &bitmap, 2);
    p += 2;

    /* name */
    *p = len;
    p++;
    memcpy(p, name, len);
    p += len;

    len += 2 + 2 + 1; /* (command+pad) + bitmap + len */
    if (len & 1)
        len++;

    rbuflen = 0;
    if ((ret = afp_openvol(obj, buf, len, rbuf, &rbuflen)) != AFP_OK)
        return 0;

    p = rbuf;
    memcpy(&bitmap, p, 2);
    p += 2;
    bitmap = ntohs(bitmap);
    if ( ! (bitmap & 1<<VOLPBIT_VID))
        return 0;

    memcpy(&vid, p, 2);
    return vid;
}

