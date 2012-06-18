/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

    Change History (most recent first):

$Log: mDNSMacOS9.c,v $
Revision 1.51  2007/09/12 19:23:17  cheshire
Get rid of unnecessary mDNSPlatformTCPIsConnected() routine

Revision 1.50  2007/04/05 20:40:37  cheshire
Remove unused mDNSPlatformTCPGetFlags()

Revision 1.49  2007/03/22 18:31:48  cheshire
Put dst parameter first in mDNSPlatformStrCopy/mDNSPlatformMemCopy, like conventional Posix strcpy/memcpy

Revision 1.48  2007/03/20 17:07:15  cheshire
Rename "struct uDNS_TCPSocket_struct" to "TCPSocket", "struct uDNS_UDPSocket_struct" to "UDPSocket"

Revision 1.47  2006/12/19 22:43:54  cheshire
Fix compiler warnings

Revision 1.46  2006/08/14 23:24:29  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.45  2006/03/19 02:00:14  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.44  2005/09/16 21:06:50  cheshire
Use mDNS_TimeNow_NoLock macro, instead of writing "mDNSPlatformRawTime() + m->timenow_adjust" all over the place

Revision 1.43  2004/12/17 23:37:49  cheshire
<rdar://problem/3485365> Guard against repeating wireless dissociation/re-association
(and other repetitive configuration changes)

Revision 1.42  2004/12/16 20:43:39  cheshire
interfaceinfo.fMask should be interfaceinfo.fNetmask

Revision 1.41  2004/10/16 00:17:00  cheshire
<rdar://problem/3770558> Replace IP TTL 255 check with local subnet source address check

Revision 1.40  2004/09/27 23:56:27  cheshire
Fix infinite loop where mDNSPlatformUnlock() called mDNS_TimeNow(),
and then mDNS_TimeNow() called mDNSPlatformUnlock()

Revision 1.39  2004/09/21 21:02:54  cheshire
Set up ifname before calling mDNS_RegisterInterface()

Revision 1.38  2004/09/17 01:08:50  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.37  2004/09/17 00:19:10  cheshire
For consistency with AllDNSLinkGroupv6, rename AllDNSLinkGroup to AllDNSLinkGroupv4

Revision 1.36  2004/09/16 21:59:16  cheshire
For consistency with zerov6Addr, rename zeroIPAddr to zerov4Addr

Revision 1.35  2004/09/16 00:24:49  cheshire
<rdar://problem/3803162> Fix unsafe use of mDNSPlatformTimeNow()

Revision 1.34  2004/09/14 23:42:36  cheshire
<rdar://problem/3801296> Need to seed random number generator from platform-layer data

Revision 1.33  2004/09/14 23:16:31  cheshire
Fix compile error: mDNS_SetFQDNs has been renamed to mDNS_SetFQDN

Revision 1.32  2004/09/14 21:03:16  cheshire
Fix spacing

Revision 1.31  2004/08/14 03:22:42  cheshire
<rdar://problem/3762579> Dynamic DNS UI <-> mDNSResponder glue
Add GetUserSpecifiedDDNSName() routine
Convert ServiceRegDomain to domainname instead of C string
Replace mDNS_GenerateFQDN/mDNS_GenerateGlobalFQDN with mDNS_SetFQDNs

Revision 1.30  2004/07/29 19:26:03  ksekar
Plaform-level changes for NAT-PMP support

Revision 1.29  2004/05/26 20:53:16  cheshire
Remove unncecessary "return( -1 );" at the end of mDNSPlatformUTC()

Revision 1.28  2004/05/20 18:39:06  cheshire
Fix build broken by addition of mDNSPlatformUTC requirement

Revision 1.27  2004/04/21 02:49:11  cheshire
To reduce future confusion, renamed 'TxAndRx' to 'McastTxRx'

Revision 1.26  2004/04/09 17:43:03  cheshire
Make sure to set the McastTxRx field so that duplicate suppression works correctly

Revision 1.25  2004/03/15 18:55:38  cheshire
Comment out debugging message

Revision 1.24  2004/03/12 21:30:26  cheshire
Build a System-Context Shared Library from mDNSCore, for the benefit of developers
like Muse Research who want to be able to use mDNS/DNS-SD from GPL-licensed code.

Revision 1.23  2004/02/09 23:24:43  cheshire
Need to set TTL 255 to interoperate with peers that check TTL (oops!)

Revision 1.22  2004/01/27 20:15:23  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.21  2004/01/24 04:59:16  cheshire
Fixes so that Posix/Linux, OS9, Windows, and VxWorks targets build again

