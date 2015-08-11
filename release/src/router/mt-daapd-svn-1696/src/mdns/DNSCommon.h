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

$Log: DNSCommon.h,v $
Revision 1.34.2.1  2006/08/29 06:24:22  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.34  2006/03/18 21:47:56  cheshire
<rdar://problem/4073825> Improve logic for delaying packets after repeated interface transitions

Revision 1.33  2006/03/10 21:51:41  cheshire
<rdar://problem/4111464> After record update, old record sometimes remains in cache
Split out SameRDataBody() into a separate routine so it can be called from other code

Revision 1.32  2005/03/21 00:33:51  shersche
<rdar://problem/4021486> Fix build warnings on Win32 platform

Revision 1.31  2005/02/18 00:43:11  cheshire
<rdar://problem/4010245> mDNSResponder should auto-truncate service names that are too long

Revision 1.30  2005/01/19 03:12:44  cheshire
Move LocalRecordReady() macro from mDNS.c to DNSCommon.h

Revision 1.29  2004/12/15 02:11:22  ksekar
<rdar://problem/3917317> Don't check for Dynamic DNS hostname uniqueness

Revision 1.28  2004/12/06 21:15:22  ksekar
<rdar://problem/3884386> mDNSResponder crashed in CheckServiceRegistrations

Revision 1.27  2004/12/03 07:20:50  ksekar
<rdar://problem/3674208> Wide-Area: Registration of large TXT record fails

Revision 1.26  2004/12/03 05:18:33  ksekar
<rdar://problem/3810596> mDNSResponder needs to return more specific TSIG errors

Revision 1.25  2004/10/26 03:52:02  cheshire
Update checkin comments

Revision 1.24  2004/10/23 01:16:00  cheshire
<rdar://problem/3851677> uDNS operations not always reliable on multi-homed hosts

Revision 1.23  2004/10/03 23:18:58  cheshire
Move address comparison macros from DNSCommon.h to mDNSEmbeddedAPI.h

Revision 1.22  2004/09/30 00:24:56  ksekar
<rdar://problem/3695802> Dynamically update default registration domains on config change

Revision 1.21  2004/09/17 01:08:48  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.20  2004/09/17 00:49:51  cheshire
Get rid of now-unused GetResourceRecord -- the correct (safe) routine to use
is GetLargeResourceRecord

Revision 1.19  2004/09/16 21:59:15  cheshire
For consistency with zerov6Addr, rename zeroIPAddr to zerov4Addr

Revision 1.18  2004/09/16 02:29:39  cheshire
Moved mDNS_Lock/mDNS_Unlock to DNSCommon.c; Added necessary locking around
uDNS_ReceiveMsg, uDNS_StartQuery, uDNS_UpdateRecord, uDNS_RegisterService

Revision 1.17  2004/09/14 23:27:46  cheshire
Fix compile errors

Revision 1.16  2004/08/13 23:46:58  cheshire
"asyncronous" -> "asynchronous"

Revision 1.15  2004/08/10 23:19:14  ksekar
<rdar://problem/3722542>: DNS Extension daemon for Wide Area Service Discovery
Moved routines/constants to allow extern access for garbage collection daemon

Revision 1.14  2004/05/28 23:42:36  ksekar
<rdar://problem/3258021>: Feature: DNS server->client notification on record changes (#7805)

Revision 1.13  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.12  2004/04/22 04:03:59  cheshire
Headers should use "extern" declarations, not "mDNSexport"

Revision 1.11  2004/04/14 23:09:28  ksekar
Support for TSIG signed dynamic updates.

Revision 1.10  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.9  2004/02/21 08:56:58  bradley
Wrap prototypes with extern "C" for C++ builds.

