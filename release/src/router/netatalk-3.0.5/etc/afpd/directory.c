/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <grp.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <utime.h>
#include <assert.h>

#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/uuid.h>
#include <atalk/unix.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/errchk.h>
#include <atalk/globals.h>
#include <atalk/fce_api.h>
#include <atalk/netatalk_conf.h>

#include "directory.h"
#include "dircache.h"
#include "desktop.h"
#include "volume.h"
#include "fork.h"
#include "file.h"
#include "filedir.h"
#include "unix.h"
#include "mangle.h"
#include "hash.h"

/*
 * FIXMEs, loose ends after the dircache rewrite:
 * o merge dircache_search_by_name and dir_add ??
 * o case-insensitivity is gone from cname
 */


/*******************************************************************************************
 * Globals
 ******************************************************************************************/

int         afp_errno;
/* As long as directory.c hasn't got its own init call, this get initialized in dircache_init */
struct dir rootParent  = {
    NULL, NULL, NULL, NULL,          /* path, d_m_name, d_u_name, d_m_name_ucs2 */
    NULL, 0, 0,                      /* qidx_node, ctime, d_flags */
    0, 0, 0, 0                       /* pdid, did, offcnt, d_vid */
};
struct dir  *curdir = &rootParent;
struct path Cur_Path = {
    0,
    "",  /* mac name */
    ".", /* unix name */
    0,   /* id */
    NULL,/* struct dir * */
    0,   /* stat is not set */
    0,   /* errno */
    {0} /* struct stat */
};

/*
 * dir_remove queues struct dirs to be freed here. We can't just delete them immeidately
 * eg in dircache_search_by_id, because a caller somewhere up the stack might be
 * referencing it.
 * So instead:
 * - we mark it as invalid by setting d_did to CNID_INVALID (ie 0)
 * - queue it in "invalid_dircache_entries" queue
 * - which is finally freed at the end of every AFP func in afp_dsi.c.
 */
q_t *invalid_dircache_entries;


/*******************************************************************************************
 * Locals
 ******************************************************************************************/


/* -------------------------
   appledouble mkdir afp error code.
*/
static int netatalk_mkdir(const struct vol *vol, const char *name)
{
    int ret;
    struct stat st;

    if (vol->v_flags & AFPVOL_UNIX_PRIV) {
        if (lstat(".", &st) < 0)
            return AFPERR_MISC;
        int mode = (DIRBITS & (~S_ISGID & st.st_mode)) | (0777 & ~vol->v_umask);
        LOG(log_maxdebug, logtype_afpd, "netatalk_mkdir(\"%s\") {parent mode: %04o, vol umask: %04o}",
            name, st.st_mode, vol->v_umask);

        ret = mkdir(name, mode);
    } else {
        ret = ad_mkdir(name, DIRBITS | 0777);
    }

    if (ret < 0) {
        switch ( errno ) {
        case ENOENT :
            return( AFPERR_NOOBJ );
        case EROFS :
            return( AFPERR_VLOCK );
        case EPERM:
        case EACCES :
            return( AFPERR_ACCESS );
        case EEXIST :
            return( AFPERR_EXIST );
        case ENOSPC :
        case EDQUOT :
            return( AFPERR_DFULL );
        default :
            return( AFPERR_PARAM );
        }
    }
    return AFP_OK;
}

/* ------------------- */
static int deletedir(const struct vol *vol, int dirfd, char *dir)
{
    char path[MAXPATHLEN + 1];
    DIR *dp;
    struct dirent   *de;
    struct stat st;
    size_t len;
    int err = AFP_OK;
    size_t remain;

    if ((len = strlen(dir)) +2 > sizeof(path))
        return AFPERR_PARAM;

    /* already gone */
    if ((dp = opendirat(dirfd, dir)) == NULL)
        return AFP_OK;

    strcpy(path, dir);
    strcat(path, "/");
    len++;
    remain = sizeof(path) -len -1;
    while ((de = readdir(dp)) && err == AFP_OK) {
        /* skip this and previous directory */
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        if (strlen(de->d_name) > remain) {
            err = AFPERR_PARAM;
            break;
        }
        strcpy(path + len, de->d_name);
        if (ostatat(dirfd, path, &st, vol_syml_opt(vol))) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            err = deletedir(vol, dirfd, path);
        } else {
            err = netatalk_unlinkat(dirfd, path);
        }
    }
    closedir(dp);

    /* okay. the directory is empty. delete it. note: we already got rid
       of .AppleDouble.  */
    if (err == AFP_OK) {
        err = netatalk_rmdir(dirfd, dir);
    }
    return err;
}

/* do a recursive copy. */
static int copydir(struct vol *vol, struct dir *ddir, int dirfd, char *src, char *dst)
{
    char spath[MAXPATHLEN + 1], dpath[MAXPATHLEN + 1];
    DIR *dp;
    struct dirent   *de;
    struct stat st;
    struct utimbuf      ut;
    size_t slen, dlen;
    size_t srem, drem;
    int err;

    /* doesn't exist or the path is too long. */
    if (((slen = strlen(src)) > sizeof(spath) - 2) ||
        ((dlen = strlen(dst)) > sizeof(dpath) - 2) ||
        ((dp = opendirat(dirfd, src)) == NULL))
        return AFPERR_PARAM;

    /* try to create the destination directory */
    if (AFP_OK != (err = netatalk_mkdir(vol, dst)) ) {
        closedir(dp);
        return err;
    }

    /* set things up to copy */
    strcpy(spath, src);
    strcat(spath, "/");
    slen++;
    srem = sizeof(spath) - slen -1;

    strcpy(dpath, dst);
    strcat(dpath, "/");
    dlen++;
    drem = sizeof(dpath) - dlen -1;

    err = AFP_OK;
    while ((de = readdir(dp))) {
        /* skip this and previous directory */
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        if (strlen(de->d_name) > srem) {
            err = AFPERR_PARAM;
            break;
        }
        strcpy(spath + slen, de->d_name);

        if (ostatat(dirfd, spath, &st, vol_syml_opt(vol)) == 0) {
            if (strlen(de->d_name) > drem) {
                err = AFPERR_PARAM;
                break;
            }
            strcpy(dpath + dlen, de->d_name);

            if (S_ISDIR(st.st_mode)) {
                if (AFP_OK != (err = copydir(vol, ddir, dirfd, spath, dpath)))
                    goto copydir_done;
            } else if (AFP_OK != (err = copyfile(vol, vol, ddir, dirfd, spath, dpath, NULL, NULL))) {
                goto copydir_done;

            } else {
                /* keep the same time stamp. */
                ut.actime = ut.modtime = st.st_mtime;
                utime(dpath, &ut);
            }
        }
    }

    /* keep the same time stamp. */
    if (ostatat(dirfd, src, &st, vol_syml_opt(vol)) == 0) {
        ut.actime = ut.modtime = st.st_mtime;
        utime(dst, &ut);
    }

copydir_done:
    closedir(dp);
    return err;
}

/* ---------------------
 * is our cached offspring count valid?
 */
static int diroffcnt(struct dir *dir, struct stat *st)
{
    return st->st_ctime == dir->d_ctime;
}

/* --------------------- */
static int invisible_dots(const struct vol *vol, const char *name)
{
    return vol_inv_dots(vol) && *name  == '.' && strcmp(name, ".") && strcmp(name, "..");
}

/* ------------------ */
static int set_dir_errors(struct path *path, const char *where, int err)
{
    switch ( err ) {
    case EPERM :
    case EACCES :
        return AFPERR_ACCESS;
    case EROFS :
        return AFPERR_VLOCK;
    }
    LOG(log_error, logtype_afpd, "setdirparam(%s): %s: %s", fullpathname(path->u_name), where, strerror(err) );
    return AFPERR_PARAM;
}

/*!
 * @brief Convert name in client encoding to server encoding
 *
 * Convert ret->m_name to ret->u_name from client encoding to server encoding.
 * This only gets called from cname().
 *
 * @returns 0 on success, -1 on error
 *
 * @note If the passed ret->m_name is mangled, we'll demangle it
 */
static int cname_mtouname(const struct vol *vol, struct dir *dir, struct path *ret, int toUTF8)
{
    static char temp[ MAXPATHLEN + 1];
    char *t;
    cnid_t fileid = 0;

    if (vol->v_obj->afp_version >= 30) {
        if (toUTF8) {
            if (dir->d_did == DIRDID_ROOT_PARENT) {
                /*
                 * With uft8 volume name is utf8-mac, but requested path may be a mangled longname. See #2611981.
                 * So we compare it with the longname from the current volume and if they match
                 * we overwrite the requested path with the utf8 volume name so that the following
                 * strcmp can match.
                 */
                ucs2_to_charset(vol->v_maccharset, vol->v_macname, temp, AFPVOL_MACNAMELEN + 1);
                if (strcasecmp(ret->m_name, temp) == 0)
                    ucs2_to_charset(CH_UTF8_MAC, vol->v_u8mname, ret->m_name, AFPVOL_U8MNAMELEN);
            } else {
                /* toUTF8 */
                if (mtoUTF8(vol, ret->m_name, strlen(ret->m_name), temp, MAXPATHLEN) == (size_t)-1) {
                    afp_errno = AFPERR_PARAM;
                    return -1;
                }
                strcpy(ret->m_name, temp);
            }
        }

        /* check for OS X mangled filename :( */
        t = demangle_osx(vol, ret->m_name, dir->d_did, &fileid);

        if (curdir == NULL) {
            /* demangle_osx() calls dirlookup() which might have clobbered curdir */
            movecwd(vol, dir);
        }

        LOG(log_maxdebug, logtype_afpd, "cname_mtouname('%s',did:%u) {demangled:'%s', fileid:%u}",
            ret->m_name, ntohl(dir->d_did), t, ntohl(fileid));

        if (t != ret->m_name) {
            ret->u_name = t;
            /* duplicate work but we can't reuse all convert_char we did in demangle_osx
             * flags weren't the same
             */
            if ( (t = utompath(vol, ret->u_name, fileid, utf8_encoding(vol->v_obj))) ) {
                /* at last got our view of mac name */
                strcpy(ret->m_name, t);
            }
        }
    } /* afp_version >= 30 */

    /* If we haven't got it by now, get it */
    if (ret->u_name == NULL) {
        if ((ret->u_name = mtoupath(vol, ret->m_name, dir->d_did, utf8_encoding(vol->v_obj))) == NULL) {
            afp_errno = AFPERR_PARAM;
            return -1;
        }
    }

    return 0;
}

