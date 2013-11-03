/*
  Copyright (c) 2009 Frank Lahm <franklahm@gmail.com>

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

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>

#include <atalk/adouble.h>
#include <atalk/unicode.h>
#include <atalk/netatalk_conf.h>
#include <atalk/volume.h>
#include <atalk/ea.h>
#include <atalk/util.h>
#include <atalk/acl.h>
#include <atalk/compat.h>
#include <atalk/cnid.h>
#include <atalk/errchk.h>

#include "cmd_dbd.h"
#include "dbif.h"
#include "db_param.h"
#include "dbd.h"

/* Some defines to ease code parsing */
#define ADDIR_OK (addir_ok == 0)
#define ADFILE_OK (adfile_ok == 0)


static char           cwdbuf[MAXPATHLEN+1];
static struct vol     *vol;
static dbd_flags_t    dbd_flags;
static char           stamp[CNID_DEV_LEN];
static char           *netatalk_dirs[] = {
    ".AppleDB",
    ".AppleDesktop",
    NULL
};
static char           *special_dirs[] = {
    ".zfs",
    NULL
};
static struct cnid_dbd_rqst rqst;
static struct cnid_dbd_rply rply;
static jmp_buf jmp;
static char pname[MAXPATHLEN] = "../";

/*
  Taken form afpd/desktop.c
*/
static char *utompath(char *upath)
{
    static char  mpath[ MAXPATHLEN + 2]; /* for convert_charset dest_len parameter +2 */
    char         *m, *u;
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
        dbd_log( LOGSTD, "Conversion from %s to %s for %s failed.",
                 vol->v_volcodepage, vol->v_maccodepage, u);
        return NULL;
    }

    return(m);
}

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

/*
  Check for special names
  Returns pointer to name or NULL.
*/
static const char *check_special_dirs(const char *name)
{
    int c;

    for (c=0; special_dirs[c]; c++) {
        if ((strcmp(name, special_dirs[c])) == 0)
            return special_dirs[c];
    }
    return NULL;
}

/*
 * We unCAPed a name, update CNID db
 */
static int update_cnid(cnid_t did, const struct stat *sp, const char *oldname, const char *newname)
{
    cnid_t id;

    /* Query the database */
    if ((id = cnid_lookup(vol->v_cdb, sp, did, (char *)oldname, strlen(oldname))) == CNID_INVALID)
        /* not in db, no need to update */
        return 0;

    /* Update the database */
    if (cnid_update(vol->v_cdb, id, sp, did, (char *)newname, strlen(newname)) < 0)
        return -1;

    return 0;
}

/*
  Check for .AppleDouble file, create if missing
*/
static int check_adfile(const char *fname, const struct stat *st, const char **newname)
{
    int ret;
    int adflags = ADFLAGS_HF;
    struct adouble ad;
    const char *adname;

    *newname = NULL;

    if (vol->v_adouble == AD_VERSION_EA) {
        if (!(dbd_flags & DBD_FLAGS_V2TOEA))
            return 0;
        if (ad_convert(fname, st, vol, newname) != 0) {
            switch (errno) {
            case ENOENT:
                break;
            default:
                dbd_log(LOGSTD, "Conversion error for \"%s/%s\": %s", cwdbuf, fname, strerror(errno));
                break;
            }
        }
        return 0;
    }
    
    if (S_ISDIR(st->st_mode))
        adflags |= ADFLAGS_DIR;

    adname = vol->ad_path(fname, adflags);

    if ((ret = access( adname, F_OK)) != 0) {
        if (errno != ENOENT) {
            dbd_log(LOGSTD, "Access error for ad-file '%s/%s': %s",
                    cwdbuf, adname, strerror(errno));
            return -1;
        }
        /* Missing. Log and create it */
        dbd_log(LOGSTD, "Missing AppleDouble file '%s/%s'", cwdbuf, adname);

        if (dbd_flags & DBD_FLAGS_SCAN)
            /* Scan only requested, dont change anything */
            return -1;

        /* Create ad file */
        ad_init(&ad, vol);

        if ((ret = ad_open(&ad, fname, adflags | ADFLAGS_CREATE | ADFLAGS_RDWR, 0666)) != 0) {
            dbd_log( LOGSTD, "Error creating AppleDouble file '%s/%s': %s",
                     cwdbuf, adname, strerror(errno));

            return -1;
        }

        /* Set name in ad-file */
        ad_setname(&ad, utompath((char *)fname));
        ad_flush(&ad);
        ad_close(&ad, ADFLAGS_HF);

        chown(adname, st->st_uid, st->st_gid);
        /* FIXME: should we inherit mode too here ? */
#if 0
        chmod(adname, st->st_mode);
#endif
    } else {
        ad_init(&ad, vol);
        if (ad_open(&ad, fname, adflags | ADFLAGS_RDONLY) != 0) {
            dbd_log( LOGSTD, "Error opening AppleDouble file for '%s/%s'", cwdbuf, fname);
            return -1;
        }
        ad_close(&ad, ADFLAGS_HF);
    }
    return 0;
}