Revision 1.8  2004/02/06 23:04:18  ksekar
Basic Dynamic Update support via mDNS_Register (dissabled via
UNICAST_REGISTRATION #define)

Revision 1.7  2004/02/03 19:47:36  ksekar
Added an asynchronous state machine mechanism to uDNS.c, including
calls to find the parent zone for a domain name.  Changes include code
in repository previously dissabled via "#if 0 incomplete".  Codepath
is currently unused, and will be called to create update records, etc.

Revision 1.6  2004/01/27 20:15:22  cheshire
<rdar://problem/3541288>: Time to prune obsolete code for listening on port 53

Revision 1.5  2004/01/24 03:40:56  cheshire
Move mDNSAddrIsDNSMulticast() from DNSCommon.h to mDNSEmbeddedAPI.h so embedded clients can use it

Revision 1.4  2004/01/24 03:38:27  cheshire
Fix minor syntactic error: Headers should use "extern" declarations, not "mDNSexport"

Revision 1.3  2004/01/23 23:23:14  ksekar
Added TCP support for truncated unicast messages.

Revision 1.2  2004/01/21 21:12:23  cheshire
Add missing newline at end of file to make Unix tools happier

Revision 1.1  2003/12/13 03:05:27  ksekar
<rdar://problem/3192548>: DynDNS: Unicast query of service records

 
 */

#ifndef __DNSCOMMON_H_
#define __DNSCOMMON_H_

#include "mDNSEmbeddedAPI.h"

#ifdef	__cplusplus
	extern "C" {
#endif


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark - DNS Protocol Constants
#endif

typedef enum
	{
	kDNSFlag0_QR_Mask     = 0x80,		// Query or response?
	kDNSFlag0_QR_Query    = 0x00,
	kDNSFlag0_QR_Response = 0x80,
	
	kDNSFlag0_OP_Mask     = 0x78,		// Operation type
	kDNSFlag0_OP_StdQuery = 0x00,
	kDNSFlag0_OP_Iquery   = 0x08,
	kDNSFlag0_OP_Status   = 0x10,
	kDNSFlag0_OP_Unused3  = 0x18,
	kDNSFlag0_OP_Notify   = 0x20,
	kDNSFlag0_OP_Update   = 0x28,
	
	kDNSFlag0_QROP_Mask   = kDNSFlag0_QR_Mask | kDNSFlag0_OP_Mask,
	
	kDNSFlag0_AA          = 0x04,		// Authoritative Answer?
	kDNSFlag0_TC          = 0x02,		// Truncated?
	kDNSFlag0_RD          = 0x01,		// Recursion Desired?
	kDNSFlag1_RA          = 0x80,		// Recursion Available?
	
	kDNSFlag1_Zero        = 0x40,		// Reserved; must be zero
	kDNSFlag1_AD          = 0x20,		// Authentic Data [RFC 2535]
	kDNSFlag1_CD          = 0x10,		// Checking Disabled [RFC 2535]

	kDNSFlag1_RC          = 0x0F,		// Response code
	kDNSFlag1_RC_NoErr    = 0x00,
	kDNSFlag1_RC_FmtErr   = 0x01,
	kDNSFlag1_RC_SrvErr   = 0x02,
	kDNSFlag1_RC_NXDomain = 0x03,
	kDNSFlag1_RC_NotImpl  = 0x04,
	kDNSFlag1_RC_Refused  = 0x05,
	kDNSFlag1_RC_YXDomain = 0x06,
	kDNSFlag1_RC_YXRRSet  = 0x07,
	kDNSFlag1_RC_NXRRSet  = 0x08,
	kDNSFlag1_RC_NotAuth  = 0x09,
	kDNSFlag1_RC_NotZone  = 0x0A
	} DNS_Flags;

typedef enum
	{
	TSIG_ErrBadSig  = 16,
	TSIG_ErrBadKey  = 17,
	TSIG_ErrBadTime = 18
	} TSIG_ErrorCode;
	

// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - General Utility Functions
#endif

extern const NetworkInterfaceInfo *GetFirstActiveInterface(const NetworkInterfaceInfo *intf);
extern mDNSInterfaceID GetNextActiveInterfaceID(const NetworkInterfaceInfo *intf);

extern mDNSu32 mDNSRandom(mDNSu32 max);
extern mDNSu32 mDNSRandomFromFixedSeed(mDNSu32 seed, mDNSu32 max);


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Domain Name Utility Functions
#endif
	
#define mdnsIsDigit(X)     ((X) >= '0' && (X) <= '9')
#define mDNSIsUpperCase(X) ((X) >= 'A' && (X) <= 'Z')
#define mDNSIsLowerCase(X) ((X) >= 'a' && (X) <= 'z')
#define mdnsIsLetter(X)    (mDNSIsUpperCase(X) || mDNSIsLowerCase(X))
	
#define mdnsValidHostChar(X, notfirst, notlast) (mdnsIsLetter(X) || mdnsIsDigit(X) || ((notfirst) && (notlast) && (X) == '-') )

extern mDNSu16 CompressedDomainNameLength(const domainname *const name, const domainname *parent);

extern mDNSu32 TruncateUTF8ToLength(mDNSu8 *string, mDNSu32 length, mDNSu32 max);
extern mDNSBool LabelContainsSuffix(const domainlabel *const name, const mDNSBool RichText);
extern mDNSu32 RemoveLabelSuffix(domainlabel *name, mDNSBool RichText);
extern void AppendLabelSuffix(domainlabel *name, mDNSu32 val, mDNSBool RichText);
extern void mDNS_HostNameCallback(mDNS *const m, AuthRecord *const rr, mStatus result);
#define ValidateDomainName(N) (DomainNameLength(N) <= MAX_DOMAIN_NAME)


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - Resource Record Utility Functions
#endif

extern mDNSu32 RDataHashValue(mDNSu16 const rdlength, const RDataBody *const rdb);

extern mDNSBool SameRDataBody(const ResourceRecord *const r1, const RDataBody *const r2);
extern mDNSBool SameRData(const ResourceRecord *const r1, const ResourceRecord *const r2);

extern mDNSBool ResourceRecordAnswersQuestion(const ResourceRecord *const rr, const DNSQuestion *const q);

extern mDNSBool SameResourceRecord(ResourceRecord *r1, ResourceRecord *r2);
	
extern mDNSu16 GetRDLength(const ResourceRecord *const rr, mDNSBool estimate);

#define GetRRDomainNameTarget(RR) (                                                                          \
	((RR)->rrtype == kDNSType_CNAME || (RR)->rrtype == kDNSType_PTR || (RR)->rrtype == kDNSType_NS)          \
	                                                                 ? &(RR)->rdata->u.name       :          \
	((RR)->rrtype == kDNSType_SRV                                  ) ? &(RR)->rdata->u.srv.target : mDNSNULL )

extern mDNSBool ValidateRData(const mDNSu16 rrtype, const mDNSu16 rdlength, const RData *const rd);
#define LocalRecordReady(X) ((X)->resrec.RecordType != kDNSRecordTypeUnique && (X)->resrec.RecordType != kDNSRecordTypeDeregistering)


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark -
#pragma mark - DNS Message Creation Functions
#endif

extern void InitializeDNSMessage(DNSMessageHeader *h, mDNSOpaque16 id, mDNSOpaque16 flags);
extern const mDNSu8 *FindCompressionPointer(const mDNSu8 *const base, const mDNSu8 *const end, const mDNSu8 *const domname);

extern mDNSu8 *putDomainNameAsLabels(const DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, const domainname *const name);
	
extern mDNSu8 *putRData(const DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, ResourceRecord *rr);

// If we have a single large record to put in the packet, then we allow the packet to be up to 9K bytes,
// but in the normal case we try to keep the packets below 1500 to avoid IP fragmentation on standard Ethernet	

extern mDNSu8 *PutResourceRecordTTLWithLimit(DNSMessage *const msg, mDNSu8 *ptr, mDNSu16 *count, ResourceRecord *rr, mDNSu32 ttl, const mDNSu8 *limit);

#define PutResourceRecordTTL(msg, ptr, count, rr, ttl) PutResourceRecordTTLWithLimit((msg), (ptr), (count), (rr), (ttl), \
	((msg)->h.numAnswers || (msg)->h.numAuthorities || (msg)->h.numAdditionals) ? (msg)->data + NormalMaxDNSMessageData : (msg)->data + AbsoluteMaxDNSMessageData)

