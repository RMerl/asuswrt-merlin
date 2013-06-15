/* MiniDLNA project
 *
 * http://sourceforge.net/projects/minidlna/
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2009  Justin Maggard
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
 *
 * Portions of the code from the MiniUPnP project:
 *
 * Copyright (c) 2006-2007, Thomas Bernard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>

#include "config.h"
#include "upnpglobalvars.h"
#include "utils.h"
#include "upnphttp.h"
#include "upnpsoap.h"
#include "upnpreplyparse.h"
#include "getifaddr.h"
#include "scanner.h"
#include "sql.h"
#include "log.h"

static void
BuildSendAndCloseSoapResp(struct upnphttp * h,
                          const char * body, int bodylen)
{
	static const char beforebody[] =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body>";

	static const char afterbody[] =
		"</s:Body>"
		"</s:Envelope>\r\n";

	BuildHeader_upnphttp(h, 200, "OK",  sizeof(beforebody) - 1
		+ sizeof(afterbody) - 1 + bodylen );

	memcpy(h->res_buf + h->res_buflen, beforebody, sizeof(beforebody) - 1);
	h->res_buflen += sizeof(beforebody) - 1;

	memcpy(h->res_buf + h->res_buflen, body, bodylen);
	h->res_buflen += bodylen;

	memcpy(h->res_buf + h->res_buflen, afterbody, sizeof(afterbody) - 1);
	h->res_buflen += sizeof(afterbody) - 1;

	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

static void
GetSystemUpdateID(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<Id>%d</Id>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;

	bodylen = snprintf(body, sizeof(body), resp,
		action, "urn:schemas-upnp-org:service:ContentDirectory:1",
		updateID, action);
	BuildSendAndCloseSoapResp(h, body, bodylen);
}

static void
IsAuthorizedValidated(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<Result>%d</Result>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;
	struct NameValueParserData data;
	const char * id;

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	id = GetValueFromNameValueList(&data, "DeviceID");
	if (!id)
		id = strstr(h->req_buf + h->req_contentoff, "<DeviceID");
	if(id)
	{
		bodylen = snprintf(body, sizeof(body), resp,
			action, "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1",
			1, action);	
		BuildSendAndCloseSoapResp(h, body, bodylen);
	}
	else
		SoapError(h, 402, "Invalid Args");

	ClearNameValueList(&data);	
}

static void
GetProtocolInfo(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<Source>"
		RESOURCE_PROTOCOL_INFO_VALUES
		"</Source>"
		"<Sink></Sink>"
		"</u:%sResponse>";

	char * body;
	int bodylen;

	bodylen = asprintf(&body, resp,
		action, "urn:schemas-upnp-org:service:ConnectionManager:1",
		action);	
	BuildSendAndCloseSoapResp(h, body, bodylen);
	free(body);
}

static void
GetSortCapabilities(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<SortCaps>"
		  "dc:title,"
		  "dc:date,"
		  "upnp:class,"
		  "upnp:album,"
		  "upnp:originalTrackNumber"
		"</SortCaps>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;

	bodylen = snprintf(body, sizeof(body), resp,
		action, "urn:schemas-upnp-org:service:ContentDirectory:1",
		action);	
	BuildSendAndCloseSoapResp(h, body, bodylen);
}

static void
GetSearchCapabilities(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:%sResponse xmlns:u=\"%s\">"
		"<SearchCaps>"
		  "dc:creator,"
		  "dc:date,"
		  "dc:title,"
		  "upnp:album,"
		  "upnp:actor,"
		  "upnp:artist,"
		  "upnp:class,"
		  "upnp:genre,"
		  "@refID"
		"</SearchCaps>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;

	bodylen = snprintf(body, sizeof(body), resp,
		action, "urn:schemas-upnp-org:service:ContentDirectory:1",
		action);	
	BuildSendAndCloseSoapResp(h, body, bodylen);
}

static void
GetCurrentConnectionIDs(struct upnphttp * h, const char * action)
{
	/* TODO: Use real data. - JM */
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<ConnectionIDs>0</ConnectionIDs>"
		"</u:%sResponse>";

	char body[512];
	int bodylen;

	bodylen = snprintf(body, sizeof(body), resp,
		action, "urn:schemas-upnp-org:service:ConnectionManager:1",
		action);	
	BuildSendAndCloseSoapResp(h, body, bodylen);
}

static void
GetCurrentConnectionInfo(struct upnphttp * h, const char * action)
{
	/* TODO: Use real data. - JM */
	static const char resp[] =
		"<u:%sResponse "
		"xmlns:u=\"%s\">"
		"<RcsID>-1</RcsID>"
		"<AVTransportID>-1</AVTransportID>"
		"<ProtocolInfo></ProtocolInfo>"
		"<PeerConnectionManager></PeerConnectionManager>"
		"<PeerConnectionID>-1</PeerConnectionID>"
		"<Direction>Output</Direction>"
		"<Status>Unknown</Status>"
		"</u:%sResponse>";

	char body[sizeof(resp)+128];
	int bodylen;
	struct NameValueParserData data;
	const char * id_str;
	int id;
	char *endptr;

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	id_str = GetValueFromNameValueList(&data, "ConnectionID");
	DPRINTF(E_INFO, L_HTTP, "GetCurrentConnectionInfo(%s)\n", id_str);
	if(id_str)
		id = strtol(id_str, &endptr, 10);
	if(!id_str || !id_str[0] || endptr[0] || id != 0)
	{
		SoapError(h, 402, "Invalid Args");
	}
	else if(id != 0)
	{
		SoapError(h, 701, "No such object error");
	}
	else
	{
		bodylen = snprintf(body, sizeof(body), resp,
			action, "urn:schemas-upnp-org:service:ConnectionManager:1",
			action);	
		BuildSendAndCloseSoapResp(h, body, bodylen);
	}
	ClearNameValueList(&data);	
}

static void
mime_to_ext(const char * mime, char * buf)
{
	switch( *mime )
	{
		/* Audio extensions */
		case 'a':
			if( strcmp(mime+6, "mpeg") == 0 )
				strcpy(buf, "mp3");
			else if( strcmp(mime+6, "mp4") == 0 )
				strcpy(buf, "m4a");
			else if( strcmp(mime+6, "x-ms-wma") == 0 )
				strcpy(buf, "wma");
			else if( strcmp(mime+6, "x-flac") == 0 )
				strcpy(buf, "flac");
			else if( strcmp(mime+6, "flac") == 0 )
				strcpy(buf, "flac");
			else if( strcmp(mime+6, "x-wav") == 0 )
				strcpy(buf, "wav");
			else if( strncmp(mime+6, "L16", 3) == 0 )
				strcpy(buf, "pcm");
			else if( strcmp(mime+6, "3gpp") == 0 )
				strcpy(buf, "3gp");
			else if( strcmp(mime, "application/ogg") == 0 )
				strcpy(buf, "ogg");
			else
				strcpy(buf, "dat");
			break;
		case 'v':
			if( strcmp(mime+6, "avi") == 0 )
				strcpy(buf, "avi");
			else if( strcmp(mime+6, "divx") == 0 )
				strcpy(buf, "avi");
			else if( strcmp(mime+6, "x-msvideo") == 0 )
				strcpy(buf, "avi");
			else if( strcmp(mime+6, "mpeg") == 0 )
				strcpy(buf, "mpg");
			else if( strcmp(mime+6, "mp4") == 0 )
				strcpy(buf, "mp4");
			else if( strcmp(mime+6, "x-ms-wmv") == 0 )
				strcpy(buf, "wmv");
			else if( strcmp(mime+6, "x-matroska") == 0 )
				strcpy(buf, "mkv");
			else if( strcmp(mime+6, "x-mkv") == 0 )
				strcpy(buf, "mkv");
			else if( strcmp(mime+6, "x-flv") == 0 )
				strcpy(buf, "flv");
			else if( strcmp(mime+6, "vnd.dlna.mpeg-tts") == 0 )
				strcpy(buf, "mpg");
			else if( strcmp(mime+6, "quicktime") == 0 )
				strcpy(buf, "mov");
			else if( strcmp(mime+6, "3gpp") == 0 )
				strcpy(buf, "3gp");
			else if( strcmp(mime+6, "x-tivo-mpeg") == 0 )
				strcpy(buf, "TiVo");
			else
				strcpy(buf, "dat");
			break;
		case 'i':
			if( strcmp(mime+6, "jpeg") == 0 )
				strcpy(buf, "jpg");
			else if( strcmp(mime+6, "png") == 0 )
				strcpy(buf, "png");
			else
				strcpy(buf, "dat");
			break;
		default:
			strcpy(buf, "dat");
			break;
	}
}

