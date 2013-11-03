/*
 * See COPYRIGHT.
 *
 * bug:
 * afp_XXXcomment are (the only) functions able to open
 * a ressource fork when there's no data fork, eg after
 * it was removed with samba.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>

#include <atalk/adouble.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <atalk/dsi.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/logger.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>
#include <atalk/unix.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/errchk.h>

#include "volume.h"
#include "directory.h"
#include "fork.h"
#include "desktop.h"
#include "mangle.h"

#define EXEC_MODE (S_IXGRP | S_IXUSR | S_IXOTH)

int setdeskmode(const struct vol *vol, const mode_t mode)
{
    EC_INIT;
    char		wd[ MAXPATHLEN + 1];
    struct stat         st;
    char		modbuf[ 12 + 1], *m;
    struct dirent	*deskp, *subp;
    DIR			*desk, *sub;

    if (!dir_rx_set(mode)) {
        /* want to remove read and search access to owner it will screw the volume */
        return -1 ;
    }
    if ( getcwd( wd , MAXPATHLEN) == NULL ) {
        return( -1 );
    }

    bstring dtpath = bfromcstr(vol->v_dbpath);
    bcatcstr(dtpath, "/" APPLEDESKTOP);

    EC_NEG1( chdir(cfrombstr(dtpath)) );

    if (( desk = opendir( "." )) == NULL ) {
        if ( chdir( wd ) < 0 ) {
            LOG(log_error, logtype_afpd, "setdeskmode: chdir %s: %s", wd, strerror(errno) );
        }
        EC_FAIL;
    }
    for ( deskp = readdir( desk ); deskp != NULL; deskp = readdir( desk )) {
        if ( strcmp( deskp->d_name, "." ) == 0 ||
                strcmp( deskp->d_name, ".." ) == 0 || strlen( deskp->d_name ) > 2 ) {
            continue;
        }
        strcpy( modbuf, deskp->d_name );
        strcat( modbuf, "/" );
        m = strchr( modbuf, '\0' );
        if (( sub = opendir( deskp->d_name )) == NULL ) {
            continue;
        }
        for ( subp = readdir( sub ); subp != NULL; subp = readdir( sub )) {
            if ( strcmp( subp->d_name, "." ) == 0 ||
                    strcmp( subp->d_name, ".." ) == 0 ) {
                continue;
            }
            *m = '\0';
            strcat( modbuf, subp->d_name );
            /* XXX: need to preserve special modes */
            if (lstat(modbuf, &st) < 0) {
                LOG(log_error, logtype_afpd, "setdeskmode: stat %s: %s",fullpathname(modbuf), strerror(errno) );
                continue;
            }

            if (S_ISDIR(st.st_mode)) {
                if ( chmod_acl( modbuf,  (DIRBITS | mode)) < 0 && errno != EPERM ) {
                     LOG(log_error, logtype_afpd, "setdeskmode: chmod %s: %s",fullpathname(modbuf), strerror(errno) );
                }
            } else if ( chmod_acl( modbuf,  mode & ~EXEC_MODE ) < 0 && errno != EPERM ) {
                LOG(log_error, logtype_afpd, "setdeskmode: chmod %s: %s",fullpathname(modbuf), strerror(errno) );
            }

        }
        closedir( sub );
        /* XXX: need to preserve special modes */
        if ( chmod_acl( deskp->d_name,  (DIRBITS | mode)) < 0 && errno != EPERM ) {
            LOG(log_error, logtype_afpd, "setdeskmode: chmod %s: %s",fullpathname(deskp->d_name), strerror(errno) );
        }
    }
    closedir( desk );
    if ( chdir( wd ) < 0 ) {
        LOG(log_error, logtype_afpd, "setdeskmode: chdir %s: %s", wd, strerror(errno) );
        EC_FAIL;
    }
    /* XXX: need to preserve special modes */
    if ( chmod_acl(bdata(dtpath),  (DIRBITS | mode)) < 0 && errno != EPERM ) {
        LOG(log_error, logtype_afpd, "setdeskmode: chmod %s: %s", bdata(dtpath), strerror(errno));
    }

EC_CLEANUP:
    bdestroy(dtpath);
    EC_EXIT;
}

