#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsd-snprintf.h"
#include "conf.h"
#include "configfile.h"
#include "daapd.h"
#include "db-generic.h"
#include "err.h"
#include "ff-dbstruct.h"
#include "ff-plugins.h"
#include "io.h"
#include "mp3-scanner.h"
#include "plugin.h"
#include "util.h"
#include "webserver.h"

EXPORT char *pi_ws_uri(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);

    if(!pwsc)
        return NULL;

    return ws_uri(pwsc);
}

EXPORT void pi_ws_will_close(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);

    if(!pwsc)
        return;

    ws_should_close(pwsc,1);
}

EXPORT int pi_ws_returnerror(WS_CONNINFO *pwsc, int ecode, char *msg) {
    ASSERT(pwsc);

    if(!pwsc)
        return FALSE;

    return ws_returnerror(pwsc,ecode,msg);
}

EXPORT char *pi_ws_getvar(WS_CONNINFO *pwsc, char *var) {
    ASSERT(pwsc);
    ASSERT(var);

    if((!pwsc) || (!var))
        return NULL;

    return ws_getvar(pwsc,var);
}

EXPORT int pi_ws_writefd(WS_CONNINFO *pwsc, char *fmt, ...) {
    char *out;
    va_list ap;
    int result;

    ASSERT((pwsc) && (fmt));

    if((!pwsc) || (!fmt))
        return FALSE;

    va_start(ap,fmt);
    out = util_vasprintf(fmt,ap);
    va_end(ap);

    result = ws_writefd(pwsc, "%s", out);
    free(out);
    return result;
}

EXPORT int pi_ws_addresponseheader(WS_CONNINFO *pwsc, char *header, char *fmt, ...) {
    char *out;
    va_list ap;
    int result;

    ASSERT(pwsc && header && fmt);

    if((!pwsc) || (!header) || (!fmt))
        return FALSE;

    va_start(ap,fmt);
    out = util_vasprintf(fmt,ap);
    va_end(ap);

    result = ws_addresponseheader(pwsc, header, "%s", out);
    free(out);
    return result;

}

EXPORT void pi_ws_emitheaders(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);

    if(!pwsc)
        return;

    ws_emitheaders(pwsc);
}

EXPORT char *pi_ws_getrequestheader(WS_CONNINFO *pwsc, char *header) {
    ASSERT((pwsc) && (header));

    if((!pwsc) || (!header))
        return NULL;

    return ws_getrequestheader(pwsc, header);
}

EXPORT int pi_ws_writebinary(WS_CONNINFO *pwsc, char *data, int len) {
    ASSERT((pwsc) && (data) && (len));

    if((!pwsc) || (!data) || (!len))
        return 0;

    return ws_writebinary(pwsc, data, len);
}

EXPORT char *pi_ws_gethostname(WS_CONNINFO *pwsc) {
    ASSERT(pwsc);

    if(!pwsc)
        return NULL;

    return ws_hostname(pwsc);
}

EXPORT int pi_ws_matchesrole(WS_CONNINFO *pwsc, char *username,
                             char *password, char *role) {
    ASSERT((pwsc) && (role));

    if((!pwsc) || (!role))
        return FALSE;

    return config_matches_role(pwsc, username, password, role);
}

/* misc helpers */
EXPORT char *pi_server_ver(void) {
    return VERSION;
}

EXPORT int pi_server_name(char *name, int *len) {
    char *servername;

    ASSERT((name) && (len));

    if((!name) || (!len))
        return FALSE;

    servername = conf_get_servername();

    /* FIXME: this is stupid */
    if((servername) && (strlen(servername) < (size_t)len)) {
        strcpy(name,servername);
    } else {
        if((size_t)len > strlen("Firefly Media Server"))
            strcpy(name,"Firefly Media Server");
    }

    free(servername);
    return CONF_E_SUCCESS;
}

