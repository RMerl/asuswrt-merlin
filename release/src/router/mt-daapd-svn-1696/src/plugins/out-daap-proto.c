/*
 * $Id: $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "ff-dbstruct.h"
#include "ff-plugins.h"
#include "out-daap.h"
#include "out-daap-proto.h"

DAAP_ITEMS taglist[] = {
    { 0x05, "miid", "dmap.itemid" },
    { 0x09, "minm", "dmap.itemname" },
    { 0x01, "mikd", "dmap.itemkind" },
    { 0x07, "mper", "dmap.persistentid" },
    { 0x0C, "mcon", "dmap.container" },
    { 0x05, "mcti", "dmap.containeritemid" },
    { 0x05, "mpco", "dmap.parentcontainerid" },
    { 0x05, "mstt", "dmap.status" },
    { 0x09, "msts", "dmap.statusstring" },
    { 0x05, "mimc", "dmap.itemcount" },
    { 0x05, "mctc", "dmap.containercount" },
    { 0x05, "mrco", "dmap.returnedcount" },
    { 0x05, "mtco", "dmap.specifiedtotalcount" },
    { 0x0C, "mlcl", "dmap.listing" },
    { 0x0C, "mlit", "dmap.listingitem" },
    { 0x0C, "mbcl", "dmap.bag" },
    { 0x0C, "mdcl", "dmap.dictionary" },
    { 0x0C, "msrv", "dmap.serverinforesponse" },
    { 0x01, "msau", "dmap.authenticationmethod" },
    { 0x01, "mslr", "dmap.loginrequired" },
    { 0x0B, "mpro", "dmap.protocolversion" },
    { 0x01, "msal", "dmap.supportsautologout" },
    { 0x01, "msup", "dmap.supportsupdate" },
    { 0x01, "mspi", "dmap.supportspersistentids" },
    { 0x01, "msex", "dmap.supportsextensions" },
    { 0x01, "msbr", "dmap.supportsbrowse" },
    { 0x01, "msqy", "dmap.supportsquery" },
    { 0x01, "msix", "dmap.supportsindex" },
    { 0x01, "msrs", "dmap.supportsresolve" },
    { 0x05, "mstm", "dmap.timeoutinterval" },
    { 0x05, "msdc", "dmap.databasescount" },
    { 0x0C, "mlog", "dmap.loginresponse" },
    { 0x05, "mlid", "dmap.sessionid" },
    { 0x0C, "mupd", "dmap.updateresponse" },
    { 0x05, "musr", "dmap.serverrevision" },
    { 0x01, "muty", "dmap.updatetype" },
    { 0x0C, "mudl", "dmap.deletedidlisting" },
    { 0x0C, "mccr", "dmap.contentcodesresponse" },
    { 0x05, "mcnm", "dmap.contentcodesnumber" },
    { 0x09, "mcna", "dmap.contentcodesname" },
    { 0x03, "mcty", "dmap.contentcodestype" },
    { 0x0B, "apro", "daap.protocolversion" },
    { 0x0C, "avdb", "daap.serverdatabases" },
    { 0x0C, "abro", "daap.databasebrowse" },
    { 0x0C, "abal", "daap.browsealbumlisting" },
    { 0x0C, "abar", "daap.browseartistlisting" },
    { 0x0C, "abcp", "daap.browsecomposerlisting" },
    { 0x0C, "abgn", "daap.browsegenrelisting" },
    { 0x0C, "adbs", "daap.databasesongs" },
    { 0x09, "asal", "daap.songalbum" },
    { 0x09, "asar", "daap.songartist" },
    { 0x03, "asbt", "daap.songbeatsperminute" },
    { 0x03, "asbr", "daap.songbitrate" },
    { 0x09, "ascm", "daap.songcomment" },
    { 0x01, "asco", "daap.songcompilation" },
    { 0x09, "ascp", "daap.songcomposer" },
    { 0x0A, "asda", "daap.songdateadded" },
    { 0x0A, "asdm", "daap.songdatemodified" },
    { 0x03, "asdc", "daap.songdisccount" },
    { 0x03, "asdn", "daap.songdiscnumber" },
    { 0x01, "asdb", "daap.songdisabled" },
    { 0x09, "aseq", "daap.songeqpreset" },
    { 0x09, "asfm", "daap.songformat" },
    { 0x09, "asgn", "daap.songgenre" },
    { 0x09, "asdt", "daap.songdescription" },
    { 0x02, "asrv", "daap.songrelativevolume" },
    { 0x05, "assr", "daap.songsamplerate" },
    { 0x05, "assz", "daap.songsize" },
    { 0x05, "asst", "daap.songstarttime" },
    { 0x05, "assp", "daap.songstoptime" },
    { 0x05, "astm", "daap.songtime" },
    { 0x03, "astc", "daap.songtrackcount" },
    { 0x03, "astn", "daap.songtracknumber" },
    { 0x01, "asur", "daap.songuserrating" },
    { 0x03, "asyr", "daap.songyear" },
    { 0x01, "asdk", "daap.songdatakind" },
    { 0x09, "asul", "daap.songdataurl" },
    { 0x0C, "aply", "daap.databaseplaylists" },
    { 0x01, "abpl", "daap.baseplaylist" },
    { 0x0C, "apso", "daap.playlistsongs" },
    { 0x0C, "arsv", "daap.resolve" },
    { 0x0C, "arif", "daap.resolveinfo" },
    { 0x05, "aeNV", "com.apple.itunes.norm-volume" },
    { 0x01, "aeSP", "com.apple.itunes.smart-playlist" },

    /* iTunes 4.5+ */
    { 0x01, "msas", "dmap.authenticationschemes" },
    { 0x05, "ascd", "daap.songcodectype" },
    { 0x05, "ascs", "daap.songcodecsubtype" },
    { 0x09, "agrp", "daap.songgrouping" },
    { 0x05, "aeSV", "com.apple.itunes.music-sharing-version" },
    { 0x05, "aePI", "com.apple.itunes.itms-playlistid" },
    { 0x05, "aeCI", "com.apple.iTunes.itms-composerid" },
    { 0x05, "aeGI", "com.apple.iTunes.itms-genreid" },
    { 0x05, "aeAI", "com.apple.iTunes.itms-artistid" },
    { 0x05, "aeSI", "com.apple.iTunes.itms-songid" },
    { 0x05, "aeSF", "com.apple.iTunes.itms-storefrontid" },

    /* iTunes 5.0+ */
    { 0x01, "ascr", "daap.songcontentrating" },
    { 0x01, "f" "\x8d" "ch", "dmap.haschildcontainers" },

    /* iTunes 6.0.2+ */
    { 0x01, "aeHV", "com.apple.itunes.has-video" },

    /* iTunes 6.0.4+ */
    { 0x05, "msas", "dmap.authenticationschemes" },
    { 0x09, "asct", "daap.songcategory" },
    { 0x09, "ascn", "daap.songcontentdescription" },
    { 0x09, "aslc", "daap.songlongcontentdescription" },
    { 0x09, "asky", "daap.songkeywords" },
    { 0x01, "apsm", "daap.playlistshufflemode" },
    { 0x01, "aprm", "daap.playlistrepeatmode" },
    { 0x01, "aePC", "com.apple.itunes.is-podcast" },
    { 0x01, "aePP", "com.apple.itunes.is-podcast-playlist" },
    { 0x01, "aeMK", "com.apple.itunes.mediakind" },
    { 0x09, "aeSN", "com.apple.itunes.series-name" },
    { 0x09, "aeNN", "com.apple.itunes.network-name" },
    { 0x09, "aeEN", "com.apple.itunes.episode-num-str" },
    { 0x05, "aeES", "com.apple.itunes.episode-sort" },
    { 0x05, "aeSU", "com.apple.itunes.season-num" },

    /* mt-daapd specific */
    { 0x09, "MSPS", "org.mt-daapd.smart-playlist-spec" },
    { 0x01, "MPTY", "org.mt-daapd.playlist-type" },
    { 0x0C, "MAPR", "org.mt-daapd.addplaylist" },
    { 0x0C, "MAPI", "org.mt-daapd.addplaylistitem" },
    { 0x0C, "MDPR", "org.mt-daapd.delplaylist" },
    { 0x0C, "MDPI", "org.mt-daapd.delplaylistitem" },
    { 0x0C, "MEPR", "org.mt-daapd.editplaylist" },

    { 0x00, NULL,   NULL }
};

