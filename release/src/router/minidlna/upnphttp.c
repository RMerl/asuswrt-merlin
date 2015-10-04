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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>

#include "config.h"
#include "upnpglobalvars.h"
#include "upnphttp.h"
#include "upnpdescgen.h"
#include "minidlnapath.h"
#include "upnpsoap.h"
#include "upnpevents.h"
#include "utils.h"
#include "getifaddr.h"
#include "image_utils.h"
#include "log.h"
#include "sql.h"
#include <libexif/exif-loader.h>
#include "tivo_utils.h"
#include "tivo_commands.h"
#include "clients.h"
#include "process.h"
#include "sendfile.h"

#define MAX_BUFFER_SIZE 2147483647
#define MIN_BUFFER_SIZE 65536

#define INIT_STR(s, d) { s.data = d; s.size = sizeof(d); s.off = 0; }

#include "icons.c"

enum event_type {
	E_INVALID,
	E_SUBSCRIBE,
	E_RENEW
};

static void SendResp_icon(struct upnphttp *, char * url);
static void SendResp_albumArt(struct upnphttp *, char * url);
static void SendResp_caption(struct upnphttp *, char * url);
static void SendResp_resizedimg(struct upnphttp *, char * url);
static void SendResp_thumbnail(struct upnphttp *, char * url);
static void SendResp_dlnafile(struct upnphttp *, char * url);

struct upnphttp * 
New_upnphttp(int s)
{
	struct upnphttp * ret;
	if(s<0)
		return NULL;
	ret = (struct upnphttp *)malloc(sizeof(struct upnphttp));
	if(ret == NULL)
		return NULL;
	memset(ret, 0, sizeof(struct upnphttp));
	ret->socket = s;
	return ret;
}

void
CloseSocket_upnphttp(struct upnphttp * h)
{
	if(close(h->socket) < 0)
	{
		DPRINTF(E_ERROR, L_HTTP, "CloseSocket_upnphttp: close(%d): %s\n", h->socket, strerror(errno));
	}
	h->socket = -1;
	h->state = 100;
}

void
Delete_upnphttp(struct upnphttp * h)
{
	if(h)
	{
		if(h->socket >= 0)
			CloseSocket_upnphttp(h);
		free(h->req_buf);
		free(h->res_buf);
		free(h);
	}
}

/* parse HttpHeaders of the REQUEST */
static void
ParseHttpHeaders(struct upnphttp * h)
{
	int client = 0;
	char * line;
	char * colon;
	char * p;
	int n;
	line = h->req_buf;
	/* TODO : check if req_buf, contentoff are ok */
	while(line < (h->req_buf + h->req_contentoff))
	{
		colon = strchr(line, ':');
		if(colon)
		{
			if(strncasecmp(line, "Content-Length", 14)==0)
			{
				p = colon;
				while(*p && (*p < '0' || *p > '9'))
					p++;
				h->req_contentlen = atoi(p);
				if(h->req_contentlen < 0) {
					DPRINTF(E_WARN, L_HTTP, "Invalid Content-Length %d", h->req_contentlen);
					h->req_contentlen = 0;
				}
			}
			else if(strncasecmp(line, "SOAPAction", 10)==0)
			{
				p = colon;
				n = 0;
				while(*p == ':' || *p == ' ' || *p == '\t')
					p++;
				while(p[n] >= ' ')
					n++;
				if(n >= 2 &&
				   ((p[0] == '"' && p[n-1] == '"') ||
				    (p[0] == '\'' && p[n-1] == '\'')))
				{
					p++;
					n -= 2;
				}
				h->req_soapAction = p;
				h->req_soapActionLen = n;
			}
			else if(strncasecmp(line, "Callback", 8)==0)
			{
				p = colon;
				while(*p && *p != '<' && *p != '\r' )
					p++;
				n = 0;
				while(p[n] && p[n] != '>' && p[n] != '\r' )
					n++;
				h->req_Callback = p + 1;
				h->req_CallbackLen = MAX(0, n - 1);
			}
			else if(strncasecmp(line, "SID", 3)==0)
			{
				//zqiu: fix bug for test 4.0.5
				//Skip extra headers like "SIDHEADER: xxxxxx xxx"
				for(p=line+3;p<colon;p++)
				{
					if(!isspace(*p))
					{
						p = NULL; //unexpected header
						break;
					}
				}
				if(p) {
					p = colon + 1;
					while(isspace(*p))
						p++;
					n = 0;
					while(p[n] && !isspace(p[n]))
						n++;
					h->req_SID = p;
					h->req_SIDLen = n;
				}
			}
			else if(strncasecmp(line, "NT", 2)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				n = 0;
				while(p[n] && !isspace(p[n]))
					n++;
				h->req_NT = p;
				h->req_NTLen = n;
			}
			/* Timeout: Seconds-nnnn */
			/* TIMEOUT
			Recommended. Requested duration until subscription expires,
			either number of seconds or infinite. Recommendation
			by a UPnP Forum working committee. Defined by UPnP vendor.
			Consists of the keyword "Second-" followed (without an
			intervening space) by either an integer or the keyword "infinite". */
			else if(strncasecmp(line, "Timeout", 7)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if(strncasecmp(p, "Second-", 7)==0) {
					h->req_Timeout = atoi(p+7);
				}
			}
			// Range: bytes=xxx-yyy
			else if(strncasecmp(line, "Range", 5)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if(strncasecmp(p, "bytes=", 6)==0) {
					h->reqflags |= FLAG_RANGE;
					h->req_RangeStart = strtoll(p+6, &colon, 10);
					h->req_RangeEnd = colon ? atoll(colon+1) : 0;
					DPRINTF(E_DEBUG, L_HTTP, "Range Start-End: %lld - %lld\n",
						(long long)h->req_RangeStart,
						h->req_RangeEnd ? (long long)h->req_RangeEnd : -1);
				}
			}
			else if(strncasecmp(line, "Host", 4)==0)
			{
				int i;
				h->reqflags |= FLAG_HOST;
				p = colon + 1;
				while(isspace(*p))
					p++;
				for(n = 0; n<n_lan_addr; n++)
				{
					for(i=0; lan_addr[n].str[i]; i++)
					{
						if(lan_addr[n].str[i] != p[i])
							break;
					}
					if(!lan_addr[n].str[i])
					{
						h->iface = n;
						break;
					}
				}
			}
			else if(strncasecmp(line, "User-Agent", 10)==0)
			{
				int i;
				/* Skip client detection if we already detected it. */
				if( client )
					goto next_header;
				p = colon + 1;
				while(isspace(*p))
					p++;
				for (i = 0; client_types[i].name; i++)
				{
					if (client_types[i].match_type != EUserAgent)
						continue;
					if (strstrc(p, client_types[i].match, '\r') != NULL)
					{
						client = i;
						break;
					}
				}
			}
			else if(strncasecmp(line, "X-AV-Client-Info", 16)==0)
			{
				int i;
				/* Skip client detection if we already detected it. */
				if( client && client_types[client].type < EStandardDLNA150 )
					goto next_header;
				p = colon + 1;
				while(isspace(*p))
					p++;
				for (i = 0; client_types[i].name; i++)
				{
					if (client_types[i].match_type != EXAVClientInfo)
						continue;
					if (strstrc(p, client_types[i].match, '\r') != NULL)
					{
						client = i;
						break;
					}
				}
			}
			else if(strncasecmp(line, "Transfer-Encoding", 17)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if(strncasecmp(p, "chunked", 7)==0)
				{
					h->reqflags |= FLAG_CHUNKED;
				}
			}
			else if(strncasecmp(line, "Accept-Language", 15)==0)
			{
				h->reqflags |= FLAG_LANGUAGE;
			}
			else if(strncasecmp(line, "getcontentFeatures.dlna.org", 27)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if( (*p != '1') || !isspace(p[1]) )
					h->reqflags |= FLAG_INVALID_REQ;
			}
			else if(strncasecmp(line, "TimeSeekRange.dlna.org", 22)==0)
			{
				h->reqflags |= FLAG_TIMESEEK;
			}
			else if(strncasecmp(line, "PlaySpeed.dlna.org", 18)==0)
			{
				h->reqflags |= FLAG_PLAYSPEED;
			}
			else if(strncasecmp(line, "realTimeInfo.dlna.org", 21)==0)
			{
				h->reqflags |= FLAG_REALTIMEINFO;
			}
			else if(strncasecmp(line, "getAvailableSeekRange.dlna.org", 21)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if( (*p != '1') || !isspace(p[1]) )
					h->reqflags |= FLAG_INVALID_REQ;
			}
			else if(strncasecmp(line, "transferMode.dlna.org", 21)==0)
			{
				p = colon + 1;
				while(isspace(*p))
					p++;
				if(strncasecmp(p, "Streaming", 9)==0)
				{
					h->reqflags |= FLAG_XFERSTREAMING;
				}
				if(strncasecmp(p, "Interactive", 11)==0)
				{
					h->reqflags |= FLAG_XFERINTERACTIVE;
				}
				if(strncasecmp(p, "Background", 10)==0)
				{
					h->reqflags |= FLAG_XFERBACKGROUND;
				}
			}
			else if(strncasecmp(line, "getCaptionInfo.sec", 18)==0)
			{
				h->reqflags |= FLAG_CAPTION;
			}
			else if(strncasecmp(line, "FriendlyName", 12)==0)
			{
				int i;
				p = colon + 1;
				while(isspace(*p))
					p++;
				for (i = 0; client_types[i].name; i++)
				{
					if (client_types[i].match_type != EFriendlyName)
						continue;
					if (strstrc(p, client_types[i].match, '\r') != NULL)
					{
						client = i;
						break;
					}
				}
			}
			else if(strncasecmp(line, "uctt.upnp.org:", 14)==0)
			{
				/* Conformance testing */
				SETFLAG(DLNA_STRICT_MASK);
			}
		}
next_header:
		line = strstr(line, "\r\n");
		if (!line)
			return;
		line += 2;
	}
	if( h->reqflags & FLAG_CHUNKED )
	{
		char *endptr;
		h->req_chunklen = -1;
		if( h->req_buflen <= h->req_contentoff )
			return;
		while( (line < (h->req_buf + h->req_buflen)) &&
		       (h->req_chunklen = strtol(line, &endptr, 16)) &&
		       (endptr != line) )
		{
			endptr = strstr(endptr, "\r\n");
			if (!endptr)
			{
				return;
			}
			line = endptr+h->req_chunklen+2;
		}

		if( endptr == line )
		{
			h->req_chunklen = -1;
			return;
		}
	}
	/* If the client type wasn't found, search the cache.
	 * This is done because a lot of clients like to send a
	 * different User-Agent with different types of requests. */
	h->req_client = SearchClientCache(h->clientaddr, 0);
	/* Add this client to the cache if it's not there already. */
	if (!h->req_client)
	{
		h->req_client = AddClientCache(h->clientaddr, client);
	}
	else if (client)
	{
		enum client_types type = client_types[client].type;
		enum client_types ctype = h->req_client->type->type;
		/* If we know the client and our new detection is generic, use our cached info */
		/* If we detected a Samsung Series B earlier, don't overwrite it with Series A info */
		if ((ctype && ctype < EStandardDLNA150 && type >= EStandardDLNA150) ||
		    (ctype == ESamsungSeriesB && type == ESamsungSeriesA))
			return;
		h->req_client->type = &client_types[client];
		h->req_client->age = uptime();
	}
}