/* 
   Remove all files with file::EA* from adouble dir
*/
static void remove_eafiles(const char *name, struct ea *ea)
{
    DIR *dp = NULL;
    struct dirent *ep;
    char eaname[MAXPATHLEN];

    strlcpy(eaname, name, sizeof(eaname));
    if (strlcat(eaname, "::EA", sizeof(eaname)) >= sizeof(eaname)) {
        dbd_log(LOGSTD, "name too long: '%s/%s/%s'", cwdbuf, ADv2_DIRNAME, name);
        return;
    }

    if ((chdir(ADv2_DIRNAME)) != 0) {
        dbd_log(LOGSTD, "Couldn't chdir to '%s/%s': %s",
                cwdbuf, ADv2_DIRNAME, strerror(errno));
        return;
    }

    if ((dp = opendir(".")) == NULL) {
        dbd_log(LOGSTD, "Couldn't open the directory '%s/%s': %s",
                cwdbuf, ADv2_DIRNAME, strerror(errno));
        goto exit;
    }

    while ((ep = readdir(dp))) {
        if (strstr(ep->d_name, eaname) != NULL) {
            dbd_log(LOGSTD, "Removing EA file: '%s/%s/%s'",
                    cwdbuf, ADv2_DIRNAME, ep->d_name);
            if ((unlink(ep->d_name)) != 0) {
                dbd_log(LOGSTD, "Error unlinking EA file '%s/%s/%s': %s",
                        cwdbuf, ADv2_DIRNAME, ep->d_name, strerror(errno));
            }
        } /* if */
    } /* while */

exit:
    if (dp)
        closedir(dp);
    if ((chdir("..")) != 0) {
        dbd_log(LOGSTD, "Couldn't chdir to '%s': %s", cwdbuf, strerror(errno));
        /* we can't proceed */
        longjmp(jmp, 1); /* this jumps back to cmd_dbd_scanvol() */
    }    
}