/*!
 * @brief Build struct path from struct dir
 *
 * The final movecwd in cname failed, possibly with EPERM or ENOENT. We:
 * 1. move cwd into parent dir (we're often already there, but not always)
 * 2. set struct path to the dirname
 * 3. in case of
 *    AFPERR_ACCESS: the dir is there, we just cant chdir into it
 *    AFPERR_NOOBJ: the dir was there when we stated it in cname, so we have a race
 *                  4. indicate there's no dir for this path
 *                  5. remove the dir
 */
static struct path *path_from_dir(struct vol *vol, struct dir *dir, struct path *ret)
{
    if (dir->d_did == DIRDID_ROOT_PARENT || dir->d_did == DIRDID_ROOT)
        return NULL;

    switch (afp_errno) {

    case AFPERR_ACCESS:
        if (movecwd( vol, dirlookup(vol, dir->d_pdid)) < 0 ) /* 1 */
            return NULL;

        memcpy(ret->m_name, cfrombstr(dir->d_m_name), blength(dir->d_m_name) + 1); /* 3 */
        if (dir->d_m_name == dir->d_u_name) {
            ret->u_name = ret->m_name;
        } else {
            ret->u_name =  ret->m_name + blength(dir->d_m_name) + 1;
            memcpy(ret->u_name, cfrombstr(dir->d_u_name), blength(dir->d_u_name) + 1);
        }

        ret->d_dir = dir;

        LOG(log_debug, logtype_afpd, "cname('%s') {path-from-dir: AFPERR_ACCESS. curdir:'%s', path:'%s'}",
            cfrombstr(dir->d_fullpath),
            cfrombstr(curdir->d_fullpath),
            ret->u_name);

        return ret;

    case AFPERR_NOOBJ:
        if (movecwd(vol, dirlookup(vol, dir->d_pdid)) < 0 ) /* 1 */
            return NULL;

        memcpy(ret->m_name, cfrombstr(dir->d_m_name), blength(dir->d_m_name) + 1);
        if (dir->d_m_name == dir->d_u_name) {
            ret->u_name = ret->m_name;
        } else {
            ret->u_name =  ret->m_name + blength(dir->d_m_name) + 1;
            memcpy(ret->u_name, cfrombstr(dir->d_u_name), blength(dir->d_u_name) + 1);
        }

        ret->d_dir = NULL;      /* 4 */
        dir_remove(vol, dir);   /* 5 */
        return ret;

    default:
        return NULL;
    }

    /* DEADC0DE: never get here */
    return NULL;
}


/*********************************************************************************************
 * Interface
 ********************************************************************************************/

int get_afp_errno(const int param)
{
    if (afp_errno != AFPERR_DID1)
        return afp_errno;
    return param;
}

/*!
 * Resolve struct dir for an absolute path
 *
 * Given a path like "/Volumes/volume/dir/subdir" in a volume "/Volumes/volume" return
 * a pointer to struct dir of "subdir".
 * 1. Remove volue path from absolute path
 * 2. start path
 * 3. loop through all elements of the remaining path from 1.
 * 4. we only allow dirs
 * 5. search dircache
 * 6. if not found in the dircache query the CNID database for the DID
 * 7. and use dirlookup to resolve the DID to a it's struct dir *
 *
 * @param vol   (r) volume the path is in, must be known
 * @param path  (r) absoule path
 *
 * @returns pointer to struct dir or NULL on error
 */
struct dir *dirlookup_bypath(const struct vol *vol, const char *path)
{
    EC_INIT;

    struct dir *dir = NULL;
    cnid_t cnid, did;
    bstring rpath = NULL;
    bstring statpath = NULL;
    struct bstrList *l = NULL;
    struct stat st;

    cnid = htonl(2);
    dir = vol->v_root;

    LOG(log_debug, logtype_afpd, "dirlookup_bypath(\"%s\")", path);

    if (strcmp(vol->v_path, path) == 0)
        return dir;

    EC_NULL(rpath = rel_path_in_vol(path, vol->v_path)); /* 1. */

    LOG(log_debug, logtype_afpd, "dirlookup_bypath: rpath: \"%s\"", cfrombstr(rpath));

    EC_NULL(statpath = bfromcstr(vol->v_path));          /* 2. */

    l = bsplit(rpath, '/');
    for (int i = 0; i < l->qty ; i++) {                  /* 3. */
        did = cnid;
        EC_ZERO(bcatcstr(statpath, "/"));
        EC_ZERO(bconcat(statpath, l->entry[i]));

        LOG(log_debug, logtype_afpd, "dirlookup_bypath: statpath: \"%s\"", cfrombstr(statpath));

        EC_ZERO_LOGSTR(lstat(cfrombstr(statpath), &st),
                       "lstat(rpath: %s, elem: %s): %s: %s",
                       cfrombstr(rpath), cfrombstr(l->entry[i]),
                       cfrombstr(statpath), strerror(errno));

        if (!(S_ISDIR(st.st_mode)))                      /* 4. */
            EC_FAIL;

        if ((dir = dircache_search_by_name(vol,          /* 5. */
                                           dir,
                                           cfrombstr(l->entry[i]),
                                           blength(l->entry[i]))) == NULL) {

            AFP_CNID_START("cnid_add");
            cnid = cnid_add(vol->v_cdb,             /* 6. */
                            &st,
                            did,
                            cfrombstr(l->entry[i]),
                            blength(l->entry[i]),
                            0);
            AFP_CNID_DONE();
            if (cnid == CNID_INVALID)
                EC_FAIL;

            if ((dir = dirlookup(vol, cnid)) == NULL) /* 7. */
                EC_FAIL;
        }
    }

EC_CLEANUP:
    bdestroy(rpath);
    bstrListDestroy(l);
    bdestroy(statpath);
    if (ret != 0)
        return NULL;

    LOG(log_debug, logtype_afpd, "dirlookup_bypath: result: \"%s\"",
        cfrombstr(dir->d_fullpath));

    return dir;
}

/*!
 * @brief Resolve a DID
 *
 * Resolve a DID, allocate a struct dir for it
 * 1. Check for special CNIDs 0 (invalid), 1 and 2.
 * 2a. Check if the DID is in the cache.
 * 2b. Check if it's really a dir  because we cache files too.
 * 3. If it's not in the cache resolve it via the database.
 * 4. Build complete server-side path to the dir.
 * 5. Check if it exists and is a directory.
 * 6. Create the struct dir and populate it.
 * 7. Add it to the cache.
 *
 * @param vol   (r) pointer to struct vol
 * @param did   (r) DID to resolve
 *
 * @returns pointer to struct dir
 */
struct dir *dirlookup(const struct vol *vol, cnid_t did)
{
    static char  buffer[12 + MAXPATHLEN + 1];
    struct stat  st;
    struct dir   *ret = NULL, *pdir;
    bstring      fullpath = NULL;
    char         *upath = NULL, *mpath;
    cnid_t       cnid, pdid;
    size_t       maxpath;
    int          buflen = 12 + MAXPATHLEN + 1;
    int          utf8;
    int          err = 0;

    LOG(log_debug, logtype_afpd, "dirlookup(did: %u): START", ntohl(did));

    /* check for did 0, 1 and 2 */
    if (did == 0 || vol == NULL) { /* 1 */
        afp_errno = AFPERR_PARAM;
        ret = NULL;
        goto exit;
    } else if (did == DIRDID_ROOT_PARENT) {
        rootParent.d_vid = vol->v_vid;
        ret = &rootParent;
        goto exit;
    } else if (did == DIRDID_ROOT) {
        ret = vol->v_root;
        goto exit;
    }

    /* Search the cache */
    if ((ret = dircache_search_by_did(vol, did)) != NULL) { /* 2a */
        if (ret->d_flags & DIRF_ISFILE) {                   /* 2b */
            afp_errno = AFPERR_BADTYPE;
            ret = NULL;
            goto exit;
        }
        if (lstat(cfrombstr(ret->d_fullpath), &st) != 0) {
            LOG(log_debug, logtype_afpd, "dirlookup(did: %u, path: \"%s\"): lstat: %s",
                ntohl(did), cfrombstr(ret->d_fullpath), strerror(errno));
            switch (errno) {
            case ENOENT:
            case ENOTDIR:
                /* It's not there anymore, so remove it */
                LOG(log_debug, logtype_afpd, "dirlookup(did: %u): calling dir_remove", ntohl(did));
                dir_remove(vol, ret);
                afp_errno = AFPERR_NOOBJ;
                ret = NULL;
                goto exit;
            default:
                ret = ret;
                goto exit;
            }
            /* DEADC0DE */
            ret = NULL;
            goto exit;            
        }
        ret = ret;
        goto exit;
    }

    utf8 = utf8_encoding(vol->v_obj);
    maxpath = (utf8) ? MAXPATHLEN - 7 : 255;

    /* Get it from the database */
    cnid = did;
    LOG(log_debug, logtype_afpd, "dirlookup(did: %u): querying CNID database", ntohl(did));

    AFP_CNID_START("cnid_resolve");
    upath = cnid_resolve(vol->v_cdb, &cnid, buffer, buflen);
    AFP_CNID_DONE();
    if (upath == NULL) {
        afp_errno = AFPERR_NOOBJ;
        err = 1;
        goto exit;
    }
    if ((upath = strdup(upath)) == NULL) { /* 3 */
        afp_errno = AFPERR_NOOBJ;
        err = 1;
        goto exit;
    }
    pdid = cnid;

    /*
     * Recurse up the tree, terminates in dirlookup when either
     * - DIRDID_ROOT is hit
     * - a cached entry is found
     */
    LOG(log_debug, logtype_afpd, "dirlookup(did: %u): recursion for did: %u",
        ntohl(did), ntohl(pdid));
    if ((pdir = dirlookup(vol, pdid)) == NULL) {
        err = 1;
        goto exit;
    }

    /* build the fullpath */
    if ((fullpath = bstrcpy(pdir->d_fullpath)) == NULL
        || bconchar(fullpath, '/') != BSTR_OK
        || bcatcstr(fullpath, upath) != BSTR_OK) {
        err = 1;
        goto exit;
    }

    /* stat it and check if it's a dir */
    LOG(log_debug, logtype_afpd, "dirlookup(did: %u): stating \"%s\"",
        ntohl(did), cfrombstr(fullpath));

