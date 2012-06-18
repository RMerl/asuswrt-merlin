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

$Log: dnssd_clientstub.c,v $
Revision 1.134  2009/06/19 23:13:24  cheshire
<rdar://problem/6990066> Library: crash at handle_resolve_response + 183
Added check for NULL after calling get_string

Revision 1.133  2009/05/27 22:19:12  cheshire
Remove questionable uses of errno

Revision 1.132  2009/05/26 21:31:07  herscher
Fix compile errors on Windows

Revision 1.131  2009/05/26 04:48:19  herscher
<rdar://problem/6844819> ExplorerPlugin does not work in B4W 2.0

Revision 1.130  2009/05/02 01:29:48  mcguire
<rdar://problem/6847601> spin calling DNSServiceProcessResult if errno was set to EWOULDBLOCK by an unrelated call

Revision 1.129  2009/05/01 19:18:50  cheshire
<rdar://problem/6843645> Using duplicate DNSServiceRefs when sharing a connection should return an error

Revision 1.128  2009/04/01 21:09:35  herscher
<rdar://problem/5925472> Current Bonjour code does not compile on Windows.

Revision 1.127  2009/03/03 21:38:19  cheshire
Improved "deliver_request ERROR" message

Revision 1.126  2009/02/12 21:02:22  cheshire
Commented out BPF "Sending fd" debugging message

Revision 1.125  2009/02/12 20:28:32  cheshire
Added some missing "const" declarations

Revision 1.124  2009/02/10 01:44:39  cheshire
<rdar://problem/6553729> DNSServiceUpdateRecord fails with kDNSServiceErr_BadReference for otherwise valid reference

Revision 1.123  2009/01/19 00:49:21  mkrochma
Type cast size_t values to unsigned long

Revision 1.122  2009/01/18 03:51:37  mkrochma
Fix warning in deliver_request on Linux

Revision 1.121  2009/01/16 23:34:37  cheshire
<rdar://problem/6504143> Uninitialized error code variable in error handling path in deliver_request

Revision 1.120  2009/01/13 05:31:35  mkrochma
<rdar://problem/6491367> Replace bzero, bcopy with mDNSPlatformMemZero, mDNSPlatformMemCopy, memset, memcpy

Revision 1.119  2009/01/11 03:45:08  mkrochma
Stop type casting num_written and num_read to int

Revision 1.118  2009/01/11 03:20:06  mkrochma
<rdar://problem/5797526> Fixes from Igor Seleznev to get mdnsd working on Solaris

Revision 1.117  2009/01/10 22:03:43  mkrochma
<rdar://problem/5797507> dnsextd fails to build on Linux

Revision 1.116  2009/01/05 16:55:24  cheshire
<rdar://problem/6452199> Stuck in "Examining available disks"
ConnectionResponse handler was accidentally matching the parent DNSServiceRef before
finding the appropriate subordinate DNSServiceRef for the operation in question.

Revision 1.115  2008/12/18 00:19:11  mcguire
<rdar://problem/6452199> Stuck in "Examining available disks"

Revision 1.114  2008/12/10 02:11:43  cheshire
ARMv5 compiler doesn't like uncommented stuff after #endif

Revision 1.113  2008/12/04 03:23:05  cheshire
Preincrement UID counter before we use it -- it helps with debugging if we know the all-zeroes ID should never appear

Revision 1.112  2008/11/25 22:56:54  cheshire
<rdar://problem/6377257> Make library code more defensive when client calls DNSServiceProcessResult with bad DNSServiceRef repeatedly

Revision 1.111  2008/10/28 17:58:44  cheshire
If client code keeps calling DNSServiceProcessResult repeatedly after an error, rate-limit the
"DNSServiceProcessResult called with DNSServiceRef with no ProcessReply function" log messages

Revision 1.110  2008/10/23 23:38:58  cheshire
For Windows compatibility, instead of "strerror(errno)" use "dnssd_strerror(dnssd_errno)"

Revision 1.109  2008/10/23 23:06:17  cheshire
Removed () from dnssd_errno macro definition -- it's not a function and doesn't need any arguments

Revision 1.108  2008/10/23 22:33:24  cheshire
Changed "NOTE:" to "Note:" so that BBEdit 9 stops putting those comment lines into the funtion popup menu

Revision 1.107  2008/10/20 21:50:11  cheshire
Improved /dev/bpf error message

Revision 1.106  2008/10/20 15:37:18  cheshire
Log error message if opening /dev/bpf fails

Revision 1.105  2008/09/27 01:26:34  cheshire
Added handler to pass back BPF fd when requested

Revision 1.104  2008/09/23 01:36:00  cheshire
Updated code to use internalPort/externalPort terminology, instead of the old privatePort/publicPort
terms (which could be misleading, because the word "private" suggests security).

Revision 1.103  2008/07/24 18:51:13  cheshire
Removed spurious spaces

Revision 1.102  2008/02/25 19:16:19  cheshire
<rdar://problem/5708953> Problems with DNSServiceGetAddrInfo API
Was returning a bogus result (NULL pointer) when following a CNAME referral

Revision 1.101  2008/02/20 21:18:21  cheshire
<rdar://problem/5708953> DNSServiceGetAddrInfo doesn't set the scope ID of returned IPv6 link local addresses

Revision 1.100  2007/11/02 17:56:37  cheshire
<rdar://problem/5565787> Bonjour API broken for 64-bit apps (SCM_RIGHTS sendmsg fails)
Wrap hack code in "#if APPLE_OSX_mDNSResponder" since (as far as we know right now)
we don't want to do this on 64-bit Linux, Solaris, etc.

Revision 1.99  2007/11/02 17:29:40  cheshire
<rdar://problem/5565787> Bonjour API broken for 64-bit apps (SCM_RIGHTS sendmsg fails)
To get 64-bit code that works, we need to NOT use the standard CMSG_* macros

Revision 1.98  2007/11/01 19:52:43  cheshire
Wrap debugging messages in "#if DEBUG_64BIT_SCM_RIGHTS"

Revision 1.97  2007/11/01 19:45:55  cheshire
Added "DEBUG_64BIT_SCM_RIGHTS" debugging code
See <rdar://problem/5565787> Bonjour API broken for 64-bit apps (SCM_RIGHTS sendmsg fails)

Revision 1.96  2007/11/01 15:59:33  cheshire
umask not being set and restored properly in USE_NAMED_ERROR_RETURN_SOCKET code
(no longer used on OS X, but relevant for other platforms)

Revision 1.95  2007/10/31 20:07:16  cheshire
<rdar://problem/5541498> Set SO_NOSIGPIPE on client socket
Refinement: the cleanup code still needs to close listenfd when necesssary

Revision 1.94  2007/10/15 22:34:27  cheshire
<rdar://problem/5541498> Set SO_NOSIGPIPE on client socket

Revision 1.93  2007/10/10 00:48:54  cheshire
<rdar://problem/5526379> Daemon spins in an infinite loop when it doesn't get the control message it's expecting

Revision 1.92  2007/10/06 03:44:44  cheshire
Testing code for <rdar://problem/5526374> kqueue does not get a kevent to wake it up when a control message arrives on a socket

Revision 1.91  2007/10/04 20:53:59  cheshire
Improved debugging message when sendmsg fails

Revision 1.90  2007/09/30 00:09:27  cheshire
<rdar://problem/5492315> Pass socket fd via SCM_RIGHTS sendmsg instead of using named UDS in the filesystem

Revision 1.89  2007/09/19 23:53:12  cheshire
Fixed spelling mistake in comment

Revision 1.88  2007/09/07 23:18:27  cheshire
<rdar://problem/5467542> Change "client_context" to be an incrementing 64-bit counter

Revision 1.87  2007/09/07 22:50:09  cheshire
Added comment explaining moreptr field in DNSServiceOp structure

Revision 1.86  2007/09/07 20:21:22  cheshire
<rdar://problem/5462371> Make DNSSD library more resilient
Add more comments explaining the moreptr/morebytes logic; don't allow DNSServiceRefSockFD or
DNSServiceProcessResult for subordinate DNSServiceRefs created using kDNSServiceFlagsShareConnection

Revision 1.85  2007/09/06 21:43:23  cheshire
<rdar://problem/5462371> Make DNSSD library more resilient
Allow DNSServiceRefDeallocate from within DNSServiceProcessResult callback

Revision 1.84  2007/09/06 18:31:47  cheshire
<rdar://problem/5462371> Make DNSSD library more resilient against client programming errors

Revision 1.83  2007/08/28 20:45:45  cheshire
Typo: ctrl_path needs to be 64 bytes, not 44 bytes

Revision 1.82  2007/08/28 19:53:52  cheshire
<rdar://problem/5437423> Bonjour failures when /tmp is not writable (e.g. when booted from installer disc)

Revision 1.81  2007/07/27 00:03:20  cheshire
Fixed compiler warnings that showed up now we're building optimized ("-Os")

Revision 1.80  2007/07/23 22:12:53  cheshire
<rdar://problem/5352299> Make mDNSResponder more defensive against malicious local clients

Revision 1.79  2007/07/23 19:58:24  cheshire
<rdar://problem/5351640> Library: Leak in DNSServiceRefDeallocate

Revision 1.78  2007/07/12 20:42:27  cheshire
<rdar://problem/5280735> If daemon is killed, return kDNSServiceErr_ServiceNotRunning
to clients instead of kDNSServiceErr_Unknown

Revision 1.77  2007/07/02 23:07:13  cheshire
<rdar://problem/5308280> Reduce DNS-SD client syslog error messages