int setdeskowner(const struct vol *vol, uid_t uid, gid_t gid)
{
    EC_INIT;
    char		wd[ MAXPATHLEN + 1];
    char		modbuf[12 + 1], *m;
    struct dirent	*deskp, *subp;
    DIR			*desk, *sub;

    if ( getcwd( wd, MAXPATHLEN ) == NULL ) {
        return( -1 );
    }

    bstring dtpath = bfromcstr(vol->v_dbpath);
    bcatcstr(dtpath, "/" APPLEDESKTOP);

    EC_NEG1( chdir(cfrombstr(dtpath)) );
    
    if (( desk = opendir( "." )) == NULL ) {
        if ( chdir( wd ) < 0 ) {
            LOG(log_error, logtype_afpd, "setdeskowner: chdir %s: %s", wd, strerror(errno) );
        }
        EC_FAIL;
    }
    for ( deskp = readdir( desk ); deskp != NULL; deskp = readdir( desk )) {
        if ( strcmp( deskp->d_name, "." ) == 0 ||
                strcmp( deskp->d_name, ".." ) == 0 ||
                strlen( deskp->d_name ) > 2 ) {
            continue;
        }
        strcpy( modbuf, deskp->d_name );
        strcat( modbuf, "/" );
        m = strchr( modbuf, '\0' );
        if (( sub = opendir( deskp->d_name )) == NULL ) {
            continue;
        }
        for ( subp = readdir( sub ); subp != NULL; subp = readdir( sub )) {
            if ( strcmp( subp->d_name, "." ) == 0 ||
                    strcmp( subp->d_name, ".." ) == 0 ) {
                continue;
            }
            *m = '\0';
            strcat( modbuf, subp->d_name );
            /* XXX: add special any uid, ignore group bits */
            if ( chown( modbuf, uid, gid ) < 0 && errno != EPERM ) {
                LOG(log_error, logtype_afpd, "setdeskown: chown %s: %s", fullpathname(modbuf), strerror(errno) );
            }
        }
        closedir( sub );
        /* XXX: add special any uid, ignore group bits */
        if ( chown( deskp->d_name, uid, gid ) < 0 && errno != EPERM ) {
            LOG(log_error, logtype_afpd, "setdeskowner: chown %s: %s",
                deskp->d_name, strerror(errno) );
        }
    }
    closedir( desk );
    if ( chdir( wd ) < 0 ) {
        LOG(log_error, logtype_afpd, "setdeskowner: chdir %s: %s", wd, strerror(errno) );
        EC_FAIL;
    }
    if (chown(cfrombstr(dtpath), uid, gid ) < 0 && errno != EPERM ) {
        LOG(log_error, logtype_afpd, "setdeskowner: chown %s: %s", fullpathname(".AppleDouble"), strerror(errno) );
    }

EC_CLEANUP:
    bdestroy(dtpath);
    EC_EXIT;
}

static void create_appledesktop_folder(const struct vol * vol)
{
    bstring olddtpath = NULL, dtpath = NULL;
    struct stat st;
    char *cmd_argv[4];

    olddtpath = bfromcstr(vol->v_path);
    bcatcstr(olddtpath, "/" APPLEDESKTOP);

    dtpath = bfromcstr(vol->v_dbpath);
    bcatcstr(dtpath, "/" APPLEDESKTOP);

    if (lstat(cfrombstr(dtpath), &st) != 0) {

        become_root();

        if (lstat(cfrombstr(olddtpath), &st) == 0) {
            cmd_argv[0] = "mv";
            cmd_argv[1] = bdata(olddtpath);
            cmd_argv[2] = bdata(dtpath);
            cmd_argv[3] = NULL;
            if (run_cmd("mv", cmd_argv) != 0) {
                LOG(log_error, logtype_afpd, "moving .AppleDesktop from \"%s\" to \"%s\" failed",
                    bdata(olddtpath), bdata(dtpath));
                mkdir(cfrombstr(dtpath), 0777);
            }
        } else {
            mkdir(cfrombstr(dtpath), 0777);
        }

        unbecome_root();
    }

    bdestroy(dtpath);
    bdestroy(olddtpath);
}

int afp_opendt(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol	*vol;
    uint16_t	vid;

    ibuf += 2;

    memcpy( &vid, ibuf, sizeof(vid));
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        *rbuflen = 0;
        return( AFPERR_PARAM );
    }

    create_appledesktop_folder(vol);

    memcpy( rbuf, &vid, sizeof(vid));
    *rbuflen = sizeof(vid);
    return( AFP_OK );
}