    if (ostat(cfrombstr(fullpath), &st, vol_syml_opt(vol)) != 0) { /* 5a */
        switch (errno) {
        case ENOENT:
            afp_errno = AFPERR_NOOBJ;
            err = 1;
            goto exit;
        case EPERM:
            afp_errno = AFPERR_ACCESS;
            err = 1;
            goto exit;
        default:
            afp_errno = AFPERR_MISC;
            err = 1;
            goto exit;
        }
    } else {
        if ( ! S_ISDIR(st.st_mode)) { /* 5b */
            afp_errno = AFPERR_BADTYPE;
            err = 1;
            goto exit;
        }
    }

    /* Get macname from unix name */
    if ( (mpath = utompath(vol, upath, did, utf8)) == NULL ) {
        afp_errno = AFPERR_NOOBJ;
        err = 1;
        goto exit;
    }

    /* Create struct dir */
    if ((ret = dir_new(mpath, upath, vol, pdid, did, fullpath, &st)) == NULL) { /* 6 */
        LOG(log_error, logtype_afpd, "dirlookup(did: %u) {%s, %s}: %s", ntohl(did), mpath, upath, strerror(errno));
        err = 1;
        goto exit;
    }
    
    /* Add it to the cache only if it's a dir */
    if (dircache_add(vol, ret) != 0) { /* 7 */
        err = 1;
        goto exit;
    }

exit:
    if (upath) free(upath);
    if (err) {
        LOG(log_debug, logtype_afpd, "dirlookup(did: %u) {exit_error: %s}",
            ntohl(did), AfpErr2name(afp_errno));
        if (fullpath)
            bdestroy(fullpath);
        if (ret) {
            dir_free(ret);
            ret = NULL;
        }
    }
    if (ret)
        LOG(log_debug, logtype_afpd, "dirlookup(did: %u): RESULT: pdid: %u, path: \"%s\"",
            ntohl(ret->d_did), ntohl(ret->d_pdid), cfrombstr(ret->d_fullpath));

    return ret;
}

#define ENUMVETO "./../Network Trash Folder/TheVolumeSettingsFolder/TheFindByContentFolder/:2eDS_Store/Contents/Desktop Folder/Trash/Benutzer/"

/*!
 * @brief Construct struct dir
 *
 * Construct struct dir from parameters.
 *
 * @param m_name   (r) directory name in UTF8-dec
 * @param u_name   (r) directory name in server side encoding
 * @param vol      (r) pointer to struct vol
 * @param pdid     (r) Parent CNID
 * @param did      (r) CNID
 * @param path     (r) Full unix path to object
 * @param st       (r) struct stat of object
 *
 * @returns pointer to new struct dir or NULL on error
 *
 * @note Most of the time mac name and unix name are the same.
 */
struct dir *dir_new(const char *m_name,
                    const char *u_name,
                    const struct vol *vol,
                    cnid_t pdid,
                    cnid_t did,
                    bstring path,
                    struct stat *st)
{
    struct dir *dir;

    dir = (struct dir *) calloc(1, sizeof( struct dir ));
    if (!dir)
        return NULL;

    if ((dir->d_m_name = bfromcstr(m_name)) == NULL) {
        free(dir);
        return NULL;
    }

    if (convert_string_allocate( (utf8_encoding(vol->v_obj)) ? CH_UTF8_MAC : vol->v_maccharset,
                                 CH_UCS2,
                                 m_name,
                                 -1, (char **)&dir->d_m_name_ucs2) == (size_t)-1 ) {
        LOG(log_error, logtype_afpd, "dir_new(did: %u) {%s, %s}: couldn't set UCS2 name", ntohl(did), m_name, u_name);
        dir->d_m_name_ucs2 = NULL;
    }

    if (m_name == u_name || !strcmp(m_name, u_name)) {
        dir->d_u_name = dir->d_m_name;
    }
    else if ((dir->d_u_name = bfromcstr(u_name)) == NULL) {
        bdestroy(dir->d_m_name);
        free(dir);
        return NULL;
    }

    dir->d_did = did;
    dir->d_pdid = pdid;
    dir->d_vid = vol->v_vid;
    dir->d_fullpath = path;
    dir->dcache_ctime = st->st_ctime;
    dir->dcache_ino = st->st_ino;
    if (!S_ISDIR(st->st_mode))
        dir->d_flags = DIRF_ISFILE;
    dir->d_rights_cache = 0xffffffff;
    return dir;
}

/*!
 * @brief Free a struct dir and all its members
 *
 * @param (rw) pointer to struct dir
 */
void dir_free(struct dir *dir)
{
    if (dir->d_u_name != dir->d_m_name) {
        bdestroy(dir->d_u_name);
    }
    if (dir->d_m_name_ucs2)
        free(dir->d_m_name_ucs2);
    bdestroy(dir->d_m_name);
    bdestroy(dir->d_fullpath);
    free(dir);
}

/*!
 * @brief Create struct dir from struct path
 *
 * Create a new struct dir from struct path. Then add it to the cache.
 *
 * 1. Open adouble file, get CNID from it.
 * 2. Search the database, hinting with the CNID from (1).
 * 3. Build fullpath and create struct dir.
 * 4. Add it to the cache.
 *
 * @param vol   (r) pointer to struct vol, possibly modified in callee
 * @param dir   (r) pointer to parrent directory
 * @param path  (rw) pointer to struct path with valid path->u_name
 * @param len   (r) strlen of path->u_name
 *
 * @returns Pointer to new struct dir or NULL on error.
 *
 * @note Function also assigns path->m_name from path->u_name.
 */
struct dir *dir_add(struct vol *vol, const struct dir *dir, struct path *path, int len)
{
    int err = 0;
    struct dir  *cdir = NULL;
    cnid_t      id;
    struct adouble  ad;
    struct adouble *adp = NULL;
    bstring fullpath = NULL;

    AFP_ASSERT(vol);
    AFP_ASSERT(dir);
    AFP_ASSERT(path);
    AFP_ASSERT(len > 0);

    if ((cdir = dircache_search_by_name(vol, dir, path->u_name, strlen(path->u_name))) != NULL) {
        /* there's a stray entry in the dircache */
        LOG(log_debug, logtype_afpd, "dir_add(did:%u,'%s/%s'): {stray cache entry: did:%u,'%s', removing}",
            ntohl(dir->d_did), cfrombstr(dir->d_fullpath), path->u_name,
            ntohl(cdir->d_did), cfrombstr(dir->d_fullpath));
        if (dir_remove(vol, cdir) != 0) {
            dircache_dump();
            AFP_PANIC("dir_add");
        }
    }

    /* get_id needs adp for reading CNID from adouble file */
    ad_init(&ad, vol);
    if ((ad_open(&ad, path->u_name, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_RDONLY)) == 0) /* 1 */
        adp = &ad;

    /* Get CNID */
    if ((id = get_id(vol, adp, &path->st, dir->d_did, path->u_name, len)) == 0) { /* 2 */
        err = 1;
        goto exit;
    }

    if (adp)
        ad_close(adp, ADFLAGS_HF);

    /* Get macname from unixname */
    if (path->m_name == NULL) {
        if ((path->m_name = utompath(vol, path->u_name, id, utf8_encoding(vol->v_obj))) == NULL) {
            LOG(log_error, logtype_afpd, "dir_add(\"%s\"): can't assign macname", path->u_name);
            err = 2;
            goto exit;
        }
    }

    /* Build fullpath */
    if ( ((fullpath = bstrcpy(dir->d_fullpath)) == NULL) /* 3 */
         || (bconchar(fullpath, '/') != BSTR_OK)
         || (bcatcstr(fullpath, path->u_name)) != BSTR_OK) {
        LOG(log_error, logtype_afpd, "dir_add: fullpath: %s", strerror(errno) );
        err = 3;
        goto exit;
    }

    /* Allocate and initialize struct dir */
    if ((cdir = dir_new(path->m_name,
                        path->u_name,
                        vol,
                        dir->d_did,
                        id,
                        fullpath,
                        &path->st)) == NULL) { /* 3 */
        err = 4;
        goto exit;
    }

    if ((dircache_add(vol, cdir)) != 0) { /* 4 */
        LOG(log_error, logtype_afpd, "dir_add: fatal dircache error: %s", cfrombstr(fullpath));
        exit(EXITERR_SYS);
    }

exit:
    if (err != 0) {
        LOG(log_debug, logtype_afpd, "dir_add('%s/%s'): error: %u",
            cfrombstr(dir->d_u_name), path->u_name, err);

        if (adp)
            ad_close(adp, ADFLAGS_HF);
        if (!cdir && fullpath)
            bdestroy(fullpath);
        if (cdir)
            dir_free(cdir);
        cdir = NULL;
    } else {
        /* no error */
        LOG(log_debug, logtype_afpd, "dir_add(did:%u,'%s/%s'): {cached: %u,'%s'}",
            ntohl(dir->d_did), cfrombstr(dir->d_fullpath), path->u_name,
            ntohl(cdir->d_did), cfrombstr(cdir->d_fullpath));
    }

    return(cdir);
}

/*!
 * Free the queue with invalid struct dirs
 *
 * This gets called at the end of every AFP func.
 */
void dir_free_invalid_q(void)
{
    struct dir *dir;
    while ((dir = (struct dir *)dequeue(invalid_dircache_entries)))
        dir_free(dir);
}

/*!
 * @brief Remove a dir from a cache and queue it for freeing
 *
 * 1. Check if the dir is locked or has opened forks
 * 2. Remove it from the cache
 * 3. Queue it for removal
 * 4. If it's a request to remove curdir, mark curdir as invalid
 * 5. Mark it as invalid
 *
 * @param (r) pointer to struct vol
 * @param (rw) pointer to struct dir
 */
int dir_remove(const struct vol *vol, struct dir *dir)
{
    AFP_ASSERT(vol);
    AFP_ASSERT(dir);

    if (dir->d_did == DIRDID_ROOT_PARENT || dir->d_did == DIRDID_ROOT)
        return 0;

    LOG(log_debug, logtype_afpd, "dir_remove(did:%u,'%s'): {removing}",
        ntohl(dir->d_did), cfrombstr(dir->d_u_name));

    dircache_remove(vol, dir, DIRCACHE | DIDNAME_INDEX | QUEUE_INDEX); /* 2 */
    enqueue(invalid_dircache_entries, dir); /* 3 */

    if (curdir == dir)                      /* 4 */
        curdir = NULL;

    dir->d_did = CNID_INVALID;              /* 5 */

    return 0;
}

