/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

$Log: helper-stubs.c,v $
Revision 1.19  2009/04/20 20:40:14  cheshire
<rdar://problem/6786150> uDNS: Running location cycling caused configd and mDNSResponder to deadlock
Changed mDNSPreferencesSetName (and similar) routines from MIG "routine" to MIG "simpleroutine"
so we don't deadlock waiting for a result that we're just going to ignore anyway

Revision 1.18  2009/03/20 22:12:27  mcguire
<rdar://problem/6703952> Support CFUserNotificationDisplayNotice in mDNSResponderHelper
Make the call to the helper a simpleroutine: don't wait for an unused return value

Revision 1.17  2009/03/20 20:52:22  cheshire
<rdar://problem/6703952> Support CFUserNotificationDisplayNotice in mDNSResponderHelper

Revision 1.16  2009/03/14 01:42:56  mcguire
<rdar://problem/5457116> BTMM: Fix issues with multiple .Mac accounts on the same machine

Revision 1.15  2009/01/22 02:14:27  cheshire
<rdar://problem/6515626> Sleep Proxy: Set correct target MAC address, instead of all zeroes

Revision 1.14  2009/01/14 01:38:42  mcguire
<rdar://problem/6492710> Write out DynamicStore per-interface SleepProxyServer info

Revision 1.13  2009/01/14 01:28:17  mcguire
removed unused variable

Revision 1.12  2008/11/11 00:46:37  cheshire
Don't just show "<unknown error>"; show the actual numeric error code too, so we can see what the unknown error was

Revision 1.11  2008/11/04 23:54:09  cheshire
Added routine mDNSSetARP(), used to replace an SPS client's entry in our ARP cache with
a dummy one, so that IP traffic to the SPS client initiated by the SPS machine can be
captured by our BPF filters, and used as a trigger to wake the sleeping machine.

Revision 1.10  2008/10/29 21:25:35  cheshire
Don't report kIOReturnNotReady errors

Revision 1.9  2008/10/24 01:42:36  cheshire
Added mDNSPowerRequest helper routine to request a scheduled wakeup some time in the future

Revision 1.8  2008/10/20 22:01:28  cheshire
Made new Mach simpleroutine "mDNSRequestBPF"

Revision 1.7  2008/09/27 00:58:32  cheshire
Added mDNSRequestBPF definition

Revision 1.6  2007/12/10 23:23:48  cheshire
Removed unnecessary log message ("mDNSKeychainGetSecrets failed 0 00000000" because mDNSKeychainGetSecrets was failing to return a valid array)

Revision 1.5  2007/09/07 22:44:03  mcguire
<rdar://problem/5448420> Move CFUserNotification code to mDNSResponderHelper

Revision 1.4  2007/09/04 22:32:58  mcguire
<rdar://problem/5453633> BTMM: BTMM overwrites /etc/racoon/remote/anonymous.conf

Revision 1.3  2007/08/23 21:44:55  cheshire
Made code layout style consistent with existing project style; added $Log header

Revision 1.2  2007/08/18 01:02:03  mcguire
<rdar://problem/5415593> No Bonjour services are getting registered at boot

Revision 1.1  2007/08/08 22:34:58  mcguire
<rdar://problem/5197869> Security: Run mDNSResponder as user id mdnsresponder instead of root
 */

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/vm_map.h>
#include <servers/bootstrap.h>
#include <IOKit/IOReturn.h>
#include <CoreFoundation/CoreFoundation.h>
#include "mDNSDebug.h"
#include "helper.h"
#include "helpermsg.h"

#define ERROR(x, y) y,
static const char *errorstring[] =
	{
	#include "helper-error.h"
	NULL
	};
#undef ERROR

static mach_port_t getHelperPort(int retry)
	{
	static mach_port_t port = MACH_PORT_NULL;
	if (retry) port = MACH_PORT_NULL;
	if (port == MACH_PORT_NULL && BOOTSTRAP_SUCCESS != bootstrap_look_up(bootstrap_port, kmDNSHelperServiceName, &port))
		LogMsg("%s: cannot contact helper", __func__);
	return port;
	}