int afp_closedt(AFPObj *obj _U_, char *ibuf _U_, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    *rbuflen = 0;
    return( AFP_OK );
}

static struct savedt	si = { { 0, 0, 0, 0 }, -1, 0, 0 };

static char *icon_dtfile(struct vol *vol, u_char creator[ 4 ])
{
    return dtfile( vol, creator, ".icon" );
}

static int iconopen(struct vol *vol, u_char creator[ 4 ], int flags, int mode)
{
    char	*dtf, *adt, *adts;

    if ( si.sdt_fd != -1 ) {
        if ( memcmp( si.sdt_creator, creator, sizeof( CreatorType )) == 0 &&
                si.sdt_vid == vol->v_vid ) {
            return 0;
        }
        close( si.sdt_fd );
        si.sdt_fd = -1;
    }

    dtf = icon_dtfile( vol, creator);

    if (( si.sdt_fd = open( dtf, flags, ad_mode( dtf, mode ))) < 0 ) {
        if ( errno == ENOENT && ( flags & O_CREAT )) {
            if (( adts = strrchr( dtf, '/' )) == NULL ) {
                return -1;
            }
            *adts = '\0';
            if (( adt = strrchr( dtf, '/' )) == NULL ) {
                return -1;
            }
            *adt = '\0';
            (void) ad_mkdir( dtf, DIRBITS | 0777 );
            *adt = '/';
            (void) ad_mkdir( dtf, DIRBITS | 0777 );
            *adts = '/';

            if (( si.sdt_fd = open( dtf, flags, ad_mode( dtf, mode ))) < 0 ) {
                LOG(log_error, logtype_afpd, "iconopen(%s): open: %s", dtf, strerror(errno) );
                return -1;
            }
        } else {
            return -1;
        }
    }

    memcpy( si.sdt_creator, creator, sizeof( CreatorType ));
    si.sdt_vid = vol->v_vid;
    si.sdt_index = 1;
    return 0;
}

int afp_addicon(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol		*vol;
    u_char		fcreator[ 4 ], imh[ 12 ], irh[ 12 ], *p;
    int			itype, cc = AFP_OK, iovcnt = 0;
    size_t 		buflen;
    uint32_t           ftype, itag;
    uint16_t		bsize, rsize, vid;

    buflen = *rbuflen;
    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        cc = AFPERR_PARAM;
        goto addicon_err;
    }

    memcpy( fcreator, ibuf, sizeof( fcreator ));
    ibuf += sizeof( fcreator );

    memcpy( &ftype, ibuf, sizeof( ftype ));
    ibuf += sizeof( ftype );

    itype = (unsigned char) *ibuf;
    ibuf += 2;

    memcpy( &itag, ibuf, sizeof( itag ));
    ibuf += sizeof( itag );

    memcpy( &bsize, ibuf, sizeof( bsize ));
    bsize = ntohs( bsize );

    if ( si.sdt_fd != -1 ) {
        (void)close( si.sdt_fd );
        si.sdt_fd = -1;
    }

    if (iconopen( vol, fcreator, O_RDWR|O_CREAT, 0666 ) < 0) {
        cc = AFPERR_NOITEM;
        goto addicon_err;
    }

    if (lseek( si.sdt_fd, (off_t) 0L, SEEK_SET ) < 0) {
        close(si.sdt_fd);
        si.sdt_fd = -1;
        LOG(log_error, logtype_afpd, "afp_addicon(%s): lseek: %s", icon_dtfile(vol, fcreator), strerror(errno) );
        cc = AFPERR_PARAM;
        goto addicon_err;
    }

    /*
     * Read icon elements until we find a match to replace, or
     * we get to the end to insert.
     */
    p = imh;
    memcpy( p, &itag, sizeof( itag ));
    p += sizeof( itag );
    memcpy( p, &ftype, sizeof( ftype ));
    p += sizeof( ftype );
    *p++ = itype;
    *p++ = 0;
    bsize = htons( bsize );
    memcpy( p, &bsize, sizeof( bsize ));
    bsize = ntohs( bsize );
    while (( cc = read( si.sdt_fd, irh, sizeof( irh ))) > 0 ) {
        memcpy( &rsize, irh + 10, sizeof( rsize ));
        rsize = ntohs( rsize );
        /*
         * Is this our set of headers?
         */
        if ( memcmp( irh, imh, sizeof( irh ) - sizeof( u_short )) == 0 ) {
            /*
             * Is the size correct?
             */
            if ( bsize != rsize )
                cc = AFPERR_ITYPE;
            break;
        }

        if ( lseek( si.sdt_fd, (off_t) rsize, SEEK_CUR ) < 0 ) {
            LOG(log_error, logtype_afpd, "afp_addicon(%s): lseek: %s", icon_dtfile(vol, fcreator),strerror(errno) );
            cc = AFPERR_PARAM;
        }
    }

    /*
     * Some error occurred, return.
     */