#if 0 /* unused */
/*!
 * @brief Modify a struct dir, adjust cache
 *
 * Any value that is 0 or NULL is not changed. If new_uname is NULL it is set to new_mname.
 * If given new_uname == new_mname, new_uname will point to new_mname.
 *
 * @param vol       (r) pointer to struct vol
 * @param dir       (rw) pointer to struct dir
 * @param pdid      (r) new parent DID
 * @param did       (r) new DID
 * @param new_mname (r) new mac-name
 * @param new_uname (r) new unix-name
 * @param pdir_fullpath (r) new fullpath of parent dir
 */
int dir_modify(const struct vol *vol,
               struct dir *dir,
               cnid_t pdid,
               cnid_t did,
               const char *new_mname,
               const char *new_uname,
               bstring pdir_fullpath)
{
    int ret = 0;

    /* Remove it from the cache */
    dircache_remove(vol, dir, DIRCACHE | DIDNAME_INDEX | QUEUE_INDEX);

    if (pdid)
        dir->d_pdid = pdid;
    if (did)
        dir->d_did = did;

    if (new_mname) {
        /* free uname if it's not the same as mname */
        if (dir->d_m_name != dir->d_u_name)
            bdestroy(dir->d_u_name);

        if (new_uname == NULL)
            new_uname = new_mname;

        /* assign new name */
        if ((bassigncstr(dir->d_m_name, new_mname)) != BSTR_OK) {
            LOG(log_error, logtype_afpd, "dir_modify: bassigncstr: %s", strerror(errno) );
            return -1;
        }

        if (new_mname == new_uname || (strcmp(new_mname, new_uname) == 0)) {
            dir->d_u_name = dir->d_m_name;
        } else {
            if ((dir->d_u_name = bfromcstr(new_uname)) == NULL) {
                LOG(log_error, logtype_afpd, "dir_modify: bassigncstr: %s", strerror(errno) );
                return -1;
            }
        }
    }

    if (pdir_fullpath) {
        if (bassign(dir->d_fullpath, pdir_fullpath) != BSTR_OK)
            return -1;
        if (bcatcstr(dir->d_fullpath, "/") != BSTR_OK)
            return -1;
        if (bcatcstr(dir->d_fullpath, new_uname) != BSTR_OK)
            return -1;
    }

    if (dir->d_m_name_ucs2)
        free(dir->d_m_name_ucs2);
    if ((size_t)-1 == convert_string_allocate((utf8_encoding())?CH_UTF8_MAC:vol->v_maccharset, CH_UCS2, dir->d_m_name, -1, (char**)&dir->d_m_name_ucs2))
        dir->d_m_name_ucs2 = NULL;

    /* Re-add it to the cache */
    if ((dircache_add(vol, dir)) != 0) {
        dircache_dump();
        AFP_PANIC("dir_modify");
    }

    return ret;
}
#endif

/*!
 * @brief Resolve a catalog node name path
 *
 * 1. Evaluate path type
 * 2. Move to start dir, if we cant, it might eg because of EACCES, build
 *    path from dirname, so eg getdirparams has sth it can chew on. curdir
 *    is dir parent then. All this is done in path_from_dir().
 * 3. Parse next cnode name in path, cases:
 * 4.   single "\0" -> do nothing
 * 5.   two or more consecutive "\0" -> chdir("..") one or more times
 * 6.   cnode name -> copy it to path.m_name
 * 7. Get unix name from mac name
 * 8. Special handling of request with did 1
 * 9. stat the cnode name
 * 10. If it's not there, it's probably an afp_createfile|dir,
 *     return with curdir = dir parent, struct path = dirname
 * 11. If it's there and it's a file, it must should be the last element of the requested
 *     path. Return with curdir = cnode name parent dir, struct path = filename
 * 12. Treat symlinks like files, dont follow them
 * 13. If it's a dir:
 * 14. Search the dircache for it
 * 15. If it's not in the cache, create a struct dir for it and add it to the cache
 * 16. chdir into the dir and
 * 17. set m_name to the mac equivalent of "."
 * 18. goto 3
 */
struct path *cname(struct vol *vol, struct dir *dir, char **cpath)
{
    static char        path[ MAXPATHLEN + 1];
    static struct path ret;

    struct dir  *cdir;
    char        *data, *p;
    int         len;
    uint32_t   hint;
    uint16_t   len16;
    int         size = 0;
    int         toUTF8 = 0;

    LOG(log_maxdebug, logtype_afpd, "came('%s'): {start}", cfrombstr(dir->d_fullpath));

    data = *cpath;
    afp_errno = AFPERR_NOOBJ;
    memset(&ret, 0, sizeof(ret));

    switch (ret.m_type = *data) { /* 1 */
    case 2:
        data++;
        len = (unsigned char) *data++;
        size = 2;
        if (vol->v_obj->afp_version >= 30) {
            ret.m_type = 3;
            toUTF8 = 1;
        }
        break;
    case 3:
        if (vol->v_obj->afp_version >= 30) {
            data++;
            memcpy(&hint, data, sizeof(hint));
            hint = ntohl(hint);
            data += sizeof(hint);

            memcpy(&len16, data, sizeof(len16));
            len = ntohs(len16);
            data += 2;
            size = 7;
            break;
        }
        /* else it's an error */
    default:
        afp_errno = AFPERR_PARAM;
        return( NULL );
    }
    *cpath += len + size;

    path[0] = 0;
    ret.m_name = path;

    if (movecwd(vol, dir) < 0 ) {
        LOG(log_debug, logtype_afpd, "cname(did:%u): failed to chdir to '%s'",
            ntohl(dir->d_did), cfrombstr(dir->d_fullpath));
        if (len == 0)
            return path_from_dir(vol, dir, &ret);
        else
            return NULL;
    }

    while (len) {         /* 3 */
        if (*data == 0) { /* 4 or 5 */
            data++;
            len--;
            while (len > 0 && *data == 0) { /* 5 */
                /* chdir to parrent dir */
                if ((dir = dirlookup(vol, dir->d_pdid)) == NULL)
                    return NULL;
                if (movecwd( vol, dir ) < 0 ) {
                    dir_remove(vol, dir);
                    return NULL;
                }
                data++;
                len--;
            }
            continue;
        }

        /* 6*/
        for ( p = path; *data != 0 && len > 0; len-- ) {
            *p++ = *data++;
            if (p > &path[UTF8FILELEN_EARLY]) {   /* FIXME safeguard, limit of early Mac OS X */
                afp_errno = AFPERR_PARAM;
                return NULL;
            }
        }
        *p = 0;            /* Terminate string */
        ret.u_name = NULL;

        if (cname_mtouname(vol, dir, &ret, toUTF8) != 0) { /* 7 */
            LOG(log_error, logtype_afpd, "cname('%s'): error from cname_mtouname", path);
            return NULL;
        }

        LOG(log_maxdebug, logtype_afpd, "came('%s'): {node: '%s}", cfrombstr(dir->d_fullpath), ret.u_name);

        /* Prevent access to our special folders like .AppleDouble */
        if (check_name(vol, ret.u_name)) {
            /* the name is illegal */
            LOG(log_info, logtype_afpd, "cname: illegal path: '%s'", ret.u_name);
            afp_errno = AFPERR_PARAM;
            if (vol->v_obj->options.flags & OPTION_VETOMSG) {
                bstring message = bformat("Attempt to access vetoed file or directory \"%s\" in directory \"%s\"",
                                          ret.u_name, bdata(dir->d_u_name));
                if (setmessage(bdata(message)) == 0)
                    /* Client may make multiple attempts, only send the message the first time */
                    kill(getpid(), SIGUSR2);
                bdestroy(message);
            }
            return NULL;
        }

        if (dir->d_did == DIRDID_ROOT_PARENT) { /* 8 */
            /*
             * Special case: CNID 1
             * root parent (did 1) has one child: the volume. Requests for did=1 with
             * some <name> must check against the volume name.
             */
            if ((strcmp(cfrombstr(vol->v_root->d_m_name), ret.m_name)) == 0)
                cdir = vol->v_root;
            else
                return NULL;
        } else {
            /*
             * CNID != 1, eg. most of the times we take this way.
             * Now check if current path-part is a file or dir:
             * o if it's dir we have to step into it
             * o if it's a file we expect it to be the last part of the requested path
             *   and thus call continue which should terminate the while loop because
             *   len = 0. Ok?
             */
            if (of_stat(vol, &ret) != 0) { /* 9 */
                /*
                 * ret.u_name doesn't exist, might be afp_createfile|dir
                 * that means it should have been the last part
                 */
                if (len > 0) {
                    /* it wasn't the last part, so we have a bogus path request */
                    afp_errno = AFPERR_NOOBJ;
                    return NULL;
                }
                /*
                 * this will terminate clean in while (1) because len == 0,
                 * probably afp_createfile|dir
                 */
                LOG(log_maxdebug, logtype_afpd, "came('%s'): {leave-cnode ENOENT (possile create request): '%s'}",
                    cfrombstr(dir->d_fullpath), ret.u_name);
                continue; /* 10 */
            }

            switch (ret.st.st_mode & S_IFMT) {
            case S_IFREG: /* 11 */
                LOG(log_debug, logtype_afpd, "came('%s'): {file: '%s'}",
                    cfrombstr(dir->d_fullpath), ret.u_name);
                if (len > 0) {
                    /* it wasn't the last part, so we have a bogus path request */
                    afp_errno = AFPERR_PARAM;
                    return NULL;
                }
                continue; /* continues while loop */
            case S_IFLNK: /* 12 */
                LOG(log_debug, logtype_afpd, "came('%s'): {link: '%s'}",
                    cfrombstr(dir->d_fullpath), ret.u_name);
                if (len > 0) {
                    LOG(log_warning, logtype_afpd, "came('%s'): {symlinked dir: '%s'}",
                        cfrombstr(dir->d_fullpath), ret.u_name);
                    afp_errno = AFPERR_PARAM;
                    return NULL;
                }
                continue; /* continues while loop */
            case S_IFDIR: /* 13 */
                break;
            default:
                LOG(log_info, logtype_afpd, "cname: special file: '%s'", ret.u_name);
                afp_errno = AFPERR_NODIR;
                return NULL;
            }

            /* Search the cache */
            int unamelen = strlen(ret.u_name);
            cdir = dircache_search_by_name(vol, dir, ret.u_name, unamelen); /* 14 */
            if (cdir == NULL) {
                /* Not in cache, create one */
                if ((cdir = dir_add(vol, dir, &ret, unamelen)) == NULL) { /* 15 */
                    LOG(log_error, logtype_afpd, "cname(did:%u, name:'%s', cwd:'%s'): failed to add dir",
                        ntohl(dir->d_did), ret.u_name, getcwdpath());
                    return NULL;
                }
            }
        } /* if/else cnid==1 */

        /* Now chdir to the evaluated dir */
        if (movecwd( vol, cdir ) < 0 ) { /* 16 */
            LOG(log_debug, logtype_afpd, "cname(cwd:'%s'): failed to chdir to new subdir '%s': %s",
                cfrombstr(curdir->d_fullpath), cfrombstr(cdir->d_fullpath), strerror(errno));
            if (len == 0)
                return path_from_dir(vol, cdir, &ret);
            else
                return NULL;
        }
        dir = cdir;
        ret.m_name[0] = 0;      /* 17, so we later know last token was a dir */
    } /* while (len) */

