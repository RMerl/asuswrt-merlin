/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/param.h>

#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/logger.h>
#include <atalk/unix.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/acl.h>
#include <atalk/globals.h>
#include <atalk/fce_api.h>
#include <atalk/netatalk_conf.h>
#include <atalk/errchk.h>

#include "directory.h"
#include "dircache.h"
#include "desktop.h"
#include "volume.h"
#include "fork.h"
#include "file.h"
#include "filedir.h"
#include "unix.h"

int afp_getfildirparams(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct stat     *st;
    struct vol      *vol;
    struct dir      *dir;
    uint32_t           did;
    int         ret;
    size_t      buflen;
    uint16_t       fbitmap, dbitmap, vid;
    struct path         *s_path;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        /* was AFPERR_PARAM but it helps OS 10.3 when a volume has been removed
         * from the list.
         */
        return( AFPERR_ACCESS );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );

    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    memcpy( &fbitmap, ibuf, sizeof( fbitmap ));
    fbitmap = ntohs( fbitmap );
    ibuf += sizeof( fbitmap );
    memcpy( &dbitmap, ibuf, sizeof( dbitmap ));
    dbitmap = ntohs( dbitmap );
    ibuf += sizeof( dbitmap );

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    LOG(log_debug, logtype_afpd, "getfildirparams(vid:%u, did:%u, f/d:%04x/%04x) {cwdid:%u, cwd: %s, name:'%s'}",
        ntohs(vid), ntohl(dir->d_did), fbitmap, dbitmap,
        ntohl(curdir->d_did), cfrombstr(curdir->d_fullpath), s_path->u_name);

    st   = &s_path->st;
    if (!s_path->st_valid) {
        /* it's a dir and it should be there
         * because we chdir in it in cname or
         * it's curdir (maybe deleted, but then we can't know).
         * So we need to try harder.
         */
        of_statdir(vol, s_path);
    }
    if ( s_path->st_errno != 0 ) {
        if (afp_errno != AFPERR_ACCESS) {
            return( AFPERR_NOOBJ );
        }
    }


    buflen = 0;
    if (S_ISDIR(st->st_mode)) {
        if (dbitmap) {
            dir = s_path->d_dir;
            if (!dir)
                return AFPERR_NOOBJ;

            ret = getdirparams(obj, vol, dbitmap, s_path, dir,
                               rbuf + 3 * sizeof( uint16_t ), &buflen );
            if (ret != AFP_OK )
                return( ret );
        }
        /* this is a directory */
        *(rbuf + 2 * sizeof( uint16_t )) = (char) FILDIRBIT_ISDIR;
    } else {
        if (fbitmap && AFP_OK != (ret = getfilparams(obj, vol, fbitmap, s_path, curdir,
                                                     rbuf + 3 * sizeof( uint16_t ), &buflen, 0)) ) {
            return( ret );
        }
        /* this is a file */
        *(rbuf + 2 * sizeof( uint16_t )) = FILDIRBIT_ISFILE;
    }
    *rbuflen = buflen + 3 * sizeof( uint16_t );
    fbitmap = htons( fbitmap );
    memcpy( rbuf, &fbitmap, sizeof( fbitmap ));
    rbuf += sizeof( fbitmap );
    dbitmap = htons( dbitmap );
    memcpy( rbuf, &dbitmap, sizeof( dbitmap ));
    rbuf += sizeof( dbitmap ) + sizeof( u_char );
    *rbuf = 0;

    return( AFP_OK );
}

