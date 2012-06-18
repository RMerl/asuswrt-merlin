/*
 * $Id: daap.c,v 1.1 2009-06-30 02:31:08 steven Exp $
 * Build daap structs for replies
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>

#include "configfile.h"
#include "db-memory.h"
#include "daap-proto.h"
#include "daap.h"
#include "err.h"
#include "daapd.h"

#include "query.h"

typedef struct tag_daap_items {
    int type;
    char *tag;
    char *description;
} DAAP_ITEMS;

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
	{ 0x00, NULL,   NULL }
};

#define OFFSET_OF(__type, __field)	((size_t) (&((__type*) 0)->__field))

static query_field_t	song_fields[] = {
    { qft_string,	"dmap.itemname",	OFFSET_OF(MP3FILE, title) },
    { qft_i32,		"dmap.itemid",		OFFSET_OF(MP3FILE, id) },
    { qft_string,	"daap.songalbum",	OFFSET_OF(MP3FILE, album) },
    { qft_string,	"daap.songartist",	OFFSET_OF(MP3FILE, artist) },
    { qft_i32,		"daap.songbitrate",	OFFSET_OF(MP3FILE, bitrate) },
    { qft_string,	"daap.songcomment",	OFFSET_OF(MP3FILE, comment) },
    { qft_i32,  	"daap.songcompilation",	OFFSET_OF(MP3FILE, compilation) },
    { qft_string,	"daap.songcomposer",	OFFSET_OF(MP3FILE, composer) },
    { qft_i32,  	"daap.songdatakind",    OFFSET_OF(MP3FILE, data_kind) },
    { qft_string,	"daap.songdataurl",	OFFSET_OF(MP3FILE, url) },
    { qft_i32,		"daap.songdateadded",	OFFSET_OF(MP3FILE, time_added) },
    { qft_i32,		"daap.songdatemodified",OFFSET_OF(MP3FILE, time_modified) },
    { qft_string,	"daap.songdescription",	OFFSET_OF(MP3FILE, description) },
    { qft_i32,		"daap.songdisccount",	OFFSET_OF(MP3FILE, total_discs) },
    { qft_i32,		"daap.songdiscnumber",	OFFSET_OF(MP3FILE, disc) },
    { qft_string,	"daap.songformat",	OFFSET_OF(MP3FILE, type) },
    { qft_string,	"daap.songgenre",	OFFSET_OF(MP3FILE, genre) },
    { qft_i32,		"daap.songsamplerate",	OFFSET_OF(MP3FILE, samplerate) },
    { qft_i32,		"daap.songsize",	OFFSET_OF(MP3FILE, file_size) },
    //    { qft_i32_const,	"daap.songstarttime",	0 },
    { qft_i32,		"daap.songstoptime",	OFFSET_OF(MP3FILE, song_length)  },
    { qft_i32,		"daap.songtime",	OFFSET_OF(MP3FILE, song_length) },
    { qft_i32,		"daap.songtrackcount",	OFFSET_OF(MP3FILE, total_tracks) },
    { qft_i32,		"daap.songtracknumber",	OFFSET_OF(MP3FILE, track) },
    { qft_i32,		"daap.songyear",	OFFSET_OF(MP3FILE, year) },
    { 0 }
};

/* Forwards */

int daap_add_mdcl(DAAP_BLOCK *root, char *tag, char *name, short int number) {
    DAAP_BLOCK *mdcl;
    int g=1;

    mdcl=daap_add_empty(root,"mdcl");
    if(mdcl) {
	g=(int)daap_add_string(mdcl,"mcnm",tag);
	g = g && daap_add_string(mdcl,"mcna",name);
	g = g && daap_add_short(mdcl,"mcty",number);
    }

    return (mdcl ? g : 0);
}

/*
 * daap_response_content_codes
 *
 * handle the daap block for the /content-codes URI
 *
 * This might more easily be done by just emitting a binary
 * of the content-codes from iTunes, since this really
 * isn't dynamic
 */

DAAP_BLOCK *daap_response_content_codes(void) {
    DAAP_BLOCK *root;
    DAAP_ITEMS *current=taglist;
    int g=1;
    
    DPRINTF(E_DBG,L_DAAP,"Preparing to get content codes\n");

    root=daap_add_empty(NULL,"mccr");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);

	while(current->type) {
	    g = g && daap_add_mdcl(root,current->tag,current->description,
				   current->type);
	    current++;
	}
    }

    if(!g) {
	daap_free(root);
	return NULL;
    }

    return root;
}