typedef struct {
    const char* tag;
    MetaFieldName_t bit;
} METAMAP;

/** map the string names specified in the meta= tag to bit numbers */
static METAMAP  db_metamap[] = {
    { "dmap.itemid",                       metaItemId },
    { "dmap.itemname",                     metaItemName },
    { "dmap.itemkind",                     metaItemKind },
    { "dmap.persistentid",                 metaPersistentId },
    { "dmap.containeritemid",              metaContainerItemId },
    { "dmap.parentcontainerid",            metaParentContainerId },
    /* end generics */
    { "daap.songalbum",                    metaSongAlbum },
    { "daap.songartist",                   metaSongArtist },
    { "daap.songbitrate",                  metaSongBitRate },
    { "daap.songbeatsperminute",           metaSongBPM },
    { "daap.songcomment",                  metaSongComment },
    { "daap.songcompilation",              metaSongCompilation },
    { "daap.songcomposer",                 metaSongComposer },
    { "daap.songdatakind",                 metaSongDataKind },
    { "daap.songdataurl",                  metaSongDataURL },
    { "daap.songdateadded",                metaSongDateAdded },
    { "daap.songdatemodified",             metaSongDateModified },
    { "daap.songdescription",              metaSongDescription },
    { "daap.songdisabled",                 metaSongDisabled },
    { "daap.songdisccount",                metaSongDiscCount },
    { "daap.songdiscnumber",               metaSongDiscNumber },
    { "daap.songeqpreset",                 metaSongEqPreset },
    { "daap.songformat",                   metaSongFormat },
    { "daap.songgenre",                    metaSongGenre },
    { "daap.songgrouping",                 metaSongGrouping },
    { "daap.songrelativevolume",           metaSongRelativeVolume },
    { "daap.songsamplerate",               metaSongSampleRate },
    { "daap.songsize",                     metaSongSize },
    { "daap.songstarttime",                metaSongStartTime },
    { "daap.songstoptime",                 metaSongStopTime },
    { "daap.songtime",                     metaSongTime },
    { "daap.songtrackcount",               metaSongTrackCount },
    { "daap.songtracknumber",              metaSongTrackNumber },
    { "daap.songuserrating",               metaSongUserRating },
    { "daap.songyear",                     metaSongYear },

    /* iTunes 4.5+ (forgot exactly when) */
    { "daap.songcodectype",                metaSongCodecType },
    { "daap.songcodecsubtype",             metaSongCodecSubType },
    { "com.apple.itunes.norm-volume",      metaItunesNormVolume },
    { "com.apple.itunes.itms-songid",      metaItmsSongId },
    { "com.apple.itunes.itms-artistid",    metaItmsArtistId },
    { "com.apple.itunes.itms-playlistid",  metaItmsPlaylistId },
    { "com.apple.itunes.itms-composerid",  metaItmsComposerId },
    { "com.apple.itunes.itms-genreid",     metaItmsGenreId },
    { "com.apple.itunes.itms-storefrontid",metaItmsStorefrontId },
    { "com.apple.itunes.smart-playlist",   metaItunesSmartPlaylist },

    /* iTunes 5.0+ */
    { "daap.songcontentrating",            metaSongContentRating },
    { "dmap.haschildcontainers",           metaHasChildContainers },

    /* iTunes 6.0.2+ */
    { "com.apple.itunes.has-video",        metaItunesHasVideo },

    /* mt-daapd specific */
    { "org.mt-daapd.smart-playlist-spec",  metaMPlaylistSpec },
    { "org.mt-daapd.playlist-type",        metaMPlaylistType },

    { 0,                                   0 }
};

