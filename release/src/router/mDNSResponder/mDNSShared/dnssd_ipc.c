/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004, Apple Computer, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	Change History (most recent first):

$Log: dnssd_ipc.c,v $
Revision 1.23  2009/04/01 21:10:34  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows

Revision 1.22  2009/02/12 20:28:31  cheshire
Added some missing "const" declarations

Revision 1.21  2008/10/23 23:21:31  cheshire
Moved definition of dnssd_strerror() to be with the definition of dnssd_errno, in dnssd_ipc.h

Revision 1.20  2007/07/23 22:12:53  cheshire
<rdar://problem/5352299> Make mDNSResponder more defensive against malicious local clients

Revision 1.19  2007/05/16 01:06:52  cheshire
<rdar://problem/4471320> Improve reliability of kDNSServiceFlagsMoreComing flag on multiprocessor machines

Revision 1.18  2007/03/21 19:01:57  cheshire
<rdar://problem/5078494> IPC code not 64-bit-savvy: assumes long=32bits, and short=16bits

Revision 1.17  2006/10/27 00:38:22  cheshire
Strip accidental trailing whitespace from lines

Revision 1.16  2006/08/14 23:05:53  cheshire
Added "tab-width" emacs header line

Revision 1.15  2005/01/27 22:57:56  cheshire
Fix compile errors on gcc4

Revision 1.14  2004/10/06 02:22:20  cheshire
Changed MacRoman copyright symbol (should have been UTF-8 in any case :-) to ASCII-compatible "(c)"

Revision 1.13  2004/10/01 22:15:55  rpantos
rdar://problem/3824265: Replace APSL in client lib with BSD license.

Revision 1.12  2004/09/16 23:14:24  cheshire
Changes for Windows compatibility

Revision 1.11  2004/06/18 04:56:09  rpantos
casting goodness

Revision 1.10  2004/06/12 01:08:14  cheshire
Changes for Windows compatibility

Revision 1.9  2004/05/18 23:51:27  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.8  2003/11/05 22:44:57  ksekar
<rdar://problem/3335230>: No bounds checking when reading data from client
Reviewed by: Stuart Cheshire

Revision 1.7  2003/08/12 19:56:25  cheshire
Update to APSL 2.0

 */

#include "dnssd_ipc.h"

#if defined(_WIN32)

char *win32_strerror(int inErrorCode)
	{
	static char buffer[1024];
	DWORD       n;
	memset(buffer, 0, sizeof(buffer));
	n = FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			(DWORD) inErrorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			buffer,
			sizeof(buffer),
			NULL);
	if (n > 0)
		{
		// Remove any trailing CR's or LF's since some messages have them.
		while ((n > 0) && isspace(((unsigned char *) buffer)[n - 1]))
			buffer[--n] = '\0';
		}
	return buffer;
	}

#endif

void put_uint32(const uint32_t l, char **ptr)
	{
	(*ptr)[0] = (char)((l >> 24) &  0xFF);
	(*ptr)[1] = (char)((l >> 16) &  0xFF);
	(*ptr)[2] = (char)((l >>  8) &  0xFF);
	(*ptr)[3] = (char)((l      ) &  0xFF);
	*ptr += sizeof(uint32_t);
	}

uint32_t get_uint32(const char **ptr, const char *end)
	{
	if (!*ptr || *ptr + sizeof(uint32_t) > end)
		{
		*ptr = NULL;
		return(0);
		}
	else
		{
		uint8_t *p = (uint8_t*) *ptr;
		*ptr += sizeof(uint32_t);
		return((uint32_t) ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]));
		}
	}

void put_uint16(uint16_t s, char **ptr)
	{
	(*ptr)[0] = (char)((s >>  8) &  0xFF);
	(*ptr)[1] = (char)((s      ) &  0xFF);
	*ptr += sizeof(uint16_t);
	}

uint16_t get_uint16(const char **ptr, const char *end)
	{
	if (!*ptr || *ptr + sizeof(uint16_t) > end)
		{
		*ptr = NULL;
		return(0);
		}
	else
		{
		uint8_t *p = (uint8_t*) *ptr;
		*ptr += sizeof(uint16_t);
		return((uint16_t) ((uint16_t)p[0] << 8 | p[1]));
		}
	}

int put_string(const char *str, char **ptr)
	{
	if (!str) str = "";
	strcpy(*ptr, str);
	*ptr += strlen(str) + 1;
	return 0;
	}

int get_string(const char **ptr, const char *const end, char *buffer, int buflen)
	{
	if (!*ptr)
		{
		*buffer = 0;
		return(-1);
		}
	else
		{
		char *lim = buffer + buflen;	// Calculate limit
		while (*ptr < end && buffer < lim)
			{
			char c = *buffer++ = *(*ptr)++;
			if (c == 0) return(0);		// Success
			}
		if (buffer == lim) buffer--;
		*buffer = 0;					// Failed, so terminate string,
		*ptr = NULL;					// clear pointer,
		return(-1);						// and return failure indication
		}
	}

void put_rdata(const int rdlen, const unsigned char *rdata, char **ptr)
	{
	memcpy(*ptr, rdata, rdlen);
	*ptr += rdlen;
	}

const char *get_rdata(const char **ptr, const char *end, int rdlen)
	{
	if (!*ptr || *ptr + rdlen > end)
		{
		*ptr = NULL;
		return(0);
		}
	else
		{
		const char *rd = *ptr;
		*ptr += rdlen;
		return rd;
		}
	}

void ConvertHeaderBytes(ipc_msg_hdr *hdr)
	{
	hdr->version   = htonl(hdr->version);
	hdr->datalen   = htonl(hdr->datalen);
	hdr->ipc_flags = htonl(hdr->ipc_flags);
	hdr->op        = htonl(hdr->op );
	hdr->reg_index = htonl(hdr->reg_index);
	}