/*
 * daap_response_login
 *
 * handle the daap block for the /login URI
 */

DAAP_BLOCK *daap_response_login(char *hostname) {
    DAAP_BLOCK *root;
    int g=1;
    int session=0;
    
    DPRINTF(E_DBG,L_DAAP,"Preparing to send login response\n");

    root=daap_add_empty(NULL,"mlog");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	session=config_get_next_session();
	g = g && daap_add_int(root,"mlid",session);
    }

    if(!g) {
	daap_free(root);
	return NULL;
    }

    DPRINTF(E_LOG,L_DAAP,"%s logging in as session %d\n",hostname,session);

    return root;
}

/*
 * daap_response_songlist
 *
 * handle the daap block for the /databases/x/items URI
 */

// fields requestable with meta=... these are really used as bit
// numbers in a long long, but are defined this way to simplify
// eventual implementation on platforms without long long support
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
    metaSongBPM,		/* beats per minute */
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
    metaSongYear
} MetaFieldName_t;

// structure mapping meta= tag names to bit numbers
typedef struct
{
    const char*		tag;
    MetaFieldName_t	bit;
} MetaDataMap;

// the dmap based tags, defined psuedo separately because they're also
// needed for DPAP, not that that's at all relevant here
#define	INCLUDE_GENERIC_META_IDS				\
    { "dmap.itemid",		metaItemId },			\
    { "dmap.itemname",		metaItemName },			\
    { "dmap.itemkind",		metaItemKind },			\
    { "dmap.persistentid",	metaPersistentId },		\
    { "dmap.containeritemid",	metaContainerItemId },		\
    { "dmap.parentcontainerid",	metaParentContainerId }

// map the string names specified in the meta= tag to bit numbers
static MetaDataMap	gSongMetaDataMap[] = {
    INCLUDE_GENERIC_META_IDS,
    { "daap.songalbum",		metaSongAlbum },
    { "daap.songartist",	metaSongArtist },
    { "daap.songbitrate",	metaSongBitRate },
    { "daap.songbeatsperminute",metaSongBPM },
    { "daap.songcomment",	metaSongComment },
    { "daap.songcompilation",	metaSongCompilation },
    { "daap.songcomposer",	metaSongComposer },
    { "daap.songdatakind",	metaSongDataKind },
    { "daap.songdataurl",	metaSongDataURL },
    { "daap.songdateadded",	metaSongDateAdded },
    { "daap.songdatemodified",	metaSongDateModified },
    { "daap.songdescription",	metaSongDescription },
    { "daap.songdisabled",	metaSongDisabled },
    { "daap.songdisccount",	metaSongDiscCount },
    { "daap.songdiscnumber",	metaSongDiscNumber },
    { "daap.songeqpreset",	metaSongEqPreset },
    { "daap.songformat",	metaSongFormat },
    { "daap.songgenre",		metaSongGenre },
    { "daap.songgrouping",	metaSongGrouping },
    { "daap.songrelativevolume",metaSongRelativeVolume },
    { "daap.songsamplerate",	metaSongSampleRate },
    { "daap.songsize",		metaSongSize },
    { "daap.songstarttime",	metaSongStartTime },
    { "daap.songstoptime",	metaSongStopTime },
    { "daap.songtime",		metaSongTime },
    { "daap.songtrackcount",	metaSongTrackCount },
    { "daap.songtracknumber",	metaSongTrackNumber },
    { "daap.songuserrating",	metaSongUserRating },
    { "daap.songyear",		metaSongYear },
    { 0,			0 }
};

typedef unsigned long long	MetaField_t;

// turn the meta= parameter into a bitfield representing the requested
// fields.  The format is actually meta=<tag>[,<tag>...] where <tag>
// is any of the strings in the table above
MetaField_t encodeMetaRequest(char* meta, MetaDataMap* map)
{
    MetaField_t		bits = 0;
    char* start;
    char* end;
    MetaDataMap* m;

    for(start = meta ; *start ; start = end)
    {
	int	len;

	if(0 == (end = strchr(start, ',')))
	    end = start + strlen(start);

	len = end - start;

	if(*end != 0)
	    end++;

	for(m = map ; m->tag ; ++m)
	    if(!strncmp(m->tag, start, len))
		break;

	if(m->tag)
	    bits |= (((MetaField_t) 1) << m->bit);
	else
	    DPRINTF(E_WARN,L_DAAP,"Unknown meta code: %.*s\n", len, start);
    }

    DPRINTF(E_DBG, L_DAAP, "meta codes: %llu\n", bits);

    return bits;
}