Revision 1.76  2007/06/22 20:12:18  cheshire
<rdar://problem/5277024> Leak in DNSServiceRefDeallocate

Revision 1.75  2007/05/23 18:59:22  cheshire
Remove unnecessary IPC_FLAGS_REUSE_SOCKET

Revision 1.74  2007/05/22 18:28:38  cheshire
Fixed compile errors in posix build

Revision 1.73  2007/05/22 01:20:47  cheshire
To determine current operation, need to check hdr->op, not sdr->op

Revision 1.72  2007/05/22 01:07:42  cheshire
<rdar://problem/3563675> API: Need a way to get version/feature information

Revision 1.71  2007/05/18 23:55:22  cheshire
<rdar://problem/4454655> Allow multiple register/browse/resolve operations to share single Unix Domain Socket

Revision 1.70  2007/05/17 20:58:22  cheshire
<rdar://problem/4647145> DNSServiceQueryRecord should return useful information with NXDOMAIN

Revision 1.69  2007/05/16 16:58:27  cheshire
<rdar://problem/4471320> Improve reliability of kDNSServiceFlagsMoreComing flag on multiprocessor machines
As long as select indicates that data is waiting, loop within DNSServiceProcessResult delivering additional results

Revision 1.68  2007/05/16 01:06:52  cheshire
<rdar://problem/4471320> Improve reliability of kDNSServiceFlagsMoreComing flag on multiprocessor machines

Revision 1.67  2007/05/15 21:57:16  cheshire
<rdar://problem/4608220> Use dnssd_SocketValid(x) macro instead of just
assuming that all negative values (or zero!) are invalid socket numbers

Revision 1.66  2007/03/27 22:23:04  cheshire
Add "dnssd_clientstub" prefix onto syslog messages

Revision 1.65  2007/03/21 22:25:23  cheshire
<rdar://problem/4172796> Remove client retry logic now that mDNSResponder uses launchd for its Unix Domain Socket

Revision 1.64  2007/03/21 19:01:56  cheshire
<rdar://problem/5078494> IPC code not 64-bit-savvy: assumes long=32bits, and short=16bits

Revision 1.63  2007/03/12 21:48:21  cheshire
<rdar://problem/5000162> Scary unlink errors in system.log
Code was using memory after it had been freed

Revision 1.62  2007/02/28 01:44:30  cheshire
<rdar://problem/5027863> Byte order bugs in uDNS.c, uds_daemon.c, dnssd_clientstub.c

Revision 1.61  2007/02/09 03:09:42  cheshire
<rdar://problem/3869251> Cleanup: Stop returning kDNSServiceErr_Unknown so often
<rdar://problem/4177924> API: Should return kDNSServiceErr_ServiceNotRunning

Revision 1.60  2007/02/08 20:33:44  cheshire
<rdar://problem/4985095> Leak on error path in DNSServiceProcessResult

Revision 1.59  2007/01/05 08:30:55  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.58  2006/10/27 00:38:22  cheshire
Strip accidental trailing whitespace from lines

Revision 1.57  2006/09/30 01:06:54  cheshire
Protocol field should be uint32_t

Revision 1.56  2006/09/27 00:44:16  herscher
<rdar://problem/4249761> API: Need DNSServiceGetAddrInfo()

Revision 1.55  2006/09/26 01:52:01  herscher
<rdar://problem/4245016> NAT Port Mapping API (for both NAT-PMP and UPnP Gateway Protocol)

Revision 1.54  2006/09/21 21:34:09  cheshire
<rdar://problem/4100000> Allow empty string name when using kDNSServiceFlagsNoAutoRename

Revision 1.53  2006/09/07 04:43:12  herscher
Fix compile error on Win32 platform by moving inclusion of syslog.h

Revision 1.52  2006/08/15 23:04:21  mkrochma
<rdar://problem/4090354> Client should be able to specify service name w/o callback

Revision 1.51  2006/07/24 23:45:55  cheshire
<rdar://problem/4605276> DNSServiceReconfirmRecord() should return error code

Revision 1.50  2006/06/28 08:22:27  cheshire
<rdar://problem/4605264> dnssd_clientstub.c needs to report unlink failures in syslog

Revision 1.49  2006/06/28 07:58:59  cheshire
Minor textual tidying

*/

#include <errno.h>
#include <stdlib.h>

#include "dnssd_ipc.h"

#if defined(_WIN32)

	#define _SSIZE_T
	#include <CommonServices.h>
	#include <DebugServices.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
	#include <stdarg.h>
	
	#define sockaddr_mdns sockaddr_in
	#define AF_MDNS AF_INET
	
	// Disable warning: "'type cast' : from data pointer 'void *' to function pointer"
	#pragma warning(disable:4055)
	
	// Disable warning: "nonstandard extension, function/data pointer conversion in expression"
	#pragma warning(disable:4152)
	
	extern BOOL IsSystemServiceDisabled();
	
	#define sleep(X) Sleep((X) * 1000)
	
	static int g_initWinsock = 0;
	#define LOG_WARNING kDebugLevelWarning
	static void syslog( int priority, const char * message, ...)
		{
		va_list args;
		int len;
		char * buffer;
		DWORD err = WSAGetLastError();
		va_start( args, message );
		len = _vscprintf( message, args ) + 1;
		buffer = malloc( len * sizeof(char) );
		if ( buffer ) { vsprintf( buffer, message, args ); OutputDebugString( buffer ); free( buffer ); }
		WSASetLastError( err );
		}
#else

	#include <sys/fcntl.h>		// For O_RDWR etc.
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <syslog.h>
	
	#define sockaddr_mdns sockaddr_un
	#define AF_MDNS AF_LOCAL

#endif

// <rdar://problem/4096913> Specifies how many times we'll try and connect to the server.

#define DNSSD_CLIENT_MAXTRIES 4

// Uncomment the line below to use the old error return mechanism of creating a temporary named socket (e.g. in /var/tmp)
//#define USE_NAMED_ERROR_RETURN_SOCKET 1

#ifndef CTL_PATH_PREFIX
#define CTL_PATH_PREFIX "/var/tmp/dnssd_result_socket."
#endif

typedef struct
	{
	ipc_msg_hdr         ipc_hdr;
	DNSServiceFlags     cb_flags;
	uint32_t            cb_interface;
	DNSServiceErrorType cb_err;
	} CallbackHeader;

typedef struct _DNSServiceRef_t DNSServiceOp;
typedef struct _DNSRecordRef_t DNSRecord;

// client stub callback to process message from server and deliver results to client application
typedef void (*ProcessReplyFn)(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *msg, const char *const end);

#define ValidatorBits 0x12345678
#define DNSServiceRefValid(X) (dnssd_SocketValid((X)->sockfd) && (((X)->sockfd ^ (X)->validator) == ValidatorBits))

// When using kDNSServiceFlagsShareConnection, there is one primary _DNSServiceOp_t, and zero or more subordinates
// For the primary, the 'next' field points to the first subordinate, and its 'next' field points to the next, and so on.
// For the primary, the 'primary' field is NULL; for subordinates the 'primary' field points back to the associated primary
struct _DNSServiceRef_t
	{
	DNSServiceOp    *next;				// For shared connection
	DNSServiceOp    *primary;			// For shared connection
	dnssd_sock_t     sockfd;			// Connected socket between client and daemon
	dnssd_sock_t     validator;			// Used to detect memory corruption, double disposals, etc.
	client_context_t uid;				// For shared connection requests, each subordinate DNSServiceRef has its own ID,
										// unique within the scope of the same shared parent DNSServiceRef
	uint32_t         op;				// request_op_t or reply_op_t
	uint32_t         max_index;			// Largest assigned record index - 0 if no additional records registered
	uint32_t         logcounter;		// Counter used to control number of syslog messages we write
	int             *moreptr;			// Set while DNSServiceProcessResult working on this particular DNSServiceRef
	ProcessReplyFn   ProcessReply;		// Function pointer to the code to handle received messages
	void            *AppCallback;		// Client callback function and context
	void            *AppContext;
	};

struct _DNSRecordRef_t
	{
	void *AppContext;
	DNSServiceRegisterRecordReply AppCallback;
	DNSRecordRef recref;
	uint32_t record_index;  // index is unique to the ServiceDiscoveryRef
	DNSServiceOp *sdr;
	};

// Write len bytes. Return 0 on success, -1 on error
static int write_all(dnssd_sock_t sd, char *buf, int len)
	{
	// Don't use "MSG_WAITALL"; it returns "Invalid argument" on some Linux versions; use an explicit while() loop instead.
	//if (send(sd, buf, len, MSG_WAITALL) != len) return -1;
	while (len)
		{
		ssize_t num_written = send(sd, buf, len, 0);
		if (num_written < 0 || num_written > len)
			{
			// Should never happen. If it does, it indicates some OS bug,
			// or that the mDNSResponder daemon crashed (which should never happen).
			syslog(LOG_WARNING, "dnssd_clientstub write_all(%d) failed %ld/%d %d %s", sd, num_written, len,
				(num_written < 0) ? dnssd_errno                 : 0,
				(num_written < 0) ? dnssd_strerror(dnssd_errno) : "");
			return -1;
			}
		buf += num_written;
		len -= num_written;
		}
	return 0;
	}

enum { read_all_success = 0, read_all_fail = -1, read_all_wouldblock = -2 };

