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
#include <utime.h>
#include <errno.h>
#include <sys/param.h>

#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/unix.h>
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

/* the format for the finderinfo fields (from IM: Toolbox Essentials):
 * field         bytes        subfield    bytes
 * 
 * files:
 * ioFlFndrInfo  16      ->       type    4  type field
 *                             creator    4  creator field
 *                               flags    2  finder flags:
 *					     alias, bundle, etc.
 *                            location    4  location in window
 *                              folder    2  window that contains file
 * 
 * ioFlXFndrInfo 16      ->     iconID    2  icon id
 *                              unused    6  reserved 
 *                              script    1  script system
 *                              xflags    1  reserved
 *                           commentID    2  comment id
 *                           putawayID    4  home directory id
 */

const u_char ufinderi[ADEDLEN_FINDERI] = {
                              0, 0, 0, 0, 0, 0, 0, 0,
                              1, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0
                          };

static const u_char old_ufinderi[] = {
                              'T', 'E', 'X', 'T', 'U', 'N', 'I', 'X'
                          };

/* ---------------------- 
*/
static int default_type(void *finder) 
{
    if (!memcmp(finder, ufinderi, 8) || !memcmp(finder, old_ufinderi, 8))
        return 1;
    return 0;
}

/* FIXME path : unix or mac name ? (for now it's unix name ) */
void *get_finderinfo(const struct vol *vol, const char *upath, struct adouble *adp, void *data, int islink)
{
    struct extmap       *em;
    void                *ad_finder = NULL;
    int                 chk_ext = 0;

    if (adp)
        ad_finder = ad_entry(adp, ADEID_FINDERI);

    if (ad_finder) {
        memcpy(data, ad_finder, ADEDLEN_FINDERI);
        /* default type ? */
        if (default_type(ad_finder)) 
            chk_ext = 1;
    }
    else {
        memcpy(data, ufinderi, ADEDLEN_FINDERI);
        chk_ext = 1;
        if (vol_inv_dots(vol) && *upath == '.') { /* make it invisible */
            uint16_t ashort;
            
            ashort = htons(FINDERINFO_INVISIBLE);
            memcpy((char *)data + FINDERINFO_FRFLAGOFF, &ashort, sizeof(ashort));
        }
    }

    if (islink && !vol_syml_opt(vol)) {
        uint16_t linkflag;
        memcpy(&linkflag, (char *)data + FINDERINFO_FRFLAGOFF, 2);
        linkflag |= htons(FINDERINFO_ISALIAS);
        memcpy((char *)data + FINDERINFO_FRFLAGOFF, &linkflag, 2);
        memcpy((char *)data + FINDERINFO_FRTYPEOFF,"slnk",4); 
        memcpy((char *)data + FINDERINFO_FRCREATOFF,"rhap",4); 
    }

    /** Only enter if no appledouble information and no finder information found. */
    if (chk_ext && (em = getextmap( upath ))) {
        memcpy(data, em->em_type, sizeof( em->em_type ));
        memcpy((char *)data + 4, em->em_creator, sizeof(em->em_creator));
    }

    return data;
}

/* ---------------------
*/
char *set_name(const struct vol *vol, char *data, cnid_t pid, char *name, cnid_t id, uint32_t utf8) 
{
    uint32_t   aint;
    char        *tp = NULL;
    char        *src = name;
    aint = strlen( name );

    if (!utf8) {
        /* want mac name */
        if (utf8_encoding(vol->v_obj)) {
            /* but name is an utf8 mac name */
            char *u, *m;
           
            /* global static variable... */
            tp = strdup(name);
            if (!(u = mtoupath(vol, name, pid, 1)) || !(m = utompath(vol, u, id, 0))) {
               aint = 0;
            }
            else {
                aint = strlen(m);
                src = m;
            }
            
        }
        if (aint > MACFILELEN)
            aint = MACFILELEN;
        *data++ = aint;
    }
    else {
        uint16_t temp;

        if (aint > UTF8FILELEN_EARLY)  /* FIXME safeguard, anyway if no ascii char it's game over*/
           aint = UTF8FILELEN_EARLY;

        utf8 = vol->v_kTextEncoding;
        memcpy(data, &utf8, sizeof(utf8));
        data += sizeof(utf8);
        
        temp = htons(aint);
        memcpy(data, &temp, sizeof(temp));
        data += sizeof(temp);
    }

    memcpy( data, src, aint );
    data += aint;
    if (tp) {
        strcpy(name, tp);
        free(tp);
    }
    return data;
}

/*
 * FIXME: PDINFO is UTF8 and doesn't need adp
*/
#define PARAM_NEED_ADP(b) ((b) & ((1 << FILPBIT_ATTR)  |\
				  (1 << FILPBIT_CDATE) |\
				  (1 << FILPBIT_MDATE) |\
				  (1 << FILPBIT_BDATE) |\
				  (1 << FILPBIT_FINFO) |\
				  (1 << FILPBIT_RFLEN) |\
				  (1 << FILPBIT_EXTRFLEN) |\
				  (1 << FILPBIT_PDINFO) |\
				  (1 << FILPBIT_FNUM) |\
				  (1 << FILPBIT_UNIXPR)))

/*!
 * @brief Get CNID for did/upath args both from database and adouble file
 *
 * 1. Get the objects CNID as stored in its adouble file
 * 2. Get the objects CNID from the database
 * 3. If there's a problem with a "dbd" database, fallback to "tdb" in memory
 * 4. In case 2 and 3 differ, store 3 in the adouble file
 *
 * @param vol    (rw) volume
 * @param adp    (rw) adouble struct of object upath, might be NULL
 * @param st     (r) stat of upath, must NOT be NULL
 * @param did    (r) parent CNID of upath
 * @param upath  (r) name of object
 * @param len    (r) strlen of upath
 */
uint32_t get_id(struct vol *vol,
                struct adouble *adp, 
                const struct stat *st,
                const cnid_t did,
                const char *upath,
                const int len) 
{
    static int first = 1;       /* mark if this func is called the first time */
    uint32_t adcnid;
    uint32_t dbcnid = CNID_INVALID;

restart:
    if (vol->v_cdb != NULL) {
        /* prime aint with what we think is the cnid, set did to zero for
           catching moved files */
        adcnid = ad_getid(adp, st->st_dev, st->st_ino, 0, vol->v_stamp); /* (1) */

        AFP_CNID_START("cnid_add");
	    dbcnid = cnid_add(vol->v_cdb, st, did, upath, len, adcnid); /* (2) */
        AFP_CNID_DONE();

	    /* Throw errors if cnid_add fails. */
	    if (dbcnid == CNID_INVALID) {
            switch (errno) {
            case CNID_ERR_CLOSE: /* the db is closed */
                break;
            case CNID_ERR_PARAM:
                LOG(log_error, logtype_afpd, "get_id: Incorrect parameters passed to cnid_add");
                afp_errno = AFPERR_PARAM;
                goto exit;
            case CNID_ERR_PATH:
                afp_errno = AFPERR_PARAM;
                goto exit;
            default:
                /* Close CNID backend if "dbd" and switch to temp in-memory "tdb" */
                /* we have to do it here for "dbd" because it uses "lazy opening" */
                /* In order to not end in a loop somehow with goto restart below  */
                /*  */
                if (first && (strcmp(vol->v_cnidscheme, "dbd") == 0)) { /* (3) */
                    cnid_close(vol->v_cdb);
                    free(vol->v_cnidscheme);
                    vol->v_cnidscheme = strdup("tdb");

                    int flags = CNID_FLAG_MEMORY;
                    if ((vol->v_flags & AFPVOL_NODEV)) {
                        flags |= CNID_FLAG_NODEV;
                    }
                    LOG(log_error, logtype_afpd, "Reopen volume %s using in memory temporary CNID DB.",
                        vol->v_path);
                    vol->v_cdb = cnid_open(vol->v_path, vol->v_umask, "tdb", flags, NULL, NULL);
                    if (vol->v_cdb) {
                        if (!(vol->v_flags & AFPVOL_TM)) {
                            vol->v_flags |= AFPVOL_RO;
                            setmessage("Something wrong with the volume's CNID DB, using temporary CNID DB instead."
                                       "Check server messages for details. Switching to read-only mode.");
                            kill(getpid(), SIGUSR2);
                        }
                        goto restart; /* now try again with the temp CNID db */
                    } else {
                        setmessage("Something wrong with the volume's CNID DB, using temporary CNID DB failed too!"
                                   "Check server messages for details, can't recover from this state!");
                    }
                }
                afp_errno = AFPERR_MISC;
                goto exit;
            }
        }
        else if (adp && adcnid && (adcnid != dbcnid)) { /* 4 */
            /* Update the ressource fork. For a folder adp is always null */
            LOG(log_debug, logtype_afpd, "get_id(%s/%s): calling ad_setid(old: %u, new: %u)",
                getcwdpath(), upath, htonl(adcnid), htonl(dbcnid));
            if (ad_setid(adp, st->st_dev, st->st_ino, dbcnid, did, vol->v_stamp)) {
                if (ad_flush(adp) != 0)
                    LOG(log_error, logtype_afpd, "get_id(\"%s\"): can't flush", fullpathname(upath));
            }
        }
    }

exit:
    first = 0;
    return dbcnid;
}
             
