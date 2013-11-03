/* 
   Copyright (c) 2009,2011 Frank Lahm <franklahm@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/

#ifndef AD_H
#define AD_H

#include <atalk/standards.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>

#include <atalk/globals.h>
#include <atalk/ftw.h>
#include <atalk/cnid.h>
#include <atalk/compat.h>
#include <atalk/volume.h>

#define DIR_DOT_OR_DOTDOT(a) \
        ((strcmp(a, ".") == 0) || (strcmp(a, "..") == 0))

#ifndef TIMESPEC_TO_TIMEVAL
#define TIMESPEC_TO_TIMEVAL(tv, ts) { \
    (tv)->tv_sec = (ts)->tv_sec; \
    (tv)->tv_usec = (ts)->tv_nsec / 1000; \
    }
#endif

#define ADVOL_V2_OR_EA(ad) ((ad) == AD_VERSION2 || (ad) == AD_VERSION_EA)

enum logtype {STD, DBG};

#define SLOG(...)                             \
    _log(STD, __VA_ARGS__)

#define ERROR(...)                              \
    do {                                        \
        _log(STD, __VA_ARGS__);                 \
        exit(1);                                \
    } while (0)

typedef struct {
    struct vol     *vol;
    char           db_stamp[ADEDLEN_PRIVSYN];
} afpvol_t;

extern int log_verbose;             /* Logging flag */
extern void _log(enum logtype lt, char *fmt, ...);

extern int ad_ls(int argc, char **argv, AFPObj *obj);
extern int ad_cp(int argc, char **argv, AFPObj *obj);
extern int ad_rm(int argc, char **argv, AFPObj *obj);
extern int ad_mv(int argc, char **argv, AFPObj *obj);
extern int ad_set(int argc, char **argv, AFPObj *obj);
extern int ad_find(int argc, char **argv, AFPObj *obj);

/* ad_util.c */
extern int openvol(AFPObj *obj, const char *path, afpvol_t *vol);
extern void closevol(afpvol_t *vol);
extern cnid_t cnid_for_path(const afpvol_t *vol, const char *path, cnid_t *did);
extern cnid_t cnid_for_paths_parent(const afpvol_t *vol, const char *path, cnid_t *did);
extern char *utompath(const struct vol *, const char *);
extern int convert_dots_encoding(const afpvol_t *svol, const afpvol_t *dvol, char *path, size_t buflen);

typedef struct {
    char *p_end;/* pointer to NULL at end of path */
    char *target_end;/* pointer to end of target base */
    char p_path[MAXPATHLEN + 2];/* pointer to the start of a path */
} PATH_T;

extern PATH_T to;
extern int fflag, iflag, lflag, nflag, pflag, vflag;

#endif /* AD_H */