// Read len bytes. Return 0 on success, read_all_fail on error, or read_all_wouldblock for 
static int read_all(dnssd_sock_t sd, char *buf, int len)
	{
	// Don't use "MSG_WAITALL"; it returns "Invalid argument" on some Linux versions; use an explicit while() loop instead.
	//if (recv(sd, buf, len, MSG_WAITALL) != len) return -1;

	while (len)
		{
		ssize_t num_read = recv(sd, buf, len, 0);
		if ((num_read == 0) || (num_read < 0) || (num_read > len))
			{
			// Should never happen. If it does, it indicates some OS bug,
			// or that the mDNSResponder daemon crashed (which should never happen).
			syslog(LOG_WARNING, "dnssd_clientstub read_all(%d) failed %ld/%d %d %s", sd, num_read, len,
				(num_read < 0) ? dnssd_errno                 : 0,
				(num_read < 0) ? dnssd_strerror(dnssd_errno) : "");
			return (num_read < 0 && dnssd_errno == dnssd_EWOULDBLOCK) ? read_all_wouldblock : read_all_fail;
			}
		buf += num_read;
		len -= num_read;
		}
	return read_all_success;
	}

// Returns 1 if more bytes remain to be read on socket descriptor sd, 0 otherwise
static int more_bytes(dnssd_sock_t sd)
	{
	struct timeval tv = { 0, 0 };
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(sd, &readfds);
	return(select(sd+1, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv) > 0);
	}

/* create_hdr
 *
 * allocate and initialize an ipc message header. Value of len should initially be the
 * length of the data, and is set to the value of the data plus the header. data_start
 * is set to point to the beginning of the data section. SeparateReturnSocket should be
 * non-zero for calls that can't receive an immediate error return value on their primary
 * socket, and therefore require a separate return path for the error code result.
 * if zero, the path to a control socket is appended at the beginning of the message buffer.
 * data_start is set past this string.
 */
static ipc_msg_hdr *create_hdr(uint32_t op, size_t *len, char **data_start, int SeparateReturnSocket, DNSServiceOp *ref)
	{
	char *msg = NULL;
	ipc_msg_hdr *hdr;
	int datalen;
#if !defined(USE_TCP_LOOPBACK)
	char ctrl_path[64] = "";	// "/var/tmp/dnssd_result_socket.xxxxxxxxxx-xxx-xxxxxx"
#endif

	if (SeparateReturnSocket)
		{
#if defined(USE_TCP_LOOPBACK)
		*len += 2;  // Allocate space for two-byte port number
#elif defined(USE_NAMED_ERROR_RETURN_SOCKET)
		struct timeval time;
		if (gettimeofday(&time, NULL) < 0)
			{ syslog(LOG_WARNING, "dnssd_clientstub create_hdr: gettimeofday failed %d %s", dnssd_errno, dnssd_strerror(dnssd_errno)); return NULL; }
		sprintf(ctrl_path, "%s%d-%.3lx-%.6lu", CTL_PATH_PREFIX, (int)getpid(),
			(unsigned long)(time.tv_sec & 0xFFF), (unsigned long)(time.tv_usec));
		*len += strlen(ctrl_path) + 1;
#else
		*len += 1;		// Allocate space for single zero byte (empty C string)
#endif
		}

	datalen = (int) *len;
	*len += sizeof(ipc_msg_hdr);

	// Write message to buffer
	msg = malloc(*len);
	if (!msg) { syslog(LOG_WARNING, "dnssd_clientstub create_hdr: malloc failed"); return NULL; }

	memset(msg, 0, *len);
	hdr = (ipc_msg_hdr *)msg;
	hdr->version                = VERSION;
	hdr->datalen                = datalen;
	hdr->ipc_flags              = 0;
	hdr->op                     = op;
	hdr->client_context         = ref->uid;
	hdr->reg_index              = 0;
	*data_start = msg + sizeof(ipc_msg_hdr);
#if defined(USE_TCP_LOOPBACK)
	// Put dummy data in for the port, since we don't know what it is yet.
	// The data will get filled in before we send the message. This happens in deliver_request().
	if (SeparateReturnSocket) put_uint16(0, data_start);
#else
	if (SeparateReturnSocket) put_string(ctrl_path, data_start);
#endif
	return hdr;
	}

static void FreeDNSServiceOp(DNSServiceOp *x)
	{
	// We don't use our DNSServiceRefValid macro here because if we're cleaning up after a socket() call failed 
	// then sockfd could legitimately contain a failing value (e.g. dnssd_InvalidSocket)
	if ((x->sockfd ^ x->validator) != ValidatorBits)
		syslog(LOG_WARNING, "dnssd_clientstub attempt to dispose invalid DNSServiceRef %p %08X %08X", x, x->sockfd, x->validator);
	else
		{
		x->next         = NULL;
		x->primary      = NULL;
		x->sockfd       = dnssd_InvalidSocket;
		x->validator    = 0xDDDDDDDD;
		x->op           = request_op_none;
		x->max_index    = 0;
		x->logcounter   = 0;
		x->moreptr      = NULL;
		x->ProcessReply = NULL;
		x->AppCallback  = NULL;
		x->AppContext   = NULL;
		free(x);
		}
	}

// Return a connected service ref (deallocate with DNSServiceRefDeallocate)
static DNSServiceErrorType ConnectToServer(DNSServiceRef *ref, DNSServiceFlags flags, uint32_t op, ProcessReplyFn ProcessReply, void *AppCallback, void *AppContext)
	{
	#if APPLE_OSX_mDNSResponder
	int NumTries = DNSSD_CLIENT_MAXTRIES;
	#else
	int NumTries = 0;
	#endif

	dnssd_sockaddr_t saddr;
	DNSServiceOp *sdr;

	if (!ref) { syslog(LOG_WARNING, "dnssd_clientstub DNSService operation with NULL DNSServiceRef"); return kDNSServiceErr_BadParam; }

	if (flags & kDNSServiceFlagsShareConnection)
		{
		if (!*ref)
			{
			syslog(LOG_WARNING, "dnssd_clientstub kDNSServiceFlagsShareConnection used with NULL DNSServiceRef");
			return kDNSServiceErr_BadParam;
			}
		if (!DNSServiceRefValid(*ref) || (*ref)->op != connection_request || (*ref)->primary)
			{
			syslog(LOG_WARNING, "dnssd_clientstub kDNSServiceFlagsShareConnection used with invalid DNSServiceRef %p %08X %08X",
				(*ref), (*ref)->sockfd, (*ref)->validator);
			*ref = NULL;
			return kDNSServiceErr_BadReference;
			}
		}

	#if defined(_WIN32)
	if (!g_initWinsock)
		{
		WSADATA wsaData;
		g_initWinsock = 1;
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { *ref = NULL; return kDNSServiceErr_ServiceNotRunning; }
		}
	// <rdar://problem/4096913> If the system service is disabled, we only want to try to connect once
	if (IsSystemServiceDisabled()) NumTries = DNSSD_CLIENT_MAXTRIES;
	#endif

	sdr = malloc(sizeof(DNSServiceOp));
	if (!sdr) { syslog(LOG_WARNING, "dnssd_clientstub ConnectToServer: malloc failed"); *ref = NULL; return kDNSServiceErr_NoMemory; }
	sdr->next          = NULL;
	sdr->primary       = NULL;
	sdr->sockfd        = dnssd_InvalidSocket;
	sdr->validator     = sdr->sockfd ^ ValidatorBits;
	sdr->op            = op;
	sdr->max_index     = 0;
	sdr->logcounter    = 0;
	sdr->moreptr       = NULL;
	sdr->uid.u32[0]    = 0;
	sdr->uid.u32[1]    = 0;
	sdr->ProcessReply  = ProcessReply;
	sdr->AppCallback   = AppCallback;
	sdr->AppContext    = AppContext;

	if (flags & kDNSServiceFlagsShareConnection)
		{
		DNSServiceOp **p = &(*ref)->next;		// Append ourselves to end of primary's list
		while (*p) p = &(*p)->next;
		*p = sdr;
		// Preincrement counter before we use it -- it helps with debugging if we know the all-zeroes ID should never appear
		if (++(*ref)->uid.u32[0] == 0) ++(*ref)->uid.u32[1];	// In parent DNSServiceOp increment UID counter
		sdr->primary    = *ref;					// Set our primary pointer
		sdr->sockfd     = (*ref)->sockfd;		// Inherit primary's socket
		sdr->validator  = (*ref)->validator;
		sdr->uid        = (*ref)->uid;
		//printf("ConnectToServer sharing socket %d\n", sdr->sockfd);
		}
	else
		{
		#ifdef SO_NOSIGPIPE
		const unsigned long optval = 1;
		#endif
		*ref = NULL;
		sdr->sockfd    = socket(AF_DNSSD, SOCK_STREAM, 0);
		sdr->validator = sdr->sockfd ^ ValidatorBits;
		if (!dnssd_SocketValid(sdr->sockfd))
			{
			syslog(LOG_WARNING, "dnssd_clientstub ConnectToServer: socket failed %d %s", dnssd_errno, dnssd_strerror(dnssd_errno));
			FreeDNSServiceOp(sdr);
			return kDNSServiceErr_NoMemory;
			}
		#ifdef SO_NOSIGPIPE
		// Some environments (e.g. OS X) support turning off SIGPIPE for a socket
		if (setsockopt(sdr->sockfd, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) < 0)
			syslog(LOG_WARNING, "dnssd_clientstub ConnectToServer: SO_NOSIGPIPE failed %d %s", dnssd_errno, dnssd_strerror(dnssd_errno));
		#endif
		#if defined(USE_TCP_LOOPBACK)
		saddr.sin_family      = AF_INET;
		saddr.sin_addr.s_addr = inet_addr(MDNS_TCP_SERVERADDR);
		saddr.sin_port        = htons(MDNS_TCP_SERVERPORT);
		#else
		saddr.sun_family      = AF_LOCAL;
		strcpy(saddr.sun_path, MDNS_UDS_SERVERPATH);
		#endif
	
		while (1)
			{
			int err = connect(sdr->sockfd, (struct sockaddr *) &saddr, sizeof(saddr));
			if (!err) break; // If we succeeded, return sdr
			// If we failed, then it may be because the daemon is still launching.
			// This can happen for processes that launch early in the boot process, while the
			// daemon is still coming up. Rather than fail here, we'll wait a bit and try again.
			// If, after four seconds, we still can't connect to the daemon,
			// then we give up and return a failure code.
			if (++NumTries < DNSSD_CLIENT_MAXTRIES) sleep(1); // Sleep a bit, then try again
			else { dnssd_close(sdr->sockfd); FreeDNSServiceOp(sdr); return kDNSServiceErr_ServiceNotRunning; }
			}
		//printf("ConnectToServer opened socket %d\n", sdr->sockfd);
		}

	*ref = sdr;
	return kDNSServiceErr_NoError;
	}

