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

$Log: mDNSPlatformFunctions.h,v $
Revision 1.1  2009-06-30 02:31:08  steven
iTune Server

Revision 1.2  2005/01/10 01:07:01  rpedde
Synchronize mDNS to Apples 58.8 drop

Revision 1.22.2.1  2003/12/05 00:03:34  cheshire
<rdar://problem/3487869> Use buffer size MAX_ESCAPED_DOMAIN_NAME instead of 256

Revision 1.22  2003/08/18 22:53:37  cheshire
<rdar://problem/3382647> mDNSResponder divide by zero in mDNSPlatformTimeNow()

Revision 1.21  2003/08/15 20:16:57  cheshire
Update comment for <rdar://problem/3366590> mDNSResponder takes too much RPRVT

Revision 1.20  2003/08/12 19:56:24  cheshire
Update to APSL 2.0

Revision 1.19  2003/08/05 22:20:15  cheshire
<rdar://problem/3330324> Need to check IP TTL on responses

Revision 1.18  2003/07/22 23:57:20  cheshire
Move platform-layer function prototypes from mDNSClientAPI.h to mDNSPlatformFunctions.h where they belong

Revision 1.17  2003/07/19 03:15:15  cheshire
Add generic MemAllocate/MemFree prototypes to mDNSPlatformFunctions.h,
and add the obvious trivial implementations to each platform support layer

Revision 1.16  2003/07/02 21:19:46  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.15  2003/05/23 22:39:45  cheshire
<rdar://problem/3268151> Need to adjust maximum packet size for IPv6

Revision 1.14  2003/04/28 21:54:57  cheshire
Fix compiler warning

Revision 1.13  2003/03/15 04:40:36  cheshire
Change type called "mDNSOpaqueID" to the more descriptive name "mDNSInterfaceID"

Revision 1.12  2003/02/21 01:54:08  cheshire
Bug #: 3099194 mDNSResponder needs performance improvements
Switched to using new "mDNS_Execute" model (see "Implementer Notes.txt")

Revision 1.11  2002/12/23 22:13:29  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.10  2002/09/21 20:44:49  zarzycki
Added APSL info

Revision 1.9  2002/09/19 04:20:43  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.8  2002/09/16 23:12:14  cheshire
Minor code tidying

Revision 1.7  2002/09/16 18:41:42  cheshire
Merge in license terms from Quinn's copy, in preparation for Darwin release

*/

// Note: All moved to mDNSClientAPI.h