/* -------------------------- */
int getmetadata(const AFPObj *obj,
                struct vol *vol,
                uint16_t bitmap,
                struct path *path, struct dir *dir, 
                char *buf, size_t *buflen, struct adouble *adp)
{
    char		*data, *l_nameoff = NULL, *upath;
    char                *utf_nameoff = NULL;
    int			bit = 0;
    uint32_t		aint;
    cnid_t              id = 0;
    uint16_t		ashort;
    u_char              achar, fdType[4];
    uint32_t           utf8 = 0;
    struct stat         *st;
    struct maccess	ma;

    LOG(log_debug, logtype_afpd, "getmetadata(\"%s\")", path->u_name);

    upath = path->u_name;
    st = &path->st;
    data = buf;

    if ( ((bitmap & ( (1 << FILPBIT_FINFO)|(1 << FILPBIT_LNAME)|(1 <<FILPBIT_PDINFO) ) ) && !path->m_name)
         || (bitmap & ( (1 << FILPBIT_LNAME) ) && utf8_encoding(obj)) /* FIXME should be m_name utf8 filename */
         || (bitmap & (1 << FILPBIT_FNUM))) {
        if (!path->id) {
            bstring fullpath;
            struct dir *cachedfile;
            int len = strlen(upath);
            if ((cachedfile = dircache_search_by_name(vol, dir, upath, len)) != NULL)
                id = cachedfile->d_did;
            else {
                id = get_id(vol, adp, st, dir->d_did, upath, len);

                /* Add it to the cache */
                LOG(log_debug, logtype_afpd, "getmetadata: caching: did:%u, \"%s\", cnid:%u",
                    ntohl(dir->d_did), upath, ntohl(id));

                /* Get macname from unixname first */
                if (path->m_name == NULL) {
                    if ((path->m_name = utompath(vol, upath, id, utf8_encoding(obj))) == NULL) {
                        LOG(log_error, logtype_afpd, "getmetadata: utompath error");
                        return AFPERR_MISC;
                    }
                }
                
                /* Build fullpath */
                if (((fullpath = bstrcpy(dir->d_fullpath)) == NULL)
                    || (bconchar(fullpath, '/') != BSTR_OK)
                    || (bcatcstr(fullpath, upath)) != BSTR_OK) {
                    LOG(log_error, logtype_afpd, "getmetadata: fullpath: %s", strerror(errno));
                    bdestroy(fullpath);
                    return AFPERR_MISC;
                }

                if ((cachedfile = dir_new(path->m_name, upath, vol, dir->d_did, id, fullpath, st)) == NULL) {
                    LOG(log_error, logtype_afpd, "getmetadata: error from dir_new");
                    return AFPERR_MISC;
                }

                if ((dircache_add(vol, cachedfile)) != 0) {
                    LOG(log_error, logtype_afpd, "getmetadata: fatal dircache error");
                    return AFPERR_MISC;
                }
            }
        } else {
            id = path->id;
        }

        if (id == CNID_INVALID)
            return afp_errno;

        if (!path->m_name) {
            path->m_name = utompath(vol, upath, id, utf8_encoding(vol->v_obj));
        }
    }
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch ( bit ) {
        case FILPBIT_ATTR :
            if ( adp ) {
                ad_getattr(adp, &ashort);
            } else if (vol_inv_dots(vol) && *upath == '.') {
                ashort = htons(ATTRBIT_INVISIBLE);
            } else
                ashort = 0;
            ashort &= ~htons(vol->v_ignattr);
#if 0
            /* FIXME do we want a visual clue if the file is read only
             */
            struct maccess	ma;
            accessmode(vol, ".", &ma, dir , NULL);
            if ((ma.ma_user & AR_UWRITE)) {
            	accessmode(vol, upath, &ma, dir , st);
            	if (!(ma.ma_user & AR_UWRITE)) {
                	ashort |= htons(ATTRBIT_NOWRITE);
                }
            }
#endif
            memcpy(data, &ashort, sizeof( ashort ));
            data += sizeof( ashort );
            LOG(log_debug, logtype_afpd, "metadata('%s'): AFP Attributes: %04x",
                path->u_name, ntohs(ashort));
            break;

        case FILPBIT_PDID :
            memcpy(data, &dir->d_did, sizeof( uint32_t ));
            data += sizeof( uint32_t );
            LOG(log_debug, logtype_afpd, "metadata('%s'):     Parent DID: %u",
                path->u_name, ntohl(dir->d_did));
            break;

        case FILPBIT_CDATE :
            if (!adp || (ad_getdate(adp, AD_DATE_CREATE, &aint) < 0))
                aint = AD_DATE_FROM_UNIX(st->st_mtime);
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case FILPBIT_MDATE :
            if ( adp && (ad_getdate(adp, AD_DATE_MODIFY, &aint) == 0)) {
                if ((st->st_mtime > AD_DATE_TO_UNIX(aint))) {
                   aint = AD_DATE_FROM_UNIX(st->st_mtime);
                }
            } else {
                aint = AD_DATE_FROM_UNIX(st->st_mtime);
            }
            memcpy(data, &aint, sizeof( int ));
            data += sizeof( int );
            break;

        case FILPBIT_BDATE :
            if (!adp || (ad_getdate(adp, AD_DATE_BACKUP, &aint) < 0))
                aint = AD_DATE_START;
            memcpy(data, &aint, sizeof( int ));
            data += sizeof( int );
            break;

        case FILPBIT_FINFO :
	        get_finderinfo(vol, upath, adp, (char *)data,S_ISLNK(st->st_mode));
            data += ADEDLEN_FINDERI;
            break;

        case FILPBIT_LNAME :
            l_nameoff = data;
            data += sizeof( uint16_t );
            break;

        case FILPBIT_SNAME :
            memset(data, 0, sizeof(uint16_t));
            data += sizeof( uint16_t );
            break;

        case FILPBIT_FNUM :
            memcpy(data, &id, sizeof( id ));
            data += sizeof( id );
            LOG(log_debug, logtype_afpd, "metadata('%s'):           CNID: %u",
                path->u_name, ntohl(id));
            break;

        case FILPBIT_DFLEN :
            if  (st->st_size > 0xffffffff)
               aint = 0xffffffff;
            else
               aint = htonl( st->st_size );
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case FILPBIT_RFLEN: {
            off_t rlen;
            if (adp) {
                if (adp->ad_rlen > 0xffffffff)
                    aint = 0xffffffff;
                else
                    aint = htonl( adp->ad_rlen);
            } else {
                rlen = ad_reso_size(path->u_name, 0, NULL);
                if (rlen > 0xffffffff)
                    rlen = 0xffffffff;
                aint = htonl(rlen);
            }
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;
        }

            /* Current client needs ProDOS info block for this file.
               Use simple heuristic and let the Mac "type" string tell
               us what the PD file code should be.  Everything gets a
               subtype of 0x0000 unless the original value was hashed
               to "pXYZ" when we created it.  See IA, Ver 2.
               <shirsch@adelphia.net> */
        case FILPBIT_PDINFO :
            if (obj->afp_version >= 30) { /* UTF8 name */
                utf8 = kTextEncodingUTF8;
                utf_nameoff = data;
                data += sizeof( uint16_t );
                aint = 0;
                memcpy(data, &aint, sizeof( aint ));
                data += sizeof( aint );
            }
            else {
                if ( adp ) {
                    memcpy(fdType, ad_entry( adp, ADEID_FINDERI ), 4 );

                    if ( memcmp( fdType, "TEXT", 4 ) == 0 ) {
                        achar = '\x04';
                        ashort = 0x0000;
                    }
                    else if ( memcmp( fdType, "PSYS", 4 ) == 0 ) {
                        achar = '\xff';
                        ashort = 0x0000;
                    }
                    else if ( memcmp( fdType, "PS16", 4 ) == 0 ) {
                        achar = '\xb3';
                        ashort = 0x0000;
                    }
                    else if ( memcmp( fdType, "BINA", 4 ) == 0 ) {
                        achar = '\x00';
                        ashort = 0x0000;
                    }
                    else if ( fdType[0] == 'p' ) {
                        achar = fdType[1];
                        ashort = (fdType[2] * 256) + fdType[3];
                    }
                    else {
                        achar = '\x00';
                        ashort = 0x0000;
                    }
                }
                else {
                    achar = '\x00';
                    ashort = 0x0000;
                }

                *data++ = achar;
                *data++ = 0;
                memcpy(data, &ashort, sizeof( ashort ));
                data += sizeof( ashort );
                memset(data, 0, sizeof( ashort ));
                data += sizeof( ashort );
            }
            break;
        case FILPBIT_EXTDFLEN:
            aint = htonl(st->st_size >> 32);
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            aint = htonl(st->st_size);
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;
        case FILPBIT_EXTRFLEN:
            if (adp) {
                aint = htonl(adp->ad_rlen >> 32);
                memcpy(data, &aint, sizeof( aint ));
                data += sizeof( aint );
                aint = htonl(adp->ad_rlen);
                memcpy(data, &aint, sizeof( aint ));
                data += sizeof( aint );
            } else {
                int64_t rlen = hton64(ad_reso_size(path->u_name, 0, NULL));
                memcpy(data, &rlen, sizeof(rlen));
                data += sizeof(rlen);
            }
            break;
        case FILPBIT_UNIXPR :
            /* accessmode may change st_mode with ACLs */
            accessmode(obj, vol, upath, &ma, dir , st);

            aint = htonl(st->st_uid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );
            aint = htonl(st->st_gid);
            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );

	    /* FIXME: ugly hack
               type == slnk indicates an OSX style symlink, 
               we have to add S_IFLNK to the mode, otherwise
               10.3 clients freak out. */

    	    aint = st->st_mode;
 	    if (adp) {
	        memcpy(fdType, ad_entry( adp, ADEID_FINDERI ), 4 );
                if ( memcmp( fdType, "slnk", 4 ) == 0 ) {
	 	    aint |= S_IFLNK;
            	}
	    }
            aint = htonl(aint);

            memcpy( data, &aint, sizeof( aint ));
            data += sizeof( aint );

            *data++ = ma.ma_user;
            *data++ = ma.ma_world;
            *data++ = ma.ma_group;
            *data++ = ma.ma_owner;
            break;
            
        default :
            return( AFPERR_BITMAP );
        }
        bitmap = bitmap>>1;
        bit++;
    }
    if ( l_nameoff ) {
        ashort = htons( data - buf );
        memcpy(l_nameoff, &ashort, sizeof( ashort ));
        data = set_name(vol, data, dir->d_did, path->m_name, id, 0);
    }
    if ( utf_nameoff ) {
        ashort = htons( data - buf );
        memcpy(utf_nameoff, &ashort, sizeof( ashort ));
        data = set_name(vol, data, dir->d_did, path->m_name, id, utf8);
    }
    *buflen = data - buf;
    return (AFP_OK);
}
                