const char *mDNSHelperError(int err)
	{
	static const char *p = "<unknown error>";
	if (mDNSHelperErrorBase < err && mDNSHelperErrorEnd > err)
		p = errorstring[err - mDNSHelperErrorBase - 1];
	return p;
	}

/* Ugly but handy. */
// We don't bother reporting kIOReturnNotReady because that error code occurs in "normal" operation
// and doesn't indicate anything unexpected that needs to be investigated

#define MACHRETRYLOOP_BEGIN(kr, retry, err, fin)                                            \
	for (;;)                                                                                \
		{
#define MACHRETRYLOOP_END(kr, retry, err, fin)												\
		if (KERN_SUCCESS == (kr)) break;													\
		else if (MACH_SEND_INVALID_DEST == (kr) && 0 == (retry)++) continue;				\
		else																				\
			{																				\
			(err) = kmDNSHelperCommunicationFailed;											\
			LogMsg("%s: Mach communication failed: %s", __func__, mach_error_string(kr));	\
			goto fin;																		\
			}																				\
		}																					\
	if (0 != (err) && kIOReturnNotReady != (err))											\
		{ LogMsg("%s: %d 0x%X (%s)", __func__, (err), (err), mDNSHelperError(err)); goto fin; }

void mDNSPreferencesSetName(int key, domainlabel *old, domainlabel *new)
	{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;
	char oldname[MAX_DOMAIN_LABEL+1] = {0};
	char newname[MAX_DOMAIN_LABEL+1] = {0};
	ConvertDomainLabelToCString_unescaped(old, oldname);
	if (new) ConvertDomainLabelToCString_unescaped(new, newname);

	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSPreferencesSetName(getHelperPort(retry), key, oldname, newname);
	MACHRETRYLOOP_END(kr, retry, err, fin);

fin:
	(void)err;
	}

void mDNSDynamicStoreSetConfig(int key, const char *subkey, CFPropertyListRef value)
	{
	CFWriteStreamRef stream = NULL;
	CFDataRef bytes = NULL;
	kern_return_t kr = KERN_FAILURE;
	int retry = 0;
	int err = 0;

	if (NULL == (stream = CFWriteStreamCreateWithAllocatedBuffers(NULL, NULL)))
		{
		err = kmDNSHelperCreationFailed;
		LogMsg("%s: CFWriteStreamCreateWithAllocatedBuffers failed", __func__);
		goto fin;
		}
	CFWriteStreamOpen(stream);
	if (0 == CFPropertyListWriteToStream(value, stream, kCFPropertyListBinaryFormat_v1_0, NULL))
		{
		err = kmDNSHelperPListWriteFailed;
		LogMsg("%s: CFPropertyListWriteToStream failed", __func__);
		goto fin;
		}
	if (NULL == (bytes = CFWriteStreamCopyProperty(stream, kCFStreamPropertyDataWritten)))
		{
		err = kmDNSHelperCreationFailed;
		LogMsg("%s: CFWriteStreamCopyProperty failed", __func__);
		goto fin;
		}
	CFWriteStreamClose(stream);
	CFRelease(stream);
	stream = NULL;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSDynamicStoreSetConfig(getHelperPort(retry), key, subkey ? subkey : "", (vm_offset_t)CFDataGetBytePtr(bytes), CFDataGetLength(bytes));
	MACHRETRYLOOP_END(kr, retry, err, fin);

fin:
	if (NULL != stream) { CFWriteStreamClose(stream); CFRelease(stream); }
	if (NULL != bytes) CFRelease(bytes);
	(void)err;
	}

void mDNSRequestBPF(void)
	{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0, err = 0;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSRequestBPF(getHelperPort(retry));
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	(void)err;
	}