    if (curdir->d_did == DIRDID_ROOT_PARENT) {
        afp_errno = AFPERR_DID1;
        return NULL;
    }

    if (ret.m_name[0] == 0) {
        /* Last part was a dir */
        ret.u_name = mtoupath(vol, ret.m_name, 0, 1); /* Force "." into a useable static buffer */
        ret.d_dir = dir;
    }

    LOG(log_debug, logtype_afpd, "came('%s') {end: curdir:'%s', path:'%s'}",
        cfrombstr(dir->d_fullpath),
        cfrombstr(curdir->d_fullpath),
        ret.u_name);

    return &ret;
}

/*
 * @brief chdir() to dir
 *
 * @param vol   (r) pointer to struct vol
 * @param dir   (r) pointer to struct dir
 *
 * @returns 0 on success, -1 on error with afp_errno set appropiately
 */
int movecwd(const struct vol *vol, struct dir *dir)
{
    int ret;

    AFP_ASSERT(vol);
    AFP_ASSERT(dir);

    LOG(log_maxdebug, logtype_afpd, "movecwd: from: curdir:\"%s\", cwd:\"%s\"",
        curdir ? cfrombstr(curdir->d_fullpath) : "INVALID", getcwdpath());

    if (dir->d_did == DIRDID_ROOT_PARENT) {
        curdir = &rootParent;
        return 0;
    }

    LOG(log_debug, logtype_afpd, "movecwd(to: did: %u, \"%s\")",
        ntohl(dir->d_did), cfrombstr(dir->d_fullpath));

    if ((ret = ochdir(cfrombstr(dir->d_fullpath), vol_syml_opt(vol))) != 0 ) {
        LOG(log_debug, logtype_afpd, "movecwd(\"%s\"): %s",
            cfrombstr(dir->d_fullpath), strerror(errno));
        if (ret == 1) {
            /* p is a symlink or getcwd failed */
            afp_errno = AFPERR_BADTYPE;

            if (chdir(vol->v_path ) < 0) {
                LOG(log_error, logtype_afpd, "can't chdir back'%s': %s", vol->v_path, strerror(errno));
                /* XXX what do we do here? */
            }
            curdir = vol->v_root;
            return -1;
        }

        switch (errno) {
        case EACCES:
        case EPERM:
            afp_errno = AFPERR_ACCESS;
            break;
        default:
            afp_errno = AFPERR_NOOBJ;
        }
        return( -1 );
    }

    curdir = dir;
    return( 0 );
}

/*
 * We can't use unix file's perm to support Apple's inherited protection modes.
 * If we aren't the file's owner we can't change its perms when moving it and smb
 * nfs,... don't even try.
 */
int check_access(const AFPObj *obj, struct vol *vol, char *path, int mode)
{
    struct maccess ma;
    char *p;

    p = ad_dir(path);
    if (!p)
        return -1;

    accessmode(obj, vol, p, &ma, curdir, NULL);
    if ((mode & OPENACC_WR) && !(ma.ma_user & AR_UWRITE))
        return -1;
    if ((mode & OPENACC_RD) && !(ma.ma_user & AR_UREAD))
        return -1;

    return 0;
}

/* --------------------- */
int file_access(const AFPObj *obj, struct vol *vol, struct path *path, int mode)
{
    struct maccess ma;

    accessmode(obj, vol, path->u_name, &ma, curdir, &path->st);

    LOG(log_debug, logtype_afpd, "file_access(\"%s\"): mapped user mode: 0x%02x",
        path->u_name, ma.ma_user);

    if ((mode & OPENACC_WR) && !(ma.ma_user & AR_UWRITE)) {
        LOG(log_debug, logtype_afpd, "file_access(\"%s\"): write access denied", path->u_name);
        return -1;
    }
    if ((mode & OPENACC_RD) && !(ma.ma_user & AR_UREAD)) {
        LOG(log_debug, logtype_afpd, "file_access(\"%s\"): read access denied", path->u_name);
        return -1;
    }
    return 0;

}

/* --------------------- */
void setdiroffcnt(struct dir *dir, struct stat *st,  uint32_t count)
{
    dir->d_offcnt = count;
    dir->d_ctime = st->st_ctime;
    dir->d_flags &= ~DIRF_CNID;
}


/* ---------------------
 * is our cached also for reenumerate id?
 */
int dirreenumerate(struct dir *dir, struct stat *st)
{
    return st->st_ctime == dir->d_ctime && (dir->d_flags & DIRF_CNID);
}

/* ------------------------------
   (".", curdir)
   (name, dir) with curdir:name == dir, from afp_enumerate
*/