int out_daap_session=0;

#define DMAPLEN(a) (((a) && strlen(a)) ? (8+(int)strlen((a))) : \
                                         ((pinfo->empty_strings) ? 8 : 0))
#define EMIT(a) (pinfo->empty_strings ? 1 : ((a) && strlen((a))) ? 1 : 0)

/* Forwards */
int daap_get_size(PRIVINFO *pinfo, char **valarray);
int daap_build_dmap(PRIVINFO *pinfo, char **valarray, unsigned char *presult, int len);

/**
 * encode a string meta request into a MetaField_t
 *
 * \param meta meta string variable from GET request
 */
MetaField_t daap_encode_meta(char *meta) {
    MetaField_t bits = 0;
    char *start;
    char *end;
    METAMAP *m;

    for(start = meta ; *start ; start = end) {
        int     len;

        if(0 == (end = strchr(start, ',')))
            end = start + strlen(start);

        len = (int)(end - start);

        if(*end != 0)
            end++;

        for(m = db_metamap ; m->tag ; ++m)
            if(!strncmp(m->tag, start, len))
                break;

        if(m->tag)
            bits |= (((MetaField_t) 1) << m->bit);
        else
            pi_log(E_WARN,"Unknown meta code: %.*s\n", len, start);
    }

    pi_log(E_DBG, "meta codes: %llu\n", bits);

    return bits;
}

/**
 * see if a specific metafield was requested
 *
 * \param meta encoded list of requested metafields
 * \param fieldNo field to test for
 */
int daap_wantsmeta(MetaField_t meta, MetaFieldName_t fieldNo) {
    return 0 != (meta & (((MetaField_t) 1) << fieldNo));
}

/**
 * add a character type to a dmap block (type 0x01)
 *
 * \param where where to serialize the dmap info
 * \tag what four byte tag
 * \value what character value
 */
int dmap_add_char(unsigned char *where, char *tag, char value) {
    /* tag */
    memcpy(where,tag,4);

    /* len */
    where[4]=where[5]=where[6]=0;
    where[7]=1;

    /* value */
    where[8] = value;
    return 9;
}

/**
 * add a short type to a dmap block (type 0x03)
 *
 * \param where where to serialize the dmap info
 * \tag what four byte tag
 * \value what character value
 */
int dmap_add_short(unsigned char *where, char *tag, short value) {
    /* tag */
    memcpy(where,tag,4);

    /* len */
    where[4]=where[5]=where[6]=0;
    where[7]=2;

    /* value */
    where[8] = (value >> 8) & 0xFF;
    where[9] = value & 0xFF;
    return 10;
}