int wantsMeta(MetaField_t meta, MetaFieldName_t fieldNo)
{
    return 0 != (meta & (((MetaField_t) 1) << fieldNo));
}

DAAP_BLOCK *daap_response_songlist(char* metaStr, char* query) {
    DAAP_BLOCK *root;
    int g=1;
    DAAP_BLOCK *mlcl;
    DAAP_BLOCK *mlit;
    ENUMHANDLE henum;
    MP3FILE *current;
    MetaField_t meta;

    query_node_t*	filter = 0;
    int			songs = 0;

    DPRINTF(E_DBG,L_DAAP,"enter daap_response_songlist\n");

    // if the meta tag is specified, encode it, if it's not specified
    // we're given the latitude to select our own subset, for
    // simplicity we just include everything.
    if(0 == metaStr)
	meta = (MetaField_t) -1ll;
    else
	meta = encodeMetaRequest(metaStr, gSongMetaDataMap);

    if(0 != query) {
	filter = query_build(query, song_fields);
	DPRINTF(E_INF,L_DAAP|L_QRY,"query: %s\n", query);
	if(err_debuglevel >= E_INF) /* this is broken */
	    query_dump(stderr,filter, 0);
    }

    DPRINTF(E_DBG,L_DAAP|L_DB,"Preparing to send db items\n");

    henum=db_enum_begin();
    if((!henum) && (db_get_song_count())) {
	DPRINTF(E_DBG,L_DAAP|L_DB,"Can't get enum handle - exiting daap_response_songlist\n");
	return NULL;
    }

    root=daap_add_empty(NULL,"adbs");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	g = g && daap_add_char(root,"muty",0);
	g = g && daap_add_int(root,"mtco",0);
	g = g && daap_add_int(root,"mrco",0);

	mlcl=daap_add_empty(root,"mlcl");

	if(mlcl) {
	    while(g && (current=db_enum(&henum))) {
		if(filter == 0 || query_test(filter, current))
		{
		    DPRINTF(E_DBG,L_DAAP|L_DB,"Got entry for %s\n",current->fname);
		    // song entry generation extracted for usage with
		    // playlists as well
		    g = 0 != daap_add_song_entry(mlcl, current, meta);
		    songs++;
		}
	    }
	} else g=0;
    }

    db_enum_end(henum);

    if(filter != 0)
	query_free(filter);

    if(!g) {
	DPRINTF(E_DBG,L_DAAP|L_DB,"Error enumerating db - exiting daap_response_songlist\n");
	daap_free(root);
	return NULL;
    }

    DPRINTF(E_DBG,L_DAAP|L_DB,"Successfully enumerated database - %d items\n",songs);

    daap_set_int(root, "mtco", songs);
    daap_set_int(root, "mrco", songs);

    DPRINTF(E_DBG,L_DAAP,"Exiting daap_response_songlist\n");
    return root;
}


