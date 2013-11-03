/*
 * Copyright (c) 1990,1993 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <utime.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <time.h>

#include <atalk/dsi.h>
#include <atalk/adouble.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/logger.h>
#include <atalk/vfs.h>
#include <atalk/uuid.h>
#include <atalk/ea.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/ftw.h>
#include <atalk/globals.h>
#include <atalk/fce_api.h>
#include <atalk/errchk.h>
#include <atalk/iniparser.h>
#include <atalk/unix.h>
#include <atalk/netatalk_conf.h>
#include <atalk/server_ipc.h>

#ifdef CNID_DB
#include <atalk/cnid.h>
#endif /* CNID_DB*/

#include "directory.h"
#include "file.h"
#include "volume.h"
#include "unix.h"
#include "mangle.h"
#include "fork.h"
#include "hash.h"
#include "acls.h"

#define VOLPASSLEN  8

extern int afprun(int root, char *cmd, int *outfd);

/*!
 * Read band-size info from Info.plist XML file of an TM sparsebundle
 *
 * @param path   (r) path to Info.plist file
 * @return           band-size in bytes, -1 on error
 */
static long long int get_tm_bandsize(const char *path)
{
    EC_INIT;
    FILE *file = NULL;
    char buf[512];
    long long int bandsize = -1;

    EC_NULL_LOGSTR( file = fopen(path, "r"),
                    "get_tm_bandsize(\"%s\"): %s",
                    path, strerror(errno) );

    while (fgets(buf, sizeof(buf), file) != NULL) {
        if (strstr(buf, "band-size") == NULL)
            continue;

        if (fscanf(file, " <integer>%lld</integer>", &bandsize) != 1) {
            LOG(log_error, logtype_afpd, "get_tm_bandsize(\"%s\"): can't parse band-size", path);
            EC_FAIL;
        }
        break;
    }

EC_CLEANUP:
    if (file)
        fclose(file);
    LOG(log_debug, logtype_afpd, "get_tm_bandsize(\"%s\"): bandsize: %lld", path, bandsize);
    return bandsize;
}

/*!
 * Return number on entries in a directory
 *
 * @param path   (r) path to dir
 * @return           number of entries, -1 on error
 */
static long long int get_tm_bands(const char *path)
{
    EC_INIT;
    long long int count = 0;
    DIR *dir = NULL;
    const struct dirent *entry;

    EC_NULL( dir = opendir(path) );

    while ((entry = readdir(dir)) != NULL)
        count++;
    count -= 2; /* All OSens I'm aware of return "." and "..", so just substract them, avoiding string comparison in loop */
        
EC_CLEANUP:
    if (dir)
        closedir(dir);
    if (ret != 0)
        return -1;
    return count;
}

/*!
 * Calculate used size of a TimeMachine volume
 *
 * This assumes that the volume is used only for TimeMachine.
 *
 * 1) readdir(path of volume)
 * 2) for every element that matches regex "\(.*\)\.sparsebundle$" :
 * 3) parse "\1.sparsebundle/Info.plist" and read the band-size XML key integer value
 * 4) readdir "\1.sparsebundle/bands/" counting files
 * 5) calculate used size as: (file_count - 1) * band-size
 *
 * The result of the calculation is returned in "volume->v_tm_used".
 * "volume->v_appended" gets reset to 0.
 * "volume->v_tm_cachetime" is updated with the current time from time(NULL).
 *
 * "volume->v_tm_used" is cached for TM_USED_CACHETIME seconds and updated by
 * "volume->v_appended". The latter is increased by X every time the client
 * appends X bytes to a file (in fork.c).
 *
 * @param vol     (rw) volume to calculate
 * @return             0 on success, -1 on error
 */