/*
  Check Extended Attributes files
*/
static int check_eafiles(const char *fname)
{
    unsigned int  count = 0;
    int ret = 0, remove;
    struct ea ea;
    struct stat st;
    char *eaname;

    if ((ret = ea_open(vol, fname, EA_RDWR, &ea)) != 0) {
        if (errno == ENOENT)
            return 0;
        dbd_log(LOGSTD, "Error calling ea_open for file: %s/%s, removing EA files", cwdbuf, fname);
        if ( ! (dbd_flags & DBD_FLAGS_SCAN))
            remove_eafiles(fname, &ea);
        return -1;
    }

    /* Check all EAs */
    while (count < ea.ea_count) {        
        dbd_log(LOGDEBUG, "EA: %s", (*ea.ea_entries)[count].ea_name);
        remove = 0;

        eaname = ea_path(&ea, (*ea.ea_entries)[count].ea_name, 0);

        if (lstat(eaname, &st) != 0) {
            if (errno == ENOENT)
                dbd_log(LOGSTD, "Missing EA: %s/%s", cwdbuf, eaname);
            else
                dbd_log(LOGSTD, "Bogus EA: %s/%s", cwdbuf, eaname);
            remove = 1;
        } else if (st.st_size != (*ea.ea_entries)[count].ea_size) {
            dbd_log(LOGSTD, "Bogus EA: %s/%s, removing it...", cwdbuf, eaname);
            remove = 1;
            if ((unlink(eaname)) != 0)
                dbd_log(LOGSTD, "Error removing EA file '%s/%s': %s",
                        cwdbuf, eaname, strerror(errno));
        }

        if (remove) {
            /* Be CAREFUL here! This should do what ea_delentry does. ea_close relies on it !*/
            free((*ea.ea_entries)[count].ea_name);
            (*ea.ea_entries)[count].ea_name = NULL;
        }

        count++;
    } /* while */

    ea_close(&ea);
    return ret;
}

/*
  Check for .AppleDouble folder and .Parent, create if missing
*/
static int check_addir(int volroot)
{
    int addir_ok, adpar_ok;
    struct stat st;
    struct adouble ad;
    char *mname = NULL;

    if (vol->v_adouble == AD_VERSION_EA)
        return 0;

    /* Check for ad-dir */
    if ( (addir_ok = access(ADv2_DIRNAME, F_OK)) != 0) {
        if (errno != ENOENT) {
            dbd_log(LOGSTD, "Access error in directory %s: %s", cwdbuf, strerror(errno));
            return -1;
        }
        dbd_log(LOGSTD, "Missing %s for '%s'", ADv2_DIRNAME, cwdbuf);
    }

    /* Check for ".Parent" */
    if ( (adpar_ok = access(vol->ad_path(".", ADFLAGS_DIR), F_OK)) != 0) {
        if (errno != ENOENT) {
            dbd_log(LOGSTD, "Access error on '%s/%s': %s",
                    cwdbuf, vol->ad_path(".", ADFLAGS_DIR), strerror(errno));
            return -1;
        }
        dbd_log(LOGSTD, "Missing .AppleDouble/.Parent for '%s'", cwdbuf);
    }

    /* Is one missing ? */
    if ((addir_ok != 0) || (adpar_ok != 0)) {
        /* Yes, but are we only scanning ? */
        if (dbd_flags & DBD_FLAGS_SCAN) {
            /* Yes:  missing .Parent is not a problem, but missing ad-dir
               causes later checking of ad-files to fail. So we have to return appropiately */
            if (addir_ok != 0)
                return -1;
            else  /* (adpar_ok != 0) */
                return 0;
        }

        /* Create ad dir and set name */
        ad_init(&ad, vol);

        if (ad_open(&ad, ".", ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_CREATE | ADFLAGS_RDWR, 0777) != 0) {
            dbd_log( LOGSTD, "Error creating AppleDouble dir in %s: %s", cwdbuf, strerror(errno));
            return -1;
        }

        /* Get basename of cwd from cwdbuf */
        mname = utompath(strrchr(cwdbuf, '/') + 1);

        /* Update name in ad file */
        ad_setname(&ad, mname);
        ad_flush(&ad);
        ad_close(&ad, ADFLAGS_HF);

        /* Inherit owner/group from "." to ".AppleDouble" and ".Parent" */
        if ((lstat(".", &st)) != 0) {
            dbd_log( LOGSTD, "Couldnt stat %s: %s", cwdbuf, strerror(errno));
            return -1;
        }
        chown(ADv2_DIRNAME, st.st_uid, st.st_gid);
        chown(vol->ad_path(".", ADFLAGS_DIR), st.st_uid, st.st_gid);
    }

    return 0;
}

