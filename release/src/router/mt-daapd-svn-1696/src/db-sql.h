/*
 * $Id: db-sql.h 1574 2007-05-03 00:18:25Z rpedde $
 * sql-specific db implementation
 *
 * Copyright (C) 2005 Ron Pedde (ron@pedde.com)
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DB_SQL_H_
#define _DB_SQL_H_

typedef char** SQL_ROW;

#ifdef HAVE_LIBSQLITE
extern int db_sql_open_sqlite2(char **pe, char *parameters);
#endif
#ifdef HAVE_LIBSQLITE3
extern int db_sql_open_sqlite3(char **pe, char *parameters);
#endif

extern int db_sql_open(char **pe, char *parameters);
extern int db_sql_init(int reload);
extern int db_sql_deinit(void);
extern int db_sql_escape(char *buffer, int *size, char *fmt, ...);
extern int db_sql_add(char **pe, MP3FILE *pmp3, int *id);
extern int db_sql_enum_start(char **pe, DBQUERYINFO *pinfo);


extern int db_sql_enum_fetch_row(char **pe, PACKED_MP3FILE *row, DBQUERYINFO *pinfo);
extern int db_sql_enum_reset(char **pe, DBQUERYINFO *pinfo);
extern int db_sql_enum_end(char **pe);
extern int db_sql_force_rescan(char **pe);
extern int db_sql_start_scan(void);
extern int db_sql_end_song_scan(void);
extern int db_sql_end_scan(void);
extern int db_sql_get_count(char **pe, int *count, CountType_t type);
extern MP3FILE *db_sql_fetch_item(char **pe, int id);
extern MP3FILE *db_sql_fetch_path(char **pe, char *path,int index);
extern M3UFILE *db_sql_fetch_playlist(char **pe, char *path, int index);
extern void db_sql_dispose_item(MP3FILE *pmp3);
extern void db_sql_dispose_playlist(M3UFILE *pm3u);
extern int db_sql_add_playlist(char **pe, char *name, int type, char *clause, char *path, int index, int *playlistid);
extern int db_sql_add_playlist_item(char **pe, int playlistid, int songid);
extern int db_sql_edit_playlist(char **pe, int id, char *name, char *clause);
extern int db_sql_delete_playlist(char **pe, int playlistid);
extern int db_sql_delete_playlist_item(char **pe, int playlistid, int songid);
extern int db_sql_playcount_increment(char **pe, int id);

extern int db_sql_fetch_row(char **pe, SQL_ROW *row, char *fmt, ...);
extern int db_sql_fetch_int(char **pe, int *result, char *fmt, ...);
extern int db_sql_fetch_char(char **pe, char **result, char *fmt, ...);
extern int db_sql_dispose_row(void);

/*
typedef enum {
    songID,
    songPath,
    songFname,
    songTitle,
    songArtist,
    songAlbum,
    songGenre,
    songComment,
    songType,
    songComposer,
    songOrchestra,
    songGrouping,
    songURL,
    songBitrate,
    songSampleRate,
    songLength,
    songFilesize,
    songYear,
    songTrack,
    songTotalTracks,
    songDisc,
    songTotalDiscs,
    songBPM,
    songCompilation,
    songRating,
    songPlayCount,
    songDataKind,
    songItemKind,
    songDescription,
    songTimeAdded,
    songTimeModified,
    songTimePlayed,
    songDBTimestamp,
    songDisabled,
    songSampleCount,
    songForceUpdate,
    songCodecType
} SongField_t;
*/

/*
typedef enum {
    plID,
    plTitle,
    plType,
    plItems,
    plQuery,
    plDBTimestamp,
    plPath
} PlaylistField_t;
*/

#define DB_SQL_EVENT_STARTUP        0
#define DB_SQL_EVENT_SONGSCANSTART  1
#define DB_SQL_EVENT_SONGSCANEND    2
#define DB_SQL_EVENT_PLSCANSTART    3
#define DB_SQL_EVENT_PLSCANEND      4
#define DB_SQL_EVENT_FULLRELOAD     5


#endif /* _DB_SQL_H_ */