#define TM_USED_CACHETIME 60    /* cache for 60 seconds */
static int get_tm_used(struct vol * restrict vol)
{
    EC_INIT;
    long long int bandsize;
    VolSpace used = 0;
    bstring infoplist = NULL;
    bstring bandsdir = NULL;
    DIR *dir = NULL;
    const struct dirent *entry;
    const char *p;
    long int links;
    time_t now = time(NULL);

    if (vol->v_tm_cachetime
        && ((vol->v_tm_cachetime + TM_USED_CACHETIME) >= now)) {
        if (vol->v_tm_used == -1)
            EC_FAIL;
        vol->v_tm_used += vol->v_appended;
        vol->v_appended = 0;
        LOG(log_debug, logtype_afpd, "getused(\"%s\"): cached: %" PRIu64 " bytes",
            vol->v_path, vol->v_tm_used);
        return 0;
    }

    vol->v_tm_cachetime = now;

    EC_NULL( dir = opendir(vol->v_path) );

    while ((entry = readdir(dir)) != NULL) {
        if (((p = strstr(entry->d_name, "sparsebundle")) != NULL)
            && (strlen(entry->d_name) == (p + strlen("sparsebundle") - entry->d_name))) {

            EC_NULL_LOG( infoplist = bformat("%s/%s/%s", vol->v_path, entry->d_name, "Info.plist") );
            
            if ((bandsize = get_tm_bandsize(cfrombstr(infoplist))) == -1) {
                bdestroy(infoplist);
                continue;
            }

            EC_NULL_LOG( bandsdir = bformat("%s/%s/%s/", vol->v_path, entry->d_name, "bands") );

            if ((links = get_tm_bands(cfrombstr(bandsdir))) == -1) {
                bdestroy(infoplist);
                bdestroy(bandsdir);
                continue;
            }

            used += (links - 1) * bandsize;
            LOG(log_debug, logtype_afpd, "getused(\"%s\"): bands: %" PRIu64 " bytes",
                cfrombstr(bandsdir), used);
        }
    }

    vol->v_tm_used = used;

EC_CLEANUP:
    if (infoplist)
        bdestroy(infoplist);
    if (bandsdir)
        bdestroy(bandsdir);
    if (dir)
        closedir(dir);

    LOG(log_debug, logtype_afpd, "getused(\"%s\"): %" PRIu64 " bytes", vol->v_path, vol->v_tm_used);

    EC_EXIT;
}

static int getvolspace(const AFPObj *obj, struct vol *vol,
                       uint32_t *bfree, uint32_t *btotal,
                       VolSpace *xbfree, VolSpace *xbtotal, uint32_t *bsize)
{
    int         spaceflag, rc;
    uint32_t   maxsize;
#ifndef NO_QUOTA_SUPPORT
    VolSpace    qfree, qtotal;
#endif

    spaceflag = AFPVOL_GVSMASK & vol->v_flags;
    /* report up to 2GB if afp version is < 2.2 (4GB if not) */
    maxsize = (obj->afp_version < 22) ? 0x7fffffffL : 0xffffffffL;

#ifdef AFS
    if ( spaceflag == AFPVOL_NONE || spaceflag == AFPVOL_AFSGVS ) {
        if ( afs_getvolspace( vol, xbfree, xbtotal, bsize ) == AFP_OK ) {
            vol->v_flags = ( ~AFPVOL_GVSMASK & vol->v_flags ) | AFPVOL_AFSGVS;
            goto getvolspace_done;
        }
    }
#endif

    if (( rc = ustatfs_getvolspace( vol, xbfree, xbtotal, bsize)) != AFP_OK ) {
        return( rc );
    }

#ifndef NO_QUOTA_SUPPORT
    if ( spaceflag == AFPVOL_NONE || spaceflag == AFPVOL_UQUOTA ) {
        if ( uquota_getvolspace(obj, vol, &qfree, &qtotal, *bsize ) == AFP_OK ) {
            vol->v_flags = ( ~AFPVOL_GVSMASK & vol->v_flags ) | AFPVOL_UQUOTA;
            *xbfree = MIN(*xbfree, qfree);
            *xbtotal = MIN(*xbtotal, qtotal);
            goto getvolspace_done;
        }
    }
#endif
    vol->v_flags = ( ~AFPVOL_GVSMASK & vol->v_flags ) | AFPVOL_USTATFS;

getvolspace_done:
    if (vol->v_limitsize) {
        if (get_tm_used(vol) != 0)
            return AFPERR_MISC;

        *xbtotal = MIN(*xbtotal, (vol->v_limitsize * 1024 * 1024));
        *xbfree = MIN(*xbfree, *xbtotal < vol->v_tm_used ? 0 : *xbtotal - vol->v_tm_used);

        LOG(log_debug, logtype_afpd,
            "volparams: total: %" PRIu64 ", used: %" PRIu64 ", free: %" PRIu64 " bytes",
            *xbtotal, vol->v_tm_used, *xbfree);
    }

    *bfree = MIN(*xbfree, maxsize);
    *btotal = MIN(*xbtotal, maxsize);
    return( AFP_OK );
}

/* -----------------------
 * set volume creation date
 * avoid duplicate, well at least it tries
 */
