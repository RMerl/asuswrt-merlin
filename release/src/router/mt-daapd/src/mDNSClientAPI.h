/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@

    Change History (most recent first):

$Log: mDNSClientAPI.h,v $
Revision 1.1  2009-06-30 02:31:08  steven
iTune Server

Revision 1.4.2.1  2005/09/08 04:15:53  rpedde
fixes for iTunes 5

Revision 1.5  2005/02/21 08:10:34  rpedde
integrate server-side conversion patches, -Wall cleanups, AMD64 fixes, and xml-rpc cleanups

Revision 1.4  2005/01/10 01:07:01  rpedde
Synchronize mDNS to Apples 58.8 drop

Revision 1.114.2.9  2004/04/22 03:17:35  cheshire
Fix use of "struct __attribute__((__packed__))" so it only applies on GCC >= 2.9

Revision 1.114.2.8  2004/03/30 06:55:37  cheshire
Gave name to anonymous struct, to avoid errors on certain compilers.
(Thanks to ramaprasad.kr@hp.com for reporting this.)

Revision 1.114.2.7  2004/03/09 02:31:27  cheshire
Remove erroneous underscore in 'packed_struct' (makes no difference now, but might in future)

Revision 1.114.2.6  2004/03/02 02:55:25  cheshire
<rdar://problem/3549576> Properly support "_services._dns-sd._udp" meta-queries

Revision 1.114.2.5  2004/02/18 23:35:17  cheshire
<rdar://problem/3488559>: Hard code domain enumeration functions to return ".local" only
Also make mDNS_StopGetDomains() a no-op too, so that we don't get warning messages in syslog

Revision 1.114.2.4  2004/01/28 23:29:20  cheshire
Fix structure packing (only affects third-party Darwin developers)

Revision 1.114.2.3  2003/12/05 00:03:34  cheshire
<rdar://problem/3487869> Use buffer size MAX_ESCAPED_DOMAIN_NAME instead of 256

Revision 1.114.2.2  2003/12/04 23:30:00  cheshire
Add "#define MAX_ESCAPED_DOMAIN_NAME 1005", needed for Posix folder to build

Revision 1.114.2.1  2003/12/03 11:07:58  cheshire
<rdar://problem/3457718>: Stop and start of a service uses old ip address (with old port number)

Revision 1.114  2003/08/29 19:44:15  cheshire
<rdar://problem/3400967> Traffic reduction: Eliminate synchronized QUs when a new service appears
1. Use m->RandomQueryDelay to impose a random delay in the range 0-500ms on queries
   that already have at least one unique answer in the cache
2. For these queries, go straight to QM, skipping QU

Revision 1.113  2003/08/21 19:31:58  cheshire
Cosmetic: Swap order of fields

Revision 1.112  2003/08/21 19:27:36  cheshire
<rdar://problem/3387878> Traffic reduction: No need to announce record for longer than TTL

Revision 1.111  2003/08/21 02:21:50  cheshire
<rdar://problem/3386473> Efficiency: Reduce repeated queries

Revision 1.110  2003/08/20 23:39:31  cheshire
<rdar://problem/3344098> Review syslog messages, and remove as appropriate

Revision 1.109  2003/08/19 22:24:10  cheshire
Comment change

Revision 1.108  2003/08/19 22:20:00  cheshire
<rdar://problem/3376721> Don't use IPv6 on interfaces that have a routable IPv4 address configured
More minor refinements

Revision 1.107  2003/08/19 06:48:25  cheshire
<rdar://problem/3376552> Guard against excessive record updates
Each record starts with 10 UpdateCredits.
Every update consumes one UpdateCredit.
UpdateCredits are replenished at a rate of one one per minute, up to a maximum of 10.
As the number of UpdateCredits declines, the number of announcements is similarly scaled back.
When fewer than 5 UpdateCredits remain, the first announcement is also delayed by an increasing amount.

Revision 1.106  2003/08/19 04:49:28  cheshire
<rdar://problem/3368159> Interaction between v4, v6 and dual-stack hosts not working quite right
1. A dual-stack host should only suppress its own query if it sees the same query from other hosts on BOTH IPv4 and IPv6.
2. When we see the first v4 (or first v6) member of a group, we re-trigger questions and probes on that interface.
3. When we see the last v4 (or v6) member of a group go away, we revalidate all the records received on that interface.

Revision 1.105  2003/08/19 02:33:37  cheshire
Update comments

Revision 1.104  2003/08/19 02:31:11  cheshire
<rdar://problem/3378386> mDNSResponder overenthusiastic with final expiration queries
Final expiration queries now only mark the question for sending on the particular interface
pertaining to the record that's expiring.

Revision 1.103  2003/08/18 19:05:44  cheshire
<rdar://problem/3382423> UpdateRecord not working right
Added "newrdlength" field to hold new length of updated rdata

Revision 1.102  2003/08/16 03:39:00  cheshire
<rdar://problem/3338440> InterfaceID -1 indicates "local only"

Revision 1.101  2003/08/15 20:16:02  cheshire
<rdar://problem/3366590> mDNSResponder takes too much RPRVT
We want to avoid touching the rdata pages, so we don't page them in.
1. RDLength was stored with the rdata, which meant touching the page just to find the length.
   Moved this from the RData to the ResourceRecord object.
2. To avoid unnecessarily touching the rdata just to compare it,
   compute a hash of the rdata and store the hash in the ResourceRecord object.

Revision 1.100  2003/08/14 19:29:04  cheshire
<rdar://problem/3378473> Include cache records in SIGINFO output
Moved declarations of DNSTypeName() and GetRRDisplayString to mDNSClientAPI.h so daemon.c can use them

Revision 1.99  2003/08/14 02:17:05  cheshire
<rdar://problem/3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.98  2003/08/12 19:56:23  cheshire
Update to APSL 2.0

Revision 1.97  2003/08/12 14:59:27  cheshire
<rdar://problem/3374490> Rate-limiting blocks some legitimate responses
When setting LastMCTime also record LastMCInterface. When checking LastMCTime to determine
whether to suppress the response, also check LastMCInterface to see if it matches.

Revision 1.96  2003/08/12 13:57:04  cheshire
<rdar://problem/3323817> Improve cache performance
Changed the number of hash table slots from 37 to 499

Revision 1.95  2003/08/09 00:55:02  cheshire
<rdar://problem/3366553> mDNSResponder is taking 20-30% of the CPU
Don't scan the whole cache after every packet.

Revision 1.94  2003/08/09 00:35:29  cheshire

Revision 1.93  2003/08/08 18:55:48  cheshire
<rdar://problem/3370365> Guard against time going backwards

Revision 1.92  2003/08/08 18:36:04  cheshire
<rdar://problem/3344154> Only need to revalidate on interface removal on platforms that have the PhantomInterfaces bug

Revision 1.91  2003/08/06 21:33:39  cheshire
Fix compiler warnings on PocketPC 2003 (Windows CE)

Revision 1.90  2003/08/06 20:30:17  cheshire
Add structure definition for rdataMX (not currently used, but good to have it for completeness)

Revision 1.89  2003/08/06 18:58:19  cheshire
Update comments

Revision 1.88  2003/07/24 23:45:44  cheshire
To eliminate compiler warnings, changed definition of mDNSBool from
"unsigned char" to "int", since "int" is in fact truly the type that C uses
for the result of comparison operators (a<b) and logical operators (a||b)

Revision 1.87  2003/07/22 23:57:20  cheshire
Move platform-layer function prototypes from mDNSClientAPI.h to mDNSPlatformFunctions.h where they belong

Revision 1.86  2003/07/20 03:52:02  ksekar
Bug #: <rdar://problem/3320722>: Feature: New Rendezvous APIs (#7875) (mDNSResponder component)
Added error type for incompatibility between daemon and client versions

Revision 1.85  2003/07/19 03:23:13  cheshire
<rdar://problem/2986147> mDNSResponder needs to receive and cache larger records

Revision 1.84  2003/07/18 23:52:12  cheshire
To improve consistency of field naming, global search-and-replace:
NextProbeTime    -> NextScheduledProbe
NextResponseTime -> NextScheduledResponse

Revision 1.83  2003/07/18 00:29:59  cheshire
<rdar://problem/3268878> Remove mDNSResponder version from packet header and use HINFO record instead

Revision 1.82  2003/07/17 17:35:04  cheshire
<rdar://problem/3325583> Rate-limit responses, to guard against packet flooding

Revision 1.81  2003/07/16 05:01:36  cheshire
Add fields 'LargeAnswers' and 'ExpectUnicastResponse' in preparation for
<rdar://problem/3315761> Need to implement "unicast response" request, using top bit of qclass

Revision 1.80  2003/07/15 01:55:12  cheshire
<rdar://problem/3315777> Need to implement service registration with subtypes

Revision 1.79  2003/07/13 02:28:00  cheshire
<rdar://problem/3325166> SendResponses didn't all its responses
Delete all references to RRInterfaceActive -- it's now superfluous

Revision 1.78  2003/07/13 01:47:53  cheshire
Fix one error and one warning in the Windows build

Revision 1.77  2003/07/11 01:32:38  cheshire
Syntactic cleanup (no change to funcationality): Now that we only have one host name,
rename field "hostname1" to "hostname", and field "RR_A1" to "RR_A".

Revision 1.76  2003/07/11 01:28:00  cheshire
<rdar://problem/3161289> No more local.arpa

Revision 1.75  2003/07/02 21:19:45  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.74  2003/07/02 02:41:23  cheshire
<rdar://problem/2986146> mDNSResponder needs to start with a smaller cache and then grow it as needed

Revision 1.73  2003/06/10 04:24:39  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache
Some additional refinements:
Don't try to do this for unicast-response queries
better tracking of Qs and KAs in multi-packet KA lists

