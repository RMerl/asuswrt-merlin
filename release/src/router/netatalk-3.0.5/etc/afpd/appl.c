/*
 *
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/param.h>
#include <atalk/logger.h>
#include <errno.h>

#include <atalk/adouble.h>
#include <atalk/afp.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>

#include "volume.h"
#include "directory.h"
#include "file.h"
#include "desktop.h"

static struct savedt	sa = { { 0, 0, 0, 0 }, -1, 0, 0};

static int pathcmp(char *p, int plen, char *q, int qlen)
{
    return (( plen == qlen && memcmp( p, q, plen ) == 0 ) ? 0 : 1 );
}

static int applopen(struct vol *vol, u_char creator[ 4 ], int flags, int mode)
{
    char	*dtf, *adt, *adts;

    if ( sa.sdt_fd != -1 ) {
        if ( !(flags & ( O_RDWR | O_WRONLY )) &&
                memcmp( sa.sdt_creator, creator, sizeof( CreatorType )) == 0 &&
                sa.sdt_vid == vol->v_vid ) {
            return( AFP_OK );
        }
        close( sa.sdt_fd );
        sa.sdt_fd = -1;
    }

    dtf = dtfile( vol, creator, ".appl" );

    if (( sa.sdt_fd = open( dtf, flags, ad_mode( dtf, mode ))) < 0 ) {
        if ( errno == ENOENT && ( flags & O_CREAT )) {
            if (( adts = strrchr( dtf, '/' )) == NULL ) {
                return( AFPERR_PARAM );
            }
            *adts = '\0';
            if (( adt = strrchr( dtf, '/' )) == NULL ) {
                return( AFPERR_PARAM );
            }
            *adt = '\0';
            (void) ad_mkdir( dtf, DIRBITS | 0777 );
            *adt = '/';
            (void) ad_mkdir( dtf, DIRBITS | 0777 );
            *adts = '/';

            if (( sa.sdt_fd = open( dtf, flags, ad_mode( dtf, mode ))) < 0 ) {
                return( AFPERR_PARAM );
            }
        } else {
            return( AFPERR_PARAM );
        }
    }
    memcpy( sa.sdt_creator, creator, sizeof( CreatorType ));
    sa.sdt_vid = vol->v_vid;
    sa.sdt_index = 0;
    return( AFP_OK );
}

/*
 * copy appls to new file, deleting any matching (old) appl entries
 */
static int copyapplfile(int sfd, int dfd, char *mpath, u_short mplen)
{
    int		cc;
    char	*p;
    uint16_t	len;
    unsigned char	appltag[ 4 ];
    char	buf[ MAXPATHLEN ];

    while (( cc = read( sfd, buf, sizeof(appltag) + sizeof( u_short ))) > 0 ) {
        p = buf + sizeof(appltag);
        memcpy( &len, p, sizeof(len));
        len = ntohs( len );
        p += sizeof( len );
        if (( cc = read( sa.sdt_fd, p, len )) < len ) {
            break;
        }
        if ( pathcmp( mpath, mplen, p, len ) != 0 ) {
            p += len;
            if ( write( dfd, buf, p - buf ) != p - buf ) {
                cc = -1;
                break;
            }
        }
    }
    return( cc );
}

/*
 * build mac. path (backwards) by traversing the directory tree
 *
 * The old way: dir and path refer to an app, path is a mac format
 * pathname.  makemacpath() builds something that looks like a cname,
 * but uses upaths instead of mac format paths.
 *
 * The new way: dir and path refer to an app, path is a mac format
 * pathname.  makemacpath() builds a cname. (zero is a path separator
 * and it's not \0 terminated).
 *
 * See afp_getappl() for the backward compatiblity code.
 */
static char *
makemacpath(const struct vol *vol, char *mpath, int mpathlen, struct dir *dir, char *path)
{
    char	*p;

    p = mpath + mpathlen;
    p -= strlen( path );
    memcpy( p, path, strlen( path )); 

    while ( dir->d_did != DIRDID_ROOT ) {
        p -= blength(dir->d_m_name) + 1;
        if (p < mpath) {
            /* FIXME: pathname too long */
            return NULL;
        }
        memcpy(p, cfrombstr(dir->d_m_name), blength(dir->d_m_name) + 1);
        if ((dir = dirlookup(vol, dir->d_pdid)) == NULL)
            return NULL;
    }
    return( p );


#if 0
    char buffer[12 + MAXPATHLEN + 1];
    int buflen = 12 + MAXPATHLEN + 1;
    char *ret = mpath;
    char *path = name;
    char *uname = NULL;
    struct bstrList *pathlist = NULL;
    cnid_t cnid = dir->d_pdid;

    /* Create list for path elements, request 16 list elements for now*/
    if ((pathlist = bstListCreateMin(16)) == NULL) {
        LOG(log_error, logtype_afpd, "makemacpath: OOM: %s", strerror(errno));
        return NULL;
    }

    while ( cnid != DIRDID_ROOT ) {

        /* construct path, copy already found uname to path element list*/
        if ((bstrListPush(pathlist, bfromcstr(path))) != BSTR_OK) {
            afp_errno = AFPERR_MISC;
            ret = NULL;
            goto exit;
        }

        /* next part */
        AFP_CNID_START("cnid_resolve");
        uname = cnid_resolve(vol->v_cdb, &cnid, buffer, buflen);
        AFP_CNID_DONE();

        if (uname == NULL) {
            afp_errno = AFPERR_NOOBJ;
            ret = NULL;
            goto exit;
        }

        if ((path = utompath(vol, uname, cnid, utf8_encoding())) == NULL) {
            afp_errno = AFPERR_MISC;
            ret = NULL;
            goto exit;
        }
    }



exit:
    if (pathlist)
        bstrListDestroy(pathlist);

    return(ret);
#endif
}