/* very minimalistic 400 error message */
static void
Send400(struct upnphttp * h)
{
	static const char body400[] =
		"<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>"
		"<BODY><H1>Bad Request</H1>The request is invalid"
		" for this HTTP version.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 400, "Bad Request",
	                    body400, sizeof(body400) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* very minimalistic 404 error message */
static void
Send404(struct upnphttp * h)
{
	static const char body404[] =
		"<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>"
		"<BODY><H1>Not Found</H1>The requested URL was not found"
		" on this server.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 404, "Not Found",
	                    body404, sizeof(body404) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* very minimalistic 406 error message */
static void
Send406(struct upnphttp * h)
{
	static const char body406[] =
		"<HTML><HEAD><TITLE>406 Not Acceptable</TITLE></HEAD>"
		"<BODY><H1>Not Acceptable</H1>An unsupported operation"
		" was requested.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 406, "Not Acceptable",
	                    body406, sizeof(body406) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* very minimalistic 416 error message */
static void
Send416(struct upnphttp * h)
{
	static const char body416[] =
		"<HTML><HEAD><TITLE>416 Requested Range Not Satisfiable</TITLE></HEAD>"
		"<BODY><H1>Requested Range Not Satisfiable</H1>The requested range"
		" was outside the file's size.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 416, "Requested Range Not Satisfiable",
	                    body416, sizeof(body416) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* very minimalistic 500 error message */
void
Send500(struct upnphttp * h)
{
	static const char body500[] = 
		"<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>"
		"<BODY><H1>Internal Server Error</H1>Server encountered "
		"and Internal Error.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 500, "Internal Server Errror",
	                    body500, sizeof(body500) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* very minimalistic 501 error message */
void
Send501(struct upnphttp * h)
{
	static const char body501[] = 
		"<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>"
		"<BODY><H1>Not Implemented</H1>The HTTP Method "
		"is not implemented by this server.</BODY></HTML>\r\n";
	h->respflags = FLAG_HTML;
	BuildResp2_upnphttp(h, 501, "Not Implemented",
	                    body501, sizeof(body501) - 1);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* Sends the description generated by the parameter */
static void
sendXMLdesc(struct upnphttp * h, char * (f)(int *))
{
	char * desc;
	int len;
	desc = f(&len);
	if(!desc)
	{
		DPRINTF(E_ERROR, L_HTTP, "Failed to generate XML description\n");
		Send500(h);
		return;
	}
	BuildResp_upnphttp(h, desc, len);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
	free(desc);
}

#ifdef READYNAS
static void
SendResp_readynas_admin(struct upnphttp * h)
{
	char body[128];
	int l;

	h->respflags = FLAG_HTML;
	l = snprintf(body, sizeof(body), "<meta http-equiv=\"refresh\" content=\"0; url=https://%s/admin/\">",
	              lan_addr[h->iface].str);

	BuildResp_upnphttp(h, body, l);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}
#endif

static void
SendResp_presentation(struct upnphttp * h)
{
	struct string_s str;
	char body[4096];
	int a, v, p, i;

	INIT_STR(str, body);

	h->respflags = FLAG_HTML;

	a = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'a*'");
	v = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'v*'");
	p = sql_get_int_field(db, "SELECT count(*) from DETAILS where MIME glob 'i*'");
	strcatf(&str,
		"<HTML><HEAD><TITLE>" SERVER_NAME " " MINIDLNA_VERSION "</TITLE></HEAD>"
		"<BODY><div style=\"text-align: center\">"
		"<h2>" SERVER_NAME " status</h2></div>");

	strcatf(&str,
		"<h3>Media library</h3>"
		"<table border=1 cellpadding=10>"
		"<tr><td>Audio files</td><td>%d</td></tr>"
		"<tr><td>Video files</td><td>%d</td></tr>"
		"<tr><td>Image files</td><td>%d</td></tr>"
		"</table>", a, v, p);

	if (scanning)
		strcatf(&str,
			"<br><i>* Media scan in progress</i><br>");

	strcatf(&str,
		"<h3>Connected clients</h3>"
		"<table border=1 cellpadding=10>"
		"<tr><td>ID</td><td>Type</td><td>IP Address</td><td>HW Address</td><td>Connections</td></tr>");
	for (i = 0; i < CLIENT_CACHE_SLOTS; i++)
	{
		if (!clients[i].addr.s_addr)
			continue;
		strcatf(&str, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%02X:%02X:%02X:%02X:%02X:%02X</td><td>%d</td></tr>",
				i, clients[i].type->name, inet_ntoa(clients[i].addr),
				clients[i].mac[0], clients[i].mac[1], clients[i].mac[2],
				clients[i].mac[3], clients[i].mac[4], clients[i].mac[5], clients[i].connections);
	}
	strcatf(&str, "</table>");

	strcatf(&str, "<br>%d connection%s currently open<br>", (number_of_children > 0 ? : 0), (number_of_children == 1 ? "" : "s"));
	strcatf(&str, "</BODY></HTML>\r\n");

	BuildResp_upnphttp(h, str.data, str.off);
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* ProcessHTTPPOST_upnphttp()
 * executes the SOAP query if it is possible */
static void
ProcessHTTPPOST_upnphttp(struct upnphttp * h)
{
	if((h->req_buflen - h->req_contentoff) >= h->req_contentlen)
	{
		if(h->req_soapAction)
		{
			/* we can process the request */
			DPRINTF(E_DEBUG, L_HTTP, "SOAPAction: %.*s\n", h->req_soapActionLen, h->req_soapAction);
			ExecuteSoapAction(h, 
				h->req_soapAction,
				h->req_soapActionLen);
		}
		else
		{
			static const char err400str[] =
				"<html><body>Bad request</body></html>";
			DPRINTF(E_WARN, L_HTTP, "No SOAPAction in HTTP headers\n");
			h->respflags = FLAG_HTML;
			BuildResp2_upnphttp(h, 400, "Bad Request",
			                    err400str, sizeof(err400str) - 1);
			SendResp_upnphttp(h);
			CloseSocket_upnphttp(h);
		}
	}
	else
	{
		/* waiting for remaining data */
		h->state = 1;
	}
}

static int
check_event(struct upnphttp *h)
{
	enum event_type type;

	if (h->req_Callback)
	{
		if (h->req_SID || !h->req_NT)
		{
			BuildResp2_upnphttp(h, 400, "Bad Request",
				            "<html><body>Bad request</body></html>", 37);
			type = E_INVALID;
		}
		else if (strncmp(h->req_Callback, "http://", 7) != 0 ||
		         strncmp(h->req_NT, "upnp:event", h->req_NTLen) != 0)
		{
			/* Missing or invalid CALLBACK : 412 Precondition Failed.
			 * If CALLBACK header is missing or does not contain a valid HTTP URL,
			 * the publisher must respond with HTTP error 412 Precondition Failed*/
			BuildResp2_upnphttp(h, 412, "Precondition Failed", 0, 0);
			type = E_INVALID;
		}
		else
			type = E_SUBSCRIBE;
	}
	else if (h->req_SID)
	{
		/* subscription renew */
		if (h->req_NT)
		{
			BuildResp2_upnphttp(h, 400, "Bad Request",
				            "<html><body>Bad request</body></html>", 37);
			type = E_INVALID;
		}
		else
			type = E_RENEW;
	}
	else
	{
		BuildResp2_upnphttp(h, 412, "Precondition Failed", 0, 0);
		type = E_INVALID;
	}

	return type;
}

static void
ProcessHTTPSubscribe_upnphttp(struct upnphttp * h, const char * path)
{
	const char * sid;
	enum event_type type;
	DPRINTF(E_DEBUG, L_HTTP, "ProcessHTTPSubscribe %s\n", path);
	DPRINTF(E_DEBUG, L_HTTP, "Callback '%.*s' Timeout=%d\n",
		h->req_CallbackLen, h->req_Callback, h->req_Timeout);
	DPRINTF(E_DEBUG, L_HTTP, "SID '%.*s'\n", h->req_SIDLen, h->req_SID);

	type = check_event(h);
	if (type == E_SUBSCRIBE)
	{
		/* - add to the subscriber list
		 * - respond HTTP/x.x 200 OK 
		 * - Send the initial event message */
		/* Server:, SID:; Timeout: Second-(xx|infinite) */
		sid = upnpevents_addSubscriber(path, h->req_Callback,
		                               h->req_CallbackLen, h->req_Timeout);
		h->respflags = FLAG_TIMEOUT;
		if (sid)
		{
			DPRINTF(E_DEBUG, L_HTTP, "generated sid=%s\n", sid);
			h->respflags |= FLAG_SID;
			h->req_SID = sid;
			h->req_SIDLen = strlen(sid);
		}
		BuildResp_upnphttp(h, 0, 0);
	}
	else if (type == E_RENEW)
	{
		/* subscription renew */
		if (renewSubscription(h->req_SID, h->req_SIDLen, h->req_Timeout) < 0)
		{
			/* Invalid SID
			   412 Precondition Failed. If a SID does not correspond to a known,
			   un-expired subscription, the publisher must respond
			   with HTTP error 412 Precondition Failed. */
			BuildResp2_upnphttp(h, 412, "Precondition Failed", 0, 0);
		}
		else
		{
			/* A DLNA device must enforce a 5 minute timeout */
			h->respflags = FLAG_TIMEOUT;
			h->req_Timeout = 300;
			h->respflags |= FLAG_SID;
			BuildResp_upnphttp(h, 0, 0);
		}
	}
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

static void
ProcessHTTPUnSubscribe_upnphttp(struct upnphttp * h, const char * path)
{
	enum event_type type;
	DPRINTF(E_DEBUG, L_HTTP, "ProcessHTTPUnSubscribe %s\n", path);
	DPRINTF(E_DEBUG, L_HTTP, "SID '%.*s'\n", h->req_SIDLen, h->req_SID);
	/* Remove from the list */
	type = check_event(h);
	if (type != E_INVALID)
	{
		if(upnpevents_removeSubscriber(h->req_SID, h->req_SIDLen) < 0)
			BuildResp2_upnphttp(h, 412, "Precondition Failed", 0, 0);
		else
			BuildResp_upnphttp(h, 0, 0);
	}
	SendResp_upnphttp(h);
	CloseSocket_upnphttp(h);
}

/* Parse and process Http Query 
 * called once all the HTTP headers have been received. */
static void
ProcessHttpQuery_upnphttp(struct upnphttp * h)
{
	char HttpCommand[16];
	char HttpUrl[512];
	char * HttpVer;
	char * p;
	int i;
	p = h->req_buf;
	if(!p)
		return;
	for(i = 0; i<15 && *p && *p != ' ' && *p != '\r'; i++)
		HttpCommand[i] = *(p++);
	HttpCommand[i] = '\0';
	while(*p==' ')
		p++;
	if(strncmp(p, "http://", 7) == 0)
	{
		p = p+7;
		while(*p!='/')
			p++;
	}
	for(i = 0; i<511 && *p && *p != ' ' && *p != '\r'; i++)
		HttpUrl[i] = *(p++);
	HttpUrl[i] = '\0';
	while(*p==' ')
		p++;
	HttpVer = h->HttpVer;
	for(i = 0; i<15 && *p && *p != '\r'; i++)
		HttpVer[i] = *(p++);
	HttpVer[i] = '\0';

	/* set the interface here initially, in case there is no Host header */
	for(i = 0; i<n_lan_addr; i++)
	{
		if( (h->clientaddr.s_addr & lan_addr[i].mask.s_addr)
		   == (lan_addr[i].addr.s_addr & lan_addr[i].mask.s_addr))
		{
			h->iface = i;
			break;
		}
	}

	ParseHttpHeaders(h);

	/* see if we need to wait for remaining data */
	if( (h->reqflags & FLAG_CHUNKED) )
	{
		if( h->req_chunklen == -1)
		{
			Send400(h);
			return;
		}
		if( h->req_chunklen )
		{
			h->state = 2;
			return;
		}
		char *chunkstart, *chunk, *endptr, *endbuf;
		chunk = endbuf = chunkstart = h->req_buf + h->req_contentoff;

		while( (h->req_chunklen = strtol(chunk, &endptr, 16)) && (endptr != chunk) )
		{
			endptr = strstr(endptr, "\r\n");
			if (!endptr)
			{
				Send400(h);
				return;
			}
			endptr += 2;

			memmove(endbuf, endptr, h->req_chunklen);

			endbuf += h->req_chunklen;
			chunk = endptr + h->req_chunklen;
		}
		h->req_contentlen = endbuf - chunkstart;
		h->req_buflen = endbuf - h->req_buf;
		h->state = 100;
	}

	DPRINTF(E_DEBUG, L_HTTP, "HTTP REQUEST: %.*s\n", h->req_buflen, h->req_buf);
	if(strcmp("POST", HttpCommand) == 0)
	{
		h->req_command = EPost;
		ProcessHTTPPOST_upnphttp(h);
	}
	else if((strcmp("GET", HttpCommand) == 0) || (strcmp("HEAD", HttpCommand) == 0))
	{
		if( ((strcmp(h->HttpVer, "HTTP/1.1")==0) && !(h->reqflags & FLAG_HOST)) || (h->reqflags & FLAG_INVALID_REQ) )
		{
			DPRINTF(E_WARN, L_HTTP, "Invalid request, responding ERROR 400.  (No Host specified in HTTP headers?)\n");
			Send400(h);
			return;
		}
		/* 7.3.33.4 */
		else if( (h->reqflags & (FLAG_TIMESEEK|FLAG_PLAYSPEED)) &&
		         !(h->reqflags & FLAG_RANGE) )
		{
			DPRINTF(E_WARN, L_HTTP, "DLNA %s requested, responding ERROR 406\n",
				h->reqflags&FLAG_TIMESEEK ? "TimeSeek" : "PlaySpeed");
			Send406(h);
			return;
		}
		else if(strcmp("GET", HttpCommand) == 0)
		{
			h->req_command = EGet;
		}
		else
		{
			h->req_command = EHead;
		}
		if(strcmp(ROOTDESC_PATH, HttpUrl) == 0)
		{
			/* If it's a Xbox360, we might need a special friendly_name to be recognized */
			if( h->req_client && h->req_client->type->type == EXbox )
			{
				char model_sav[2];
				i = 0;
				memcpy(model_sav, modelnumber, 2);
				strcpy(modelnumber, "1");
				if( !strchr(friendly_name, ':') )
				{
					i = strlen(friendly_name);
					snprintf(friendly_name+i, FRIENDLYNAME_MAX_LEN-i, ": 1");
				}
				sendXMLdesc(h, genRootDesc);
				if( i )
					friendly_name[i] = '\0';
				memcpy(modelnumber, model_sav, 2);
			}
			else if( h->req_client && h->req_client->type->flags & FLAG_SAMSUNG_DCM10 )
			{
				sendXMLdesc(h, genRootDescSamsung);
			}
			else
			{
				sendXMLdesc(h, genRootDesc);
			}
		}
		else if(strcmp(CONTENTDIRECTORY_PATH, HttpUrl) == 0)
		{
			sendXMLdesc(h, genContentDirectory);
		}
		else if(strcmp(CONNECTIONMGR_PATH, HttpUrl) == 0)
		{
			sendXMLdesc(h, genConnectionManager);
		}
		else if(strcmp(X_MS_MEDIARECEIVERREGISTRAR_PATH, HttpUrl) == 0)
		{
			sendXMLdesc(h, genX_MS_MediaReceiverRegistrar);
		}
		else if(strncmp(HttpUrl, "/MediaItems/", 12) == 0)
		{
			SendResp_dlnafile(h, HttpUrl+12);
		}
		else if(strncmp(HttpUrl, "/Thumbnails/", 12) == 0)
		{
			SendResp_thumbnail(h, HttpUrl+12);
		}
		else if(strncmp(HttpUrl, "/AlbumArt/", 10) == 0)
		{
			SendResp_albumArt(h, HttpUrl+10);
		}
		#ifdef TIVO_SUPPORT
		else if(strncmp(HttpUrl, "/TiVoConnect", 12) == 0)
		{
			if( GETFLAG(TIVO_MASK) )
			{
				if( *(HttpUrl+12) == '?' )
				{
					ProcessTiVoCommand(h, HttpUrl+13);
				}
				else
				{
					DPRINTF(E_WARN, L_HTTP, "Invalid TiVo request! %s\n", HttpUrl+12);
					Send404(h);
				}
			}
			else
			{
				DPRINTF(E_WARN, L_HTTP, "TiVo request with out TiVo support enabled! %s\n",
					HttpUrl+12);
				Send404(h);
			}
		}
		#endif
		else if(strncmp(HttpUrl, "/Resized/", 9) == 0)
		{
			SendResp_resizedimg(h, HttpUrl+9);
		}
		else if(strncmp(HttpUrl, "/icons/", 7) == 0)
		{
			SendResp_icon(h, HttpUrl+7);
		}
		else if(strncmp(HttpUrl, "/Captions/", 10) == 0)
		{
			SendResp_caption(h, HttpUrl+10);
		}
		else if(strncmp(HttpUrl, "/status", 7) == 0)
		{
			SendResp_presentation(h);
		}
		else if(strcmp(HttpUrl, "/") == 0)
		{
			#ifdef READYNAS
			SendResp_readynas_admin(h);
			#else
			SendResp_presentation(h);
			#endif
		}
		else
		{
			DPRINTF(E_WARN, L_HTTP, "%s not found, responding ERROR 404\n", HttpUrl);
			Send404(h);
		}
	}
	else if(strcmp("SUBSCRIBE", HttpCommand) == 0)
	{
		h->req_command = ESubscribe;
		ProcessHTTPSubscribe_upnphttp(h, HttpUrl);
	}
	else if(strcmp("UNSUBSCRIBE", HttpCommand) == 0)
	{
		h->req_command = EUnSubscribe;
		ProcessHTTPUnSubscribe_upnphttp(h, HttpUrl);
	}
	else
	{
		DPRINTF(E_WARN, L_HTTP, "Unsupported HTTP Command %s\n", HttpCommand);
		Send501(h);
	}
}


void
Process_upnphttp(struct upnphttp * h)
{
	char buf[2048];
	int n;
	if(!h)
		return;
	switch(h->state)
	{
	case 0:
		n = recv(h->socket, buf, 2048, 0);
		if(n<0)
		{
			DPRINTF(E_ERROR, L_HTTP, "recv (state0): %s\n", strerror(errno));
			h->state = 100;
		}
		else if(n==0)
		{
			DPRINTF(E_WARN, L_HTTP, "HTTP Connection closed unexpectedly\n");
			h->state = 100;
		}
		else
		{
			int new_req_buflen;
			const char * endheaders;
			/* if 1st arg of realloc() is null,
			 * realloc behaves the same as malloc() */
			new_req_buflen = n + h->req_buflen + 1;
			if (new_req_buflen >= 1024 * 1024)
			{
				DPRINTF(E_ERROR, L_HTTP, "Receive headers too large (received %d bytes)\n", new_req_buflen);
				h->state = 100;
				break;
			}
			h->req_buf = (char *)realloc(h->req_buf, new_req_buflen);
			if (!h->req_buf)
			{
				DPRINTF(E_ERROR, L_HTTP, "Receive headers: %s\n", strerror(errno));
				h->state = 100;
				break;
			}
			memcpy(h->req_buf + h->req_buflen, buf, n);
			h->req_buflen += n;
			h->req_buf[h->req_buflen] = '\0';
			/* search for the string "\r\n\r\n" */
			endheaders = strstr(h->req_buf, "\r\n\r\n");
			if(endheaders)
			{
				h->req_contentoff = endheaders - h->req_buf + 4;
				h->req_contentlen = h->req_buflen - h->req_contentoff;
				ProcessHttpQuery_upnphttp(h);
			}
		}
		break;
	case 1:
	case 2:
		n = recv(h->socket, buf, sizeof(buf), 0);
		if(n < 0)
		{
			DPRINTF(E_ERROR, L_HTTP, "recv (state%d): %s\n", h->state, strerror(errno));
			h->state = 100;
		}
		else if(n == 0)
		{
			DPRINTF(E_WARN, L_HTTP, "HTTP Connection closed unexpectedly\n");
			h->state = 100;
		}
		else
		{
			buf[sizeof(buf)-1] = '\0';
			/*fwrite(buf, 1, n, stdout);*/	/* debug */
			h->req_buf = (char *)realloc(h->req_buf, n + h->req_buflen);
			if (!h->req_buf)
			{
				DPRINTF(E_ERROR, L_HTTP, "Receive request body: %s\n", strerror(errno));
				h->state = 100;
				break;
			}
			memcpy(h->req_buf + h->req_buflen, buf, n);
			h->req_buflen += n;
			if((h->req_buflen - h->req_contentoff) >= h->req_contentlen)
			{
				/* Need the struct to point to the realloc'd memory locations */
				if( h->state == 1 )
				{
					ParseHttpHeaders(h);
					ProcessHTTPPOST_upnphttp(h);
				}
				else if( h->state == 2 )
				{
					ProcessHttpQuery_upnphttp(h);
				}
			}
		}
		break;
	default:
		DPRINTF(E_WARN, L_HTTP, "Unexpected state: %d\n", h->state);
	}
}

/* with response code and response message
 * also allocate enough memory */

void
BuildHeader_upnphttp(struct upnphttp * h, int respcode,
                     const char * respmsg,
                     int bodylen)
{
	static const char httpresphead[] =
		"%s %d %s\r\n"
		"Content-Type: %s\r\n"
		"Connection: close\r\n"
		"Content-Length: %d\r\n"
		"Server: " MINIDLNA_SERVER_STRING "\r\n";
	time_t curtime = time(NULL);
	char date[30];
	int templen;
	struct string_s res;
	if(!h->res_buf)
	{
		templen = sizeof(httpresphead) + 256 + bodylen;
		h->res_buf = (char *)malloc(templen);
		h->res_buf_alloclen = templen;
	}
	res.data = h->res_buf;
	res.size = h->res_buf_alloclen;
	res.off = 0;
	strcatf(&res, httpresphead, "HTTP/1.1",
	              respcode, respmsg,
	              (h->respflags&FLAG_HTML)?"text/html":"text/xml; charset=\"utf-8\"",
							 bodylen);
	/* Additional headers */
	if(h->respflags & FLAG_TIMEOUT) {
		strcatf(&res, "Timeout: Second-");
		if(h->req_Timeout) {
			strcatf(&res, "%d\r\n", h->req_Timeout);
		} else {
			strcatf(&res, "300\r\n");
		}
	}
	if(h->respflags & FLAG_SID) {
		strcatf(&res, "SID: %.*s\r\n", h->req_SIDLen, h->req_SID);
	}
	if(h->reqflags & FLAG_LANGUAGE) {
		strcatf(&res, "Content-Language: en\r\n");
	}
	strftime(date, 30,"%a, %d %b %Y %H:%M:%S GMT" , gmtime(&curtime));
	strcatf(&res, "Date: %s\r\n", date);
	strcatf(&res, "EXT:\r\n");
	strcatf(&res, "\r\n");
	h->res_buflen = res.off;
	if(h->res_buf_alloclen < (h->res_buflen + bodylen))
	{
		h->res_buf = (char *)realloc(h->res_buf, (h->res_buflen + bodylen));
		h->res_buf_alloclen = h->res_buflen + bodylen;
	}
}

void
BuildResp2_upnphttp(struct upnphttp * h, int respcode,
                    const char * respmsg,
                    const char * body, int bodylen)
{
	BuildHeader_upnphttp(h, respcode, respmsg, bodylen);
	if( h->req_command == EHead )
		return;
	if(body)
		memcpy(h->res_buf + h->res_buflen, body, bodylen);
	h->res_buflen += bodylen;
}

/* responding 200 OK ! */
void
BuildResp_upnphttp(struct upnphttp *h, const char *body, int bodylen)
{
	BuildResp2_upnphttp(h, 200, "OK", body, bodylen);
}

void
SendResp_upnphttp(struct upnphttp * h)
{
	int n;
	DPRINTF(E_DEBUG, L_HTTP, "HTTP RESPONSE: %.*s\n", h->res_buflen, h->res_buf);
	n = send(h->socket, h->res_buf, h->res_buflen, 0);
	if(n<0)
	{
		DPRINTF(E_ERROR, L_HTTP, "send(res_buf): %s\n", strerror(errno));
	}
	else if(n < h->res_buflen)
	{
		/* TODO : handle correctly this case */
		DPRINTF(E_ERROR, L_HTTP, "send(res_buf): %d bytes sent (out of %d)\n",
						n, h->res_buflen);
	}
}

static int
send_data(struct upnphttp * h, char * header, size_t size, int flags)
{
	int n;

	n = send(h->socket, header, size, flags);
	if(n<0)
	{
		DPRINTF(E_ERROR, L_HTTP, "send(res_buf): %s\n", strerror(errno));
	} 
	else if(n < h->res_buflen)
	{
		/* TODO : handle correctly this case */
		DPRINTF(E_ERROR, L_HTTP, "send(res_buf): %d bytes sent (out of %d)\n",
						n, h->res_buflen);
	}
	else
	{
		return 0;
	}
	return 1;
}

static void
send_file(struct upnphttp * h, int sendfd, off_t offset, off_t end_offset)
{
	off_t send_size;
	off_t ret;
	char *buf = NULL;
#if HAVE_SENDFILE
	int try_sendfile = 1;
#endif

	while( offset <= end_offset )
	{
#if HAVE_SENDFILE
		if( try_sendfile )
		{
			send_size = ( ((end_offset - offset) < MAX_BUFFER_SIZE) ? (end_offset - offset + 1) : MAX_BUFFER_SIZE);
			ret = sys_sendfile(h->socket, sendfd, &offset, send_size);
			if( ret == -1 )
			{
				DPRINTF(E_DEBUG, L_HTTP, "sendfile error :: error no. %d [%s]\n", errno, strerror(errno));
				/* If sendfile isn't supported on the filesystem, don't bother trying to use it again. */
				if( errno == EOVERFLOW || errno == EINVAL )
					try_sendfile = 0;
				else if( errno != EAGAIN )
					break;
			}
			else
			{
				//DPRINTF(E_DEBUG, L_HTTP, "sent %lld bytes to %d. offset is now %lld.\n", ret, h->socket, offset);
				continue;
			}
		}
#endif
		/* Fall back to regular I/O */
		if( !buf )
			buf = malloc(MIN_BUFFER_SIZE);
		send_size = (((end_offset - offset) < MIN_BUFFER_SIZE) ? (end_offset - offset + 1) : MIN_BUFFER_SIZE);
		lseek(sendfd, offset, SEEK_SET);
		ret = read(sendfd, buf, send_size);
		if( ret == -1 ) {
			DPRINTF(E_DEBUG, L_HTTP, "read error :: error no. %d [%s]\n", errno, strerror(errno));
			if( errno == EAGAIN )
				continue;
			else
				break;
		}
		ret = write(h->socket, buf, ret);
		if( ret == -1 ) {
			DPRINTF(E_DEBUG, L_HTTP, "write error :: error no. %d [%s]\n", errno, strerror(errno));
			if( errno == EAGAIN )
				continue;
			else
				break;
		}
		offset += ret;
	}
	free(buf);
}

static void
start_dlna_header(struct string_s *str, int respcode, const char *tmode, const char *mime)
{
	char date[30];
	time_t now;

	now = time(NULL);
	strftime(date, sizeof(date),"%a, %d %b %Y %H:%M:%S GMT" , gmtime(&now));
	strcatf(str, "HTTP/1.1 %d OK\r\n"
	             "Connection: close\r\n"
	             "Date: %s\r\n"
	             "Server: " MINIDLNA_SERVER_STRING "\r\n"
	             "EXT:\r\n"
	             "realTimeInfo.dlna.org: DLNA.ORG_TLAG=*\r\n"
	             "transferMode.dlna.org: %s\r\n"
	             "Content-Type: %s\r\n",
	             respcode, date, tmode, mime);
}

#include <shutils.h>
#if (!defined(RTN66U) && !defined(RTN56U))
extern unsigned char buf_png_sm[];
extern unsigned char buf_png_lrg[];
extern unsigned char buf_jpeg_sm[];
extern unsigned char buf_jpeg_lrg[];
extern int size_png_sm;
extern int size_png_lrg;
extern int size_jpeg_sm;
extern int size_jpeg_lrg;
#endif

static void
SendResp_icon(struct upnphttp * h, char * icon)
{
	char header[512];
	char mime[12] = "image/";
	char *data;
	int size;
	struct string_s str;
#if defined(RTN66U) || defined(RTN56U)
	if( strcmp(icon, "sm.png") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending small PNG icon\n");
		data = (char *)png_sm;
		size = sizeof(png_sm)-1;
		strcpy(mime+6, "png");
	}
	else if( strcmp(icon, "lrg.png") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending large PNG icon\n");
		data = (char *)png_lrg;
		size = sizeof(png_lrg)-1;
		strcpy(mime+6, "png");
	}
	else if( strcmp(icon, "sm.jpg") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending small JPEG icon\n");
		data = (char *)jpeg_sm;
		size = sizeof(jpeg_sm)-1;
		strcpy(mime+6, "jpeg");
	}
	else if( strcmp(icon, "lrg.jpg") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending large JPEG icon\n");
		data = (char *)jpeg_lrg;
		size = sizeof(jpeg_lrg)-1;
		strcpy(mime+6, "jpeg");
	}
#else
	if( strcmp(icon, "sm.png") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending small PNG icon\n");
		data = (char *)buf_png_sm;
		size = size_png_sm;
		strcpy(mime+6, "png");
	}
	else if( strcmp(icon, "lrg.png") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending large PNG icon\n");
		data = (char *)buf_png_lrg;
		size = size_png_lrg;
		strcpy(mime+6, "png");
	}
	else if( strcmp(icon, "sm.jpg") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending small JPEG icon\n");
		data = (char *)buf_jpeg_sm;
		size = size_jpeg_sm;
		strcpy(mime+6, "jpeg");
	}
	else if( strcmp(icon, "lrg.jpg") == 0 )
	{
		DPRINTF(E_DEBUG, L_HTTP, "Sending large JPEG icon\n");
		data = (char *)buf_jpeg_lrg;
		size = size_jpeg_lrg;
		strcpy(mime+6, "jpeg");
	}
#endif
	else
	{
		DPRINTF(E_WARN, L_HTTP, "Invalid icon request: %s\n", icon);
		Send404(h);
		return;
	}

	INIT_STR(str, header);

	start_dlna_header(&str, 200, "Interactive", mime);
	strcatf(&str, "Content-Length: %d\r\n\r\n", size);

	if( send_data(h, str.data, str.off, MSG_MORE) == 0 )
	{
		if( h->req_command != EHead )
			send_data(h, data, size, 0);
	}
	CloseSocket_upnphttp(h);
}

static void
SendResp_albumArt(struct upnphttp * h, char * object)
{
	char header[512];
	char *path;
	off_t size;
	long long id;
	int fd;
	struct string_s str;

	if( h->reqflags & (FLAG_XFERSTREAMING|FLAG_RANGE) )
	{
		DPRINTF(E_WARN, L_HTTP, "Client tried to specify transferMode as Streaming with an image!\n");
		Send406(h);
		return;
	}

	id = strtoll(object, NULL, 10);

	path = sql_get_text_field(db, "SELECT PATH from ALBUM_ART where ID = '%lld'", id);
	if( !path )
	{
		DPRINTF(E_WARN, L_HTTP, "ALBUM_ART ID %s not found, responding ERROR 404\n", object);
		Send404(h);
		return;
	}
	DPRINTF(E_INFO, L_HTTP, "Serving album art ID: %lld [%s]\n", id, path);

	fd = open(path, O_RDONLY);
	if( fd < 0 ) {
		DPRINTF(E_ERROR, L_HTTP, "Error opening %s\n", path);
		sqlite3_free(path);
		Send404(h);
		return;
	}
	sqlite3_free(path);
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	INIT_STR(str, header);

	start_dlna_header(&str, 200, "Interactive", "image/jpeg");
	strcatf(&str, "Content-Length: %jd\r\n"
	              "contentFeatures.dlna.org: DLNA.ORG_PN=JPEG_TN\r\n\r\n",
	              (intmax_t)size);

	if( send_data(h, str.data, str.off, MSG_MORE) == 0 )
	{
		if( h->req_command != EHead )
			send_file(h, fd, 0, size-1);
	}
	close(fd);
	CloseSocket_upnphttp(h);
}

static void
SendResp_caption(struct upnphttp * h, char * object)
{
	char header[512];
	char *path;
	off_t size;
	long long id;
	int fd;
	struct string_s str;

	id = strtoll(object, NULL, 10);

	path = sql_get_text_field(db, "SELECT PATH from CAPTIONS where ID = %lld", id);
	if( !path )
	{
		DPRINTF(E_WARN, L_HTTP, "CAPTION ID %s not found, responding ERROR 404\n", object);
		Send404(h);
		return;
	}
	DPRINTF(E_INFO, L_HTTP, "Serving caption ID: %lld [%s]\n", id, path);

	fd = open(path, O_RDONLY);
	if( fd < 0 ) {
		DPRINTF(E_ERROR, L_HTTP, "Error opening %s\n", path);
		sqlite3_free(path);
		Send404(h);
		return;
	}
	sqlite3_free(path);
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	INIT_STR(str, header);

	start_dlna_header(&str, 200, "Interactive", "smi/caption");
	strcatf(&str, "Content-Length: %jd\r\n\r\n", (intmax_t)size);

	if( send_data(h, str.data, str.off, MSG_MORE) == 0 )
	{
		if( h->req_command != EHead )
			send_file(h, fd, 0, size-1);
	}
	close(fd);
	CloseSocket_upnphttp(h);
}

static void
SendResp_thumbnail(struct upnphttp * h, char * object)
{
	char header[512];
	char *path;
	long long id;
	ExifData *ed;
	ExifLoader *l;
	struct string_s str;

	if( h->reqflags & (FLAG_XFERSTREAMING|FLAG_RANGE) )
	{
		DPRINTF(E_WARN, L_HTTP, "Client tried to specify transferMode as Streaming with an image!\n");
		Send406(h);
		return;
	}

	id = strtoll(object, NULL, 10);
	path = sql_get_text_field(db, "SELECT PATH from DETAILS where ID = '%lld'", id);
	if( !path )
	{
		DPRINTF(E_WARN, L_HTTP, "DETAIL ID %s not found, responding ERROR 404\n", object);
		Send404(h);
		return;
	}
	DPRINTF(E_INFO, L_HTTP, "Serving thumbnail for ObjectId: %lld [%s]\n", id, path);

	if( access(path, F_OK) != 0 )
	{
		DPRINTF(E_ERROR, L_HTTP, "Error accessing %s\n", path);
		Send404(h);
		sqlite3_free(path);
		return;
	}

	l = exif_loader_new();
	exif_loader_write_file(l, path);
	ed = exif_loader_get_data(l);
	exif_loader_unref(l);
	sqlite3_free(path);

	if( !ed || !ed->size )
	{
		Send404(h);
		if( ed )
			exif_data_unref(ed);
		return;
	}

	INIT_STR(str, header);

	start_dlna_header(&str, 200, "Interactive", "image/jpeg");
	strcatf(&str, "Content-Length: %jd\r\n"
	              "contentFeatures.dlna.org: DLNA.ORG_PN=JPEG_TN;DLNA.ORG_CI=1\r\n\r\n",
	              (intmax_t)ed->size);

	if( send_data(h, str.data, str.off, MSG_MORE) == 0 )
	{
		if( h->req_command != EHead )
			send_data(h, (char *)ed->data, ed->size, 0);
	}
	exif_data_unref(ed);
	CloseSocket_upnphttp(h);
}

static void
SendResp_resizedimg(struct upnphttp * h, char * object)
{
	char header[512];
	char buf[128];
	struct string_s str;
	char **result;
	char dlna_pn[22];
	uint32_t dlna_flags = DLNA_FLAG_DLNA_V1_5|DLNA_FLAG_HTTP_STALLING|DLNA_FLAG_TM_B|DLNA_FLAG_TM_I;
	int width=640, height=480, dstw, dsth, size;
	int srcw, srch;
	unsigned char * data = NULL;
	char *path, *file_path = NULL;
	char *resolution = NULL;
	char *key, *val;
	char *saveptr, *item = NULL;
	int rotate = 0;
	int pixw = 0, pixh = 0;
	long long id;
	int rows=0, chunked, ret;
	image_s *imsrc = NULL, *imdst = NULL;
	int scale = 1;
	const char *tmode;

	id = strtoll(object, &saveptr, 10);
	snprintf(buf, sizeof(buf), "SELECT PATH, RESOLUTION, ROTATION from DETAILS where ID = '%lld'", (long long)id);
	ret = sql_get_table(db, buf, &result, &rows, NULL);
	if( ret != SQLITE_OK )
	{
		Send500(h);
		return;
	}
	if( rows )
	{
		file_path = result[3];
		resolution = result[4];
		rotate = result[5] ? atoi(result[5]) : 0;
	}
	if( !file_path || !resolution || (access(file_path, F_OK) != 0) )
	{
		DPRINTF(E_WARN, L_HTTP, "%s not found, responding ERROR 404\n", object);
		sqlite3_free_table(result);
		Send404(h);
		return;
	}

	if( saveptr )
		saveptr = strchr(saveptr, '?');
	path = saveptr ? saveptr + 1 : object;
	for( item = strtok_r(path, "&,", &saveptr); item != NULL; item = strtok_r(NULL, "&,", &saveptr) )
	{
		decodeString(item, 1);
		val = item;
		key = strsep(&val, "=");
		if( !val )
			continue;
		DPRINTF(E_DEBUG, L_GENERAL, "%s: %s\n", key, val);
		if( strcasecmp(key, "width") == 0 )
		{
			width = atoi(val);
		}
		else if( strcasecmp(key, "height") == 0 )
		{
			height = atoi(val);
		}
		else if( strcasecmp(key, "rotation") == 0 )
		{
			rotate = (rotate + atoi(val)) % 360;
			sql_exec(db, "UPDATE DETAILS set ROTATION = %d where ID = %lld", rotate, id);
		}
		else if( strcasecmp(key, "pixelshape") == 0 )
		{
			ret = sscanf(val, "%d:%d", &pixw, &pixh);
			if( ret != 2 )
				pixw = pixh = 0;
		}
	}

#if USE_FORK
	pid_t newpid = 0;
	newpid = process_fork(h->req_client);
	if( newpid > 0 )
	{
		CloseSocket_upnphttp(h);
		goto resized_error;
	}
#endif
	if( h->reqflags & (FLAG_XFERSTREAMING|FLAG_RANGE) )
	{
		DPRINTF(E_WARN, L_HTTP, "Client tried to specify transferMode as Streaming with an image!\n");
		Send406(h);
		goto resized_error;
	}

	DPRINTF(E_INFO, L_HTTP, "Serving resized image for ObjectId: %lld [%s]\n", id, file_path);
	if( rotate )
		DPRINTF(E_DEBUG, L_HTTP, "Rotating image %d degrees\n", rotate);
	switch( rotate )
	{
		case 90:
			ret = sscanf(resolution, "%dx%d", &srch, &srcw);
			rotate = ROTATE_90;
			break;
		case 270:
			ret = sscanf(resolution, "%dx%d", &srch, &srcw);
			rotate = ROTATE_270;
			break;
		case 180:
			ret = sscanf(resolution, "%dx%d", &srcw, &srch);
			rotate = ROTATE_180;
			break;
		default:
			ret = sscanf(resolution, "%dx%d", &srcw, &srch);
			rotate = ROTATE_NONE;
			break;
	}
	if( ret != 2 )
	{
		Send500(h);
		return;
	}
	/* Figure out the best destination resolution we can use */
	dstw = width;
	dsth = ((((width<<10)/srcw)*srch)>>10);
	if( dsth > height )
	{
		dsth = height;
		dstw = (((height<<10)/srch) * srcw>>10);
	}
	/* Account for pixel shape */
	if( pixw && pixh )
	{
		if( pixh > pixw )
			dsth = dsth * pixw / pixh;
		else if( pixw > pixh )
			dstw = dstw * pixh / pixw;
	}

	if( dstw <= 160 && dsth <= 160 )
		strcpy(dlna_pn, "DLNA.ORG_PN=JPEG_TN;");
	else if( dstw <= 640 && dsth <= 480 )
		strcpy(dlna_pn, "DLNA.ORG_PN=JPEG_SM;");
	else if( dstw <= 1024 && dsth <= 768 )
		strcpy(dlna_pn, "DLNA.ORG_PN=JPEG_MED;");
	else
		strcpy(dlna_pn, "DLNA.ORG_PN=JPEG_LRG;");

	if( srcw>>4 >= dstw && srch>>4 >= dsth)
		scale = 8;
	else if( srcw>>3 >= dstw && srch>>3 >= dsth )
		scale = 4;
	else if( srcw>>2 >= dstw && srch>>2 >= dsth )
		scale = 2;

	INIT_STR(str, header);

#if USE_FORK
	if( (h->reqflags & FLAG_XFERBACKGROUND) && (setpriority(PRIO_PROCESS, 0, 19) == 0) )
		tmode = "Background";
	else
#endif
		tmode = "Interactive";
	start_dlna_header(&str, 200, tmode, "image/jpeg");
	strcatf(&str, "contentFeatures.dlna.org: %sDLNA.ORG_CI=1;DLNA.ORG_FLAGS=%08X%024X\r\n",
	              dlna_pn, dlna_flags, 0);

	if( strcmp(h->HttpVer, "HTTP/1.0") == 0 )
	{
		chunked = 0;
		imsrc = image_new_from_jpeg(file_path, 1, NULL, 0, scale, rotate);
	}
	else
	{
		chunked = 1;
		strcatf(&str, "Transfer-Encoding: chunked\r\n\r\n");
	}

	if( !chunked )
	{
		if( !imsrc )
		{
			DPRINTF(E_WARN, L_HTTP, "Unable to open image %s!\n", file_path);
			Send500(h);
			goto resized_error;
		}

		imdst = image_resize(imsrc, dstw, dsth);
		data = image_save_to_jpeg_buf(imdst, &size);

		strcatf(&str, "Content-Length: %d\r\n\r\n", size);
	}

	if( (send_data(h, str.data, str.off, 0) == 0) && (h->req_command != EHead) )
	{
		if( chunked )
		{
			imsrc = image_new_from_jpeg(file_path, 1, NULL, 0, scale, rotate);
			if( !imsrc )
			{
				DPRINTF(E_WARN, L_HTTP, "Unable to open image %s!\n", file_path);
				Send500(h);
				goto resized_error;
			}
			imdst = image_resize(imsrc, dstw, dsth);
			data = image_save_to_jpeg_buf(imdst, &size);

			ret = sprintf(buf, "%x\r\n", size);
			send_data(h, buf, ret, MSG_MORE);
			send_data(h, (char *)data, size, MSG_MORE);
			send_data(h, "\r\n0\r\n\r\n", 7, 0);
		}
		else
		{
			send_data(h, (char *)data, size, 0);
		}
	}
	DPRINTF(E_INFO, L_HTTP, "Done serving %s\n", file_path);
	if( imsrc )
		image_free(imsrc);
	if( imdst )
		image_free(imdst);
	CloseSocket_upnphttp(h);
resized_error:
	sqlite3_free_table(result);
#if USE_FORK
	if( newpid == 0 )
		_exit(0);
#endif
}

static void
SendResp_dlnafile(struct upnphttp *h, char *object)
{
	char header[1024];
	struct string_s str;
	char buf[128];
	char **result;
	int rows, ret;
	off_t total, offset, size;
	int64_t id;
	int sendfh;
	uint32_t dlna_flags = DLNA_FLAG_DLNA_V1_5|DLNA_FLAG_HTTP_STALLING|DLNA_FLAG_TM_B;
	uint32_t cflags = h->req_client ? h->req_client->type->flags : 0;
	const char *tmode;
	enum client_types ctype = h->req_client ? h->req_client->type->type : 0;
	static struct { int64_t id;
	                enum client_types client;
	                char path[PATH_MAX];
	                char mime[32];
	                char dlna[96];
	              } last_file = { 0, 0 };
#if USE_FORK
	pid_t newpid = 0;
#endif

	id = strtoll(object, NULL, 10);
	if( cflags & FLAG_MS_PFS )
	{
		if( strstr(object, "?albumArt=true") )
		{
			char *art;
			art = sql_get_text_field(db, "SELECT ALBUM_ART from DETAILS where ID = '%lld'", id);
			if (art)
			{
				SendResp_albumArt(h, art);
				sqlite3_free(art);
			}
			else
				Send404(h);
			return;
		}
	}
	if( id != last_file.id || ctype != last_file.client )
	{
		snprintf(buf, sizeof(buf), "SELECT PATH, MIME, DLNA_PN from DETAILS where ID = '%lld'", (long long)id);
		ret = sql_get_table(db, buf, &result, &rows, NULL);
		if( (ret != SQLITE_OK) )
		{
			DPRINTF(E_ERROR, L_HTTP, "Didn't find valid file for %lld!\n", (long long)id);
			Send500(h);
			return;
		}
		if( !rows || !result[3] || !result[4] )
		{
			DPRINTF(E_WARN, L_HTTP, "%s not found, responding ERROR 404\n", object);
			sqlite3_free_table(result);
			Send404(h);
			return;
		}
		/* Cache the result */
		last_file.id = id;
		last_file.client = ctype;
		strncpy(last_file.path, result[3], sizeof(last_file.path)-1);
		if( result[4] )
		{
			strncpy(last_file.mime, result[4], sizeof(last_file.mime)-1);
			/* From what I read, Samsung TV's expect a [wrong] MIME type of x-mkv. */
			if( cflags & FLAG_SAMSUNG )
			{
				if( strcmp(last_file.mime+6, "x-matroska") == 0 )
					strcpy(last_file.mime+8, "mkv");
				/* Samsung TV's such as the A750 can natively support many
				   Xvid/DivX AVI's however, the DLNA server needs the 
				   mime type to say video/mpeg */
				else if( ctype == ESamsungSeriesA && strcmp(last_file.mime+6, "x-msvideo") == 0 )
					strcpy(last_file.mime+6, "mpeg");
			}
			/* ... and Sony BDP-S370 won't play MKV unless we pretend it's a DiVX file */
			else if( ctype == ESonyBDP )
			{
				if( strcmp(last_file.mime+6, "x-matroska") == 0 ||
				    strcmp(last_file.mime+6, "mpeg") == 0 )
					strcpy(last_file.mime+6, "divx");
			}
		}
		if( result[5] )
			snprintf(last_file.dlna, sizeof(last_file.dlna), "DLNA.ORG_PN=%s;", result[5]);
		else
			last_file.dlna[0] = '\0';
		sqlite3_free_table(result);
	}
#if USE_FORK
	newpid = process_fork(h->req_client);
	if( newpid > 0 )
	{
		CloseSocket_upnphttp(h);
		return;
	}
#endif

	DPRINTF(E_INFO, L_HTTP, "Serving DetailID: %lld [%s]\n", (long long)id, last_file.path);

	if( h->reqflags & FLAG_XFERSTREAMING )
	{
		if( strncmp(last_file.mime, "image", 5) == 0 )
		{
			DPRINTF(E_WARN, L_HTTP, "Client tried to specify transferMode as Streaming with an image!\n");
			Send406(h);
			goto error;
		}
	}
	else if( h->reqflags & FLAG_XFERINTERACTIVE )
	{
		if( h->reqflags & FLAG_REALTIMEINFO )
		{
			DPRINTF(E_WARN, L_HTTP, "Bad realTimeInfo flag with Interactive request!\n");
			Send400(h);
			goto error;
		}
		if( strncmp(last_file.mime, "image", 5) != 0 )
		{
			DPRINTF(E_WARN, L_HTTP, "Client tried to specify transferMode as Interactive without an image!\n");
			/* Samsung TVs (well, at least the A950) do this for some reason,
			 * and I don't see them fixing this bug any time soon. */
			if( !(cflags & FLAG_SAMSUNG) || GETFLAG(DLNA_STRICT_MASK) )
			{
				Send406(h);
				goto error;
			}
		}
	}

	offset = h->req_RangeStart;
	sendfh = open(last_file.path, O_RDONLY);
	if( sendfh < 0 ) {
		DPRINTF(E_ERROR, L_HTTP, "Error opening %s\n", last_file.path);
		Send404(h);
		goto error;
	}
	size = lseek(sendfh, 0, SEEK_END);
	lseek(sendfh, 0, SEEK_SET);

	INIT_STR(str, header);

#if USE_FORK
	if( (h->reqflags & FLAG_XFERBACKGROUND) && (setpriority(PRIO_PROCESS, 0, 19) == 0) )
		tmode = "Background";
	else
#endif
	if( strncmp(last_file.mime, "image", 5) == 0 )
		tmode = "Interactive";
	else
		tmode = "Streaming";

	start_dlna_header(&str, (h->reqflags & FLAG_RANGE ? 206 : 200), tmode, last_file.mime);

	if( h->reqflags & FLAG_RANGE )
	{
		if( !h->req_RangeEnd || h->req_RangeEnd == size )
		{
			h->req_RangeEnd = size - 1;
		}
		if( (h->req_RangeStart > h->req_RangeEnd) || (h->req_RangeStart < 0) )
		{
			DPRINTF(E_WARN, L_HTTP, "Specified range was invalid!\n");
			Send400(h);
			close(sendfh);
			goto error;
		}
		if( h->req_RangeEnd >= size )
		{
			DPRINTF(E_WARN, L_HTTP, "Specified range was outside file boundaries!\n");
			Send416(h);
			close(sendfh);
			goto error;
		}

		total = h->req_RangeEnd - h->req_RangeStart + 1;
		strcatf(&str, "Content-Length: %jd\r\n"
		              "Content-Range: bytes %jd-%jd/%jd\r\n",
		              (intmax_t)total, (intmax_t)h->req_RangeStart,
		              (intmax_t)h->req_RangeEnd, (intmax_t)size);
	}
	else
	{
		h->req_RangeEnd = size - 1;
		total = size;
		strcatf(&str, "Content-Length: %jd\r\n", (intmax_t)total);
	}

	switch( *last_file.mime )
	{
		case 'i':
			dlna_flags |= DLNA_FLAG_TM_I;
			break;
		case 'a':
		case 'v':
		default:
			dlna_flags |= DLNA_FLAG_TM_S;
			break;
	}

	if( h->reqflags & FLAG_CAPTION )
	{
		if( sql_get_int_field(db, "SELECT ID from CAPTIONS where ID = '%lld'", (long long)id) > 0 )
			strcatf(&str, "CaptionInfo.sec: http://%s:%d/Captions/%lld.srt\r\n",
			              lan_addr[h->iface].str, runtime_vars.port, (long long)id);
	}

	strcatf(&str, "Accept-Ranges: bytes\r\n"
	              "contentFeatures.dlna.org: %sDLNA.ORG_OP=%02X;DLNA.ORG_CI=%X;DLNA.ORG_FLAGS=%08X%024X\r\n\r\n",
	              last_file.dlna, 1, 0, dlna_flags, 0);

	//DEBUG DPRINTF(E_DEBUG, L_HTTP, "RESPONSE: %s\n", str.data);
	if( send_data(h, str.data, str.off, MSG_MORE) == 0 )
	{
		if( h->req_command != EHead )
			send_file(h, sendfh, offset, h->req_RangeEnd);
	}
	close(sendfh);

	CloseSocket_upnphttp(h);
error:
#if USE_FORK
	if( newpid == 0 )
		_exit(0);
#endif
	return;
}