/* Standard DLNA/UPnP filter flags */
#define FILTER_CHILDCOUNT                        0x00000001
#define FILTER_DC_CREATOR                        0x00000002
#define FILTER_DC_DATE                           0x00000004
#define FILTER_DC_DESCRIPTION                    0x00000008
#define FILTER_DLNA_NAMESPACE                    0x00000010
#define FILTER_REFID                             0x00000020
#define FILTER_RES                               0x00000040
#define FILTER_RES_BITRATE                       0x00000080
#define FILTER_RES_DURATION                      0x00000100
#define FILTER_RES_NRAUDIOCHANNELS               0x00000200
#define FILTER_RES_RESOLUTION                    0x00000400
#define FILTER_RES_SAMPLEFREQUENCY               0x00000800
#define FILTER_RES_SIZE                          0x00001000
#define FILTER_UPNP_ACTOR                        0x00002000
#define FILTER_UPNP_ALBUM                        0x00004000
#define FILTER_UPNP_ALBUMARTURI                  0x00008000
#define FILTER_UPNP_ALBUMARTURI_DLNA_PROFILEID   0x00010000
#define FILTER_UPNP_ARTIST                       0x00020000
#define FILTER_UPNP_GENRE                        0x00040000
#define FILTER_UPNP_ORIGINALTRACKNUMBER          0x00080000
#define FILTER_UPNP_SEARCHCLASS                  0x00100000
#define FILTER_UPNP_STORAGEUSED                  0x00200000
/* Vendor-specific filter flags */
#define FILTER_SEC_CAPTION_INFO_EX               0x01000000
#define FILTER_SEC_DCM_INFO                      0x02000000
#define FILTER_PV_SUBTITLE_FILE_TYPE             0x04000000
#define FILTER_PV_SUBTITLE_FILE_URI              0x08000000
#define FILTER_AV_MEDIA_CLASS                    0x10000000

static u_int32_t
set_filter_flags(char * filter, struct upnphttp *h)
{
	char *item, *saveptr = NULL;
	u_int32_t flags = 0;

	if( !filter || (strlen(filter) <= 1) )
		/* Not the full 32 bits.  Skip vendor-specific stuff by default. */
		return 0xFFFFFF;
	if( h->reqflags & FLAG_SAMSUNG )
		flags |= FILTER_DLNA_NAMESPACE;
	item = strtok_r(filter, ",", &saveptr);
	while( item != NULL )
	{
		if( saveptr )
			*(item-1) = ',';
		while( isspace(*item) )
			item++;
		if( strcmp(item, "@childCount") == 0 )
		{
			flags |= FILTER_CHILDCOUNT;
		}
		else if( strcmp(item, "dc:creator") == 0 )
		{
			flags |= FILTER_DC_CREATOR;
		}
		else if( strcmp(item, "dc:date") == 0 )
		{
			flags |= FILTER_DC_DATE;
		}
		else if( strcmp(item, "dc:description") == 0 )
		{
			flags |= FILTER_DC_DESCRIPTION;
		}
		else if( strcmp(item, "dlna") == 0 )
		{
			flags |= FILTER_DLNA_NAMESPACE;
		}
		else if( strcmp(item, "@refID") == 0 )
		{
			flags |= FILTER_REFID;
		}
		else if( strcmp(item, "upnp:album") == 0 )
		{
			flags |= FILTER_UPNP_ALBUM;
		}
		else if( strcmp(item, "upnp:albumArtURI") == 0 )
		{
			flags |= FILTER_UPNP_ALBUMARTURI;
			if( h->reqflags & FLAG_SAMSUNG )
				flags |= FILTER_UPNP_ALBUMARTURI_DLNA_PROFILEID;
		}
		else if( strcmp(item, "upnp:albumArtURI@dlna:profileID") == 0 )
		{
			flags |= FILTER_UPNP_ALBUMARTURI;
			flags |= FILTER_UPNP_ALBUMARTURI_DLNA_PROFILEID;
		}
		else if( strcmp(item, "upnp:artist") == 0 )
		{
			flags |= FILTER_UPNP_ARTIST;
		}
		else if( strcmp(item, "upnp:actor") == 0 )
		{
			flags |= FILTER_UPNP_ACTOR;
		}
		else if( strcmp(item, "upnp:genre") == 0 )
		{
			flags |= FILTER_UPNP_GENRE;
		}
		else if( strcmp(item, "upnp:originalTrackNumber") == 0 )
		{
			flags |= FILTER_UPNP_ORIGINALTRACKNUMBER;
		}
		else if( strcmp(item, "upnp:searchClass") == 0 )
		{
			flags |= FILTER_UPNP_SEARCHCLASS;
		}
		else if( strcmp(item, "upnp:storageUsed") == 0 )
		{
			flags |= FILTER_UPNP_STORAGEUSED;
		}
		else if( strcmp(item, "res") == 0 )
		{
			flags |= FILTER_RES;
		}
		else if( (strcmp(item, "res@bitrate") == 0) ||
		         (strcmp(item, "@bitrate") == 0) ||
		         ((strcmp(item, "bitrate") == 0) && (flags & FILTER_RES)) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_BITRATE;
		}
		else if( (strcmp(item, "res@duration") == 0) ||
		         (strcmp(item, "@duration") == 0) ||
		         ((strcmp(item, "duration") == 0) && (flags & FILTER_RES)) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_DURATION;
		}
		else if( (strcmp(item, "res@nrAudioChannels") == 0) ||
		         (strcmp(item, "@nrAudioChannels") == 0) ||
		         ((strcmp(item, "nrAudioChannels") == 0) && (flags & FILTER_RES)) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_NRAUDIOCHANNELS;
		}
		else if( (strcmp(item, "res@resolution") == 0) ||
		         (strcmp(item, "@resolution") == 0) ||
		         ((strcmp(item, "resolution") == 0) && (flags & FILTER_RES)) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_RESOLUTION;
		}
		else if( (strcmp(item, "res@sampleFrequency") == 0) ||
		         (strcmp(item, "@sampleFrequency") == 0) ||
		         ((strcmp(item, "sampleFrequency") == 0) && (flags & FILTER_RES)) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_SAMPLEFREQUENCY;
		}
		else if( (strcmp(item, "res@size") == 0) ||
		         (strcmp(item, "@size") == 0) ||
		         (strcmp(item, "size") == 0) )
		{
			flags |= FILTER_RES;
			flags |= FILTER_RES_SIZE;
		}
		else if( strcmp(item, "sec:CaptionInfoEx") == 0 )
		{
			flags |= FILTER_SEC_CAPTION_INFO_EX;
		}
		else if( strcmp(item, "sec:dcmInfo") == 0 )
		{
			flags |= FILTER_SEC_DCM_INFO;
		}
		else if( strcmp(item, "res@pv:subtitleFileType") == 0 )
		{
			flags |= FILTER_PV_SUBTITLE_FILE_TYPE;
		}
		else if( strcmp(item, "res@pv:subtitleFileUri") == 0 )
		{
			flags |= FILTER_PV_SUBTITLE_FILE_URI;
		}
		else if( strcmp(item, "av:mediaClass") == 0 )
		{
			flags |= FILTER_AV_MEDIA_CLASS;
		}
		item = strtok_r(NULL, ",", &saveptr);
	}

	return flags;
}

char *
parse_sort_criteria(char *sortCriteria, int *error)
{
	char *order = NULL;
	char *item, *saveptr;
	int i, ret, reverse, title_sorted = 0;
	struct string_s str;
	*error = 0;

	if( !sortCriteria )
		return NULL;

	if( (item = strtok_r(sortCriteria, ",", &saveptr)) )
	{
		order = malloc(4096);
		str.data = order;
		str.size = 4096;
		str.off = 0;
		strcatf(&str, "order by  ");
	}
	for( i=0; item != NULL; i++ )
	{
		reverse=0;
		if( i )
			strcatf(&str, ", ");
		if( *item == '+' )
		{
			item++;
		}
		else if( *item == '-' )
		{
			reverse = 1;
			item++;
		}
		else
		{
			DPRINTF(E_ERROR, L_HTTP, "No order specified [%s]\n", item);
			*error = -1;
			goto unhandled_order;
		}
		if( strcasecmp(item, "upnp:class") == 0 )
		{
			strcatf(&str, "o.CLASS");
		}
		else if( strcasecmp(item, "dc:title") == 0 )
		{
			strcatf(&str, "d.TITLE");
			title_sorted = 1;
		}
		else if( strcasecmp(item, "dc:date") == 0 )
		{
			strcatf(&str, "d.DATE");
		}
		else if( strcasecmp(item, "upnp:originalTrackNumber") == 0 )
		{
			strcatf(&str, "d.DISC, d.TRACK");
		}
		else if( strcasecmp(item, "upnp:album") == 0 )
		{
			strcatf(&str, "d.ALBUM");
		}
		else
		{
			DPRINTF(E_ERROR, L_HTTP, "Unhandled SortCriteria [%s]\n", item);
			*error = -1;
			if( i )
			{
				ret = strlen(order);
				order[ret-2] = '\0';
			}
			i--;
			goto unhandled_order;
		}

		if( reverse )
			strcatf(&str, " DESC");
		unhandled_order:
		item = strtok_r(NULL, ",", &saveptr);
	}
	if( i <= 0 )
	{
		free(order);
		return NULL;
	}
	/* Add a "tiebreaker" sort order */
	if( !title_sorted )
		strcatf(&str, ", TITLE ASC");

	return order;
}

