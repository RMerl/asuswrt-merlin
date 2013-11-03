/*
 * Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>
 * Copyright (c) 1991, 1993, 1994
 * The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>

#ifdef HAVE_SOLARIS_ACLS
#include <sys/acl.h>
#endif  /* HAVE_SOLARIS_ACLS */

#ifdef HAVE_POSIX_ACLS
#include <sys/types.h>
#include <sys/acl.h>
#endif /* HAVE_POSIX_ACLS */

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/logger.h>
#include <atalk/errchk.h>
#include <atalk/unicode.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>


#include "ad.h"

int log_verbose;             /* Logging flag */

void _log(enum logtype lt, char *fmt, ...)
{
    int len;
    static char logbuffer[1024];
    va_list args;

    if ( (lt == STD) || (log_verbose == 1)) {
        va_start(args, fmt);
        len = vsnprintf(logbuffer, 1023, fmt, args);
        va_end(args);
        logbuffer[1023] = 0;

        printf("%s\n", logbuffer);
    }
}

/*!
 * Load volinfo and initialize struct vol
 *
 * Only opens "dbd" volumes !
 *
 * @param path   (r)  path to evaluate
 * @param vol    (rw) structure to initialize
 *
 * @returns 0 on success, exits on error
 */
int openvol(AFPObj *obj, const char *path, afpvol_t *vol)
{
    int flags = 0;

    memset(vol, 0, sizeof(afpvol_t));

    if ((vol->vol = getvolbypath(obj, path)) == NULL)
        return -1;

    if (STRCMP(vol->vol->v_cnidscheme, != , "dbd"))
        ERROR("\"%s\" isn't a \"dbd\" CNID volume!", vol->vol->v_path);

    /* Sanity checks to ensure we can touch this volume */
    if (vol->vol->v_adouble != AD_VERSION2
        && vol->vol->v_adouble != AD_VERSION_EA)
        ERROR("Unsupported adouble versions: %u", vol->vol->v_adouble);

    if (vol->vol->v_vfs_ea != AFPVOL_EA_SYS)
        ERROR("Unsupported Extended Attributes option: %u", vol->vol->v_vfs_ea);

    if ((vol->vol->v_flags & AFPVOL_NODEV))
        flags |= CNID_FLAG_NODEV;

    if ((vol->vol->v_cdb = cnid_open(vol->vol->v_path,
                                     0000,
                                     "dbd",
                                     flags,
                                     vol->vol->v_cnidserver,
                                     vol->vol->v_cnidport)) == NULL)
        ERROR("Cant initialize CNID database connection for %s", vol->vol->v_path);

    cnid_getstamp(vol->vol->v_cdb,
                  vol->db_stamp,
                  sizeof(vol->db_stamp));
    
    return 0;
}

void closevol(afpvol_t *vol)
{
    if (vol->vol) {
        if (vol->vol->v_cdb) {
            cnid_close(vol->vol->v_cdb);
            vol->vol->v_cdb = NULL;
        }
    }
    memset(vol, 0, sizeof(afpvol_t));
}

/*
  Taken form afpd/desktop.c
*/
char *utompath(const struct vol *vol, const char *upath)
{
    static char  mpath[ MAXPATHLEN + 2]; /* for convert_charset dest_len parameter +2 */
    char         *m;
    const char   *u;
    uint16_t     flags = CONV_IGNORE | CONV_UNESCAPEHEX;
    size_t       outlen;

    if (!upath)
        return NULL;

    m = mpath;
    u = upath;
    outlen = strlen(upath);

    if ((vol->v_casefold & AFPVOL_UTOMUPPER))
        flags |= CONV_TOUPPER;
    else if ((vol->v_casefold & AFPVOL_UTOMLOWER))
        flags |= CONV_TOLOWER;

    if ((vol->v_flags & AFPVOL_EILSEQ)) {
        flags |= CONV__EILSEQ;
    }

    /* convert charsets */
    if ((size_t)-1 == ( outlen = convert_charset(vol->v_volcharset,
                                                 CH_UTF8_MAC,
                                                 vol->v_maccharset,
                                                 u, outlen, mpath, MAXPATHLEN, &flags)) ) {
        SLOG("Conversion from %s to %s for %s failed.",
             vol->v_volcodepage, vol->v_maccodepage, u);
        return NULL;
    }

    return(m);
}


/*!
 * Convert dot encoding of basename _in place_
 *
 * path arg can be "[/][dir/ | ...]filename". It will be converted in place
 * possible encoding ".file" as ":2efile" which means the result will be
 * longer then the original which means provide a big enough buffer.
 *
 * @param svol   (r)  source volume
 * @param dvol   (r)  destinatio volume
 * @param path   (rw) path to convert _in place_
 * @param buflen (r)  size of path buffer (max strlen == buflen -1)
 *
 * @returns 0 on sucess, -1 on error
 */
