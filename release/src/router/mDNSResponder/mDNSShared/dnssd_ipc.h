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

$Log: dnssd_ipc.h,v $
Revision 1.46  2009/05/27 22:20:44  cheshire
Removed unused dnssd_errno_assign() (we have no business writing to errno -- we should only read it)

Revision 1.45  2009/05/26 21:31:07  herscher
Fix compile errors on Windows

Revision 1.44  2009/02/12 20:28:31  cheshire
Added some missing "const" declarations

Revision 1.43  2008/10/23 23:21:31  cheshire
Moved definition of dnssd_strerror() to be with the definition of dnssd_errno, in dnssd_ipc.h

Revision 1.42  2008/10/23 23:06:17  cheshire
Removed () from dnssd_errno macro definition -- it's not a function and doesn't need any arguments

Revision 1.41  2008/09/27 01:04:09  cheshire
Added "send_bpf" to list of request_op_t operation codes

Revision 1.40  2007/09/07 20:56:03  cheshire
Renamed uint32_t field in client_context_t from "ptr64" to more accurate name "u32"

Revision 1.39  2007/08/18 01:02:04  mcguire
<rdar://problem/5415593> No Bonjour services are getting registered at boot

Revision 1.38  2007/08/08 22:34:59  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root

Revision 1.37  2007/07/28 00:00:43  cheshire
Renamed CompileTimeAssertionCheck structure for consistency with others

Revision 1.36  2007/07/23 22:12:53  cheshire
<rdar://problem/5352299> Make mDNSResponder more defensive against malicious local clients

Revision 1.35  2007/05/23 18:59:22  cheshire
Remove unnecessary IPC_FLAGS_REUSE_SOCKET

Revision 1.34  2007/05/22 01:07:42  cheshire
<rdar://problem/3563675> API: Need a way to get version/feature information

Revision 1.33  2007/05/18 23:55:22  cheshire
<rdar://problem/4454655> Allow multiple register/browse/resolve operations to share single Unix Domain Socket

Revision 1.32  2007/05/18 20:31:20  cheshire
Rename port_mapping_create_request to port_mapping_request

Revision 1.31  2007/05/18 17:56:20  cheshire
Rename port_mapping_create_reply_op to port_mapping_reply_op

Revision 1.30  2007/05/16 01:06:52  cheshire
<rdar://problem/4471320> Improve reliability of kDNSServiceFlagsMoreComing flag on multiprocessor machines

Revision 1.29  2007/05/15 21:57:16  cheshire
<rdar://problem/4608220> Use dnssd_SocketValid(x) macro instead of just
assuming that all negative values (or zero!) are invalid socket numbers

Revision 1.28  2007/03/21 19:01:57  cheshire
<rdar://problem/5078494> IPC code not 64-bit-savvy: assumes long=32bits, and short=16bits

Revision 1.27  2006/10/27 00:38:22  cheshire
Strip accidental trailing whitespace from lines

Revision 1.26  2006/09/27 00:44:36  herscher
<rdar://problem/4249761> API: Need DNSServiceGetAddrInfo()

Revision 1.25  2006/09/26 01:51:07  herscher
<rdar://problem/4245016> NAT Port Mapping API (for both NAT-PMP and UPnP Gateway Protocol)

Revision 1.24  2006/09/18 19:21:42  cheshire
<rdar://problem/4737048> gcc's structure padding breaks Bonjour APIs on
64-bit clients; need to declare ipc_msg_hdr structure "packed"

Revision 1.23  2006/08/14 23:05:53  cheshire
Added "tab-width" emacs header line

Revision 1.22  2006/06/28 08:56:26  cheshire
Added "_op" to the end of the operation code enum values,
to differentiate them from the routines with the same names

Revision 1.21  2005/09/29 06:38:13  herscher
Remove #define MSG_WAITALL on Windows.  We don't use this macro anymore, and it's presence causes warnings to be emitted when compiling against the latest Microsoft Platform SDK.

Revision 1.20  2005/03/21 00:39:31  shersche
<rdar://problem/4021486> Fix build warnings on Win32 platform

Revision 1.19  2005/02/02 02:25:22  cheshire
<rdar://problem/3980388> /var/run/mDNSResponder should be /var/run/mdnsd on Linux