inline static void
add_resized_res(int srcw, int srch, int reqw, int reqh, char *dlna_pn,
                char *detailID, struct Response *args)
{
	int dstw = reqw;
	int dsth = reqh;

	if( args->flags & FLAG_NO_RESIZE )
		return;

	strcatf(args->str, "&lt;res ");
	if( args->filter & FILTER_RES_RESOLUTION )
	{
		dstw = reqw;
		dsth = ((((reqw<<10)/srcw)*srch)>>10);
		if( dsth > reqh ) {
			dsth = reqh;
			dstw = (((reqh<<10)/srch) * srcw>>10);
		}
		strcatf(args->str, "resolution=\"%dx%d\" ", dstw, dsth);
	}
	strcatf(args->str, "protocolInfo=\"http-get:*:image/jpeg:"
	                          "DLNA.ORG_PN=%s;DLNA.ORG_CI=1;DLNA.ORG_FLAGS=%08X%024X\"&gt;"
	                          "http://%s:%d/Resized/%s.jpg?width=%d,height=%d"
	                          "&lt;/res&gt;",
	                          dlna_pn, DLNA_FLAG_DLNA_V1_5|DLNA_FLAG_HTTP_STALLING|DLNA_FLAG_TM_B|DLNA_FLAG_TM_I, 0,
	                          lan_addr[args->iface].str, runtime_vars.port,
	                          detailID, dstw, dsth);
}

inline static void
add_res(char *size, char *duration, char *bitrate, char *sampleFrequency,
        char *nrAudioChannels, char *resolution, char *dlna_pn, char *mime,
        char *detailID, char *ext, struct Response *args)
{
	strcatf(args->str, "&lt;res ");
	if( size && (args->filter & FILTER_RES_SIZE) ) {
		strcatf(args->str, "size=\"%s\" ", size);
	}
	if( duration && (args->filter & FILTER_RES_DURATION) ) {
		strcatf(args->str, "duration=\"%s\" ", duration);
	}
	if( bitrate && (args->filter & FILTER_RES_BITRATE) ) {
		int br = atoi(bitrate);
		if(args->flags & FLAG_MS_PFS)
			br /= 8;
		strcatf(args->str, "bitrate=\"%d\" ", br);
	}
	if( sampleFrequency && (args->filter & FILTER_RES_SAMPLEFREQUENCY) ) {
		strcatf(args->str, "sampleFrequency=\"%s\" ", sampleFrequency);
	}
	if( nrAudioChannels && (args->filter & FILTER_RES_NRAUDIOCHANNELS) ) {
		strcatf(args->str, "nrAudioChannels=\"%s\" ", nrAudioChannels);
	}
	if( resolution && (args->filter & FILTER_RES_RESOLUTION) ) {
		strcatf(args->str, "resolution=\"%s\" ", resolution);
	}
	if( args->filter & (FILTER_PV_SUBTITLE_FILE_TYPE|FILTER_PV_SUBTITLE_FILE_URI) )
	{
		if( sql_get_int_field(db, "SELECT ID from CAPTIONS where ID = '%s'", detailID) > 0 )
		{
			if( args->filter & FILTER_PV_SUBTITLE_FILE_TYPE )
				strcatf(args->str, "pv:subtitleFileType=\"SRT\" ");
			if( args->filter & FILTER_PV_SUBTITLE_FILE_URI )
				strcatf(args->str, "pv:subtitleFileUri=\"http://%s:%d/Captions/%s.srt\" ",
			                lan_addr[args->iface].str, runtime_vars.port, detailID);
		}
	}
	strcatf(args->str, "protocolInfo=\"http-get:*:%s:%s\"&gt;"
	                          "http://%s:%d/MediaItems/%s.%s"
	                          "&lt;/res&gt;",
	                          mime, dlna_pn, lan_addr[args->iface].str,
	                          runtime_vars.port, detailID, ext);
}

#define COLUMNS "o.REF_ID, o.DETAIL_ID, o.CLASS," \
                " d.SIZE, d.TITLE, d.DURATION, d.BITRATE, d.SAMPLERATE, d.ARTIST," \
                " d.ALBUM, d.GENRE, d.COMMENT, d.CHANNELS, d.TRACK, d.DATE, d.RESOLUTION," \
                " d.THUMBNAIL, d.CREATOR, d.DLNA_PN, d.MIME, d.ALBUM_ART, d.DISC "
#define SELECT_COLUMNS "SELECT o.OBJECT_ID, o.PARENT_ID, " COLUMNS