//
// extracted song entry generation used by both database item lists
// and play list item lists
//
DAAP_BLOCK* daap_add_song_entry(DAAP_BLOCK* mlcl, MP3FILE* song, MetaField_t meta)
{
    DAAP_BLOCK* mlit;
    int g = 1;

    mlit=daap_add_empty(mlcl,"mlit");
    if(mlit) {
	if(wantsMeta(meta, metaItemKind))
	    g = g && daap_add_char(mlit,"mikd",song->item_kind); /* audio */

	if(wantsMeta(meta, metaSongDataKind))
	    g = g && daap_add_char(mlit,"asdk",song->data_kind); /* local file */

	if(wantsMeta(meta, metaSongDataURL))
	    g = g && daap_add_string(mlit,"asul",song->url);

	if(song->album && (wantsMeta(meta, metaSongAlbum)))
	    g = g && daap_add_string(mlit,"asal",song->album);

	if(song->artist && wantsMeta(meta, metaSongArtist))
	    g = g && daap_add_string(mlit,"asar",song->artist);

	if(song->bpm && (wantsMeta(meta, metaSongBPM)))
	    g = g && daap_add_short(mlit,"asbt",song->bpm); /* bpm */

	if(song->bitrate && (wantsMeta(meta, metaSongBitRate)))
	    g = g && daap_add_short(mlit,"asbr",song->bitrate); /* bitrate!! */

	if(song->comment && (wantsMeta(meta, metaSongComment)))
	    g = g && daap_add_string(mlit,"ascm",song->comment); /* comment */

        if(song->compilation && (wantsMeta(meta, metaSongCompilation)))
            g = g && daap_add_char(mlit,"asco",song->compilation); /* compilation */
		    
	if(song->composer && (wantsMeta(meta, metaSongComposer)))
	    g = g && daap_add_string(mlit,"ascp",song->composer); /* composer */

	if(song->grouping && (wantsMeta(meta, metaSongGrouping)))
	    g = g && daap_add_string(mlit,"agrp",song->grouping); /* grouping */

	if(song->time_added && (wantsMeta(meta, metaSongDateAdded)))
	    g = g && daap_add_int(mlit,"asda",song->time_added); /* added */

	if(song->time_modified && (wantsMeta(meta, metaSongDateModified)))
	    g = g && daap_add_int(mlit,"asdm",song->time_modified); /* modified */

	if(song->total_discs && (wantsMeta(meta, metaSongDiscCount)))
	    /* # of discs */
	    g = g && daap_add_short(mlit,"asdc",song->total_discs);

	if(song->disc && (wantsMeta(meta, metaSongDiscNumber)))
	    /* disc number */
	    g = g && daap_add_short(mlit,"asdn",song->disc);

	// asdk must be early in the item, moved to the top
	// g = g && daap_add_char(mlit,"asdk",0); /* song datakind? */
	// aseq - null string!

	if(song->genre && (wantsMeta(meta, metaSongGenre)))
	    g = g && daap_add_string(mlit,"asgn",song->genre); /* genre */

	if(wantsMeta(meta, metaItemId))
	    g = g && daap_add_int(mlit,"miid",song->id); /* id */
		
	if(wantsMeta(meta, metaPersistentId)) 
	    g = g && daap_add_long(mlit,"mper",0,song->id);

	/* these quite go hand in hand */
	if(wantsMeta(meta, metaSongFormat))
	    g = g && daap_add_string(mlit,"asfm",song->type); /* song format */

	if(wantsMeta(meta, metaSongDescription))
	    g = g && daap_add_string(mlit, "asdt",song->description);

	if(wantsMeta(meta, metaItemName))
	    g = g && daap_add_string(mlit,"minm",song->title); /* descr */

	// mper (long)
	// g = g && daap_add_char(mlit,"asdb",0); /* disabled */
	// g = g && daap_add_char(mlit,"asrv",0); /* rel vol */
	if(song->samplerate && (wantsMeta(meta, metaSongSampleRate)))
	    g = g && daap_add_int(mlit,"assr",song->samplerate); /* samp rate */
		    
	if(song->file_size && (wantsMeta(meta, metaSongSize)))
	    g = g && daap_add_int(mlit,"assz",song->file_size); /* Size! */
		    
	if(wantsMeta(meta, metaSongStartTime))
	    g = g && daap_add_int(mlit,"asst",0); /* song start time? */
	if(wantsMeta(meta, metaSongStopTime))
	    g = g && daap_add_int(mlit,"assp",0); /* song stop time */

	if(song->song_length && (wantsMeta(meta, metaSongTime)))
	    g = g && daap_add_int(mlit,"astm",song->song_length); /* song time */

	if(song->total_tracks && (wantsMeta(meta, metaSongTrackCount)))
	    g = g && daap_add_short(mlit,"astc",song->total_tracks); /* track count */
		    
	if(song->track && (wantsMeta(meta, metaSongTrackNumber)))
	    g = g && daap_add_short(mlit,"astn",song->track); /* track number */

	// g = g && daap_add_char(mlit,"asur",3); /* rating */
	if(song->year && (wantsMeta(meta, metaSongYear)))
	    g = g && daap_add_short(mlit,"asyr",song->year);
    }

    if(g == 0)
    {
	daap_free(mlit);
	mlit = 0;
    }
	
    return mlit;
}

/*
 * daap_response_update
 *
 * handle the daap block for the /update URI
 */

DAAP_BLOCK *daap_response_update(int fd, int clientver) {
    DAAP_BLOCK *root;
    int g=1;
    fd_set rset;
    struct timeval tv;
    int result;

    DPRINTF(E_DBG,L_DAAP,"Preparing to send update response\n");

    while(clientver == db_version()) {
	FD_ZERO(&rset);
	FD_SET(fd,&rset);

	tv.tv_sec=30;
	tv.tv_usec=0;

	result=select(fd+1,&rset,NULL,NULL,&tv);
	if(FD_ISSET(fd,&rset)) {
	    /* can't be ready for read, must be error */
	    DPRINTF(E_DBG,L_DAAP,"Socket closed?\n");
	    
	    return NULL;
	}
    }

    root=daap_add_empty(NULL,"mupd");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	/* theoretically, this would go up if the db changes? */
	g = g && daap_add_int(root,"musr",db_version());
    }

    if(!g) {
	daap_free(root);
	return NULL;
    }

    return root;
}