#define PutResourceRecordTTLJumbo(msg, ptr, count, rr, ttl) PutResourceRecordTTLWithLimit((msg), (ptr), (count), (rr), (ttl), \
	(msg)->data + AbsoluteMaxDNSMessageData)
			
extern mDNSu8 *PutResourceRecordCappedTTL(DNSMessage *const msg, mDNSu8 *ptr, mDNSu16 *count, ResourceRecord *rr, mDNSu32 maxttl);

extern mDNSu8 *putEmptyResourceRecord(DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, mDNSu16 *count, const AuthRecord *rr);

extern mDNSu8 *putQuestion(DNSMessage *const msg, mDNSu8 *ptr, const mDNSu8 *const limit, const domainname *const name, mDNSu16 rrtype, mDNSu16 rrclass);

extern mDNSu8 *putZone(DNSMessage *const msg, mDNSu8 *ptr, mDNSu8 *limit, const domainname *zone, mDNSOpaque16 zoneClass);

extern mDNSu8 *putPrereqNameNotInUse(domainname *name, DNSMessage *msg, mDNSu8 *ptr, mDNSu8 *end);

extern mDNSu8 *putDeletionRecord(DNSMessage *msg, mDNSu8 *ptr, ResourceRecord *rr);

extern  mDNSu8 *putDeleteRRSet(DNSMessage *msg, mDNSu8 *ptr, const domainname *name, mDNSu16 rrtype);