static void vol_setdate(uint16_t id, struct adouble *adp, time_t date)
{
    struct vol  *volume;
    struct vol  *vol = getvolumes();

    for ( volume = getvolumes(); volume; volume = volume->v_next ) {
        if (volume->v_vid == id) {
            vol = volume;
        }
        else if ((time_t)(AD_DATE_FROM_UNIX(date)) == volume->v_ctime) {
            date = date+1;
            volume = getvolumes(); /* restart */
        }
    }
    vol->v_ctime = AD_DATE_FROM_UNIX(date);
    ad_setdate(adp, AD_DATE_CREATE | AD_DATE_UNIX, date);
}

/* ----------------------- */
static int getvolparams(const AFPObj *obj, uint16_t bitmap, struct vol *vol, struct stat *st, char *buf, size_t *buflen)
{
    struct adouble  ad;
    int         bit = 0, isad = 1;
    uint32_t       aint;
    u_short     ashort;
    uint32_t       bfree, btotal, bsize;
    VolSpace            xbfree, xbtotal; /* extended bytes */
    char        *data, *nameoff = NULL;
    char                *slash;

    LOG(log_debug, logtype_afpd, "getvolparams: Volume '%s'", vol->v_localname);

    /* courtesy of jallison@whistle.com:
     * For MacOS8.x support we need to create the
     * .Parent file here if it doesn't exist. */

    /* Convert adouble:v2 to adouble:ea on the fly */
    (void)ad_convert(vol->v_path, st, vol, NULL);

    ad_init(&ad, vol);
    if (ad_open(&ad, vol->v_path, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_RDWR | ADFLAGS_CREATE, 0666) != 0 ) {
        isad = 0;
        vol->v_ctime = AD_DATE_FROM_UNIX(st->st_mtime);

    } else if (ad_get_MD_flags( &ad ) & O_CREAT) {
        slash = strrchr( vol->v_path, '/' );
        if(slash)
            slash++;
        else
            slash = vol->v_path;
        if (ad_getentryoff(&ad, ADEID_NAME)) {
            ad_setentrylen( &ad, ADEID_NAME, strlen( slash ));
            memcpy(ad_entry( &ad, ADEID_NAME ), slash,
                   ad_getentrylen( &ad, ADEID_NAME ));
        }
        vol_setdate(vol->v_vid, &ad, st->st_mtime);
        ad_flush(&ad);
    }
    else {
        if (ad_getdate(&ad, AD_DATE_CREATE, &aint) < 0)
            vol->v_ctime = AD_DATE_FROM_UNIX(st->st_mtime);
        else
            vol->v_ctime = aint;
    }

    if (( bitmap & ( (1<<VOLPBIT_BFREE)|(1<<VOLPBIT_BTOTAL) |
                     (1<<VOLPBIT_XBFREE)|(1<<VOLPBIT_XBTOTAL) |
                     (1<<VOLPBIT_BSIZE)) ) != 0 ) {
        if ( getvolspace(obj, vol, &bfree, &btotal, &xbfree, &xbtotal,
                          &bsize) != AFP_OK ) {
            if ( isad ) {
                ad_close( &ad, ADFLAGS_HF );
            }
            return( AFPERR_PARAM );
        }
    }

    data = buf;
    while ( bitmap != 0 ) {
        while (( bitmap & 1 ) == 0 ) {
            bitmap = bitmap>>1;
            bit++;
        }

        switch ( bit ) {
        case VOLPBIT_ATTR :
            ashort = 0;
            /* check for read-only.
             * NOTE: we don't actually set the read-only flag unless
             *       it's passed in that way as it's possible to mount
             *       a read-write filesystem under a read-only one. */
            if ((vol->v_flags & AFPVOL_RO) ||
                ((utime(vol->v_path, NULL) < 0) && (errno == EROFS))) {
                ashort |= VOLPBIT_ATTR_RO;
            }
            /* prior 2.1 only VOLPBIT_ATTR_RO is defined */
            if (obj->afp_version > 20) {
                if (vol->v_cdb != NULL && (vol->v_cdb->flags & CNID_FLAG_PERSISTENT))
                    ashort |= VOLPBIT_ATTR_FILEID;
                ashort |= VOLPBIT_ATTR_CATSEARCH;

                if (obj->afp_version >= 30) {
                    ashort |= VOLPBIT_ATTR_UTF8;
                    if (vol->v_flags & AFPVOL_UNIX_PRIV)
                        ashort |= VOLPBIT_ATTR_UNIXPRIV;
                    if (vol->v_flags & AFPVOL_TM)
                        ashort |= VOLPBIT_ATTR_TM;
                    if (vol->v_flags & AFPVOL_NONETIDS)
                        ashort |= VOLPBIT_ATTR_NONETIDS;
                    if (obj->afp_version >= 32) {
                        if (vol->v_vfs_ea)
                            ashort |= VOLPBIT_ATTR_EXT_ATTRS;
                        if (vol->v_flags & AFPVOL_ACLS)
                            ashort |= VOLPBIT_ATTR_ACLS;
                    }
                }
            }
            ashort = htons(ashort);
            memcpy(data, &ashort, sizeof( ashort ));
            data += sizeof( ashort );
            break;

        case VOLPBIT_SIG :
            ashort = htons( AFPVOLSIG_DEFAULT );
            memcpy(data, &ashort, sizeof( ashort ));
            data += sizeof( ashort );
            break;

        case VOLPBIT_CDATE :
            aint = vol->v_ctime;
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case VOLPBIT_MDATE :
            if ( st->st_mtime > vol->v_mtime ) {
                vol->v_mtime = st->st_mtime;
            }
            aint = AD_DATE_FROM_UNIX(vol->v_mtime);
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case VOLPBIT_BDATE :
            if (!isad ||  (ad_getdate(&ad, AD_DATE_BACKUP, &aint) < 0))
                aint = AD_DATE_START;
            memcpy(data, &aint, sizeof( aint ));
            data += sizeof( aint );
            break;

        case VOLPBIT_VID :
            memcpy(data, &vol->v_vid, sizeof( vol->v_vid ));
            data += sizeof( vol->v_vid );
            break;

        case VOLPBIT_BFREE :
            bfree = htonl( bfree );
            memcpy(data, &bfree, sizeof( bfree ));
            data += sizeof( bfree );
            break;

        case VOLPBIT_BTOTAL :
            btotal = htonl( btotal );
            memcpy(data, &btotal, sizeof( btotal ));
            data += sizeof( btotal );
            break;

#ifndef NO_LARGE_VOL_SUPPORT
        case VOLPBIT_XBFREE :
            xbfree = hton64( xbfree );
            memcpy(data, &xbfree, sizeof( xbfree ));
            data += sizeof( xbfree );
            break;

        case VOLPBIT_XBTOTAL :
            xbtotal = hton64( xbtotal );
            memcpy(data, &xbtotal, sizeof( xbtotal ));
            data += sizeof( xbfree );
            break;
#endif /* ! NO_LARGE_VOL_SUPPORT */

        case VOLPBIT_NAME :
            nameoff = data;
            data += sizeof( uint16_t );
            break;

        case VOLPBIT_BSIZE:  /* block size */
            bsize = htonl(bsize);
            memcpy(data, &bsize, sizeof(bsize));
            data += sizeof(bsize);
            break;

        default :
            if ( isad ) {
                ad_close( &ad, ADFLAGS_HF );
            }
            return( AFPERR_BITMAP );
        }
        bitmap = bitmap>>1;
        bit++;
    }
    if ( nameoff ) {
        ashort = htons( data - buf );
        memcpy(nameoff, &ashort, sizeof( ashort ));
        /* name is always in mac charset */
        aint = ucs2_to_charset( vol->v_maccharset, vol->v_macname, data+1, AFPVOL_MACNAMELEN + 1);
        if ( aint <= 0 ) {
            *buflen = 0;
            return AFPERR_MISC;
        }

        *data++ = aint;
        data += aint;
    }
    if ( isad ) {
        ad_close(&ad, ADFLAGS_HF);
    }
    *buflen = data - buf;
    return( AFP_OK );
}