/* ----------------------- */
int getfilparams(const AFPObj *obj, struct vol *vol, uint16_t bitmap, struct path *path,
                 struct dir *dir, char *buf, size_t *buflen, int in_enumerate)
{
    struct adouble	ad, *adp;
    int                 opened = 0;
    int rc;    
    int flags; /* uninitialized ok */

    LOG(log_debug, logtype_afpd, "getfilparams(\"%s\")", path->u_name);

    opened = PARAM_NEED_ADP(bitmap);
    adp = NULL;

    if (opened) {
        char *upath;
        /*
         * Dont check for and resturn open fork attributes when enumerating
         * This saves a lot of syscalls, the client will hopefully only use the result
         * in FPGetFileParms where we return the correct value
         */
        flags = (!in_enumerate &&(bitmap & (1 << FILPBIT_ATTR))) ? ADFLAGS_CHECK_OF : 0;

        adp = of_ad(vol, path, &ad);
        upath = path->u_name;

        if ( ad_metadata( upath, flags, adp) < 0 ) {
            switch (errno) {
            case EACCES:
                LOG(log_error, logtype_afpd, "getfilparams(%s): %s: check resource fork permission?",
                upath, strerror(errno));
                return AFPERR_ACCESS;
            case EIO:
                LOG(log_error, logtype_afpd, "getfilparams(%s): bad resource fork", upath);
                /* fall through */
            case ENOENT:
            default:
                adp = NULL;
                break;
            }
        }
    }
    rc = getmetadata(obj, vol, bitmap, path, dir, buf, buflen, adp);

    if (opened)
        ad_close(adp, ADFLAGS_HF | flags);

    return( rc );
}

/* ----------------------------- */
int afp_createfile(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct adouble	ad;
    struct vol		*vol;
    struct dir		*dir;
    struct ofork        *of = NULL;
    char		*path, *upath;
    int			creatf, did, openf, retvalue = AFP_OK;
    uint16_t		vid;
    struct path		*s_path;
    
    *rbuflen = 0;
    ibuf++;
    creatf = (unsigned char) *ibuf++;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );

    if (NULL == ( vol = getvolbyvid( vid )) )
        return( AFPERR_PARAM );

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy(&did, ibuf, sizeof( did));
    ibuf += sizeof( did );

    if (NULL == ( dir = dirlookup( vol, did )) )
        return afp_errno;

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) )
        return get_afp_errno(AFPERR_PARAM);
    if ( *s_path->m_name == '\0' )
        return( AFPERR_BADTYPE );

    upath = s_path->u_name;
    ad_init(&ad, vol);
    
    /* if upath is deleted we already in trouble anyway */
    if ((of = of_findname(vol, s_path))) {
        if (creatf)
            return AFPERR_BUSY;
        else
            return AFPERR_EXIST;
    }

    if (creatf)
        openf = ADFLAGS_RDWR | ADFLAGS_CREATE | ADFLAGS_TRUNC;
    else
    	/* on a soft create, if the file is open then ad_open won't fail
    	   because open syscall is not called */
        openf = ADFLAGS_RDWR | ADFLAGS_CREATE | ADFLAGS_EXCL;

    if (ad_open(&ad, upath, ADFLAGS_DF | ADFLAGS_HF | ADFLAGS_NOHF | openf, 0666) < 0) {
        switch ( errno ) {
        case EROFS:
            return AFPERR_VLOCK;
        case ENOENT : /* we were already in 'did folder' so chdir() didn't fail */
            return ( AFPERR_NOOBJ );
        case EEXIST :
            return( AFPERR_EXIST );
        case EACCES :
            return( AFPERR_ACCESS );
        case EDQUOT:
        case ENOSPC :
	    LOG(log_info, logtype_afpd, "afp_createfile: DISK FULL");
            return( AFPERR_DFULL );
        default :
            return( AFPERR_PARAM );
        }
    }
    if ( ad_meta_fileno( &ad ) == -1 ) { /* Hard META / HF */
         /* FIXME with hard create on an existing file, we already
          * corrupted the data file.
          */
         netatalk_unlink( upath );
         ad_close( &ad, ADFLAGS_DF );
         return AFPERR_ACCESS;
    }

    path = s_path->m_name;
    ad_setname(&ad, path);

    struct stat st;
    if (lstat(upath, &st) != 0) {
        LOG(log_error, logtype_afpd, "afp_createfile(\"%s\"): stat: %s",
            upath, strerror(errno));
        ad_close(&ad, ADFLAGS_DF|ADFLAGS_HF);
        return AFPERR_MISC;
    }

    cnid_t id;
    if ((id = get_id(vol, &ad, &st, dir->d_did, upath, strlen(upath))) == CNID_INVALID) {
        LOG(log_error, logtype_afpd, "afp_createfile(\"%s\"): CNID error", upath);
        goto createfile_iderr;
    }
    (void)ad_setid(&ad, st.st_dev, st.st_ino, id, dir->d_did, vol->v_stamp);

createfile_iderr:
    ad_flush(&ad);
    ad_close(&ad, ADFLAGS_DF|ADFLAGS_HF );
    fce_register(FCE_FILE_CREATE, fullpathname(upath), NULL, fce_file);

    curdir->d_offcnt++;

    setvoltime(obj, vol );

    return (retvalue);
}

int afp_setfilparams(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol	*vol;
    struct dir	*dir;
    struct path *s_path;
    int		did, rc;
    uint16_t	vid, bitmap;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno; /* was AFPERR_NOOBJ */
    }

    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }

    if (path_isadir(s_path)) {
        return( AFPERR_BADTYPE ); /* it's a directory */
    }

    if ( s_path->st_errno != 0 ) {
        return( AFPERR_NOOBJ );
    }

    if ((u_long)ibuf & 1 ) {
        ibuf++;
    }

    if (AFP_OK == ( rc = setfilparams(obj, vol, s_path, bitmap, ibuf )) ) {
        setvoltime(obj, vol );
    }

    return( rc );
}

/*
 * cf AFP3.0.pdf page 252 for change_mdate and change_parent_mdate logic  
 * 
*/
extern struct path Cur_Path;

int setfilparams(const AFPObj *obj, struct vol *vol,
                 struct path *path, uint16_t f_bitmap, char *buf )
{
    struct adouble	ad, *adp;
    struct extmap	*em;
    int			bit, isad = 1, err = AFP_OK;
    char                *upath;
    u_char              achar, *fdType, xyy[4]; /* uninitialized, OK 310105 */
    uint16_t		ashort, bshort, oshort;
    uint32_t		aint;
    uint32_t		upriv;
    uint16_t           upriv_bit = 0;
        struct utimbuf	ut;
    int                 change_mdate = 0;
    int                 change_parent_mdate = 0;
    int                 newdate = 0;
    struct timeval      tv;
    uid_t		f_uid;
    gid_t		f_gid;
    uint16_t           bitmap = f_bitmap;
    uint32_t           cdate,bdate;
    u_char              finder_buf[32];
    int symlinked = S_ISLNK(path->st.st_mode);
    int fp;
    ssize_t len;
    char symbuf[MAXPATHLEN+1];

#ifdef DEBUG
    LOG(log_debug9, logtype_afpd, "begin setfilparams:");
#endif /* DEBUG */

    adp = of_ad(vol, path, &ad);
    upath = path->u_name;

    if (!vol_unix_priv(vol) && check_access(obj, vol, upath, OPENACC_WR ) < 0) {
        return AFPERR_ACCESS;
    }