Revision 1.72  2003/06/10 01:46:27  cheshire
Add better comments explaining how these data structures are intended to be used from the client layer

Revision 1.71  2003/06/07 06:45:05  cheshire
<rdar://problem/3283666> No need for multiple machines to all be sending the same queries

Revision 1.70  2003/06/07 04:50:53  cheshire
<rdar://problem/3283637> React when we observe other people query unsuccessfully for a record that's in our cache

Revision 1.69  2003/06/07 04:22:17  cheshire
Add MsgBuffer for error log and debug messages

Revision 1.68  2003/06/07 01:46:38  cheshire
<rdar://problem/3283540> When query produces zero results, call mDNS_Reconfirm() on any antecedent records

Revision 1.67  2003/06/07 01:22:14  cheshire
<rdar://problem/3283516> mDNSResponder needs an mDNS_Reconfirm() function

Revision 1.66  2003/06/07 00:59:43  cheshire
<rdar://problem/3283454> Need some randomness to spread queries on the network

Revision 1.65  2003/06/06 21:41:11  cheshire
For consistency, mDNS_StopQuery() should return an mStatus result, just like all the other mDNSCore routines

Revision 1.64  2003/06/06 21:38:55  cheshire
Renamed 'NewData' as 'FreshData' (The data may not be new data, just a refresh of data that we
already had in our cache. This refreshes our TTL on the data, but the data itself stays the same.)

Revision 1.63  2003/06/06 17:20:14  cheshire
For clarity, rename question fields name/rrtype/rrclass as qname/qtype/qclass
(Global search-and-replace; no functional change to code execution.)

Revision 1.62  2003/06/04 01:25:33  cheshire
<rdar://problem/3274950> Cannot perform multi-packet known-answer suppression messages
Display time interval between first and subsequent queries

Revision 1.61  2003/06/03 05:02:16  cheshire
<rdar://problem/3277080> Duplicate registrations not handled as efficiently as they should be

Revision 1.60  2003/05/31 00:09:49  cheshire
<rdar://problem/3274862> Add ability to discover what services are on a network

Revision 1.59  2003/05/29 06:11:35  cheshire
<rdar://problem/3272214>:	Report if there appear to be too many "Resolve" callbacks

Revision 1.58  2003/05/29 05:48:06  cheshire
Minor fix for when generating printf warnings: mDNS_snprintf arguments are now 3,4

Revision 1.57  2003/05/26 03:21:27  cheshire
Tidy up address structure naming:
mDNSIPAddr         => mDNSv4Addr (for consistency with mDNSv6Addr)
mDNSAddr.addr.ipv4 => mDNSAddr.ip.v4
mDNSAddr.addr.ipv6 => mDNSAddr.ip.v6

Revision 1.56  2003/05/26 03:01:27  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.55  2003/05/26 00:47:30  cheshire
Comment clarification

Revision 1.54  2003/05/24 16:39:48  cheshire
<rdar://problem/3268631> SendResponses also needs to handle multihoming better

Revision 1.53  2003/05/23 02:15:37  cheshire
Fixed misleading use of the term "duplicate suppression" where it should have
said "known answer suppression". (Duplicate answer suppression is something
different, and duplicate question suppression is yet another thing, so the use
of the completely vague term "duplicate suppression" was particularly bad.)

Revision 1.52  2003/05/22 02:29:22  cheshire
<rdar://problem/2984918> SendQueries needs to handle multihoming better
Complete rewrite of SendQueries. Works much better now :-)

Revision 1.51  2003/05/21 20:14:55  cheshire
Fix comments and warnings

Revision 1.50  2003/05/14 07:08:36  cheshire
<rdar://problem/3159272> mDNSResponder should be smarter about reconfigurations
Previously, when there was any network configuration change, mDNSResponder
would tear down the entire list of active interfaces and start again.
That was very disruptive, and caused the entire cache to be flushed,
and caused lots of extra network traffic. Now it only removes interfaces
that have really gone, and only adds new ones that weren't there before.

Revision 1.49  2003/05/07 01:49:36  cheshire
Remove "const" in ConstructServiceName prototype

Revision 1.48  2003/05/07 00:18:44  cheshire
Fix typo: "kDNSQClass_Mask" should be "kDNSClass_Mask"

Revision 1.47  2003/05/06 00:00:46  cheshire
<rdar://problem/3248914> Rationalize naming of domainname manipulation functions

Revision 1.46  2003/04/30 20:39:09  cheshire
Add comment

Revision 1.45  2003/04/29 00:40:50  cheshire
Fix compiler warnings

Revision 1.44  2003/04/26 02:41:56  cheshire
<rdar://problem/3241281> Change timenow from a local variable to a structure member

Revision 1.43  2003/04/25 01:45:56  cheshire
<rdar://problem/3240002> mDNS_RegisterNoSuchService needs to include a host name

Revision 1.42  2003/04/15 20:58:31  jgraessl

Bug #: 3229014
Added a hash to lookup records in the cache.

Revision 1.41  2003/04/15 18:09:13  jgraessl

Bug #: 3228892
Reviewed by: Stuart Cheshire
Added code to keep track of when the next cache item will expire so we can
call TidyRRCache only when necessary.

Revision 1.40  2003/03/29 01:55:19  cheshire
<rdar://problem/3212360> mDNSResponder sometimes suffers false self-conflicts when it sees its own packets
Solution: Major cleanup of packet timing and conflict handling rules

Revision 1.39  2003/03/27 03:30:55  cheshire
<rdar://problem/3210018> Name conflicts not handled properly, resulting in memory corruption, and eventual crash
Problem was that HostNameCallback() was calling mDNS_DeregisterInterface(), which is not safe in a callback
Fixes:
1. Make mDNS_DeregisterInterface() safe to call from a callback
2. Make HostNameCallback() use mDNS_DeadvertiseInterface() instead
   (it never really needed to deregister the interface at all)

Revision 1.38  2003/03/15 04:40:36  cheshire
Change type called "mDNSOpaqueID" to the more descriptive name "mDNSInterfaceID"

Revision 1.37  2003/03/14 21:34:11  cheshire
<rdar://problem/3176950> Can't setup and print to Lexmark PS printers via Airport Extreme
Increase size of cache rdata from 512 to 768

Revision 1.36  2003/03/05 03:38:35  cheshire
Bug #: 3185731 Bogus error message in console: died or deallocated, but no record of client can be found!
Fixed by leaving client in list after conflict, until client explicitly deallocates

Revision 1.35  2003/02/21 02:47:54  cheshire
Bug #: 3099194 mDNSResponder needs performance improvements
Several places in the code were calling CacheRRActive(), which searched the entire
question list every time, to see if this cache resource record answers any question.
Instead, we now have a field "CRActiveQuestion" in the resource record structure

Revision 1.34  2003/02/21 01:54:08  cheshire
Bug #: 3099194 mDNSResponder needs performance improvements
Switched to using new "mDNS_Execute" model (see "Implementer Notes.txt")

Revision 1.33  2003/02/20 06:48:32  cheshire
Bug #: 3169535 Xserve RAID needs to do interface-specific registrations
Reviewed by: Josh Graessley, Bob Bradley

Revision 1.32  2003/01/31 03:35:59  cheshire
Bug #: 3147097 mDNSResponder sometimes fails to find the correct results
When there were *two* active questions in the list, they were incorrectly
finding *each other* and *both* being marked as duplicates of another question

Revision 1.31  2003/01/29 02:46:37  cheshire
Fix for IPv6:
A physical interface is identified solely by its InterfaceID (not by IP and type).
On a given InterfaceID, mDNSCore may send both v4 and v6 multicasts.
In cases where the requested outbound protocol (v4 or v6) is not supported on
that InterfaceID, the platform support layer should simply discard that packet.

Revision 1.30  2003/01/29 01:47:08  cheshire
Rename 'Active' to 'CRActive' or 'InterfaceActive' for improved clarity

Revision 1.29  2003/01/28 05:23:43  cheshire
Bug #: 3147097 mDNSResponder sometimes fails to find the correct results
Add 'Active' flag for interfaces

Revision 1.28  2003/01/28 01:35:56  cheshire
Revise comment about ThisQInterval to reflect new semantics

Revision 1.27  2003/01/13 23:49:42  jgraessl
Merged changes for the following fixes in to top of tree:
3086540  computer name changes not handled properly
3124348  service name changes are not properly handled
3124352  announcements sent in pairs, failing chattiness test

Revision 1.26  2002/12/23 22:13:28  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.25  2002/09/21 20:44:49  zarzycki
Added APSL info

Revision 1.24  2002/09/19 23:47:35  cheshire
Added mDNS_RegisterNoSuchService() function for assertion of non-existance
of a particular named service

Revision 1.23  2002/09/19 21:25:34  cheshire
mDNS_snprintf() doesn't need to be in a separate file

Revision 1.22  2002/09/19 04:20:43  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.21  2002/09/17 01:06:35  cheshire
Change mDNS_AdvertiseLocalAddresses to be a parameter to mDNS_Init()

Revision 1.20  2002/09/16 18:41:41  cheshire
Merge in license terms from Quinn's copy, in preparation for Darwin release

*/

#ifndef __mDNSClientAPI_h
#define __mDNSClientAPI_h

#include <stdarg.h>		// stdarg.h is required for for va_list support for the mDNS_vsnprintf declaration
#include "mDNSDebug.h"