Revision 1.20  2003/11/14 20:59:09  cheshire
Clients can't use AssignDomainName macro because mDNSPlatformMemCopy is defined in mDNSPlatformFunctions.h.
Best solution is just to combine mDNSEmbeddedAPI.h and mDNSPlatformFunctions.h into a single file.

Revision 1.19  2003/08/18 23:09:20  cheshire
<rdar://problem/3382647> mDNSResponder divide by zero in mDNSPlatformRawTime()

Revision 1.18  2003/08/12 19:56:24  cheshire
Update to APSL 2.0

 */

#include <stdio.h>
#include <stdarg.h>						// For va_list support

#include <LowMem.h>						// For LMGetCurApName()
#include <TextUtils.h>					// For smSystemScript
#include <UnicodeConverter.h>			// For ConvertFromPStringToUnicode()

#include "mDNSEmbeddedAPI.h"				// Defines the interface provided to the client layer above

#include "mDNSMacOS9.h"					// Defines the specific types needed to run mDNS on this platform

// ***************************************************************************
// Constants

static const TSetBooleanOption kReusePortOption =
	{ kOTBooleanOptionSize,          INET_IP, IP_REUSEPORT,      0, true };

// IP_RCVDSTADDR with TSetByteOption/kOTOneByteOptionSize works on OS 9, OS X Classic, and OS 9 Carbon,
// but gives error #-3151 (kOTBadOptionErr) on OS X Carbon.
// If we instead use TSetBooleanOption/kOTBooleanOptionSize then OTOptionManagement on OS X Carbon
// no longer returns -3151 but it still doesn't actually work -- no destination addresses
// are delivered by OTRcvUData. I think it's just a bug in OS X Carbon.
static const TSetByteOption kRcvDestAddrOption =
	{ kOTOneByteOptionSize,          INET_IP, IP_RCVDSTADDR,     0, true };
//static const TSetBooleanOption kRcvDestAddrOption =
//	{ kOTBooleanOptionSize,          INET_IP, IP_RCVDSTADDR,     0, true };

static const TSetByteOption kSetUnicastTTLOption =
	{ kOTOneByteOptionSize,          INET_IP, IP_TTL,            0, 255 };

static const TSetByteOption kSetMulticastTTLOption =
	{ kOTOneByteOptionSize,          INET_IP, IP_MULTICAST_TTL,  0, 255 };

static const TIPAddMulticastOption kAddLinkMulticastOption  =
	{ sizeof(TIPAddMulticastOption), INET_IP, IP_ADD_MEMBERSHIP, 0, { 224,  0,  0,251 }, { 0,0,0,0 } };

//static const TIPAddMulticastOption kAddAdminMulticastOption =
//	{ sizeof(TIPAddMulticastOption), INET_IP, IP_ADD_MEMBERSHIP, 0, { 239,255,255,251 }, { 0,0,0,0 } };

// Bind endpoint to port number. Don't specify any specific IP address --
// we want to receive unicasts on all interfaces, as well as multicasts.
typedef struct { OTAddressType fAddressType; mDNSIPPort fPort; mDNSv4Addr fHost; UInt8 fUnused[8]; } mDNSInetAddress;
//static const mDNSInetAddress mDNSPortInetAddress = { AF_INET, { 0,0 }, { 0,0,0,0 } };	// For testing legacy client support
#define MulticastDNSPortAsNumber 5353
static const mDNSInetAddress mDNSPortInetAddress = { AF_INET, { MulticastDNSPortAsNumber >> 8, MulticastDNSPortAsNumber & 0xFF }, { 0,0,0,0 } };
static const TBind mDNSbindReq = { sizeof(mDNSPortInetAddress), sizeof(mDNSPortInetAddress), (UInt8*)&mDNSPortInetAddress, 0 };

static const TNetbuf zeroTNetbuf = { 0 };

// ***************************************************************************
// Functions

mDNSlocal void SafeDebugStr(unsigned char *buffer)
	{
	int i;
	// Don't want semicolons in MacsBug messages -- they signify commands to execute
	for (i=1; i<= buffer[0]; i++) if (buffer[i] == ';') buffer[i] = '.';
	DebugStr(buffer);
	}

#if MDNS_DEBUGMSGS
mDNSexport void debugf_(const char *format, ...)
	{
	unsigned char buffer[256];
    va_list ptr;
	va_start(ptr,format);
	buffer[0] = (unsigned char)mDNS_vsnprintf((char*)buffer+1, 255, format, ptr);
	va_end(ptr);
#if MDNS_ONLYSYSTEMTASK
	buffer[1+buffer[0]] = 0;
	fprintf(stderr, "%s\n", buffer+1);
	fflush(stderr);
#else
	SafeDebugStr(buffer);
#endif
	}
#endif