    /* with unix priv maybe we have to change adouble file priv first */
    bit = 0;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }
        switch(  bit ) {
        case FILPBIT_ATTR :
            change_mdate = 1;
            memcpy(&ashort, buf, sizeof( ashort ));
            buf += sizeof( ashort );
            break;
        case FILPBIT_CDATE :
            change_mdate = 1;
            memcpy(&cdate, buf, sizeof(cdate));
            buf += sizeof( cdate );
            break;
        case FILPBIT_MDATE :
            memcpy(&newdate, buf, sizeof( newdate ));
            buf += sizeof( newdate );
            break;
        case FILPBIT_BDATE :
            change_mdate = 1;
            memcpy(&bdate, buf, sizeof( bdate));
            buf += sizeof( bdate );
            break;
        case FILPBIT_FINFO :
            change_mdate = 1;
            if (memcmp(buf,"slnkrhap",8) == 0
                && !(S_ISLNK(path->st.st_mode))
                && !(vol->v_flags & AFPVOL_FOLLOWSYM)) {
                /* request to turn this into a symlink */
                if ((fp = open(path->u_name, O_RDONLY)) == -1) {
                    err = AFPERR_MISC;
                    goto setfilparam_done;
                }
                len = read(fp, symbuf, MAXPATHLEN);
                close(fp);
                if (!(len > 0)) {
                    err = AFPERR_MISC;
                    goto setfilparam_done;
                }
                if (unlink(path->u_name) != 0) {
                    err = AFPERR_MISC;
                    goto setfilparam_done;
                }
                symbuf[len] = 0;
                if (symlink(symbuf, path->u_name) != 0) {
                    err = AFPERR_MISC;
                    goto setfilparam_done;
                }
                of_stat(vol, path);
                symlinked = 1;
            }
            memcpy(finder_buf, buf, 32 );
            buf += 32;
            break;
        case FILPBIT_UNIXPR :
            if (!vol_unix_priv(vol)) {
            	/* this volume doesn't use unix priv */
            	err = AFPERR_BITMAP;
            	bitmap = 0;
            	break;
            }
            change_mdate = 1;
            change_parent_mdate = 1;

            memcpy( &aint, buf, sizeof( aint ));
            f_uid = ntohl (aint);
            buf += sizeof( aint );
            memcpy( &aint, buf, sizeof( aint ));
            f_gid = ntohl (aint);
            buf += sizeof( aint );
            setfilowner(vol, f_uid, f_gid, path);

            memcpy( &upriv, buf, sizeof( upriv ));
            buf += sizeof( upriv );
            upriv = ntohl (upriv);
            if ((upriv & S_IWUSR)) {
            	setfilunixmode(vol, path, upriv);
            }
            else {
            	/* do it later */
            	upriv_bit = 1;
            }
            break;
        case FILPBIT_PDINFO :
            if (obj->afp_version < 30) { /* else it's UTF8 name */
                achar = *buf;
                buf += 2;
                /* Keep special case to support crlf translations */
                if ((unsigned int) achar == 0x04) {
	       	    fdType = (u_char *)"TEXT";
		    buf += 2;
                } else {
            	    xyy[0] = ( u_char ) 'p';
            	    xyy[1] = achar;
            	    xyy[3] = *buf++;
            	    xyy[2] = *buf++;
            	    fdType = xyy;
	        }
                break;
            }
            /* fallthrough */
        default :
            err = AFPERR_BITMAP;
            /* break while loop */
            bitmap = 0;
            break;
        }

        bitmap = bitmap>>1;
        bit++;
    }

    /* second try with adouble open 
    */
    if (ad_open(adp, upath, ADFLAGS_HF | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666) < 0) {
        LOG(log_debug, logtype_afpd, "setfilparams: ad_open_metadata error");
        /*
         * For some things, we don't need an adouble header:
         * - change of modification date
         * - UNIX privs (Bug-ID #2863424)
         */
        if (!symlinked && f_bitmap & ~(1<<FILPBIT_MDATE | 1<<FILPBIT_UNIXPR)) {
            LOG(log_debug, logtype_afpd, "setfilparams: need adouble access");
            return AFPERR_ACCESS;
        }
        LOG(log_debug, logtype_afpd, "setfilparams: no adouble perms, but only FILPBIT_MDATE and/or FILPBIT_UNIXPR");
        isad = 0;
    } else if ((ad_get_MD_flags( adp ) & O_CREAT) ) {
        ad_setname(adp, path->m_name);
        cnid_t id;
        if ((id = get_id(vol, adp, &path->st, curdir->d_did, upath, strlen(upath))) == CNID_INVALID) {
            LOG(log_error, logtype_afpd, "afp_createfile(\"%s\"): CNID error", upath);
            return AFPERR_MISC;
        }
        (void)ad_setid(adp, path->st.st_dev, path->st.st_ino, id, curdir->d_did, vol->v_stamp);
    }
    
    bit = 0;
    bitmap = f_bitmap;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch(  bit ) {
        case FILPBIT_ATTR :
            ad_getattr(adp, &bshort);
            oshort = bshort;
            if ( ntohs( ashort ) & ATTRBIT_SETCLR ) {
                ashort &= ~htons(vol->v_ignattr);
                bshort |= htons( ntohs( ashort ) & ~ATTRBIT_SETCLR );
            } else {
                bshort &= ~ashort;
            }
            if ((bshort & htons(ATTRBIT_INVISIBLE)) != (oshort & htons(ATTRBIT_INVISIBLE)))
                change_parent_mdate = 1;
            ad_setattr(adp, bshort);
            break;
        case FILPBIT_CDATE :
            ad_setdate(adp, AD_DATE_CREATE, cdate);
            break;
        case FILPBIT_MDATE :
            break;
        case FILPBIT_BDATE :
            ad_setdate(adp, AD_DATE_BACKUP, bdate);
            break;
        case FILPBIT_FINFO :
            if (default_type( ad_entry( adp, ADEID_FINDERI ))
                    && ( 
                     ((em = getextmap( path->m_name )) &&
                      !memcmp(finder_buf, em->em_type, sizeof( em->em_type )) &&
                      !memcmp(finder_buf + 4, em->em_creator,sizeof( em->em_creator)))
                     || ((em = getdefextmap()) &&
                      !memcmp(finder_buf, em->em_type, sizeof( em->em_type )) &&
                      !memcmp(finder_buf + 4, em->em_creator,sizeof( em->em_creator)))
            )) {
                memcpy(finder_buf, ufinderi, 8 );
            }
            memcpy(ad_entry( adp, ADEID_FINDERI ), finder_buf, 32 );
            break;
        case FILPBIT_UNIXPR :
            if (upriv_bit) {
            	setfilunixmode(vol, path, upriv);
            }
            break;
        case FILPBIT_PDINFO :
            if (obj->afp_version < 30) { /* else it's UTF8 name */
                memcpy(ad_entry( adp, ADEID_FINDERI ), fdType, 4 );
                memcpy(ad_entry( adp, ADEID_FINDERI ) + 4, "pdos", 4 );
                break;
            }
            /* fallthrough */
        default :
            err = AFPERR_BITMAP;
            goto setfilparam_done;
        }
        bitmap = bitmap>>1;
        bit++;
    }

setfilparam_done:
    if (change_mdate && newdate == 0 && gettimeofday(&tv, NULL) == 0) {
       newdate = AD_DATE_FROM_UNIX(tv.tv_sec);
    }
    if (newdate) {
       if (isad)
          ad_setdate(adp, AD_DATE_MODIFY, newdate);
       ut.actime = ut.modtime = AD_DATE_TO_UNIX(newdate);
       utime(upath, &ut);
    }

    if (isad) {
        ad_flush(adp);
        ad_close(adp, ADFLAGS_HF);
    }

    if (change_parent_mdate && gettimeofday(&tv, NULL) == 0) {
        newdate = AD_DATE_FROM_UNIX(tv.tv_sec);
        bitmap = 1<<FILPBIT_MDATE;
        setdirparams(vol, &Cur_Path, bitmap, (char *)&newdate);
    }

#ifdef DEBUG
    LOG(log_debug9, logtype_afpd, "end setfilparams:");
#endif /* DEBUG */
    return err;
}

/*
 * renamefile and copyfile take the old and new unix pathnames
 * and the new mac name.
 *
 * sdir_fd     source dir fd to which src path is relative (for openat et al semantics)
 *             passing -1 means this is not used, src path is a full path
 * src         the source path 
 * dst         the dest filename in current dir
 * newname     the dest mac name
 * adp         adouble struct of src file, if open, or & zeroed one
 *
 */