int getdirparams(const AFPObj *obj,
                 const struct vol *vol,
                 uint16_t bitmap, struct path *s_path,
                 struct dir *dir,
                 char *buf, size_t *buflen )
{
    struct maccess  ma;
    struct adouble  ad;
    char        *data, *l_nameoff = NULL, *utf_nameoff = NULL;
    int         bit = 0, isad = 0;
    uint32_t           aint;
    uint16_t       ashort;
    int                 ret;
    uint32_t           utf8 = 0;
    cnid_t              pdid;
    struct stat *st = &s_path->st;
    char *upath = s_path->u_name;

    if ((bitmap & ((1 << DIRPBIT_ATTR)  |
                   (1 << DIRPBIT_CDATE) |
                   (1 << DIRPBIT_MDATE) |
                   (1 << DIRPBIT_BDATE) |
                   (1 << DIRPBIT_FINFO)))) {

        ad_init(&ad, vol);
        if ( !ad_metadata( upath, ADFLAGS_DIR, &ad) ) {
            isad = 1;
            if (ad.ad_mdp->adf_flags & O_CREAT) {
                /* We just created it */
                if (s_path->m_name == NULL) {
                    if ((s_path->m_name = utompath(vol,
                                                   upath,
                                                   dir->d_did,
                                                   utf8_encoding(obj))) == NULL) {
                        LOG(log_error, logtype_afpd,
                            "getdirparams(\"%s\"): can't assign macname",
                            cfrombstr(dir->d_fullpath));
                        return AFPERR_MISC;
                    }
                }
                ad_setname(&ad, s_path->m_name);
                ad_setid( &ad,
                          s_path->st.st_dev,
                          s_path->st.st_ino,
                          dir->d_did,
                          dir->d_pdid,
                          vol->v_stamp);
                ad_flush( &ad);
            }
        }
    }

    pdid = dir->d_pdid;

    data = buf;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch ( bit ) {
        case DIRPBIT_ATTR :
            if ( isad ) {
                ad_getattr(&ad, &ashort);
            } else if (invisible_dots(vol, cfrombstr(dir->d_u_name))) {
                ashort = htons(ATTRBIT_INVISIBLE);
            } else
                ashort = 0;
            ashort &= ~htons(vol->v_ignattr);
            memcpy( data, &ashort, sizeof( ashort ));
            data += sizeof( ashort );
            break;

        case DIRPBIT_PDID :
            memcpy( data, &pdid, sizeof( pdid ));
            data += sizeof( pdid );
            LOG(log_debug, logtype_afpd, "metadata('%s'):     Parent DID: %u",
                s_path->u_name, ntohl(pdid));
            break;

        case DIRPBIT_CDATE :
            if (!isad || (ad_getdate(&ad, AD_DATE_CREATE, &aint) < 0))
                aint = AD_DATE_FROM_UNIX(st->st_mtime);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case DIRPBIT_MDATE :
            aint = AD_DATE_FROM_UNIX(st->st_mtime);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case DIRPBIT_BDATE :
            if (!isad || (ad_getdate(&ad, AD_DATE_BACKUP, &aint) < 0))
                aint = AD_DATE_START;
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case DIRPBIT_FINFO :
            if ( isad ) {
                memcpy( data, ad_entry( &ad, ADEID_FINDERI ), 32 );
            } else { /* no appledouble */
                memset( data, 0, 32 );
                /* dot files are by default visible */
                if (invisible_dots(vol, cfrombstr(dir->d_u_name))) {
                    ashort = htons(FINDERINFO_INVISIBLE);
                    memcpy(data + FINDERINFO_FRFLAGOFF, &ashort, sizeof(ashort));
                }
            }
            data += 32;
            break;

        case DIRPBIT_LNAME :
            if (dir->d_m_name) /* root of parent can have a null name */
                l_nameoff = data;
            else
                memset(data, 0, sizeof(uint16_t));
            data += sizeof( uint16_t );
            break;

        case DIRPBIT_SNAME :
            memset(data, 0, sizeof(uint16_t));
            data += sizeof( uint16_t );
            break;

        case DIRPBIT_DID :
            memcpy( data, &dir->d_did, sizeof( aint ));
            data += sizeof( aint );
            LOG(log_debug, logtype_afpd, "metadata('%s'):            DID: %u",
                s_path->u_name, ntohl(dir->d_did));
            break;

        case DIRPBIT_OFFCNT :
            ashort = 0;
            /* this needs to handle current directory access rights */
            if (diroffcnt(dir, st)) {
                ashort = (dir->d_offcnt > 0xffff)?0xffff:dir->d_offcnt;
            }
            else if ((ret = for_each_dirent(vol, upath, NULL,NULL)) >= 0) {
                setdiroffcnt(dir, st,  ret);
                ashort = (dir->d_offcnt > 0xffff)?0xffff:dir->d_offcnt;
            }
            ashort = htons( ashort );
            memcpy( data, &ashort, sizeof( ashort ));
            data += sizeof( ashort );
            break;

        case DIRPBIT_UID :
            aint = htonl(st->st_uid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case DIRPBIT_GID :
            aint = htonl(st->st_gid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case DIRPBIT_ACCESS :
            accessmode(obj, vol, upath, &ma, dir , st);

            *data++ = ma.ma_user;
            *data++ = ma.ma_world;
            *data++ = ma.ma_group;
            *data++ = ma.ma_owner;
            break;

            /* Client has requested the ProDOS information block.
               Just pass back the same basic block for all
               directories. <shirsch@ibm.net> */
        case DIRPBIT_PDINFO :
            if (obj->afp_version >= 30) { /* UTF8 name */
                utf8 = kTextEncodingUTF8;
                if (dir->d_m_name) /* root of parent can have a null name */
                    utf_nameoff = data;
                else
                    memset(data, 0, sizeof(uint16_t));
                data += sizeof( uint16_t );
                aint = 0;
                memcpy(data, &aint, sizeof( aint ));
                data += sizeof( aint );
            }
            else { /* ProDOS Info Block */
                *data++ = 0x0f;
                *data++ = 0;
                ashort = htons( 0x0200 );
                memcpy( data, &ashort, sizeof( ashort ));
                data += sizeof( ashort );
                memset( data, 0, sizeof( ashort ));
                data += sizeof( ashort );
            }
            break;

        case DIRPBIT_UNIXPR :
            /* accessmode may change st_mode with ACLs */
            accessmode(obj, vol, upath, &ma, dir, st);

            aint = htonl(st->st_uid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            aint = htonl(st->st_gid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );

            aint = st->st_mode;
            aint = htonl ( aint & ~S_ISGID );  /* Remove SGID, OSX doesn't like it ... */
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );

            *data++ = ma.ma_user;
            *data++ = ma.ma_world;
            *data++ = ma.ma_group;
            *data++ = ma.ma_owner;
            break;

        default :
            if ( isad ) {
                ad_close(&ad, ADFLAGS_HF);
            }
            return( AFPERR_BITMAP );
        }
        bitmap = bitmap>>1;
        bit++;
    }
    if ( l_nameoff ) {
        ashort = htons( data - buf );
        memcpy( l_nameoff, &ashort, sizeof( ashort ));
        data = set_name(vol, data, pdid, cfrombstr(dir->d_m_name), dir->d_did, 0);
    }
    if ( utf_nameoff ) {
        ashort = htons( data - buf );
        memcpy( utf_nameoff, &ashort, sizeof( ashort ));
        data = set_name(vol, data, pdid, cfrombstr(dir->d_m_name), dir->d_did, utf8);
    }
    if ( isad ) {
        ad_close(&ad, ADFLAGS_HF);
    }
    *buflen = data - buf;
    return( AFP_OK );
}

/* ----------------------------- */
int path_error(struct path *path, int error)
{
/* - a dir with access error
 * - no error it's a file
 * - file not found
 */
    if (path_isadir(path))
        return afp_errno;
    if (path->st_valid && path->st_errno)
        return error;
    return AFPERR_BADTYPE ;
}

/* ----------------------------- */
int afp_setdirparams(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol  *vol;
    struct dir  *dir;
    struct path *path;
    uint16_t   vid, bitmap;
    uint32_t   did;
    int     rc;

    *rbuflen = 0;
    ibuf += 2;
    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( int );

    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    memcpy( &bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    if ( *path->m_name != '\0' ) {
        rc = path_error(path, AFPERR_NOOBJ);
        /* maybe we are trying to set perms back */
        if (rc != AFPERR_ACCESS)
            return rc;
    }

    /*
     * If ibuf is odd, make it even.
     */
    if ((u_long)ibuf & 1 ) {
        ibuf++;
    }

    if (AFP_OK == ( rc = setdirparams(vol, path, bitmap, ibuf )) ) {
        setvoltime(obj, vol );
    }
    return( rc );
}

/*
 * assume path == '\0' eg. it's a directory in canonical form
 */
int setdirparams(struct vol *vol, struct path *path, uint16_t d_bitmap, char *buf )
{
    struct maccess  ma;
    struct adouble  ad;
    struct utimbuf      ut;
    struct timeval      tv;

    char                *upath;
    struct dir          *dir;
    int         bit, isad = 0;
    int                 cdate, bdate;
    int                 owner, group;
    uint16_t       ashort, bshort, oshort;
    int                 err = AFP_OK;
    int                 change_mdate = 0;
    int                 change_parent_mdate = 0;
    int                 newdate = 0;
    uint16_t           bitmap = d_bitmap;
    u_char              finder_buf[32];
    uint32_t       upriv;
    mode_t              mpriv = 0;
    bool                set_upriv = false, set_maccess = false;

    LOG(log_debug, logtype_afpd, "setdirparams(\"%s\", bitmap: %02x)", path->u_name, bitmap);

    bit = 0;
    upath = path->u_name;
    dir   = path->d_dir;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch( bit ) {
        case DIRPBIT_ATTR :
            change_mdate = 1;
            memcpy( &ashort, buf, sizeof( ashort ));
            buf += sizeof( ashort );
            break;
        case DIRPBIT_CDATE :
            change_mdate = 1;
            memcpy(&cdate, buf, sizeof(cdate));
            buf += sizeof( cdate );
            break;
        case DIRPBIT_MDATE :
            memcpy(&newdate, buf, sizeof(newdate));
            buf += sizeof( newdate );
            break;
        case DIRPBIT_BDATE :
            change_mdate = 1;
            memcpy(&bdate, buf, sizeof(bdate));
            buf += sizeof( bdate );
            break;
        case DIRPBIT_FINFO :
            change_mdate = 1;
            memcpy( finder_buf, buf, 32 );
            buf += 32;
            break;
        case DIRPBIT_UID :  /* What kind of loser mounts as root? */
            memcpy( &owner, buf, sizeof(owner));
            buf += sizeof( owner );
            break;
        case DIRPBIT_GID :
            memcpy( &group, buf, sizeof( group ));
            buf += sizeof( group );
            break;
        case DIRPBIT_ACCESS :
            set_maccess = true;
            change_mdate = 1;
            ma.ma_user = *buf++;
            ma.ma_world = *buf++;
            ma.ma_group = *buf++;
            ma.ma_owner = *buf++;
            mpriv = mtoumode( &ma ) | vol->v_dperm;
            break;
            /* Ignore what the client thinks we should do to the
               ProDOS information block.  Skip over the data and
               report nothing amiss. <shirsch@ibm.net> */
        case DIRPBIT_PDINFO :
            if (vol->v_obj->afp_version < 30) {
                buf += 6;
            }
            else {
                err = AFPERR_BITMAP;
                bitmap = 0;
            }
            break;
        case DIRPBIT_UNIXPR :
            if (vol_unix_priv(vol)) {
                set_upriv = true;
                memcpy( &owner, buf, sizeof(owner)); /* FIXME need to change owner too? */
                buf += sizeof( owner );
                memcpy( &group, buf, sizeof( group ));
                buf += sizeof( group );

                change_mdate = 1;
                memcpy( &upriv, buf, sizeof( upriv ));
                buf += sizeof( upriv );
                upriv = ntohl (upriv) | vol->v_dperm;
                break;
            }
            /* fall through */
        default :
            err = AFPERR_BITMAP;
            bitmap = 0;
            break;
        }

        bitmap = bitmap>>1;
        bit++;
    }

    if (d_bitmap & ((1<<DIRPBIT_ATTR) | (1<<DIRPBIT_CDATE) | (1<<DIRPBIT_BDATE) | (1<<DIRPBIT_FINFO))) {
        ad_init(&ad, vol);
        if (ad_open(&ad, upath, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_CREATE | ADFLAGS_RDWR, 0777) != 0) {
            LOG(log_debug, logtype_afpd, "setdirparams(\"%s\", bitmap: %02x): need adouble", path->u_name, d_bitmap);
            return AFPERR_ACCESS;
        }
        if ((ad_get_MD_flags(&ad) & O_CREAT)) {
            ad_setname(&ad, cfrombstr(curdir->d_m_name));
        }
        isad = 1;
    }

    bit = 0;
    bitmap = d_bitmap;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch( bit ) {
        case DIRPBIT_ATTR :
            if (isad) {
                ad_getattr(&ad, &bshort);
                oshort = bshort;
                if ( ntohs( ashort ) & ATTRBIT_SETCLR ) {
                    ashort &= ~htons(vol->v_ignattr);
                    bshort |= htons( ntohs( ashort ) & ~ATTRBIT_SETCLR );
                } else {
                    bshort &= ~ashort;
                }
                if ((bshort & htons(ATTRBIT_INVISIBLE)) != (oshort & htons(ATTRBIT_INVISIBLE)))
                    change_parent_mdate = 1;
                ad_setattr(&ad, bshort);
            }
            break;
        case DIRPBIT_CDATE :
            if (isad) {
                ad_setdate(&ad, AD_DATE_CREATE, cdate);
            }
            break;
        case DIRPBIT_MDATE :
            break;
        case DIRPBIT_BDATE :
            if (isad) {
                ad_setdate(&ad, AD_DATE_BACKUP, bdate);
            }
            break;
        case DIRPBIT_FINFO :
            if (isad) {
                /* Fixes #2802236 */
                uint16_t fflags;
                memcpy(&fflags, finder_buf + FINDERINFO_FRFLAGOFF, sizeof(uint16_t));
                fflags &= htons(~FINDERINFO_ISHARED);
                memcpy(finder_buf + FINDERINFO_FRFLAGOFF, &fflags, sizeof(uint16_t));
                /* #2802236 end */

                if (  dir->d_did == DIRDID_ROOT ) {
                    /*
                     * Alright, we admit it, this is *really* sick!
                     * The 4 bytes that we don't copy, when we're dealing
                     * with the root of a volume, are the directory's
                     * location information. This eliminates that annoying
                     * behavior one sees when mounting above another mount
                     * point.
                     */
                    memcpy( ad_entry( &ad, ADEID_FINDERI ), finder_buf, 10 );
                    memcpy( ad_entry( &ad, ADEID_FINDERI ) + 14, finder_buf + 14, 18 );
                } else {
                    memcpy( ad_entry( &ad, ADEID_FINDERI ), finder_buf, 32 );
                }
            }
            break;
        case DIRPBIT_UID :  /* What kind of loser mounts as root? */
            if ( (dir->d_did == DIRDID_ROOT) &&
                 (setdeskowner(vol, ntohl(owner), -1 ) < 0)) {
                err = set_dir_errors(path, "setdeskowner", errno);
                if (isad && err == AFPERR_PARAM) {
                    err = AFP_OK; /* ???*/
                }
                else {
                    goto setdirparam_done;
                }
            }
            if ( setdirowner(vol, upath, ntohl(owner), -1 ) < 0 ) {
                err = set_dir_errors(path, "setdirowner", errno);
                goto setdirparam_done;
            }
            break;
        case DIRPBIT_GID :
            if (dir->d_did == DIRDID_ROOT)
                setdeskowner(vol, -1, ntohl(group) );
            if ( setdirowner(vol, upath, -1, ntohl(group) ) < 0 ) {
                err = set_dir_errors(path, "setdirowner", errno);
                goto setdirparam_done;
            }
            break;
        case DIRPBIT_ACCESS :
            break;
        case DIRPBIT_PDINFO :
            if (vol->v_obj->afp_version >= 30) {
                err = AFPERR_BITMAP;
                goto setdirparam_done;
            }
            break;
        case DIRPBIT_UNIXPR :
            if (!vol_unix_priv(vol)) {
                err = AFPERR_BITMAP;
                goto setdirparam_done;
            }
            break;
        default :
            err = AFPERR_BITMAP;
            goto setdirparam_done;
            break;
        }

        bitmap = bitmap>>1;
        bit++;
    }

setdirparam_done:
    if (change_mdate && newdate == 0 && gettimeofday(&tv, NULL) == 0) {
        newdate = AD_DATE_FROM_UNIX(tv.tv_sec);
    }
    if (newdate) {
        if (isad)
            ad_setdate(&ad, AD_DATE_MODIFY, newdate);
        ut.actime = ut.modtime = AD_DATE_TO_UNIX(newdate);
        utime(upath, &ut);
    }

    if (isad) {
        if (path->st_valid && !path->st_errno) {
            struct stat *st = &path->st;
            if (dir && dir->d_pdid) {
                ad_setid(&ad, st->st_dev, st->st_ino,  dir->d_did, dir->d_pdid, vol->v_stamp);
            }
        }
        if (ad_flush(&ad) != 0) {
            switch (errno) {
            case EACCES:
                err = AFPERR_ACCESS;
                break;
            default:
                err = AFPERR_MISC;
                break;
           }
        }
        ad_close(&ad, ADFLAGS_HF);
    }

    if (err == AFP_OK) {
        if (set_maccess == true) {
            if (dir->d_did == DIRDID_ROOT) {
                setdeskmode(vol, mpriv);
                if (!dir_rx_set(mpriv)) {
                    /* we can't remove read and search for owner on volume root */
                    err = AFPERR_ACCESS;
                    goto setprivdone;
                }
            }
            if (setdirunixmode(vol, upath, mpriv) < 0) {
                LOG(log_info, logtype_afpd, "setdirparams(\"%s\"): setdirunixmode: %s",
                    fullpathname(upath), strerror(errno));
                err = set_dir_errors(path, "setdirmode", errno);
            }
        }
        if ((set_upriv == true) && vol_unix_priv(vol)) {
            if (dir->d_did == DIRDID_ROOT) {
                if (!dir_rx_set(upriv)) {
                    /* we can't remove read and search for owner on volume root */
                    err = AFPERR_ACCESS;
                    goto setprivdone;
                }
                setdeskowner(vol, -1, ntohl(group));
                setdeskmode(vol, upriv);
            }

            if (setdirowner(vol, upath, -1, ntohl(group)) < 0) {
                LOG(log_info, logtype_afpd, "setdirparams(\"%s\"): setdirowner: %s",
                    fullpathname(upath), strerror(errno));
                err = set_dir_errors(path, "setdirowner", errno);
                goto setprivdone;
            }

            if (setdirunixmode(vol, upath, upriv) < 0) {
                LOG(log_info, logtype_afpd, "setdirparams(\"%s\"): setdirunixmode: %s",
                    fullpathname(upath), strerror(errno));
                err = set_dir_errors(path, "setdirunixmode", errno);
            }
        }
    }

setprivdone:
    if (change_parent_mdate && dir->d_did != DIRDID_ROOT
        && gettimeofday(&tv, NULL) == 0) {
        if (movecwd(vol, dirlookup(vol, dir->d_pdid)) == 0) {
            newdate = AD_DATE_FROM_UNIX(tv.tv_sec);
            /* be careful with bitmap because now dir is null */
            bitmap = 1<<DIRPBIT_MDATE;
            setdirparams(vol, &Cur_Path, bitmap, (char *)&newdate);
            /* should we reset curdir ?*/
        }
    }
    return err;
}

