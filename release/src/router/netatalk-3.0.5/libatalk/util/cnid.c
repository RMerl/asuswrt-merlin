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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>

#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/logger.h>
#include <atalk/errchk.h>
#include <atalk/unicode.h>

/*!
 * Build path relativ to volume root
 *
 * path might be:
 * (a) relative:
 *     "dir/subdir" with cwd: "/afp_volume/topdir"
 * (b) absolute:
 *     "/afp_volume/dir/subdir"
 *
 * @param path     (r) path relative to cwd() or absolute
 * @param volpath  (r) volume path that path is a subdir of (has been computed in volinfo funcs)
 *
 * @returns relative path in new bstring, caller must bdestroy it
 */
bstring rel_path_in_vol(const char *path, const char *volpath)
{
    EC_INIT;
    int cwd = -1;
    bstring fpath = NULL;
    char *dname = NULL;
    struct stat st;

    if (path == NULL || volpath == NULL)
        return NULL;

    EC_NEG1_LOG(cwd = open(".", O_RDONLY));

    EC_ZERO_LOGSTR(lstat(path, &st), "lstat(%s): %s", path, strerror(errno));

    if (path[0] == '/') {
        EC_NULL(fpath = bfromcstr(path));
    } else {
        switch (S_IFMT & st.st_mode) {
        case S_IFREG:
        case S_IFLNK:
            EC_NULL_LOG(dname = strdup(path));
            EC_ZERO_LOGSTR(chdir(dirname(dname)), "chdir(%s): %s", dirname, strerror(errno));
            free(dname);
            dname = NULL;
            EC_NULL(fpath = bfromcstr(getcwdpath()));
            BSTRING_STRIP_SLASH(fpath);
            EC_ZERO(bcatcstr(fpath, "/"));
            EC_NULL_LOG(dname = strdup(path));
            EC_ZERO(bcatcstr(fpath, basename(dname)));
            break;

        case S_IFDIR:
            EC_ZERO_LOGSTR(chdir(path), "chdir(%s): %s", path, strerror(errno));
            EC_NULL(fpath = bfromcstr(getcwdpath()));
            break;

        default:
            EC_FAIL;
        }
    }

    BSTRING_STRIP_SLASH(fpath);

    /*
     * Now we have eg:
     *   fpath:   /Volume/netatalk/dir/bla
     *   volpath: /Volume/netatalk/
     * we want: "dir/bla"
     */
    int len = strlen(volpath);
    if (volpath[len-1] != '/')
        /* in case volpath has no trailing slash */
        len ++;
    EC_ZERO(bdelete(fpath, 0, len));

EC_CLEANUP:
    if (dname) free(dname);
    if (cwd != -1) {
        fchdir(cwd);
        close(cwd);
    }
    if (ret != 0)
        return NULL;
    return fpath;
}