Revision 1.18  2005/01/27 22:57:56  cheshire
Fix compile errors on gcc4

Revision 1.17  2004/11/23 03:39:47  cheshire
Let interface name/index mapping capability live directly in JNISupport.c,
instead of having to call through to the daemon via IPC to get this information.

Revision 1.16  2004/11/12 03:21:41  rpantos
rdar://problem/3809541 Add DNSSDMapIfIndexToName, DNSSDMapNameToIfIndex.

Revision 1.15  2004/10/06 02:22:20  cheshire
Changed MacRoman copyright symbol (should have been UTF-8 in any case :-) to ASCII-compatible "(c)"

Revision 1.14  2004/10/01 22:15:55  rpantos
rdar://problem/3824265: Replace APSL in client lib with BSD license.

Revision 1.13  2004/09/16 23:14:25  cheshire
Changes for Windows compatibility

Revision 1.12  2004/09/16 21:46:38  ksekar
<rdar://problem/3665304> Need SPI for LoginWindow to associate a UID with a Wide Area domain

Revision 1.11  2004/08/10 06:24:56  cheshire
Use types with precisely defined sizes for 'op' and 'reg_index', for better
compatibility if the daemon and the client stub are built using different compilers

Revision 1.10  2004/07/07 17:39:25  shersche
Change MDNS_SERVERPORT from 5533 to 5354.

Revision 1.9  2004/06/25 00:26:27  rpantos
Changes to fix the Posix build on Solaris.

Revision 1.8  2004/06/18 04:56:51  rpantos
Add layer for platform code

Revision 1.7  2004/06/12 01:08:14  cheshire
Changes for Windows compatibility

Revision 1.6  2003/08/12 19:56:25  cheshire
Update to APSL 2.0

 */

#ifndef DNSSD_IPC_H
#define DNSSD_IPC_H

#include "dns_sd.h"

//
// Common cross platform services
//
#if defined(WIN32)
#	include <winsock2.h>
#	define dnssd_InvalidSocket	INVALID_SOCKET
#	define dnssd_SocketValid(s) ((s) != INVALID_SOCKET)
#	define dnssd_EWOULDBLOCK	WSAEWOULDBLOCK
#	define dnssd_EINTR			WSAEINTR
#	define dnssd_sock_t			SOCKET
#	define dnssd_socklen_t		int
#	define dnssd_close(sock)	closesocket(sock)
#	define dnssd_errno			WSAGetLastError()
#	define dnssd_strerror(X)	win32_strerror(X)
#	define ssize_t				int
#	define getpid				_getpid
#else
#	include <sys/types.h>
#	include <unistd.h>
#	include <sys/un.h>
#	include <string.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <sys/stat.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	define dnssd_InvalidSocket	-1
#	define dnssd_SocketValid(s) ((s) >= 0)
#	define dnssd_EWOULDBLOCK	EWOULDBLOCK
#	define dnssd_EINTR			EINTR
#	define dnssd_EPIPE			EPIPE
#	define dnssd_sock_t			int
#	define dnssd_socklen_t		unsigned int
#	define dnssd_close(sock)	close(sock)
#	define dnssd_errno			errno
#	define dnssd_strerror(X)	strerror(X)
#endif

#if defined(USE_TCP_LOOPBACK)
#	define AF_DNSSD				AF_INET
#	define MDNS_TCP_SERVERADDR	"127.0.0.1"
#	define MDNS_TCP_SERVERPORT	5354
#	define LISTENQ				5
#	define dnssd_sockaddr_t		struct sockaddr_in
#else
#	define AF_DNSSD				AF_LOCAL
#	ifndef MDNS_UDS_SERVERPATH
#		define MDNS_UDS_SERVERPATH	"/var/run/mDNSResponder"
#	endif
#	define LISTENQ				100
    // longest legal control path length
#	define MAX_CTLPATH			256
#	define dnssd_sockaddr_t		struct sockaddr_un
#endif

// Compatibility workaround
#ifndef AF_LOCAL
#define	AF_LOCAL	AF_UNIX
#endif

// General UDS constants
#define TXT_RECORD_INDEX ((uint32_t)(-1))	// record index for default text record