EXPORT void pi_log(int level, char *fmt, ...) {
    char *out;
    va_list ap;

    va_start(ap,fmt);
    out=util_vasprintf(fmt,ap);
    va_end(ap);

    DPRINTF(level,L_PLUG,"%s",out);
    free(out);
}

/**
 * check to see if we can transcode
 *
 * @param codec the codec we are trying to serve
 * @returns TRUE if we can transcode, FALSE otherwise
 */
EXPORT int pi_should_transcode(WS_CONNINFO *pwsc, char *codec) {
    return plugin_ssc_should_transcode(pwsc,codec);
}


EXPORT int pi_db_enum_start(char **pe, DB_QUERY *pinfo) {
    DBQUERYINFO *pqi;
    int result;

    pqi = (DBQUERYINFO*)malloc(sizeof(DBQUERYINFO));
    if(!pqi) {
        if(pe) *pe = strdup("Malloc error");
        return DB_E_MALLOC;
    }
    memset(pqi,0,sizeof(DBQUERYINFO));
    pinfo->priv = (void*)pqi;

    if(pinfo->filter) {
        pqi->pt = sp_init();
        if(!sp_parse(pqi->pt,pinfo->filter,pinfo->filter_type)) {
            DPRINTF(E_LOG,L_PLUG,"Ignoring bad query (%s): %s\n",
                    pinfo->filter,sp_get_error(pqi->pt));
            sp_dispose(pqi->pt);
            pqi->pt = NULL;
        }
    }

    if((pinfo->limit) || (pinfo->offset)) {
        pqi->index_low = pinfo->offset;
        pqi->index_high = pinfo->offset + pinfo->limit - 1;
        if(pqi->index_high < pqi->index_low)
            pqi->index_high = 9999999;

        pqi->index_type = indexTypeSub;
    } else {
        pqi->index_type = indexTypeNone;
    }

    pqi->want_count = 1;

    switch(pinfo->query_type) {
    case QUERY_TYPE_PLAYLISTS:
        pqi->query_type = queryTypePlaylists;
        break;
    case QUERY_TYPE_DISTINCT:
        if((strcmp(pinfo->distinct_field,"artist") == 0)) {
            pqi->query_type = queryTypeBrowseArtists;
        } else if((strcmp(pinfo->distinct_field,"genre") == 0)) {
            pqi->query_type = queryTypeBrowseGenres;
        } else if((strcmp(pinfo->distinct_field,"album") == 0)) {
            pqi->query_type = queryTypeBrowseAlbums;
        } else if((strcmp(pinfo->distinct_field,"composer") == 0)) {
            pqi->query_type = queryTypeBrowseComposers;
        } else {
            if(pe) *pe = strdup("Unsupported browse type");
            if(pqi->pt)
                sp_dispose(pqi->pt);
            pqi->pt = NULL;
            return -1; /* not really a db error for this */
        }
        break;
    case QUERY_TYPE_ITEMS:
    default:
        pqi->query_type = queryTypePlaylistItems;
        pqi->correct_order = conf_get_int("scan","correct_order",1);
        break;
    }

    pqi->playlist_id = pinfo->playlist_id;
    result =  db_enum_start(pe, pqi);
    pinfo->totalcount = pqi->specifiedtotalcount;

    return DB_E_SUCCESS;
}

EXPORT int pi_db_enum_fetch_row(char **pe, char ***row, DB_QUERY *pinfo) {
    return db_enum_fetch_row(pe, (PACKED_MP3FILE*)row,
                             (DBQUERYINFO*)pinfo->priv);
}

EXPORT int pi_db_enum_end(char **pe) {
    return db_enum_end(pe);
}

EXPORT int pi_db_enum_restart(char **pe, DB_QUERY *pinfo) {
    DBQUERYINFO *pqi;

    pqi = (DBQUERYINFO*)pinfo->priv;
    return db_enum_reset(pe,pqi);
}