int renamefile(struct vol *vol, struct dir *ddir, int sdir_fd, char *src, char *dst, char *newname, struct adouble *adp)
{
    int		rc;

    LOG(log_debug, logtype_afpd,
        "renamefile: src[%d, \"%s\"] -> dst[\"%s\"]", sdir_fd, src, dst);

    if ( unix_rename( sdir_fd, src, -1, dst ) < 0 ) {
        switch ( errno ) {
        case ENOENT :
            return( AFPERR_NOOBJ );
        case EPERM:
        case EACCES :
            return( AFPERR_ACCESS );
        case EROFS:
            return AFPERR_VLOCK;
        case EXDEV :			/* Cross device move -- try copy */
           /* NOTE: with open file it's an error because after the copy we will 
            * get two files, it's fixable for our process (eg reopen the new file, get the
            * locks, and so on. But it doesn't solve the case with a second process
            */
    	    if (adp->ad_open_forks) {
    	        /* FIXME  warning in syslog so admin'd know there's a conflict ?*/
    	        return AFPERR_OLOCK; /* little lie */
    	    }
            if (AFP_OK != ( rc = copyfile(vol, vol, ddir, sdir_fd, src, dst, newname, NULL )) ) {
                /* on error copyfile delete dest */
                return( rc );
            }
            return deletefile(vol, sdir_fd, src, 0);
        default :
            return( AFPERR_PARAM );
        }
    }

    if (vol->vfs->vfs_renamefile(vol, sdir_fd, src, dst) < 0 ) {
        int err;
        
        err = errno;        
	/* try to undo the data fork rename,
	 * we know we are on the same device 
	*/
	if (err) {
        unix_rename(-1, dst, sdir_fd, src ); 
    	    /* return the first error */
    	    switch ( err) {
            case ENOENT :
                return AFPERR_NOOBJ;
            case EPERM:
            case EACCES :
                return AFPERR_ACCESS ;
            case EROFS:
                return AFPERR_VLOCK;
            default :
                return AFPERR_PARAM ;
            }
        }
    }

    /* don't care if we can't open the newly renamed ressource fork */
    if (ad_open(adp, dst, ADFLAGS_HF | ADFLAGS_RDWR) == 0) {
        ad_setname(adp, newname);
        ad_flush( adp );
        ad_close( adp, ADFLAGS_HF );
    }

    return( AFP_OK );
}

/* ---------------- 
   convert a Mac long name to an utf8 name,
*/
size_t mtoUTF8(const struct vol *vol, const char *src, size_t srclen, char *dest, size_t destlen)
{
size_t    outlen;

    if ((size_t)-1 == (outlen = convert_string ( vol->v_maccharset, CH_UTF8_MAC, src, srclen, dest, destlen)) ) {
	return -1;
    }
    return outlen;
}

/* ---------------- */
int copy_path_name(const struct vol *vol, char *newname, char *ibuf)
{
char        type = *ibuf;
size_t      plen = 0;
uint16_t   len16;
uint32_t   hint;

    if ( type != 2 && !(vol->v_obj->afp_version >= 30 && type == 3) ) {
        return -1;
    }
    ibuf++;
    switch (type) {
    case 2:
        if (( plen = (unsigned char)*ibuf++ ) != 0 ) {
            if (vol->v_obj->afp_version >= 30) {
                /* convert it to UTF8 
                */
                if ((plen = mtoUTF8(vol, ibuf, plen, newname, AFPOBJ_TMPSIZ)) == (size_t)-1)
                   return -1;
            }
            else {
                strncpy( newname, ibuf, plen );
                newname[ plen ] = '\0';
            }
            if (strlen(newname) != plen) {
                /* there's \0 in newname, e.g. it's a pathname not
                 * only a filename. 
                */
            	return -1;
            }
        }
        break;
    case 3:
        memcpy(&hint, ibuf, sizeof(hint));
        ibuf += sizeof(hint);
           
        memcpy(&len16, ibuf, sizeof(len16));
        ibuf += sizeof(len16);
        plen = ntohs(len16);
        
        if (plen) {
            if (plen > AFPOBJ_TMPSIZ) {
            	return -1;
            }
            strncpy( newname, ibuf, plen );
            newname[ plen ] = '\0';
            if (strlen(newname) != plen) {
            	return -1;
            }
        }
        break;
    }
    return plen;
}