// IPC data encoding constants and types
#define VERSION 1
#define IPC_FLAGS_NOREPLY 1	// set flag if no asynchronous replies are to be sent to client

// Structure packing macro. If we're not using GNUC, it's not fatal. Most compilers naturally pack the on-the-wire
// structures correctly anyway, so a plain "struct" is usually fine. In the event that structures are not packed
// correctly, our compile-time assertion checks will catch it and prevent inadvertent generation of non-working code.
#ifndef packedstruct
 #if ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 9)))
  #define packedstruct struct __attribute__((__packed__))
  #define packedunion  union  __attribute__((__packed__))
 #else
  #define packedstruct struct
  #define packedunion  union
 #endif
#endif

typedef enum
    {
    request_op_none = 0,	// No request yet received on this connection
    connection_request = 1,	// connected socket via DNSServiceConnect()
    reg_record_request,		// reg/remove record only valid for connected sockets
    remove_record_request,
    enumeration_request,
    reg_service_request,
    browse_request,
    resolve_request,
    query_request,
    reconfirm_record_request,
    add_record_request,
    update_record_request,
    setdomain_request,		// Up to here is in Tiger and B4W 1.0.3
	getproperty_request,	// New in B4W 1.0.4
    port_mapping_request,	// New in Leopard and B4W 2.0
	addrinfo_request,
	send_bpf,				// New in SL

	cancel_request = 63
    } request_op_t;

typedef enum
    {
    enumeration_reply_op = 64,
    reg_service_reply_op,
    browse_reply_op,
    resolve_reply_op,
    query_reply_op,
    reg_record_reply_op,	// Up to here is in Tiger and B4W 1.0.3
    getproperty_reply_op,	// New in B4W 1.0.4
    port_mapping_reply_op,	// New in Leopard and B4W 2.0
	addrinfo_reply_op
    } reply_op_t;

#if defined(_WIN64)
#	pragma pack(4)
#endif

// Define context object big enough to hold a 64-bit pointer,
// to accomodate 64-bit clients communicating with 32-bit daemon.
// There's no reason for the daemon to ever be a 64-bit process, but its clients might be
typedef packedunion
    {
    void *context;
    uint32_t u32[2];
    } client_context_t;

typedef packedstruct
    {
    uint32_t version;
    uint32_t datalen;
    uint32_t ipc_flags;
    uint32_t op;		// request_op_t or reply_op_t
    client_context_t client_context; // context passed from client, returned by server in corresponding reply
    uint32_t reg_index;            // identifier for a record registered via DNSServiceRegisterRecord() on a
    // socket connected by DNSServiceConnect().  Must be unique in the scope of the connection, such that and
    // index/socket pair uniquely identifies a record.  (Used to select records for removal by DNSServiceRemoveRecord())
    } ipc_msg_hdr;

// routines to write to and extract data from message buffers.
// caller responsible for bounds checking.
// ptr is the address of the pointer to the start of the field.
// it is advanced to point to the next field, or the end of the message

void put_uint32(const uint32_t l, char **ptr);
uint32_t get_uint32(const char **ptr, const char *end);

void put_uint16(uint16_t s, char **ptr);
uint16_t get_uint16(const char **ptr, const char *end);

#define put_flags put_uint32
#define get_flags get_uint32

#define put_error_code put_uint32
#define get_error_code get_uint32

int put_string(const char *str, char **ptr);
int get_string(const char **ptr, const char *const end, char *buffer, int buflen);

void put_rdata(const int rdlen, const unsigned char *rdata, char **ptr);
const char *get_rdata(const char **ptr, const char *end, int rdlen);  // return value is rdata pointed to by *ptr -
                                         // rdata is not copied from buffer.

void ConvertHeaderBytes(ipc_msg_hdr *hdr);

struct CompileTimeAssertionChecks_dnssd_ipc
	{
	// Check that the compiler generated our on-the-wire packet format structure definitions
	// properly packed, without adding padding bytes to align fields on 32-bit or 64-bit boundaries.
	char assert0[(sizeof(client_context_t) ==  8) ? 1 : -1];
	char assert1[(sizeof(ipc_msg_hdr)      == 28) ? 1 : -1];
	};

#endif // DNSSD_IPC_H