addicon_err:
    if ( cc < 0 ) {
        dsi_writeinit(obj->dsi, rbuf, buflen);
        dsi_writeflush(obj->dsi);
        return cc;
    }

    DSI *dsi = obj->dsi;

    iovcnt = dsi_writeinit(dsi, rbuf, buflen);

    /* add headers at end of file */
    if ((cc == 0) && (write(si.sdt_fd, imh, sizeof(imh)) < 0)) {
        LOG(log_error, logtype_afpd, "afp_addicon(%s): write: %s", icon_dtfile(vol, fcreator), strerror(errno));
        dsi_writeflush(dsi);
        return AFPERR_PARAM;
    }

    if ((cc = write(si.sdt_fd, rbuf, iovcnt)) < 0) {
        LOG(log_error, logtype_afpd, "afp_addicon(%s): write: %s", icon_dtfile(vol, fcreator), strerror(errno));
        dsi_writeflush(dsi);
        return AFPERR_PARAM;
    }

    while ((iovcnt = dsi_write(dsi, rbuf, buflen))) {
        if ((cc = write(si.sdt_fd, rbuf, iovcnt)) < 0) {
            LOG(log_error, logtype_afpd, "afp_addicon(%s): write: %s", icon_dtfile(vol, fcreator), strerror(errno));
            dsi_writeflush(dsi);
            return AFPERR_PARAM;
        }
    }

    close( si.sdt_fd );
    si.sdt_fd = -1;
    return( AFP_OK );
}

static const u_char	utag[] = { 0, 0, 0, 0 };
static const u_char	ucreator[] = { 0, 0, 0, 0 };/* { 'U', 'N', 'I', 'X' };*/
static const u_char	utype[] = { 0, 0, 0, 0 };/* { 'T', 'E', 'X', 'T' };*/
static const short	usize = 256;

#if 0
static const u_char	uicon[] = {
    0x1F, 0xFF, 0xFC, 0x00, 0x10, 0x00, 0x06, 0x00,
    0x10, 0x00, 0x05, 0x00, 0x10, 0x00, 0x04, 0x80,
    0x10, 0x00, 0x04, 0x40, 0x10, 0x00, 0x04, 0x20,
    0x10, 0x00, 0x07, 0xF0, 0x10, 0x00, 0x00, 0x10,
    0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x10,
    0x10, 0x00, 0x00, 0x10, 0x10, 0x80, 0x02, 0x10,
    0x11, 0x80, 0x03, 0x10, 0x12, 0x80, 0x02, 0x90,
    0x12, 0x80, 0x02, 0x90, 0x14, 0x80, 0x02, 0x50,
    0x14, 0x87, 0xC2, 0x50, 0x14, 0x58, 0x34, 0x50,
    0x14, 0x20, 0x08, 0x50, 0x12, 0x16, 0xD0, 0x90,
    0x11, 0x01, 0x01, 0x10, 0x12, 0x80, 0x02, 0x90,
    0x12, 0x9C, 0x72, 0x90, 0x14, 0x22, 0x88, 0x50,
    0x14, 0x41, 0x04, 0x50, 0x14, 0x49, 0x24, 0x50,
    0x14, 0x55, 0x54, 0x50, 0x14, 0x5D, 0x74, 0x50,
    0x14, 0x5D, 0x74, 0x50, 0x12, 0x49, 0x24, 0x90,
    0x12, 0x22, 0x88, 0x90, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFC, 0x00, 0x1F, 0xFF, 0xFE, 0x00,
    0x1F, 0xFF, 0xFF, 0x00, 0x1F, 0xFF, 0xFF, 0x80,
    0x1F, 0xFF, 0xFF, 0xC0, 0x1F, 0xFF, 0xFF, 0xE0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
    0x1F, 0xFF, 0xFF, 0xF0, 0x1F, 0xFF, 0xFF, 0xF0,
};
#endif