/*
 * daap_response_playlists
 *
 * handle the daap block for the /databases/containers URI
 */
DAAP_BLOCK *daap_response_playlists(char *name) {
    DAAP_BLOCK *root=NULL;
    DAAP_BLOCK *mlcl=NULL;
    DAAP_BLOCK *mlit=NULL;
    int g=1;
    int playlistid;
    ENUMHANDLE henum;

    DPRINTF(E_DBG,L_DAAP,"Preparing to send playlists\n");
    
    root=daap_add_empty(NULL,"aply");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	g = g && daap_add_char(root,"muty",0); 
	g = g && daap_add_int(root,"mtco",1 + db_get_playlist_count());
	g = g && daap_add_int(root,"mrco",1 + db_get_playlist_count());
	mlcl=daap_add_empty(root,"mlcl");
	if(mlcl) {
	    mlit=daap_add_empty(mlcl,"mlit");
	    if(mlit) {
		g = g && daap_add_int(mlit,"miid",0x1);
		g = g && daap_add_long(mlit,"mper",0,1);
		g = g && daap_add_string(mlit,"minm",name);
		g = g && daap_add_int(mlit,"mimc",db_get_song_count());
	    }

	    g = g && mlit;

	    /* add the rest of the playlists */
	    henum=db_playlist_enum_begin();
	    while(henum) {
		playlistid=db_playlist_enum(&henum);
		DPRINTF(E_DBG,L_DAAP|L_PL,"Returning playlist %d\n",playlistid);
		DPRINTF(E_DBG,L_DAAP|L_PL,"  -- Songs: %d\n",
			db_get_playlist_entry_count(playlistid));
		DPRINTF(E_DBG,L_DAAP|L_PL,"  -- Smart: %s\n",
			db_get_playlist_is_smart(playlistid) ?
			"Yes" : "No");
		mlit=daap_add_empty(mlcl,"mlit");
		if(mlit) {
		    g = g && daap_add_int(mlit,"miid",playlistid);
		    g = g && daap_add_long(mlit,"mper",0,playlistid);
		    g = g && daap_add_string(mlit,"minm",db_get_playlist_name(playlistid));
		    g = g && daap_add_int(mlit,"mimc",db_get_playlist_entry_count(playlistid));
		    if(db_get_playlist_is_smart(playlistid)) {
			g = g && daap_add_char(mlit,"aeSP",0x1);
		    }
		}
		g = g && mlit;
	    }
	    db_playlist_enum_end(henum);
	}

    }

    g = g && mlcl;

    if(!g) {
	DPRINTF(E_INF,L_DAAP,"Memory problem.  Bailing\n");
	daap_free(root);
	return NULL;
    }

    return root;
}

/*
 * daap_response_dbinfo
 *
 * handle the daap block for the /databases URI
 */

DAAP_BLOCK *daap_response_dbinfo(char *name) {
    DAAP_BLOCK *root=NULL;
    DAAP_BLOCK *mlcl=NULL;
    DAAP_BLOCK *mlit=NULL;
    int g=1;

    DPRINTF(E_DBG,L_DAAP|L_DB,"Preparing to send db info\n");
    
    root=daap_add_empty(NULL,"avdb");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	g = g && daap_add_char(root,"muty",0); 
	g = g && daap_add_int(root,"mtco",1);
	g = g && daap_add_int(root,"mrco",1);
	mlcl=daap_add_empty(root,"mlcl");
	if(mlcl) {
	    mlit=daap_add_empty(mlcl,"mlit");
	    if(mlit) {
		g = g && daap_add_int(mlit,"miid",1);
		g = g && daap_add_long(mlit,"mper",0,1);
		g = g && daap_add_string(mlit,"minm",name);
		g = g && daap_add_int(mlit,"mimc",db_get_song_count()); /* songs */
		g = g && daap_add_int(mlit,"mctc",1 + db_get_playlist_count()); /* playlists */
	    }
	}
    }

    g = g && mlcl && mlit;

    if(!g) {
	DPRINTF(E_INF,L_DAAP,"Memory problem.  Bailing\n");
	daap_free(root);
	return NULL;
    }

    DPRINTF(E_DBG,L_DAAP|L_DB,"Sent db info... %d songs, %d playlists\n",db_get_song_count(),
	    db_get_playlist_count());

    return root;
}