#define deliver_request_bailout(MSG) \
	do { syslog(LOG_WARNING, "dnssd_clientstub deliver_request: %s failed %d (%s)", (MSG), dnssd_errno, dnssd_strerror(dnssd_errno)); goto cleanup; } while(0)

static DNSServiceErrorType deliver_request(ipc_msg_hdr *hdr, DNSServiceOp *sdr)
	{
	uint32_t datalen = hdr->datalen;	// We take a copy here because we're going to convert hdr->datalen to network byte order
	#if defined(USE_TCP_LOOPBACK) || defined(USE_NAMED_ERROR_RETURN_SOCKET)
	char *const data = (char *)hdr + sizeof(ipc_msg_hdr);
	#endif
	dnssd_sock_t listenfd = dnssd_InvalidSocket, errsd = dnssd_InvalidSocket;
	DNSServiceErrorType err = kDNSServiceErr_Unknown;	// Default for the "goto cleanup" cases
	int MakeSeparateReturnSocket = 0;

	// Note: need to check hdr->op, not sdr->op.
	// hdr->op contains the code for the specific operation we're currently doing, whereas sdr->op
	// contains the original parent DNSServiceOp (e.g. for an add_record_request, hdr->op will be
	// add_record_request but the parent sdr->op will be connection_request or reg_service_request)
	if (sdr->primary ||
		hdr->op == reg_record_request || hdr->op == add_record_request || hdr->op == update_record_request || hdr->op == remove_record_request)
		MakeSeparateReturnSocket = 1;

	if (!DNSServiceRefValid(sdr))
		{
		syslog(LOG_WARNING, "dnssd_clientstub deliver_request: invalid DNSServiceRef %p %08X %08X", sdr, sdr->sockfd, sdr->validator);
		return kDNSServiceErr_BadReference;
		}

	if (!hdr) { syslog(LOG_WARNING, "dnssd_clientstub deliver_request: !hdr"); return kDNSServiceErr_Unknown; }

	if (MakeSeparateReturnSocket)
		{
		#if defined(USE_TCP_LOOPBACK)
			{
			union { uint16_t s; u_char b[2]; } port;
			dnssd_sockaddr_t caddr;
			dnssd_socklen_t len = (dnssd_socklen_t) sizeof(caddr);
			listenfd = socket(AF_DNSSD, SOCK_STREAM, 0);
			if (!dnssd_SocketValid(listenfd)) deliver_request_bailout("TCP socket");

			caddr.sin_family      = AF_INET;
			caddr.sin_port        = 0;
			caddr.sin_addr.s_addr = inet_addr(MDNS_TCP_SERVERADDR);
			if (bind(listenfd, (struct sockaddr*) &caddr, sizeof(caddr)) < 0) deliver_request_bailout("TCP bind");
			if (getsockname(listenfd, (struct sockaddr*) &caddr, &len)   < 0) deliver_request_bailout("TCP getsockname");
			if (listen(listenfd, 1)                                      < 0) deliver_request_bailout("TCP listen");
			port.s = caddr.sin_port;
			data[0] = port.b[0];  // don't switch the byte order, as the
			data[1] = port.b[1];  // daemon expects it in network byte order
			}
		#elif defined(USE_NAMED_ERROR_RETURN_SOCKET)
			{
			mode_t mask;
			int bindresult;
			dnssd_sockaddr_t caddr;
			listenfd = socket(AF_DNSSD, SOCK_STREAM, 0);
			if (!dnssd_SocketValid(listenfd)) deliver_request_bailout("USE_NAMED_ERROR_RETURN_SOCKET socket");

			caddr.sun_family = AF_LOCAL;
			// According to Stevens (section 3.2), there is no portable way to
			// determine whether sa_len is defined on a particular platform.
			#ifndef NOT_HAVE_SA_LEN
			caddr.sun_len = sizeof(struct sockaddr_un);
			#endif
			strcpy(caddr.sun_path, data);
			mask = umask(0);
			bindresult = bind(listenfd, (struct sockaddr *)&caddr, sizeof(caddr));
			umask(mask);
			if (bindresult          < 0) deliver_request_bailout("USE_NAMED_ERROR_RETURN_SOCKET bind");
			if (listen(listenfd, 1) < 0) deliver_request_bailout("USE_NAMED_ERROR_RETURN_SOCKET listen");
			}
		#else
			{
			dnssd_sock_t sp[2];
			if (socketpair(AF_DNSSD, SOCK_STREAM, 0, sp) < 0) deliver_request_bailout("socketpair");
			else
				{
				errsd    = sp[0];	// We'll read our four-byte error code from sp[0]
				listenfd = sp[1];	// We'll send sp[1] to the daemon
				}
			}
		#endif
		}

#if !defined(USE_TCP_LOOPBACK) && !defined(USE_NAMED_ERROR_RETURN_SOCKET)
	// If we're going to make a separate error return socket, and pass it to the daemon
	// using sendmsg, then we'll hold back one data byte to go with it.
	// On some versions of Unix (including Leopard) sending a control message without
	// any associated data does not work reliably -- e.g. one particular issue we ran
	// into is that if the receiving program is in a kqueue loop waiting to be notified
	// of the received message, it doesn't get woken up when the control message arrives.
	if (MakeSeparateReturnSocket || sdr->op == send_bpf) datalen--;		// Okay to use sdr->op when checking for op == send_bpf
#endif

	// At this point, our listening socket is set up and waiting, if necessary, for the daemon to connect back to
	ConvertHeaderBytes(hdr);
	//syslog(LOG_WARNING, "dnssd_clientstub deliver_request writing %lu bytes", (unsigned long)(datalen + sizeof(ipc_msg_hdr)));
	//if (MakeSeparateReturnSocket) syslog(LOG_WARNING, "dnssd_clientstub deliver_request name is %s", data);
#if TEST_SENDING_ONE_BYTE_AT_A_TIME
	unsigned int i;
	for (i=0; i<datalen + sizeof(ipc_msg_hdr); i++)
		{
		syslog(LOG_WARNING, "dnssd_clientstub deliver_request writing %d", i);
		if (write_all(sdr->sockfd, ((char *)hdr)+i, 1) < 0)
			{ syslog(LOG_WARNING, "write_all (byte %u) failed", i); goto cleanup; }
		usleep(10000);
		}
#else
	if (write_all(sdr->sockfd, (char *)hdr, datalen + sizeof(ipc_msg_hdr)) < 0)
		{
		syslog(LOG_WARNING, "dnssd_clientstub deliver_request ERROR: write_all(%d, %lu bytes) failed",
			sdr->sockfd, (unsigned long)(datalen + sizeof(ipc_msg_hdr)));
		goto cleanup;
		}
#endif

	if (!MakeSeparateReturnSocket) errsd = sdr->sockfd;
	if (MakeSeparateReturnSocket || sdr->op == send_bpf)	// Okay to use sdr->op when checking for op == send_bpf
		{
#if defined(USE_TCP_LOOPBACK) || defined(USE_NAMED_ERROR_RETURN_SOCKET)
		// At this point we may block in accept for a few milliseconds waiting for the daemon to connect back to us,
		// but that's okay -- the daemon is a trusted service and we know if won't take more than a few milliseconds to respond.
		dnssd_sockaddr_t daddr;
		dnssd_socklen_t len = sizeof(daddr);
		errsd = accept(listenfd, (struct sockaddr *)&daddr, &len);
		if (!dnssd_SocketValid(errsd)) deliver_request_bailout("accept");
#else

#if APPLE_OSX_mDNSResponder
// On Leopard, the stock definitions of the CMSG_* macros in /usr/include/sys/socket.h,
// while arguably correct in theory, nonetheless in practice produce code that doesn't work on 64-bit machines
// For details see <rdar://problem/5565787> Bonjour API broken for 64-bit apps (SCM_RIGHTS sendmsg fails)
#undef  CMSG_DATA
#define CMSG_DATA(cmsg) ((unsigned char *)(cmsg) + (sizeof(struct cmsghdr)))
#undef  CMSG_SPACE
#define CMSG_SPACE(l)   ((sizeof(struct cmsghdr)) + (l))
#undef  CMSG_LEN
#define CMSG_LEN(l)     ((sizeof(struct cmsghdr)) + (l))
#endif

		struct iovec vec = { ((char *)hdr) + sizeof(ipc_msg_hdr) + datalen, 1 }; // Send the last byte along with the SCM_RIGHTS
		struct msghdr msg;
		struct cmsghdr *cmsg;
		char cbuf[CMSG_SPACE(sizeof(dnssd_sock_t))];

		if (sdr->op == send_bpf)	// Okay to use sdr->op when checking for op == send_bpf
			{
			int i;
			char p[12];		// Room for "/dev/bpf999" with terminating null
			for (i=0; i<100; i++)
				{
				snprintf(p, sizeof(p), "/dev/bpf%d", i);
				listenfd = open(p, O_RDWR, 0);
				//if (dnssd_SocketValid(listenfd)) syslog(LOG_WARNING, "Sending fd %d for %s", listenfd, p);
				if (!dnssd_SocketValid(listenfd) && dnssd_errno != EBUSY)
					syslog(LOG_WARNING, "Error opening %s %d (%s)", p, dnssd_errno, dnssd_strerror(dnssd_errno));
				if (dnssd_SocketValid(listenfd) || dnssd_errno != EBUSY) break;
				}
			}

		msg.msg_name       = 0;
		msg.msg_namelen    = 0;
		msg.msg_iov        = &vec;
		msg.msg_iovlen     = 1;
		msg.msg_control    = cbuf;
		msg.msg_controllen = CMSG_LEN(sizeof(dnssd_sock_t));
		msg.msg_flags      = 0;
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len     = CMSG_LEN(sizeof(dnssd_sock_t));
		cmsg->cmsg_level   = SOL_SOCKET;
		cmsg->cmsg_type    = SCM_RIGHTS;
		*((dnssd_sock_t *)CMSG_DATA(cmsg)) = listenfd;

#if TEST_KQUEUE_CONTROL_MESSAGE_BUG
		sleep(1);
#endif

#if DEBUG_64BIT_SCM_RIGHTS
		syslog(LOG_WARNING, "dnssd_clientstub sendmsg read sd=%d write sd=%d %ld %ld %ld/%ld/%ld/%ld",
			errsd, listenfd, sizeof(dnssd_sock_t), sizeof(void*),
			sizeof(struct cmsghdr) + sizeof(dnssd_sock_t),
			CMSG_LEN(sizeof(dnssd_sock_t)), (long)CMSG_SPACE(sizeof(dnssd_sock_t)),
			(long)((char*)CMSG_DATA(cmsg) + 4 - cbuf));
#endif // DEBUG_64BIT_SCM_RIGHTS

		if (sendmsg(sdr->sockfd, &msg, 0) < 0)
			{
			syslog(LOG_WARNING, "dnssd_clientstub deliver_request ERROR: sendmsg failed read sd=%d write sd=%d errno %d (%s)",
				errsd, listenfd, dnssd_errno, dnssd_strerror(dnssd_errno));
			err = kDNSServiceErr_Incompatible;
			goto cleanup;
			}

#if DEBUG_64BIT_SCM_RIGHTS
		syslog(LOG_WARNING, "dnssd_clientstub sendmsg read sd=%d write sd=%d okay", errsd, listenfd);
#endif // DEBUG_64BIT_SCM_RIGHTS

#endif
		// Close our end of the socketpair *before* blocking in read_all to get the four-byte error code.
		// Otherwise, if the daemon closes our socket (or crashes), we block in read_all() forever
		// because the socket is not closed (we still have an open reference to it ourselves).
		dnssd_close(listenfd);
		listenfd = dnssd_InvalidSocket;		// Make sure we don't close it a second time in the cleanup handling below
		}

	// At this point we may block in read_all for a few milliseconds waiting for the daemon to send us the error code,
	// but that's okay -- the daemon is a trusted service and we know if won't take more than a few milliseconds to respond.
	if (sdr->op == send_bpf)	// Okay to use sdr->op when checking for op == send_bpf
		err = kDNSServiceErr_NoError;
	else if (read_all(errsd, (char*)&err, (int)sizeof(err)) < 0)
		err = kDNSServiceErr_ServiceNotRunning;	// On failure read_all will have written a message to syslog for us
	else
		err = ntohl(err);

	//syslog(LOG_WARNING, "dnssd_clientstub deliver_request: retrieved error code %d", err);

cleanup:
	if (MakeSeparateReturnSocket)
		{
		if (dnssd_SocketValid(listenfd)) dnssd_close(listenfd);
		if (dnssd_SocketValid(errsd))    dnssd_close(errsd);
#if defined(USE_NAMED_ERROR_RETURN_SOCKET)
		// syslog(LOG_WARNING, "dnssd_clientstub deliver_request: removing UDS: %s", data);
		if (unlink(data) != 0)
			syslog(LOG_WARNING, "dnssd_clientstub WARNING: unlink(\"%s\") failed errno %d (%s)", data, dnssd_errno, dnssd_strerror(dnssd_errno));
		// else syslog(LOG_WARNING, "dnssd_clientstub deliver_request: removed UDS: %s", data);
#endif
		}

	free(hdr);
	return err;
	}

