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

$Log: mDNSPosix.h,v $
Revision 1.1  2009-06-30 02:31:08  steven
iTune Server

Revision 1.2  2005/01/10 01:07:01  rpedde
Synchronize mDNS to Apples 58.8 drop

Revision 1.9  2003/10/30 19:25:19  cheshire
Fix warning on certain compilers

Revision 1.8  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.7  2003/07/02 21:19:59  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.6  2003/03/13 03:46:21  cheshire
Fixes to make the code build on Linux

Revision 1.5  2003/03/08 00:35:56  cheshire
Switched to using new "mDNS_Execute" model (see "mDNSCore/Implementer Notes.txt")

Revision 1.4  2002/12/23 22:13:31  jgraessl

Reviewed by: Stuart Cheshire
Initial IPv6 support for mDNSResponder.

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 04:20:44  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.1  2002/09/17 06:24:34  cheshire
First checkin

*/

#ifndef __mDNSPlatformPosix_h
#define __mDNSPlatformPosix_h

#include <sys/time.h>

#if HAVE_IPV6
#define mDNSIPv6Support 1
#endif

#ifdef  __cplusplus
    extern "C" {
#endif

// This is a global because debugf_() needs to be able to check its value
extern int gMDNSPlatformPosixVerboseLevel;

struct mDNS_PlatformSupport_struct
	{
    // No additional data required for Posix at this time
	long dummy[1];	// Some compilers don't like empty structures
	};

extern mStatus mDNSPlatformPosixRefreshInterfaceList(mDNS *const m);
    // See comment in implementation.

// Call mDNSPosixGetFDSet before calling select(), to update the parameters
// as may be necessary to meet the needs of the mDNSCore code.
// The timeout pointer MUST NOT be NULL.
// Set timeout->tv_sec to 0x3FFFFFFF if you want to have effectively no timeout
// After calling mDNSPosixGetFDSet(), call select(nfds, &readfds, NULL, NULL, &timeout); as usual
// After select() returns, call mDNSPosixProcessFDSet() to let mDNSCore do its work
extern void mDNSPosixGetFDSet(mDNS *const m, int *nfds, fd_set *readfds, struct timeval *timeout);
extern void mDNSPosixProcessFDSet(mDNS *const m, fd_set *readfds);

#ifdef  __cplusplus
    }
#endif

#endif
