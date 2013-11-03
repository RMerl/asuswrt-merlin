/*
 * Copyright (c) 2010, Frank Lahm <franklahm@googlemail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atalk/ftw.h>
#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/util.h>
#include <atalk/unix.h>
#include <atalk/volume.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/queue.h>

#include "ad.h"

#define STRIP_TRAILING_SLASH(p) {                                   \
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')  \
            *--(p).p_end = 0;                                       \
    }

static afpvol_t volume;

static cnid_t did, pdid;
static int Rflag;
static volatile sig_atomic_t alarmed;
static int badrm, rval;

static char           *netatalk_dirs[] = {
    ".AppleDB",
    ".AppleDesktop",
    NULL
};

/* Forward declarations */
static int rm(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf);

/*
  Check for netatalk special folders e.g. ".AppleDB" or ".AppleDesktop"
  Returns pointer to name or NULL.
*/
static const char *check_netatalk_dirs(const char *name)
{
    int c;

    for (c=0; netatalk_dirs[c]; c++) {
        if ((strcmp(name, netatalk_dirs[c])) == 0)
            return netatalk_dirs[c];
    }
    return NULL;
}

static void upfunc(void)
{
    did = pdid;
}

/*
  SIGNAL handling:
  catch SIGINT and SIGTERM which cause clean exit. Ignore anything else.
*/

static void sig_handler(int signo)
{
    alarmed = 1;
    return;
}

static void set_signal(void)
{
    struct sigaction sv;

    sv.sa_handler = sig_handler;
    sv.sa_flags = SA_RESTART;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGTERM, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGTERM): %s", strerror(errno));

    if (sigaction(SIGINT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGINT): %s", strerror(errno));

    memset(&sv, 0, sizeof(struct sigaction));
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask);

    if (sigaction(SIGABRT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGABRT): %s", strerror(errno));

    if (sigaction(SIGHUP, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGHUP): %s", strerror(errno));

    if (sigaction(SIGQUIT, &sv, NULL) < 0)
        ERROR("error in sigaction(SIGQUIT): %s", strerror(errno));
}

static void usage_rm(void)
{
    printf(
        "Usage: ad rm [-vR] <file|dir> [<file|dir> ...]\n\n"
        "The rm utility attempts to remove the non-directory type files specified\n"
        "on the command line.\n"
        "If the files and directories reside on an AFP volume, the corresponding\n"
        "CNIDs are deleted from the volumes database.\n\n"
        "The options are as follows:\n\n"
        "   -R   Attempt to remove the file hierarchy rooted in each file argument.\n"
        "   -v   Be verbose when deleting files, showing them as they are removed.\n"
        );
    exit(EXIT_FAILURE);
}

int ad_rm(int argc, char *argv[], AFPObj *obj)
{
    int ch;

    pdid = htonl(1);
    did = htonl(2);

    while ((ch = getopt(argc, argv, "vR")) != -1)
        switch (ch) {
        case 'R':
            Rflag = 1;
            break;
        case 'v':
            vflag = 1;
            break;
        default:
            usage_rm();
            break;
        }
    argc -= optind;
    argv += optind;

    if (argc < 1)
        usage_rm();

    set_signal();
    cnid_init();

    /* Set end of argument list */
    argv[argc] = NULL;

    for (int i = 0; argv[i] != NULL; i++) {
        /* Load .volinfo file for source */
        openvol(obj, argv[i], &volume);

        if (nftw(argv[i], rm, upfunc, 20, FTW_DEPTH | FTW_PHYS) == -1) {
            if (alarmed) {
                SLOG("...break");
            } else {
                SLOG("Error: %s", argv[i]);
            }
            closevol(&volume);
        }
    }
    return rval;
}

static int rm(const char *path,
              const struct stat *statp,
              int tflag,
              struct FTW *ftw)
{
    cnid_t cnid;

    if (alarmed)
        return -1;

    const char *dir = strrchr(path, '/');
    if (dir == NULL)
        dir = path;
    else
        dir++;
    if (check_netatalk_dirs(dir) != NULL)
        return FTW_SKIP_SUBTREE;

    switch (statp->st_mode & S_IFMT) {

    case S_IFLNK:
        if (volume.vol->v_path) {
            if ((volume.vol->v_adouble == AD_VERSION2)
                && (strstr(path, ".AppleDouble") != NULL)) {
                /* symlink inside adouble dir */
                if (unlink(path) != 0)
                    badrm = rval = 1;
                break;
            }

            /* Get CNID of Parent and add new childir to CNID database */
            pdid = did;
            if ((cnid = cnid_for_path(&volume, path, &did)) == CNID_INVALID) {
                SLOG("Error resolving CNID for %s", path);
                return -1;
            }
            if (cnid_delete(volume.vol->v_cdb, cnid) != 0) {
                SLOG("Error removing CNID %u for %s", ntohl(cnid), path);
                return -1;
            }
        }

        if (unlink(path) != 0) {
            badrm = rval = 1;
            break;
        }

        break;

    case S_IFDIR:
        if (!Rflag) {
            SLOG("%s is a directory", path);
            return FTW_SKIP_SUBTREE;
        }

        if (volume.vol->v_path) {
            if ((volume.vol->v_adouble == AD_VERSION2)
                && (strstr(path, ".AppleDouble") != NULL)) {
                /* should be adouble dir itself */
                if (rmdir(path) != 0) {
                    SLOG("Error removing dir \"%s\": %s", path, strerror(errno));
                    badrm = rval = 1;
                    return -1;
                }
                break;
            }

            /* Get CNID of Parent and add new childir to CNID database */
            if ((did = cnid_for_path(&volume, path, &pdid)) == CNID_INVALID) {
                SLOG("Error resolving CNID for %s", path);
                return -1;
            }
            if (cnid_delete(volume.vol->v_cdb, did) != 0) {
                SLOG("Error removing CNID %u for %s", ntohl(did), path);
                return -1;
            }
        }

        if (rmdir(path) != 0) {
            SLOG("Error removing dir \"%s\": %s", path, strerror(errno));
            badrm = rval = 1;
            return -1;
        }

        break;

    case S_IFBLK:
    case S_IFCHR:
        SLOG("%s is a device file.", path);
        badrm = rval = 1;
        break;

    case S_IFSOCK:
        SLOG("%s is a socket.", path);
        badrm = rval = 1;
        break;

    case S_IFIFO:
        SLOG("%s is a FIFO.", path);
        badrm = rval = 1;
        break;

    default:
        if (volume.vol->v_path) {
            if ((volume.vol->v_adouble == AD_VERSION2)
                && (strstr(path, ".AppleDouble") != NULL)) {
                /* file in adouble dir */
                if (unlink(path) != 0)
                    badrm = rval = 1;
                break;
            }

            /* Get CNID of Parent and add new childir to CNID database */
            pdid = did;
            if ((cnid = cnid_for_path(&volume, path, &did)) == CNID_INVALID) {
                SLOG("Error resolving CNID for %s", path);
                return -1;
            }
            if (cnid_delete(volume.vol->v_cdb, cnid) != 0) {
                SLOG("Error removing CNID %u for %s", ntohl(cnid), path);
                return -1;
            }

            /* Ignore errors, because with -R adouble stuff is always alread gone */
            volume.vol->vfs->vfs_deletefile(volume.vol, -1, path);
        }

        if (unlink(path) != 0) {
            badrm = rval = 1;
            break;
        }

        break;
    }

    if (vflag && !badrm)
        (void)printf("%s\n", path);

    return 0;
}