static int
callback(void *args, int argc, char **argv, char **azColName)
{
	struct Response *passed_args = (struct Response *)args;
	char *id = argv[0], *parent = argv[1], *refID = argv[2], *detailID = argv[3], *class = argv[4], *size = argv[5], *title = argv[6],
	     *duration = argv[7], *bitrate = argv[8], *sampleFrequency = argv[9], *artist = argv[10], *album = argv[11],
	     *genre = argv[12], *comment = argv[13], *nrAudioChannels = argv[14], *track = argv[15], *date = argv[16], *resolution = argv[17],
	     *tn = argv[18], *creator = argv[19], *dlna_pn = argv[20], *mime = argv[21], *album_art = argv[22];
	char dlna_buf[128];
	char ext[5];
	struct string_s *str = passed_args->str;
	int ret = 0;

	/* Make sure we have at least 8KB left of allocated memory to finish the response. */
	if( str->off > (str->size - 8192) )
	{
#if MAX_RESPONSE_SIZE > 0
		if( (str->size+DEFAULT_RESP_SIZE) <= MAX_RESPONSE_SIZE )
		{
#endif
			str->data = realloc(str->data, (str->size+DEFAULT_RESP_SIZE));
			if( str->data )
			{
				str->size += DEFAULT_RESP_SIZE;
				DPRINTF(E_DEBUG, L_HTTP, "UPnP SOAP response enlarged to %d. [%d results so far]\n",
					str->size, passed_args->returned);
			}
			else
			{
				DPRINTF(E_ERROR, L_HTTP, "UPnP SOAP response was too big, and realloc failed!\n");
				return -1;
			}
#if MAX_RESPONSE_SIZE > 0
		}
		else
		{
			DPRINTF(E_ERROR, L_HTTP, "UPnP SOAP response cut short, to not exceed the max response size [%lld]!\n", (long long int)MAX_RESPONSE_SIZE);
			return -1;
		}
#endif
	}
	passed_args->returned++;

	if( runtime_vars.root_container && strcmp(parent, runtime_vars.root_container) == 0 )
		parent = "0";

	if( strncmp(class, "item", 4) == 0 )
	{
		uint32_t dlna_flags = DLNA_FLAG_DLNA_V1_5|DLNA_FLAG_HTTP_STALLING|DLNA_FLAG_TM_B;
		char *alt_title = NULL;
		/* We may need special handling for certain MIME types */
		if( *mime == 'v' )
		{
			dlna_flags |= DLNA_FLAG_TM_S;
			if( passed_args->flags & FLAG_MIME_AVI_DIVX )
			{
				if( strcmp(mime, "video/x-msvideo") == 0 )
				{
					if( creator )
						strcpy(mime+6, "divx");
					else
						strcpy(mime+6, "avi");
				}
			}
			else if( passed_args->flags & FLAG_MIME_AVI_AVI )
			{
				if( strcmp(mime, "video/x-msvideo") == 0 )
				{
					strcpy(mime+6, "avi");
				}
			}
			else if( passed_args->client == EFreeBox && dlna_pn )
			{
				if( strncmp(dlna_pn, "AVC_TS", 6) == 0 ||
				    strncmp(dlna_pn, "MPEG_TS", 7) == 0 )
				{
					strcpy(mime+6, "mp2t");
				}
			}
			if( !(passed_args->flags & FLAG_DLNA) )
			{
				if( strcmp(mime+6, "vnd.dlna.mpeg-tts") == 0 )
				{
					strcpy(mime+6, "mpeg");
				}
			}
			/* From what I read, Samsung TV's expect a [wrong] MIME type of x-mkv. */
			if( passed_args->flags & FLAG_SAMSUNG )
			{
				if( strcmp(mime+6, "x-matroska") == 0 )
				{
					strcpy(mime+8, "mkv");
				}
			}
			/* LG hack: subtitles won't get used unless dc:title contains a dot. */
			else if( passed_args->client == ELGDevice && (passed_args->filter & FILTER_RES) )
			{
				if( sql_get_int_field(db, "SELECT ID from CAPTIONS where ID = '%s'", detailID) > 0 )
				{
					ret = asprintf(&alt_title, "%s.", title);
					if( ret > 0 )
						title = alt_title;
					else
						alt_title = NULL;
				}
			}
		}
		else if( *mime == 'a' )
		{
			dlna_flags |= DLNA_FLAG_TM_S;
			if( strcmp(mime+6, "x-flac") == 0 )
			{
				if( passed_args->flags & FLAG_MIME_FLAC_FLAC )
				{
					strcpy(mime+6, "flac");
				}
			}
			else if( strcmp(mime+6, "x-wav") == 0 )
			{
				if( passed_args->flags & FLAG_MIME_WAV_WAV )
				{
					strcpy(mime+6, "wav");
				}
			}
		}
		else
			dlna_flags |= DLNA_FLAG_TM_I;

		if( dlna_pn )
			snprintf(dlna_buf, sizeof(dlna_buf), "DLNA.ORG_PN=%s;"
			                                     "DLNA.ORG_OP=01;"
			                                     "DLNA.ORG_CI=0;"
			                                     "DLNA.ORG_FLAGS=%08X%024X",
			                                     dlna_pn, dlna_flags, 0);
		else if( passed_args->flags & FLAG_DLNA )
			snprintf(dlna_buf, sizeof(dlna_buf), "DLNA.ORG_OP=01;"
			                                     "DLNA.ORG_CI=0;"
			                                     "DLNA.ORG_FLAGS=%08X%024X",
			                                     dlna_flags, 0);
		else
			strcpy(dlna_buf, "*");

		ret = strcatf(str, "&lt;item id=\"%s\" parentID=\"%s\" restricted=\"1\"", id, parent);
		if( refID && (passed_args->filter & FILTER_REFID) ) {
			ret = strcatf(str, " refID=\"%s\"", refID);
		}
		ret = strcatf(str, "&gt;"
		                   "&lt;dc:title&gt;%s&lt;/dc:title&gt;"
		                   "&lt;upnp:class&gt;object.%s&lt;/upnp:class&gt;",
		                   title, class);
		if( comment && (passed_args->filter & FILTER_DC_DESCRIPTION) ) {
			ret = strcatf(str, "&lt;dc:description&gt;%.384s&lt;/dc:description&gt;", comment);
		}
		if( creator && (passed_args->filter & FILTER_DC_CREATOR) ) {
			ret = strcatf(str, "&lt;dc:creator&gt;%s&lt;/dc:creator&gt;", creator);
		}
		if( date && (passed_args->filter & FILTER_DC_DATE) ) {
			ret = strcatf(str, "&lt;dc:date&gt;%s&lt;/dc:date&gt;", date);
		}
		if( passed_args->filter & FILTER_SEC_DCM_INFO ) {
			/* Get bookmark */
			ret = strcatf(str, "&lt;sec:dcmInfo&gt;CREATIONDATE=0,FOLDER=%s,BM=%d&lt;/sec:dcmInfo&gt;",
			              title, sql_get_int_field(db, "SELECT SEC from BOOKMARKS where ID = '%s'", detailID));
		}
		if( artist ) {
			if( (*mime == 'v') && (passed_args->filter & FILTER_UPNP_ACTOR) ) {
				ret = strcatf(str, "&lt;upnp:actor&gt;%s&lt;/upnp:actor&gt;", artist);
			}
			if( passed_args->filter & FILTER_UPNP_ARTIST ) {
				ret = strcatf(str, "&lt;upnp:artist&gt;%s&lt;/upnp:artist&gt;", artist);
			}
		}
		if( album && (passed_args->filter & FILTER_UPNP_ALBUM) ) {
			ret = strcatf(str, "&lt;upnp:album&gt;%s&lt;/upnp:album&gt;", album);
		}
		if( genre && (passed_args->filter & FILTER_UPNP_GENRE) ) {
			ret = strcatf(str, "&lt;upnp:genre&gt;%s&lt;/upnp:genre&gt;", genre);
		}
		if( strncmp(id, MUSIC_PLIST_ID, strlen(MUSIC_PLIST_ID)) == 0 ) {
			track = strrchr(id, '$')+1;
		}
		if( track && atoi(track) && (passed_args->filter & FILTER_UPNP_ORIGINALTRACKNUMBER) ) {
			ret = strcatf(str, "&lt;upnp:originalTrackNumber&gt;%s&lt;/upnp:originalTrackNumber&gt;", track);
		}
		if( album_art && atoi(album_art) )
		{
			/* Video and audio album art is handled differently */
			if( *mime == 'v' && (passed_args->filter & FILTER_RES) && !(passed_args->flags & FLAG_MS_PFS) ) {
				ret = strcatf(str, "&lt;res protocolInfo=\"http-get:*:image/jpeg:DLNA.ORG_PN=JPEG_TN\"&gt;"
				                   "http://%s:%d/AlbumArt/%s-%s.jpg"
				                   "&lt;/res&gt;",
				                   lan_addr[passed_args->iface].str, runtime_vars.port, album_art, detailID);
			} else if( passed_args->filter & FILTER_UPNP_ALBUMARTURI ) {
				ret = strcatf(str, "&lt;upnp:albumArtURI");
				if( passed_args->filter & FILTER_UPNP_ALBUMARTURI_DLNA_PROFILEID ) {
					ret = strcatf(str, " dlna:profileID=\"JPEG_TN\" xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\"");
				}
				ret = strcatf(str, "&gt;http://%s:%d/AlbumArt/%s-%s.jpg&lt;/upnp:albumArtURI&gt;",
				                   lan_addr[passed_args->iface].str, runtime_vars.port, album_art, detailID);
			}
		}
		if( (passed_args->flags & FLAG_MS_PFS) && *mime == 'i' ) {
			if( passed_args->client == EMediaRoom && !album )
				ret = strcatf(str, "&lt;upnp:album&gt;%s&lt;/upnp:album&gt;", "[No Keywords]");

			/* EVA2000 doesn't seem to handle embedded thumbnails */
			if( !(passed_args->flags & FLAG_RESIZE_THUMBS) && tn && atoi(tn) ) {
				ret = strcatf(str, "&lt;upnp:albumArtURI&gt;"
				                   "http://%s:%d/Thumbnails/%s.jpg"
			        	           "&lt;/upnp:albumArtURI&gt;",
			                	   lan_addr[passed_args->iface].str, runtime_vars.port, detailID);
			} else {
				ret = strcatf(str, "&lt;upnp:albumArtURI&gt;"
				                   "http://%s:%d/Resized/%s.jpg?width=160,height=160"
			        	           "&lt;/upnp:albumArtURI&gt;",
			                	   lan_addr[passed_args->iface].str, runtime_vars.port, detailID);
			}
		}
		if( passed_args->filter & FILTER_RES ) {
			mime_to_ext(mime, ext);
			add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
			        resolution, dlna_buf, mime, detailID, ext, passed_args);
			if( *mime == 'i' ) {
				int srcw, srch;
				if( resolution && (sscanf(resolution, "%dx%d", &srcw, &srch) == 2) )
				{
					if( srcw > 4096 || srch > 4096 )
						add_resized_res(srcw, srch, 4096, 4096, "JPEG_LRG", detailID, passed_args);
					if( srcw > 1024 || srch > 768 )
						add_resized_res(srcw, srch, 1024, 768, "JPEG_MED", detailID, passed_args);
					if( srcw > 640 || srch > 480 )
						add_resized_res(srcw, srch, 640, 480, "JPEG_SM", detailID, passed_args);
				}
				if( !(passed_args->flags & FLAG_RESIZE_THUMBS) && tn && atoi(tn) ) {
					ret = strcatf(str, "&lt;res protocolInfo=\"http-get:*:%s:%s\"&gt;"
					                   "http://%s:%d/Thumbnails/%s.jpg"
					                   "&lt;/res&gt;",
					                   mime, "DLNA.ORG_PN=JPEG_TN;DLNA.ORG_CI=1", lan_addr[passed_args->iface].str,
					                   runtime_vars.port, detailID);
				}
				else
					add_resized_res(srcw, srch, 160, 160, "JPEG_TN", detailID, passed_args);
			}
			else if( *mime == 'v' ) {
				switch( passed_args->client ) {
				case EToshibaTV:
					if( dlna_pn &&
					    (strncmp(dlna_pn, "MPEG_TS_HD_NA", 13) == 0 ||
					     strncmp(dlna_pn, "MPEG_TS_SD_NA", 13) == 0 ||
					     strncmp(dlna_pn, "AVC_TS_MP_HD_AC3", 16) == 0 ||
					     strncmp(dlna_pn, "AVC_TS_HP_HD_AC3", 16) == 0))
					{
						sprintf(dlna_buf, "DLNA.ORG_PN=%s;DLNA.ORG_OP=01;DLNA.ORG_CI=0", "MPEG_PS_NTSC");
						add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
						        resolution, dlna_buf, mime, detailID, ext, passed_args);
					}
					break;
				case ESonyBDP:
					if( dlna_pn &&
					    (strncmp(dlna_pn, "AVC_TS", 6) == 0 ||
					     strncmp(dlna_pn, "MPEG_TS", 7) == 0) )
					{
						if( strncmp(dlna_pn, "MPEG_TS_SD_NA", 13) != 0 )
						{
							sprintf(dlna_buf, "DLNA.ORG_PN=%s;DLNA.ORG_OP=01;DLNA.ORG_CI=0", "MPEG_TS_SD_NA");
							add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
							        resolution, dlna_buf, mime, detailID, ext, passed_args);
						}
						if( strncmp(dlna_pn, "MPEG_TS_SD_EU", 13) != 0 )
						{
							sprintf(dlna_buf, "DLNA.ORG_PN=%s;DLNA.ORG_OP=01;DLNA.ORG_CI=0", "MPEG_TS_SD_EU");
							add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
							        resolution, dlna_buf, mime, detailID, ext, passed_args);
						}
					}
					else if( (dlna_pn &&
					          (strncmp(dlna_pn, "AVC_MP4", 7) == 0 ||
					           strncmp(dlna_pn, "MPEG4_P2_MP4", 12) == 0)) ||
					         strcmp(mime+6, "x-matroska") == 0 ||
					         strcmp(mime+6, "x-msvideo") == 0 ||
					         strcmp(mime+6, "mpeg") == 0 )
					{
						strcpy(mime+6, "avi");
						if( !dlna_pn || strncmp(dlna_pn, "MPEG_PS_NTSC", 12) != 0 )
						{
							sprintf(dlna_buf, "DLNA.ORG_PN=%s;DLNA.ORG_OP=01;DLNA.ORG_CI=0", "MPEG_PS_NTSC");
							add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
						        	resolution, dlna_buf, mime, detailID, ext, passed_args);
						}
						if( !dlna_pn || strncmp(dlna_pn, "MPEG_PS_PAL", 11) != 0 )
						{
							sprintf(dlna_buf, "DLNA.ORG_PN=%s;DLNA.ORG_OP=01;DLNA.ORG_CI=0", "MPEG_PS_PAL");
							add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
						        	resolution, dlna_buf, mime, detailID, ext, passed_args);
						}
					}
					break;
				case ESonyBravia:
					/* BRAVIA KDL-##*X### series TVs do natively support AVC/AC3 in TS, but
					   require profile to be renamed (applies to _T and _ISO variants also) */
					if( dlna_pn &&
					    (strncmp(dlna_pn, "AVC_TS_MP_SD_AC3", 16) == 0 ||
					     strncmp(dlna_pn, "AVC_TS_MP_HD_AC3", 16) == 0 ||
					     strncmp(dlna_pn, "AVC_TS_HP_HD_AC3", 16) == 0))
					{
					        sprintf(dlna_buf, "DLNA.ORG_PN=AVC_TS_HD_50_AC3%s", dlna_pn + 16);
						add_res(size, duration, bitrate, sampleFrequency, nrAudioChannels,
						        resolution, dlna_buf, mime, detailID, ext, passed_args);
					}
					break;
				case ELGDevice:
					if( alt_title )
					{
						ret = strcatf(str, "&lt;res protocolInfo=\"http-get:*:text/srt:*\"&gt;"
						                     "http://%s:%d/Captions/%s.srt"
						                   "&lt;/res&gt;",
						                   lan_addr[passed_args->iface].str, runtime_vars.port, detailID);
						free(alt_title);
					}
					break;
				case ESamsungSeriesC:
				default:
					if( passed_args->filter & FILTER_SEC_CAPTION_INFO_EX )
					{
						if( sql_get_int_field(db, "SELECT ID from CAPTIONS where ID = '%s'", detailID) > 0 )
						{
							ret = strcatf(str, "&lt;sec:CaptionInfoEx sec:type=\"srt\"&gt;"
							                     "http://%s:%d/Captions/%s.srt"
							                   "&lt;/sec:CaptionInfoEx&gt;",
							                   lan_addr[passed_args->iface].str, runtime_vars.port, detailID);
						}
					}
					break;
				}
			}
		}
		ret = strcatf(str, "&lt;/item&gt;");
	}
	else if( strncmp(class, "container", 9) == 0 )
	{
		ret = strcatf(str, "&lt;container id=\"%s\" parentID=\"%s\" restricted=\"1\" ", id, parent);
		if( passed_args->filter & FILTER_CHILDCOUNT )
		{
			int children;
			ret = sql_get_int_field(db, "SELECT count(*) from OBJECTS where PARENT_ID = '%s';", id);
			children = (ret > 0) ? ret : 0;
			ret = strcatf(str, "childCount=\"%d\"", children);
		}
		/* If the client calls for BrowseMetadata on root, we have to include our "upnp:searchClass"'s, unless they're filtered out */
		if( (passed_args->requested == 1) && (strcmp(id, "0") == 0) )
		{
			ret = strcatf(str, " searchable=\"1\"");
			if( passed_args->filter & FILTER_UPNP_SEARCHCLASS )
			{
				ret = strcatf(str, "&gt;"
				                   "&lt;upnp:searchClass includeDerived=\"1\"&gt;object.item.audioItem&lt;/upnp:searchClass&gt;"
				                   "&lt;upnp:searchClass includeDerived=\"1\"&gt;object.item.imageItem&lt;/upnp:searchClass&gt;"
				                   "&lt;upnp:searchClass includeDerived=\"1\"&gt;object.item.videoItem&lt;/upnp:searchClass");
			}
		}
		ret = strcatf(str, "&gt;"
		                   "&lt;dc:title&gt;%s&lt;/dc:title&gt;"
		                   "&lt;upnp:class&gt;object.%s&lt;/upnp:class&gt;",
		                   title, class);
		if( (passed_args->filter & FILTER_UPNP_STORAGEUSED) && strcmp(class+10, "storageFolder") == 0 ) {
			ret = strcatf(str, "&lt;upnp:storageUsed&gt;%s&lt;/upnp:storageUsed&gt;", (size ? size : "-1"));
		}
		if( creator && (passed_args->filter & FILTER_DC_CREATOR) ) {
			ret = strcatf(str, "&lt;dc:creator&gt;%s&lt;/dc:creator&gt;", creator);
		}
		if( genre && (passed_args->filter & FILTER_UPNP_GENRE) ) {
			ret = strcatf(str, "&lt;upnp:genre&gt;%s&lt;/upnp:genre&gt;", genre);
		}
		if( artist && (passed_args->filter & FILTER_UPNP_ARTIST) ) {
			ret = strcatf(str, "&lt;upnp:artist&gt;%s&lt;/upnp:artist&gt;", artist);
		}
		if( album_art && atoi(album_art) && (passed_args->filter & FILTER_UPNP_ALBUMARTURI) ) {
			ret = strcatf(str, "&lt;upnp:albumArtURI ");
			if( passed_args->filter & FILTER_UPNP_ALBUMARTURI_DLNA_PROFILEID ) {
				ret = strcatf(str, "dlna:profileID=\"JPEG_TN\" xmlns:dlna=\"urn:schemas-dlna-org:metadata-1-0/\"");
			}
			ret = strcatf(str, "&gt;http://%s:%d/AlbumArt/%s-%s.jpg&lt;/upnp:albumArtURI&gt;",
			                   lan_addr[passed_args->iface].str, runtime_vars.port, album_art, detailID);
		}
		if( passed_args->filter & FILTER_AV_MEDIA_CLASS ) {
			char class;
			if( strncmp(id, MUSIC_ID, sizeof(MUSIC_ID)) == 0 )
				class = 'M';
			else if( strncmp(id, VIDEO_ID, sizeof(VIDEO_ID)) == 0 )
				class = 'V';
			else if( strncmp(id, IMAGE_ID, sizeof(IMAGE_ID)) == 0 )
				class = 'P';
			else
				class = 0;
			if( class )
				ret = strcatf(str, "&lt;av:mediaClass xmlns:av=\"urn:schemas-sony-com:av\"&gt;"
				                    "%c&lt;/av:mediaClass&gt;", class);
		}
		ret = strcatf(str, "&lt;/container&gt;");
	}

	return 0;
}