/*
 * daap_response_server_info
 *
 * handle the daap block for the /server-info URI
 */
DAAP_BLOCK *daap_response_server_info(char *name, char *client_version) {
    DAAP_BLOCK *root;
    int g=1;
    int mpro = 2 << 16;
    int apro = 3 << 16;

    DPRINTF(E_DBG,L_DAAP,"Preparing to send server-info for client ver %s\n",client_version);

    root=daap_add_empty(NULL,"msrv");

    if(root) {
	if((client_version) && (!strcmp(client_version,"1.0"))) {
	    mpro = 1 << 16;
	    apro = 1 << 16;
	}

	if((client_version) && (!strcmp(client_version,"2.0"))) {
	    mpro = 1 << 16;
	    apro = 2 << 16;
	}

	g = (int)daap_add_int(root,"mstt",200); /* result */
	g = g && daap_add_int(root,"mpro",mpro); /* dmap proto ? */
	g = g && daap_add_int(root,"apro",apro); /* daap protocol */

	g = g && daap_add_string(root,"minm",name); /* server name */

#if 0
	/* DWB: login isn't actually required since the session id
	   isn't recorded, and isn't actually used for anything */
	/* logon is always required, even if a password isn't */
	g = g && daap_add_char(root,"mslr",1);
#endif

	/* authentication method is 0 for nothing, 1 for name and
	   password, 2 for password only */
	g = g && daap_add_char(root,"msau", config.readpassword != NULL ? 2 : 0);

	/* actual time out seems faster then 30 minutes */
	g = g && daap_add_int(root,"mstm",1800); /* timeout  - iTunes=1800 */

	/* presence of most of the support* variables indicates
	   support, the actual value is required to be zero, I've
	   commented out the ones I don't believe are actually
	   supported */
	g = g && daap_add_char(root,"msex",0); /* extensions */
	g = g && daap_add_char(root,"msix",0); /* indexing? */

	g = g && daap_add_char(root,"msbr",0); /* browsing */
	g = g && daap_add_char(root,"msqy",0); /* queries */

	g = g && daap_add_char(root,"msup",0); /* update */

#if 0
	g = g && daap_add_char(root,"mspi",0); /* persistant ids */
	g = g && daap_add_char(root,"msal",0); /* autologout */
	g = g && daap_add_char(root,"msrs",0); /* resolve?  req. persist id */
#endif
        g = g && daap_add_int(root,"msdc",1); /* database count */
    }

    if(!g) {
	daap_free(root);
	return NULL;
    }

    return root;
}


/* 
 * daap_response_playlist_items
 *
 * given a playlist number, return the items on the playlist
 */