int convert_dots_encoding(const afpvol_t *svol, const afpvol_t *dvol, char *path, size_t buflen)
{
    static charset_t from = (charset_t) -1;
    static char buf[MAXPATHLEN+2];
    char *bname = stripped_slashes_basename(path);
    int pos = bname - path;
    uint16_t flags = 0;

    if ( ! svol->vol->v_path) {
        /* no source volume: escape special chars (eg ':') */
        from = dvol->vol->v_volcharset; /* src = dst charset */
        if (dvol->vol->v_adouble == AD_VERSION2)
            flags |= CONV_ESCAPEHEX;
    } else {
        from = svol->vol->v_volcharset;
    }

    int len = convert_charset(from,
                              dvol->vol->v_volcharset,
                              dvol->vol->v_maccharset,
                              bname, strlen(bname),
                              buf, MAXPATHLEN,
                              &flags);
    if (len == -1)
        return -1;

    if (strlcpy(bname, buf, MAXPATHLEN - pos) > MAXPATHLEN - pos)
        return -1;
    return 0;
}

/*!
 * ResolvesCNID of a given paths
 *
 * path might be:
 * (a) relative:
 *     "dir/subdir" with cwd: "/afp_volume/topdir"
 * (b) absolute:
 *     "/afp_volume/dir/subdir"
 *
 * path MUST be pointing inside vol, this is usually the case as vol has been build from
 * path using loadvolinfo and friends.
 *
 * @param vol  (r) pointer to afpvol_t
 * @param path (r) path, see above
 * @param did  (rw) parent CNID of returned CNID
 *
 * @returns CNID of path
 */
cnid_t cnid_for_path(const afpvol_t *vol,
                     const char *path,
                     cnid_t *did)
{
    EC_INIT;

    cnid_t cnid;
    bstring rpath = NULL;
    bstring statpath = NULL;
    struct bstrList *l = NULL;
    struct stat st;

    cnid = htonl(2);

    EC_NULL(rpath = rel_path_in_vol(path, vol->vol->v_path));
    EC_NULL(statpath = bfromcstr(vol->vol->v_path));
    EC_ZERO(bcatcstr(statpath, "/"));

    l = bsplit(rpath, '/');
    for (int i = 0; i < l->qty ; i++) {
        *did = cnid;

        EC_ZERO(bconcat(statpath, l->entry[i]));
        EC_ZERO_LOGSTR(lstat(cfrombstr(statpath), &st),
                       "lstat(rpath: %s, elem: %s): %s: %s",
                       cfrombstr(rpath), cfrombstr(l->entry[i]),
                       cfrombstr(statpath), strerror(errno));

        if ((cnid = cnid_add(vol->vol->v_cdb,
                             &st,
                             *did,
                             cfrombstr(l->entry[i]),
                             blength(l->entry[i]),
                             0)) == CNID_INVALID) {
            EC_FAIL;
        }
        EC_ZERO(bcatcstr(statpath, "/"));
    }

EC_CLEANUP:
    bdestroy(rpath);
    bstrListDestroy(l);
    bdestroy(statpath);
    if (ret != 0)
        return CNID_INVALID;

    return cnid;
}

/*!
 * Resolves CNID of a given paths parent directory
 *
 * path might be:
 * (a) relative:
 *     "dir/subdir" with cwd: "/afp_volume/topdir"
 * (b) absolute:
 *     "/afp_volume/dir/subdir"
 *
 * path MUST be pointing inside vol, this is usually the case as vol has been build from
 * path using loadvolinfo and friends.
 *
 * @param vol  (r) pointer to afpvol_t
 * @param path (r) path, see above
 * @param did  (rw) parent CNID of returned CNID
 *
 * @returns CNID of path
 */
cnid_t cnid_for_paths_parent(const afpvol_t *vol,
                             const char *path,
                             cnid_t *did)
{
    EC_INIT;

    cnid_t cnid;
    bstring rpath = NULL;
    bstring statpath = NULL;
    struct bstrList *l = NULL;
    struct stat st;

    *did = htonl(1);
    cnid = htonl(2);

    EC_NULL(rpath = rel_path_in_vol(path, vol->vol->v_path));
    EC_NULL(statpath = bfromcstr(vol->vol->v_path));

    l = bsplit(rpath, '/');
    if (l->qty == 1)
        /* only one path element, means parent dir cnid is volume root = 2 */
        goto EC_CLEANUP;
    for (int i = 0; i < (l->qty - 1); i++) {
        *did = cnid;
        EC_ZERO(bconcat(statpath, l->entry[i]));
        EC_ZERO_LOGSTR(lstat(cfrombstr(statpath), &st),
                       "lstat(rpath: %s, elem: %s): %s: %s",
                       cfrombstr(rpath), cfrombstr(l->entry[i]),
                       cfrombstr(statpath), strerror(errno));

        if ((cnid = cnid_add(vol->vol->v_cdb,
                             &st,
                             *did,
                             cfrombstr(l->entry[i]),
                             blength(l->entry[i]),
                             0)) == CNID_INVALID) {
            EC_FAIL;
        }
        EC_ZERO(bcatcstr(statpath, "/"));
    }

EC_CLEANUP:
    bdestroy(rpath);
    bstrListDestroy(l);
    bdestroy(statpath);
    if (ret != 0)
        return CNID_INVALID;

    return cnid;
}