int DNSSD_API DNSServiceRefSockFD(DNSServiceRef sdRef)
	{
	if (!sdRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRefSockFD called with NULL DNSServiceRef"); return dnssd_InvalidSocket; }

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRefSockFD called with invalid DNSServiceRef %p %08X %08X",
			sdRef, sdRef->sockfd, sdRef->validator);
		return dnssd_InvalidSocket;
		}

	if (sdRef->primary)
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRefSockFD undefined for kDNSServiceFlagsShareConnection subordinate DNSServiceRef %p", sdRef);
		return dnssd_InvalidSocket;
		}

	return (int) sdRef->sockfd;
	}

// Handle reply from server, calling application client callback. If there is no reply
// from the daemon on the socket contained in sdRef, the call will block.
DNSServiceErrorType DNSSD_API DNSServiceProcessResult(DNSServiceRef sdRef)
	{
	int morebytes = 0;

	if (!sdRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult called with NULL DNSServiceRef"); return kDNSServiceErr_BadParam; }

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return kDNSServiceErr_BadReference;
		}

	if (sdRef->primary)
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult undefined for kDNSServiceFlagsShareConnection subordinate DNSServiceRef %p", sdRef);
		return kDNSServiceErr_BadReference;
		}

	if (!sdRef->ProcessReply)
		{
		static int num_logs = 0;
		if (num_logs < 10) syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult called with DNSServiceRef with no ProcessReply function");
		if (num_logs < 1000) num_logs++; else sleep(1);
		return kDNSServiceErr_BadReference;
		}

	do
		{
		CallbackHeader cbh;
		char *data;
	
		// return NoError on EWOULDBLOCK. This will handle the case
		// where a non-blocking socket is told there is data, but it was a false positive.
		// On error, read_all will write a message to syslog for us, so don't need to duplicate that here
		// Note: If we want to properly support using non-blocking sockets in the future 
		int result = read_all(sdRef->sockfd, (void *)&cbh.ipc_hdr, sizeof(cbh.ipc_hdr));
		if (result == read_all_fail)
			{
			sdRef->ProcessReply = NULL;
			return kDNSServiceErr_ServiceNotRunning;
			}
		else if (result == read_all_wouldblock)
			{
			if (morebytes && sdRef->logcounter < 100)
				{
				sdRef->logcounter++;
				syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult error: select indicated data was waiting but read_all returned EWOULDBLOCK");
				}
			return kDNSServiceErr_NoError;
			}
	
		ConvertHeaderBytes(&cbh.ipc_hdr);
		if (cbh.ipc_hdr.version != VERSION)
			{
			syslog(LOG_WARNING, "dnssd_clientstub DNSServiceProcessResult daemon version %d does not match client version %d", cbh.ipc_hdr.version, VERSION);
			sdRef->ProcessReply = NULL;
			return kDNSServiceErr_Incompatible;
			}
	
		data = malloc(cbh.ipc_hdr.datalen);
		if (!data) return kDNSServiceErr_NoMemory;
		if (read_all(sdRef->sockfd, data, cbh.ipc_hdr.datalen) < 0) // On error, read_all will write a message to syslog for us
			{
			free(data);
			sdRef->ProcessReply = NULL;
			return kDNSServiceErr_ServiceNotRunning;
			}
		else
			{
			const char *ptr = data;
			cbh.cb_flags     = get_flags     (&ptr, data + cbh.ipc_hdr.datalen);
			cbh.cb_interface = get_uint32    (&ptr, data + cbh.ipc_hdr.datalen);
			cbh.cb_err       = get_error_code(&ptr, data + cbh.ipc_hdr.datalen);

			// CAUTION: We have to handle the case where the client calls DNSServiceRefDeallocate from within the callback function.
			// To do this we set moreptr to point to morebytes. If the client does call DNSServiceRefDeallocate(),
			// then that routine will clear morebytes for us, and cause us to exit our loop.
			morebytes = more_bytes(sdRef->sockfd);
			if (morebytes)
				{
				cbh.cb_flags |= kDNSServiceFlagsMoreComing;
				sdRef->moreptr = &morebytes;
				}
			if (ptr) sdRef->ProcessReply(sdRef, &cbh, ptr, data + cbh.ipc_hdr.datalen);
			// Careful code here:
			// If morebytes is non-zero, that means we set sdRef->moreptr above, and the operation was not
			// cancelled out from under us, so now we need to clear sdRef->moreptr so we don't leave a stray
			// dangling pointer pointing to a long-gone stack variable.
			// If morebytes is zero, then one of two thing happened:
			// (a) morebytes was 0 above, so we didn't set sdRef->moreptr, so we don't need to clear it
			// (b) morebytes was 1 above, and we set sdRef->moreptr, but the operation was cancelled (with DNSServiceRefDeallocate()),
			//     so we MUST NOT try to dereference our stale sdRef pointer.
			if (morebytes) sdRef->moreptr = NULL;
			}
		free(data);
		} while (morebytes);

	return kDNSServiceErr_NoError;
	}