int afp_geticoninfo(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol	*vol;
    unsigned char	fcreator[ 4 ], ih[ 12 ];
    uint16_t	vid, iindex, bsize;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( fcreator, ibuf, sizeof( fcreator ));
    ibuf += sizeof( fcreator );
    memcpy( &iindex, ibuf, sizeof( iindex ));
    iindex = ntohs( iindex );

    if ( memcmp( fcreator, ucreator, sizeof( ucreator )) == 0 ) {
        if ( iindex > 1 ) {
            return( AFPERR_NOITEM );
        }
        memcpy( ih, utag, sizeof( utag ));
        memcpy( ih + sizeof( utag ), utype, sizeof( utype ));
        *( ih + sizeof( utag ) + sizeof( utype )) = 1;
        *( ih + sizeof( utag ) + sizeof( utype ) + 1 ) = 0;
        memcpy( ih + sizeof( utag ) + sizeof( utype ) + 2, &usize,
                sizeof( usize ));
        memcpy( rbuf, ih, sizeof( ih ));
        *rbuflen = sizeof( ih );
        return( AFP_OK );
    }

    if ( iconopen( vol, fcreator, O_RDONLY, 0 ) < 0) {
        return( AFPERR_NOITEM );
    }

    if ( iindex < si.sdt_index ) {
        if ( lseek( si.sdt_fd, (off_t) 0L, SEEK_SET ) < 0 ) {
            return( AFPERR_PARAM );
        }
        si.sdt_index = 1;
    }

    /*
     * Position to the correct spot.
     */
    for (;;) {
        if ( read( si.sdt_fd, ih, sizeof( ih )) != sizeof( ih )) {
            close( si.sdt_fd );
            si.sdt_fd = -1;
            return( AFPERR_NOITEM );
        }
        memcpy( &bsize, ih + 10, sizeof( bsize ));
        bsize = ntohs(bsize);
        if ( lseek( si.sdt_fd, (off_t) bsize, SEEK_CUR ) < 0 ) {
            LOG(log_error, logtype_afpd, "afp_iconinfo(%s): lseek: %s", icon_dtfile(vol, fcreator), strerror(errno) );
            return( AFPERR_PARAM );
        }
        if ( si.sdt_index == iindex ) {
            memcpy( rbuf, ih, sizeof( ih ));
            *rbuflen = sizeof( ih );
            return( AFP_OK );
        }
        si.sdt_index++;
    }
}