#ifdef	__cplusplus
	extern "C" {
#endif

// ***************************************************************************
// Function scope indicators

// If you see "mDNSlocal" before a function name in a C file, it means the function is not callable outside this file
#ifndef mDNSlocal
#define mDNSlocal static
#endif
// If you see "mDNSexport" before a symbol in a C file, it means the symbol is exported for use by clients
// For every "mDNSexport" in a C file, there needs to be a corresponding "extern" declaration in some header file
// (When a C file #includes a header file, the "extern" declarations tell the compiler:
// "This symbol exists -- but not necessarily in this C file.")
#ifndef mDNSexport
#define mDNSexport
#endif

// ***************************************************************************
// Structure packing macro

// If we're not using GNUC, it's not fatal.
// Most compilers naturally pack the on-the-wire structures correctly anyway, so a plain "struct" is usually fine.
// In the event that structures are not packed correctly, mDNS_Init() will detect this and report an error, so the
// developer will know what's wrong, and can investigate what needs to be done on that compiler to provide proper packing.
#ifndef packedstruct
 #if ((__GNUC__ > 2) || ((__GNUC__ == 2) && (__GNUC_MINOR__ >= 9)))
  #define packedstruct struct __attribute__((__packed__))
  #define packedunion  union  __attribute__((__packed__))
 #else
  #define packedstruct struct
  #define packedunion  union
 #endif
#endif

// ***************************************************************************
#if 0
#pragma mark - DNS Resource Record class and type constants
#endif

typedef enum							// From RFC 1035
	{
	kDNSClass_IN               = 1,		// Internet
	kDNSClass_CS               = 2,		// CSNET
	kDNSClass_CH               = 3,		// CHAOS
	kDNSClass_HS               = 4,		// Hesiod
	kDNSClass_NONE             = 254,	// Used in DNS UPDATE [RFC 2136]

	kDNSClass_Mask             = 0x7FFF,// Multicast DNS uses the bottom 15 bits to identify the record class...
	kDNSClass_UniqueRRSet      = 0x8000,// ... and the top bit indicates that all other cached records are now invalid

	kDNSQClass_ANY             = 255,	// Not a DNS class, but a DNS query class, meaning "all classes"
	kDNSQClass_UnicastResponse = 0x8000	// Top bit set in a question means "unicast response acceptable"
	} DNS_ClassValues;

typedef enum				// From RFC 1035
	{
	kDNSType_A = 1,			//  1 Address
	kDNSType_NS,			//  2 Name Server
	kDNSType_MD,			//  3 Mail Destination
	kDNSType_MF,			//  4 Mail Forwarder
	kDNSType_CNAME,			//  5 Canonical Name
	kDNSType_SOA,			//  6 Start of Authority
	kDNSType_MB,			//  7 Mailbox
	kDNSType_MG,			//  8 Mail Group
	kDNSType_MR,			//  9 Mail Rename
	kDNSType_NULL,			// 10 NULL RR
	kDNSType_WKS,			// 11 Well-known-service
	kDNSType_PTR,			// 12 Domain name pointer
	kDNSType_HINFO,			// 13 Host information
	kDNSType_MINFO,			// 14 Mailbox information
	kDNSType_MX,			// 15 Mail Exchanger
	kDNSType_TXT,			// 16 Arbitrary text string

	kDNSType_AAAA = 28,		// 28 IPv6 address
	kDNSType_SRV = 33,		// 33 Service record

	kDNSQType_ANY = 255		// Not a DNS type, but a DNS query type, meaning "all types"
	} DNS_TypeValues;

// ***************************************************************************
#if 0
#pragma mark - Simple types
#endif

// mDNS defines its own names for these common types to simplify portability across
// multiple platforms that may each have their own (different) names for these types.
typedef          int   mDNSBool;
typedef   signed char  mDNSs8;
typedef unsigned char  mDNSu8;
typedef   signed short mDNSs16;
typedef unsigned short mDNSu16;
#include <sys/types.h>
typedef int32_t    mDNSs32;
typedef u_int32_t  mDNSu32;

// To enforce useful type checking, we make mDNSInterfaceID be a pointer to a dummy struct
// This way, mDNSInterfaceIDs can be assigned, and compared with each other, but not with other types
// Declaring the type to be the typical generic "void *" would lack this type checking
typedef struct mDNSInterfaceID_dummystruct { void *dummy; } *mDNSInterfaceID;

// These types are for opaque two- and four-byte identifiers.
// The "NotAnInteger" fields of the unions allow the value to be conveniently passed around in a
// register for the sake of efficiency, and compared for equality or inequality, but don't forget --
// just because it is in a register doesn't mean it is an integer. Operations like greater than,
// less than, add, multiply, increment, decrement, etc., are undefined for opaque identifiers,
// and if you make the mistake of trying to do those using the NotAnInteger field, then you'll
// find you get code that doesn't work consistently on big-endian and little-endian machines.
typedef packedunion { mDNSu8 b[2]; mDNSu16 NotAnInteger; } mDNSOpaque16;
typedef packedunion { mDNSu8 b[4]; mDNSu32 NotAnInteger; } mDNSOpaque32;
typedef packedunion { mDNSu8 b[16]; mDNSu16 w[8]; mDNSu32 l[4]; } mDNSOpaque128;

typedef mDNSOpaque16  mDNSIPPort;		// An IP port is a two-byte opaque identifier (not an integer)
typedef mDNSOpaque32  mDNSv4Addr;		// An IP address is a four-byte opaque identifier (not an integer)
typedef mDNSOpaque128 mDNSv6Addr;		// An IPv6 address is a 16-byte opaque identifier (not an integer)

enum
	{
	mDNSAddrType_None    = 0,
	mDNSAddrType_IPv4    = 4,
	mDNSAddrType_IPv6    = 6,
	mDNSAddrType_Unknown = ~0	// Special marker value used in known answer list recording
	};

typedef struct
	{
	mDNSs32 type;
	union { mDNSv6Addr v6; mDNSv4Addr v4; } ip;
	} mDNSAddr;

enum { mDNSfalse = 0, mDNStrue = 1 };

#define mDNSNULL 0L

enum
	{
	mStatus_Waiting           = 1,
	mStatus_NoError           = 0,

	// mDNS return values are in the range FFFE FF00 (-65792) to FFFE FFFF (-65537)
	// The top end of the range (FFFE FFFF) is used for error codes;
	// the bottom end of the range (FFFE FF00) is used for non-error values;

	// Error codes:
	mStatus_UnknownErr        = -65537,		// 0xFFFE FFFF
	mStatus_NoSuchNameErr     = -65538,
	mStatus_NoMemoryErr       = -65539,
	mStatus_BadParamErr       = -65540,
	mStatus_BadReferenceErr   = -65541,
	mStatus_BadStateErr       = -65542,
	mStatus_BadFlagsErr       = -65543,
	mStatus_UnsupportedErr    = -65544,
	mStatus_NotInitializedErr = -65545,
	mStatus_NoCache           = -65546,
	mStatus_AlreadyRegistered = -65547,
	mStatus_NameConflict      = -65548,
	mStatus_Invalid           = -65549,
	//                        = -65550,
	mStatus_Incompatible      = -65551,
	mStatus_BadInterfaceErr   = -65552,

	// -65553 - -65789 currently unused

	// Non-error values:
	mStatus_GrowCache         = -65790,
	mStatus_ConfigChanged     = -65791,
	mStatus_MemFree           = -65792		// 0xFFFE FF00
	};

typedef mDNSs32 mStatus;

// RFC 1034/1035 specify that a domain label consists of a length byte plus up to 63 characters
#define MAX_DOMAIN_LABEL 63
typedef struct { mDNSu8 c[ 64]; } domainlabel;		// One label: length byte and up to 63 characters

// RFC 1034/1035 specify that a domain name, including length bytes, data bytes, and terminating zero, may be up to 255 bytes long
#define MAX_DOMAIN_NAME 255
typedef struct { mDNSu8 c[256]; } domainname;		// Up to 255 bytes of length-prefixed domainlabels

typedef struct { mDNSu8 c[256]; } UTF8str255;		// Null-terminated C string

// The longest legal textual form of a DNS name is 1005 bytes, including the C-string terminating NULL at the end.
// Explanation:
// When a native domainname object is converted to printable textual form using ConvertDomainNameToCString(),
// non-printing characters are represented in the conventional DNS way, as '\ddd', where ddd is a three-digit decimal number.
// The longest legal domain name is 255 bytes, in the form of four labels as shown below:
// Length byte, 63 data bytes, length byte, 63 data bytes, length byte, 63 data bytes, length byte, 61 data bytes, zero byte.
// Each label is encoded textually as characters followed by a trailing dot.
// If every character has to be represented as a four-byte escape sequence, then this makes the maximum textual form four labels
// plus the C-string terminating NULL as shown below:
// 63*4+1 + 63*4+1 + 63*4+1 + 61*4+1 + 1 = 1005.
// Note that MAX_ESCAPED_DOMAIN_LABEL is not normally used: If you're only decoding a single label, escaping is usually not required.
// It is for domain names, where dots are used as label separators, that proper escaping is vital.
#define MAX_ESCAPED_DOMAIN_LABEL 254
#define MAX_ESCAPED_DOMAIN_NAME 1005

// ***************************************************************************
#if 0
#pragma mark - Resource Record structures
#endif

// Authoritative Resource Records:
// There are four basic types: Shared, Advisory, Unique, Known Unique

// * Shared Resource Records do not have to be unique
// -- Shared Resource Records are used for DNS-SD service PTRs
// -- It is okay for several hosts to have RRs with the same name but different RDATA
// -- We use a random delay on responses to reduce collisions when all the hosts respond to the same query
// -- These RRs typically have moderately high TTLs (e.g. one hour)
// -- These records are announced on startup and topology changes for the benefit of passive listeners
// -- These records send a goodbye packet when deregistering
//
// * Advisory Resource Records are like Shared Resource Records, except they don't send a goodbye packet
//
// * Unique Resource Records should be unique among hosts within any given mDNS scope
// -- The majority of Resource Records are of this type
// -- If two entities on the network have RRs with the same name but different RDATA, this is a conflict
// -- Responses may be sent immediately, because only one host should be responding to any particular query
// -- These RRs typically have low TTLs (e.g. ten seconds)
// -- On startup and after topology changes, a host issues queries to verify uniqueness

// * Known Unique Resource Records are treated like Unique Resource Records, except that mDNS does
// not have to verify their uniqueness because this is already known by other means (e.g. the RR name
// is derived from the host's IP or Ethernet address, which is already known to be a unique identifier).

// Summary of properties of different record types:
// Probe?    Does this record type send probes before announcing?
// Conflict? Does this record type react if we observe an apparent conflict?
// Goodbye?  Does this record type send a goodbye packet on departure?
//
//               Probe? Conflict? Goodbye? Notes
// Unregistered                            Should not appear in any list (sanity check value)
// Shared         No      No       Yes     e.g. Service PTR record
// Deregistering  No      No       Yes     Shared record about to announce its departure and leave the list
// Advisory       No      No       No
// Unique         Yes     Yes      No      Record intended to be unique -- will probe to verify
// Verified       Yes     Yes      No      Record has completed probing, and is verified unique
// KnownUnique    No      Yes      No      Record is assumed by other means to be unique

// Valid lifecycle of a record:
// Unregistered ->                   Shared      -> Deregistering -(goodbye)-> Unregistered
// Unregistered ->                   Advisory                               -> Unregistered
// Unregistered -> Unique -(probe)-> Verified                               -> Unregistered
// Unregistered ->                   KnownUnique                            -> Unregistered

// Each Authoritative kDNSRecordType has only one bit set. This makes it easy to quickly see if a record
// is one of a particular set of types simply by performing the appropriate bitwise masking operation.

// Cache Resource Records (received from the network):
// There are four basic types: Answer, Unique Answer, Additional, Unique Additional
// Bit 7 (the top bit) of kDNSRecordType is always set for Cache Resource Records; always clear for Authoritative Resource Records
// Bit 6 (value 0x40) is set for answer records; clear for additional records
// Bit 5 (value 0x20) is set for records received with the kDNSClass_UniqueRRSet

enum
	{
	kDNSRecordTypeUnregistered     = 0x00,	// Not currently in any list
	kDNSRecordTypeDeregistering    = 0x01,	// Shared record about to announce its departure and leave the list

	kDNSRecordTypeUnique           = 0x02,	// Will become a kDNSRecordTypeVerified when probing is complete

	kDNSRecordTypeAdvisory         = 0x04,	// Like Shared, but no goodbye packet
	kDNSRecordTypeShared           = 0x08,	// Shared means record name does not have to be unique -- use random delay on responses
	kDNSRecordTypeVerified         = 0x10,	// Unique means mDNS should check that name is unique (and then send immediate responses)
	kDNSRecordTypeKnownUnique      = 0x20,	// Known Unique means mDNS can assume name is unique without checking

	kDNSRecordTypeUniqueMask       = (kDNSRecordTypeUnique | kDNSRecordTypeVerified | kDNSRecordTypeKnownUnique),
	kDNSRecordTypeActiveMask       = (kDNSRecordTypeAdvisory | kDNSRecordTypeShared | kDNSRecordTypeVerified | kDNSRecordTypeKnownUnique),

	kDNSRecordTypePacketAdd        = 0x80,	// Received in the Additional Section of a DNS Response
	kDNSRecordTypePacketAddUnique  = 0xA0,	// Received in the Additional Section of a DNS Response with kDNSClass_UniqueRRSet set
	kDNSRecordTypePacketAns        = 0xC0,	// Received in the Answer Section of a DNS Response
	kDNSRecordTypePacketAnsUnique  = 0xE0,	// Received in the Answer Section of a DNS Response with kDNSClass_UniqueRRSet set

	kDNSRecordTypePacketAnsMask    = 0x40,	// True for PacketAns       and PacketAnsUnique
	kDNSRecordTypePacketUniqueMask = 0x20	// True for PacketAddUnique and PacketAnsUnique
	};

typedef packedstruct { mDNSu16 priority; mDNSu16 weight; mDNSIPPort port; domainname target; } rdataSRV;
typedef packedstruct { mDNSu16 preference; domainname exchange; } rdataMX;

// StandardAuthRDSize is 264 (256+8), which is large enough to hold a maximum-sized SRV record
// MaximumRDSize is 8K the absolute maximum we support (at least for now)
#define StandardAuthRDSize 264
#define MaximumRDSize 8192

// InlineCacheRDSize is 64
// Records received from the network with rdata this size or less have their rdata stored right in the CacheRecord object
// Records received from the network with rdata larger than this have additional storage allocated for the rdata
// A quick unscientific sample from a busy network at Apple with lots of machines revealed this:
// 1461 records in cache
// 292 were one-byte TXT records
// 136 were four-byte A records
// 184 were sixteen-byte AAAA records
// 780 were various PTR, TXT and SRV records from 12-64 bytes
// Only 69 records had rdata bigger than 64 bytes
#define InlineCacheRDSize 64

typedef union
	{
	mDNSu8      data[StandardAuthRDSize];
	mDNSv4Addr  ip;			// For 'A' record
	mDNSv6Addr  ipv6;		// For 'AAAA' record
	domainname  name;		// For PTR and CNAME records
	UTF8str255  txt;		// For TXT record
	rdataSRV    srv;		// For SRV record
	rdataMX     mx;			// For MX record
	} RDataBody;

typedef struct
	{
	mDNSu16    MaxRDLength;	// Amount of storage allocated for rdata (usually sizeof(RDataBody))
	RDataBody  u;
	} RData;
#define sizeofRDataHeader (sizeof(RData) - sizeof(RDataBody))

typedef struct AuthRecord_struct AuthRecord;
typedef struct CacheRecord_struct CacheRecord;
typedef struct ResourceRecord_struct ResourceRecord;
typedef struct DNSQuestion_struct DNSQuestion;
typedef struct mDNS_struct mDNS;
typedef struct mDNS_PlatformSupport_struct mDNS_PlatformSupport;

// Note: Within an mDNSRecordCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute() 
typedef void mDNSRecordCallback(mDNS *const m, AuthRecord *const rr, mStatus result);

// Note:
// Restrictions: An mDNSRecordUpdateCallback may not make any mDNS API calls.
// The intent of this callback is to allow the client to free memory, if necessary.
// The internal data structures of the mDNS code may not be in a state where mDNS API calls may be made safely.
typedef void mDNSRecordUpdateCallback(mDNS *const m, AuthRecord *const rr, RData *OldRData);

struct ResourceRecord_struct
	{
	mDNSu8          RecordType;			// See enum above
	mDNSInterfaceID InterfaceID;		// Set if this RR is specific to one interface
										// For records received off the wire, InterfaceID is *always* set to the receiving interface
										// For our authoritative records, InterfaceID is usually zero, except for those few records
										// that are interface-specific (e.g. address records, especially linklocal addresses)
	domainname      name;				
	mDNSu16         rrtype;
	mDNSu16         rrclass;
	mDNSu32         rroriginalttl;		// In seconds
	mDNSu16         rdlength;			// Size of the raw rdata, in bytes
	mDNSu16         rdestimate;			// Upper bound on size of rdata after name compression
	mDNSu32         namehash;			// Name-based (i.e. case insensitive) hash of name
	mDNSu32         rdatahash;			// 32-bit hash of the raw rdata
	mDNSu32         rdnamehash;			// Set if this rdata contains a domain name (e.g. PTR, SRV, CNAME etc.)
	RData           *rdata;				// Pointer to storage for this rdata
	};

struct AuthRecord_struct
	{
	// For examples of how to set up this structure for use in mDNS_Register(),
	// see mDNS_AdvertiseInterface() or mDNS_RegisterService().
	// Basically, resrec and persistent metadata need to be set up before calling mDNS_Register().
	// mDNS_SetupResourceRecord() is avaliable as a helper routine to set up most fields to sensible default values for you

	AuthRecord     *next;				// Next in list; first element of structure for efficiency reasons
	ResourceRecord  resrec;

	// Persistent metadata for Authoritative Records
	AuthRecord     *Additional1;		// Recommended additional record to include in response
	AuthRecord     *Additional2;		// Another additional
	AuthRecord     *DependentOn;		// This record depends on another for its uniqueness checking
	AuthRecord     *RRSet;				// This unique record is part of an RRSet
	mDNSRecordCallback *RecordCallback;	// Callback function to call for state changes
	void           *RecordContext;		// Context parameter for the callback function
	mDNSu8          HostTarget;			// Set if the target of this record (PTR, CNAME, SRV, etc.) is our host name

	// Transient state for Authoritative Records
	mDNSu8          Acknowledged;		// Set if we've given the success callback to the client
	mDNSu8          ProbeCount;			// Number of probes remaining before this record is valid (kDNSRecordTypeUnique)
	mDNSu8          AnnounceCount;		// Number of announcements remaining (kDNSRecordTypeShared)
	mDNSu8          IncludeInProbe;		// Set if this RR is being put into a probe right now
	mDNSInterfaceID ImmedAnswer;		// Someone on this interface issued a query we need to answer (all-ones for all interfaces)
	mDNSInterfaceID ImmedAdditional;	// Hint that we might want to also send this record, just to be helpful
	mDNSInterfaceID SendRNow;			// The interface this query is being sent on right now
	mDNSv4Addr      v4Requester;		// Recent v4 query for this record, or all-ones if more than one recent query
	mDNSv6Addr      v6Requester;		// Recent v6 query for this record, or all-ones if more than one recent query
	AuthRecord     *NextResponse;		// Link to the next element in the chain of responses to generate
	const mDNSu8   *NR_AnswerTo;		// Set if this record was selected by virtue of being a direct answer to a question
	AuthRecord     *NR_AdditionalTo;	// Set if this record was selected by virtue of being additional to another
	mDNSs32         ThisAPInterval;		// In platform time units: Current interval for announce/probe
	mDNSs32         AnnounceUntil;		// In platform time units: Creation time + TTL
	mDNSs32         LastAPTime;			// In platform time units: Last time we sent announcement/probe
	mDNSs32         LastMCTime;			// Last time we multicast this record (used to guard against packet-storm attacks)
	mDNSInterfaceID LastMCInterface;	// Interface this record was multicast on at the time LastMCTime was recorded
	RData          *NewRData;			// Set if we are updating this record with new rdata
	mDNSu16         newrdlength;		// ... and the length of the new RData
	mDNSRecordUpdateCallback *UpdateCallback;
	mDNSu32         UpdateCredits;		// Token-bucket rate limiting of excessive updates
	mDNSs32         NextUpdateCredit;	// Time next token is added to bucket
	mDNSs32         UpdateBlocked;		// Set if update delaying is in effect

	RData           rdatastorage;		// Normally the storage is right here, except for oversized records
	// rdatastorage MUST be the last thing in the structure -- when using oversized AuthRecords, extra bytes
	// are appended after the end of the AuthRecord, logically augmenting the size of the rdatastorage
	// DO NOT ADD ANY MORE FIELDS HERE
	};

struct CacheRecord_struct
	{
	CacheRecord    *next;				// Next in list; first element of structure for efficiency reasons
	ResourceRecord  resrec;

	// Transient state for Cache Records
	CacheRecord    *NextInKAList;		// Link to the next element in the chain of known answers to send
	mDNSs32         TimeRcvd;			// In platform time units
	mDNSs32         NextRequiredQuery;	// In platform time units
	mDNSs32         LastUsed;			// In platform time units
	mDNSu32         UseCount;			// Number of times this RR has been used to answer a question
	DNSQuestion    *CRActiveQuestion;	// Points to an active question referencing this answer
	mDNSu32         UnansweredQueries;	// Number of times we've issued a query for this record without getting an answer
	mDNSs32         LastUnansweredTime;	// In platform time units; last time we incremented UnansweredQueries
	mDNSu32         MPUnansweredQ;		// Multi-packet query handling: Number of times we've seen a query for this record
	mDNSs32         MPLastUnansweredQT;	// Multi-packet query handling: Last time we incremented MPUnansweredQ
	mDNSu32         MPUnansweredKA;		// Multi-packet query handling: Number of times we've seen this record in a KA list
	mDNSBool        MPExpectingKA;		// Multi-packet query handling: Set when we increment MPUnansweredQ; allows one KA
	CacheRecord    *NextInCFList;		// Set if this is in the list of records we just received with the cache flush bit set

	struct { mDNSu16 MaxRDLength; mDNSu8 data[InlineCacheRDSize]; } rdatastorage;	// Storage for small records is right here
	};

typedef struct
	{
	CacheRecord r;
	mDNSu8 _extradata[MaximumRDSize-InlineCacheRDSize];		// Glue on the necessary number of extra bytes
	} LargeCacheRecord;

typedef struct NetworkInterfaceInfo_struct NetworkInterfaceInfo;

struct NetworkInterfaceInfo_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	NetworkInterfaceInfo *next;

	mDNSBool        InterfaceActive;	// InterfaceActive is set if interface is sending & receiving packets
										// InterfaceActive is clear if interface is here to represent an address with A
										// and/or AAAA records, but there is already an earlier representative for this
										// physical interface which will be used for the actual sending & receiving
										// packets (this status may change as interfaces are added and removed)
	mDNSBool        IPv4Available;		// If InterfaceActive, set if v4 available on this InterfaceID
	mDNSBool        IPv6Available;		// If InterfaceActive, set if v6 available on this InterfaceID

	// Standard AuthRecords that every Responder host should have (one per active IP address)
	AuthRecord RR_A;					// 'A' or 'AAAA' (address) record for our ".local" name
	AuthRecord RR_PTR;					// PTR (reverse lookup) record
	AuthRecord RR_HINFO;

	// Client API fields: The client must set up these fields *before* calling mDNS_RegisterInterface()
	mDNSInterfaceID InterfaceID;
	mDNSAddr        ip;
	mDNSBool        Advertise;			// Set Advertise to false if you are only searching on this interface
	mDNSBool        TxAndRx;			// Set to false if not sending and receiving packets on this interface
	};