static void
BrowseContentDirectory(struct upnphttp * h, const char * action)
{
	static const char resp0[] =
			"<u:BrowseResponse "
			"xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
			"<Result>"
			"&lt;DIDL-Lite"
			CONTENT_DIRECTORY_SCHEMAS;
	char *zErrMsg = NULL;
	char *sql, *ptr;
	struct Response args;
	struct string_s str;
	int totalMatches;
	int ret;
	char *ObjectID, *Filter, *BrowseFlag, *SortCriteria;
	char *orderBy = NULL;
	struct NameValueParserData data;
	int RequestedCount = 0;
	int StartingIndex = 0;

	memset(&args, 0, sizeof(args));
	memset(&str, 0, sizeof(str));

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);

	ObjectID = GetValueFromNameValueList(&data, "ObjectID");
	Filter = GetValueFromNameValueList(&data, "Filter");
	BrowseFlag = GetValueFromNameValueList(&data, "BrowseFlag");
	SortCriteria = GetValueFromNameValueList(&data, "SortCriteria");

	if( (ptr = GetValueFromNameValueList(&data, "RequestedCount")) )
		RequestedCount = atoi(ptr);
	if( RequestedCount < 0 )
	{
		SoapError(h, 402, "Invalid Args");
		goto browse_error;
	}
	if( !RequestedCount )
		RequestedCount = -1;
	if( (ptr = GetValueFromNameValueList(&data, "StartingIndex")) )
		StartingIndex = atoi(ptr);
	if( StartingIndex < 0 )
	{
		SoapError(h, 402, "Invalid Args");
		goto browse_error;
	}
	if( !BrowseFlag || (strcmp(BrowseFlag, "BrowseDirectChildren") && strcmp(BrowseFlag, "BrowseMetadata")) )
	{
		SoapError(h, 402, "Invalid Args");
		goto browse_error;
	}
	if( !ObjectID && !(ObjectID = GetValueFromNameValueList(&data, "ContainerID")) )
	{
		SoapError(h, 402, "Invalid Args");
		goto browse_error;
	}

	str.data = malloc(DEFAULT_RESP_SIZE);
	str.size = DEFAULT_RESP_SIZE;
	str.off = sprintf(str.data, "%s", resp0);
	/* See if we need to include DLNA namespace reference */
	args.iface = h->iface;
	args.filter = set_filter_flags(Filter, h);
	if( args.filter & FILTER_DLNA_NAMESPACE )
		ret = strcatf(&str, DLNA_NAMESPACE);
	if( args.filter & (FILTER_PV_SUBTITLE_FILE_TYPE|FILTER_PV_SUBTITLE_FILE_URI) )
		ret = strcatf(&str, PV_NAMESPACE);
	strcatf(&str, "&gt;\n");

	args.returned = 0;
	args.requested = RequestedCount;
	args.client = h->req_client;
	args.flags = h->reqflags;
	args.str = &str;
	if( args.flags & FLAG_MS_PFS )
	{
		if( !strchr(ObjectID, '$') && (strcmp(ObjectID, "0") != 0) )
		{
			ptr = sql_get_text_field(db, "SELECT OBJECT_ID from OBJECTS"
			                             " where OBJECT_ID in "
			                             "('"MUSIC_ID"$%q', '"VIDEO_ID"$%q', '"IMAGE_ID"$%q')",
			                             ObjectID, ObjectID, ObjectID);
			if( ptr )
			{
				ObjectID = ptr;
				args.flags |= FLAG_FREE_OBJECT_ID;
			}
		}
	}
	DPRINTF(E_DEBUG, L_HTTP, "Browsing ContentDirectory:\n"
	                         " * ObjectID: %s\n"
	                         " * Count: %d\n"
	                         " * StartingIndex: %d\n"
	                         " * BrowseFlag: %s\n"
	                         " * Filter: %s\n"
	                         " * SortCriteria: %s\n",
				ObjectID, RequestedCount, StartingIndex,
	                        BrowseFlag, Filter, SortCriteria);

	if( strcmp(ObjectID, "0") == 0 )
	{
		args.flags |= FLAG_ROOT_CONTAINER;
		if( runtime_vars.root_container )
		{
			if( (args.flags & FLAG_AUDIO_ONLY) && (strcmp(runtime_vars.root_container, BROWSEDIR_ID) == 0) )
				ObjectID = MUSIC_DIR_ID;
			else
				ObjectID = runtime_vars.root_container;
		}
		else
		{
			if( args.flags & FLAG_AUDIO_ONLY )
				ObjectID = MUSIC_ID;
		}
	}

	if( strcmp(BrowseFlag+6, "Metadata") == 0 )
	{
		args.requested = 1;
		sql = sqlite3_mprintf("SELECT %s, " COLUMNS
		                      "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
	        	              " where OBJECT_ID = '%q';",
		                      (args.flags & FLAG_ROOT_CONTAINER) ? "0, -1" : "o.OBJECT_ID, o.PARENT_ID",
		                      ObjectID);
		ret = sqlite3_exec(db, sql, callback, (void *) &args, &zErrMsg);
		totalMatches = args.returned;
	}
	else
	{
		ret = sql_get_int_field(db, "SELECT count(*) from OBJECTS where PARENT_ID = '%q'", ObjectID);
		totalMatches = (ret > 0) ? ret : 0;
		ret = 0;
		if( SortCriteria )
		{
#ifdef __sparc__ /* Sorting takes too long on slow processors with very large containers */
			if( totalMatches < 10000 )
#endif
			orderBy = parse_sort_criteria(SortCriteria, &ret);
		}
		else
		{
			if( strncmp(ObjectID, MUSIC_PLIST_ID, strlen(MUSIC_PLIST_ID)) == 0 )
			{
				if( strcmp(ObjectID, MUSIC_PLIST_ID) == 0 )
					ret = asprintf(&orderBy, "order by d.TITLE");
				else
					ret = asprintf(&orderBy, "order by length(OBJECT_ID), OBJECT_ID");
			}
			else if( args.client == ERokuSoundBridge )
			{
#ifdef __sparc__
				if( totalMatches < 10000 )
#endif
				ret = asprintf(&orderBy, "order by o.CLASS, d.DISC, d.TRACK, d.TITLE");
			}
			if( ret == -1 )
			{
				orderBy = NULL;
				ret = 0;
			}
		}
		/* If it's a DLNA client, return an error for bad sort criteria */
		if( ret < 0 && ((args.flags & FLAG_DLNA) || GETFLAG(DLNA_STRICT_MASK)) )
		{
			SoapError(h, 709, "Unsupported or invalid sort criteria");
			goto browse_error;
		}

		sql = sqlite3_mprintf( SELECT_COLUMNS
		                      "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
				      " where PARENT_ID = '%q' %s limit %d, %d;",
				      ObjectID, orderBy, StartingIndex, RequestedCount);
		DPRINTF(E_DEBUG, L_HTTP, "Browse SQL: %s\n", sql);
		ret = sqlite3_exec(db, sql, callback, (void *) &args, &zErrMsg);
	}
	if( (ret != SQLITE_OK) && (zErrMsg != NULL) )
	{
		DPRINTF(E_WARN, L_HTTP, "SQL error: %s\nBAD SQL: %s\n", zErrMsg, sql);
		sqlite3_free(zErrMsg);
		SoapError(h, 709, "Unsupported or invalid sort criteria");
		goto browse_error;
	}
	sqlite3_free(sql);
	/* Does the object even exist? */
	if( !totalMatches )
	{
		ret = sql_get_int_field(db, "SELECT count(*) from OBJECTS where OBJECT_ID = '%q'", ObjectID);
		if( ret <= 0 )
		{
			SoapError(h, 701, "No such object error");
			goto browse_error;
		}
	}
	ret = strcatf(&str, "&lt;/DIDL-Lite&gt;</Result>\n"
	                    "<NumberReturned>%u</NumberReturned>\n"
	                    "<TotalMatches>%u</TotalMatches>\n"
	                    "<UpdateID>%u</UpdateID>"
	                    "</u:BrowseResponse>",
	                    args.returned, totalMatches, updateID);
	BuildSendAndCloseSoapResp(h, str.data, str.off);