/* ------------------------- */
static int stat_vol(const AFPObj *obj, uint16_t bitmap, struct vol *vol, char *rbuf, size_t *rbuflen)
{
    struct stat st;
    int     ret;
    size_t  buflen;

    if ( stat( vol->v_path, &st ) < 0 ) {
        *rbuflen = 0;
        return( AFPERR_PARAM );
    }
    /* save the volume device number */
    vol->v_dev = st.st_dev;

    buflen = *rbuflen - sizeof( bitmap );
    if (( ret = getvolparams(obj, bitmap, vol, &st,
                              rbuf + sizeof( bitmap ), &buflen )) != AFP_OK ) {
        *rbuflen = 0;
        return( ret );
    }
    *rbuflen = buflen + sizeof( bitmap );
    bitmap = htons( bitmap );
    memcpy(rbuf, &bitmap, sizeof( bitmap ));
    return( AFP_OK );

}

/* ------------------------------- */
int afp_getsrvrparms(AFPObj *obj, char *ibuf _U_, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct timeval  tv;
    struct stat     st;
    struct vol      *volume;
    char    *data;
    char        *namebuf;
    int         vcnt;
    size_t      len;
    uint32_t    aint;

    load_volumes(obj);

    data = rbuf + 5;
    for ( vcnt = 0, volume = getvolumes(); volume; volume = volume->v_next ) {
        if (!(volume->v_flags & AFPVOL_NOSTAT)) {
            struct maccess ma;

            if ( stat( volume->v_path, &st ) < 0 ) {
                LOG(log_info, logtype_afpd, "afp_getsrvrparms(%s): stat: %s",
                    volume->v_path, strerror(errno) );
                continue;       /* can't access directory */
            }
            if (!S_ISDIR(st.st_mode)) {
                continue;       /* not a dir */
            }
            accessmode(obj, volume, volume->v_path, &ma, NULL, &st);
            if ((ma.ma_user & (AR_UREAD | AR_USEARCH)) != (AR_UREAD | AR_USEARCH)) {
                continue;   /* no r-x access */
            }
        }

        if (utf8_encoding(obj)) {
            len = ucs2_to_charset_allocate(CH_UTF8_MAC, &namebuf, volume->v_u8mname);
        } else {
            len = ucs2_to_charset_allocate(obj->options.maccharset, &namebuf, volume->v_macname);
        }

        if (len == (size_t)-1)
            continue;

        /* set password bit if there's a volume password */
        *data = (volume->v_password) ? AFPSRVR_PASSWD : 0;

        *data++ |= 0; /* UNIX PRIVS BIT ..., OSX doesn't seem to use it, so we don't either */
        *data++ = len;
        memcpy(data, namebuf, len );
        data += len;
        free(namebuf);
        vcnt++;
    }

    *rbuflen = data - rbuf;
    data = rbuf;
    if ( gettimeofday( &tv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "afp_getsrvrparms: gettimeofday: %s", strerror(errno) );
        *rbuflen = 0;
        return AFPERR_PARAM;
    }

    aint = AD_DATE_FROM_UNIX(tv.tv_sec);
    memcpy(data, &aint, sizeof( uint32_t));
    data += sizeof( uint32_t);
    *data = vcnt;
    return( AFP_OK );
}