/**
 * add an int type to a dmap block (type 0x05)
 *
 * \param where where to serialize the dmap info
 * \tag what four byte tag
 * \value what character value
 */

int dmap_add_int(unsigned char *where, char *tag, int value) {
    /* tag */
    memcpy(where,tag,4);
    /* len */
    where[4]=where[5]=where[6]=0;
    where[7]=4;

    /* value */
    where[8] = (value >> 24) & 0xFF;
    where[9] = (value >> 16) & 0xFF;
    where[10] = (value >> 8) & 0xFF;
    where[11] = value & 0xFF;

    return 12;
}

int dmap_add_long(unsigned char *where, char *tag, uint64_t value) {
    uint32_t v_hi;
    uint32_t v_lo;

    /* tag */
    memcpy(where,tag,4);
    /* len */
    where[4]=where[5]=where[6]=0;
    where[7]=8;

    v_hi = (uint32_t)((value >> 32) & 0xFFFFFFFF);
    v_lo = (uint32_t)(value & 0xFFFFFFFF);

    /* value */
    where[8] = (v_hi >> 24) & 0xFF;
    where[9] = (v_hi >> 16) & 0xFF;
    where[10] = (v_hi >> 8) & 0xFF;
    where[11] = v_hi & 0xFF;

    where[12] = (v_lo >> 24) & 0xFF;
    where[13] = (v_lo >> 16) & 0xFF;
    where[14] = (v_lo >> 8) & 0xFF;
    where[15] = v_lo & 0xFF;

    return 16;
}


/**
 * add a string type to a dmap block (type 0x09)
 *
 * \param where where to serialize the dmap info
 * \tag what four byte tag
 * \value what character value
 */

int dmap_add_string(unsigned char *where, char *tag, char *value) {
    int len=0;

    if(value)
        len = (int)strlen(value);

    /* tag */
    memcpy(where,tag,4);

    /* length */
    where[4]=(len >> 24) & 0xFF;
    where[5]=(len >> 16) & 0xFF;
    where[6]=(len >> 8) & 0xFF;
    where[7]=len & 0xFF;

    if(len)
        strncpy((char*)where+8,value,len);
    return 8 + len;
}

/**
 * add a literal chunk of data to a dmap block
 *
 * \param where where to serialize the dmap info
 * \param tag what four byte tag
 * \param value what to put there
 * \param size how much data to cram in there
 */
int dmap_add_literal(unsigned char *where, char *tag,
                        char *value, int size) {
    /* tag */
    memcpy(where,tag,4);

    /* length */
    where[4]=(size >> 24) & 0xFF;
    where[5]=(size >> 16) & 0xFF;
    where[6]=(size >> 8) & 0xFF;
    where[7]=size & 0xFF;

    memcpy(where+8,value,size);
    return 8+size;
}


/**
 * add a container type to a dmap block (type 0x0C)
 *
 * \param where where to serialize the dmap info
 * \tag what four byte tag
 * \value what character value
 */

int dmap_add_container(unsigned char *where, char *tag, int size) {
    int len=size;

    /* tag */
    memcpy(where,tag,4);

    /* length */
    where[4]=(len >> 24) & 0xFF;
    where[5]=(len >> 16) & 0xFF;
    where[6]=(len >> 8) & 0xFF;
    where[7]=len & 0xFF;

    return 8;
}

/**
 * Get the next available session id.
 * This is vulnerable to races, but we don't track sessions,
 * so there really isn't a point anyway.
 *
 * @returns duh... the next available session id
 */
int daap_get_next_session(void) {
    int session;

    session=++out_daap_session;

    return session;
}

/**
 * find the size of the response by walking through the query and
 * sizing it
 *
 * @returns DB_E_SUCCESS on success, error code otherwise
 */
int daap_enum_size(char **pe, PRIVINFO *pinfo, int *count, int *total_size) {
    int err;
    int record_size;
    char **row;

    pi_log(E_DBG,"Enumerating size\n");

    *count=0;
    *total_size = 0;

    while((!(err=pi_db_enum_fetch_row(pe,&row,&pinfo->dq))) && (row)) {
        if((record_size = daap_get_size(pinfo,row))) {
            *total_size += record_size;
            *count = *count + 1;
        }
    }

    if(err) {
        pi_db_enum_end(NULL);
        pi_db_enum_dispose(NULL,&pinfo->dq);
        return err;
    }

    err=pi_db_enum_restart(pe, &pinfo->dq);

    pi_log(E_DBG,"Got size: %d\n",*total_size);
    return err;
}

/**
 * fetch the next record from the enum
 */