EXPORT void pi_db_enum_dispose(char **pe, DB_QUERY *pinfo) {
    DBQUERYINFO *pqi;

    if(!pinfo)
        return;

    if(pinfo->priv) {
        pqi = (DBQUERYINFO *)pinfo->priv;
        if(pqi->pt) {
            sp_dispose(pqi->pt);
            pqi->pt = NULL;
        }
        free(pqi);
        pinfo->priv = NULL;
    }
}

EXPORT void pi_stream(WS_CONNINFO *pwsc, char *id) {
    int session = 0;
    MP3FILE *pmp3;
    IOHANDLE hfile;
    uint64_t bytes_copied=0;
    uint64_t real_len;
    uint64_t file_len;
    uint64_t offset=0;
    int item;

    /* stream out the song */
    ws_should_close(pwsc,1);

    item = atoi(id);

    if(ws_getrequestheader(pwsc,"range")) {
        offset=(off_t)atol(ws_getrequestheader(pwsc,"range") + 6);
    }

    /* FIXME: error handling */
    pmp3=db_fetch_item(NULL,item);
    if(!pmp3) {
        DPRINTF(E_LOG,L_DAAP|L_WS|L_DB,"Could not find requested item %lu\n",item);
        config_set_status(pwsc,session,NULL);
        ws_returnerror(pwsc,404,"File Not Found");
    } else if (pi_should_transcode(pwsc,pmp3->codectype)) {
        /************************
         * Server side conversion
         ************************/
        config_set_status(pwsc,session,
                          "Transcoding '%s' (id %d)",
                          pmp3->title,pmp3->id);

        DPRINTF(E_WARN,L_WS,
                "Session %d: Streaming file '%s' to %s (offset %ld)\n",
                session,pmp3->fname, ws_hostname(pwsc),(long)offset);

        /* estimate the real length of this thing */
        bytes_copied =  plugin_ssc_transcode(pwsc,pmp3,offset,1);
        if(bytes_copied != -1)
            real_len = bytes_copied;

        config_set_status(pwsc,session,NULL);
        db_dispose_item(pmp3);
    } else {
        /**********************
         * stream file normally
         **********************/
        if(pmp3->data_kind != 0) {
            ws_returnerror(pwsc,500,"Can't stream radio station");
            return;
        }

        hfile = io_new();
        if(!hfile)
            DPRINTF(E_FATAL,L_WS,"Cannot allocate file handle\n");

        if(!io_open(hfile,"file://%U",pmp3->path)) {
            /* FIXME: ws_set_errstr */
            ws_set_err(pwsc,E_WS_NATIVE);
            DPRINTF(E_WARN,L_WS,"Thread %d: Error opening %s: %s\n",
                ws_threadno(pwsc),pmp3->path,io_errstr(hfile));
            ws_returnerror(pwsc,404,"Not found");
            config_set_status(pwsc,session,NULL);
            db_dispose_item(pmp3);
            io_dispose(hfile);
        } else {
            io_size(hfile,&real_len);
            file_len = real_len - offset;

            DPRINTF(E_DBG,L_WS,"Thread %d: Length of file (remaining): %lld\n",
                    ws_threadno(pwsc),file_len);

            // DWB:  fix content-type to correctly reflect data
            // content type (dmap tagged) should only be used on
            // dmap protocol requests, not the actually song data
            if(pmp3->type)
                ws_addresponseheader(pwsc,"Content-Type","audio/%s",pmp3->type);

            ws_addresponseheader(pwsc,"Content-Length","%ld",(long)file_len);

            if((ws_getrequestheader(pwsc,"user-agent")) &&
               (!strncmp(ws_getrequestheader(pwsc,"user-agent"),
                         "Hifidelio",9))) {
                ws_addresponseheader(pwsc,"Connection","Keep-Alive");
                ws_should_close(pwsc,0);
            } else {
                ws_addresponseheader(pwsc,"Connection","Close");
            }

            if(!offset)
                ws_writefd(pwsc,"HTTP/1.1 200 OK\r\n");
            else {
                ws_addresponseheader(pwsc,"Content-Range","bytes %ld-%ld/%ld",
                                     (long)offset,(long)real_len,
                                     (long)real_len+1);
                ws_writefd(pwsc,"HTTP/1.1 206 Partial Content\r\n");
            }

            ws_emitheaders(pwsc);

            config_set_status(pwsc,session,"Streaming '%s' (id %d)",
                              pmp3->title, pmp3->id);
            DPRINTF(E_WARN,L_WS,"Session %d: Streaming file '%s' to %s (offset %d)\n",
                    session,pmp3->fname, ws_hostname(pwsc),(long)offset);

            if(offset) {
                DPRINTF(E_INF,L_WS,"Seeking to offset %ld\n",(long)offset);
                io_setpos(hfile,offset,SEEK_SET);
            }

            if(!ws_copyfile(pwsc,hfile,&bytes_copied)) {
                /* FIXME: Get webserver error string */
                DPRINTF(E_INF,L_WS,"Error copying file to remote...\n");
                ws_should_close(pwsc,1);
            } else {
                DPRINTF(E_INF,L_WS,"Finished streaming file to remote: %lld bytes\n",
                        bytes_copied);
            }

            config_set_status(pwsc,session,NULL);
            io_close(hfile);
            io_dispose(hfile);
            db_dispose_item(pmp3);
        }
    }
    /* update play counts */
    if(bytes_copied  >= (real_len * 80 / 100)) {
        db_playcount_increment(NULL,pmp3->id);
        if(!offset)
            config.stats.songs_served++; /* FIXME: remove stat races */
    }
    //    free(pqi);
}