/*
  Check if file cotains "::EA" and if it does check if its correspondig data fork exists.
  Returns:
  0 = name is not an EA file
  1 = name is an EA file and no problem was found
  -1 = name is an EA file and data fork is gone
 */
static int check_eafile_in_adouble(const char *name)
{
    int ret = 0;
    char *namep, *namedup = NULL;

    /* Check if this is an AFPVOL_EA_AD vol */
    if (vol->v_vfs_ea == AFPVOL_EA_AD) {
        /* Does the filename contain "::EA" ? */
        namedup = strdup(name);
        if ((namep = strstr(namedup, "::EA")) == NULL) {
            ret = 0;
            goto ea_check_done;
        } else {
            /* File contains "::EA" so it's an EA file. Check for data file  */

            /* Get string before "::EA" from EA filename */
            namep[0] = 0;
            strlcpy(pname + 3, namedup, sizeof(pname)); /* Prepends "../" */

            if ((access( pname, F_OK)) == 0) {
                ret = 1;
                goto ea_check_done;
            } else {
                ret = -1;
                if (errno != ENOENT) {
                    dbd_log(LOGSTD, "Access error for file '%s/%s': %s",
                            cwdbuf, name, strerror(errno));
                    goto ea_check_done;
                }

                /* Orphaned EA file*/
                dbd_log(LOGSTD, "Orphaned Extended Attribute file '%s/%s/%s'",
                        cwdbuf, ADv2_DIRNAME, name);

                if (dbd_flags & DBD_FLAGS_SCAN)
                    /* Scan only requested, dont change anything */
                    goto ea_check_done;

                if ((unlink(name)) != 0) {
                    dbd_log(LOGSTD, "Error unlinking orphaned Extended Attribute file '%s/%s/%s'",
                            cwdbuf, ADv2_DIRNAME, name);
                }
            } /* if (access) */
        } /* if strstr */
    } /* if AFPVOL_EA_AD */

ea_check_done:
    if (namedup)
        free(namedup);

    return ret;
}

/*
  Check files and dirs inside .AppleDouble folder:
  - remove orphaned files
  - bail on dirs
*/
static int read_addir(void)
{
    DIR *dp;
    struct dirent *ep;
    struct stat st;

    if ((chdir(ADv2_DIRNAME)) != 0) {
        if (vol->v_adouble == AD_VERSION_EA) {
            return 0;
        }
        dbd_log(LOGSTD, "Couldn't chdir to '%s/%s': %s",
                cwdbuf, ADv2_DIRNAME, strerror(errno));
        return -1;
    }

    if ((dp = opendir(".")) == NULL) {
        dbd_log(LOGSTD, "Couldn't open the directory '%s/%s': %s",
                cwdbuf, ADv2_DIRNAME, strerror(errno));
        return -1;
    }

    while ((ep = readdir(dp))) {
        /* Check if its "." or ".." */
        if (DIR_DOT_OR_DOTDOT(ep->d_name))
            continue;

        /* Skip ".Parent" */
        if (STRCMP(ep->d_name, ==, ".Parent"))
            continue;

        if ((lstat(ep->d_name, &st)) < 0) {
            dbd_log( LOGSTD, "Lost file or dir while enumeratin dir '%s/%s/%s', probably removed: %s",
                     cwdbuf, ADv2_DIRNAME, ep->d_name, strerror(errno));
            continue;
        }

        /* Check for dirs */
        if (S_ISDIR(st.st_mode)) {
            dbd_log( LOGSTD, "Unexpected directory '%s' in AppleDouble dir '%s/%s'",
                     ep->d_name, cwdbuf, ADv2_DIRNAME);
            continue;
        }

        /* Check if for orphaned and corrupt Extended Attributes file */
        if (check_eafile_in_adouble(ep->d_name) != 0)
            continue;

        /* Check for data file */
        strcpy(pname + 3, ep->d_name);
        if ((access( pname, F_OK)) != 0) {
            if (errno != ENOENT) {
                dbd_log(LOGSTD, "Access error for file '%s/%s': %s",
                        cwdbuf, pname, strerror(errno));
                continue;
            }
            /* Orphaned ad-file*/
            dbd_log(LOGSTD, "Orphaned AppleDoube file '%s/%s/%s'",
                    cwdbuf, ADv2_DIRNAME, ep->d_name);

            if (dbd_flags & DBD_FLAGS_SCAN)
                /* Scan only requested, dont change anything */
                continue;;

            if ((unlink(ep->d_name)) != 0) {
                dbd_log(LOGSTD, "Error unlinking orphaned AppleDoube file '%s/%s/%s'",
                        cwdbuf, ADv2_DIRNAME, ep->d_name);

            }
        }
    }

    if ((chdir("..")) != 0) {
        dbd_log(LOGSTD, "Couldn't chdir back to '%s' from AppleDouble dir: %s",
                cwdbuf, strerror(errno));
        /* This really is EOT! */
        longjmp(jmp, 1); /* this jumps back to cmd_dbd_scanvol() */
    }

    closedir(dp);

    return 0;
}