int afp_geticon(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol	*vol;
    off_t       offset;
    ssize_t	rc, buflen;
    u_char	fcreator[ 4 ], ftype[ 4 ], itype, ih[ 12 ];
    uint16_t	vid, bsize, rsize;

    buflen = *rbuflen;
    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( fcreator, ibuf, sizeof( fcreator ));
    ibuf += sizeof( fcreator );
    memcpy( ftype, ibuf, sizeof( ftype ));
    ibuf += sizeof( ftype );
    itype = (unsigned char) *ibuf++;
    ibuf++;
    memcpy( &bsize, ibuf, sizeof( bsize ));
    bsize = ntohs( bsize );

#if 0
    if ( memcmp( fcreator, ucreator, sizeof( ucreator )) == 0 &&
            memcmp( ftype, utype, sizeof( utype )) == 0 &&
            itype == 1 &&
            bsize <= usize ) {
        memcpy( rbuf, uicon, bsize);
        *rbuflen = bsize;
        return( AFP_OK );
    }
#endif

    if ( iconopen( vol, fcreator, O_RDONLY, 0 ) < 0) {
        return( AFPERR_NOITEM );
    }

    if ( lseek( si.sdt_fd, (off_t) 0L, SEEK_SET ) < 0 ) {
        close(si.sdt_fd);
        si.sdt_fd = -1;
        LOG(log_error, logtype_afpd, "afp_geticon(%s): lseek: %s", icon_dtfile(vol, fcreator), strerror(errno));
        return( AFPERR_PARAM );
    }

    si.sdt_index = 1;
    offset = 0;
    while (( rc = read( si.sdt_fd, ih, sizeof( ih ))) > 0 ) {
        si.sdt_index++;
        offset += sizeof(ih);
        if ( memcmp( ih + sizeof( int ), ftype, sizeof( ftype )) == 0 &&
                *(ih + sizeof( int ) + sizeof( ftype )) == itype ) {
            break;
        }
        memcpy( &rsize, ih + 10, sizeof( rsize ));
        rsize = ntohs( rsize );
        if ( lseek( si.sdt_fd, (off_t) rsize, SEEK_CUR ) < 0 ) {
            LOG(log_error, logtype_afpd, "afp_geticon(%s): lseek: %s", icon_dtfile(vol, fcreator), strerror(errno) );
            return( AFPERR_PARAM );
        }
        offset += rsize;
    }

    if ( rc < 0 ) {
        LOG(log_error, logtype_afpd, "afp_geticon(%s): read: %s", icon_dtfile(vol, fcreator), strerror(errno));
        return( AFPERR_PARAM );
    }

    if ( rc == 0 ) {
        return( AFPERR_NOITEM );
    }

    memcpy( &rsize, ih + 10, sizeof( rsize ));
    rsize = ntohs( rsize );
#define min(a,b)	((a)<(b)?(a):(b))
    rc = min( bsize, rsize );

    if (buflen < rc) {
        DSI *dsi = obj->dsi;
        struct stat st;
        off_t size;

        size = (fstat(si.sdt_fd, &st) < 0) ? 0 : st.st_size;
        if (size < rc + offset) {
            return AFPERR_PARAM;
        }

#ifndef WITH_SENDFILE
        if ((buflen = dsi_readinit(dsi, rbuf, buflen, rc, AFP_OK)) < 0)
            goto geticon_exit;
#endif

        *rbuflen = buflen;
        /* do to the streaming nature, we have to exit if we encounter
         * a problem. much confusion results otherwise. */
        while (*rbuflen > 0) {
#ifdef WITH_SENDFILE
            if (dsi_stream_read_file(dsi, si.sdt_fd, offset, dsi->datasize, AFP_OK) < 0) {
                switch (errno) {
                case ENOSYS:
                case EINVAL:  /* there's no guarantee that all fs support sendfile */
                    break;
                default:
                    goto geticon_exit;
                }
            }
            else {
                dsi_readdone(dsi);
                return AFP_OK;
            }
#endif
            buflen = read(si.sdt_fd, rbuf, *rbuflen);
            if (buflen < 0)
                goto geticon_exit;

            /* dsi_read() also returns buffer size of next allocation */
            buflen = dsi_read(dsi, rbuf, buflen); /* send it off */
            if (buflen < 0)
                goto geticon_exit;

            *rbuflen = buflen;
        }
        
        dsi_readdone(dsi);
        return AFP_OK;

geticon_exit:
        LOG(log_error, logtype_afpd, "afp_geticon(%s): %s", icon_dtfile(vol, fcreator), strerror(errno));
        dsi_readdone(dsi);
        obj->exit(EXITERR_SYS);
        return AFP_OK;

    } else {
        if ( read( si.sdt_fd, rbuf, rc ) < rc ) {
            return( AFPERR_PARAM );
        }
        *rbuflen = rc;
    }
    return AFP_OK;
}

/* ---------------------- */
static const char		hexdig[] = "0123456789abcdef";
char *dtfile(const struct vol *vol, u_char creator[], char *ext )
{
    static char	path[ MAXPATHLEN + 1];
    char	*p;
    unsigned int i;

    strcpy( path, vol->v_dbpath );
    strcat( path, "/" APPLEDESKTOP "/" );
    for ( p = path; *p != '\0'; p++ )
        ;

    if ( !isprint( creator[ 0 ] ) || creator[ 0 ] == '/' ) {
        *p++ = hexdig[ ( creator[ 0 ] & 0xf0 ) >> 4 ];
        *p++ = hexdig[ creator[ 0 ] & 0x0f ];
    } else {
        *p++ = creator[ 0 ];
    }

    *p++ = '/';

    for ( i = 0; i < sizeof( CreatorType ); i++ ) {
        if ( !isprint( creator[ i ] ) || creator[ i ] == '/' ) {
            *p++ = hexdig[ ( creator[ i ] & 0xf0 ) >> 4 ];
            *p++ = hexdig[ creator[ i ] & 0x0f ];
        } else {
            *p++ = creator[ i ];
        }
    }
    *p = '\0';
    strcat( path, ext );

    return( path );
}

/* ---------------------------
 * mpath is only a filename 
 * did filename parent directory ID.
*/