DAAP_BLOCK *daap_response_playlist_items(unsigned int playlist, char* metaStr, char* query) {
    DAAP_BLOCK *root;
    DAAP_BLOCK *mlcl;
    DAAP_BLOCK *mlit;
    ENUMHANDLE henum;
    MP3FILE *current;
    unsigned long int itemid;
    int g=1;
    unsigned long long meta;
    query_node_t*	filter = 0;
    int			songs = 0;

    // if no meta information is specifically requested, return only
    // the base play list information.  iTunes only requests the base
    // information as it rebuilds the entire database locally so it's
    // just replicated information
    if(0 == metaStr)
	meta = ((1ll << metaItemId) |
		(1ll << metaItemName) |
		(1ll << metaItemKind) |
		(1ll << metaContainerItemId) |
		(1ll << metaParentContainerId));
    else
	meta = encodeMetaRequest(metaStr, gSongMetaDataMap);

    if(0 != query) {
	filter = query_build(query, song_fields);
	DPRINTF(E_INF,L_DAAP|L_QRY,"query: %s\n",query);
	if(err_debuglevel >= E_INF) /* this is broken */
	    query_dump(stderr,filter, 0);
    }

    DPRINTF(E_DBG,L_DAAP|L_PL,"Preparing to send playlist items for pl #%d\n",playlist);
    
    if(playlist == 1) {
	henum=db_enum_begin();
    } else {
	henum=db_playlist_items_enum_begin(playlist);
    }

    /* we can allow an empty playlist... 
    if(!henum)
	return NULL;
    */

    root=daap_add_empty(NULL,"apso");
    if(root) {
	g = (int)daap_add_int(root,"mstt",200);
	g = g && daap_add_char(root,"muty",0);
	g = g && daap_add_int(root,"mtco",0);
	g = g && daap_add_int(root,"mrco",0);

	mlcl=daap_add_empty(root,"mlcl");

	if(mlcl) {
	    if(playlist == 1) {
		while((current=db_enum(&henum))) {
		    if(0 == filter || query_test(filter, current))
		    {
			songs++;
			mlit=daap_add_song_entry(mlcl, current, meta);
			if(0 != mlit) {
			    if(wantsMeta(meta, metaContainerItemId))
				g = g && daap_add_int(mlit,"mcti",current->id);
			} else g=0;
		    }
		}
	    } else { /* other playlist */
		while((itemid=db_playlist_items_enum(&henum)) != -1) {
		    current = db_find(itemid);
		    if(0 != current) {
			if(0 == filter || query_test(filter, current))
			{
			    songs++;
			    DPRINTF(E_DBG,L_DAAP|L_PL,"Adding itemid %lu\n",itemid);
			    mlit=daap_add_song_entry(mlcl,current,meta);
			    if(0 != mlit) {
				if(wantsMeta(meta, metaContainerItemId)) // current->id?
				    //				    g = g && daap_add_int(mlit,"mcti",playlist);
				    g = g && daap_add_int(mlit,"mcti",current->id);
			    } else g = 0;
			}
			db_dispose(current);
			free(current);
		    } else g = 0;
		}
	    }
	} else g=0;
    }

    if(playlist == 1)
	db_enum_end(henum);
    else
	db_playlist_items_enum_end(henum);

    if(0 != filter)
	query_free(filter);

    if(!g) {
	daap_free(root);
	return NULL;
    }

    DPRINTF(E_DBG,L_DAAP|L_PL,"Sucessfully enumerated %d items\n",songs);

    daap_set_int(root, "mtco", songs);
    daap_set_int(root, "mrco", songs);

    return root;
}

//
// handle the index= parameter
// format is:
//	index=<item>		a single item from the list by index
//	index=<l>-<h>		a range of items from the list by
//				index from l to h inclusive
//	index=<l>-		a range of items from the list by
//				index from l to the end of the list
//	index=-<n>		the last <n> items from the list
//
void daap_handle_index(DAAP_BLOCK* block, const char* index)
{
    int		first;
    int		count;
    int		size;
    char*	ptr;
    DAAP_BLOCK*	list;
    DAAP_BLOCK*	item;
    DAAP_BLOCK**back;
    int		n;

    // get the actual list
    if(0 == (list = daap_find(block, "mlcl")))
	return;

    // count the items in the list
    for(size = 0, item = list->children ; item ; item = item->next)
	if(!strncmp(item->tag, "mlit", 4))
	    size++;

    // range start
    n = strtol(index, &ptr, 10);

    // "-n": tail range, keep the last n entries
    if(n < 0)
    {
	n *= -1;

	// if we have too many entries, figure out which to keep
	if(n < size)
	{
	    first = size - n;
	    count = n;
	}

	// if we don't have enough entries, keep what we have
	else
	{
	    first = 0;
	    count = size;
	}
    }

    // "n": single item
    else if(0 == *ptr)
    {
	// item exists, return one item at the appropriate index
	if(n < size)
	{
	    first = n;
	    count = 1;
	}

	// item doesn't exist, return zero items
	else
	{
	    first = 0;
	    count = 0;
	}
    }

    // "x-y": true range
    else if('-' == *ptr)
    {
	// record range start
	first = n;

	// "x-": x to end
	if(*++ptr == 0)
	    n = size;
	    
	// record range end
	else
	{
	    n = strtol(ptr, &ptr, 10) + 1;
	    
	    // wanting more than there is, return fewer
	    if(n > size)
		n = size;
	}

	count = n - first;
    }

    // update the returned record count entry, it's required, so
    // should have already be created
    daap_set_int(block, "mrco", count);

    DPRINTF(E_INF,L_DAAP|L_IND, "index:%s first:%d count:%d\n", index, first, count);

    // remove the first first entries
    for(back = &list->children ; *back && first ; )
	if(!strncmp((**back).tag, "mlit", 4))
	{
	    DPRINTF(E_DBG,L_DAAP|L_IND, "first:%d removing\n", first);
	    daap_remove(*back);
	    first--;
	}
	else
	    back = &(**back).next;

    // keep the next count items
    for( ; *back && count ; back = &(**back).next)
	if(!strncmp((**back).tag, "mlit", 4))
	{
	    DPRINTF(E_DBG,L_DAAP|L_IND,"count:%d keeping\n", count);
	    count--;
	}

    // remove the rest of items
    while(*back)
    {
	if(!strncmp((**back).tag, "mlit", 4))
	{
	    DPRINTF(E_DBG,L_DAAP|L_IND,"removing spare\n");
	    daap_remove(*back);
	}
	else
	    back = &(**back).next;
    }
}