/*
  Check CNID for a file/dir, both from db and from ad-file.
  For detailed specs see intro.

  @return Correct CNID of object or CNID_INVALID (ie 0) on error
*/
static cnid_t check_cnid(const char *name, cnid_t did, struct stat *st, int adfile_ok)
{
    int adflags = ADFLAGS_HF;
    cnid_t db_cnid, ad_cnid;
    struct adouble ad;

    adflags = ADFLAGS_HF | (S_ISDIR(st->st_mode) ? ADFLAGS_DIR : 0);

    /* Get CNID from ad-file */
    ad_cnid = CNID_INVALID;
    if (ADFILE_OK) {
        ad_init(&ad, vol);
        if (ad_open(&ad, name, adflags | ADFLAGS_RDWR) != 0) {
            
            if (vol->v_adouble != AD_VERSION_EA) {
                dbd_log( LOGSTD, "Error opening AppleDouble file for '%s/%s': %s", cwdbuf, name, strerror(errno));
                return CNID_INVALID;
            }
            dbd_log( LOGDEBUG, "File without meta EA: \"%s/%s\"", cwdbuf, name);
            adfile_ok = 1;
        } else {
            ad_cnid = ad_getid(&ad, st->st_dev, st->st_ino, 0, stamp);
            if (ad_cnid == CNID_INVALID)
                dbd_log( LOGSTD, "Bad CNID in adouble file of '%s/%s'", cwdbuf, name);
            else
                dbd_log( LOGDEBUG, "CNID from .AppleDouble file for '%s/%s': %u", cwdbuf, name, ntohl(ad_cnid));
            ad_close(&ad, ADFLAGS_HF);
        }
    }

    /* Get CNID from database */
    if ((db_cnid = cnid_add(vol->v_cdb, st, did, (char *)name, strlen(name), ad_cnid)) == CNID_INVALID)
        return CNID_INVALID;

    /* Compare CNID from db and adouble file */
    if (ad_cnid != db_cnid && adfile_ok == 0) {
        /* Mismatch, overwrite ad file with value from db */
        dbd_log(LOGSTD, "CNID mismatch for '%s/%s', CNID db: %u, ad-file: %u",
                cwdbuf, name, ntohl(db_cnid), ntohl(ad_cnid));
        ad_init(&ad, vol);
        if (ad_open(&ad, name, adflags | ADFLAGS_HF | ADFLAGS_RDWR) != 0) {
            dbd_log(LOGSTD, "Error opening AppleDouble file for '%s/%s': %s",
                    cwdbuf, name, strerror(errno));
            return CNID_INVALID;
        }
        ad_setid( &ad, st->st_dev, st->st_ino, db_cnid, did, stamp);
        ad_flush(&ad);
        ad_close(&ad, ADFLAGS_HF);
    }

    return db_cnid;
}

