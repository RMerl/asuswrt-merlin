/* MiniDLNA media server
 * Copyright (C) 2009  Justin Maggard
 *
 * This file is part of MiniDLNA.
 *
 * MiniDLNA is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * MiniDLNA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MiniDLNA. If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#ifdef TIVO_SUPPORT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <time.h>
#include <sys/stat.h>

#include "tivo_utils.h"
#include "upnpglobalvars.h"
#include "upnphttp.h"
#include "upnpsoap.h"
#include "utils.h"
#include "sql.h"
#include "log.h"

static void
SendRootContainer(struct upnphttp *h)
{
	char *resp;
	int len;

	len = xasprintf(&resp, "<?xml version='1.0' encoding='UTF-8' ?>\n"
			"<TiVoContainer>"
			 "<Details>"
			  "<ContentType>x-container/tivo-server</ContentType>"
			  "<SourceFormat>x-container/folder</SourceFormat>"
			  "<TotalDuration>0</TotalDuration>"
			  "<TotalItems>3</TotalItems>"
			  "<Title>%s</Title>"
			 "</Details>"
			 "<ItemStart>0</ItemStart>"
			 "<ItemCount>3</ItemCount>"
			 "<Item>"
			  "<Details>"
			   "<ContentType>x-container/tivo-photos</ContentType>"
			   "<SourceFormat>x-container/folder</SourceFormat>"
			   "<Title>Pictures on %s</Title>"
			  "</Details>"
			  "<Links>"
			   "<Content>"
			    "<Url>/TiVoConnect?Command=QueryContainer&amp;Container=3</Url>"
			   "</Content>"
			  "</Links>"
			 "</Item>"
			 "<Item>"
			  "<Details>"
			   "<ContentType>x-container/tivo-music</ContentType>"
			   "<SourceFormat>x-container/folder</SourceFormat>"
			   "<Title>Music on %s</Title>"
			  "</Details>"
			  "<Links>"
			   "<Content>"
			    "<Url>/TiVoConnect?Command=QueryContainer&amp;Container=1</Url>"
			   "</Content>"
			  "</Links>"
			 "</Item>"
			 "<Item>"
			  "<Details>"
			   "<ContentType>x-container/tivo-videos</ContentType>"
			   "<SourceFormat>x-container/folder</SourceFormat>"
			   "<Title>Videos on %s</Title>"
			  "</Details>"
			  "<Links>"
			   "<Content>"
			    "<Url>/TiVoConnect?Command=QueryContainer&amp;Container=2</Url>"
	                    "<ContentType>x-container/tivo-videos</ContentType>"
			   "</Content>"
			  "</Links>"
			 "</Item>"
			"</TiVoContainer>",
	                friendly_name, friendly_name, friendly_name, friendly_name);
	BuildResp_upnphttp(h, resp, len);
	free(resp);
	SendResp_upnphttp(h);
}

static void
SendFormats(struct upnphttp *h, const char *sformat)
{
	char *resp;
	int len;

	len = xasprintf(&resp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
			"<TiVoFormats>"
			 "<Format>"
			   "<ContentType>video/x-tivo-mpeg</ContentType>"
			   "<Description/>"
			 "</Format>"
			 "<Format>"
			   "<ContentType>%s</ContentType>"
			   "<Description/>"
			 "</Format>"
			"</TiVoFormats>", sformat);
	BuildResp_upnphttp(h, resp, len);
	free(resp);
	SendResp_upnphttp(h);
}

static char *
tivo_unescape_tag(char *tag)
{
	modifyString(tag, "&amp;amp;", "&amp;", 1);
	modifyString(tag, "&amp;amp;lt;", "&lt;", 1);
	modifyString(tag, "&amp;lt;", "&lt;", 1);
	modifyString(tag, "&amp;amp;gt;", "&gt;", 1);
	modifyString(tag, "&amp;gt;", "&gt;", 1);
	modifyString(tag, "&amp;quot;", "&quot;", 1);
	return tag;
}

#define FLAG_SEND_RESIZED  0x01
#define FLAG_NO_PARAMS     0x02
#define FLAG_VIDEO         0x04
static int
callback(void *args, int argc, char **argv, char **azColName)
{
	struct Response *passed_args = (struct Response *)args;
	char *id = argv[0], *class = argv[1], *detailID = argv[2], *size = argv[3], *title = argv[4], *duration = argv[5],
             *bitrate = argv[6], *sampleFrequency = argv[7], *artist = argv[8], *album = argv[9], *genre = argv[10],
             *comment = argv[11], *date = argv[12], *resolution = argv[13], *mime = argv[14];
	struct string_s *str = passed_args->str;

	if( strncmp(class, "item", 4) == 0 )
	{
		int flags = 0;
		tivo_unescape_tag(title);
		if( strncmp(mime, "audio", 5) == 0 )
		{
			flags |= FLAG_NO_PARAMS;
			strcatf(str, "<Item><Details>"
			             "<ContentType>%s</ContentType>"
			             "<SourceFormat>%s</SourceFormat>"
			             "<SourceSize>%s</SourceSize>",
			             "audio/*", mime, size);
			strcatf(str, "<SongTitle>%s</SongTitle>", title);
			if( date )
				strcatf(str, "<AlbumYear>%.*s</AlbumYear>", 4, date);
		}
		else if( strcmp(mime, "image/jpeg") == 0 )
		{
			flags |= FLAG_SEND_RESIZED;
			strcatf(str, "<Item><Details>"
			             "<ContentType>%s</ContentType>"
			             "<SourceFormat>%s</SourceFormat>"
			             "<SourceSize>%s</SourceSize>",
			             "image/*", mime, size);
			if( date )
			{
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				tm.tm_isdst = -1; // Have libc figure out if DST is in effect or not
				strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
				strcatf(str, "<CaptureDate>0x%X</CaptureDate>", (unsigned int)mktime(&tm));
			}
			if( comment )
				strcatf(str, "<Caption>%s</Caption>", comment);
		}
		else if( strncmp(mime, "video", 5) == 0 )
		{
			char *episode;
			flags |= FLAG_VIDEO;
			strcatf(str, "<Item><Details>"
			             "<ContentType>%s</ContentType>"
			             "<SourceFormat>%s</SourceFormat>"
			             "<SourceSize>%s</SourceSize>",
			             mime, mime, size);
			episode = strstr(title, " - ");
			if( episode )
			{
				strcatf(str, "<EpisodeTitle>%s</EpisodeTitle>", episode+3);
				*episode = '\0';
			}
			if( date )
			{
				struct tm tm;
				memset(&tm, 0, sizeof(tm));
				tm.tm_isdst = -1; // Have libc figure out if DST is in effect or not
				strptime(date, "%Y-%m-%dT%H:%M:%S", &tm);
				strcatf(str, "<CaptureDate>0x%X</CaptureDate>", (unsigned int)mktime(&tm));
			}
			if( comment )
				strcatf(str, "<Description>%s</Description>", tivo_unescape_tag(comment));
		}
		else
		{
			return 0;
		}
		strcatf(str, "<Title>%s</Title>", title);
		if( artist ) {
			strcatf(str, "<ArtistName>%s</ArtistName>", tivo_unescape_tag(artist));
		}
		if( album ) {
			strcatf(str, "<AlbumTitle>%s</AlbumTitle>", tivo_unescape_tag(album));
		}
		if( genre ) {
			strcatf(str, "<MusicGenre>%s</MusicGenre>", tivo_unescape_tag(genre));
		}
		if( resolution ) {
			char *width = strsep(&resolution, "x");
			strcatf(str, "<SourceWidth>%s</SourceWidth>"
			                   "<SourceHeight>%s</SourceHeight>",
			                   width, resolution);
		}
		if( duration ) {
			strcatf(str, "<Duration>%d</Duration>",
			      atoi(strrchr(duration, '.')+1) + (1000*atoi(strrchr(duration, ':')+1))
			      + (60000*atoi(strrchr(duration, ':')-2)) + (3600000*atoi(duration)));
		}
		if( bitrate ) {
			strcatf(str, "<SourceBitRate>%s</SourceBitRate>", bitrate);
		}
		if( sampleFrequency ) {
			strcatf(str, "<SourceSampleRate>%s</SourceSampleRate>", sampleFrequency);
		}
		strcatf(str, "</Details><Links>"
		             "<Content>"
		               "<ContentType>%s</ContentType>"
		               "<Url>/%s/%s.%s</Url>%s"
		             "</Content>",
		             mime,
		             (flags & FLAG_SEND_RESIZED) ? "Resized" : "MediaItems",
		             detailID, mime_to_ext(mime),
		             (flags & FLAG_NO_PARAMS) ? "<AcceptsParams>No</AcceptsParams>" : "");
		if( flags & FLAG_VIDEO )
		{
			strcatf(str, "<CustomIcon>"
			               "<ContentType>image/*</ContentType>"
			               "<Url>urn:tivo:image:save-until-i-delete-recording</Url>"
			             "</CustomIcon>");
		}
		strcatf(str, "</Links>");
	}
	else if( strncmp(class, "container", 9) == 0 )
	{
		int count;
		/* Determine the number of children */