/* -----------------------------------
*/
int afp_copyfile(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol	*s_vol, *d_vol;
    struct dir	*dir;
    char	*newname, *p, *upath;
    struct path *s_path;
    uint32_t	sdid, ddid;
    int         err, retvalue = AFP_OK;
    uint16_t	svid, dvid;

    struct adouble ad, *adp;
    int denyreadset;
    
    *rbuflen = 0;
    ibuf += 2;

    memcpy(&svid, ibuf, sizeof( svid ));
    ibuf += sizeof( svid );
    if (NULL == ( s_vol = getvolbyvid( svid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy(&sdid, ibuf, sizeof( sdid ));
    ibuf += sizeof( sdid );
    if (NULL == ( dir = dirlookup( s_vol, sdid )) ) {
        return afp_errno;
    }

    memcpy(&dvid, ibuf, sizeof( dvid ));
    ibuf += sizeof( dvid );
    memcpy(&ddid, ibuf, sizeof( ddid ));
    ibuf += sizeof( ddid );

    if (NULL == ( s_path = cname( s_vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }
    if ( path_isadir(s_path) ) {
        return( AFPERR_BADTYPE );
    }

    /* don't allow copies when the file is open.
     * XXX: the spec only calls for read/deny write access.
     *      however, copyfile doesn't have any of that info,
     *      and locks need to stay coherent. as a result,
     *      we just balk if the file is opened already. */

    adp = of_ad(s_vol, s_path, &ad);

    if (ad_open(adp, s_path->u_name, ADFLAGS_DF | ADFLAGS_HF | ADFLAGS_NOHF | ADFLAGS_RDONLY | ADFLAGS_SETSHRMD) < 0) {
        return AFPERR_DENYCONF;
    }
#ifdef HAVE_FSHARE_T
    fshare_t shmd;
    shmd.f_access = F_RDACC;
    shmd.f_deny = F_NODNY;
    if (fcntl(ad_data_fileno(adp), F_SHARE, &shmd) != 0) {
        retvalue = AFPERR_DENYCONF;
        goto copy_exit;
    }
    if (AD_RSRC_OPEN(adp) && fcntl(ad_reso_fileno(adp), F_SHARE, &shmd) != 0) {
        retvalue = AFPERR_DENYCONF;
        goto copy_exit;
    }
#endif
    denyreadset = (ad_testlock(adp, ADEID_DFORK, AD_FILELOCK_DENY_RD) != 0 || 
                  ad_testlock(adp, ADEID_RFORK, AD_FILELOCK_DENY_RD) != 0 );

    if (denyreadset) {
        retvalue = AFPERR_DENYCONF;
        goto copy_exit;
    }

    newname = obj->newtmp;
    strcpy( newname, s_path->m_name );

    p = ctoupath( s_vol, curdir, newname );
    if (!p) {
        retvalue = AFPERR_PARAM;
        goto copy_exit;
    }

    if (NULL == ( d_vol = getvolbyvid( dvid )) ) {
        retvalue = AFPERR_PARAM;
        goto copy_exit;
    }

    if (d_vol->v_flags & AFPVOL_RO) {
        retvalue = AFPERR_VLOCK;
        goto copy_exit;
    }

    if (NULL == ( dir = dirlookup( d_vol, ddid )) ) {
        retvalue = afp_errno;
        goto copy_exit;
    }

    if (( s_path = cname( d_vol, dir, &ibuf )) == NULL ) {
        retvalue = get_afp_errno(AFPERR_NOOBJ);
        goto copy_exit;
    }
    
    if ( *s_path->m_name != '\0' ) {
	retvalue =path_error(s_path, AFPERR_NOOBJ);
        goto copy_exit;
    }

    /* one of the handful of places that knows about the path type */
    if (copy_path_name(d_vol, newname, ibuf) < 0) {
        retvalue = AFPERR_PARAM;
        goto copy_exit;
    }
    /* newname is always only a filename so curdir *is* its
     * parent folder
    */
    if (NULL == (upath = mtoupath(d_vol, newname, curdir->d_did, utf8_encoding(d_vol->v_obj)))) {
        retvalue =AFPERR_PARAM;
        goto copy_exit;
    }

    if ( (err = copyfile(s_vol, d_vol, curdir, -1, p, upath , newname, adp)) < 0 ) {
        retvalue = err;
        goto copy_exit;
    }
    curdir->d_offcnt++;

    setvoltime(obj, d_vol );

copy_exit:
    ad_close( adp, ADFLAGS_DF |ADFLAGS_HF | ADFLAGS_SETSHRMD);
    return( retvalue );
}

/* ----------------------------------
 * if newname is NULL (from directory.c) we don't want to copy the resource fork.
 * because we are doing it elsewhere.
 * currently if newname is NULL then adp is NULL. 
 */
int copyfile(struct vol *s_vol,
             struct vol *d_vol, 
             struct dir *d_dir, 
             int sfd,
             char *src,
             char *dst,
             char *newname,
             struct adouble *adp)
{
    struct adouble	ads, add;
    int			err = 0;
    int                 ret_err = 0;
    int                 adflags;
    int                 stat_result;
    struct stat         st;
    
    LOG(log_debug, logtype_afpd, "copyfile(sfd:%d,s:'%s',d:'%s',n:'%s')",
        sfd, src, dst, newname);

    if (adp == NULL) {
        ad_init(&ads, s_vol);
        adp = &ads;
    }

    adflags = ADFLAGS_DF | ADFLAGS_HF | ADFLAGS_NOHF | ADFLAGS_RF | ADFLAGS_NORF;

    if (ad_openat(adp, sfd, src, adflags | ADFLAGS_RDONLY) < 0) {
        ret_err = errno;
        goto done;
    }

    if (!AD_META_OPEN(adp))
        /* no resource fork, don't create one for dst file */
        adflags &= ~ADFLAGS_HF;

    if (!AD_RSRC_OPEN(adp))
        /* no resource fork, don't create one for dst file */
        adflags &= ~ADFLAGS_RF;

    stat_result = fstat(ad_data_fileno(adp), &st); /* saving stat exit code, thus saving us on one more stat later on */

    if (stat_result < 0) {           
      /* unlikely but if fstat fails, the default file mode will be 0666. */
      st.st_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    }

    ad_init(&add, d_vol);
    if (ad_open(&add, dst, adflags | ADFLAGS_RDWR | ADFLAGS_CREATE | ADFLAGS_EXCL, st.st_mode | S_IRUSR | S_IWUSR) < 0) {
        ret_err = errno;
        ad_close( adp, adflags );
        if (EEXIST != ret_err) {
            deletefile(d_vol, -1, dst, 0);
            goto done;
        }
        return AFPERR_EXIST;
    }

    if ((err = copy_fork(ADEID_DFORK, &add, adp)) != 0)
        LOG(log_error, logtype_afpd, "copyfile('%s'): %s", src, strerror(errno));

    if (err == 0)
        if ((err = d_vol->vfs->vfs_copyfile(d_vol, sfd, src, dst)) != 0)
            LOG(log_error, logtype_afpd, "copyfile('%s'): %s", src, strerror(errno));

    if (err < 0)
       ret_err = errno;

    if (AD_META_OPEN(&add)) {
        if (AD_META_OPEN(adp))
            ad_copy_header(&add, adp);
        ad_setname(&add, dst);
        cnid_t id;
        struct stat stdest;
        if (fstat(ad_meta_fileno(&add), &stdest) != 0) {
            ret_err = errno;
            goto error;
        }
        if ((id = get_id(d_vol, &add, &stdest, d_dir->d_did, dst, strlen(dst))) == CNID_INVALID) {
            ret_err = EINVAL;
            goto error;
        }
        (void)ad_setid(&add, stdest.st_dev, stdest.st_ino, id, d_dir->d_did, d_vol->v_stamp);
        ad_flush(&add);
    }

error:
    ad_close( adp, adflags );

    if (ad_close( &add, adflags ) <0) {
       ret_err = errno;
    } 

    if (ret_err) {
        deletefile(d_vol, -1, dst, 0);
    }
    else if (stat_result == 0) {
        /* set dest modification date to src date */
        struct utimbuf	ut;

    	ut.actime = ut.modtime = st.st_mtime;
    	utime(dst, &ut);
    	/* FIXME netatalk doesn't use resource fork file date
    	 * but maybe we should set its modtime too.
    	*/
    }

done:
    switch ( ret_err ) {
    case 0:
        return AFP_OK;
    case EDQUOT:
    case EFBIG:
    case ENOSPC:
	LOG(log_info, logtype_afpd, "copyfile: DISK FULL");
        return AFPERR_DFULL;
    case ENOENT:
        return AFPERR_NOOBJ;
    case EACCES:
        return AFPERR_ACCESS;
    case EROFS:
        return AFPERR_VLOCK;
    }
    return AFPERR_PARAM;
}


/* -----------------------------------
   vol: not NULL delete cnid entry. then we are in curdir and file is a only filename
   checkAttrib:   1 check kFPDeleteInhibitBit (deletfile called by afp_delete)

   when deletefile is called we don't have lock on it, file is closed (for us)
   untrue if called by renamefile
   
   ad_open always try to open file RDWR first and ad_lock takes care of
   WRITE lock on read only file.
*/

static int check_attrib(const struct vol *vol, struct adouble *adp)
{
uint16_t   bshort = 0;

	ad_getattr(adp, &bshort);
    /*
     * Does kFPDeleteInhibitBit (bit 8) set?
     */
	if (!(vol->v_ignattr & ATTRBIT_NODELETE) && (bshort & htons(ATTRBIT_NODELETE))) {
		return AFPERR_OLOCK;
	}
     if ((bshort & htons(ATTRBIT_DOPEN | ATTRBIT_ROPEN))) {
    	return AFPERR_BUSY;
	}
	return 0;
}
/* 
 * dirfd can be used for unlinkat semantics
 */
int deletefile(const struct vol *vol, int dirfd, char *file, int checkAttrib)
{
    struct adouble	ad;
    struct adouble      *adp = NULL;
    int			adflags, err = AFP_OK;
    int			meta = 0;

    LOG(log_debug, logtype_afpd, "deletefile('%s')", file);

    ad_init(&ad, vol);
    if (checkAttrib) {
        /* was EACCESS error try to get only metadata */
        /* we never want to create a resource fork here, we are going to delete it 
         * moreover sometimes deletefile is called with a no existent file and 
         * ad_open would create a 0 byte resource fork
        */
        if ( ad_metadataat(dirfd, file, ADFLAGS_CHECK_OF, &ad) == 0 ) {
            if ((err = check_attrib(vol, &ad))) {
                ad_close(&ad, ADFLAGS_HF | ADFLAGS_CHECK_OF);
               return err;
            }
            meta = 1;
        }
    }
 
    /* try to open both forks at once */
    adflags = ADFLAGS_DF;
    if (ad_openat(&ad, dirfd, file, adflags | ADFLAGS_RF | ADFLAGS_NORF | ADFLAGS_RDONLY) < 0 ) {
        switch (errno) {
        case ENOENT:
            err = AFPERR_NOOBJ;
            goto end;
        case EACCES: /* maybe it's a file with no write mode for us */
            break;   /* was return AFPERR_ACCESS;*/
        case EROFS:
            err = AFPERR_VLOCK;
            goto end;
        default:
            err = AFPERR_PARAM;
            goto end;
        }
    }
    else {
        adp = &ad;
    }

    if ( adp && AD_RSRC_OPEN(adp) ) { /* there's a resource fork */
        adflags |= ADFLAGS_RF;
        /* FIXME we have a pb here because we want to know if a file is open 
         * there's a 'priority inversion' if you can't open the ressource fork RW
         * you can delete it if it's open because you can't get a write lock.
         * 
         * ADLOCK_FILELOCK means the whole ressource fork, not only after the 
         * metadatas
         *
         * FIXME it doesn't work for RFORK open read only and fork open without deny mode
         */
        if (ad_tmplock(&ad, ADEID_RFORK, ADLOCK_WR |ADLOCK_FILELOCK, 0, 0, 0) < 0 ) {
            err = AFPERR_BUSY;
            goto end;
        }
    }

    if (adp && ad_tmplock( &ad, ADEID_DFORK, ADLOCK_WR, 0, 0, 0 ) < 0) {
        LOG(log_error, logtype_afpd, "deletefile('%s'): ad_tmplock error: %s", file, strerror(errno));
        err = AFPERR_BUSY;
    } else if (!(err = vol->vfs->vfs_deletefile(vol, dirfd, file)) && !(err = netatalk_unlinkat(dirfd, file )) ) {
        cnid_t id;
        if (checkAttrib) {
            AFP_CNID_START("cnid_get");
            id = cnid_get(vol->v_cdb, curdir->d_did, file, strlen(file));
            AFP_CNID_DONE();
            if (id) {
                AFP_CNID_START("cnid_delete");
                cnid_delete(vol->v_cdb, id);
                AFP_CNID_DONE();
            }
        }
    }

end:
    if (meta)
        ad_close(&ad, ADFLAGS_HF | ADFLAGS_CHECK_OF);

    if (adp)
        ad_close( &ad, adflags );  /* ad_close removes locks if any */

    return err;
}

/* ------------------------------------ */
/* return a file id */
int afp_createid(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct stat         *st;
    struct vol		*vol;
    struct dir		*dir;
    char		*upath;
    int                 len;
    cnid_t		did, id;
    u_short		vid;
    struct path         *s_path;

    *rbuflen = 0;

    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof(vid);

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM);
    }

    if (vol->v_cdb == NULL || !(vol->v_cdb->flags & CNID_FLAG_PERSISTENT)) {
        return AFPERR_NOOP;
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy(&did, ibuf, sizeof( did ));
    ibuf += sizeof(did);

    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno; /* was AFPERR_PARAM */
    }

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ); /* was AFPERR_PARAM */
    }

    if ( path_isadir(s_path) ) {
        return( AFPERR_BADTYPE );
    }

    upath = s_path->u_name;
    switch (s_path->st_errno) {
        case 0:
             break; /* success */
        case EPERM:
        case EACCES:
            return AFPERR_ACCESS;
        case ENOENT:
            return AFPERR_NOOBJ;
        default:
            return AFPERR_PARAM;
    }
    st = &s_path->st;
    AFP_CNID_START("cnid_lookup");
    id = cnid_lookup(vol->v_cdb, st, did, upath, len = strlen(upath));
    AFP_CNID_DONE();
    if (id) {
        memcpy(rbuf, &id, sizeof(id));
        *rbuflen = sizeof(id);
        return AFPERR_EXISTID;
    }

    if ((id = get_id(vol, NULL, st, did, upath, len)) != CNID_INVALID) {
        memcpy(rbuf, &id, sizeof(id));
        *rbuflen = sizeof(id);
        return AFP_OK;
    }

    return afp_errno;
}

/* ------------------------------- */
struct reenum {
    struct vol *vol;
    cnid_t     did;
};

static int reenumerate_loop(struct dirent *de, char *mname _U_, void *data)
{
    struct path   path;
    struct reenum *param = data;
    struct vol    *vol = param->vol;  
    cnid_t        did  = param->did;
    cnid_t	  aint;
    
    if (ostat(de->d_name, &path.st, vol_syml_opt(vol)) < 0)
        return 0;
    
    /* update or add to cnid */
    AFP_CNID_START("cnid_add");
    aint = cnid_add(vol->v_cdb, &path.st, did, de->d_name, strlen(de->d_name), 0); /* ignore errors */
    AFP_CNID_DONE();

    return 0;
}

/* --------------------
 * Ok the db is out of synch with the dir.
 * but if it's a deleted file we don't want to do it again and again.
*/
static int
reenumerate_id(struct vol *vol, char *name, struct dir *dir)
{
    int             ret;
    struct reenum   data;
    struct stat     st;
    
    if (vol->v_cdb == NULL) {
	return -1;
    }
    
    /* FIXME use of_statdir ? */
    if (ostat(name, &st, vol_syml_opt(vol))) {
	return -1;
    }

    if (dirreenumerate(dir, &st)) {
        /* we already did it once and the dir haven't been modified */
    	return dir->d_offcnt;
    }
    
    data.vol = vol;
    data.did = dir->d_did;
    if ((ret = for_each_dirent(vol, name, reenumerate_loop, (void *)&data)) >= 0) {
        setdiroffcnt(curdir, &st,  ret);
        dir->d_flags |= DIRF_CNID;
    }

    return ret;
}

/* ------------------------------
   resolve a file id */
int afp_resolveid(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    char		*upath;
    struct path         path;
    int                 err, retry=0;
    size_t		buflen;
    cnid_t		id, cnid;
    uint16_t		vid, bitmap;

    static char buffer[12 + MAXPATHLEN + 1];
    int len = 12 + MAXPATHLEN + 1;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof(vid);

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM);
    }

    if (vol->v_cdb == NULL || !(vol->v_cdb->flags & CNID_FLAG_PERSISTENT)) {
        return AFPERR_NOOP;
    }

    memcpy(&id, ibuf, sizeof( id ));
    ibuf += sizeof(id);
    cnid = id;
    
    if (!id) {
        /* some MacOS versions after a catsearch do a *lot* of afp_resolveid with 0 */
        return AFPERR_NOID;
    }