/*
  This is called recursively for all dirs.
  volroot=1 means we're in the volume root dir, 0 means we aren't.
  We use this when checking for netatalk private folders like .AppleDB.
  did is our parents CNID.
*/
static int dbd_readdir(int volroot, cnid_t did)
{
    int cwd, ret = 0, adfile_ok, addir_ok;
    cnid_t cnid = 0;
    const char *name;
    DIR *dp;
    struct dirent *ep;
    static struct stat st;      /* Save some stack space */

    /* Check again for .AppleDouble folder, check_adfile also checks/creates it */
    if ((addir_ok = check_addir(volroot)) != 0)
        if ( ! (dbd_flags & DBD_FLAGS_SCAN))
            /* Fatal on rebuild run, continue if only scanning ! */
            return -1;

    /* Check AppleDouble files in AppleDouble folder, but only if it exists or could be created */
    if (ADDIR_OK)
        if ((read_addir()) != 0)
            if ( ! (dbd_flags & DBD_FLAGS_SCAN))
                /* Fatal on rebuild run, continue if only scanning ! */
                return -1;

    if ((dp = opendir (".")) == NULL) {
        dbd_log(LOGSTD, "Couldn't open the directory: %s",strerror(errno));
        return -1;
    }

    while ((ep = readdir (dp))) {
        /* Check if we got a termination signal */
        if (alarmed)
            longjmp(jmp, 1); /* this jumps back to cmd_dbd_scanvol() */

        /* Check if its "." or ".." */
        if (DIR_DOT_OR_DOTDOT(ep->d_name))
            continue;

        /* Check for netatalk special folders e.g. ".AppleDB" or ".AppleDesktop" */
        if ((name = check_netatalk_dirs(ep->d_name)) != NULL) {
            if (! volroot)
                dbd_log(LOGSTD, "Nested %s in %s", name, cwdbuf);
            continue;
        }

        /* Check for special folders in volume root e.g. ".zfs" */
        if (volroot) {
            if ((name = check_special_dirs(ep->d_name)) != NULL) {
                dbd_log(LOGSTD, "Ignoring special dir \"%s\"", name);
                continue;
            }
        }

        /* Skip .AppleDouble dir in this loop */
        if (STRCMP(ep->d_name, == , ADv2_DIRNAME))
            continue;

        if (!vol->vfs->vfs_validupath(vol, ep->d_name)) {
            dbd_log(LOGDEBUG, "Ignoring \"%s\"", ep->d_name);
            continue;
        }

        if ((ret = lstat(ep->d_name, &st)) < 0) {
            dbd_log( LOGSTD, "Lost file while reading dir '%s/%s', probably removed: %s",
                     cwdbuf, ep->d_name, strerror(errno));
            continue;
        }
        
        switch (st.st_mode & S_IFMT) {
        case S_IFREG:
        case S_IFDIR:
        case S_IFLNK:
            break;
        default:
            dbd_log(LOGSTD, "Bad filetype: %s/%s", cwdbuf, ep->d_name);
            if ( ! (dbd_flags & DBD_FLAGS_SCAN)) {
                if ((unlink(ep->d_name)) != 0) {
                    dbd_log(LOGSTD, "Error removing: %s/%s: %s", cwdbuf, ep->d_name, strerror(errno));
                }
            }
            continue;
        }

        /**************************************************************************
           Statistics
         **************************************************************************/
        static unsigned long long statcount = 0;
        static time_t t = 0;

        if (t == 0)
            t = time(NULL);

        statcount++;
        if ((statcount % 10000) == 0) {
            if (dbd_flags & DBD_FLAGS_STATS)            
                dbd_log(LOGSTD, "Scanned: %10llu, time: %10llu s",
                        statcount, (unsigned long long)(time(NULL) - t));
        }

        /**************************************************************************
           Tests
        **************************************************************************/

        /* Check for appledouble file, create if missing, but only if we have addir */
        const char *name = NULL;
        adfile_ok = -1;
        if (ADDIR_OK)
            adfile_ok = check_adfile(ep->d_name, &st, &name);

        if (!S_ISLNK(st.st_mode)) {
            if (name == NULL) {
                name = ep->d_name;
            } else {
                update_cnid(did, &st, ep->d_name, name);
            }

            /* Check CNIDs */
            cnid = check_cnid(name, did, &st, adfile_ok);

            /* Check EA files */
            if (vol->v_vfs_ea == AFPVOL_EA_AD)
                check_eafiles(name);
        }

        /**************************************************************************
          Recursion
        **************************************************************************/
        if (S_ISDIR(st.st_mode) && cnid) { /* If we have no cnid for it we cant enter recursion */
            strcat(cwdbuf, "/");
            strcat(cwdbuf, name);
            dbd_log( LOGDEBUG, "Entering directory: %s", cwdbuf);
            if (-1 == (cwd = open(".", O_RDONLY))) {
                dbd_log( LOGSTD, "Cant open directory '%s': %s", cwdbuf, strerror(errno));
                continue;
            }
            if (0 != chdir(name)) {
                dbd_log( LOGSTD, "Cant chdir to directory '%s': %s", cwdbuf, strerror(errno));
                close(cwd);
                continue;
            }

            ret = dbd_readdir(0, cnid);

            fchdir(cwd);
            close(cwd);
            *(strrchr(cwdbuf, '/')) = 0;
            if (ret < 0)
                return -1;
        }
    }

    /*
      Use results of previous checks
    */
    if ((vol->v_adouble == AD_VERSION_EA) && (dbd_flags & DBD_FLAGS_V2TOEA)) {
        if (rmdir(ADv2_DIRNAME) != 0) {
            switch (errno) {
            case ENOENT:
                break;
            default:
                dbd_log(LOGSTD, "Error removing adouble dir \"%s/%s\": %s", cwdbuf, ADv2_DIRNAME, strerror(errno));
                break;
            }
        }
    }
    closedir(dp);
    return ret;
}