#ifdef __sparc__ /* Adding filters on large containers can take a long time on slow processors */
		count = sql_get_int_field(db, "SELECT count(*) from OBJECTS where PARENT_ID = '%s'", id);
#else
		count = sql_get_int_field(db, "SELECT count(*) from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID) where PARENT_ID = '%s' and "
		                              " (MIME in ('image/jpeg', 'audio/mpeg', 'video/mpeg', 'video/x-tivo-mpeg', 'video/x-tivo-mpeg-ts')"
		                              " or CLASS glob 'container*')", id);
#endif
		strcatf(str, "<Item>"
		             "<Details>"
		               "<ContentType>x-container/folder</ContentType>"
		               "<SourceFormat>x-container/folder</SourceFormat>"
		               "<Title>%s</Title>"
		               "<TotalItems>%d</TotalItems>"
		             "</Details>"
		             "<Links>"
		               "<Content>"
		                 "<Url>/TiVoConnect?Command=QueryContainer&amp;Container=%s</Url>"
		                 "<ContentType>x-tivo-container/folder</ContentType>"
		               "</Content>"
		             "</Links>",
		             tivo_unescape_tag(title), count, id);
	}
	strcatf(str, "</Item>");

	passed_args->returned++;

	return 0;
}

#define SELECT_COLUMNS "SELECT o.OBJECT_ID, o.CLASS, o.DETAIL_ID, d.SIZE, d.TITLE," \
	               " d.DURATION, d.BITRATE, d.SAMPLERATE, d.ARTIST, d.ALBUM, d.GENRE," \
	               " d.COMMENT, d.DATE, d.RESOLUTION, d.MIME, d.DISC, d.TRACK "