char *mtoupath(const struct vol *vol, char *mpath, cnid_t did, int utf8)
{
    static char  upath[ MAXPATHLEN + 2]; /* for convert_charset dest_len parameter +2 */
    char	*m, *u;
    size_t       inplen;
    size_t       outlen;
    uint16_t	 flags;
        
    if ( *mpath == '\0' ) {
        strcpy(upath, ".");
        return upath;
    }

    /* set conversion flags */
    flags = vol->v_mtou_flags;
    
    m = demangle(vol, mpath, did);
    if (m != mpath) {
        return m;
    }

    m = mpath;
    u = upath;

    inplen = strlen(m);
    outlen = MAXPATHLEN;

    if ((size_t)-1 == (outlen = convert_charset ( (utf8)?CH_UTF8_MAC:vol->v_maccharset, vol->v_volcharset, vol->v_maccharset, m, inplen, u, outlen, &flags)) ) {
        LOG(log_error, logtype_afpd, "conversion from %s to %s for %s failed.", (utf8)?"UTF8-MAC":vol->v_maccodepage, vol->v_volcodepage, mpath);
	    return NULL;
    }

#ifdef DEBUG
    LOG(log_debug9, logtype_afpd, "mtoupath: '%s':'%s'", mpath, upath);
#endif /* DEBUG */
    return( upath );
}

/* --------------- 
 * id filename ID
*/
char *utompath(const struct vol *vol, char *upath, cnid_t id, int utf8)
{
    static char  mpath[ MAXPATHLEN + 2]; /* for convert_charset dest_len parameter +2 */
    char        *m, *u;
    uint16_t    flags;
    size_t       outlen;

    m = mpath;
    outlen = strlen(upath);

    flags = vol->v_utom_flags;

    u = upath;

    /* convert charsets */
    if ((size_t)-1 == ( outlen = convert_charset ( vol->v_volcharset, (utf8)?CH_UTF8_MAC:vol->v_maccharset, vol->v_maccharset, u, outlen, mpath, MAXPATHLEN, &flags)) ) { 
        LOG(log_error, logtype_afpd, "Conversion from %s to %s for %s (%u) failed.", vol->v_volcodepage, vol->v_maccodepage, u, ntohl(id));
	goto utompath_error;
    }

    flags = !!(flags & CONV_REQMANGLE);

    if (utf8)
        flags |= 2;

    m = mangle(vol, mpath, outlen, upath, id, flags);

#ifdef DEBUG
    LOG(log_debug9, logtype_afpd, "utompath: '%s':'%s':'%2.2X'", upath, m, ntohl(id));
#endif /* DEBUG */
    return(m);

utompath_error:
    u = "???";
    m = mangle(vol, u, strlen(u), upath, id, (utf8)?3:1);
    return(m);
}

/* ------------------------- */
static int ad_addcomment(const AFPObj *obj, struct vol *vol, struct path *path, char *ibuf)
{
    struct ofork        *of;
    char                *name, *upath;
    int                 isadir;
    int			clen;
    struct adouble	ad, *adp;

    clen = (u_char)*ibuf++;
    clen = min( clen, 199 );

    upath = path->u_name;
    if (check_access(obj, vol, upath, OPENACC_WR ) < 0) {
        return AFPERR_ACCESS;
    }
    
    isadir = path_isadir(path);
    if (isadir || !(of = of_findname(vol, path))) {
        ad_init(&ad, vol);
        adp = &ad;
    } else
        adp = of->of_ad;

    if (ad_open(adp, upath,
                ADFLAGS_HF | ( (isadir) ? ADFLAGS_DIR : 0) | ADFLAGS_CREATE | ADFLAGS_RDWR,
                0666) < 0 ) {
        return( AFPERR_ACCESS );
    }

    if (ad_getentryoff(adp, ADEID_COMMENT)) {
        if ( (ad_get_MD_flags( adp ) & O_CREAT) ) {
            if ( *path->m_name == '\0' ) {
                name = (char *)curdir->d_m_name->data;
            } else {
                name = path->m_name;
            }
            ad_setname(adp, name);
        }
        ad_setentrylen( adp, ADEID_COMMENT, clen );
        memcpy( ad_entry( adp, ADEID_COMMENT ), ibuf, clen );
        ad_flush( adp );
    }
    ad_close(adp, ADFLAGS_HF);
    return( AFP_OK );
}