int daap_enum_fetch(char **pe, PRIVINFO *pinfo, int *size, unsigned char **pdmap) {
    int err;
    int result_size=0;
    unsigned char *presult;
    char **row;

    err=pi_db_enum_fetch_row(pe, &row, &pinfo->dq);
    if(err) {
        pi_db_enum_end(NULL);
        pi_db_enum_dispose(NULL,&pinfo->dq);
        return err;
    }

    if(row) {
        result_size = daap_get_size(pinfo,row);
        if(result_size) {
            presult = (unsigned char*)malloc(result_size);
            if(!presult) {
                pi_log(E_FATAL,"Malloc error\n");
            }

            daap_build_dmap(pinfo,row,presult,result_size);
            *pdmap=presult;
            *size = result_size;
        }
    } else {
        *size = 0;
    }

    return 0;
}

/**
 * get the size of the generated dmap, given a specific meta
 */
int daap_get_size(PRIVINFO *pinfo, char **valarray) {
    int size;
    int transcode;

    switch(pinfo->dq.query_type) {
    case QUERY_TYPE_DISTINCT: /* simple 'mlit' entry */
        return valarray[0] ? (8 + (int) strlen(valarray[0])) : 0;
    case QUERY_TYPE_PLAYLISTS:
        size = 8;   /* mlit */
        size += 12; /* mimc - you get it whether you want it or not */
        if(daap_wantsmeta(pinfo->meta, metaItemId))
            size += 12; /* miid */
        if(daap_wantsmeta(pinfo->meta, metaItunesSmartPlaylist)) {
            if(valarray[PL_TYPE] && (atoi(valarray[PL_TYPE])==1) &&
               (atoi(valarray[PL_ID]) != 1))
                size += 9;  /* aeSP */
        }

        if(atoi(valarray[PL_ID]) == 1) {
            size += 9; /* abpl */
        }

        if(daap_wantsmeta(pinfo->meta, metaItemName))
            size += (8 + (int) strlen(valarray[PL_TITLE])); /* minm */
        if(valarray[PL_TYPE] && (atoi(valarray[PL_TYPE])==1) &&
           daap_wantsmeta(pinfo->meta, metaMPlaylistSpec))
            size += (8 + (int) strlen(valarray[PL_QUERY])); /* MSPS */
        if(daap_wantsmeta(pinfo->meta, metaMPlaylistType))
            size += 9; /* MPTY */
        return size;
        break;
    case QUERY_TYPE_ITEMS:
        /* see if this is going to be transcoded */
        transcode = pi_should_transcode(pinfo->pwsc,valarray[SG_CODECTYPE]);

        /* Items that get changed by transcode:
         *
         * type:         item  8: changes to 'wav'
         * description:  item 29: changes to 'wav audio file'
         * bitrate:      item 15: guestimated, based on item 15, samplerate
         *
         * probably file size should change as well, but currently doesn't
         */

        size = 8; /* mlit */
        if(daap_wantsmeta(pinfo->meta, metaItemKind))
            /* mikd */
            size += 9;
        if(daap_wantsmeta(pinfo->meta, metaSongDataKind))
            /* asdk */
            size += 9;
        if(daap_wantsmeta(pinfo->meta, metaSongDataURL))
            /* asul */
            size += DMAPLEN(valarray[SG_URL]);
        if(daap_wantsmeta(pinfo->meta, metaSongAlbum))
            /* asal */
            size += DMAPLEN(valarray[SG_ALBUM]);
        if(daap_wantsmeta(pinfo->meta, metaSongArtist))
            /* asar */
            size += DMAPLEN(valarray[SG_ARTIST]);
        if(daap_wantsmeta(pinfo->meta, metaSongBPM))
            /* asbt */
            size += 10;
        if(daap_wantsmeta(pinfo->meta, metaSongBitRate)) {
            /* asbr */
            if(transcode) {
                if(valarray[SG_SAMPLERATE] && atoi(valarray[SG_SAMPLERATE])) {
                    size += 10;
                }
            } else {
                if(valarray[SG_BITRATE] && atoi(valarray[SG_BITRATE])) {
                    size += 10;
                }
            }
        }
        if(daap_wantsmeta(pinfo->meta, metaSongComment))
            /* ascm */
            size += DMAPLEN(valarray[SG_COMMENT]);
        if(valarray[SG_COMPILATION] && atoi(valarray[SG_COMPILATION]) &&
           daap_wantsmeta(pinfo->meta,metaSongCompilation))
            /* asco */
            size += 9;
        if(daap_wantsmeta(pinfo->meta, metaSongComposer))
            /* ascp */
            size += DMAPLEN(valarray[SG_COMPOSER]);
        if(daap_wantsmeta(pinfo->meta, metaSongGrouping))
            /* agrp */
            size += DMAPLEN(valarray[SG_GROUPING]);
        if(valarray[SG_TIME_ADDED] && atoi(valarray[SG_TIME_ADDED]) &&
           daap_wantsmeta(pinfo->meta, metaSongDateAdded))
            /* asda */
            size += 12;
        if(valarray[SG_TIME_MODIFIED] && atoi(valarray[SG_TIME_MODIFIED]) &&
           daap_wantsmeta(pinfo->meta,metaSongDateModified))
            /* asdm */
            size += 12;
        if(daap_wantsmeta(pinfo->meta, metaSongDiscCount))
            /* asdc */
            size += 10;
        if(daap_wantsmeta(pinfo->meta, metaSongDiscNumber))
            /* asdn */
            size += 10;
        if(daap_wantsmeta(pinfo->meta, metaSongGenre))
            /* asgn */
            size += DMAPLEN(valarray[SG_GENRE]);
        if(daap_wantsmeta(pinfo->meta,metaItemId))
            /* miid */
            size += 12;
        if(daap_wantsmeta(pinfo->meta,metaSongFormat)) {
            /* asfm */
            if(transcode) {
                size += 11;   /* 'wav' */
            } else {
                size += DMAPLEN(valarray[SG_TYPE]);
            }
        }
        if(daap_wantsmeta(pinfo->meta,metaSongDescription)) {
            /* asdt */
            if(transcode) {
                size += 22;  /* 'wav audio file' */
            } else {
                size += DMAPLEN(valarray[SG_DESCRIPTION]);
            }
        }
        if(daap_wantsmeta(pinfo->meta,metaItemName))
            /* minm */
            size += DMAPLEN(valarray[SG_TITLE]);
        if(valarray[SG_DISABLED] && atoi(valarray[SG_DISABLED]) &&
           daap_wantsmeta(pinfo->meta,metaSongDisabled))
            /* asdb */
            size += 9;
        if(valarray[SG_SAMPLERATE] && atoi(valarray[SG_SAMPLERATE]) &&
           daap_wantsmeta(pinfo->meta,metaSongSampleRate))
            /* assr */
            size += 12;
        if(valarray[SG_FILE_SIZE] && atoi(valarray[SG_FILE_SIZE]) &&
           daap_wantsmeta(pinfo->meta,metaSongSize))
            /* assz */
            size += 12;

        /* In the old daap code, we always returned 0 for asst and assp
         * (song start time, song stop time).  I don't know if this
         * is required, so I'm going to disabled it
         */

        if(valarray[SG_SONG_LENGTH] && atoi(valarray[SG_SONG_LENGTH]) &&
           daap_wantsmeta(pinfo->meta, metaSongTime))
            /* astm */
            size += 12;
        if(valarray[SG_TOTAL_TRACKS] && atoi(valarray[SG_TOTAL_TRACKS]) &&
           daap_wantsmeta(pinfo->meta, metaSongTrackCount))
            /* astc */
            size += 10;
        if(valarray[SG_TRACK] && atoi(valarray[SG_TRACK]) &&
           daap_wantsmeta(pinfo->meta, metaSongTrackNumber))
            /* astn */
            size += 10;
        if(daap_wantsmeta(pinfo->meta, metaSongUserRating))
            /* asur */
            size += 9;
        if(valarray[SG_YEAR] && atoi(valarray[SG_YEAR]) &&
           daap_wantsmeta(pinfo->meta, metaSongYear))
            /* asyr */
            size += 10;
        if(daap_wantsmeta(pinfo->meta, metaContainerItemId))
            /* mcti */
            size += 12;
        /* FIXME:  This is not right - doesn't have to be 4 */
        if((valarray[SG_CODECTYPE]) && (strlen(valarray[SG_CODECTYPE])==4) &&
           daap_wantsmeta(pinfo->meta,metaSongCodecType))
            /* ascd */
            size += 12;
        if(daap_wantsmeta(pinfo->meta,metaSongContentRating))
            /* ascr */
            size += 9;
        if(daap_wantsmeta(pinfo->meta,metaItunesHasVideo))
            /* aeHV */
            size += 9;

        return size;
        break;

    default:
        pi_log(E_LOG,"Unknown query type: %d\n",(int)pinfo->dq.query_type);
        return 0;
    }
    return 0;
}