EXPORT  int pi_db_add_playlist(char **pe, char *name, int type, char *clause,
                               char *path, int index, int *playlistid) {
    return db_add_playlist(pe, name, type, clause, path, index, playlistid);
}

EXPORT int pi_db_add_playlist_item(char **pe, int playlistid, int songid) {
    return db_add_playlist_item(pe, playlistid, songid);
}

EXPORT int pi_db_edit_playlist(char **pe, int id, char *name, char *clause) {
    return db_edit_playlist(pe, id, name, clause);
}

EXPORT int pi_db_delete_playlist(char **pe, int playlistid) {
    return db_delete_playlist(pe, playlistid);
}

EXPORT int pi_db_delete_playlist_item(char **pe, int playlistid, int songid) {
    return db_delete_playlist_item(pe, playlistid, songid);
}

EXPORT int pi_db_revision(void) {
    return db_revision();
}

EXPORT int pi_db_count_items(int what) {
    int count=0;
    char *pe = NULL;

    switch(what) {
    case COUNT_SONGS:
        db_get_song_count(&pe,&count);
        break;
    case COUNT_PLAYLISTS:
        db_get_playlist_count(&pe,&count);
        break;
    }

    if(pe) {
        DPRINTF(E_LOG,L_DB,"Error getting item count: %s\n",pe);
        free(pe);
    }
    return count;
}

EXPORT int pi_db_wait_update(WS_CONNINFO *pwsc) {
    int clientver=1;
    int lastver=0;
    IO_WAITHANDLE hwait;
    uint32_t ms;

    if(ws_getvar(pwsc,"revision-number")) {
        clientver=atoi(ws_getvar(pwsc,"revision-number"));
    }

    /* wait for db_version to be stable for 30 seconds */
    hwait = io_wait_new();
    if(!hwait)
        DPRINTF(E_FATAL,L_MISC,"Can't get wait handle in db_wait_update\n");

    /* FIXME: Move this up into webserver to avoid groping around
     * inside reserved data structures */

    io_wait_add(hwait,pwsc->hclient,IO_WAIT_ERROR);

    while((clientver == db_revision()) ||
          (lastver && (db_revision() != lastver))) {
        lastver = db_revision();

        if(!io_wait(hwait,&ms) && (ms != 0)) {
            /* can't be ready for read, must be error */
            DPRINTF(E_DBG,L_DAAP,"Update session stopped\n");
            io_wait_dispose(hwait);
            return FALSE;
        }
    }

    io_wait_dispose(hwait);

    return TRUE;
}