browse_error:
	ClearNameValueList(&data);
	if( args.flags & FLAG_FREE_OBJECT_ID )
		sqlite3_free(ObjectID);
	free(orderBy);
	free(str.data);
}

static void
SearchContentDirectory(struct upnphttp * h, const char * action)
{
	static const char resp0[] =
			"<u:SearchResponse "
			"xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
			"<Result>"
			"&lt;DIDL-Lite"
			CONTENT_DIRECTORY_SCHEMAS;
	char *zErrMsg = NULL;
	char *sql, *ptr;
	struct Response args;
	struct string_s str;
	int totalMatches;
	int ret;
	char *ContainerID, *Filter, *SearchCriteria, *SortCriteria;
	char *newSearchCriteria = NULL, *orderBy = NULL;
	char groupBy[] = "group by DETAIL_ID";
	struct NameValueParserData data;
	int RequestedCount = 0;
	int StartingIndex = 0;

	memset(&args, 0, sizeof(args));
	memset(&str, 0, sizeof(str));

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);

	ContainerID = GetValueFromNameValueList(&data, "ContainerID");
	Filter = GetValueFromNameValueList(&data, "Filter");
	SearchCriteria = GetValueFromNameValueList(&data, "SearchCriteria");
	SortCriteria = GetValueFromNameValueList(&data, "SortCriteria");

	if( (ptr = GetValueFromNameValueList(&data, "RequestedCount")) )
		RequestedCount = atoi(ptr);
	if( !RequestedCount )
		RequestedCount = -1;
	if( (ptr = GetValueFromNameValueList(&data, "StartingIndex")) )
		StartingIndex = atoi(ptr);
	if( !ContainerID )
	{
		if( !(ContainerID = GetValueFromNameValueList(&data, "ObjectID")) )
		{
			SoapError(h, 402, "Invalid Args");
			goto search_error;
		}
	}

	str.data = malloc(DEFAULT_RESP_SIZE);
	str.size = DEFAULT_RESP_SIZE;
	str.off = sprintf(str.data, "%s", resp0);
	/* See if we need to include DLNA namespace reference */
	args.iface = h->iface;
	args.filter = set_filter_flags(Filter, h);
	if( args.filter & FILTER_DLNA_NAMESPACE )
	{
		ret = strcatf(&str, DLNA_NAMESPACE);
	}
	strcatf(&str, "&gt;\n");

	args.returned = 0;
	args.requested = RequestedCount;
	args.client = h->req_client;
	args.flags = h->reqflags;
	args.str = &str;
	if( args.flags & FLAG_MS_PFS )
	{
		if( !strchr(ContainerID, '$') && (strcmp(ContainerID, "0") != 0) )
		{
			ptr = sql_get_text_field(db, "SELECT OBJECT_ID from OBJECTS"
			                             " where OBJECT_ID in "
			                             "('"MUSIC_ID"$%q', '"VIDEO_ID"$%q', '"IMAGE_ID"$%q')",
			                             ContainerID, ContainerID, ContainerID);
			if( ptr )
			{
				ContainerID = ptr;
				args.flags |= FLAG_FREE_OBJECT_ID;
			}
		}
	}
	DPRINTF(E_DEBUG, L_HTTP, "Searching ContentDirectory:\n"
	                         " * ObjectID: %s\n"
	                         " * Count: %d\n"
	                         " * StartingIndex: %d\n"
	                         " * SearchCriteria: %s\n"
	                         " * Filter: %s\n"
	                         " * SortCriteria: %s\n",
				ContainerID, RequestedCount, StartingIndex,
	                        SearchCriteria, Filter, SortCriteria);

	if( strcmp(ContainerID, "0") == 0 )
		*ContainerID = '*';
	else if( strcmp(ContainerID, MUSIC_ALL_ID) == 0 )
		groupBy[0] = '\0';
	if( !SearchCriteria )
	{
		newSearchCriteria = strdup("1 = 1");
		SearchCriteria = newSearchCriteria;
	}
	else
	{
		SearchCriteria = modifyString(SearchCriteria, "&quot;", "\"", 0);
		SearchCriteria = modifyString(SearchCriteria, "&apos;", "'", 0);
		SearchCriteria = modifyString(SearchCriteria, "&lt;", "<", 0);
		SearchCriteria = modifyString(SearchCriteria, "&gt;", ">", 0);
		SearchCriteria = modifyString(SearchCriteria, "object.", "", 0);
		SearchCriteria = modifyString(SearchCriteria, "derivedfrom", "like", 1);
		SearchCriteria = modifyString(SearchCriteria, "contains", "like", 2);
		SearchCriteria = modifyString(SearchCriteria, "dc:date", "d.DATE", 0);
		SearchCriteria = modifyString(SearchCriteria, "dc:title", "d.TITLE", 0);
		SearchCriteria = modifyString(SearchCriteria, "dc:creator", "d.CREATOR", 0);
		SearchCriteria = modifyString(SearchCriteria, "upnp:class", "o.CLASS", 0);
		SearchCriteria = modifyString(SearchCriteria, "upnp:actor", "d.ARTIST", 0);
		SearchCriteria = modifyString(SearchCriteria, "upnp:artist", "d.ARTIST", 0);
		SearchCriteria = modifyString(SearchCriteria, "upnp:album", "d.ALBUM", 0);
		SearchCriteria = modifyString(SearchCriteria, "upnp:genre", "d.GENRE", 0);
		SearchCriteria = modifyString(SearchCriteria, "exists true", "is not NULL", 0);
		SearchCriteria = modifyString(SearchCriteria, "exists false", "is NULL", 0);
		SearchCriteria = modifyString(SearchCriteria, "@refID", "REF_ID", 0);
		if( strstr(SearchCriteria, "@id") )
		{
			newSearchCriteria = strdup(SearchCriteria);
			SearchCriteria = newSearchCriteria = modifyString(newSearchCriteria, "@id", "OBJECT_ID", 0);
		}
		if( strstr(SearchCriteria, "res is ") )
		{
			if( !newSearchCriteria )
				newSearchCriteria = strdup(SearchCriteria);
			SearchCriteria = newSearchCriteria = modifyString(newSearchCriteria, "res is ", "MIME is ", 0);
		}
		if( strstr(SearchCriteria, "\\\"") )
		{
			if( !newSearchCriteria )
				newSearchCriteria = strdup(SearchCriteria);
			SearchCriteria = newSearchCriteria = modifyString(newSearchCriteria, "\\\"", "&amp;quot;", 0);
		}
	}
	DPRINTF(E_DEBUG, L_HTTP, "Translated SearchCriteria: %s\n", SearchCriteria);

	totalMatches = sql_get_int_field(db, "SELECT (select count(distinct DETAIL_ID)"
	                                     " from OBJECTS o left join DETAILS d on (o.DETAIL_ID = d.ID)"
	                                     " where (OBJECT_ID glob '%q$*') and (%s))"
	                                     " + "
	                                     "(select count(*) from OBJECTS o left join DETAILS d on (o.DETAIL_ID = d.ID)"
	                                     " where (OBJECT_ID = '%q') and (%s))",
	                                     ContainerID, SearchCriteria, ContainerID, SearchCriteria);
	if( totalMatches < 0 )
	{
		/* Must be invalid SQL, so most likely bad or unhandled search criteria. */
		SoapError(h, 708, "Unsupported or invalid search criteria");
		goto search_error;
	}
	/* Does the object even exist? */
	if( !totalMatches )
	{
		ret = sql_get_int_field(db, "SELECT count(*) from OBJECTS where OBJECT_ID = '%q'",
		                        !strcmp(ContainerID, "*")?"0":ContainerID);
		if( ret <= 0 )
		{
			SoapError(h, 710, "No such container");
			goto search_error;
		}
	}