/* ------------------------- */
static int volume_codepage(AFPObj *obj, struct vol *volume)
{
    struct charset_functions *charset;
    /* Codepages */

    if (!volume->v_volcodepage)
        volume->v_volcodepage = strdup("UTF8");

    if ( (charset_t) -1 == ( volume->v_volcharset = add_charset(volume->v_volcodepage)) ) {
        LOG (log_error, logtype_afpd, "Setting codepage %s as volume codepage failed", volume->v_volcodepage);
        return -1;
    }

    if ( NULL == (charset = find_charset_functions(volume->v_volcodepage)) || charset->flags & CHARSET_ICONV ) {
        LOG (log_warning, logtype_afpd, "WARNING: volume encoding %s is *not* supported by netatalk, expect problems !!!!", volume->v_volcodepage);
    }

    if (!volume->v_maccodepage)
        volume->v_maccodepage = strdup(obj->options.maccodepage);

    if ( (charset_t) -1 == ( volume->v_maccharset = add_charset(volume->v_maccodepage)) ) {
        LOG (log_error, logtype_afpd, "Setting codepage %s as mac codepage failed", volume->v_maccodepage);
        return -1;
    }

    if ( NULL == ( charset = find_charset_functions(volume->v_maccodepage)) || ! (charset->flags & CHARSET_CLIENT) ) {
        LOG (log_error, logtype_afpd, "Fatal error: mac charset %s not supported", volume->v_maccodepage);
        return -1;
    }
    volume->v_kTextEncoding = htonl(charset->kTextEncoding);
    return 0;
}

/* ------------------------- */
static int volume_openDB(const AFPObj *obj, struct vol *volume)
{
    int flags = 0;

    if ((volume->v_flags & AFPVOL_NODEV)) {
        flags |= CNID_FLAG_NODEV;
    }

    LOG(log_debug, logtype_afpd, "CNID server: %s:%s", volume->v_cnidserver, volume->v_cnidport);

    volume->v_cdb = cnid_open(volume->v_path,
                              volume->v_umask,
                              volume->v_cnidscheme,
                              flags,
                              volume->v_cnidserver,
                              volume->v_cnidport);

    if ( ! volume->v_cdb && ! (flags & CNID_FLAG_MEMORY)) {
        /* The first attempt failed and it wasn't yet an attempt to open in-memory */
        LOG(log_error, logtype_afpd, "Can't open volume \"%s\" CNID backend \"%s\" ",
            volume->v_path, volume->v_cnidscheme);
        LOG(log_error, logtype_afpd, "Reopen volume %s using in memory temporary CNID DB.",
            volume->v_path);
        flags |= CNID_FLAG_MEMORY;
        volume->v_cdb = cnid_open (volume->v_path, volume->v_umask, "tdb", flags, NULL, NULL);
#ifdef SERVERTEXT
        /* kill ourself with SIGUSR2 aka msg pending */
        if (volume->v_cdb) {
            setmessage("Something wrong with the volume's CNID DB, using temporary CNID DB instead."
                       "Check server messages for details!");
            kill(getpid(), SIGUSR2);
            /* deactivate cnid caching/storing in AppleDouble files */
        }
#endif
    }

    return (!volume->v_cdb)?-1:0;
}