typedef struct ExtraResourceRecord_struct ExtraResourceRecord;
struct ExtraResourceRecord_struct
	{
	ExtraResourceRecord *next;
	AuthRecord r;
	// Note: Add any additional fields *before* the AuthRecord in this structure, not at the end.
	// In some cases clients can allocate larger chunks of memory and set r->rdata->MaxRDLength to indicate
	// that this extra memory is available, which would result in any fields after the AuthRecord getting smashed
	};

// Note: Within an mDNSServiceCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute() 
typedef struct ServiceRecordSet_struct ServiceRecordSet;
typedef void mDNSServiceCallback(mDNS *const m, ServiceRecordSet *const sr, mStatus result);
struct ServiceRecordSet_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_RegisterService();
	// all required data is passed as parameters to that function.
	mDNSServiceCallback *ServiceCallback;
	void                *ServiceContext;
	ExtraResourceRecord *Extras;	// Optional list of extra AuthRecords attached to this service registration
	mDNSu32              NumSubTypes;
	AuthRecord          *SubTypes;
	mDNSBool             Conflict;	// Set if this record set was forcibly deregistered because of a conflict
	domainname           Host;		// Set if this service record does not use the standard target host name
	AuthRecord           RR_ADV;	// e.g. _services._dns-sd._udp.local. PTR _printer._tcp.local.
	AuthRecord           RR_PTR;	// e.g. _printer._tcp.local.        PTR Name._printer._tcp.local.
	AuthRecord           RR_SRV;	// e.g. Name._printer._tcp.local.   SRV 0 0 port target
	AuthRecord           RR_TXT;	// e.g. Name._printer._tcp.local.   TXT PrintQueueName
	// Don't add any fields after AuthRecord RR_TXT.
	// This is where the implicit extra space goes if we allocate a ServiceRecordSet containing an oversized RR_TXT record
	};