#ifdef __sparc__ /* Sorting takes too long on slow processors with very large containers */
	ret = 0;
	if( totalMatches < 10000 )
#endif
		orderBy = parse_sort_criteria(SortCriteria, &ret);
	/* If it's a DLNA client, return an error for bad sort criteria */
	if( ret < 0 && ((args.flags & FLAG_DLNA) || GETFLAG(DLNA_STRICT_MASK)) )
	{
		SoapError(h, 709, "Unsupported or invalid sort criteria");
		goto search_error;
	}

	sql = sqlite3_mprintf( SELECT_COLUMNS
	                      "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
	                      " where OBJECT_ID glob '%q$*' and (%s) %s "
	                      "%z %s"
	                      " limit %d, %d",
	                      ContainerID, SearchCriteria, groupBy,
	                      (*ContainerID == '*') ? NULL :
                              sqlite3_mprintf("UNION ALL " SELECT_COLUMNS
	                                      "from OBJECTS o left join DETAILS d on (d.ID = o.DETAIL_ID)"
	                                      " where OBJECT_ID = '%q' and (%s) ", ContainerID, SearchCriteria),
	                      orderBy, StartingIndex, RequestedCount);
	DPRINTF(E_DEBUG, L_HTTP, "Search SQL: %s\n", sql);
	ret = sqlite3_exec(db, sql, callback, (void *) &args, &zErrMsg);
	if( (ret != SQLITE_OK) && (zErrMsg != NULL) )
	{
		DPRINTF(E_WARN, L_HTTP, "SQL error: %s\nBAD SQL: %s\n", zErrMsg, sql);
		sqlite3_free(zErrMsg);
	}
	sqlite3_free(sql);
	ret = strcatf(&str, "&lt;/DIDL-Lite&gt;</Result>\n"
	                    "<NumberReturned>%u</NumberReturned>\n"
	                    "<TotalMatches>%u</TotalMatches>\n"
	                    "<UpdateID>%u</UpdateID>"
	                    "</u:SearchResponse>",
	                    args.returned, totalMatches, updateID);
	BuildSendAndCloseSoapResp(h, str.data, str.off);