/*
 * Send list of open volumes to afpd master via IPC
 */
static void server_ipc_volumes(AFPObj *obj)
{
    struct vol *volume, *vols;
    volume = vols = getvolumes();
    bstring openvolnames = bfromcstr("");
    bool firstvol = true;

    while (volume) {
        if (volume->v_flags & AFPVOL_OPEN) {
            if (!firstvol)
                bcatcstr(openvolnames, ", ");
            else
                firstvol = false;
            bcatcstr(openvolnames, volume->v_localname);
        }
        volume = volume->v_next;
    }

    ipc_child_write(obj->ipc_fd, IPC_VOLUMES, blength(openvolnames), bdata(openvolnames));
    bdestroy(openvolnames);
}

/* -------------------------
 * we are the user here
 */
int afp_openvol(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf, size_t *rbuflen)
{
    struct stat st;
    char    *volname;

    struct vol  *volume;
    struct dir  *dir;
    int     len, ret;
    size_t  namelen;
    uint16_t   bitmap;
    char        *vol_uname;
    char        *vol_mname = NULL;
    char        *volname_tmp;

    ibuf += 2;
    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof( bitmap );

    *rbuflen = 0;
    if (( bitmap & (1<<VOLPBIT_VID)) == 0 ) {
        return AFPERR_BITMAP;
    }

    len = (unsigned char)*ibuf++;
    volname = obj->oldtmp;

    if ((volname_tmp = strchr(volname,'+')) != NULL)
        volname = volname_tmp+1;

    if (utf8_encoding(obj)) {
        namelen = convert_string(CH_UTF8_MAC, CH_UCS2, ibuf, len, volname, sizeof(obj->oldtmp));
    } else {
        namelen = convert_string(obj->options.maccharset, CH_UCS2, ibuf, len, volname, sizeof(obj->oldtmp));
    }

    if ( namelen <= 0) {
        return AFPERR_PARAM;
    }

    ibuf += len;
    if ((len + 1) & 1) /* pad to an even boundary */
        ibuf++;

    load_volumes(obj);

    for ( volume = getvolumes(); volume; volume = volume->v_next ) {
        if ( strcasecmp_w( (ucs2_t*) volname, volume->v_name ) == 0 ) {
            break;
        }
    }

    if ( volume == NULL ) {
        return AFPERR_PARAM;
    }

    /* check for a volume password */
    if (volume->v_password && strncmp(ibuf, volume->v_password, VOLPASSLEN)) {
        return AFPERR_ACCESS;
    }

    if (( volume->v_flags & AFPVOL_OPEN  ) ) {
        /* the volume is already open */
        return stat_vol(obj, bitmap, volume, rbuf, rbuflen);
    }

    if (volume->v_root_preexec) {
        if ((ret = afprun(1, volume->v_root_preexec, NULL)) && volume->v_root_preexec_close) {
            LOG(log_error, logtype_afpd, "afp_openvol(%s): root preexec : %d", volume->v_path, ret );
            return AFPERR_MISC;
        }
    }

    if (volume->v_preexec) {
        if ((ret = afprun(0, volume->v_preexec, NULL)) && volume->v_preexec_close) {
            LOG(log_error, logtype_afpd, "afp_openvol(%s): preexec : %d", volume->v_path, ret );
            return AFPERR_MISC;
        }
    }

    if ( stat( volume->v_path, &st ) < 0 ) {
        return AFPERR_PARAM;
    }

    if ( chdir( volume->v_path ) < 0 ) {
        return AFPERR_PARAM;
    }

    if (volume_codepage(obj, volume) < 0) {
        ret = AFPERR_MISC;
        goto openvol_err;
    }

    /* initialize volume variables
     * FIXME file size
     */
    if (utf8_encoding(obj)) {
        volume->max_filename = UTF8FILELEN_EARLY;
    }
    else {
        volume->max_filename = MACFILELEN;
    }

    volume->v_flags |= AFPVOL_OPEN;
    volume->v_cdb = NULL;

    if (utf8_encoding(obj)) {
        len = convert_string_allocate(CH_UCS2, CH_UTF8_MAC, volume->v_u8mname, namelen, &vol_mname);
    } else {
        len = convert_string_allocate(CH_UCS2, obj->options.maccharset, volume->v_macname, namelen, &vol_mname);
    }
    if ( !vol_mname || len <= 0) {
        ret = AFPERR_MISC;
        goto openvol_err;
    }

    if ((vol_uname = strrchr(volume->v_path, '/')) == NULL)
        vol_uname = volume->v_path;
    else if (vol_uname[1] != '\0')
        vol_uname++;

    if ((dir = dir_new(vol_mname,
                       vol_uname,
                       volume,
                       DIRDID_ROOT_PARENT,
                       DIRDID_ROOT,
                       bfromcstr(volume->v_path),
                       &st)
            ) == NULL) {
        free(vol_mname);
        LOG(log_error, logtype_afpd, "afp_openvol(%s): malloc: %s", volume->v_path, strerror(errno) );
        ret = AFPERR_MISC;
        goto openvol_err;
    }
    volume->v_root = dir;
    curdir = dir;

    if (volume_openDB(obj, volume) < 0) {
        LOG(log_error, logtype_afpd, "Fatal error: cannot open CNID or invalid CNID backend for %s: %s",
            volume->v_path, volume->v_cnidscheme);
        ret = AFPERR_MISC;
        goto openvol_err;
    }

    ret  = stat_vol(obj, bitmap, volume, rbuf, rbuflen);

    if (ret == AFP_OK) {
        /*
         * If you mount a volume twice, the second time the trash appears on
         * the desk-top.  That's because the Mac remembers the DID for the
         * trash (even for volumes in different zones, on different servers).
         * Just so this works better, we prime the DID cache with the trash,
         * fixing the trash at DID 17.
         * FIXME (RL): should it be done inside a CNID backend ? (always returning Trash DID when asked) ?
         */
        if ((volume->v_cdb->flags & CNID_FLAG_PERSISTENT)) {

            /* FIXME find db time stamp */
            if (cnid_getstamp(volume->v_cdb, volume->v_stamp, sizeof(volume->v_stamp)) < 0) {
                LOG (log_error, logtype_afpd,
                     "afp_openvol(%s): Fatal error: Unable to get stamp value from CNID backend",
                     volume->v_path);
                ret = AFPERR_MISC;
                goto openvol_err;
            }
        }

        const char *msg;
        if ((msg = atalk_iniparser_getstring(obj->iniconfig, volume->v_configname, "login message",  NULL)) != NULL)
            setmessage(msg);

        free(vol_mname);
        server_ipc_volumes(obj);
        return( AFP_OK );
    }

openvol_err:
    if (volume->v_root) {
        dir_free( volume->v_root );
        volume->v_root = NULL;
    }

    volume->v_flags &= ~AFPVOL_OPEN;
    if (volume->v_cdb != NULL) {
        cnid_close(volume->v_cdb);
        volume->v_cdb = NULL;
    }
    free(vol_mname);
    *rbuflen = 0;
    return ret;
}