// ***************************************************************************
#if 0
#pragma mark - Question structures
#endif

// We record the last eight instances of each duplicate query
// This gives us v4/v6 on each of Ethernet/AirPort and Firewire, and two free slots "for future expansion"
// If the host has more active interfaces that this it is not fatal -- duplicate question suppression will degrade gracefully.
// Since we will still remember the last eight, the busiest interfaces will still get the effective duplicate question suppression.
#define DupSuppressInfoSize 8

typedef struct
	{
	mDNSs32               Time;
	mDNSInterfaceID       InterfaceID;
	mDNSs32               Type;				// v4 or v6?
	} DupSuppressInfo;

// Note: Within an mDNSQuestionCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute() 
typedef void mDNSQuestionCallback(mDNS *const m, DNSQuestion *question, const ResourceRecord *const answer, mDNSBool AddRecord);
struct DNSQuestion_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	DNSQuestion          *next;
	mDNSu32               qnamehash;
	mDNSs32               LastQTime;		// Last scheduled transmission of this Q on *all* applicable interfaces
	mDNSs32               ThisQInterval;	// LastQTime + ThisQInterval is the next scheduled transmission of this Q
											// ThisQInterval > 0 for an active question;
											// ThisQInterval = 0 for a suspended question that's still in the list
											// ThisQInterval = -1 for a cancelled question that's been removed from the list
	mDNSu32               RecentAnswers;	// Number of answers since the last time we sent this query
	mDNSu32               CurrentAnswers;	// Number of records currently in the cache that answer this question
	mDNSu32               LargeAnswers;		// Number of answers with rdata > 1024 bytes
	mDNSu32               UniqueAnswers;	// Number of answers received with kDNSClass_UniqueRRSet bit set
	DNSQuestion          *DuplicateOf;
	DNSQuestion          *NextInDQList;
	DupSuppressInfo       DupSuppress[DupSuppressInfoSize];
	mDNSInterfaceID       SendQNow;			// The interface this query is being sent on right now
	mDNSBool              SendOnAll;		// Set if we're sending this question on all active interfaces
	mDNSs32               LastQTxTime;		// Last time this Q was sent on one (but not necessarily all) interfaces

	// Client API fields: The client must set up these fields *before* calling mDNS_StartQuery()
	mDNSInterfaceID       InterfaceID;		// Non-zero if you want to issue link-local queries only on a single specific IP interface
	domainname            qname;
	mDNSu16               qtype;
	mDNSu16               qclass;
	mDNSQuestionCallback *QuestionCallback;
	void                 *QuestionContext;
	};

typedef struct
	{
	// Client API fields: The client must set up name and InterfaceID *before* calling mDNS_StartResolveService()
	// When the callback is invoked, ip, port, TXTlen and TXTinfo will have been filled in with the results learned from the network.
	domainname      name;
	mDNSInterfaceID InterfaceID;		// ID of the interface the response was received on
	mDNSAddr        ip;					// Remote (destination) IP address where this service can be accessed
	mDNSIPPort      port;				// Port where this service can be accessed
	mDNSu16         TXTlen;
	mDNSu8          TXTinfo[2048];		// Additional demultiplexing information (e.g. LPR queue name)
	} ServiceInfo;