EXPORT char *pi_conf_alloc_string(char *section, char *key, char *dflt) {
    return conf_alloc_string(section, key, dflt);
}

EXPORT void pi_conf_dispose_string(char *str) {
    free(str);
}

EXPORT int pi_conf_get_int(char *section, char *key, int dflt) {
    return conf_get_int(section, key, dflt);
}

EXPORT void pi_config_set_status(WS_CONNINFO *pwsc, int session, char *fmt, ...) {
    char *out;
    va_list ap;

    ASSERT(fmt);
    if(!fmt)
        return;

    va_start(ap,fmt);
    out = util_vasprintf(fmt,ap);
    va_end(ap);

    config_set_status(pwsc, session, "%s", out);
    free(out);
}

/**
 * allocte an io object
 *
 * @returns NULL on malloc error, IOHANDLE otherwise
 */
EXPORT IOHANDLE pi_io_new(void) {
    return io_new();
}

/**
 * open an io object given a printf-style URI
 *
 * @param io io handle allocated with pi_io_new
 * @param fmt printf-style format string for URI (%U URI-encodes strings)
 * @returns TRUE on success, FALSE otherwise.  use io_err* to get error info
 */
EXPORT int pi_io_open(IOHANDLE io, char *fmt, ...) {
    char uri_copy[4096];
    va_list ap;

    va_start(ap, fmt);
    io_util_vsnprintf(uri_copy, sizeof(uri_copy), fmt, ap);
    va_end(ap);

    return io_open(io, "%s", uri_copy);
}

/**
 * close an open io_object
 *
 * @param io handle to close
 * @returns TRUE on success, FALSE otherwise
 */
EXPORT int pi_io_close(IOHANDLE io) {
    return io_close(io);
}

/**
 * read from an open io handle
 *
 * @param io open handle to read from
 * @param buf buffer to read into
 * @param len length to read, on return: length read
 * @returns TRUE on success, FALSE otherwise, len set with bytes read
 */
EXPORT int pi_io_read(IOHANDLE io, unsigned char *buf, uint32_t *len){
    return io_read(io, buf, len);
}

/**
 * read from an io handle with timeout
 *
 * Returns FALSE on read error or timeout.  Timeout versus
 * read error condition can be determined by ms.  If ms is 0,
 * then a timeout condition occurred
 *
 * @param io open handle to read from
 * @param buf buffer to read into
 * @param len length to read, on return: length read
 * @param ms time to wait (in ms), on return: time left
 * @returns TRUE on success, FALSE on failure (or timeout)
 */
EXPORT int pi_io_read_timeout(IOHANDLE io, unsigned char *buf, uint32_t *len, uint32_t *ms) {
    return io_read_timeout(io, buf, len, ms);
}

/**
 * write a block of data to an open io handle
 *
 * @param io io handle to write to
 * @param buf buffer to write from
 * @param len bytes to write to io handle, on return: bytes written
 * @returns TRUE on success, FALSE otherwise
 */
EXPORT int pi_io_write(IOHANDLE io, unsigned char *buf, uint32_t *len) {
    return io_write(io, buf, len);
}

/**
 * write a printf formatted string to io handle
 *
 * @param io io handle to write to
 * @param fmt printf style format specifier
 * @returns TRUE on success, FALSE otherwise
 */
EXPORT int pi_io_printf(IOHANDLE io, char *fmt, ...) {
    char *out;
    va_list ap;
    int result;

    ASSERT(fmt);
    if(!fmt)
        return FALSE;

    va_start(ap,fmt);
    out = util_vasprintf(fmt,ap);
    va_end(ap);

    result = io_printf(io,"%s",out);
    free(out);

    return result;
}

