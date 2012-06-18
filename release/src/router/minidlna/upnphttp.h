/* MiniDLNA project
 *
 * http://sourceforge.net/projects/minidlna/
 *
 * MiniDLNA media server
 * Copyright (C) 2008-2012  Justin Maggard
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
#ifndef __UPNPHTTP_H__
#define __UPNPHTTP_H__

#include <netinet/in.h>
#include <sys/queue.h>

#include "minidlnatypes.h"
#include "config.h"

/* server: HTTP header returned in all HTTP responses : */
#define MINIDLNA_SERVER_STRING	OS_VERSION " DLNADOC/1.50 UPnP/1.0 " SERVER_NAME "/" MINIDLNA_VERSION

/*
 states :
  0 - waiting for data to read
  1 - waiting for HTTP Post Content.
  ...
  >= 100 - to be deleted
*/
enum httpCommands {
	EUnknown = 0,
	EGet,
	EPost,
	EHead,
	ESubscribe,
	EUnSubscribe
};

struct upnphttp {
	int socket;
	struct in_addr clientaddr;	/* client address */
	int iface;
	int state;
	char HttpVer[16];
	/* request */
	char * req_buf;
	int req_buflen;
	int req_contentlen;
	int req_contentoff;     /* header length */
	enum httpCommands req_command;
	enum client_types req_client;
	const char * req_soapAction;
	int req_soapActionLen;
	const char * req_Callback;	/* For SUBSCRIBE */
	int req_CallbackLen;
	const char * req_NT;
	int req_NTLen;
	int req_Timeout;
	const char * req_SID;		/* For UNSUBSCRIBE */
	int req_SIDLen;
	off_t req_RangeStart;
	off_t req_RangeEnd;
	long int req_chunklen;
	uint32_t reqflags;
	/* response */
	char * res_buf;
	int res_buflen;
	int res_buf_alloclen;
	uint32_t respflags;
	/*int res_contentlen;*/
	/*int res_contentoff;*/		/* header length */
	LIST_ENTRY(upnphttp) entries;
};

#define FLAG_TIMEOUT            0x00000001
#define FLAG_SID                0x00000002
#define FLAG_RANGE              0x00000004
#define FLAG_HOST               0x00000008
#define FLAG_LANGUAGE           0x00000010

#define FLAG_INVALID_REQ        0x00000040
#define FLAG_HTML               0x00000080

#define FLAG_CHUNKED            0x00000100
#define FLAG_TIMESEEK           0x00000200
#define FLAG_REALTIMEINFO       0x00000400
#define FLAG_PLAYSPEED          0x00000800
#define FLAG_XFERSTREAMING      0x00001000
#define FLAG_XFERINTERACTIVE    0x00002000
#define FLAG_XFERBACKGROUND     0x00004000
#define FLAG_CAPTION            0x00008000

#define FLAG_DLNA               0x00100000
#define FLAG_MIME_AVI_DIVX      0x00200000
#define FLAG_MIME_AVI_AVI       0x00400000
#define FLAG_MIME_FLAC_FLAC     0x00800000
#define FLAG_MIME_WAV_WAV       0x01000000
#define FLAG_NO_RESIZE          0x02000000
#define FLAG_MS_PFS             0x04000000 // Microsoft PlaysForSure client
#define FLAG_SAMSUNG            0x08000000
#define FLAG_SAMSUNG_TV         0x10000000
#define FLAG_AUDIO_ONLY         0x20000000

#define FLAG_FREE_OBJECT_ID     0x00000001
#define FLAG_ROOT_CONTAINER     0x00000002

/* New_upnphttp() */
struct upnphttp *
New_upnphttp(int);

/* CloseSocket_upnphttp() */
void
CloseSocket_upnphttp(struct upnphttp *);

/* Delete_upnphttp() */
void
Delete_upnphttp(struct upnphttp *);

/* Process_upnphttp() */
void
Process_upnphttp(struct upnphttp *);

/* BuildHeader_upnphttp()
 * build the header for the HTTP Response
 * also allocate the buffer for body data */
void
BuildHeader_upnphttp(struct upnphttp * h, int respcode,
                     const char * respmsg,
                     int bodylen);

/* BuildResp_upnphttp() 
 * fill the res_buf buffer with the complete
 * HTTP 200 OK response from the body passed as argument */
void
BuildResp_upnphttp(struct upnphttp *, const char *, int);

/* BuildResp2_upnphttp()
 * same but with given response code/message */
void
BuildResp2_upnphttp(struct upnphttp * h, int respcode,
                    const char * respmsg,
                    const char * body, int bodylen);

/* Error messages */
void
Send500(struct upnphttp *);
void
Send501(struct upnphttp *);

/* SendResp_upnphttp() */
void
SendResp_upnphttp(struct upnphttp *);

int
SearchClientCache(struct in_addr addr, int quiet);

void
SendResp_icon(struct upnphttp *, char * url);
void
SendResp_albumArt(struct upnphttp *, char * url);
void
SendResp_caption(struct upnphttp *, char * url);
void
SendResp_resizedimg(struct upnphttp *, char * url);
void
SendResp_thumbnail(struct upnphttp *, char * url);
/* SendResp_dlnafile()
 * send the actual file data for a UPnP-A/V or DLNA request. */
void
SendResp_dlnafile(struct upnphttp *, char * url);
#endif