typedef struct _browse_item browse_item;
struct _browse_item
{
    char*		name;
    browse_item*	next;
};

static void add_browse_item(browse_item** root, char* name)
{
    browse_item*	item;

    while(0 != (item = *root) && strcasecmp(item->name, name) < 0)
	root = &item->next;

    if(item && strcasecmp(item->name, name) == 0)
	return;

    item = calloc(1, sizeof(browse_item));
    item->name = strdup(name);
    item->next = *root;
    *root = item;
}

static void free_browse_items(browse_item* root)
{
    while(0 != root)
    {
	browse_item*	next = root->next;

	free(root->name);
	free(root);

	root = next;
    }
}

static int count_browse_items(browse_item* root)
{
    int	count = 0;

    while(0 != root)
    {
	root = root->next;
	count++;
    }

    return count;
}

/* in theory each type of browse has a separate subset of these fields
   which can be used for filtering, but it's just not worth the effort
   and doesn't save anything */
static query_field_t browse_fields[] = {
    { qft_string,	"daap.songartist",	OFFSET_OF(MP3FILE, artist) },
    { qft_string,	"daap.songalbum",	OFFSET_OF(MP3FILE, album) },
    { qft_string,	"daap.songgenre",	OFFSET_OF(MP3FILE, genre) },
    { qft_string,	"daap.songcomposer",	OFFSET_OF(MP3FILE, composer) },
    { 0 }
};

DAAP_BLOCK* daap_response_browse(const char* name, const char* filter)
{
    MP3FILE*		current;
    ENUMHANDLE		henum;
    size_t		field;
    char*		l_type;
    browse_item*	items = 0;
    browse_item*	item;
    DAAP_BLOCK*		root = 0;
    query_node_t*	query = 0;

    if(!strcmp(name, "artists"))
    {
	field = OFFSET_OF(MP3FILE, artist);
	l_type = "abar";
    }
    else if(!strcmp(name, "genres"))
    {
	field = OFFSET_OF(MP3FILE, genre);
	l_type = "abgn";
    }
    else if(!strcmp(name, "albums"))
    {
	field = OFFSET_OF(MP3FILE, album);
	l_type = "abal";
    }
    else if(!strcmp(name, "composers"))
    {
	field = OFFSET_OF(MP3FILE, composer);
	l_type = "abcp";
    }
    else
    {
	DPRINTF(E_WARN,L_DAAP|L_BROW,"Invalid browse request: %s\n", name);
	return NULL;
    }

    if(0 != filter && 
       0 == (query = query_build(filter, browse_fields)))
	return NULL;

    if(query) {
	DPRINTF(E_INF,L_DAAP|L_BROW|L_QRY,"query: %s\n",filter);
	if(err_debuglevel >= E_INF) /* this is broken */
	    query_dump(stderr,query, 0);
    }

    if(0 == (henum = db_enum_begin()))
	return NULL;

    while((current = db_enum(&henum)))
    {
	if(0 == query || query_test(query, current))
	{
	    char*	name = * (char**) ((size_t) current + field);

	    if(0 != name)
		add_browse_item(&items, name);
	}
    }

    db_enum_end(henum);

    if(0 != (root = daap_add_empty(0, "abro")))
    {
	int		count = count_browse_items(items);
	DAAP_BLOCK*	mlcl;

	if(!daap_add_int(root, "mstt", 200) ||
	   !daap_add_int(root, "mtco", count) ||
	   !daap_add_int(root, "mrco", count) ||
	   0 == (mlcl = daap_add_empty(root, l_type)))
	    goto error;

	for(item = items ; item ; item = item->next)
	{
	    if(!daap_add_string(mlcl, "mlit", item->name))
		goto error;
	}
    }

    free_browse_items(items);

    if(0 != query)
	query_free(query);

    return root;

 error:
    free_browse_items(items);

    if(0 != query)
	query_free(query);

    if(root != 0)
	daap_free(root);

    return NULL;
}