void closevol(const AFPObj *obj, struct vol *vol)
{
    if (!vol)
        return;

    vol->v_flags &= ~AFPVOL_OPEN;

    of_closevol(obj, vol);

    dir_free( vol->v_root );
    vol->v_root = NULL;
    if (vol->v_cdb != NULL) {
        cnid_close(vol->v_cdb);
        vol->v_cdb = NULL;
    }

    if (vol->v_postexec) {
        afprun(0, vol->v_postexec, NULL);
    }
    if (vol->v_root_postexec) {
        afprun(1, vol->v_root_postexec, NULL);
    }
}

/* ------------------------- */
void close_all_vol(const AFPObj *obj)
{
    struct vol  *ovol;
    curdir = NULL;
    for ( ovol = getvolumes(); ovol; ovol = ovol->v_next ) {
        if ( (ovol->v_flags & AFPVOL_OPEN) ) {
            ovol->v_flags &= ~AFPVOL_OPEN;
            closevol(obj, ovol);
        }
    }
}

/* ------------------------- */
int afp_closevol(AFPObj *obj, char *ibuf, size_t ibuflen _U_, char *rbuf _U_, size_t *rbuflen)
{
    struct vol  *vol;
    uint16_t   vid;

    *rbuflen = 0;
    ibuf += 2;
    memcpy(&vid, ibuf, sizeof( vid ));
    if (NULL == ( vol = getvolbyvid( vid )) ) {
        return( AFPERR_PARAM );
    }

    (void)chdir("/");
    curdir = NULL;
    closevol(obj, vol);
    server_ipc_volumes(obj);

    return( AFP_OK );
}