#if MDNS_BUILDINGSHAREDLIBRARY >= 2
// When building the non-debug version of the Extension, intended to go on end-user systems, we don't want
// MacsBug breaks for *anything*, not even for the serious LogMsg messages that on OS X would be written to syslog
mDNSexport void LogMsg(const char *format, ...) { (void)format; }
#else
mDNSexport void LogMsg(const char *format, ...)
	{
	unsigned char buffer[256];
    va_list ptr;
	va_start(ptr,format);
	buffer[0] = (unsigned char)mDNS_vsnprintf((char*)buffer+1, 255, format, ptr);
	va_end(ptr);
#if MDNS_ONLYSYSTEMTASK
	buffer[1+buffer[0]] = 0;
	fprintf(stderr, "%s\n", buffer+1);
	fflush(stderr);
#else
	SafeDebugStr(buffer);
#endif
	}
#endif

mDNSexport mStatus mDNSPlatformSendUDP(const mDNS *const m, const void *const msg, const mDNSu8 *const end,
	mDNSInterfaceID InterfaceID, const mDNSAddr *dst, mDNSIPPort dstPort)
	{
	// Note: If we did multi-homing, we'd have to use the InterfaceID parameter to specify from which interface to send this response
	#pragma unused(InterfaceID)

	InetAddress InetDest;
	TUnitData senddata;
	
	if (dst->type != mDNSAddrType_IPv4) return(mStatus_NoError);

	InetDest.fAddressType = AF_INET;
	InetDest.fPort        = dstPort.NotAnInteger;
	InetDest.fHost        = dst->ip.v4.NotAnInteger;

	senddata.addr .maxlen = sizeof(InetDest);
	senddata.addr .len    = sizeof(InetDest);
	senddata.addr .buf    = (UInt8*)&InetDest;
	senddata.opt          = zeroTNetbuf;
	senddata.udata.maxlen = (UInt32)((UInt8*)end - (UInt8*)msg);
	senddata.udata.len    = (UInt32)((UInt8*)end - (UInt8*)msg);
	senddata.udata.buf    = (UInt8*)msg;
	
	return(OTSndUData(m->p->ep, &senddata));
	}

mDNSlocal OSStatus readpacket(mDNS *m)
	{
	mDNSAddr senderaddr, destaddr;
	mDNSInterfaceID interface;
	mDNSIPPort senderport;
	InetAddress sender;
	char options[256];
	DNSMessage packet;
	TUnitData recvdata;
	OTFlags flags = 0;
	OSStatus err;
	
	recvdata.addr .maxlen = sizeof(sender);
	recvdata.addr .len    = 0;
	recvdata.addr .buf    = (UInt8*)&sender;
	recvdata.opt  .maxlen = sizeof(options);
	recvdata.opt  .len    = 0;
	recvdata.opt  .buf    = (UInt8*)&options;
	recvdata.udata.maxlen = sizeof(packet);
	recvdata.udata.len    = 0;
	recvdata.udata.buf    = (UInt8*)&packet;
	
	err = OTRcvUData(m->p->ep, &recvdata, &flags);
	if (err && err != kOTNoDataErr) debugf("OTRcvUData error %d", err);
	
	if (err) return(err);

	senderaddr.type = mDNSAddrType_IPv4;
	senderaddr.ip.v4.NotAnInteger = sender.fHost;
	senderport.NotAnInteger = sender.fPort;
	
	destaddr.type = mDNSAddrType_IPv4;
	destaddr.ip.v4  = zerov4Addr;

	#if OTCARBONAPPLICATION
	// IP_RCVDSTADDR is known to fail on OS X Carbon, so we'll just assume the packet was probably multicast
	destaddr.ip.v4  = AllDNSLinkGroup_v4.ip.v4;
	#endif

	if (recvdata.opt.len)
		{
		TOption *c = nil;
		while (1)
			{
			err = OTNextOption(recvdata.opt.buf, recvdata.opt.len, &c);
			if (err || !c) break;
			if (c->level == INET_IP && c->name == IP_RCVDSTADDR && c->len - kOTOptionHeaderSize == sizeof(destaddr.ip.v4))
				mDNSPlatformMemCopy(&destaddr.ip.v4, c->value, sizeof(destaddr.ip.v4));
			}
		}

	interface = m->HostInterfaces->InterfaceID;

	if      (flags & T_MORE)                                debugf("ERROR: OTRcvUData() buffer too small (T_MORE set)");
	else if (recvdata.addr.len < sizeof(InetAddress))       debugf("ERROR: recvdata.addr.len (%d) too short", recvdata.addr.len);
	else mDNSCoreReceive(m, &packet, recvdata.udata.buf + recvdata.udata.len, &senderaddr, senderport, &destaddr, MulticastDNSPort, interface);
	
	return(err);
	}