// Note: Within an mDNSServiceInfoQueryCallback mDNS all API calls are legal except mDNS_Init(), mDNS_Close(), mDNS_Execute() 
typedef struct ServiceInfoQuery_struct ServiceInfoQuery;
typedef void mDNSServiceInfoQueryCallback(mDNS *const m, ServiceInfoQuery *query);
struct ServiceInfoQuery_struct
	{
	// Internal state fields. These are used internally by mDNSCore; the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_StartResolveService();
	// all required data is passed as parameters to that function.
	// The ServiceInfoQuery structure memory is working storage for mDNSCore to discover the requested information
	// and place it in the ServiceInfo structure. After the client has called mDNS_StopResolveService(), it may
	// dispose of the ServiceInfoQuery structure while retaining the results in the ServiceInfo structure.
	DNSQuestion                   qSRV;
	DNSQuestion                   qTXT;
	DNSQuestion                   qAv4;
	DNSQuestion                   qAv6;
	mDNSu8                        GotSRV;
	mDNSu8                        GotTXT;
	mDNSu8                        GotADD;
	mDNSu32                       Answers;
	ServiceInfo                  *info;
	mDNSServiceInfoQueryCallback *ServiceInfoQueryCallback;
	void                         *ServiceInfoQueryContext;
	};

// ***************************************************************************
#if 0
#pragma mark - Main mDNS object, used to hold all the mDNS state
#endif

typedef void mDNSCallback(mDNS *const m, mStatus result);

#define CACHE_HASH_SLOTS 499

enum
	{
	mDNS_KnownBug_PhantomInterfaces = 1
	};

struct mDNS_struct
	{
	// Internal state fields. These hold the main internal state of mDNSCore;
	// the client layer needn't be concerned with them.
	// No fields need to be set up by the client prior to calling mDNS_Init();
	// all required data is passed as parameters to that function.

	mDNS_PlatformSupport *p;			// Pointer to platform-specific data of indeterminite size
	mDNSu32  KnownBugs;
	mDNSBool AdvertiseLocalAddresses;
	mStatus mDNSPlatformStatus;
	mDNSCallback *MainCallback;
	void         *MainContext;

	// For debugging: To catch and report locking failures
	mDNSu32 mDNS_busy;					// Incremented between mDNS_Lock/mDNS_Unlock section
	mDNSu32 mDNS_reentrancy;			// Incremented when calling a client callback
	mDNSu8  mDNS_shutdown;				// Set when we're shutting down, allows us to skip some unnecessary steps
	mDNSu8  lock_rrcache;				// For debugging: Set at times when these lists may not be modified
	mDNSu8  lock_Questions;
	mDNSu8  lock_Records;
	char MsgBuffer[80];					// Temp storage used while building error log messages

	// Task Scheduling variables
	mDNSs32  timenow;					// The time that this particular activation of the mDNS code started
	mDNSs32  timenow_last;				// The time the last time we ran
	mDNSs32  timenow_adjust;			// Correction applied if we ever discover time went backwards
	mDNSs32  NextScheduledEvent;		// Derived from values below
	mDNSs32  SuppressSending;			// Don't send *any* packets during this time
	mDNSs32  NextCacheCheck;			// Next time to refresh cache record before it expires
	mDNSs32  NextScheduledQuery;		// Next time to send query in its exponential backoff sequence
	mDNSs32  NextScheduledProbe;		// Next time to probe for new authoritative record
	mDNSs32  NextScheduledResponse;		// Next time to send authoritative record(s) in responses
	mDNSs32  ExpectUnicastResponse;		// Set when we send a query with the kDNSQClass_UnicastResponse bit set
	mDNSs32  RandomQueryDelay;			// For de-synchronization of query packets on the wire
	mDNSBool SendDeregistrations;		// Set if we need to send deregistrations (immediately)
	mDNSBool SendImmediateAnswers;		// Set if we need to send answers (immediately -- or as soon as SuppressSending clears)
	mDNSBool SleepState;				// Set if we're sleeping (send no more packets)

	// These fields only required for mDNS Searcher...
	DNSQuestion *Questions;				// List of all registered questions, active and inactive
	DNSQuestion *NewQuestions;			// Fresh questions not yet answered from cache
	DNSQuestion *CurrentQuestion;		// Next question about to be examined in AnswerLocalQuestions()
	DNSQuestion *LocalOnlyQuestions;	// Questions with InterfaceID set to ~0 ("local only")
	DNSQuestion *NewLocalOnlyQuestions;	// Fresh local-only questions not yet answered
	mDNSu32 rrcache_size;				// Total number of available cache entries
	mDNSu32	rrcache_totalused;			// Number of cache entries currently occupied
	mDNSu32 rrcache_active;				// Number of cache entries currently occupied by records that answer active questions
	mDNSu32 rrcache_report;
	CacheRecord *rrcache_free;
	CacheRecord *rrcache_hash[CACHE_HASH_SLOTS];
	CacheRecord **rrcache_tail[CACHE_HASH_SLOTS];
	mDNSu32 rrcache_used[CACHE_HASH_SLOTS];

	// Fields below only required for mDNS Responder...
	domainlabel nicelabel;				// Rich text label encoded using canonically precomposed UTF-8
	domainlabel hostlabel;				// Conforms to RFC 1034 "letter-digit-hyphen" ARPANET host name rules
	domainname  hostname;				// Host Name, e.g. "Foo.local."
	UTF8str255 HIHardware;
	UTF8str255 HISoftware;
	AuthRecord *ResourceRecords;
	AuthRecord *DuplicateRecords;		// Records currently 'on hold' because they are duplicates of existing records
	AuthRecord *LocalOnlyRecords;		// Local records registered with InterfaceID set to ~0 ("local only")
	AuthRecord *NewLocalOnlyRecords;	// Fresh local-only records not yet delivered to local-only questions
	mDNSBool    DiscardLocalOnlyRecords;// Set when we have "remove" events we need to deliver to local-only questions
	AuthRecord *CurrentRecord;			// Next AuthRecord about to be examined
	NetworkInterfaceInfo *HostInterfaces;
	mDNSs32 ProbeFailTime;
	mDNSs32 NumFailedProbes;
	mDNSs32 SuppressProbes;
	};

// ***************************************************************************
#if 0
#pragma mark - Useful Static Constants
#endif

extern const mDNSIPPort      zeroIPPort;
extern const mDNSv4Addr      zeroIPAddr;
extern const mDNSv6Addr      zerov6Addr;
extern const mDNSv4Addr      onesIPv4Addr;
extern const mDNSv6Addr      onesIPv6Addr;
extern const mDNSInterfaceID mDNSInterface_Any;

extern const mDNSIPPort      UnicastDNSPort;
extern const mDNSIPPort      MulticastDNSPort;
extern const mDNSv4Addr      AllDNSAdminGroup;
extern const mDNSv4Addr      AllDNSLinkGroup;
extern const mDNSv6Addr      AllDNSLinkGroupv6;
extern const mDNSAddr        AllDNSLinkGroup_v4;
extern const mDNSAddr        AllDNSLinkGroup_v6;

// ***************************************************************************
#if 0
#pragma mark - Main Client Functions
#endif

// Every client should call mDNS_Init, passing in storage for the mDNS object, mDNS_PlatformSupport object, and rrcache.
// The rrcachesize parameter is the size of (i.e. number of entries in) the rrcache array passed in.
// Most clients use mDNS_Init_AdvertiseLocalAddresses. This causes mDNSCore to automatically
// create the correct address records for all the hosts interfaces. If you plan to advertise
// services being offered by the local machine, this is almost always what you want.
// There are two cases where you might use mDNS_Init_DontAdvertiseLocalAddresses:
// 1. A client-only device, that browses for services but doesn't advertise any of its own.
// 2. A proxy-registration service, that advertises services being offered by other machines, and takes
//    the appropriate steps to manually create the correct address records for those other machines.
// In principle, a proxy-like registration service could manually create address records for its own machine too,
// but this would be pointless extra effort when using mDNS_Init_AdvertiseLocalAddresses does that for you.
//
// When mDNS has finished setting up the client's callback is called
// A client can also spin and poll the mDNSPlatformStatus field to see when it changes from mStatus_Waiting to mStatus_NoError
//
// Call mDNS_Close to tidy up before exiting
//
// Call mDNS_Register with a completed AuthRecord object to register a resource record
// If the resource record type is kDNSRecordTypeUnique (or kDNSknownunique) then if a conflicting resource record is discovered,
// the resource record's mDNSRecordCallback will be called with error code mStatus_NameConflict. The callback should deregister
// the record, and may then try registering the record again after picking a new name (e.g. by automatically appending a number).
//
// Call mDNS_StartQuery to initiate a query. mDNS will proceed to issue Multicast DNS query packets, and any time a response
// is received containing a record which matches the question, the DNSQuestion's mDNSAnswerCallback function will be called
// Call mDNS_StopQuery when no more answers are required
//
// Care should be taken on multi-threaded or interrupt-driven environments.
// The main mDNS routines call mDNSPlatformLock() on entry and mDNSPlatformUnlock() on exit;
// each platform layer needs to implement these appropriately for its respective platform.
// For example, if the support code on a particular platform implements timer callbacks at interrupt time, then
// mDNSPlatformLock/Unlock need to disable interrupts or do similar concurrency control to ensure that the mDNS
// code is not entered by an interrupt-time timer callback while in the middle of processing a client call.

extern mStatus mDNS_Init      (mDNS *const m, mDNS_PlatformSupport *const p,
								CacheRecord *rrcachestorage, mDNSu32 rrcachesize,
								mDNSBool AdvertiseLocalAddresses,
								mDNSCallback *Callback, void *Context);
// See notes above on use of NoCache/ZeroCacheSize
#define mDNS_Init_NoCache                     mDNSNULL
#define mDNS_Init_ZeroCacheSize               0
// See notes above on use of Advertise/DontAdvertiseLocalAddresses
#define mDNS_Init_AdvertiseLocalAddresses     mDNStrue
#define mDNS_Init_DontAdvertiseLocalAddresses mDNSfalse
#define mDNS_Init_NoInitCallback              mDNSNULL
#define mDNS_Init_NoInitCallbackContext       mDNSNULL