/* --------------------------
   poll if a volume is changed by other processes.
   return
   0 no attention msg sent
   1 attention msg sent
   -1 error (socket closed)

   Note: if attention return -1 no packet has been
   sent because the buffer is full, we don't care
   either there's no reader or there's a lot of
   traffic and another pollvoltime will follow
*/
int  pollvoltime(AFPObj *obj)

{
    struct vol       *vol;
    struct timeval   tv;
    struct stat      st;

    if (!(obj->afp_version > 21 && obj->options.flags & OPTION_SERVERNOTIF))
        return 0;

    if ( gettimeofday( &tv, NULL ) < 0 )
        return 0;

    for ( vol = getvolumes(); vol; vol = vol->v_next ) {
        if ( (vol->v_flags & AFPVOL_OPEN)  && vol->v_mtime + 30 < tv.tv_sec) {
            if ( !stat( vol->v_path, &st ) && vol->v_mtime != st.st_mtime ) {
                vol->v_mtime = st.st_mtime;
                if (!obj->attention(obj->dsi, AFPATTN_NOTIFY | AFPATTN_VOLCHANGED))
                    return -1;
                return 1;
            }
        }
    }
    return 0;
}

/* ------------------------- */
void setvoltime(AFPObj *obj, struct vol *vol)
{
    struct timeval  tv;

    if ( gettimeofday( &tv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "setvoltime(%s): gettimeofday: %s", vol->v_path, strerror(errno) );
        return;
    }
    if( utime( vol->v_path, NULL ) < 0 ) {
        /* write of time failed ... probably a read only filesys,
         * where no other users can interfere, so there's no issue here
         */
    }

    /* a little granularity */
    if (vol->v_mtime < tv.tv_sec) {
        vol->v_mtime = tv.tv_sec;
        /* or finder doesn't update free space
         * AFP 3.2 and above clients seem to be ok without so many notification
         */
        if (obj->afp_version < 32 && obj->options.flags & OPTION_SERVERNOTIF) {
            obj->attention(obj->dsi, AFPATTN_NOTIFY | AFPATTN_VOLCHANGED);
        }
    }
}

/* ------------------------- */
int afp_getvolparams(AFPObj *obj, char *ibuf, size_t ibuflen _U_,char *rbuf, size_t *rbuflen)
{
    struct vol  *vol;
    uint16_t   vid, bitmap;

    ibuf += 2;
    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );

    if (NULL == ( vol = getvolbyvid( vid )) ) {
        *rbuflen = 0;
        return( AFPERR_PARAM );
    }

    return stat_vol(obj, bitmap, vol, rbuf, rbuflen);
}

/* ------------------------- */
int afp_setvolparams(AFPObj *obj _U_, char *ibuf, size_t ibuflen _U_, char *rbuf _U_,  size_t *rbuflen)
{
    struct adouble ad;
    struct vol  *vol;
    uint16_t   vid, bitmap;
    uint32_t   aint;

    ibuf += 2;
    *rbuflen = 0;

    memcpy(&vid, ibuf, sizeof( vid ));
    ibuf += sizeof( vid );
    memcpy(&bitmap, ibuf, sizeof( bitmap ));
    bitmap = ntohs( bitmap );
    ibuf += sizeof(bitmap);

    if (( vol = getvolbyvid( vid )) == NULL ) {
        return( AFPERR_PARAM );
    }

    if ((vol->v_flags & AFPVOL_RO))
        return AFPERR_VLOCK;

    /* we can only set the backup date. */
    if (bitmap != (1 << VOLPBIT_BDATE))
        return AFPERR_BITMAP;

    ad_init(&ad, vol);
    if ( ad_open(&ad,  vol->v_path, ADFLAGS_HF | ADFLAGS_DIR | ADFLAGS_RDWR) < 0 ) {
        if (errno == EROFS)
            return AFPERR_VLOCK;

        return AFPERR_ACCESS;
    }

    memcpy(&aint, ibuf, sizeof(aint));
    ad_setdate(&ad, AD_DATE_BACKUP, aint);
    ad_flush(&ad);
    ad_close(&ad, ADFLAGS_HF);
    return( AFP_OK );
}