mDNSexport TCPSocket *mDNSPlatformTCPSocket(mDNS * const m, TCPSocketFlags flags, mDNSIPPort * port)
	{
	(void)m;			// Unused
	(void)flags;		// Unused
	(void)port;			// Unused
	return NULL;
	}

mDNSexport TCPSocket *mDNSPlatformTCPAccept(TCPSocketFlags flags, int sd)
	{
	(void)flags;		// Unused
	(void)sd;			// Unused
	return NULL;
	}

mDNSexport int mDNSPlatformTCPGetFD(TCPSocket *sock)
	{
	(void)sock;			// Unused
	return -1;
	}

mDNSexport mStatus mDNSPlatformTCPConnect(TCPSocket *sock, const mDNSAddr * dst, mDNSOpaque16 dstport, mDNSInterfaceID InterfaceID,
                                      TCPConnectionCallback callback, void * context)
	{
	(void)sock;			// Unused
	(void)dst;			// Unused
	(void)dstport;		// Unused
	(void)InterfaceID;	// Unused
	(void)callback;		// Unused
	(void)context;		// Unused
	return(mStatus_UnsupportedErr);
	}

mDNSexport void mDNSPlatformTCPCloseConnection(TCPSocket *sd)
	{
	(void)sd;			// Unused
	}

mDNSexport long mDNSPlatformReadTCP(TCPSocket *sock, void *buf, unsigned long buflen, mDNSBool *closed)
	{
	(void)sock;			// Unused
	(void)buf;			// Unused
	(void)buflen;		// Unused
	(void)closed;		// Unused
	return(0);
	}

mDNSexport long mDNSPlatformWriteTCP(TCPSocket *sock, const char *msg, unsigned long len)
	{
	(void)sock;			// Unused
	(void)msg;			// Unused
	(void)len;			// Unused
	return(0);
	}

mDNSexport UDPSocket *mDNSPlatformUDPSocket(mDNS * const m, mDNSIPPort port)
	{
	(void)m;			// Unused
	(void)port;			// Unused
	return NULL;
	}

mDNSexport void           mDNSPlatformUDPClose(UDPSocket *sock)
	{
	(void)sock;			// Unused
	}

mDNSlocal void mDNSOptionManagement(mDNS *const m)
	{
	OSStatus err;

	// Make sure the length in the TNetbuf agrees with the length in the TOptionHeader
	m->p->optReq.opt.len    = m->p->optBlock.h.len;
	m->p->optReq.opt.maxlen = m->p->optBlock.h.len;
	if (m->p->optReq.opt.maxlen < 4)
		m->p->optReq.opt.maxlen = 4;

	err = OTOptionManagement(m->p->ep, &m->p->optReq, NULL);
	if (err) LogMsg("OTOptionManagement failed %d", err);
	}

mDNSlocal void mDNSinitComplete(mDNS *const m, mStatus result)
	{
	m->mDNSPlatformStatus = result;
	mDNSCoreInitComplete(m, mStatus_NoError);
	}