extern void    mDNS_GrowCache (mDNS *const m, CacheRecord *storage, mDNSu32 numrecords);
extern void    mDNS_Close     (mDNS *const m);
extern mDNSs32 mDNS_Execute   (mDNS *const m);

extern mStatus mDNS_Register  (mDNS *const m, AuthRecord *const rr);
extern mStatus mDNS_Update    (mDNS *const m, AuthRecord *const rr, mDNSu32 newttl,
								const mDNSu16 newrdlength, 
								RData *const newrdata, mDNSRecordUpdateCallback *Callback);
extern mStatus mDNS_Deregister(mDNS *const m, AuthRecord *const rr);

extern mStatus mDNS_StartQuery(mDNS *const m, DNSQuestion *const question);
extern mStatus mDNS_StopQuery (mDNS *const m, DNSQuestion *const question);
extern mStatus mDNS_Reconfirm (mDNS *const m, CacheRecord *const cacherr);
extern mStatus mDNS_ReconfirmByValue(mDNS *const m, ResourceRecord *const rr);

// ***************************************************************************
#if 0
#pragma mark - Platform support functions that are accessible to the client layer too
#endif

extern mDNSs32  mDNSPlatformOneSecond;
extern mDNSs32  mDNSPlatformTimeNow(void);

// ***************************************************************************
#if 0
#pragma mark - General utility and helper functions
#endif

// mDNS_RegisterService is a single call to register the set of resource records associated with a given named service.
//
// mDNS_StartResolveService is single call which is equivalent to multiple calls to mDNS_StartQuery,
// to find the IP address, port number, and demultiplexing information for a given named service.
// As with mDNS_StartQuery, it executes asynchronously, and calls the ServiceInfoQueryCallback when the answer is
// found. After the service is resolved, the client should call mDNS_StopResolveService to complete the transaction.
// The client can also call mDNS_StopResolveService at any time to abort the transaction.
//
// mDNS_GetBrowseDomains is a special case of the mDNS_StartQuery call, where the resulting answers
// are a list of PTR records indicating (in the rdata) domains that are recommended for browsing.
// After getting the list of domains to browse, call mDNS_StopQuery to end the search.
// mDNS_GetDefaultBrowseDomain returns the name of the domain that should be highlighted by default.
//
// mDNS_GetRegistrationDomains and mDNS_GetDefaultRegistrationDomain are the equivalent calls to get the list
// of one or more domains that should be offered to the user as choices for where they may register their service,
// and the default domain in which to register in the case where the user has made no selection.

extern void    mDNS_SetupResourceRecord(AuthRecord *rr, RData *RDataStorage, mDNSInterfaceID InterfaceID,
               mDNSu16 rrtype, mDNSu32 ttl, mDNSu8 RecordType, mDNSRecordCallback Callback, void *Context);

extern mStatus mDNS_RegisterService  (mDNS *const m, ServiceRecordSet *sr,
               const domainlabel *const name, const domainname *const type, const domainname *const domain,
               const domainname *const host, mDNSIPPort port, const mDNSu8 txtinfo[], mDNSu16 txtlen,
               AuthRecord *SubTypes, mDNSu32 NumSubTypes,
               const mDNSInterfaceID InterfaceID, mDNSServiceCallback Callback, void *Context);
extern mStatus mDNS_AddRecordToService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra, RData *rdata, mDNSu32 ttl);
extern mStatus mDNS_RemoveRecordFromService(mDNS *const m, ServiceRecordSet *sr, ExtraResourceRecord *extra);
extern mStatus mDNS_RenameAndReregisterService(mDNS *const m, ServiceRecordSet *const sr, const domainlabel *newname);
extern mStatus mDNS_DeregisterService(mDNS *const m, ServiceRecordSet *sr);

extern mStatus mDNS_RegisterNoSuchService(mDNS *const m, AuthRecord *const rr,
               const domainlabel *const name, const domainname *const type, const domainname *const domain,
               const domainname *const host,
               const mDNSInterfaceID InterfaceID, mDNSRecordCallback Callback, void *Context);
#define        mDNS_DeregisterNoSuchService mDNS_Deregister

extern mStatus mDNS_StartBrowse(mDNS *const m, DNSQuestion *const question,
               const domainname *const srv, const domainname *const domain,
               const mDNSInterfaceID InterfaceID, mDNSQuestionCallback *Callback, void *Context);
#define        mDNS_StopBrowse mDNS_StopQuery

extern mStatus mDNS_StartResolveService(mDNS *const m, ServiceInfoQuery *query, ServiceInfo *info, mDNSServiceInfoQueryCallback *Callback, void *Context);
extern void    mDNS_StopResolveService (mDNS *const m, ServiceInfoQuery *query);

typedef enum
	{
	mDNS_DomainTypeBrowse              = 0,
	mDNS_DomainTypeBrowseDefault       = 1,
	mDNS_DomainTypeRegistration        = 2,
	mDNS_DomainTypeRegistrationDefault = 3
	} mDNS_DomainType;

extern mStatus mDNS_GetDomains(mDNS *const m, DNSQuestion *const question, mDNS_DomainType DomainType, const mDNSInterfaceID InterfaceID, mDNSQuestionCallback *Callback, void *Context);
// In the Panther mDNSResponder we don't do unicast queries yet, so there's no point trying to do domain enumeration
// mDNS_GetDomains() and mDNS_StopGetDomains() are set to be no-ops so that clients don't try to do browse/register operations that will fail
//#define        mDNS_StopGetDomains mDNS_StopQuery
#define        mDNS_StopGetDomains(m,q) ((void)(m),(void)(q))
extern mStatus mDNS_AdvertiseDomains(mDNS *const m, AuthRecord *rr, mDNS_DomainType DomainType, const mDNSInterfaceID InterfaceID, char *domname);
#define        mDNS_StopAdvertiseDomains mDNS_Deregister

// ***************************************************************************
#if 0
#pragma mark - DNS name utility functions
#endif

// In order to expose the full capabilities of the DNS protocol (which allows any arbitrary eight-bit values
// in domain name labels, including unlikely characters like ascii nulls and even dots) all the mDNS APIs
// work with DNS's native length-prefixed strings. For convenience in C, the following utility functions
// are provided for converting between C's null-terminated strings and DNS's length-prefixed strings.

// Assignment
// A simple C structure assignment of a domainname can cause a protection fault by accessing unmapped memory,
// because that object is defined to be 256 bytes long, but not all domainname objects are truly the full size.
// This macro uses mDNSPlatformMemCopy() to make sure it only touches the actual bytes that are valid.
#define AssignDomainName(DST, SRC) mDNSPlatformMemCopy((SRC).c, (DST).c, DomainNameLength(&(SRC)))

// Comparison functions
extern mDNSBool SameDomainLabel(const mDNSu8 *a, const mDNSu8 *b);
extern mDNSBool SameDomainName(const domainname *const d1, const domainname *const d2);

// Get total length of domain name, in native DNS format, including terminal root label
//   (e.g. length of "com." is 5 (length byte, three data bytes, final zero)
extern mDNSu16  DomainNameLength(const domainname *const name);

// Append functions to append one or more labels to an existing native format domain name:
//   AppendLiteralLabelString adds a single label from a literal C string, with no escape character interpretation.
//   AppendDNSNameString      adds zero or more labels from a C string using conventional DNS dots-and-escaping interpretation
//   AppendDomainLabel        adds a single label from a native format domainlabel
//   AppendDomainName         adds zero or more labels from a native format domainname
extern mDNSu8  *AppendLiteralLabelString(domainname *const name, const char *cstr);
extern mDNSu8  *AppendDNSNameString     (domainname *const name, const char *cstr);
extern mDNSu8  *AppendDomainLabel       (domainname *const name, const domainlabel *const label);
extern mDNSu8  *AppendDomainName        (domainname *const name, const domainname *const append);

// Convert from null-terminated string to native DNS format:
//   The DomainLabel form makes a single label from a literal C string, with no escape character interpretation.
//   The DomainName form makes native format domain name from a C string using conventional DNS interpretation:
//     dots separate labels, and within each label, '\.' represents a literal dot, '\\' represents a literal
//     backslash and backslash with three decimal digits (e.g. \000) represents an arbitrary byte value.
extern mDNSBool MakeDomainLabelFromLiteralString(domainlabel *const label, const char *cstr);
extern mDNSu8  *MakeDomainNameFromDNSNameString (domainname  *const name,  const char *cstr);

// Convert native format domainlabel or domainname back to C string format
// IMPORTANT:
// When using ConvertDomainLabelToCString, the target buffer must be MAX_ESCAPED_DOMAIN_LABEL (254) bytes long
// to guarantee there will be no buffer overrun. It is only safe to use a buffer shorter than this in rare cases
// where the label is known to be constrained somehow (for example, if the label is known to be either "_tcp" or "_udp").
// Similarly, when using ConvertDomainNameToCString, the target buffer must be MAX_ESCAPED_DOMAIN_NAME (1005) bytes long.
// See definitions of MAX_ESCAPED_DOMAIN_LABEL and MAX_ESCAPED_DOMAIN_NAME for more detailed explanation.
extern char    *ConvertDomainLabelToCString_withescape(const domainlabel *const name, char *cstr, char esc);
#define         ConvertDomainLabelToCString_unescaped(D,C) ConvertDomainLabelToCString_withescape((D), (C), 0)
#define         ConvertDomainLabelToCString(D,C)           ConvertDomainLabelToCString_withescape((D), (C), '\\')
extern char    *ConvertDomainNameToCString_withescape(const domainname *const name, char *cstr, char esc);
#define         ConvertDomainNameToCString_unescaped(D,C) ConvertDomainNameToCString_withescape((D), (C), 0)
#define         ConvertDomainNameToCString(D,C)           ConvertDomainNameToCString_withescape((D), (C), '\\')