search_error:
	ClearNameValueList(&data);
	if( args.flags & FLAG_FREE_OBJECT_ID )
		sqlite3_free(ContainerID);
	free(orderBy);
	free(newSearchCriteria);
	free(str.data);
}

/*
If a control point calls QueryStateVariable on a state variable that is not
buffered in memory within (or otherwise available from) the service,
the service must return a SOAP fault with an errorCode of 404 Invalid Var.

QueryStateVariable remains useful as a limited test tool but may not be
part of some future versions of UPnP.
*/
static void
QueryStateVariable(struct upnphttp * h, const char * action)
{
	static const char resp[] =
        "<u:%sResponse "
        "xmlns:u=\"%s\">"
		"<return>%s</return>"
        "</u:%sResponse>";

	char body[512];
	int bodylen;
	struct NameValueParserData data;
	const char * var_name;

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	/*var_name = GetValueFromNameValueList(&data, "QueryStateVariable"); */
	/*var_name = GetValueFromNameValueListIgnoreNS(&data, "varName");*/
	var_name = GetValueFromNameValueList(&data, "varName");

	DPRINTF(E_INFO, L_HTTP, "QueryStateVariable(%.40s)\n", var_name);

	if(!var_name)
	{
		SoapError(h, 402, "Invalid Args");
	}
	else if(strcmp(var_name, "ConnectionStatus") == 0)
	{	
		bodylen = snprintf(body, sizeof(body), resp,
                           action, "urn:schemas-upnp-org:control-1-0",
		                   "Connected", action);
		BuildSendAndCloseSoapResp(h, body, bodylen);
	}
	else
	{
		DPRINTF(E_WARN, L_HTTP, "%s: Unknown: %s\n", action, var_name?var_name:"");
		SoapError(h, 404, "Invalid Var");
	}

	ClearNameValueList(&data);	
}

static void
SamsungGetFeatureList(struct upnphttp * h, const char * action)
{
	static const char resp[] =
		"<u:X_GetFeatureListResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
		"<FeatureList>"
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"
		"&lt;Features xmlns=\"urn:schemas-upnp-org:av:avs\""
		" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
		" xsi:schemaLocation=\"urn:schemas-upnp-org:av:avs http://www.upnp.org/schemas/av/avs.xsd\"&gt;"
		"&lt;Feature name=\"samsung.com_BASICVIEW\" version=\"1\"&gt;"
		 "&lt;container id=\"1\" type=\"object.item.audioItem\"/&gt;"
		 "&lt;container id=\"2\" type=\"object.item.videoItem\"/&gt;"
		 "&lt;container id=\"3\" type=\"object.item.imageItem\"/&gt;"
		"&lt;/Feature&gt;"
		"</FeatureList></u:X_GetFeatureListResponse>";

	BuildSendAndCloseSoapResp(h, resp, sizeof(resp)-1);
}

static void
SamsungSetBookmark(struct upnphttp * h, const char * action)
{
	static const char resp[] =
	    "<u:X_SetBookmarkResponse"
	    " xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">"
	    "</u:X_SetBookmarkResponse>";

	struct NameValueParserData data;
	char *ObjectID, *PosSecond;
	int ret;

	ParseNameValue(h->req_buf + h->req_contentoff, h->req_contentlen, &data);
	ObjectID = GetValueFromNameValueList(&data, "ObjectID");
	PosSecond = GetValueFromNameValueList(&data, "PosSecond");
	if( ObjectID && PosSecond )
	{
		ret = sql_exec(db, "INSERT OR REPLACE into BOOKMARKS"
		                   " VALUES "
		                   "((select DETAIL_ID from OBJECTS where OBJECT_ID = '%q'), %q)", ObjectID, PosSecond);
		if( ret != SQLITE_OK )
			DPRINTF(E_WARN, L_METADATA, "Error setting bookmark %s on ObjectID='%s'\n", PosSecond, ObjectID);
		BuildSendAndCloseSoapResp(h, resp, sizeof(resp)-1);
	}
	else
		SoapError(h, 402, "Invalid Args");

	ClearNameValueList(&data);	
}

static const struct 
{
	const char * methodName; 
	void (*methodImpl)(struct upnphttp *, const char *);
}
soapMethods[] =
{
	{ "QueryStateVariable", QueryStateVariable},
	{ "Browse", BrowseContentDirectory},
	{ "Search", SearchContentDirectory},
	{ "GetSearchCapabilities", GetSearchCapabilities},
	{ "GetSortCapabilities", GetSortCapabilities},
	{ "GetSystemUpdateID", GetSystemUpdateID},
	{ "GetProtocolInfo", GetProtocolInfo},
	{ "GetCurrentConnectionIDs", GetCurrentConnectionIDs},
	{ "GetCurrentConnectionInfo", GetCurrentConnectionInfo},
	{ "IsAuthorized", IsAuthorizedValidated},
	{ "IsValidated", IsAuthorizedValidated},
	{ "X_GetFeatureList", SamsungGetFeatureList},
	{ "X_SetBookmark", SamsungSetBookmark},
	{ 0, 0 }
};

void
ExecuteSoapAction(struct upnphttp * h, const char * action, int n)
{
	char * p;

	p = strchr(action, '#');
	if(p)
	{
		int i = 0;
		int len;
		int methodlen;
		char * p2;
		p++;
		p2 = strchr(p, '"');
		if(p2)
			methodlen = p2 - p;
		else
			methodlen = n - (p - action);
		DPRINTF(E_DEBUG, L_HTTP, "SoapMethod: %.*s\n", methodlen, p);
		while(soapMethods[i].methodName)
		{
			len = strlen(soapMethods[i].methodName);
			if(strncmp(p, soapMethods[i].methodName, len) == 0)
			{
				soapMethods[i].methodImpl(h, soapMethods[i].methodName);
				return;
			}
			i++;
		}

		DPRINTF(E_WARN, L_HTTP, "SoapMethod: Unknown: %.*s\n", methodlen, p);
	}

	SoapError(h, 401, "Invalid Action");
}

/* Standard Errors:
 *
 * errorCode errorDescription Description
 * --------	---------------- -----------
 * 401 		Invalid Action 	No action by that name at this service.
 * 402 		Invalid Args 	Could be any of the following: not enough in args,
 * 							too many in args, no in arg by that name, 
 * 							one or more in args are of the wrong data type.
 * 403 		Out of Sync 	Out of synchronization.
 * 501 		Action Failed 	May be returned in current state of service
 * 							prevents invoking that action.
 * 600-699 	TBD 			Common action errors. Defined by UPnP Forum
 * 							Technical Committee.
 * 700-799 	TBD 			Action-specific errors for standard actions.
 * 							Defined by UPnP Forum working committee.
 * 800-899 	TBD 			Action-specific errors for non-standard actions. 
 * 							Defined by UPnP vendor.
*/
void
SoapError(struct upnphttp * h, int errCode, const char * errDesc)
{
	static const char resp[] = 
		"<s:Envelope "
		"xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
		"<s:Body>"
		"<s:Fault>"
		"<faultcode>s:Client</faultcode>"
		"<faultstring>UPnPError</faultstring>"
		"<detail>"
		"<UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\">"
		"<errorCode>%d</errorCode>"
		"<errorDescription>%s</errorDescription>"
		"</UPnPError>"
		"</detail>"
		"</s:Fault>"
		"</s:Body>"
		"</s:Envelope>";

	char body[2048];
	int bodylen;

	DPRINTF(E_WARN, L_HTTP, "Returning UPnPError %d: %s\n", errCode, errDesc);
	bodylen = snprintf(body, sizeof(body), resp, errCode, errDesc);
	BuildResp2_upnphttp(h, 500, "Internal Server Error", body, bodylen);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