int afp_addappl(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    int			tfd, cc;
    uint32_t           did;
    uint16_t		vid, mplen;
    struct path         *path;
    char                *dtf, *p, *mp;
    unsigned char		creator[ 4 ];
    unsigned char		appltag[ 4 ];
    char		*mpath, *tempfile;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid ))) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    memcpy( creator, ibuf, sizeof( creator ));
    ibuf += sizeof( creator );

    memcpy( appltag, ibuf, sizeof( appltag ));
    ibuf += sizeof( appltag );

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }
    if ( path_isadir(path) ) {
        return( AFPERR_BADTYPE );
    }

    if ( applopen( vol, creator, O_RDWR|O_CREAT, 0666 ) != AFP_OK ) {
        return( AFPERR_PARAM );
    }
    if ( lseek( sa.sdt_fd, 0L, SEEK_SET ) < 0 ) {
        return( AFPERR_PARAM );
    }
    dtf = dtfile( vol, creator, ".appl.temp" );
    tempfile = obj->oldtmp;
    strcpy( tempfile, dtf );
    if (( tfd = open( tempfile, O_RDWR|O_CREAT, 0666 )) < 0 ) {
        return( AFPERR_PARAM );
    }
    mpath = obj->newtmp;
    mp = makemacpath( vol, mpath, AFPOBJ_TMPSIZ, curdir, path->m_name );
    if (!mp) {
        close(tfd);
        return AFPERR_PARAM;
    }
    mplen =  mpath + AFPOBJ_TMPSIZ - mp;

    /* write the new appl entry at start of temporary file */
    p = mp - sizeof( u_short );
    mplen = htons( mplen );
    memcpy( p, &mplen, sizeof( mplen ));
    mplen = ntohs( mplen );
    p -= sizeof( appltag );
    memcpy(p, appltag, sizeof( appltag ));
    cc = mpath + AFPOBJ_TMPSIZ - p;
    if ( write( tfd, p, cc ) != cc ) {
        close(tfd);
        unlink( tempfile );
        return( AFPERR_PARAM );
    }
    cc = copyapplfile( sa.sdt_fd, tfd, mp, mplen );
    close( tfd );
    close( sa.sdt_fd );
    sa.sdt_fd = -1;

    if ( cc < 0 ) {
        unlink( tempfile );
        return( AFPERR_PARAM );
    }
    if ( rename( tempfile, dtfile( vol, creator, ".appl" )) < 0 ) {
        return( AFPERR_PARAM );
    }
    return( AFP_OK );
}

int afp_rmvappl(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    int			tfd, cc;
    uint32_t           did;
    uint16_t		vid, mplen;
    struct path    	*path;
    char                *dtf, *mp;
    unsigned char		creator[ 4 ];
    char                *tempfile, *mpath;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid ))) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
        return afp_errno;
    }

    memcpy( creator, ibuf, sizeof( creator ));
    ibuf += sizeof( creator );

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
        return get_afp_errno(AFPERR_PARAM);
    }
    if ( path_isadir(path) ) {
        return( AFPERR_BADTYPE );
    }

    if ( applopen( vol, creator, O_RDWR, 0666 ) != AFP_OK ) {
        return( AFPERR_NOOBJ );
    }
    if ( lseek( sa.sdt_fd, 0L, SEEK_SET ) < 0 ) {
        return( AFPERR_PARAM );
    }
    dtf = dtfile( vol, creator, ".appl.temp" );
    tempfile = obj->oldtmp;
    strcpy( tempfile, dtf );
    if (( tfd = open( tempfile, O_RDWR|O_CREAT, 0666 )) < 0 ) {
        return( AFPERR_PARAM );
    }
    mpath = obj->newtmp;
    mp = makemacpath( vol, mpath, AFPOBJ_TMPSIZ, curdir, path->m_name );
    if (!mp) {
        close(tfd);
        return AFPERR_PARAM ;
    }

    mplen =  mpath + AFPOBJ_TMPSIZ - mp;
    cc = copyapplfile( sa.sdt_fd, tfd, mp, mplen );
    close( tfd );
    close( sa.sdt_fd );
    sa.sdt_fd = -1;

    if ( cc < 0 ) {
        unlink( tempfile );
        return( AFPERR_PARAM );
    }
    if ( rename( tempfile, dtfile( vol, creator, ".appl" )) < 0 ) {
        return( AFPERR_PARAM );
    }
    return( AFP_OK );
}