mDNSlocal pascal void mDNSNotifier(void *contextPtr, OTEventCode code, OTResult result, void *cookie)
	{
	mDNS *const m = (mDNS *const)contextPtr;
	if (!m) debugf("mDNSNotifier FATAL ERROR! No context");
	switch (code)
		{
		case T_OPENCOMPLETE:
			{
			OSStatus err;
			InetInterfaceInfo interfaceinfo;
			if (result) { LogMsg("T_OPENCOMPLETE failed %d", result); mDNSinitComplete(m, result); return; }
			//debugf("T_OPENCOMPLETE");
			m->p->ep = (EndpointRef)cookie;
			//debugf("OTInetGetInterfaceInfo");
			// (In future may want to loop over all interfaces instead of just using kDefaultInetInterface)
			err = OTInetGetInterfaceInfo(&interfaceinfo, kDefaultInetInterface);
			if (err) { LogMsg("OTInetGetInterfaceInfo failed %d", err); mDNSinitComplete(m, err); return; }

			// Make our basic standard host resource records (address, PTR, etc.)
			m->p->interface.InterfaceID             = (mDNSInterfaceID)&m->p->interface;
			m->p->interface.ip  .type               = mDNSAddrType_IPv4;
			m->p->interface.ip  .ip.v4.NotAnInteger = interfaceinfo.fAddress;
			m->p->interface.mask.type               = mDNSAddrType_IPv4;
			m->p->interface.mask.ip.v4.NotAnInteger = interfaceinfo.fNetmask;
			m->p->interface.ifname[0]               = 0;
			m->p->interface.Advertise               = m->AdvertiseLocalAddresses;
			m->p->interface.McastTxRx               = mDNStrue;
			}
			
		case T_OPTMGMTCOMPLETE:
		case T_BINDCOMPLETE:
			// IP_RCVDSTADDR is known to fail on OS X Carbon, so we don't want to abort for that error
			// (see comment above at the definition of kRcvDestAddrOption)
			#if OTCARBONAPPLICATION
			if (result && m->p->mOTstate == mOT_RcvDestAddr)
				LogMsg("Carbon IP_RCVDSTADDR option failed %d; continuing anyway", result);
			else
			#endif
			if (result) { LogMsg("T_OPTMGMTCOMPLETE/T_BINDCOMPLETE %d failed %d", m->p->mOTstate, result); mDNSinitComplete(m, result); return; }
			//LogMsg("T_OPTMGMTCOMPLETE/T_BINDCOMPLETE %d", m->p->mOTstate);
			switch (++m->p->mOTstate)
				{
				case mOT_ReusePort:		m->p->optBlock.b = kReusePortOption;         mDNSOptionManagement(m); break;
				case mOT_RcvDestAddr:	m->p->optBlock.i = kRcvDestAddrOption;       mDNSOptionManagement(m); break;
				case mOT_SetUTTL:		m->p->optBlock.i = kSetUnicastTTLOption;     mDNSOptionManagement(m); break;
				case mOT_SetMTTL:		m->p->optBlock.i = kSetMulticastTTLOption;   mDNSOptionManagement(m); break;
				case mOT_LLScope:		m->p->optBlock.m = kAddLinkMulticastOption;  mDNSOptionManagement(m); break;
//				case mOT_AdminScope:	m->p->optBlock.m = kAddAdminMulticastOption; mDNSOptionManagement(m); break;
				case mOT_Bind:			OTBind(m->p->ep, (TBind*)&mDNSbindReq, NULL); break;
				case mOT_Ready:         mDNSinitComplete(m, mStatus_NoError);
										// Can't do mDNS_RegisterInterface until *after* mDNSinitComplete has set m->mDNSPlatformStatus to mStatus_NoError
										mDNS_RegisterInterface(m, &m->p->interface, mDNSfalse);
										break;
				default:                LogMsg("Unexpected m->p->mOTstate %d", m->p->mOTstate-1);
				}
			break;

		case T_DATA:
			//debugf("T_DATA");
			while (readpacket(m) == kOTNoError) continue;	// Read packets until we run out
			break;

		case kOTProviderWillClose: LogMsg("kOTProviderWillClose"); break;
		case kOTProviderIsClosed:		// Machine is going to sleep, shutting down, or reconfiguring IP
			LogMsg("kOTProviderIsClosed");
			if (m->p->mOTstate == mOT_Ready)
				{
				m->p->mOTstate = mOT_Closed;
				mDNS_DeregisterInterface(m, &m->p->interface, mDNSfalse);
				}
			if (m->p->ep) { OTCloseProvider(m->p->ep); m->p->ep = NULL; }
			break;						// Do we need to do anything?

		default: debugf("mDNSNotifier: Unexpected OTEventCode %X", code);
			break;
		}
	}

#if MDNS_ONLYSYSTEMTASK

static Boolean     ONLYSYSTEMTASKevent;
static void       *ONLYSYSTEMTASKcontextPtr;
static OTEventCode ONLYSYSTEMTASKcode;
static OTResult    ONLYSYSTEMTASKresult;
static void       *ONLYSYSTEMTASKcookie;

mDNSlocal pascal void CallmDNSNotifier(void *contextPtr, OTEventCode code, OTResult result, void *cookie)
	{
	ONLYSYSTEMTASKcontextPtr = contextPtr;
	ONLYSYSTEMTASKcode       = code;
	ONLYSYSTEMTASKresult     = result;
	ONLYSYSTEMTASKcookie     = cookie;
	}

#else

mDNSlocal pascal void CallmDNSNotifier(void *contextPtr, OTEventCode code, OTResult result, void *cookie)
	{
	mDNS *const m = (mDNS *const)contextPtr;
	if (!m) debugf("mDNSNotifier FATAL ERROR! No context");
	if (m->p->nesting) LogMsg("CallmDNSNotifier ERROR! OTEnterNotifier is supposed to suppress notifier callbacks");
	mDNSNotifier(contextPtr, code, result, cookie);
	}

#endif

static OTNotifyUPP CallmDNSNotifierUPP;