retry:
    AFP_CNID_START("cnid_resolve");
    upath = cnid_resolve(vol->v_cdb, &id, buffer, len);
    AFP_CNID_DONE();
    if (upath == NULL) {
        return AFPERR_NOID; /* was AFPERR_BADID, but help older Macs */
    }

    if (NULL == ( dir = dirlookup( vol, id )) ) {
        return AFPERR_NOID; /* idem AFPERR_PARAM */
    }
    if (movecwd(vol, dir) < 0) {
        switch (errno) {
        case EACCES:
        case EPERM:
            return AFPERR_ACCESS;
        case ENOENT:
            return AFPERR_NOID;
        default:
            return AFPERR_PARAM;
        }
    }

    memset(&path, 0, sizeof(path));
    path.u_name = upath;
    if (of_stat(vol, &path) < 0 ) {
#ifdef ESTALE
        /* with nfs and our working directory is deleted */
	if (errno == ESTALE) {
	    errno = ENOENT;
	}
#endif	
	if ( errno == ENOENT && !retry) {
	    /* cnid db is out of sync, reenumerate the directory and update ids */
	    reenumerate_id(vol, ".", dir);
	    id = cnid;
	    retry = 1;
	    goto retry;
        }
        switch (errno) {
        case EACCES:
        case EPERM:
            return AFPERR_ACCESS;
        case ENOENT:
            return AFPERR_NOID;
        default:
            return AFPERR_PARAM;
        }
    }

    /* directories are bad */
    if (S_ISDIR(path.st.st_mode)) {
        /* OS9 and OSX don't return the same error code  */
        return (obj->afp_version >=30)?AFPERR_NOID:AFPERR_BADTYPE;
    }

    memcpy(&bitmap, ibuf, sizeof(bitmap));
    bitmap = ntohs( bitmap );
    if (NULL == (path.m_name = utompath(vol, upath, cnid, utf8_encoding(obj)))) {
        return AFPERR_NOID;
    }
    path.id = cnid;
    if (AFP_OK != (err = getfilparams(obj, vol, bitmap, &path , curdir, 
                                      rbuf + sizeof(bitmap), &buflen, 0))) {
        return err;
    }
    *rbuflen = buflen + sizeof(bitmap);
    memcpy(rbuf, ibuf, sizeof(bitmap));

    return AFP_OK;
}

/* ------------------------------ */
int afp_deleteid(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct stat         st;
    struct vol		*vol;
    struct dir		*dir;
    char                *upath;
    int                 err;
    cnid_t		id;
    cnid_t		fileid;
    u_short		vid;
    static char buffer[12 + MAXPATHLEN + 1];
    int len = 12 + MAXPATHLEN + 1;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof(vid);

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM);
    }

    if (vol->v_cdb == NULL || !(vol->v_cdb->flags & CNID_FLAG_PERSISTENT)) {
        return AFPERR_NOOP;
    }

    if (vol->v_flags & AFPVOL_RO)
        return AFPERR_VLOCK;

    memcpy(&id, ibuf, sizeof( id ));
    ibuf += sizeof(id);
    fileid = id;

    AFP_CNID_START("cnid_resolve");
    upath = cnid_resolve(vol->v_cdb, &id, buffer, len);
    AFP_CNID_DONE();
        if (upath == NULL) {
        return AFPERR_NOID;
    }

    if (NULL == ( dir = dirlookup( vol, id )) ) {
        if (afp_errno == AFPERR_NOOBJ) {
            err = AFPERR_NOOBJ;
            goto delete;
        }
        return( AFPERR_PARAM );
    }

    err = AFP_OK;
    if ((movecwd(vol, dir) < 0) || (ostat(upath, &st, vol_syml_opt(vol)) < 0)) {
        switch (errno) {
        case EACCES:
        case EPERM:
            return AFPERR_ACCESS;
#ifdef ESTALE
	case ESTALE:
#endif	
        case ENOENT:
            /* still try to delete the id */
            err = AFPERR_NOOBJ;
            break;
        default:
            return AFPERR_PARAM;
        }
    }
    else if (S_ISDIR(st.st_mode)) /* directories are bad */
        return AFPERR_BADTYPE;

delete:
    AFP_CNID_START("cnid_delete");
    if (cnid_delete(vol->v_cdb, fileid)) {
        AFP_CNID_DONE();
        switch (errno) {
        case EROFS:
            return AFPERR_VLOCK;
        case EPERM:
        case EACCES:
            return AFPERR_ACCESS;
        default:
            return AFPERR_PARAM;
        }
    }
    AFP_CNID_DONE();
    return err;
}

/* ------------------------------ */
static struct adouble *find_adouble(const AFPObj *obj, struct vol *vol, struct path *path, struct ofork **of, struct adouble *adp)
{
    int             ret;

    if (path->st_errno) {
        switch (path->st_errno) {
        case ENOENT:
            afp_errno = AFPERR_NOID;
            break;
        case EPERM:
        case EACCES:
            afp_errno = AFPERR_ACCESS;
            break;
        default:
            afp_errno = AFPERR_PARAM;
            break;
        }
        return NULL;
    }
    /* we use file_access both for legacy Mac perm and
     * for unix privilege, rename will take care of folder perms
    */
    if (file_access(obj, vol, path, OPENACC_WR ) < 0) {
        afp_errno = AFPERR_ACCESS;
        return NULL;
    }
    
    if ((*of = of_findname(vol, path))) {
        /* reuse struct adouble so it won't break locks */
        adp = (*of)->of_ad;
    }
    else {
        ret = ad_open(adp, path->u_name, ADFLAGS_HF | ADFLAGS_RDWR);
        /* META and HF */
        if ( !ret && ad_reso_fileno(adp) != -1 && !(adp->ad_resource_fork.adf_flags & ( O_RDWR | O_WRONLY))) {
            /* from AFP spec.
             * The user must have the Read & Write privilege for both files in order to use this command.
             */
            ad_close(adp, ADFLAGS_HF);
            afp_errno = AFPERR_ACCESS;
            return NULL;
        }
    }
    return adp;
}

#define APPLETEMP ".AppleTempXXXXXX"