void DNSSD_API DNSServiceRefDeallocate(DNSServiceRef sdRef)
	{
	if (!sdRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRefDeallocate called with NULL DNSServiceRef"); return; }

	if (!DNSServiceRefValid(sdRef))		// Also verifies dnssd_SocketValid(sdRef->sockfd) for us too
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRefDeallocate called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return;
		}

	// If we're in the middle of a DNSServiceProcessResult() invocation for this DNSServiceRef, clear its morebytes flag to break it out of its while loop
	if (sdRef->moreptr) *(sdRef->moreptr) = 0;

	if (sdRef->primary)		// If this is a subordinate DNSServiceOp, just send a 'stop' command
		{
		DNSServiceOp **p = &sdRef->primary->next;
		while (*p && *p != sdRef) p = &(*p)->next;
		if (*p)
			{
			char *ptr;
			size_t len = 0;
			ipc_msg_hdr *hdr = create_hdr(cancel_request, &len, &ptr, 0, sdRef);
			ConvertHeaderBytes(hdr);
			write_all(sdRef->sockfd, (char *)hdr, len);
			free(hdr);
			*p = sdRef->next;
			FreeDNSServiceOp(sdRef);
			}
		}
	else					// else, make sure to terminate all subordinates as well
		{
		dnssd_close(sdRef->sockfd);
		while (sdRef)
			{
			DNSServiceOp *p = sdRef;
			sdRef = sdRef->next;
			FreeDNSServiceOp(p);
			}
		}
	}

DNSServiceErrorType DNSSD_API DNSServiceGetProperty(const char *property, void *result, uint32_t *size)
	{
	char *ptr;
	size_t len = strlen(property) + 1;
	ipc_msg_hdr *hdr;
	DNSServiceOp *tmp;
	uint32_t actualsize;

	DNSServiceErrorType err = ConnectToServer(&tmp, 0, getproperty_request, NULL, NULL, NULL);
	if (err) return err;

	hdr = create_hdr(getproperty_request, &len, &ptr, 0, tmp);
	if (!hdr) { DNSServiceRefDeallocate(tmp); return kDNSServiceErr_NoMemory; }

	put_string(property, &ptr);
	err = deliver_request(hdr, tmp);		// Will free hdr for us
	if (read_all(tmp->sockfd, (char*)&actualsize, (int)sizeof(actualsize)) < 0)
		{ DNSServiceRefDeallocate(tmp); return kDNSServiceErr_ServiceNotRunning; }

	actualsize = ntohl(actualsize);
	if (read_all(tmp->sockfd, (char*)result, actualsize < *size ? actualsize : *size) < 0)
		{ DNSServiceRefDeallocate(tmp); return kDNSServiceErr_ServiceNotRunning; }
	DNSServiceRefDeallocate(tmp);

	// Swap version result back to local process byte order
	if (!strcmp(property, kDNSServiceProperty_DaemonVersion) && *size >= 4)
		*(uint32_t*)result = ntohl(*(uint32_t*)result);

	*size = actualsize;
	return kDNSServiceErr_NoError;
	}