/* ----------------------------- */
int afp_addcomment(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    struct path         *path;
    uint32_t           did;
    uint16_t		vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
	return afp_errno;
    }

    if (NULL == ( path = cname( vol, dir, &ibuf )) ) {
	return get_afp_errno(AFPERR_NOOBJ);
    }

    if ((u_long)ibuf & 1 ) {
        ibuf++;
    }

    return ad_addcomment(obj, vol, path, ibuf);
}

/* -------------------- */
static int ad_getcomment(struct vol *vol, struct path *path, char *rbuf, size_t *rbuflen)
{
    struct adouble	ad, *adp;
    struct ofork        *of;
    char		*upath;
    int                 isadir;
    int			clen;

    upath = path->u_name;
    isadir = path_isadir(path);
    if (isadir || !(of = of_findname(vol, path))) {
        ad_init(&ad, vol);
        adp = &ad;
    } else
        adp = of->of_ad;
        
    if ( ad_metadata( upath, ((isadir) ? ADFLAGS_DIR : 0), adp) < 0 ) {
        return( AFPERR_NOITEM );
    }

    if (!ad_getentryoff(adp, ADEID_COMMENT)) {
        ad_close(adp, ADFLAGS_HF);
        return AFPERR_NOITEM;
    }
    /*
     * Make sure the AD file is not bogus.
     */
    if ( ad_getentrylen( adp, ADEID_COMMENT ) <= 0 ||
            ad_getentrylen( adp, ADEID_COMMENT ) > 199 ) {
        ad_close(adp, ADFLAGS_HF);
        return( AFPERR_NOITEM );
    }

    clen = min( ad_getentrylen( adp, ADEID_COMMENT ), 128 ); /* OSX only use 128, greater kill Adobe CS2 */
    *rbuf++ = clen;
    memcpy( rbuf, ad_entry( adp, ADEID_COMMENT ), clen);
    *rbuflen = clen + 1;
    ad_close(adp, ADFLAGS_HF);

    return( AFP_OK );
}

/* -------------------- */
int afp_getcomment(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    struct path         *s_path;
    uint32_t		did;
    uint16_t		vid;
    
    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
	return afp_errno;
    }

    if (NULL == ( s_path = cname( vol, dir, &ibuf )) ) {
	return get_afp_errno(AFPERR_NOOBJ);
    }

    return ad_getcomment(vol, s_path, rbuf, rbuflen);
}

/* ----------------------- */
static int ad_rmvcomment(const AFPObj *obj, struct vol *vol, struct path *path)
{
    struct adouble	ad, *adp;
    struct ofork        *of;
    int                 isadir;
    char		*upath;

    upath = path->u_name;
    if (check_access(obj, vol, upath, OPENACC_WR ) < 0) {
        return AFPERR_ACCESS;
    }

    isadir = path_isadir(path);
    if (isadir || !(of = of_findname(vol, path))) {
        ad_init(&ad, vol);
        adp = &ad;
    } else
        adp = of->of_ad;

    if ( ad_open(adp, upath, ADFLAGS_HF | ADFLAGS_RDWR | ((isadir) ? ADFLAGS_DIR : 0)) < 0 ) {
        switch ( errno ) {
        case ENOENT :
            return( AFPERR_NOITEM );
        case EACCES :
            return( AFPERR_ACCESS );
        default :
            return( AFPERR_PARAM );
        }
    }

    if (ad_getentryoff(adp, ADEID_COMMENT)) {
        ad_setentrylen( adp, ADEID_COMMENT, 0 );
        ad_flush( adp );
    }
    ad_close(adp, ADFLAGS_HF);
    return( AFP_OK );
}

/* ----------------------- */
int afp_rmvcomment(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol		*vol;
    struct dir		*dir;
    struct path         *s_path;
    uint32_t		did;
    uint16_t		vid;

    *rbuflen = 0;
    ibuf += 2;

    memcpy( &vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    memcpy( &did, ibuf, sizeof( did ));
    ibuf += sizeof( did );
    if (NULL == ( dir = dirlookup( vol, did )) ) {
	return afp_errno;
    }

    if (NULL == ( s_path = cname( vol, dir, &ibuf ))) {
	return get_afp_errno(AFPERR_NOOBJ);
    }
    
    return ad_rmvcomment(obj, vol, s_path);
}