int afp_syncdir(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
#ifdef HAVE_DIRFD
    DIR                  *dp;
#endif
    int                  dfd;
    struct vol           *vol;
    struct dir           *dir;
    uint32_t            did;
    uint16_t            vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == (vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );

    /*
     * Here's the deal:
     * if it's CNID 2 our only choice to meet the specs is call sync.
     * For any other CNID just sync that dir. To my knowledge the
     * intended use of FPSyncDir is to sync the volume so all we're
     * ever going to see here is probably CNID 2. Anyway, we' prepared.
     */

    if ( ntohl(did) == 2 ) {
        sync();
    } else {
        if (NULL == ( dir = dirlookup( vol, did )) ) {
            return afp_errno; /* was AFPERR_NOOBJ */
        }

        if (movecwd( vol, dir ) < 0 )
            return ( AFPERR_NOOBJ );

        /*
         * Assuming only OSens that have dirfd also may require fsyncing directories
         * in order to flush metadata e.g. Linux.
         */

#ifdef HAVE_DIRFD
        if (NULL == ( dp = opendir( "." )) ) {
            switch( errno ) {
            case ENOENT :
                return( AFPERR_NOOBJ );
            case EACCES :
                return( AFPERR_ACCESS );
            default :
                return( AFPERR_PARAM );
            }
        }

        LOG(log_debug, logtype_afpd, "afp_syncdir: dir: '%s'", dir->d_u_name);

        dfd = dirfd( dp );
        if ( fsync ( dfd ) < 0 )
            LOG(log_error, logtype_afpd, "afp_syncdir(%s):  %s",
                dir->d_u_name, strerror(errno) );
        closedir(dp); /* closes dfd too */
#endif

        if ( -1 == (dfd = open(vol->ad_path(".", ADFLAGS_DIR), O_RDWR))) {
            switch( errno ) {
            case ENOENT:
                return( AFPERR_NOOBJ );
            case EACCES:
                return( AFPERR_ACCESS );
            default:
                return( AFPERR_PARAM );
            }
        }

        LOG(log_debug, logtype_afpd, "afp_syncdir: ad-file: '%s'",
            vol->ad_path(".", ADFLAGS_DIR) );

        if ( fsync(dfd) < 0 )
            LOG(log_error, logtype_afpd, "afp_syncdir(%s): %s",
                vol->ad_path(cfrombstr(dir->d_u_name), ADFLAGS_DIR), strerror(errno) );
        close(dfd);
    }

    return ( AFP_OK );
}

int afp_createdir(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct adouble  ad;
    struct vol      *vol;
    struct dir      *dir;
    char        *upath;
    struct path         *s_path;
    uint32_t       did;
    uint16_t       vid;
    int                 err;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno; /* was AFPERR_NOOBJ */
    }
    /* for concurrent access we need to be sure we are not in the
     * folder we want to create...
     */
    movecwd(vol, dir);

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }
    /* cname was able to move curdir to it! */
    if (*s_path->m_name == '\0')
        return AFPERR_EXIST;

    upath = s_path->u_name;

    if (AFP_OK != (err = netatalk_mkdir(vol, upath))) {
        return err;
    }

    if (of_stat(vol, s_path) < 0) {
        return AFPERR_MISC;
    }

    curdir->d_offcnt++;

    if ((dir = dir_add(vol, curdir, s_path, strlen(s_path->u_name))) == NULL) {
        return AFPERR_MISC;
    }

    if ( movecwd( vol, dir ) < 0 ) {
        return( AFPERR_PARAM );
    }

    ad_init(&ad, vol);
    if (ad_open(&ad, ".", ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_CREATE | ADFLAGS_RDWR, 0777) < 0)  {
        return( AFPERR_ACCESS );
    }
    ad_setname(&ad, s_path->m_name);
    ad_setid( &ad, s_path->st.st_dev, s_path->st.st_ino, dir->d_did, did, vol->v_stamp);

    fce_register(FCE_DIR_CREATE, bdata(curdir->d_fullpath), NULL, fce_dir);

    ad_flush(&ad);
    ad_close(&ad, ADFLAGS_HF);

    memcpy( rbuf, &dir->d_did, sizeof( uint32_t ));
    *rbuflen = sizeof( uint32_t );
    setvoltime(obj, vol );
    return( AFP_OK );
}

/*
 * dst       new unix filename (not a pathname)
 * newname   new mac name
 * newparent curdir
 * dirfd     -1 means ignore dirfd (or use AT_FDCWD), otherwise src is relative to dirfd
 */
int renamedir(struct vol *vol,
              int dirfd,
              char *src,
              char *dst,
              struct dir *dir,
              struct dir *newparent,
              char *newname)
{
    struct adouble  ad;
    int             err;

    /* existence check moved to afp_moveandrename */
    if ( unix_rename(dirfd, src, -1, dst ) < 0 ) {
        switch ( errno ) {
        case ENOENT :
            return( AFPERR_NOOBJ );
        case EACCES :
            return( AFPERR_ACCESS );
        case EROFS:
            return AFPERR_VLOCK;
        case EINVAL:
            /* tried to move directory into a subdirectory of itself */
            return AFPERR_CANTMOVE;
        case EXDEV:
            /* this needs to copy and delete. bleah. that means we have
             * to deal with entire directory hierarchies. */
            if ((err = copydir(vol, newparent, dirfd, src, dst)) < 0) {
                deletedir(vol, -1, dst);
                return err;
            }
            if ((err = deletedir(vol, dirfd, src)) < 0)
                return err;
            break;
        default :
            return( AFPERR_PARAM );
        }
    }

    vol->vfs->vfs_renamedir(vol, dirfd, src, dst);

    ad_init(&ad, vol);

    if (ad_open(&ad, dst, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_RDWR) == 0) {
        ad_setname(&ad, newname);
        ad_flush(&ad);
        ad_close(&ad, ADFLAGS_HF);
    }

    return( AFP_OK );
}