static void
SendItemDetails(struct upnphttp *h, int64_t item)
{
	char *sql;
	char *zErrMsg = NULL;
	struct Response args;
	struct string_s str;
	int ret;
	memset(&args, 0, sizeof(args));
	memset(&str, 0, sizeof(str));

	str.data = malloc(32768);
	str.size = 32768;
	str.off = sprintf(str.data, "<?xml version='1.0' encoding='UTF-8' ?>\n<TiVoItem>");
	args.str = &str;
	args.requested = 1;
	xasprintf(&sql, SELECT_COLUMNS
	               "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
		       " where o.DETAIL_ID = %lld group by o.DETAIL_ID", (long long)item);
	DPRINTF(E_DEBUG, L_TIVO, "%s\n", sql);
	ret = sqlite3_exec(db, sql, callback, (void *) &args, &zErrMsg);
	free(sql);
	if( ret != SQLITE_OK )
	{
		DPRINTF(E_ERROR, L_HTTP, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	strcatf(&str, "</TiVoItem>");

	BuildResp_upnphttp(h, str.data, str.off);
	free(str.data);
	SendResp_upnphttp(h);
}

static void
SendContainer(struct upnphttp *h, const char *objectID, int itemStart, int itemCount, char *anchorItem,
              int anchorOffset, int recurse, char *sortOrder, char *filter, unsigned long int randomSeed)
{
	char *resp = malloc(262144);
	char *sql, *item, *saveptr;
	char *zErrMsg = NULL;
	char **result;
	char *title, *which;
	char what[10], order[96]={0}, order2[96]={0}, myfilter[256]={0};
	char str_buf[1024];
	char type[8];
	char groupBy[19] = {0};
	struct Response args;
	struct string_s str;
	int totalMatches = 0;
	int i, ret;
	memset(&args, 0, sizeof(args));
	memset(&str, 0, sizeof(str));

	args.str = &str;
	str.data = resp+1024;
	str.size = 262144-1024;
	if( itemCount >= 0 )
	{
		args.requested = itemCount;
	}
	else
	{
		if( itemCount == -100 )
			itemCount = 1;
		args.requested = itemCount * -1;
	}

	switch( *objectID )
	{
		case '1':
			strcpy(type, "music");
			break;
		case '2':
			strcpy(type, "videos");
			break;
		case '3':
			strcpy(type, "photos");
			break;
		default:
			strcpy(type, "server");
			break;
	}

	if( strlen(objectID) == 1 )
	{
		switch( *objectID )
		{
			case '1':
				xasprintf(&title, "Music on %s", friendly_name);
				break;
			case '2':
				xasprintf(&title, "Videos on %s", friendly_name);
				break;
			case '3':
				xasprintf(&title, "Pictures on %s", friendly_name);
				break;
			default:
				xasprintf(&title, "Unknown on %s", friendly_name);
				break;
		}
	}
	else
	{
		item = sql_get_text_field(db, "SELECT NAME from OBJECTS where OBJECT_ID = '%q'", objectID);
		if( item )
		{
			title = escape_tag(item, 1);
			sqlite3_free(item);
		}
		else
			title = strdup("UNKNOWN");
	}

	if( recurse )
	{
		which = sqlite3_mprintf("OBJECT_ID glob '%q$*'", objectID);
		strcpy(groupBy, "group by DETAIL_ID");
	}
	else
	{
		which = sqlite3_mprintf("PARENT_ID = '%q'", objectID);
	}

	if( sortOrder )
	{
		if( strcasestr(sortOrder, "Random") )
		{
			sprintf(order, "tivorandom(%lu)", randomSeed);
			if( itemCount < 0 )
				sprintf(order2, "tivorandom(%lu) DESC", randomSeed);
			else
				sprintf(order2, "tivorandom(%lu)", randomSeed);
		}
		else
		{
			short title_state = 0;
			item = strtok_r(sortOrder, ",", &saveptr);
			while( item != NULL )
			{
				int reverse=0;
				if( *item == '!' )
				{
					reverse = 1;
					item++;
				}
				if( strcasecmp(item, "Type") == 0 )
				{
					strcat(order, "CLASS");
					strcat(order2, "CLASS");
				}
				else if( strcasecmp(item, "Title") == 0 )
				{
					/* Explicitly sort music by track then title. */
					if( title_state < 2 && *objectID == '1' )
					{
						if( !title_state )
						{
							strcat(order, "DISC");
							strcat(order2, "DISC");
							title_state = 1;
						}
						else
						{
							strcat(order, "TRACK");
							strcat(order2, "TRACK");
							title_state = 2;
						}
					}
					else
					{
						strcat(order, "TITLE");
						strcat(order2, "TITLE");
						title_state = -1;
					}
				}
				else if( strcasecmp(item, "CreationDate") == 0 ||
				         strcasecmp(item, "CaptureDate") == 0 )
				{
					strcat(order, "DATE");
					strcat(order2, "DATE");
				}
				else
				{
					DPRINTF(E_INFO, L_TIVO, "Unhandled SortOrder [%s]\n", item);
					goto unhandled_order;
				}

				if( reverse )
				{
					strcat(order, " DESC");
					if( itemCount >= 0 )
						strcat(order2, " DESC");
					else
						strcat(order2, " ASC");
				}
				else
				{
					strcat(order, " ASC");
					if( itemCount >= 0 )
						strcat(order2, " ASC");
					else
						strcat(order2, " DESC");
				}
				strcat(order, ", ");
				strcat(order2, ", ");
				unhandled_order:
				if( title_state <= 0 )
					item = strtok_r(NULL, ",", &saveptr);
			}
			if( title_state != -1 )
			{
				strcat(order, "TITLE ASC, ");
				if( itemCount >= 0 )
					strcat(order2, "TITLE ASC, ");
				else
					strcat(order2, "TITLE DESC, ");
			}
			strcat(order, "DETAIL_ID ASC");
			if( itemCount >= 0 )
				strcat(order2, "DETAIL_ID ASC");
			else
				strcat(order2, "DETAIL_ID DESC");
		}
	}
	else
	{
		sprintf(order, "CLASS, NAME, DETAIL_ID");
		if( itemCount < 0 )
			sprintf(order2, "CLASS DESC, NAME DESC, DETAIL_ID DESC");
		else
			sprintf(order2, "CLASS, NAME, DETAIL_ID");
	}

	if( filter )
	{
		item = strtok_r(filter, ",", &saveptr);
		for( i=0; item != NULL; i++ )
		{
			if( i )
			{
				strcat(myfilter, " or ");
			}
			if( (strcasecmp(item, "x-container/folder") == 0) ||
			    (strncasecmp(item, "x-tivo-container/", 17) == 0) )
			{
				strcat(myfilter, "CLASS glob 'container*'");
			}
			else if( strncasecmp(item, "image", 5) == 0 )
			{
				strcat(myfilter, "MIME = 'image/jpeg'");
			}
			else if( strncasecmp(item, "audio", 5) == 0 )
			{
				strcat(myfilter, "MIME = 'audio/mpeg'");
			}
			else if( strncasecmp(item, "video", 5) == 0 )
			{
				strcat(myfilter, "MIME in ('video/mpeg', 'video/x-tivo-mpeg', 'video/x-tivo-mpeg-ts')");
			}
			else
			{
				DPRINTF(E_INFO, L_TIVO, "Unhandled Filter [%s]\n", item);
				if( i )
				{
					ret = strlen(myfilter);
					myfilter[ret-4] = '\0';
				}
				i--;
			}
			item = strtok_r(NULL, ",", &saveptr);
		}
	}
	else
	{
		strcpy(myfilter, "MIME in ('image/jpeg', 'audio/mpeg', 'video/mpeg', 'video/x-tivo-mpeg', 'video/x-tivo-mpeg-ts') or CLASS glob 'container*'");
	}

	if( anchorItem )
	{
		if( strstr(anchorItem, "QueryContainer") )
		{
			strcpy(what, "OBJECT_ID");
			saveptr = strrchr(anchorItem, '=');
			if( saveptr )
				anchorItem = saveptr + 1;
		}
		else
		{
			strcpy(what, "DETAIL_ID");
		}
		sqlite3Prng.isInit = 0;
		sql = sqlite3_mprintf("SELECT %s from OBJECTS o left join DETAILS d on (o.DETAIL_ID = d.ID)"
		                      " where %s and (%s)"
	                              " %s"
		                      " order by %s", what, which, myfilter, groupBy, order2);
		DPRINTF(E_DEBUG, L_TIVO, "%s\n", sql);
		if( (sql_get_table(db, sql, &result, &ret, NULL) == SQLITE_OK) && ret )
		{
			for( i=1; i<=ret; i++ )
			{
				if( strcmp(anchorItem, result[i]) == 0 )
				{
					if( itemCount < 0 )
						itemStart = ret - i + itemCount;
					else
						itemStart += i;
					break;
				}
			}
			sqlite3_free_table(result);
		}
		sqlite3_free(sql);
	}
	args.start = itemStart+anchorOffset;
	sqlite3Prng.isInit = 0;

	ret = sql_get_int_field(db, "SELECT count(distinct DETAIL_ID) "
	                            "from OBJECTS o left join DETAILS d on (o.DETAIL_ID = d.ID)"
	                            " where %s and (%s)",
	                            which, myfilter);
	totalMatches = (ret > 0) ? ret : 0;
	if( itemCount < 0 && !itemStart && !anchorOffset )
	{
		args.start = totalMatches + itemCount;
	}

	sql = sqlite3_mprintf(SELECT_COLUMNS
	                      "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
		              " where %s and (%s)"
	                      " %s"
			      " order by %s limit %d, %d",
	                      which, myfilter, groupBy, order, args.start, args.requested);
	DPRINTF(E_DEBUG, L_TIVO, "%s\n", sql);
	ret = sqlite3_exec(db, sql, callback, (void *) &args, &zErrMsg);
	sqlite3_free(sql);
	if( ret != SQLITE_OK )
	{
		DPRINTF(E_ERROR, L_HTTP, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		Send500(h);
		sqlite3_free(which);
		free(title);
		free(resp);
		return;
	}
	strcatf(&str, "</TiVoContainer>");

	ret = sprintf(str_buf, "<?xml version='1.0' encoding='UTF-8' ?>\n"
			       "<TiVoContainer>"
	                         "<Details>"
	                           "<ContentType>x-container/tivo-%s</ContentType>"
	                           "<SourceFormat>x-container/folder</SourceFormat>"
	                           "<TotalItems>%d</TotalItems>"
	                           "<Title>%s</Title>"
	                         "</Details>"
	                         "<ItemStart>%d</ItemStart>"
	                         "<ItemCount>%d</ItemCount>",
	                         type, totalMatches, title, args.start, args.returned);
	str.data -= ret;
	memcpy(str.data, &str_buf, ret);
	str.size = str.off+ret;
	free(title);
	sqlite3_free(which);
	BuildResp_upnphttp(h, str.data, str.size);
	free(resp);
	SendResp_upnphttp(h);
}

void
ProcessTiVoCommand(struct upnphttp *h, const char *orig_path)
{
	char *path;
	char *key, *val;
	char *saveptr = NULL, *item;
	char *command = NULL, *container = NULL, *anchorItem = NULL;
	char *sortOrder = NULL, *filter = NULL, *sformat = NULL;
	int64_t detailItem=0;
	int itemStart=0, itemCount=-100, anchorOffset=0, recurse=0;
	unsigned long int randomSeed=0;

	path = strdup(orig_path);
	DPRINTF(E_DEBUG, L_GENERAL, "Processing TiVo command %s\n", path);

	item = strtok_r( path, "&", &saveptr );
	while( item != NULL )
	{
		if( *item == '\0' )
		{
			item = strtok_r( NULL, "&", &saveptr );
			continue;
		}
		decodeString(item, 1);
		val = item;
		key = strsep(&val, "=");
		decodeString(val, 1);
		DPRINTF(E_DEBUG, L_GENERAL, "%s: %s\n", key, val);
		if( strcasecmp(key, "Command") == 0 )
		{
			command = val;
		}
		else if( strcasecmp(key, "Container") == 0 )
		{
			container = val;
		}
		else if( strcasecmp(key, "ItemStart") == 0 )
		{
			itemStart = atoi(val);
		}
		else if( strcasecmp(key, "ItemCount") == 0 )
		{
			itemCount = atoi(val);
		}
		else if( strcasecmp(key, "AnchorItem") == 0 )
		{
			anchorItem = basename(val);
		}
		else if( strcasecmp(key, "AnchorOffset") == 0 )
		{
			anchorOffset = atoi(val);
		}
		else if( strcasecmp(key, "Recurse") == 0 )
		{
			recurse = strcasecmp("yes", val) == 0 ? 1 : 0;
		}
		else if( strcasecmp(key, "SortOrder") == 0 )
		{
			sortOrder = val;
		}
		else if( strcasecmp(key, "Filter") == 0 )
		{
			filter = val;
		}
		else if( strcasecmp(key, "RandomSeed") == 0 )
		{
			randomSeed = strtoul(val, NULL, 10);
		}
		else if( strcasecmp(key, "Url") == 0 )
		{
			if( val )
				detailItem = strtoll(basename(val), NULL, 10);
		}
		else if( strcasecmp(key, "SourceFormat") == 0 )
		{
			sformat = val;
		}
		else if( strcasecmp(key, "Format") == 0 || // Only send XML
		         strcasecmp(key, "SerialNum") == 0 || // Unused for now
		         strcasecmp(key, "DoGenres") == 0 ) // Not sure what this is, so ignore it
		{
			;;
		}
		else
		{
			DPRINTF(E_DEBUG, L_GENERAL, "Unhandled parameter [%s]\n", key);
		}
		item = strtok_r( NULL, "&", &saveptr );
	}
	if( anchorItem )
	{
		strip_ext(anchorItem);
	}

	if( command )
	{
		if( strcmp(command, "QueryContainer") == 0 )
		{
			if( !container || (strcmp(container, "/") == 0) )
			{
				SendRootContainer(h);
			}
			else
			{
				SendContainer(h, container, itemStart, itemCount, anchorItem,
				              anchorOffset, recurse, sortOrder, filter, randomSeed);
			}
		}
		else if( strcmp(command, "QueryItem") == 0 )
		{
			SendItemDetails(h, detailItem);
		}
		else if( strcmp(command, "QueryFormats") == 0 )
		{
			SendFormats(h, sformat);
		}
		else
		{
			DPRINTF(E_DEBUG, L_GENERAL, "Unhandled command [%s]\n", command);
			Send501(h);
			free(path);
			return;
		}
	}
	free(path);
	CloseSocket_upnphttp(h);
}
#endif // TIVO_SUPPORT