mDNSlocal OSStatus mDNSOpenEndpoint(const mDNS *const m)
	{
	OSStatus err;
	// m->optReq is pre-set to point to the shared m->optBlock
	// m->optBlock is filled in by each OTOptionManagement call
	m->p->optReq.opt.maxlen = sizeof(m->p->optBlock);
	m->p->optReq.opt.len    = sizeof(m->p->optBlock);
	m->p->optReq.opt.buf    = (UInt8*)&m->p->optBlock;
	m->p->optReq.flags      = T_NEGOTIATE;

	// Open an endpoint and start answering queries
	//printf("Opening endpoint now...\n");
	m->p->ep = NULL;
	m->p->mOTstate = mOT_Start;
	err = OTAsyncOpenEndpoint(OTCreateConfiguration(kUDPName), 0, NULL, CallmDNSNotifierUPP, (void*)m);
	if (err) { LogMsg("ERROR: OTAsyncOpenEndpoint(UDP) failed with error <%d>", err); return(err); }
	return(kOTNoError);
	}

// Define these here because they're not in older versions of OpenTransport.h
enum
	{
	xOTStackIsLoading   = 0x27000001,	/* Sent before Open Transport attempts to load the TCP/IP protocol stack.*/
	xOTStackWasLoaded   = 0x27000002,	/* Sent after the TCP/IP stack has been successfully loaded.*/
	xOTStackIsUnloading = 0x27000003	/* Sent before Open Transport unloads the TCP/IP stack.*/
	};

static mDNS *ClientNotifierContext;

mDNSlocal pascal void ClientNotifier(void *contextPtr, OTEventCode code, OTResult result, void *cookie)
	{
	mDNS *const m = ClientNotifierContext;

	#pragma unused(contextPtr)		// Usually zero (except one in the 'xOTStackIsLoading' case)
	#pragma unused(cookie)			// Usually 'ipv4' (except for kOTPortNetworkChange)
	#pragma unused(result)			// Usually zero

	switch (code)
		{
		case xOTStackIsLoading:		break;
		case xOTStackWasLoaded:		if (m->p->mOTstate == mOT_Closed)
										{
										LogMsg("kOTStackWasLoaded: Re-opening endpoint");
										if (m->p->ep)
											LogMsg("kOTStackWasLoaded: ERROR: m->p->ep already set");
										m->mDNSPlatformStatus = mStatus_Waiting;
										m->p->mOTstate = mOT_Reset;
										#if !MDNS_ONLYSYSTEMTASK
										mDNSOpenEndpoint(m);
										#endif
										}
									else
										LogMsg("kOTStackWasLoaded (no action)");
									break;
		case xOTStackIsUnloading:	break;
		case kOTPortNetworkChange:	break;
		default: debugf("ClientNotifier unknown code %X, %X, %d", contextPtr, code, result); break;
		}
	}

#if TARGET_API_MAC_CARBON

mDNSlocal void GetUserSpecifiedComputerName(domainlabel *const namelabel)
	{
	CFStringRef cfs = CSCopyMachineName();
	CFStringGetPascalString(cfs, namelabel->c, sizeof(*namelabel), kCFStringEncodingUTF8);
	CFRelease(cfs);
	}

#else

mDNSlocal OSStatus ConvertStringHandleToUTF8(const StringHandle machineName, UInt8 *const utf8, ByteCount maxlen)
	{
	OSStatus status;
	TextEncoding utf8TextEncoding, SystemTextEncoding;
	UnicodeMapping theMapping;
	TextToUnicodeInfo textToUnicodeInfo;		
	ByteCount unicodelen = 0;
	
	if (maxlen > 255) maxlen = 255;	// Can't put more than 255 in a Pascal String

	utf8TextEncoding = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicodeUTF8Format);
	UpgradeScriptInfoToTextEncoding(smSystemScript, kTextLanguageDontCare, kTextRegionDontCare, NULL, &SystemTextEncoding);
	theMapping.unicodeEncoding = utf8TextEncoding;
	theMapping.otherEncoding   = SystemTextEncoding;
	theMapping.mappingVersion  = kUnicodeUseLatestMapping;
	status = CreateTextToUnicodeInfo(&theMapping, &textToUnicodeInfo);
	if (status == noErr)
		{
		status = ConvertFromPStringToUnicode(textToUnicodeInfo, *machineName, maxlen, &unicodelen, (UniCharArrayPtr)&(utf8[1]));
		DisposeTextToUnicodeInfo(&textToUnicodeInfo);
		}
	utf8[0] = (UInt8)unicodelen;
	return(status);
	}

mDNSlocal void GetUserSpecifiedComputerName(domainlabel *const namelabel)
	{
	StringHandle machineName = GetString(-16413);	// Get machine name set in file sharing
	if (machineName)
		{
		char machineNameState = HGetState((Handle)machineName);
		HLock((Handle)machineName);
		ConvertStringHandleToUTF8(machineName, namelabel->c, MAX_DOMAIN_LABEL);
		HSetState((Handle)machineName, machineNameState);
		}
	}