/* delete an empty directory */
int deletecurdir(struct vol *vol)
{
    struct dirent *de;
    struct stat st;
    struct dir  *fdir, *pdir;
    struct adouble  ad;
    uint16_t       ashort;
    int err;

    if ((pdir = dirlookup(vol, curdir->d_pdid)) == NULL) {
        return( AFPERR_ACCESS );
    }

    fdir = curdir;

    ad_init(&ad, vol);
    /* we never want to create a resource fork here, we are going to delete it */
    if ( ad_metadata( ".", ADFLAGS_DIR, &ad) == 0 ) {

        ad_getattr(&ad, &ashort);
        ad_close(&ad, ADFLAGS_HF);
        if (!(vol->v_ignattr & ATTRBIT_NODELETE) && (ashort & htons(ATTRBIT_NODELETE))) {
            return  AFPERR_OLOCK;
        }
    }
    err = vol->vfs->vfs_deletecurdir(vol);
    if (err) {
        LOG(log_error, logtype_afpd, "deletecurdir: error deleting AppleDouble files in \"%s\"",
            cfrombstr(curdir->d_fullpath));
        return err;
    }

    if (movecwd(vol, pdir) < 0) {
        err = afp_errno;
        goto delete_done;
    }

    LOG(log_debug, logtype_afpd, "deletecurdir: moved to \"%s\"",
        cfrombstr(curdir->d_fullpath));

    err = netatalk_rmdir_all_errors(-1, cfrombstr(fdir->d_u_name));

    switch (err) {
    case AFP_OK:
    case AFPERR_NOOBJ:
        break;
    case AFPERR_DIRNEMPT:
        if (delete_vetoed_files(vol, bdata(fdir->d_u_name), false) != 0)
            goto delete_done;
        err = AFP_OK;
        break;
    default:
        LOG(log_error, logtype_afpd, "deletecurdir(\"%s\"): netatalk_rmdir_all_errors error",
            cfrombstr(curdir->d_fullpath));
        goto delete_done;
    }

    AFP_CNID_START("cnid_delete");
    cnid_delete(vol->v_cdb, fdir->d_did);
    AFP_CNID_DONE();

    dir_remove( vol, fdir );

delete_done:
    return err;
}

int afp_mapid(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct passwd   *pw;
    struct group    *gr;
    char        *name;
    uint32_t           id;
    int         len, sfunc;
    int         utf8 = 0;

    ibuf++;
    sfunc = (unsigned char) *ibuf++;
    *rbuflen = 0;

    if (sfunc >= 3 && sfunc <= 6) {
        if (obj->afp_version < 30) {
            return( AFPERR_PARAM );
        }
        utf8 = 1;
    }

    switch ( sfunc ) {
    case 1 :
    case 3 :/* unicode */
        memcpy( &id, ibuf, sizeof( id ));
        id = ntohl(id);
        if ( id != 0 ) {
            if (( pw = getpwuid( id )) == NULL ) {
                return( AFPERR_NOITEM );
            }
            len = convert_string_allocate( obj->options.unixcharset, ((!utf8)?obj->options.maccharset:CH_UTF8_MAC),
                                           pw->pw_name, -1, &name);
        } else {
            len = 0;
            name = NULL;
        }
        break;
    case 2 :
    case 4 : /* unicode */
        memcpy( &id, ibuf, sizeof( id ));
        id = ntohl(id);
        if ( id != 0 ) {
            if (NULL == ( gr = (struct group *)getgrgid( id ))) {
                return( AFPERR_NOITEM );
            }
            len = convert_string_allocate( obj->options.unixcharset, (!utf8)?obj->options.maccharset:CH_UTF8_MAC,
                                           gr->gr_name, -1, &name);
        } else {
            len = 0;
            name = NULL;
        }
        break;

    case 5 : /* UUID -> username */
    case 6 : /* UUID -> groupname */
        if ((obj->afp_version < 32) || !(obj->options.flags & OPTION_UUID ))
            return AFPERR_PARAM;
        LOG(log_debug, logtype_afpd, "afp_mapid: valid UUID request");
        uuidtype_t type;
        len = getnamefromuuid((unsigned char*) ibuf, &name, &type);
        if (len != 0)       /* its a error code, not len */
            return AFPERR_NOITEM;
        switch (type) {
        case UUID_USER:
            if (( pw = getpwnam( name )) == NULL )
                return( AFPERR_NOITEM );
            LOG(log_debug, logtype_afpd, "afp_mapid: name:%s -> uid:%d", name, pw->pw_uid);
            id = htonl(UUID_USER);
            memcpy( rbuf, &id, sizeof( id ));
            id = htonl( pw->pw_uid);
            rbuf += sizeof( id );
            memcpy( rbuf, &id, sizeof( id ));
            rbuf += sizeof( id );
            *rbuflen = 2 * sizeof( id );
            break;
        case UUID_GROUP:
            if (( gr = getgrnam( name )) == NULL )
                return( AFPERR_NOITEM );
            LOG(log_debug, logtype_afpd, "afp_mapid: group:%s -> gid:%d", name, gr->gr_gid);
            id = htonl(UUID_GROUP);
            memcpy( rbuf, &id, sizeof( id ));
            rbuf += sizeof( id );
            id = htonl( gr->gr_gid);
            memcpy( rbuf, &id, sizeof( id ));
            rbuf += sizeof( id );
            *rbuflen = 2 * sizeof( id );
            break;
        default:
            return AFPERR_MISC;
        }
        break;

    default :
        return( AFPERR_PARAM );
    }

    if (name)
        len = strlen( name );

    if (utf8) {
        uint16_t tp = htons(len);
        memcpy(rbuf, &tp, sizeof(tp));
        rbuf += sizeof(tp);
        *rbuflen += 2;
    }
    else {
        *rbuf++ = len;
        *rbuflen += 1;
    }
    if ( len > 0 ) {
        memcpy( rbuf, name, len );
    }
    *rbuflen += len;
    if (name)
        free(name);
    return( AFP_OK );
}

int afp_mapname(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct passwd   *pw;
    struct group    *gr;
    int             len, sfunc;
    uint32_t       id;
    uint16_t       ulen;

    ibuf++;
    sfunc = (unsigned char) *ibuf++;
    *rbuflen = 0;
    LOG(log_debug, logtype_afpd, "afp_mapname: sfunc: %d", sfunc);
    switch ( sfunc ) {
    case 1 :
    case 2 : /* unicode */
        if (obj->afp_version < 30) {
            return( AFPERR_PARAM );
        }
        memcpy(&ulen, ibuf, sizeof(ulen));
        len = ntohs(ulen);
        ibuf += 2;
        LOG(log_debug, logtype_afpd, "afp_mapname: alive");
        break;
    case 3 :
    case 4 :
        len = (unsigned char) *ibuf++;
        break;
    case 5 : /* username -> UUID  */
    case 6 : /* groupname -> UUID */
        if ((obj->afp_version < 32) || !(obj->options.flags & OPTION_UUID ))
            return AFPERR_PARAM;
        memcpy(&ulen, ibuf, sizeof(ulen));
        len = ntohs(ulen);
        ibuf += 2;
        break;
    default :
        return( AFPERR_PARAM );
    }

    ibuf[ len ] = '\0';

    if ( len == 0 )
        return AFPERR_PARAM;
    else {
        switch ( sfunc ) {
        case 1 : /* unicode */
        case 3 :
            if (NULL == ( pw = (struct passwd *)getpwnam( ibuf )) ) {
                return( AFPERR_NOITEM );
            }
            id = pw->pw_uid;
            id = htonl(id);
            memcpy( rbuf, &id, sizeof( id ));
            *rbuflen = sizeof( id );
            break;

        case 2 : /* unicode */
        case 4 :
            LOG(log_debug, logtype_afpd, "afp_mapname: getgrnam for name: %s",ibuf);
            if (NULL == ( gr = (struct group *)getgrnam( ibuf ))) {
                return( AFPERR_NOITEM );
            }
            id = gr->gr_gid;
            LOG(log_debug, logtype_afpd, "afp_mapname: getgrnam for name: %s -> id: %d",ibuf, id);
            id = htonl(id);
            memcpy( rbuf, &id, sizeof( id ));
            *rbuflen = sizeof( id );
            break;
        case 5 :        /* username -> UUID */
            LOG(log_debug, logtype_afpd, "afp_mapname: name: %s",ibuf);
            if (0 != getuuidfromname(ibuf, UUID_USER, (unsigned char *)rbuf))
                return AFPERR_NOITEM;
            *rbuflen = UUID_BINSIZE;
            break;
        case 6 :        /* groupname -> UUID */
            LOG(log_debug, logtype_afpd, "afp_mapname: name: %s",ibuf);
            if (0 != getuuidfromname(ibuf, UUID_GROUP, (unsigned char *)rbuf))
                return AFPERR_NOITEM;
            *rbuflen = UUID_BINSIZE;
            break;
        }
    }
    return( AFP_OK );
}

/* ------------------------------------
   variable DID support
*/
int afp_closedir(AFPObj *obj _U_, char *ibuf _U_, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
#if 0
    struct vol   *vol;
    struct dir   *dir;
    uint16_t    vid;
    uint32_t    did;
#endif /* 0 */

    *rbuflen = 0;

    /* do nothing as dids are static for the life of the process. */
#if 0
    ibuf += 2;

    memcpy(&vid,  ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (( vol = getvolbyvid( vid )) == NULL ) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (( dir = dirlookup( vol, did )) == NULL ) {
        return( AFPERR_PARAM );
    }

    /* dir_remove -- deletedid */
#endif /* 0 */

    return AFP_OK;
}

/* did creation gets done automatically
 * there's a pb again with case but move it to cname
 */
int afp_opendir(AFPObj *obj _U_, char *ibuf, size_t ibuflen  _U_, char *rbuf, size_t *rbuflen)
{
    struct vol      *vol;
    struct dir      *parentdir;
    struct path     *path;
    uint32_t       did;
    uint16_t       vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof( vid );

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy(&did, ibuf, sizeof(did));
    ibuf += sizeof(did);

    if (NULL == ( parentdir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    if (NULL == ( path = cname( vol, parentdir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }

    if ( *path->m_name != '\0' ) {
        return path_error(path, AFPERR_NOOBJ);
    }

    if ( !path->st_valid && of_stat(vol, path) < 0 ) {
        return( AFPERR_NOOBJ );
    }
    if ( path->st_errno ) {
        return( AFPERR_NOOBJ );
    }

    memcpy(rbuf, &curdir->d_did, sizeof(curdir->d_did));
    *rbuflen = sizeof(curdir->d_did);
    return AFP_OK;
}