int afp_getappl(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol		*vol;
    char		*p, *q;
    int			cc; 
    size_t		buflen;
    uint16_t		vid, aindex, bitmap, len;
    unsigned char		creator[ 4 ];
    unsigned char		appltag[ 4 ];
    char                *buf, *cbuf;
    struct path         *path;
    
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        *rbuflen = 0;
        return( AFPERR_PARAM );
    }

    memcpy( creator, ibuf, sizeof( creator ));
    ibuf += sizeof( creator );

    memcpy( &aindex, ibuf, sizeof( aindex ));
    ibuf += sizeof( aindex );
    aindex = ntohs( aindex );
    if ( aindex ) { /* index 0 == index 1 */
        --aindex;
    }

    memcpy( &bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    if ( applopen( vol, creator, O_RDONLY, 0666 ) != AFP_OK ) {
        *rbuflen = 0;
        return( AFPERR_NOITEM );
    }
    if ( aindex < sa.sdt_index ) {
        if ( lseek( sa.sdt_fd, 0L, SEEK_SET ) < 0 ) {
            *rbuflen = 0;
            return( AFPERR_PARAM );
        }
        sa.sdt_index = 0;
    }

    /* position to correct spot within appl file */
    buf = obj->oldtmp;
    while (( cc = read( sa.sdt_fd, buf, sizeof( appltag )
                        + sizeof( u_short ))) > 0 ) {
        p = buf + sizeof( appltag );
        memcpy( &len, p, sizeof( len ));
        len = ntohs( len );
        p += sizeof( u_short );
        if (( cc = read( sa.sdt_fd, p, len )) < len ) {
            break;
        }
        if ( sa.sdt_index == aindex ) {
            break;
        }
        sa.sdt_index++;
    }
    if ( cc <= 0 || sa.sdt_index != aindex ) {
        *rbuflen = 0;
        return( AFPERR_NOITEM );
    }
    sa.sdt_index++;

#ifdef APPLCNAME
    /*
     * Check to see if this APPL mapping has an mpath or a upath.  If
     * there are any ':'s in the name, it is a upath and must be converted
     * to an mpath.  Hopefully, this code will go away.
     */
    {
#define hextoint( c )	( isdigit( c ) ? c - '0' : c + 10 - 'a' )
#define islxdigit(x)	(!isupper(x)&&isxdigit(x))

        char	utomname[ MAXPATHLEN + 1];
        char		*u, *m;
        int		i, h;

        u = p;
        m = utomname;
        i = len;
        while ( i ) {
            if ( *u == ':' && *(u+1) != '\0' && islxdigit( *(u+1)) &&
                    *(u+2) != '\0' && islxdigit( *(u+2))) {
                ++u, --i;
                h = hextoint( *u ) << 4;
                ++u, --i;
                h |= hextoint( *u );
                *m++ = h;
            } else {
                *m++ = *u;
            }
            ++u, --i;
        }

        len = m - utomname;
        p = utomname;

        if ( p[ len - 1 ] == '\0' ) {
            len--;
        }
    }
#endif /* APPLCNAME */

    /* fake up a cname */
    cbuf = obj->newtmp;
    q = cbuf;
    *q++ = 2;	/* long path type */
    *q++ = (unsigned char)len;
    memcpy( q, p, len );
    q = cbuf;

    if (( path = cname( vol, vol->v_root, &q )) == NULL ) {
        *rbuflen = 0;
        return( AFPERR_NOITEM );
    }

    if ( path_isadir(path) || path->st_errno ) {
        *rbuflen = 0;
        return( AFPERR_NOITEM );
    }
    buflen = *rbuflen - sizeof( bitmap ) - sizeof( appltag );
    if ( getfilparams(obj, vol, bitmap, path, curdir, rbuf + sizeof( bitmap ) +
                      sizeof( appltag ), &buflen, 0) != AFP_OK ) {
        *rbuflen = 0;
        return( AFPERR_BITMAP );
    }

    *rbuflen = buflen + sizeof( bitmap ) + sizeof( appltag );
    bitmap = htons( bitmap );
    memcpy( rbuf, &bitmap, sizeof( bitmap ));
    rbuf += sizeof( bitmap );
    memcpy( rbuf, appltag, sizeof( appltag ));
    rbuf += sizeof( appltag );
    return( AFP_OK );
}

