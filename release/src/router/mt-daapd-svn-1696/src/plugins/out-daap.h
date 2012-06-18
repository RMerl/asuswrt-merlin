/*
 * $Id: out-daap.h 1629 2007-08-09 06:33:41Z rpedde $
 */

#ifndef _OUT_DAAP_H_
#define _OUT_DAAP_H_

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

#include "ff-plugins.h"

typedef enum {
    // generic meta data
    metaItemId,
    metaItemName,
    metaItemKind,
    metaPersistentId,
    metaContainerItemId,
    metaParentContainerId,

    firstTypeSpecificMetaId,

    // song meta data
    metaSongAlbum = firstTypeSpecificMetaId,
    metaSongArtist,
    metaSongBPM,
    metaSongBitRate,
    metaSongComment,
    metaSongCompilation,
    metaSongComposer,
    metaSongDataKind,
    metaSongDataURL,
    metaSongDateAdded,
    metaSongDateModified,
    metaSongDescription,
    metaSongDisabled,
    metaSongDiscCount,
    metaSongDiscNumber,
    metaSongEqPreset,
    metaSongFormat,
    metaSongGenre,
    metaSongGrouping,
    metaSongRelativeVolume,
    metaSongSampleRate,
    metaSongSize,
    metaSongStartTime,
    metaSongStopTime,
    metaSongTime,
    metaSongTrackCount,
    metaSongTrackNumber,
    metaSongUserRating,
    metaSongYear,

    /* iTunes 4.5 + */
    metaSongCodecType,
    metaSongCodecSubType,
    metaItunesNormVolume,
    metaItmsSongId,
    metaItmsArtistId,
    metaItmsPlaylistId,
    metaItmsComposerId,
    metaItmsGenreId,
    metaItmsStorefrontId,
    metaItunesSmartPlaylist,

    /* iTunes 5.0 + */
    metaSongContentRating,
    metaHasChildContainers,

    /* iTunes 6.0.2+ */
    metaItunesHasVideo,

    /* mt-daapd specific */
    metaMPlaylistSpec,
    metaMPlaylistType
} MetaFieldName_t;

typedef unsigned long long MetaField_t;
typedef struct tag_ws_conninfo WS_CONNINFO;

typedef struct tag_daap_privinfo {
    DB_QUERY dq;
    int uri_count;
    MetaField_t meta;
    int empty_strings;
    struct tag_output_info *output_info;
    int session_id;
    char *uri_sections[10];
    WS_CONNINFO *pwsc;
} PRIVINFO;

#endif /* _OUT_DAAP_H_ */