int daap_build_dmap(PRIVINFO *pinfo, char **valarray, unsigned char *presult, int len) {
    unsigned char *current = presult;
    int transcode;
    int samplerate=0;

    switch(pinfo->dq.query_type) {
    case QUERY_TYPE_DISTINCT:
        return dmap_add_string(current,"mlit",valarray[0]);
    case QUERY_TYPE_PLAYLISTS:
        /* do I want to include the mlit? */
        current += dmap_add_container(current,"mlit",len - 8);
        if(daap_wantsmeta(pinfo->meta,metaItemId))
            current += dmap_add_int(current,"miid",atoi(valarray[PL_ID]));
        current += dmap_add_int(current,"mimc",atoi(valarray[PL_ITEMS]));
        if(daap_wantsmeta(pinfo->meta,metaItunesSmartPlaylist)) {
            if(valarray[PL_TYPE] && (atoi(valarray[PL_TYPE]) == 1) &&
               (atoi(valarray[PL_ID]) != 1))
                current += dmap_add_char(current,"aeSP",1);
        }
        if(atoi(valarray[PL_ID]) == 1) {
            current += dmap_add_char(current,"abpl",1);
        }

        if(daap_wantsmeta(pinfo->meta,metaItemName))
            current += dmap_add_string(current,"minm",valarray[PL_TITLE]);
        if((valarray[PL_TYPE]) && (atoi(valarray[PL_TYPE])==1) &&
           daap_wantsmeta(pinfo->meta, metaMPlaylistSpec))
            current += dmap_add_string(current,"MSPS",valarray[PL_QUERY]);
        if(daap_wantsmeta(pinfo->meta, metaMPlaylistType))
            current += dmap_add_char(current,"MPTY",atoi(valarray[PL_TYPE]));
        break;
    case QUERY_TYPE_ITEMS:
        /* see if this is going to be transcoded */
        transcode = pi_should_transcode(pinfo->pwsc,valarray[SG_CODECTYPE]);

        /* Items that get changed by transcode:
         *
         * type:         item  8: changes to 'wav'
         * description:  item 29: changes to 'wav audio file'
         * bitrate:      item 15: guestimated, but doesn't change file size
         *
         * probably file size should change as well, but currently doesn't
         */

        current += dmap_add_container(current,"mlit",len-8);
        if(daap_wantsmeta(pinfo->meta, metaItemKind))
            current += dmap_add_char(current,"mikd",
                                        (char)atoi(valarray[SG_ITEM_KIND]));
        if(daap_wantsmeta(pinfo->meta, metaSongDataKind))
            current += dmap_add_char(current,"asdk",
                                        (char)atoi(valarray[SG_DATA_KIND]));
        if(EMIT(valarray[13]) && daap_wantsmeta(pinfo->meta, metaSongDataURL))
            current += dmap_add_string(current,"asul",valarray[SG_URL]);
        if(EMIT(valarray[5]) && daap_wantsmeta(pinfo->meta, metaSongAlbum))
            current += dmap_add_string(current,"asal",valarray[SG_ALBUM]);
        if(EMIT(valarray[4]) && daap_wantsmeta(pinfo->meta, metaSongArtist))
            current += dmap_add_string(current,"asar",valarray[SG_ARTIST]);
        if(daap_wantsmeta(pinfo->meta, metaSongBPM))
            current += dmap_add_short(current,"asbt",
                                         (short)atoi(valarray[SG_BPM]));
        if(valarray[SG_BITRATE] && atoi(valarray[SG_BITRATE]) &&
           daap_wantsmeta(pinfo->meta, metaSongBitRate)) {
            if(transcode) {
                if(valarray[SG_SAMPLERATE])
                    samplerate=atoi(valarray[SG_SAMPLERATE]);
                if(samplerate) {
                    current += dmap_add_short(current,"asbr",
                                                 (short)(samplerate / 250 * 8));
                }
            } else {
                current += dmap_add_short(current,"asbr",
                                             (short)atoi(valarray[SG_BITRATE]));
            }
        }
        if(EMIT(valarray[SG_COMMENT]) &&
           daap_wantsmeta(pinfo->meta, metaSongComment))
            current += dmap_add_string(current,"ascm",valarray[SG_COMMENT]);

        if(valarray[SG_COMPILATION] && atoi(valarray[SG_COMPILATION]) &&
           daap_wantsmeta(pinfo->meta,metaSongCompilation))
            current += dmap_add_char(current,"asco",
                                        (char)atoi(valarray[SG_COMPILATION]));

        if(EMIT(valarray[SG_COMPOSER]) &&
           daap_wantsmeta(pinfo->meta, metaSongComposer))
            current += dmap_add_string(current,"ascp",
                                          valarray[SG_COMPOSER]);

        if(EMIT(valarray[SG_GROUPING]) &&
           daap_wantsmeta(pinfo->meta, metaSongGrouping))
            current += dmap_add_string(current,"agrp",
                                          valarray[SG_GROUPING]);

        if(valarray[SG_TIME_ADDED] && atoi(valarray[SG_TIME_ADDED]) &&
           daap_wantsmeta(pinfo->meta, metaSongDateAdded))
            current += dmap_add_int(current,"asda",
                                       (int)atoi(valarray[SG_TIME_ADDED]));

        if(valarray[SG_TIME_MODIFIED] && atoi(valarray[SG_TIME_MODIFIED]) &&
           daap_wantsmeta(pinfo->meta,metaSongDateModified))
            current += dmap_add_int(current,"asdm",
                                       (int)atoi(valarray[SG_TIME_MODIFIED]));

        if(daap_wantsmeta(pinfo->meta, metaSongDiscCount))
            current += dmap_add_short(current,"asdc",
                                         (short)atoi(valarray[SG_TOTAL_DISCS]));
        if(daap_wantsmeta(pinfo->meta, metaSongDiscNumber))
            current += dmap_add_short(current,"asdn",
                                         (short)atoi(valarray[SG_DISC]));

        if(EMIT(valarray[SG_GENRE]) &&
           daap_wantsmeta(pinfo->meta, metaSongGenre))
            current += dmap_add_string(current,"asgn",valarray[SG_GENRE]);

        if(daap_wantsmeta(pinfo->meta,metaItemId))
            current += dmap_add_int(current,"miid",
                                       (int)atoi(valarray[SG_ID]));

        if(EMIT(valarray[SG_TYPE]) &&
           daap_wantsmeta(pinfo->meta,metaSongFormat)) {
            if(transcode) {
                current += dmap_add_string(current,"asfm","wav");
            } else {
                current += dmap_add_string(current,"asfm",
                                              valarray[SG_TYPE]);
            }
        }

        if(EMIT(valarray[SG_DESCRIPTION]) &&
           daap_wantsmeta(pinfo->meta,metaSongDescription)) {
            if(transcode) {
                current += dmap_add_string(current,"asdt","wav audio file");
            } else {
                current += dmap_add_string(current,"asdt",
                                              valarray[SG_DESCRIPTION]);
            }
        }
        if(EMIT(valarray[SG_TITLE]) &&
           daap_wantsmeta(pinfo->meta,metaItemName))
            current += dmap_add_string(current,"minm",valarray[SG_TITLE]);

        if(valarray[SG_DISABLED] && atoi(valarray[SG_DISABLED]) &&
           daap_wantsmeta(pinfo->meta,metaSongDisabled))
            current += dmap_add_char(current,"asdb",
                                        (char)atoi(valarray[SG_DISABLED]));

        if(valarray[SG_SAMPLERATE] && atoi(valarray[SG_SAMPLERATE]) &&
           daap_wantsmeta(pinfo->meta,metaSongSampleRate))
            current += dmap_add_int(current,"assr",
                                       atoi(valarray[SG_SAMPLERATE]));

        if(valarray[SG_FILE_SIZE] && atoi(valarray[SG_FILE_SIZE]) &&
           daap_wantsmeta(pinfo->meta,metaSongSize))
            current += dmap_add_int(current,"assz",
                                       atoi(valarray[SG_FILE_SIZE]));

        if(valarray[SG_SONG_LENGTH] && atoi(valarray[SG_SONG_LENGTH]) &&
           daap_wantsmeta(pinfo->meta, metaSongTime))
            current += dmap_add_int(current,"astm",
                                       atoi(valarray[SG_SONG_LENGTH]));

        if(valarray[SG_TOTAL_TRACKS] && atoi(valarray[SG_TOTAL_TRACKS]) &&
           daap_wantsmeta(pinfo->meta, metaSongTrackCount))
            current += dmap_add_short(current,"astc",
                                         (short)atoi(valarray[SG_TOTAL_TRACKS]));

        if(valarray[SG_TRACK] && atoi(valarray[SG_TRACK]) &&
           daap_wantsmeta(pinfo->meta, metaSongTrackNumber))
            current += dmap_add_short(current,"astn",
                                         (short)atoi(valarray[SG_TRACK]));

        if(daap_wantsmeta(pinfo->meta, metaSongUserRating))
            current += dmap_add_char(current,"asur",
                                        (char)atoi(valarray[SG_RATING]));

        if(valarray[SG_YEAR] && atoi(valarray[SG_YEAR]) &&
           daap_wantsmeta(pinfo->meta, metaSongYear))
            current += dmap_add_short(current,"asyr",
                                         (short)atoi(valarray[SG_YEAR]));

        if((valarray[SG_CODECTYPE]) && (strlen(valarray[SG_CODECTYPE]) == 4) &&
           daap_wantsmeta(pinfo->meta,metaSongCodecType))
            current += dmap_add_literal(current,"ascd",
                                           valarray[SG_CODECTYPE],4);
        if(daap_wantsmeta(pinfo->meta, metaContainerItemId))
            current += dmap_add_int(current,"mcti",atoi(valarray[SG_ID]));

        if(daap_wantsmeta(pinfo->meta, metaItunesHasVideo))
            current += dmap_add_char(current,"aeHV",
                                        atoi(valarray[SG_HAS_VIDEO]));

        if(daap_wantsmeta(pinfo->meta, metaSongContentRating))
                current += dmap_add_char(current,"ascr",
                                            atoi(valarray[SG_CONTENTRATING]));
        return 0;
        break;

    default:
        pi_log(E_LOG,"Unknown query type: %d\n",(int)pinfo->dq.query_type);
        return 0;
    }
    return 0;
}