extern mDNSu8 *putDeleteAllRRSets(DNSMessage *msg, mDNSu8 *ptr, const domainname *name);
	
extern mDNSu8 *putUpdateLease(DNSMessage *msg, mDNSu8 *end, mDNSu32 lease);
	
#define PutResourceRecord(MSG, P, C, RR) PutResourceRecordTTL((MSG), (P), (C), (RR), (RR)->rroriginalttl)


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - DNS Message Parsing Functions
#endif

extern mDNSu32 DomainNameHashValue(const domainname *const name);

extern void SetNewRData(ResourceRecord *const rr, RData *NewRData, mDNSu16 rdlength);


extern const mDNSu8 *skipDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end);

extern const mDNSu8 *getDomainName(const DNSMessage *const msg, const mDNSu8 *ptr, const mDNSu8 *const end,
	domainname *const name);
	
extern const mDNSu8 *skipResourceRecord(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end);

extern const mDNSu8 *GetLargeResourceRecord(mDNS *const m, const DNSMessage * const msg, const mDNSu8 *ptr, 
    const mDNSu8 * end, const mDNSInterfaceID InterfaceID, mDNSu8 RecordType, LargeCacheRecord *largecr);

extern const mDNSu8 *skipQuestion(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end);

extern const mDNSu8 *getQuestion(const DNSMessage *msg, const mDNSu8 *ptr, const mDNSu8 *end, const mDNSInterfaceID InterfaceID,
	DNSQuestion *question);
	
extern const mDNSu8 *LocateAnswers(const DNSMessage *const msg, const mDNSu8 *const end);

extern const mDNSu8 *LocateAuthorities(const DNSMessage *const msg, const mDNSu8 *const end);

extern const mDNSu8 *LocateAdditionals(const DNSMessage *const msg, const mDNSu8 *const end);


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark -
#pragma mark - Packet Sending Functions
#endif

extern mStatus mDNSSendDNSMessage(const mDNS *const m, DNSMessage *const msg, mDNSu8 *end,
	mDNSInterfaceID InterfaceID, const mDNSAddr *dst, mDNSIPPort dstport, int sd, uDNS_AuthInfo *authInfo);


// ***************************************************************************
#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark -
#pragma mark - RR List Management & Task Management
#endif

extern void mDNS_Lock(mDNS *const m);
extern void mDNS_Unlock(mDNS *const m);

#ifdef	__cplusplus
	}
#endif

#endif // __DNSCOMMON_H_