#endif

static pascal void mDNSTimerTask(void *arg)
	{
#if MDNS_ONLYSYSTEMTASK
#pragma unused(arg)
	ONLYSYSTEMTASKevent = true;
#else
	mDNS *const m = (mDNS *const)arg;
	if (!m->p->ep) LogMsg("mDNSTimerTask NO endpoint");
	if (m->mDNS_busy) LogMsg("mDNS_busy");
	if (m->p->nesting) LogMsg("mDNSTimerTask ERROR! OTEnterNotifier is supposed to suppress timer callbacks too");
	
	// If our timer fires at a time when we have no endpoint, ignore it --
	// once we reopen our endpoint and get our T_BINDCOMPLETE message we'll call
	// mDNS_RegisterInterface(), which does a lock/unlock, which retriggers the timer.
	// Likewise, if m->mDNS_busy or m->p->nesting, we'll catch this on the unlock
	if (m->p->ep && m->mDNS_busy == 0 && m->p->nesting == 0) mDNS_Execute(m);
#endif
	}

#if TEST_SLEEP
long sleep, wake, mode;
#endif

mDNSexport mStatus mDNSPlatformInit(mDNS *const m)
	{
	OSStatus err = InitOpenTransport();

	ClientNotifierContext = m;
	// Note: OTRegisterAsClient returns kOTNotSupportedErr when running as Carbon code on OS X
	// -- but that's okay, we don't need a ClientNotifier when running as Carbon code on OS X
	OTRegisterAsClient(NULL, NewOTNotifyUPP(ClientNotifier));
	
	m->p->OTTimerTask = OTCreateTimerTask(NewOTProcessUPP(mDNSTimerTask), m);
	m->p->nesting     = 0;

#if TEST_SLEEP
	sleep = TickCount() + 600;
	wake = TickCount() + 1200;
	mode = 0;
#endif

	// Set up the nice label
	m->nicelabel.c[0] = 0;
	GetUserSpecifiedComputerName(&m->nicelabel);
//	m->nicelabel = *(domainlabel*)"\pStu";	// For conflict testing
	if (m->nicelabel.c[0] == 0) MakeDomainLabelFromLiteralString(&m->nicelabel, "Macintosh");

	// Set up the RFC 1034-compliant label
	m->hostlabel.c[0] = 0;
	ConvertUTF8PstringToRFC1034HostLabel(m->nicelabel.c, &m->hostlabel);
	if (m->hostlabel.c[0] == 0) MakeDomainLabelFromLiteralString(&m->hostlabel, "Macintosh");

	mDNS_SetFQDN(m);

	// When it's finished mDNSOpenEndpoint asynchronously calls mDNSinitComplete() and then mDNS_RegisterInterface()
	CallmDNSNotifierUPP = NewOTNotifyUPP(CallmDNSNotifier);
	err = mDNSOpenEndpoint(m);
	if (err)
		{
		LogMsg("mDNSOpenEndpoint failed %d", err);
		if (m->p->OTTimerTask) OTDestroyTimerTask(m->p->OTTimerTask);
		OTUnregisterAsClient();
		CloseOpenTransport();
		}
	return(err);
	}

extern void mDNSPlatformClose (mDNS *const m)
	{
	if (m->p->mOTstate == mOT_Ready)
		{
		m->p->mOTstate = mOT_Closed;
		mDNS_DeregisterInterface(m, &m->p->interface, mDNSfalse);
		}
	if (m->p->ep)          { OTCloseProvider   (m->p->ep);          m->p->ep          = NULL; }
	if (m->p->OTTimerTask) { OTDestroyTimerTask(m->p->OTTimerTask); m->p->OTTimerTask = 0;    }

	OTUnregisterAsClient();
	CloseOpenTransport();
	}

#if MDNS_ONLYSYSTEMTASK
extern void mDNSPlatformIdle(mDNS *const m);
mDNSexport void mDNSPlatformIdle(mDNS *const m)
	{
	while (ONLYSYSTEMTASKcontextPtr)
		{
		void *contextPtr = ONLYSYSTEMTASKcontextPtr;
		ONLYSYSTEMTASKcontextPtr = NULL;
		mDNSNotifier(contextPtr, ONLYSYSTEMTASKcode, ONLYSYSTEMTASKresult, ONLYSYSTEMTASKcookie);
		}
	if (ONLYSYSTEMTASKevent)
		{
		ONLYSYSTEMTASKevent = false;
		mDNS_Execute(m);
		}

	if (m->p->mOTstate == mOT_Reset)
		{
		printf("\n");
		printf("******************************************************************************\n");
		printf("\n");
		printf("Reopening endpoint\n");
		mDNSOpenEndpoint(m);
		m->ResourceRecords = NULL;
		}

#if TEST_SLEEP
	switch (mode)
		{
		case 0: if ((long)TickCount() - sleep >= 0) { mDNSCoreMachineSleep(m, 1); mode++; }
				break;
		case 1: if ((long)TickCount() - wake >= 0) { mDNSCoreMachineSleep(m, 0); mode++; }
				break;
		}
#endif
	}