extern void     ConvertUTF8PstringToRFC1034HostLabel(const mDNSu8 UTF8Name[], domainlabel *const hostlabel);

extern mDNSu8  *ConstructServiceName(domainname *const fqdn, const domainlabel *name, const domainname *type, const domainname *const domain);
extern mDNSBool DeconstructServiceName(const domainname *const fqdn, domainlabel *const name, domainname *const type, domainname *const domain);

// Note: Some old functions have been replaced by more sensibly-named versions.
// You can uncomment the hash-defines below if you don't want to have to change your source code right away.
// When updating your code, note that (unlike the old versions) *all* the new routines take the target object
// as their first parameter.
//#define ConvertCStringToDomainName(SRC,DST)  MakeDomainNameFromDNSNameString((DST),(SRC))
//#define ConvertCStringToDomainLabel(SRC,DST) MakeDomainLabelFromLiteralString((DST),(SRC))
//#define AppendStringLabelToName(DST,SRC)     AppendLiteralLabelString((DST),(SRC))
//#define AppendStringNameToName(DST,SRC)      AppendDNSNameString((DST),(SRC))
//#define AppendDomainLabelToName(DST,SRC)     AppendDomainLabel((DST),(SRC))
//#define AppendDomainNameToName(DST,SRC)      AppendDomainName((DST),(SRC))

// ***************************************************************************
#if 0
#pragma mark - Other utility functions
#endif

extern mDNSu32 mDNS_vsnprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, va_list arg);
extern mDNSu32 mDNS_snprintf(char *sbuffer, mDNSu32 buflen, const char *fmt, ...) IS_A_PRINTF_STYLE_FUNCTION(3,4);
extern char *DNSTypeName(mDNSu16 rrtype);
extern char *GetRRDisplayString_rdb(mDNS *const m, const ResourceRecord *rr, RDataBody *rd);
#define GetRRDisplayString(m, rr) GetRRDisplayString_rdb((m), &(rr)->resrec, &(rr)->resrec.rdata->u)
extern mDNSBool mDNSSameAddress(const mDNSAddr *ip1, const mDNSAddr *ip2);
extern void IncrementLabelSuffix(domainlabel *name, mDNSBool RichText);

// ***************************************************************************
#if 0
#pragma mark - PlatformSupport interface
#endif

// This section defines the interface to the Platform Support layer.
// Normal client code should not use any of types defined here, or directly call any of the functions defined here.
// The definitions are placed here because sometimes clients do use these calls indirectly, via other supported client operations.
// For example, AssignDomainName is a macro defined using mDNSPlatformMemCopy()

typedef packedstruct
	{
	mDNSOpaque16 id;
	mDNSOpaque16 flags;
	mDNSu16 numQuestions;
	mDNSu16 numAnswers;
	mDNSu16 numAuthorities;
	mDNSu16 numAdditionals;
	} DNSMessageHeader;

// We can send and receive packets up to 9000 bytes (Ethernet Jumbo Frame size, if that ever becomes widely used)
// However, in the normal case we try to limit packets to 1500 bytes so that we don't get IP fragmentation on standard Ethernet
// 40 (IPv6 header) + 8 (UDP header) + 12 (DNS message header) + 1440 (DNS message body) = 1500 total
#define AbsoluteMaxDNSMessageData 8940
#define NormalMaxDNSMessageData 1440
typedef packedstruct
	{
	DNSMessageHeader h;						// Note: Size 12 bytes
	mDNSu8 data[AbsoluteMaxDNSMessageData];	// 40 (IPv6) + 8 (UDP) + 12 (DNS header) + 8940 (data) = 9000
	} DNSMessage;

// Every platform support module must provide the following functions.
// mDNSPlatformInit() typically opens a communication endpoint, and starts listening for mDNS packets.
// When Setup is complete, the platform support layer calls mDNSCoreInitComplete().
// mDNSPlatformSendUDP() sends one UDP packet
// When a packet is received, the PlatformSupport code calls mDNSCoreReceive()
// mDNSPlatformClose() tidies up on exit
// Note: mDNSPlatformMemAllocate/mDNSPlatformMemFree are only required for handling oversized resource records.
// If your target platform has a well-defined specialized application, and you know that all the records it uses
// are InlineCacheRDSize or less, then you can just make a simple mDNSPlatformMemAllocate() stub that always returns
// NULL. InlineCacheRDSize is a compile-time constant, which is set by default to 64. If you need to handle records
// a little larger than this and you don't want to have to implement run-time allocation and freeing, then you
// can raise the value of this constant to a suitable value (at the expense of increased memory usage).
extern mStatus  mDNSPlatformInit        (mDNS *const m);
extern void     mDNSPlatformClose       (mDNS *const m);
extern mStatus  mDNSPlatformSendUDP(const mDNS *const m, const DNSMessage *const msg, const mDNSu8 *const end,
	mDNSInterfaceID InterfaceID, mDNSIPPort srcport, const mDNSAddr *dst, mDNSIPPort dstport);

extern void     mDNSPlatformLock        (const mDNS *const m);
extern void     mDNSPlatformUnlock      (const mDNS *const m);

extern void     mDNSPlatformStrCopy     (const void *src,       void *dst);
extern mDNSu32  mDNSPlatformStrLen      (const void *src);
extern void     mDNSPlatformMemCopy     (const void *src,       void *dst, mDNSu32 len);
extern mDNSBool mDNSPlatformMemSame     (const void *src, const void *dst, mDNSu32 len);
extern void     mDNSPlatformMemZero     (                       void *dst, mDNSu32 len);
extern void *   mDNSPlatformMemAllocate (mDNSu32 len);
extern void     mDNSPlatformMemFree     (void *mem);
extern mStatus  mDNSPlatformTimeInit    (mDNSs32 *timenow);

// The core mDNS code provides these functions, for the platform support code to call at appropriate times
//
// mDNS_GenerateFQDN() is called once on startup (typically from mDNSPlatformInit())
// and then again on each subsequent change of the dot-local host name.
//
// mDNS_RegisterInterface() is used by the platform support layer to inform mDNSCore of what
// physical and/or logical interfaces are available for sending and receiving packets.
// Typically it is called on startup for each available interface, but register/deregister may be
// called again later, on multiple occasions, to inform the core of interface configuration changes.
// If set->Advertise is set non-zero, then mDNS_RegisterInterface() also registers the standard
// resource records that should be associated with every publicised IP address/interface:
// -- Name-to-address records (A/AAAA)
// -- Address-to-name records (PTR)
// -- Host information (HINFO)
//
// mDNSCoreInitComplete() is called when the platform support layer is finished.
// Typically this is at the end of mDNSPlatformInit(), but may be later
// (on platforms like OT that allow asynchronous initialization of the networking stack).
//
// mDNSCoreReceive() is called when a UDP packet is received
//
// mDNSCoreMachineSleep() is called when the machine sleeps or wakes
// (This refers to heavyweight laptop-style sleep/wake that disables network access,
// not lightweight second-by-second CPU power management modes.)

extern void     mDNS_GenerateFQDN(mDNS *const m);
extern mStatus  mDNS_RegisterInterface  (mDNS *const m, NetworkInterfaceInfo *set);
extern void     mDNS_DeregisterInterface(mDNS *const m, NetworkInterfaceInfo *set);
extern void     mDNSCoreInitComplete(mDNS *const m, mStatus result);
extern void     mDNSCoreReceive(mDNS *const m, DNSMessage *const msg, const mDNSu8 *const end,
								const mDNSAddr *const srcaddr, const mDNSIPPort srcport,
								const mDNSAddr *const dstaddr, const mDNSIPPort dstport, const mDNSInterfaceID InterfaceID, mDNSu8 ttl);
extern void     mDNSCoreMachineSleep(mDNS *const m, mDNSBool wake);

// ***************************************************************************
#if 0
#pragma mark - Compile-Time assertion checks
#endif

// Some C compiler cleverness. We can make the compiler check certain things for
// us, and report compile-time errors if anything is wrong. The usual way to do
// this would be to use a run-time "if" statement, but then you don't find out
// what's wrong until you run the software. This way, if the assertion condition
// is false, the array size is negative, and the complier complains immediately.

struct mDNS_CompileTimeAssertionChecks
	{
	// Check that the compiler generated our on-the-wire packet format structure definitions
	// properly packed, without adding padding bytes to align fields on 32-bit or 64-bit boundaries.
	char assert0[(sizeof(rdataSRV)         == 262                          ) ? 1 : -1];
	char assert1[(sizeof(DNSMessageHeader) ==  12                          ) ? 1 : -1];
	char assert2[(sizeof(DNSMessage)       ==  12+AbsoluteMaxDNSMessageData) ? 1 : -1];
	char assert3[(sizeof(mDNSs8)           ==   1                          ) ? 1 : -1];
	char assert4[(sizeof(mDNSu8)           ==   1                          ) ? 1 : -1];
	char assert5[(sizeof(mDNSs16)          ==   2                          ) ? 1 : -1];
	char assert6[(sizeof(mDNSu16)          ==   2                          ) ? 1 : -1];
	char assert7[(sizeof(mDNSs32)          ==   4                          ) ? 1 : -1];
	char assert8[(sizeof(mDNSu32)          ==   4                          ) ? 1 : -1];
	char assert9[(sizeof(mDNSOpaque16)     ==   2                          ) ? 1 : -1];
	char assertA[(sizeof(mDNSOpaque32)     ==   4                          ) ? 1 : -1];
	char assertB[(sizeof(mDNSOpaque128)    ==  16                          ) ? 1 : -1];
	};

// ***************************************************************************

#ifdef	__cplusplus
	}
#endif

#endif