/*
  Main func called from cmd_dbd.c
*/
int cmd_dbd_scanvol(struct vol *vol_in, dbd_flags_t flags)
{
    EC_INIT;
    struct stat st;

    /* Run with umask 0 */
    umask(0);

    /* Make vol accessible for all funcs */
    vol = vol_in;
    dbd_flags = flags;

    /* We only support unicode volumes ! */
    if (vol->v_volcharset != CH_UTF8) {
        dbd_log(LOGSTD, "Not a Unicode volume: %s, %u != %u", vol->v_volcodepage, vol->v_volcharset, CH_UTF8);
        EC_FAIL;
    }

    /*
     * Get CNID database stamp, cnid_getstamp() passes the buffer,
     * then cnid_resolve() actually gets the value from the db
     */
    cnid_getstamp(vol->v_cdb, stamp, sizeof(stamp));

    if (setjmp(jmp) != 0) {
        EC_EXIT_STATUS(0); /* Got signal, jump from dbd_readdir */
    }

    strcpy(cwdbuf, vol->v_path);
    chdir(vol->v_path);

    if ((vol->v_adouble == AD_VERSION_EA) && (dbd_flags & DBD_FLAGS_V2TOEA)) {
        if (lstat(".", &st) != 0)
            EC_FAIL;
        if (ad_convert(".", &st, vol, NULL) != 0) {
            switch (errno) {
            case ENOENT:
                break;
            default:
                dbd_log(LOGSTD, "Conversion error for \"%s\": %s", vol->v_path, strerror(errno));
                break;
            }
        }
    }

    /* Start recursion */
    EC_NEG1( dbd_readdir(1, htonl(2)) );  /* 2 = volumeroot CNID */

EC_CLEANUP:
    EC_EXIT;
}