#endif

mDNSexport void    mDNSPlatformLock(const mDNS *const m)
	{
	if (!m) { DebugStr("\pmDNSPlatformLock m NULL!"); return; }
	if (!m->p) { DebugStr("\pmDNSPlatformLock m->p NULL!"); return; }

	// If we try to call OTEnterNotifier and fail because we're already running at
	// Notifier context, then make sure we don't do the matching OTLeaveNotifier() on exit.
	// If we haven't even opened our endpoint yet, then just increment m->p->nesting for the same reason
	if (m->p->mOTstate == mOT_Ready && !m->p->ep) DebugStr("\pmDNSPlatformLock: m->p->mOTstate == mOT_Ready && !m->p->ep");
	if (!m->p->ep || m->p->nesting || OTEnterNotifier(m->p->ep) == false) m->p->nesting++;
	}

mDNSlocal void ScheduleNextTimerCallback(const mDNS *const m)
	{
	if (m->mDNSPlatformStatus == mStatus_NoError)
		{
		SInt32 interval = m->NextScheduledEvent - mDNS_TimeNow_NoLock(m);
		if      (interval < 1)                 interval = 1;
		else if (interval > 0x70000000 / 1000) interval = 0x70000000 / mDNSPlatformOneSecond;
		else                                   interval = (interval * 1000 + mDNSPlatformOneSecond-1)/ mDNSPlatformOneSecond;
		OTScheduleTimerTask(m->p->OTTimerTask, (OTTimeout)interval);
		}
	}

mDNSexport void    mDNSPlatformUnlock(const mDNS *const m)
	{
	if (!m) { DebugStr("\pmDNSPlatformUnlock m NULL!"); return; }
	if (!m->p) { DebugStr("\pmDNSPlatformUnlock m->p NULL!"); return; }

	if (m->p->ep && m->mDNS_busy == 0) ScheduleNextTimerCallback(m);

	if (m->p->nesting) m->p->nesting--;
	else OTLeaveNotifier(m->p->ep);
	}

mDNSexport void     mDNSPlatformStrCopy(      void *dst, const void *src)             { OTStrCopy((char*)dst, (char*)src); }
mDNSexport UInt32   mDNSPlatformStrLen (                 const void *src)             { return(OTStrLength((char*)src)); }
mDNSexport void     mDNSPlatformMemCopy(      void *dst, const void *src, UInt32 len) { OTMemcpy(dst, src, len); }
mDNSexport mDNSBool mDNSPlatformMemSame(const void *dst, const void *src, UInt32 len) { return(OTMemcmp(dst, src, len)); }
mDNSexport void     mDNSPlatformMemZero(      void *dst,                  UInt32 len) { OTMemzero(dst, len); }
mDNSexport void *   mDNSPlatformMemAllocate(mDNSu32 len)                              { return(OTAllocMem(len)); }
mDNSexport void     mDNSPlatformMemFree(void *mem)                                    { OTFreeMem(mem); }
mDNSexport mDNSu32  mDNSPlatformRandomSeed(void)                                      { return(TickCount()); }
mDNSexport mStatus  mDNSPlatformTimeInit(void)                                        { return(mStatus_NoError); }
mDNSexport SInt32   mDNSPlatformRawTime()                                             { return((SInt32)TickCount()); }
mDNSexport SInt32   mDNSPlatformOneSecond = 60;

mDNSexport mDNSs32	mDNSPlatformUTC(void)
	{
	// Classic Mac OS since Midnight, 1st Jan 1904
	// Standard Unix counts from 1970
	// This value adjusts for the 66 years and 17 leap-days difference
	mDNSu32 SecsSince1904;
	MachineLocation ThisLocation;
	#define TIME_ADJUST (((1970 - 1904) * 365 + 17) * 24 * 60 * 60)
	#define ThisLocationGMTdelta ((ThisLocation.u.gmtDelta << 8) >> 8)
	GetDateTime(&SecsSince1904);
	ReadLocation(&ThisLocation);
	return((mDNSs32)(SecsSince1904 - ThisLocationGMTdelta - TIME_ADJUST));
	}