int afp_exchangefiles(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct stat         srcst, destst;
    struct vol		*vol;
    struct dir		*dir, *sdir;
    char		*spath, temp[17], *p;
    char                *supath, *upath;
    struct path         *path;
    int                 err;
    struct adouble	ads;
    struct adouble	add;
    struct adouble	*adsp = NULL;
    struct adouble	*addp = NULL;
    struct ofork	*s_of = NULL;
    struct ofork	*d_of = NULL;
    int                 crossdev;
    
    int                 slen, dlen;
    uint32_t		sid, did;
    uint16_t		vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy(&vid, ibuf, sizeof(vid));
    ibuf += sizeof(vid);

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM);
    }

    if ((vol->v_flags & AFPVOL_RO))
        return AFPERR_VLOCK;

    /* source and destination dids */
    memcpy(&sid, ibuf, sizeof(sid));
    ibuf += sizeof(sid);
    memcpy(&did, ibuf, sizeof(did));
    ibuf += sizeof(did);

    /* source file */
    if (NULL == (dir = dirlookup( vol, sid )) ) {
        return afp_errno; /* was AFPERR_PARAM */
    }

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_NOOBJ); 
    }

    if ( path_isadir(path) ) {
        return AFPERR_BADTYPE;   /* it's a dir */
    }

    /* save some stuff */
    srcst = path->st;
    sdir = curdir;
    spath = obj->oldtmp;
    supath = obj->newtmp;
    strcpy(spath, path->m_name);
    strcpy(supath, path->u_name); /* this is for the cnid changing */
    p = absupath( vol, sdir, supath);
    if (!p) {
        /* pathname too long */
        return AFPERR_PARAM ;
    }
    
    ad_init(&ads, vol);
    if (!(adsp = find_adouble(obj, vol, path, &s_of, &ads))) {
        return afp_errno;
    }

    /* ***** from here we may have resource fork open **** */
    
    /* look for the source cnid. if it doesn't exist, don't worry about
     * it. */
    AFP_CNID_START("cnid_lookup");
    sid = cnid_lookup(vol->v_cdb, &srcst, sdir->d_did, supath,slen = strlen(supath));
    AFP_CNID_DONE();

    if (NULL == ( dir = dirlookup( vol, did )) ) {
        err = afp_errno; /* was AFPERR_PARAM */
        goto err_exchangefile;
    }

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
        err = get_afp_errno(AFPERR_NOOBJ); 
        goto err_exchangefile;
    }

    if ( path_isadir(path) ) {
        err = AFPERR_BADTYPE;
        goto err_exchangefile;
    }

    /* FPExchangeFiles is the only call that can return the SameObj
     * error */
    if ((curdir == sdir) && strcmp(spath, path->m_name) == 0) {
        err = AFPERR_SAMEOBJ;
        goto err_exchangefile;
    }

    ad_init(&add, vol);
    if (!(addp = find_adouble(obj, vol, path, &d_of, &add))) {
        err = afp_errno;
        goto err_exchangefile;
    }
    destst = path->st;

    /* they are not on the same device and at least one is open
     * FIXME broken for for crossdev and adouble v2
     * return an error 
    */
    crossdev = (srcst.st_dev != destst.st_dev);
    if (/* (d_of || s_of)  && */ crossdev) {
        err = AFPERR_MISC;
        goto err_exchangefile;
    }

    /* look for destination id. */
    upath = path->u_name;
    AFP_CNID_START("cnid_lookup");
    did = cnid_lookup(vol->v_cdb, &destst, curdir->d_did, upath, dlen = strlen(upath));
    AFP_CNID_DONE();

    /* construct a temp name.
     * NOTE: the temp file will be in the dest file's directory. it
     * will also be inaccessible from AFP. */
    memcpy(temp, APPLETEMP, sizeof(APPLETEMP));
    int fd;
    if ((fd = mkstemp(temp)) == -1) {
        err = AFPERR_MISC;
        goto err_exchangefile;
    }
    close(fd);

    if (crossdev) {
        /* FIXME we need to close fork for copy, both s_of and d_of are null */
       ad_close(adsp, ADFLAGS_HF);
       ad_close(addp, ADFLAGS_HF);
    }

    /* now, quickly rename the file. we error if we can't. */
    if ((err = renamefile(vol, curdir, -1, p, temp, temp, adsp)) != AFP_OK)
        goto err_exchangefile;
    of_rename(vol, s_of, sdir, spath, curdir, temp);

    /* rename destination to source */
    if ((err = renamefile(vol, curdir, -1, upath, p, spath, addp)) != AFP_OK)
        goto err_src_to_tmp;
    of_rename(vol, d_of, curdir, path->m_name, sdir, spath);

    /* rename temp to destination */
    if ((err = renamefile(vol, curdir, -1, temp, upath, path->m_name, adsp)) != AFP_OK)
        goto err_dest_to_src;
    of_rename(vol, s_of, curdir, temp, curdir, path->m_name);

    /* 
     * id's need switching. src -> dest and dest -> src. 
     * we need to re-stat() if it was a cross device copy.
     */
    if (sid) {
        AFP_CNID_START("cnid_delete");        
        cnid_delete(vol->v_cdb, sid);
        AFP_CNID_DONE();
    }
    if (did) {
        AFP_CNID_START("cnid_delete");
        cnid_delete(vol->v_cdb, did);
        AFP_CNID_DONE();
    }

    if ((did && ( (crossdev && ostat(upath, &srcst, vol_syml_opt(vol)) < 0) || 
                cnid_update(vol->v_cdb, did, &srcst, curdir->d_did,upath, dlen) < 0))
       ||
        (sid && ( (crossdev && ostat(p, &destst, vol_syml_opt(vol)) < 0) ||
                cnid_update(vol->v_cdb, sid, &destst, sdir->d_did,supath, slen) < 0))
    ) {
        switch (errno) {
        case EPERM:
        case EACCES:
            err = AFPERR_ACCESS;
            break;
        default:
            err = AFPERR_PARAM;
        }
        goto err_temp_to_dest;
    }

    if (AD_META_OPEN(adsp) || AD_META_OPEN(addp)) {
        struct adouble adtmp;
        bool opened_ads, opened_add;

        ad_init(&adtmp, vol);
        ad_init_offsets(&adtmp);

        if (!AD_META_OPEN(adsp)) {
            if (ad_open(adsp, p, ADFLAGS_HF) != 0)
                return -1;
            opened_ads = true;
        }

        if (!AD_META_OPEN(addp)) {
            if (ad_open(addp, upath, ADFLAGS_HF) != 0)
                return -1;
            opened_add = true;
        }

        if (ad_copy_header(&adtmp, adsp) != 0)
            goto err_temp_to_dest;
        if (ad_copy_header(adsp, addp) != 0)
            goto err_temp_to_dest;
        if (ad_copy_header(addp, &adtmp) != 0)
            goto err_temp_to_dest;
        ad_flush(adsp);
        ad_flush(addp);

        if (opened_ads)
            ad_close(adsp, ADFLAGS_HF);
        if (opened_add)
            ad_close(addp, ADFLAGS_HF);
    }

    /* FIXME: we should switch ressource fork too */
    
    /* here we need to reopen if crossdev */
    if (sid && ad_setid(addp, destst.st_dev, destst.st_ino,  sid, sdir->d_did, vol->v_stamp)) 
    {
       ad_flush( addp );
    }
        
    if (did && ad_setid(adsp, srcst.st_dev, srcst.st_ino,  did, curdir->d_did, vol->v_stamp)) 
    {
       ad_flush( adsp );
    }

    /* change perms, src gets dest perm and vice versa */

    become_root();

    /*
     * we need to exchange ACL entries as well
     */
    /* exchange_acls(vol, p, upath); */

    path->st = srcst;
    path->st_valid = 1;
    path->st_errno = 0;
    path->m_name = NULL;
    path->u_name = upath;

    setfilunixmode(vol, path, destst.st_mode);
    setfilowner(vol, destst.st_uid, destst.st_gid, path);

    path->st = destst;
    path->st_valid = 1;
    path->st_errno = 0;
    path->u_name = p;

    setfilunixmode(vol, path, srcst.st_mode);
    setfilowner(vol, srcst.st_uid, srcst.st_gid, path);

    unbecome_root();

    err = AFP_OK;
    goto err_exchangefile;

    /* all this stuff is so that we can unwind a failed operation
     * properly. */
err_temp_to_dest:
    /* rename dest to temp */
    renamefile(vol, curdir, -1, upath, temp, temp, adsp);
    of_rename(vol, s_of, curdir, upath, curdir, temp);

err_dest_to_src:
    /* rename source back to dest */
    renamefile(vol, curdir, -1, p, upath, path->m_name, addp);
    of_rename(vol, d_of, sdir, spath, curdir, path->m_name);

err_src_to_tmp:
    /* rename temp back to source */
    renamefile(vol, curdir, -1, temp, p, spath, adsp);
    of_rename(vol, s_of, curdir, temp, sdir, spath);

err_exchangefile:
    if ( !s_of && adsp && ad_meta_fileno(adsp) != -1 ) { /* META */
       ad_close(adsp, ADFLAGS_HF);
    }
    if ( !d_of && addp && ad_meta_fileno(addp) != -1 ) {/* META */
       ad_close(addp, ADFLAGS_HF);
    }

    struct dir *cached;
    if ((cached = dircache_search_by_did(vol, sid)) != NULL)
        (void)dir_remove(vol, cached);
    if ((cached = dircache_search_by_did(vol, did)) != NULL)
        (void)dir_remove(vol, cached);

    return err;
}