static void handle_resolve_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *end)
	{
	char fullname[kDNSServiceMaxDomainName];
	char target[kDNSServiceMaxDomainName];
	uint16_t txtlen;
	union { uint16_t s; u_char b[2]; } port;
	unsigned char *txtrecord;

	get_string(&data, end, fullname, kDNSServiceMaxDomainName);
	get_string(&data, end, target,   kDNSServiceMaxDomainName);
	if (!data || data + 2 > end) data = NULL;
	else
		{
		port.b[0] = *data++;
		port.b[1] = *data++;
		}
	txtlen = get_uint16(&data, end);
	txtrecord = (unsigned char *)get_rdata(&data, end, txtlen);

	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_resolve_response: error reading result from daemon");
	else ((DNSServiceResolveReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, fullname, target, port.s, txtlen, txtrecord, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceResolve
	(
	DNSServiceRef                 *sdRef,
	DNSServiceFlags               flags,
	uint32_t                      interfaceIndex,
	const char                    *name,
	const char                    *regtype,
	const char                    *domain,
	DNSServiceResolveReply        callBack,
	void                          *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err;

	if (!name || !regtype || !domain || !callBack) return kDNSServiceErr_BadParam;

	err = ConnectToServer(sdRef, flags, resolve_request, handle_resolve_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	// Calculate total message length
	len = sizeof(flags);
	len += sizeof(interfaceIndex);
	len += strlen(name) + 1;
	len += strlen(regtype) + 1;
	len += strlen(domain) + 1;

	hdr = create_hdr(resolve_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(name, &ptr);
	put_string(regtype, &ptr);
	put_string(domain, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

static void handle_query_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	uint32_t ttl;
	char name[kDNSServiceMaxDomainName];
	uint16_t rrtype, rrclass, rdlen;
	const char *rdata;

	get_string(&data, end, name, kDNSServiceMaxDomainName);
	rrtype  = get_uint16(&data, end);
	rrclass = get_uint16(&data, end);
	rdlen   = get_uint16(&data, end);
	rdata   = get_rdata(&data, end, rdlen);
	ttl     = get_uint32(&data, end);

	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_query_response: error reading result from daemon");
	else ((DNSServiceQueryRecordReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, name, rrtype, rrclass, rdlen, rdata, ttl, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceQueryRecord
	(
	DNSServiceRef              *sdRef,
	DNSServiceFlags             flags,
	uint32_t                    interfaceIndex,
	const char                 *name,
	uint16_t                    rrtype,
	uint16_t                    rrclass,
	DNSServiceQueryRecordReply  callBack,
	void                       *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err = ConnectToServer(sdRef, flags, query_request, handle_query_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	if (!name) name = "\0";

	// Calculate total message length
	len = sizeof(flags);
	len += sizeof(uint32_t);  // interfaceIndex
	len += strlen(name) + 1;
	len += 2 * sizeof(uint16_t);  // rrtype, rrclass

	hdr = create_hdr(query_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(name, &ptr);
	put_uint16(rrtype, &ptr);
	put_uint16(rrclass, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

static void handle_addrinfo_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	char hostname[kDNSServiceMaxDomainName];
	uint16_t rrtype, rrclass, rdlen;
	const char *rdata;
	uint32_t ttl;

	get_string(&data, end, hostname, kDNSServiceMaxDomainName);
	rrtype  = get_uint16(&data, end);
	rrclass = get_uint16(&data, end);
	rdlen   = get_uint16(&data, end);
	rdata   = get_rdata (&data, end, rdlen);
	ttl     = get_uint32(&data, end);

	// We only generate client callbacks for A and AAAA results (including NXDOMAIN results for
	// those types, if the client has requested those with the kDNSServiceFlagsReturnIntermediates).
	// Other result types, specifically CNAME referrals, are not communicated to the client, because
	// the DNSServiceGetAddrInfoReply interface doesn't have any meaningful way to communiate CNAME referrals.
	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_addrinfo_response: error reading result from daemon");
	else if (rrtype == kDNSServiceType_A || rrtype == kDNSServiceType_AAAA)
		{
		struct sockaddr_in  sa4;
		struct sockaddr_in6 sa6;
		const struct sockaddr *const sa = (rrtype == kDNSServiceType_A) ? (struct sockaddr*)&sa4 : (struct sockaddr*)&sa6;
		if (rrtype == kDNSServiceType_A)
			{
			memset(&sa4, 0, sizeof(sa4));
			#ifndef NOT_HAVE_SA_LEN
			sa4.sin_len = sizeof(struct sockaddr_in);
			#endif
			sa4.sin_family = AF_INET;
			//  sin_port   = 0;
			if (!cbh->cb_err) memcpy(&sa4.sin_addr, rdata, rdlen);
			}
		else
			{
			memset(&sa6, 0, sizeof(sa6));
			#ifndef NOT_HAVE_SA_LEN
			sa6.sin6_len = sizeof(struct sockaddr_in6);
			#endif
			sa6.sin6_family     = AF_INET6;
			//  sin6_port     = 0;
			//  sin6_flowinfo = 0;
			//  sin6_scope_id = 0;
			if (!cbh->cb_err)
				{
				memcpy(&sa6.sin6_addr, rdata, rdlen);
				if (IN6_IS_ADDR_LINKLOCAL(&sa6.sin6_addr)) sa6.sin6_scope_id = cbh->cb_interface;
				}
			}
		((DNSServiceGetAddrInfoReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, hostname, sa, ttl, sdr->AppContext);
		// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
		}
	}

DNSServiceErrorType DNSSD_API DNSServiceGetAddrInfo
	(
	DNSServiceRef                    *sdRef,
	DNSServiceFlags                  flags,
	uint32_t                         interfaceIndex,
	uint32_t                         protocol,
	const char                       *hostname,
	DNSServiceGetAddrInfoReply       callBack,
	void                             *context          /* may be NULL */
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err;

	if (!hostname) return kDNSServiceErr_BadParam;

	err = ConnectToServer(sdRef, flags, addrinfo_request, handle_addrinfo_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	// Calculate total message length
	len = sizeof(flags);
	len += sizeof(uint32_t);      // interfaceIndex
	len += sizeof(uint32_t);      // protocol
	len += strlen(hostname) + 1;

	hdr = create_hdr(addrinfo_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_uint32(protocol, &ptr);
	put_string(hostname, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}
	
static void handle_browse_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	char replyName[256], replyType[kDNSServiceMaxDomainName], replyDomain[kDNSServiceMaxDomainName];
	get_string(&data, end, replyName, 256);
	get_string(&data, end, replyType, kDNSServiceMaxDomainName);
	get_string(&data, end, replyDomain, kDNSServiceMaxDomainName);
	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_browse_response: error reading result from daemon");
	else ((DNSServiceBrowseReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, replyName, replyType, replyDomain, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceBrowse
	(
	DNSServiceRef         *sdRef,
	DNSServiceFlags        flags,
	uint32_t               interfaceIndex,
	const char            *regtype,
	const char            *domain,
	DNSServiceBrowseReply  callBack,
	void                  *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err = ConnectToServer(sdRef, flags, browse_request, handle_browse_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	if (!domain) domain = "";
	len = sizeof(flags);
	len += sizeof(interfaceIndex);
	len += strlen(regtype) + 1;
	len += strlen(domain) + 1;

	hdr = create_hdr(browse_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(regtype, &ptr);
	put_string(domain, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

DNSServiceErrorType DNSSD_API DNSServiceSetDefaultDomainForUser(DNSServiceFlags flags, const char *domain)
	{
	DNSServiceOp *tmp;
	char *ptr;
	size_t len = sizeof(flags) + strlen(domain) + 1;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err = ConnectToServer(&tmp, 0, setdomain_request, NULL, NULL, NULL);
	if (err) return err;

	hdr = create_hdr(setdomain_request, &len, &ptr, 0, tmp);
	if (!hdr) { DNSServiceRefDeallocate(tmp); return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_string(domain, &ptr);
	err = deliver_request(hdr, tmp);		// Will free hdr for us
	DNSServiceRefDeallocate(tmp);
	return err;
	}

static void handle_regservice_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	char name[256], regtype[kDNSServiceMaxDomainName], domain[kDNSServiceMaxDomainName];
	get_string(&data, end, name, 256);
	get_string(&data, end, regtype, kDNSServiceMaxDomainName);
	get_string(&data, end, domain,  kDNSServiceMaxDomainName);
	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_regservice_response: error reading result from daemon");
	else ((DNSServiceRegisterReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_err, name, regtype, domain, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceRegister
	(
	DNSServiceRef                       *sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	const char                          *name,
	const char                          *regtype,
	const char                          *domain,
	const char                          *host,
	uint16_t                            PortInNetworkByteOrder,
	uint16_t                            txtLen,
	const void                          *txtRecord,
	DNSServiceRegisterReply             callBack,
	void                                *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err;
	union { uint16_t s; u_char b[2]; } port = { PortInNetworkByteOrder };

	if (!name) name = "";
	if (!regtype) return kDNSServiceErr_BadParam;
	if (!domain) domain = "";
	if (!host) host = "";
	if (!txtRecord) txtRecord = (void*)"";

	// No callback must have auto-rename
	if (!callBack && (flags & kDNSServiceFlagsNoAutoRename)) return kDNSServiceErr_BadParam;

	err = ConnectToServer(sdRef, flags, reg_service_request, callBack ? handle_regservice_response : NULL, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	len = sizeof(DNSServiceFlags);
	len += sizeof(uint32_t);  // interfaceIndex
	len += strlen(name) + strlen(regtype) + strlen(domain) + strlen(host) + 4;
	len += 2 * sizeof(uint16_t);  // port, txtLen
	len += txtLen;

	hdr = create_hdr(reg_service_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }
	if (!callBack) hdr->ipc_flags |= IPC_FLAGS_NOREPLY;

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(name, &ptr);
	put_string(regtype, &ptr);
	put_string(domain, &ptr);
	put_string(host, &ptr);
	*ptr++ = port.b[0];
	*ptr++ = port.b[1];
	put_uint16(txtLen, &ptr);
	put_rdata(txtLen, txtRecord, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

static void handle_enumeration_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	char domain[kDNSServiceMaxDomainName];
	get_string(&data, end, domain, kDNSServiceMaxDomainName);
	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_enumeration_response: error reading result from daemon");
	else ((DNSServiceDomainEnumReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, domain, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceEnumerateDomains
	(
	DNSServiceRef             *sdRef,
	DNSServiceFlags            flags,
	uint32_t                   interfaceIndex,
	DNSServiceDomainEnumReply  callBack,
	void                      *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err;

	int f1 = (flags & kDNSServiceFlagsBrowseDomains) != 0;
	int f2 = (flags & kDNSServiceFlagsRegistrationDomains) != 0;
	if (f1 + f2 != 1) return kDNSServiceErr_BadParam;

	err = ConnectToServer(sdRef, flags, enumeration_request, handle_enumeration_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	len = sizeof(DNSServiceFlags);
	len += sizeof(uint32_t);

	hdr = create_hdr(enumeration_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

static void ConnectionResponse(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *const data, const char *const end)
	{
	DNSRecordRef rref = cbh->ipc_hdr.client_context.context;
	(void)data; // Unused

	//printf("ConnectionResponse got %d\n", cbh->ipc_hdr.op);
	if (cbh->ipc_hdr.op != reg_record_reply_op)
		{
		// When using kDNSServiceFlagsShareConnection, need to search the list of associated DNSServiceOps
		// to find the one this response is intended for, and then call through to its ProcessReply handler.
		// We start with our first subordinate DNSServiceRef -- don't want to accidentally match the parent DNSServiceRef.
		DNSServiceOp *op = sdr->next;
		while (op && (op->uid.u32[0] != cbh->ipc_hdr.client_context.u32[0] || op->uid.u32[1] != cbh->ipc_hdr.client_context.u32[1]))
			op = op->next;
		// Note: We may sometimes not find a matching DNSServiceOp, in the case where the client has
		// cancelled the subordinate DNSServiceOp, but there are still messages in the pipeline from the daemon
		if (op && op->ProcessReply) op->ProcessReply(op, cbh, data, end);
		// WARNING: Don't touch op or sdr after this -- client may have called DNSServiceRefDeallocate
		return;
		}

	if (sdr->op == connection_request)
		rref->AppCallback(rref->sdr, rref, cbh->cb_flags, cbh->cb_err, rref->AppContext);
	else
		{
		syslog(LOG_WARNING, "dnssd_clientstub ConnectionResponse: sdr->op != connection_request");
		rref->AppCallback(rref->sdr, rref, 0, kDNSServiceErr_Unknown, rref->AppContext);
		}
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceCreateConnection(DNSServiceRef *sdRef)
	{
	char *ptr;
	size_t len = 0;
	ipc_msg_hdr *hdr;
	DNSServiceErrorType err = ConnectToServer(sdRef, 0, connection_request, ConnectionResponse, NULL, NULL);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL
	
	hdr = create_hdr(connection_request, &len, &ptr, 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}

DNSServiceErrorType DNSSD_API DNSServiceRegisterRecord
	(
	DNSServiceRef                  sdRef,
	DNSRecordRef                  *RecordRef,
	DNSServiceFlags                flags,
	uint32_t                       interfaceIndex,
	const char                    *fullname,
	uint16_t                       rrtype,
	uint16_t                       rrclass,
	uint16_t                       rdlen,
	const void                    *rdata,
	uint32_t                       ttl,
	DNSServiceRegisterRecordReply  callBack,
	void                          *context
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr = NULL;
	DNSRecordRef rref = NULL;
	int f1 = (flags & kDNSServiceFlagsShared) != 0;
	int f2 = (flags & kDNSServiceFlagsUnique) != 0;
	if (f1 + f2 != 1) return kDNSServiceErr_BadParam;

	if (!sdRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRegisterRecord called with NULL DNSServiceRef"); return kDNSServiceErr_BadParam; }

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRegisterRecord called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return kDNSServiceErr_BadReference;
		}

	if (sdRef->op != connection_request)
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRegisterRecord called with non-DNSServiceCreateConnection DNSServiceRef %p %d", sdRef, sdRef->op);
		return kDNSServiceErr_BadReference;
		}

	*RecordRef = NULL;

	len = sizeof(DNSServiceFlags);
	len += 2 * sizeof(uint32_t);  // interfaceIndex, ttl
	len += 3 * sizeof(uint16_t);  // rrtype, rrclass, rdlen
	len += strlen(fullname) + 1;
	len += rdlen;

	hdr = create_hdr(reg_record_request, &len, &ptr, 1, sdRef);
	if (!hdr) return kDNSServiceErr_NoMemory;

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(fullname, &ptr);
	put_uint16(rrtype, &ptr);
	put_uint16(rrclass, &ptr);
	put_uint16(rdlen, &ptr);
	put_rdata(rdlen, rdata, &ptr);
	put_uint32(ttl, &ptr);

	rref = malloc(sizeof(DNSRecord));
	if (!rref) { free(hdr); return kDNSServiceErr_NoMemory; }
	rref->AppContext = context;
	rref->AppCallback = callBack;
	rref->record_index = sdRef->max_index++;
	rref->sdr = sdRef;
	*RecordRef = rref;
	hdr->client_context.context = rref;
	hdr->reg_index = rref->record_index;

	return deliver_request(hdr, sdRef);		// Will free hdr for us
	}

// sdRef returned by DNSServiceRegister()
DNSServiceErrorType DNSSD_API DNSServiceAddRecord
	(
	DNSServiceRef    sdRef,
	DNSRecordRef    *RecordRef,
	DNSServiceFlags  flags,
	uint16_t         rrtype,
	uint16_t         rdlen,
	const void      *rdata,
	uint32_t         ttl
	)
	{
	ipc_msg_hdr *hdr;
	size_t len = 0;
	char *ptr;
	DNSRecordRef rref;

	if (!sdRef)     { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceAddRecord called with NULL DNSServiceRef");        return kDNSServiceErr_BadParam; }
	if (!RecordRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceAddRecord called with NULL DNSRecordRef pointer"); return kDNSServiceErr_BadParam; }
	if (sdRef->op != reg_service_request)
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceAddRecord called with non-DNSServiceRegister DNSServiceRef %p %d", sdRef, sdRef->op);
		return kDNSServiceErr_BadReference;
		}

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceAddRecord called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return kDNSServiceErr_BadReference;
		}

	*RecordRef = NULL;

	len += 2 * sizeof(uint16_t);  // rrtype, rdlen
	len += rdlen;
	len += sizeof(uint32_t);
	len += sizeof(DNSServiceFlags);

	hdr = create_hdr(add_record_request, &len, &ptr, 1, sdRef);
	if (!hdr) return kDNSServiceErr_NoMemory;
	put_flags(flags, &ptr);
	put_uint16(rrtype, &ptr);
	put_uint16(rdlen, &ptr);
	put_rdata(rdlen, rdata, &ptr);
	put_uint32(ttl, &ptr);

	rref = malloc(sizeof(DNSRecord));
	if (!rref) { free(hdr); return kDNSServiceErr_NoMemory; }
	rref->AppContext = NULL;
	rref->AppCallback = NULL;
	rref->record_index = sdRef->max_index++;
	rref->sdr = sdRef;
	*RecordRef = rref;
	hdr->reg_index = rref->record_index;

	return deliver_request(hdr, sdRef);		// Will free hdr for us
	}

// DNSRecordRef returned by DNSServiceRegisterRecord or DNSServiceAddRecord
DNSServiceErrorType DNSSD_API DNSServiceUpdateRecord
	(
	DNSServiceRef    sdRef,
	DNSRecordRef     RecordRef,
	DNSServiceFlags  flags,
	uint16_t         rdlen,
	const void      *rdata,
	uint32_t         ttl
	)
	{
	ipc_msg_hdr *hdr;
	size_t len = 0;
	char *ptr;

	if (!sdRef) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceUpdateRecord called with NULL DNSServiceRef"); return kDNSServiceErr_BadParam; }

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceUpdateRecord called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return kDNSServiceErr_BadReference;
		}

	// Note: RecordRef is allowed to be NULL

	len += sizeof(uint16_t);
	len += rdlen;
	len += sizeof(uint32_t);
	len += sizeof(DNSServiceFlags);

	hdr = create_hdr(update_record_request, &len, &ptr, 1, sdRef);
	if (!hdr) return kDNSServiceErr_NoMemory;
	hdr->reg_index = RecordRef ? RecordRef->record_index : TXT_RECORD_INDEX;
	put_flags(flags, &ptr);
	put_uint16(rdlen, &ptr);
	put_rdata(rdlen, rdata, &ptr);
	put_uint32(ttl, &ptr);
	return deliver_request(hdr, sdRef);		// Will free hdr for us
	}

DNSServiceErrorType DNSSD_API DNSServiceRemoveRecord
	(
	DNSServiceRef    sdRef,
	DNSRecordRef     RecordRef,
	DNSServiceFlags  flags
	)
	{
	ipc_msg_hdr *hdr;
	size_t len = 0;
	char *ptr;
	DNSServiceErrorType err;

	if (!sdRef)            { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRemoveRecord called with NULL DNSServiceRef"); return kDNSServiceErr_BadParam; }
	if (!RecordRef)        { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRemoveRecord called with NULL DNSRecordRef");  return kDNSServiceErr_BadParam; }
	if (!sdRef->max_index) { syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRemoveRecord called with bad DNSServiceRef");  return kDNSServiceErr_BadReference; }

	if (!DNSServiceRefValid(sdRef))
		{
		syslog(LOG_WARNING, "dnssd_clientstub DNSServiceRemoveRecord called with invalid DNSServiceRef %p %08X %08X", sdRef, sdRef->sockfd, sdRef->validator);
		return kDNSServiceErr_BadReference;
		}

	len += sizeof(flags);
	hdr = create_hdr(remove_record_request, &len, &ptr, 1, sdRef);
	if (!hdr) return kDNSServiceErr_NoMemory;
	hdr->reg_index = RecordRef->record_index;
	put_flags(flags, &ptr);
	err = deliver_request(hdr, sdRef);		// Will free hdr for us
	if (!err) free(RecordRef);
	return err;
	}

DNSServiceErrorType DNSSD_API DNSServiceReconfirmRecord
	(
	DNSServiceFlags  flags,
	uint32_t         interfaceIndex,
	const char      *fullname,
	uint16_t         rrtype,
	uint16_t         rrclass,
	uint16_t         rdlen,
	const void      *rdata
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	DNSServiceOp *tmp;

	DNSServiceErrorType err = ConnectToServer(&tmp, flags, reconfirm_record_request, NULL, NULL, NULL);
	if (err) return err;

	len = sizeof(DNSServiceFlags);
	len += sizeof(uint32_t);
	len += strlen(fullname) + 1;
	len += 3 * sizeof(uint16_t);
	len += rdlen;
	hdr = create_hdr(reconfirm_record_request, &len, &ptr, 0, tmp);
	if (!hdr) { DNSServiceRefDeallocate(tmp); return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_string(fullname, &ptr);
	put_uint16(rrtype, &ptr);
	put_uint16(rrclass, &ptr);
	put_uint16(rdlen, &ptr);
	put_rdata(rdlen, rdata, &ptr);

	err = deliver_request(hdr, tmp);		// Will free hdr for us
	DNSServiceRefDeallocate(tmp);
	return err;
	}

static void handle_port_mapping_response(DNSServiceOp *const sdr, const CallbackHeader *const cbh, const char *data, const char *const end)
	{
	union { uint32_t l; u_char b[4]; } addr;
	uint8_t protocol = 0;
	union { uint16_t s; u_char b[2]; } internalPort;
	union { uint16_t s; u_char b[2]; } externalPort;
	uint32_t ttl = 0;

	if (!data || data + 13 > end) data = NULL;
	else
		{
		addr        .b[0] = *data++;
		addr        .b[1] = *data++;
		addr        .b[2] = *data++;
		addr        .b[3] = *data++;
		protocol          = *data++;
		internalPort.b[0] = *data++;
		internalPort.b[1] = *data++;
		externalPort.b[0] = *data++;
		externalPort.b[1] = *data++;
		ttl               = get_uint32(&data, end);
		}

	if (!data) syslog(LOG_WARNING, "dnssd_clientstub handle_port_mapping_response: error reading result from daemon");
	else ((DNSServiceNATPortMappingReply)sdr->AppCallback)(sdr, cbh->cb_flags, cbh->cb_interface, cbh->cb_err, addr.l, protocol, internalPort.s, externalPort.s, ttl, sdr->AppContext);
	// MUST NOT touch sdr after invoking AppCallback -- client is allowed to dispose it from within callback function
	}

DNSServiceErrorType DNSSD_API DNSServiceNATPortMappingCreate
	(
	DNSServiceRef                       *sdRef,
	DNSServiceFlags                     flags,
	uint32_t                            interfaceIndex,
	uint32_t                            protocol,     /* TCP and/or UDP */
	uint16_t                            internalPortInNetworkByteOrder,
	uint16_t                            externalPortInNetworkByteOrder,
	uint32_t                            ttl,          /* time to live in seconds */
	DNSServiceNATPortMappingReply       callBack,
	void                                *context      /* may be NULL */
	)
	{
	char *ptr;
	size_t len;
	ipc_msg_hdr *hdr;
	union { uint16_t s; u_char b[2]; } internalPort = { internalPortInNetworkByteOrder };
	union { uint16_t s; u_char b[2]; } externalPort = { externalPortInNetworkByteOrder };

	DNSServiceErrorType err = ConnectToServer(sdRef, flags, port_mapping_request, handle_port_mapping_response, callBack, context);
	if (err) return err;	// On error ConnectToServer leaves *sdRef set to NULL

	len = sizeof(flags);
	len += sizeof(interfaceIndex);
	len += sizeof(protocol);
	len += sizeof(internalPort);
	len += sizeof(externalPort);
	len += sizeof(ttl);

	hdr = create_hdr(port_mapping_request, &len, &ptr, (*sdRef)->primary ? 1 : 0, *sdRef);
	if (!hdr) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; return kDNSServiceErr_NoMemory; }

	put_flags(flags, &ptr);
	put_uint32(interfaceIndex, &ptr);
	put_uint32(protocol, &ptr);
	*ptr++ = internalPort.b[0];
	*ptr++ = internalPort.b[1];
	*ptr++ = externalPort.b[0];
	*ptr++ = externalPort.b[1];
	put_uint32(ttl, &ptr);

	err = deliver_request(hdr, *sdRef);		// Will free hdr for us
	if (err) { DNSServiceRefDeallocate(*sdRef); *sdRef = NULL; }
	return err;
	}
