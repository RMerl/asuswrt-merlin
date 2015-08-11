/*
 * $Id: $
 *
 * Database structure and fields
 */

#ifndef _FF_DBSTRUCT_H_
#define _FF_DBSTRUCT_H_

#define PL_ID               0
#define PL_TITLE            1
#define PL_TYPE             2
#define PL_ITEMS            3
#define PL_QUERY            4
#define PL_DB_TIMESTAMP     5
#define PL_PATH             6
#define PL_IDX              7

#define SG_ID               0
#define SG_PATH             1
#define SG_FNAME            2
#define SG_TITLE            3
#define SG_ARTIST           4
#define SG_ALBUM            5
#define SG_GENRE            6
#define SG_COMMENT          7
#define SG_TYPE             8
#define SG_COMPOSER         9
#define SG_ORCHESTRA       10
#define SG_CONDUCTOR       11
#define SG_GROUPING        12
#define SG_URL             13
#define SG_BITRATE         14
#define SG_SAMPLERATE      15
#define SG_SONG_LENGTH     16
#define SG_FILE_SIZE       17
#define SG_YEAR            18
#define SG_TRACK           19
#define SG_TOTAL_TRACKS    20
#define SG_DISC            21
#define SG_TOTAL_DISCS     22
#define SG_BPM             23
#define SG_COMPILATION     24
#define SG_RATING          25
#define SG_PLAY_COUNT      26
#define SG_DATA_KIND       27
#define SG_ITEM_KIND       28
#define SG_DESCRIPTION     29
#define SG_TIME_ADDED      30
#define SG_TIME_MODIFIED   31
#define SG_TIME_PLAYED     32
#define SG_DB_TIMESTAMP    33
#define SG_DISABLED        34
#define SG_SAMPLE_COUNT    35
#define SG_FORCE_UPDATE    36
#define SG_CODECTYPE       37
#define SG_IDX             38
#define SG_HAS_VIDEO       39
#define SG_CONTENTRATING   40
#define SG_BITS_PER_SAMPLE 41
#define SG_ALBUM_ARTIST    42

/* Packed and unpacked formats */
typedef struct tag_mp3file {
    char *path;
    uint32_t index;
    char *fname;
    char *title;     /* TIT2 */
    char *artist;    /* TPE1 */
    char *album;     /* TALB */
    char *genre;     /* TCON */
    char *comment;   /* COMM */
    char *type;
    char *composer;  /* TCOM */
    char *orchestra; /* TPE2 */
    char *conductor; /* TPE3 */
    char *grouping;  /* TIT1 */
    char *url;       /* daap.songdataurl (asul) */

    uint32_t bitrate;
    uint32_t samplerate;
    uint32_t song_length;
    uint64_t file_size; /* ?? */
    uint32_t year;        /* TDRC */

    uint32_t track;       /* TRCK */
    uint32_t total_tracks;

    uint32_t disc;        /* TPOS */
    uint32_t total_discs;

    uint32_t time_added; /* really should be a time_t */
    uint32_t time_modified;
    uint32_t time_played;

    uint32_t play_count;
    uint32_t rating;
    uint32_t db_timestamp;

    uint32_t disabled;
    uint32_t bpm;         /* TBPM */

    uint32_t got_id3;
    uint32_t id;

    char *description;  /* long file type */
    char *codectype;          /* song.codectype */

    uint32_t item_kind;              /* song or movie */
    uint32_t data_kind;              /* dmap.datakind (asdk) */
    uint32_t force_update;
    uint64_t sample_count;
    char compilation;

    /* iTunes 5+ */
    uint32_t contentrating;

    /* iTunes 6.0.2 */
    uint32_t has_video;
    uint32_t bits_per_sample;

    char *album_artist;
} MP3FILE;

typedef struct tag_m3ufile {
    uint32_t id;          /**< integer id (miid) */
    char *title;          /**< playlist name as displayed in iTunes (minm) */
    uint32_t type;        /**< see PL_ types */
    uint32_t items;       /**< number of items (mimc) */
    char *query;          /**< where clause if type 1 (MSPS) */
    uint32_t db_timestamp;/**< time last updated */
    char *path;           /**< path of underlying playlist (if type 2) */
    uint32_t index;       /**< index of playlist for paths with multiple playlists */
} M3UFILE;

typedef struct tag_packed_m3ufile {
    char *id;
    char *title;
    char *type;
    char *items;
    char *query;
    char *db_timestamp;
    char *path;
    char *index;
} PACKED_M3UFILE;

typedef struct tag_packed_mp3file {
    char *id;
    char *path;
    char *fname;
    char *title;
    char *artist;
    char *album;
    char *genre;
    char *comment;
    char *type;
    char *composer;
    char *orchestra;
    char *conductor;
    char *grouping;
    char *url;
    char *bitrate;
    char *samplerate;
    char *song_length;
    char *file_size;
    char *year;
    char *track;
    char *total_tracks;
    char *disc;
    char *total_discs;
    char *bpm;
    char *compilation;
    char *rating;
    char *play_count;
    char *data_kind;
    char *item_kind;
    char *description;
    char *time_added;
    char *time_modified;
    char *time_played;
    char *db_timestamp;
    char *disabled;
    char *sample_count;
    char *force_update;
    char *codectype;
    char *idx;
    char *has_video;
    char *contentrating;
    char *bits_per_sample;
    char *album_artist;
} PACKED_MP3FILE;

#define PL_STATICWEB  0
#define PL_SMART      1
#define PL_STATICFILE 2
#define PL_STATICXML  3

#endif /* _FF_DBSTRUCT_H_ */