int afp_setfildirparams(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct stat *st;
    struct vol  *vol;
    struct dir  *dir;
    struct path *path;
    uint16_t   vid, bitmap;
    int     did, rc;

    *rbuflen = 0;
    ibuf += 2;
    memcpy( &vid, ibuf, sizeof(vid));
    ibuf += sizeof( vid );

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy( &did, ibuf, sizeof( did));
    ibuf += sizeof( did);

    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    memcpy( &bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    if (NULL == ( path = cname( vol, dir, &ibuf ))) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    st   = &path->st;
    if (!path->st_valid) {
        /* it's a dir and it should be there
         * because we chdir in it in cname
         */
        of_statdir(vol, path);
    }

    if ( path->st_errno != 0 ) {
        if (afp_errno != AFPERR_ACCESS)
            return( AFPERR_NOOBJ );
    }
    /*
     * If ibuf is odd, make it even.
     */
    if ((u_long)ibuf & 1 ) {
        ibuf++;
    }

    if (S_ISDIR(st->st_mode)) {
        rc = setdirparams(vol, path, bitmap, ibuf );
    } else {
        rc = setfilparams(obj, vol, path, bitmap, ibuf );
    }
    if ( rc == AFP_OK ) {
        setvoltime(obj, vol );
    }

    return( rc );
}

/* --------------------------------------------
   Factorise some checks on a pathname
*/
int check_name(const struct vol *vol, char *name)
{
    if (!vol->vfs->vfs_validupath(vol, name)) {
        LOG(log_error, logtype_afpd, "check_name: illegal name: '%s'", name);
        return AFPERR_EXIST;
    }

    /* check for vetoed filenames */
    if (veto_file(vol->v_veto, name))
        return AFPERR_EXIST;
    return 0;
}

/* ------------------------- 
    move and rename sdir:oldname to curdir:newname in volume vol
    special care is needed for lock   
*/
static int moveandrename(struct vol *vol,
                         struct dir *sdir,
                         int sdir_fd,
                         char *oldname,
                         char *newname,
                         int isdir)
{
    char            *oldunixname = NULL;
    char            *upath;
    int             rc;
    struct stat     *st, nst;
    int             adflags;
    struct adouble	ad;
    struct adouble	*adp;
    struct ofork	*opened = NULL;
    struct path     path;
    cnid_t          id;
    int             cwd_fd = -1;

    LOG(log_debug, logtype_afpd,
        "moveandrename: [\"%s\"/\"%s\"] -> \"%s\"",
        cfrombstr(sdir->d_u_name), oldname, newname);

    ad_init(&ad, vol);
    adp = &ad;
    adflags = 0;

    if (!isdir) {
        if ((oldunixname = strdup(mtoupath(vol, oldname, sdir->d_did, utf8_encoding(vol->v_obj)))) == NULL)
            return AFPERR_PARAM; /* can't convert */
        AFP_CNID_START("cnid_get");
        id = cnid_get(vol->v_cdb, sdir->d_did, oldunixname, strlen(oldunixname));
        AFP_CNID_DONE();

#ifndef HAVE_ATFUNCS
        /* Need full path */
        free(oldunixname);
        if ((oldunixname = strdup(ctoupath(vol, sdir, oldname))) == NULL)
            return AFPERR_PARAM; /* pathname too long */
#endif /* HAVE_ATFUNCS */

        path.st_valid = 0;
        path.u_name = oldunixname;

#ifdef HAVE_ATFUNCS
        opened = of_findnameat(sdir_fd, &path);
#else
        opened = of_findname(vol, &path);
#endif /* HAVE_ATFUNCS */

        if (opened) {
            /* reuse struct adouble so it won't break locks */
            adp = opened->of_ad;
        }
    } else {
        id = sdir->d_did; /* we already have the CNID */
        if ((oldunixname = strdup(ctoupath( vol, dirlookup(vol, sdir->d_pdid), oldname))) == NULL)
            return AFPERR_PARAM;
        adflags = ADFLAGS_DIR;
    }

    /*
     * oldunixname now points to either
     *   a) full pathname of the source fs object (if renameat is not available)
     *   b) the oldname (renameat is available)
     * we are in the dest folder so we need to use 
     *   a) oldunixname for ad_open
     *   b) fchdir sdir_fd before eg ad_open or use *at functions where appropiate
     */

    if (sdir_fd != -1) {
        if ((cwd_fd = open(".", O_RDONLY)) == -1)
            return AFPERR_MISC;
        if (fchdir(sdir_fd) != 0) {
            rc = AFPERR_MISC;
            goto exit;
        }
    }
    if (!ad_metadata(oldunixname, adflags, adp)) {
        uint16_t bshort;

        ad_getattr(adp, &bshort);
        
        ad_close(adp, ADFLAGS_HF);
        if (!(vol->v_ignattr & ATTRBIT_NORENAME) && (bshort & htons(ATTRBIT_NORENAME))) {
            rc = AFPERR_OLOCK;
            goto exit;
        }
    }
    if (sdir_fd != -1) {
        if (fchdir(cwd_fd) != 0) {
            LOG(log_error, logtype_afpd, "moveandrename: %s", strerror(errno) );
            rc = AFPERR_MISC;
            goto exit;
        }
    }

    if (NULL == (upath = mtoupath(vol, newname, curdir->d_did, utf8_encoding(vol->v_obj)))){ 
        rc = AFPERR_PARAM;
        goto exit;
    }
    path.u_name = upath;
    st = &path.st;
    if (0 != (rc = check_name(vol, upath))) {
        goto exit;
    }

    /* source == destination. we just silently accept this. */
    if ((!isdir && curdir == sdir) || (isdir && curdir->d_did == sdir->d_pdid)) {
        if (strcmp(oldname, newname) == 0) {
            rc = AFP_OK;
            goto exit;
        }

        if (stat(upath, st) == 0) {
            if (!stat(oldunixname, &nst) && !(nst.st_dev == st->st_dev && nst.st_ino == st->st_ino) ) {
                /* not the same file */
                rc = AFPERR_EXIST;
                goto exit;
            }
            errno = 0;
        }
    } else if (stat(upath, st ) == 0) {
        rc = AFPERR_EXIST;
        goto exit;
    }

    if ( !isdir ) {
        path.st_valid = 1;
        path.st_errno = errno;
        if (of_findname(vol, &path)) {
            rc = AFPERR_EXIST; /* was AFPERR_BUSY; */
        } else {
            rc = renamefile(vol, curdir, sdir_fd, oldunixname, upath, newname, adp );
            if (rc == AFP_OK)
                of_rename(vol, opened, sdir, oldname, curdir, newname);
        }
    } else {
        rc = renamedir(vol, sdir_fd, oldunixname, upath, sdir, curdir, newname);
    }
    if ( rc == AFP_OK && id ) {
        /* renaming may have moved the file/dir across a filesystem */
        if (stat(upath, st) < 0) {
            rc = AFPERR_MISC;
            goto exit;
        }

        /* Remove it from the cache */
        struct dir *cacheddir = dircache_search_by_did(vol, id);
        if (cacheddir) {
            LOG(log_warning, logtype_afpd,"Still cached: \"%s/%s\"", getcwdpath(), upath);
            (void)dir_remove(vol, cacheddir);
        }

        /* Fixup adouble info */
        if (!ad_metadata(upath, adflags, adp)) {
            ad_setid(adp, st->st_dev, st->st_ino, id, curdir->d_did, vol->v_stamp);
            ad_flush(adp);
            ad_close(adp, ADFLAGS_HF);
        }

        /* fix up the catalog entry */
        AFP_CNID_START("cnid_update");
        cnid_update(vol->v_cdb, id, st, curdir->d_did, upath, strlen(upath));
        AFP_CNID_DONE();
    }

exit:
    if (cwd_fd != -1)
        close(cwd_fd);
    if (oldunixname)
        free(oldunixname);
    return rc;
}

/* -------------------------------------------- */
int afp_rename(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol  *vol;
    struct dir  *sdir;
    char        *oldname, *newname;
    struct path *path;
    uint32_t   did;
    int         plen;
    uint16_t   vid;
    int         isdir = 0;
    int         rc;

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
    if (NULL == ( sdir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    /* source pathname */
    if (NULL == ( path = cname( vol, sdir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    sdir = curdir;
    newname = obj->newtmp;
    oldname = obj->oldtmp;
    isdir = path_isadir(path);
    if ( *path->m_name != '\0' ) {
        strcpy(oldname, path->m_name); /* an extra copy for of_rename */
        if (isdir) {
            /* curdir parent dir, need to move sdir back */
            sdir = path->d_dir;
        }
    }
    else {
        if ( sdir->d_did == DIRDID_ROOT ) { /* root directory */
            return( AFPERR_NORENAME );
        }
        /* move to destination dir */
        if ( movecwd( vol, dirlookup(vol, sdir->d_pdid) ) < 0 ) {
            return afp_errno;
        }
        memcpy(oldname, cfrombstr(sdir->d_m_name), blength(sdir->d_m_name) +1);
    }

    /* another place where we know about the path type */
    if ((plen = copy_path_name(vol, newname, ibuf)) < 0) {
        return( AFPERR_PARAM );
    }

    if (!plen) {
        return AFP_OK; /* newname == oldname same dir */
    }
    
    rc = moveandrename(vol, sdir, -1, oldname, newname, isdir);
    if ( rc == AFP_OK ) {
        setvoltime(obj, vol );
    }

    return( rc );
}

/* 
 * Recursivley delete vetoed files and directories if the volume option is set
 *
 * @param vol   (r) volume handle
 * @param upath (r) path of directory
 *
 * If the volume option delete veto files is set, this function recursively scans the
 * directory "upath" for vetoed files and tries deletes these, the it will try to delete
 * the directory. That may fail if the directory contains normal files that aren't vetoed.
 *
 * @returns 0 if the directory upath and all of its contents were deleted, otherwise -1.
 *            If the volume option is not set it returns -1.
 */
int delete_vetoed_files(struct vol *vol, const char *upath, bool in_vetodir)
{
    EC_INIT;
    DIR            *dp = NULL;
    struct dirent  *de;
    struct stat     sb;
    int             pwd = -1;
    bool            vetoed;

    if (!(vol->v_flags & AFPVOL_DELVETO))
        return -1;

    EC_NEG1( pwd = open(".", O_RDONLY));
    EC_ZERO( chdir(upath) );
    EC_NULL( dp = opendir(".") );

    while ((de = readdir(dp))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;

        if (stat(de->d_name, &sb) != 0) {
            LOG(log_error, logtype_afpd, "delete_vetoed_files(\"%s/%s\"): %s",
                upath, de->d_name, strerror(errno));
                EC_EXIT_STATUS(AFPERR_DIRNEMPT);
        }

        if (in_vetodir || veto_file(vol->v_veto, de->d_name))
            vetoed = true;
        else
            vetoed = false;

        if (vetoed) {
            LOG(log_debug, logtype_afpd, "delete_vetoed_files(\"%s/%s\"): deleting vetoed file",
                upath, de->d_name);
            switch (sb.st_mode & S_IFMT) {
            case S_IFDIR:
                /* recursion */
                EC_ZERO( delete_vetoed_files(vol, de->d_name, vetoed));
                break;
            case S_IFREG:
            case S_IFLNK:
                EC_ZERO( netatalk_unlink(de->d_name) );
                break;
            default:
                break;
            }
        }
    }

    EC_ZERO_LOG( fchdir(pwd) );
    pwd = -1;
    EC_ZERO_LOG( rmdir(upath) );

EC_CLEANUP:
    if (dp)
        closedir(dp);
    if (pwd != -1) {
        if (fchdir(pwd) != 0)
            ret = -1;
    }

    EC_EXIT;
}

/* ------------------------------- */
int afp_delete(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol  *vol;
    struct dir  *dir;
    struct path *s_path;
    char        *upath;
    int         did;
    int         rc = AFP_OK;
    uint16_t    vid;

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

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    upath = s_path->u_name;
    if (path_isadir(s_path)) {
        if (*s_path->m_name != '\0' || curdir->d_did == DIRDID_ROOT) {
            if (vol->v_adouble == AD_VERSION2)
                return AFPERR_ACCESS;
            if (*s_path->m_name == '\0' && curdir->d_did == DIRDID_ROOT)
                return AFPERR_ACCESS;
            if (rmdir(upath) != 0) {
                switch (errno) {
                case ENOTEMPTY:
                    if (delete_vetoed_files(vol, upath, false) != 0)
                        return AFPERR_DIRNEMPT;
                    break;
                case EACCES:
                    return AFPERR_ACCESS;
                default:
                    return AFPERR_MISC;
                }
            }
            struct dir *deldir;
            cnid_t delcnid = CNID_INVALID;
            if ((deldir = dircache_search_by_name(vol, curdir, upath, strlen(upath)))) {
                delcnid = deldir->d_did;
                dir_remove(vol, deldir);
            }
            if (delcnid == CNID_INVALID) {
                AFP_CNID_START("cnid_get");
                delcnid = cnid_get(vol->v_cdb, curdir->d_did, upath, strlen(upath));
                AFP_CNID_DONE();
            }
            if (delcnid != CNID_INVALID) {
                AFP_CNID_START("cnid_delete");
                cnid_delete(vol->v_cdb, delcnid);
                AFP_CNID_DONE();
            }
            fce_register(FCE_DIR_DELETE, fullpathname(upath), NULL, fce_dir);
        } else {
            /* we have to cache this, the structs are lost in deletcurdir*/
            /* but we need the positive returncode to send our event */
            bstring dname;
            if ((dname = bstrcpy(curdir->d_u_name)) == NULL)
                return AFPERR_MISC;
            if ((rc = deletecurdir(vol)) == AFP_OK)
                fce_register(FCE_DIR_DELETE, fullpathname(cfrombstr(dname)), NULL, fce_dir);
            bdestroy(dname);
        }
    } else if (of_findname(vol, s_path)) {
        rc = AFPERR_BUSY;
    } else {
        /* it's a file st_valid should always be true
         * only test for ENOENT because EACCES needs
         * to read meta data in deletefile
         */
        if (s_path->st_valid && s_path->st_errno == ENOENT) {
            rc = AFPERR_NOOBJ;
        } else {
            if ((rc = deletefile(vol, -1, upath, 1)) == AFP_OK) {
				fce_register(FCE_FILE_DELETE, fullpathname(upath), NULL, fce_file);
                if (vol->v_tm_used < s_path->st.st_size)
                    vol->v_tm_used = 0;
                else 
                    vol->v_tm_used -= s_path->st.st_size;
            }
            struct dir *cachedfile;
            if ((cachedfile = dircache_search_by_name(vol, dir, upath, strlen(upath)))) {
                dircache_remove(vol, cachedfile, DIRCACHE | DIDNAME_INDEX | QUEUE_INDEX);
                dir_free(cachedfile);
            }
        }
    }
    if ( rc == AFP_OK ) {
        curdir->d_offcnt--;
        setvoltime(obj, vol );
    }

    return( rc );
}
/* ------------------------ */
char *absupath(const struct vol *vol, struct dir *dir, char *u)
{
    static char pathbuf[MAXPATHLEN + 1];
    bstring path;

    if (u == NULL || dir == NULL || vol == NULL)
        return NULL;

    if ((path = bstrcpy(dir->d_fullpath)) == NULL)
        return NULL;
    if (bcatcstr(path, "/") != BSTR_OK)
        return NULL;
    if (bcatcstr(path, u) != BSTR_OK)
        return NULL;
    if (path->slen > MAXPATHLEN) {
        bdestroy(path);
        return NULL;
    }

    LOG(log_debug, logtype_afpd, "absupath: %s", cfrombstr(path));

    strncpy(pathbuf, cfrombstr(path), blength(path) + 1);
    bdestroy(path);

    return(pathbuf);
}

char *ctoupath(const struct vol *vol, struct dir *dir, char *name)
{
    if (vol == NULL || dir == NULL || name == NULL)
        return NULL;
    return absupath(vol, dir, mtoupath(vol, name, dir->d_did, utf8_encoding(vol->v_obj)));
}

/* ------------------------- */
int afp_moveandrename(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol  *vol;
    struct dir  *sdir, *ddir;
    int         isdir;
    char    *oldname, *newname;
    struct path *path;
    int     did;
    int     pdid;
    int         plen;
    uint16_t   vid;
    int         rc;
    int     sdir_fd = -1;


    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    /* source did followed by dest did */
    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( int );
    if (NULL == ( sdir = dirlookup( vol, did )) ) {
        return afp_errno; /* was AFPERR_PARAM */
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( int );

    /* source pathname */
    if (NULL == ( path = cname( vol, sdir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ);
    }

    sdir = curdir;
    newname = obj->newtmp;
    oldname = obj->oldtmp;

    isdir = path_isadir(path);
    if ( *path->m_name != '\0' ) {
        if (isdir) {
            sdir = path->d_dir;
        }
        strcpy(oldname, path->m_name); /* an extra copy for of_rename */
    } else {
        memcpy(oldname, cfrombstr(sdir->d_m_name), blength(sdir->d_m_name) + 1);
    }

#ifdef HAVE_ATFUNCS
    if ((sdir_fd = open(".", O_RDONLY)) == -1)
        return AFPERR_MISC;
#endif

    /* get the destination directory */
    if (NULL == ( ddir = dirlookup( vol, did )) ) {
        rc = afp_errno; /*  was AFPERR_PARAM */
        goto exit;
    }
    if (NULL == ( path = cname( vol, ddir, &ibuf ))) {
        rc = AFPERR_NOOBJ;
        goto exit;
    }
    pdid = curdir->d_did;
    if ( *path->m_name != '\0' ) {
        rc = path_error(path, AFPERR_NOOBJ);
        goto exit;
    }

    /* one more place where we know about path type */
    if ((plen = copy_path_name(vol, newname, ibuf)) < 0) {
        rc = AFPERR_PARAM;
        goto exit;
    }

    if (!plen) {
        strcpy(newname, oldname);
    }

    /* This does the work */
    LOG(log_debug, logtype_afpd, "afp_move(oldname:'%s', newname:'%s', isdir:%u)",
        oldname, newname, isdir);
    rc = moveandrename(vol, sdir, sdir_fd, oldname, newname, isdir);

    if ( rc == AFP_OK ) {
        char *upath = mtoupath(vol, newname, pdid, utf8_encoding(obj));

        if (NULL == upath) {
            rc = AFPERR_PARAM;
            goto exit;
        }
        /* if unix priv don't try to match perm with dest folder */
        if (!isdir && !vol_unix_priv(vol)) {
            int  admode = ad_mode("", 0777) | vol->v_fperm;

            setfilmode(vol, upath, admode, path->st_valid ? &path->st : NULL);
            vol->vfs->vfs_setfilmode(vol, upath, admode, path->st_valid ? &path->st : NULL);
        }
        setvoltime(obj, vol );
    }

exit:
#ifdef HAVE_ATFUNCS
    if (sdir_fd != -1)
        close(sdir_fd);
#endif

    return( rc );
}

int veto_file(const char*veto_str, const char*path)
/* given a veto_str like "abc/zxc/" and path "abc", return 1
 * veto_str should be '/' delimited
 * if path matches any one of the veto_str elements exactly, then 1 is returned
 * otherwise, 0 is returned.
 */
{
    int i;  /* index to veto_str */
    int j;  /* index to path */

    if ((veto_str == NULL) || (path == NULL))
        return 0;

    for(i=0, j=0; veto_str[i] != '\0'; i++) {
        if (veto_str[i] == '/') {
            if ((j>0) && (path[j] == '\0')) {
                LOG(log_debug, logtype_afpd, "vetoed file:'%s'", path);
                return 1;
            }
            j = 0;
        } else {
            if (veto_str[i] != path[j]) {
                while ((veto_str[i] != '/')
                       && (veto_str[i] != '\0'))
                    i++;
                j = 0;
                continue;
            }
            j++;
        }
    }
    return 0;
}