int mDNSPowerRequest(int key, int interval)
	{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0, err = 0;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSPowerRequest(getHelperPort(retry), key, interval, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	return err;
	}

int mDNSSetARP(int ifindex, const v4addr_t ip, const ethaddr_t eth)
	{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0, err = 0;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSSetARP(getHelperPort(retry), ifindex, (uint8_t*)ip, (uint8_t*)eth, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	return err;
	}

void mDNSNotify(const char *title, const char *msg)	// Both strings are UTF-8 text
	{
	kern_return_t kr = KERN_FAILURE;
	int retry = 0, err = 0;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSNotify(getHelperPort(retry), title, msg);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	(void)err;
	}

int mDNSKeychainGetSecrets(CFArrayRef *result)
	{
	CFPropertyListRef plist = NULL;
	CFDataRef bytes = NULL;
	kern_return_t kr = KERN_FAILURE;
	unsigned int numsecrets = 0;
	void *secrets = NULL;
	mach_msg_type_number_t secretsCnt = 0;
	int retry = 0, err = 0;

	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSKeychainGetSecrets(getHelperPort(retry), &numsecrets, (vm_offset_t *)&secrets, &secretsCnt, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);

	if (NULL == (bytes = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, secrets, secretsCnt, kCFAllocatorNull)))
		{
		err = kmDNSHelperCreationFailed;
		LogMsg("%s: CFDataCreateWithBytesNoCopy failed", __func__);
		goto fin;
		}
	if (NULL == (plist = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, bytes, kCFPropertyListImmutable, NULL)))
		{
		err = kmDNSHelperInvalidPList;
		LogMsg("%s: CFPropertyListCreateFromXMLData failed", __func__);
		goto fin;
		}
	if (CFArrayGetTypeID() != CFGetTypeID(plist))
		{
		err = kmDNSHelperTypeError;
		LogMsg("%s: Unexpected result type", __func__);
		CFRelease(plist);
		plist = NULL;
		goto fin;
		}
	*result = (CFArrayRef)plist;

fin:
	if (NULL != bytes) CFRelease(bytes);
	if (NULL != secrets) vm_deallocate(mach_task_self(), (vm_offset_t)secrets, secretsCnt);
	return err;
	}

void mDNSAutoTunnelInterfaceUpDown(int updown, v6addr_t address)
	{
	kern_return_t kr = KERN_SUCCESS;
	int retry = 0, err = 0;
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSAutoTunnelInterfaceUpDown(getHelperPort(retry), updown, address);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	(void)err;
	}

void mDNSConfigureServer(int updown, const domainname *const fqdn)
	{
	kern_return_t kr = KERN_SUCCESS;
	int retry = 0, err = 0;
	char fqdnStr[MAX_ESCAPED_DOMAIN_NAME] = { 0 };
	if (fqdn && ConvertDomainNameToCString(fqdn, fqdnStr))
		{
		// remove the trailing dot, as that is not used in the keychain entry racoon will lookup
		mDNSu32 fqdnEnd = mDNSPlatformStrLen(fqdnStr);
		if (fqdnEnd) fqdnStr[fqdnEnd - 1] = 0;
		}
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSConfigureServer(getHelperPort(retry), updown, fqdnStr);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	(void)err;
	}

int mDNSAutoTunnelSetKeys(int replacedelete, v6addr_t local_inner,
    v4addr_t local_outer, short local_port, v6addr_t remote_inner,
    v4addr_t remote_outer, short remote_port, const domainname *const fqdn)
	{
	kern_return_t kr = KERN_SUCCESS;
	int retry = 0, err = 0;
	char fqdnStr[MAX_ESCAPED_DOMAIN_NAME] = { 0 };
	if (fqdn && ConvertDomainNameToCString(fqdn, fqdnStr))
		{
		// remove the trailing dot, as that is not used in the keychain entry racoon will lookup
		mDNSu32 fqdnEnd = mDNSPlatformStrLen(fqdnStr);
		if (fqdnEnd) fqdnStr[fqdnEnd - 1] = 0;
		}
	MACHRETRYLOOP_BEGIN(kr, retry, err, fin);
	kr = proxy_mDNSAutoTunnelSetKeys(getHelperPort(retry), replacedelete, local_inner, local_outer, local_port, remote_inner, remote_outer, remote_port, fqdnStr, &err);
	MACHRETRYLOOP_END(kr, retry, err, fin);
fin:
	return err;
	}