/**
 * get the (64-bit) size of a file.  Built in URI's use seekability
 * to determine extent of file, so non-seekable io objects (sockets, etc)
 * will return IO_E_BADFN.
 *
 * @param io io handle to get size of
 * @param size returns the 64-bit size of file
 * @returns TRUE on success, FALSE on error (check io_errstr)
 */
EXPORT int pi_io_size(IOHANDLE io, uint64_t *size) {
    return io_size(io, size);
}

/**
 * set the position of file read handle.  There are several limitations
 * to this.  Non seekable handles (sockets, etc) won't seek, NOT
 * EVEN FORWARD!
 *
 * FIXME: seeking on buffered file handle makes explode
 *
 * @param io handle to set position of
 * @param offset how far to move
 * @param whence from where (in lseek style -- SEEK_SET, SEEK_CUR, SEEK_END)
 * @returns TRUE on success, FALSE otherwise (check io_errstr)
 */
EXPORT int pi_io_setpos(IOHANDLE io, uint64_t offset, int whence) {
    return io_setpos(io, offset, whence);
}

/**
 * get current file position in a stream.  Like setpos, this won't work on non-
 * streamable io handles, and it won't (currently) work on buffered file handles.
 * in addition, it might behave strangely various filter drivers (ssl, etc)
 *
 * @param io io handle to get position for
 * @param pos on return, the current file position
 * @returns TRUE on success, FALSE otherwise (see io_errstr)
 */
EXPORT int pi_io_getpos(IOHANDLE io, uint64_t *pos) {
    return io_getpos(io, pos);
}


/**
 * turn on buffering for a file handle.  This really only makes sense
 * when doing readlines.  Note that you can't currently turn of buffered
 * mode, so if doing a mix of buffered and unbuffered io, don't buffer the
 * handle.  Also, once the handle is buffered, setpos and getpos won't work
 * right, and will very likely make Bad Things Happen.  You have been
 * warned.
 *
 * @param io handle to buffer
 * @returns TRUE
 */
EXPORT int pi_io_buffer(IOHANDLE io) {
    return io_buffer(io);
}

/**
 * read a line from the file handle.  If the file is opened with
 * ascii=1, then line ending conversions to/from windows/unix will
 * take place.
 *
 * @param io handle to read line from
 * @param buf buffer to read line into
 * @param len size of buffer, on return: bytes read
 * @returns TRUE on success, FALSE on error (see io_errstr)
 */
EXPORT int pi_io_readline(IOHANDLE io, unsigned char *buf, uint32_t *len) {
    return io_readline(io, buf, len);
}

/**
 * read a line from a file handle with timeout.
 *
 * Errors (including timeout) return FALSE.  Timeout errors
 * can be detected because ms=0
 *
 * @param io handle to read from
 * @param buf buffer to read into
 * @param len size of buffer, on return: bytes read
 * @param ms timeout, in ms, on return: time remaining
 * @returns TRUE on success, FALSE otherwise
 */
EXPORT int pi_io_readline_timeout(IOHANDLE io, unsigned char *buf, uint32_t *len, uint32_t *ms) {
    return io_readline_timeout(io, buf, len, ms);
}

/**
 * get error string of last error
 *
 * @param io handle to get error of
 * @returns error string, does not need to be free'd
 */
EXPORT char* pi_io_errstr(IOHANDLE io) {
    return io_errstr(io);
}

/**
 * get native error code
 *
 * @param io handle to get error code for
 * @returns error code (see io-errors.h)
 */
EXPORT int pi_io_errcode(IOHANDLE io) {
    return io_errcode(io);
}

/**
 * dispose of an io handle, freeing up any internally allocated
 * memory and structs.  This implicitly calls io_close, so
 * you don't *need* to do io_close(hio); io_dispoase(hio);, but I
 * do anyway.  :)
 *
 * @param io handle to dispose of
 */
EXPORT void pi_io_dispose(IOHANDLE io) {
    return io_dispose(io);
}
