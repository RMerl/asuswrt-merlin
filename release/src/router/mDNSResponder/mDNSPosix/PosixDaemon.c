/* -*- Mode: C; tab-width: 4 -*-
 *
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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

	File:		daemon.c

	Contains:	main & associated Application layer for mDNSResponder on Linux.

	Change History (most recent first):

$Log: PosixDaemon.c,v $
Revision 1.49  2009/04/30 20:07:51  mcguire
<rdar://problem/6822674> Support multiple UDSs from launchd

Revision 1.48  2009/04/11 01:43:28  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.47  2009/01/11 03:20:06  mkrochma
<rdar://problem/5797526> Fixes from Igor Seleznev to get mdnsd working on Solaris

Revision 1.46  2008/11/03 23:09:15  cheshire
Don't need to include mDNSDebug.h as well as mDNSEmbeddedAPI.h

Revision 1.45  2008/10/03 18:25:17  cheshire
Instead of calling "m->MainCallback" function pointer directly, call mDNSCore routine "mDNS_ConfigChanged(m);"

Revision 1.44  2008/09/15 23:52:30  cheshire
<rdar://problem/6218902> mDNSResponder-177 fails to compile on Linux with .desc pseudo-op
Made __crashreporter_info__ symbol conditional, so we only use it for OS X build

Revision 1.43  2007/10/22 20:05:34  cheshire
Use mDNSPlatformSourceAddrForDest instead of FindSourceAddrForIP

Revision 1.42  2007/09/18 19:09:02  cheshire
<rdar://problem/5489549> mDNSResponderHelper (and other binaries) missing SCCS version strings

Revision 1.41  2007/09/04 17:02:25  cheshire
<rdar://problem/5458929> False positives in changed files list in nightly builds
Added MDNS_VERSIONSTR_NODTS option at the reqest of Rishi Srivatsavai (Sun)

Revision 1.40  2007/07/31 23:08:34  mcguire
<rdar://problem/5329542> BTMM: Make AutoTunnel mode work with multihoming

Revision 1.39  2007/03/21 00:30:44  cheshire
Remove obsolete mDNS_DeleteDNSServers() call

Revision 1.38  2007/02/14 01:58:19  cheshire
<rdar://problem/4995831> Don't delete Unix Domain Socket on exit if we didn't create it on startup

Revision 1.37  2007/02/07 19:32:00  cheshire
<rdar://problem/4980353> All mDNSResponder components should contain version strings in SCCS-compatible format

Revision 1.36  2007/02/06 19:06:48  cheshire
<rdar://problem/3956518> Need to go native with launchd

Revision 1.35  2007/01/05 08:30:52  cheshire
Trim excessive "$Log" checkin history from before 2006
(checkin history still available via "cvs log ..." of course)

Revision 1.34  2007/01/05 05:46:08  cheshire
Add mDNS *const m parameter to udsserver_handle_configchange()

Revision 1.33  2006/12/21 00:10:53  cheshire
Make mDNS_PlatformSupport PlatformStorage a static global instead of a stack variable

Revision 1.32  2006/11/03 22:28:50  cheshire
PosixDaemon needs to handle mStatus_ConfigChanged and mStatus_GrowCache messages

Revision 1.31  2006/08/14 23:24:46  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.30  2006/07/07 01:09:12  cheshire
<rdar://problem/4472013> Add Private DNS server functionality to dnsextd
Only use mallocL/freeL debugging routines when building mDNSResponder, not dnsextd

*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>

#include "mDNSEmbeddedAPI.h"
#include "mDNSPosix.h"
#include "mDNSUNP.h"		// For daemon()
#include "uds_daemon.h"
#include "PlatformCommon.h"

#define CONFIG_FILE "/etc/mdnsd.conf"
static domainname DynDNSZone;                // Default wide-area zone for service registration
static domainname DynDNSHostname;

#define RR_CACHE_SIZE 500
static CacheEntity gRRCache[RR_CACHE_SIZE];
static mDNS_PlatformSupport PlatformStorage;

mDNSlocal void mDNS_StatusCallback(mDNS *const m, mStatus result)
	{
	(void)m; // Unused
	if (result == mStatus_NoError)
		{
		// On successful registration of dot-local mDNS host name, daemon may want to check if
		// any name conflict and automatic renaming took place, and if so, record the newly negotiated
		// name in persistent storage for next time. It should also inform the user of the name change.
		// On Mac OS X we store the current dot-local mDNS host name in the SCPreferences store,
		// and notify the user with a CFUserNotification.
		}
	else if (result == mStatus_ConfigChanged)
		{
		udsserver_handle_configchange(m);
		}
	else if (result == mStatus_GrowCache)
		{
		// Allocate another chunk of cache storage
		CacheEntity *storage = malloc(sizeof(CacheEntity) * RR_CACHE_SIZE);
		if (storage) mDNS_GrowCache(m, storage, RR_CACHE_SIZE);
		}
	}

// %%% Reconfigure() probably belongs in the platform support layer (mDNSPosix.c), not the daemon cde
// -- all client layers running on top of mDNSPosix.c need to handle network configuration changes,
// not only the Unix Domain Socket Daemon

static void Reconfigure(mDNS *m)
	{
	mDNSAddr DynDNSIP;
	const mDNSAddr dummy = { mDNSAddrType_IPv4, { { { 1, 1, 1, 1 } } } };;
	mDNS_SetPrimaryInterfaceInfo(m, NULL, NULL, NULL);
	if (ParseDNSServers(m, uDNS_SERVERS_FILE) < 0)
		LogMsg("Unable to parse DNS server list. Unicast DNS-SD unavailable");
	ReadDDNSSettingsFromConfFile(m, CONFIG_FILE, &DynDNSHostname, &DynDNSZone, NULL);
	mDNSPlatformSourceAddrForDest(&DynDNSIP, &dummy);
	if (DynDNSHostname.c[0]) mDNS_AddDynDNSHostName(m, &DynDNSHostname, NULL, NULL);
	if (DynDNSIP.type)       mDNS_SetPrimaryInterfaceInfo(m, &DynDNSIP, NULL, NULL);
	mDNS_ConfigChanged(m);
	}

// Do appropriate things at startup with command line arguments. Calls exit() if unhappy.
mDNSlocal void ParseCmdLinArgs(int argc, char **argv)
	{
	if (argc > 1)
		{
		if (0 == strcmp(argv[1], "-debug")) mDNS_DebugMode = mDNStrue;
		else printf("Usage: %s [-debug]\n", argv[0]);
		}

	if (!mDNS_DebugMode)
		{
		int result = daemon(0, 0);
		if (result != 0) { LogMsg("Could not run as daemon - exiting"); exit(result); }
#if __APPLE__
		LogMsg("The POSIX mdnsd should only be used on OS X for testing - exiting");
		exit(-1);
#endif
		}
	}

mDNSlocal void DumpStateLog(mDNS *const m)
// Dump a little log of what we've been up to.
	{
	LogMsg("---- BEGIN STATE LOG ----");
	udsserver_info(m);
	LogMsg("----  END STATE LOG  ----");
	}

mDNSlocal mStatus MainLoop(mDNS *m) // Loop until we quit.
	{
	sigset_t	signals;
	mDNSBool	gotData = mDNSfalse;

	mDNSPosixListenForSignalInEventLoop(SIGINT);
	mDNSPosixListenForSignalInEventLoop(SIGTERM);
	mDNSPosixListenForSignalInEventLoop(SIGUSR1);
	mDNSPosixListenForSignalInEventLoop(SIGPIPE);
	mDNSPosixListenForSignalInEventLoop(SIGHUP) ;

	for (; ;)
		{
		// Work out how long we expect to sleep before the next scheduled task
		struct timeval	timeout;
		mDNSs32			ticks;

		// Only idle if we didn't find any data the last time around
		if (!gotData)
			{
			mDNSs32			nextTimerEvent = mDNS_Execute(m);
			nextTimerEvent = udsserver_idle(nextTimerEvent);
			ticks = nextTimerEvent - mDNS_TimeNow(m);
			if (ticks < 1) ticks = 1;
			}
		else	// otherwise call EventLoop again with 0 timemout
			ticks = 0;

		timeout.tv_sec = ticks / mDNSPlatformOneSecond;
		timeout.tv_usec = (ticks % mDNSPlatformOneSecond) * 1000000 / mDNSPlatformOneSecond;

		(void) mDNSPosixRunEventLoopOnce(m, &timeout, &signals, &gotData);

		if (sigismember(&signals, SIGHUP )) Reconfigure(m);
		if (sigismember(&signals, SIGUSR1)) DumpStateLog(m);
		// SIGPIPE happens when we try to write to a dead client; death should be detected soon in request_callback() and cleaned up.
		if (sigismember(&signals, SIGPIPE)) LogMsg("Received SIGPIPE - ignoring");
		if (sigismember(&signals, SIGINT) || sigismember(&signals, SIGTERM)) break;
		}
	return EINTR;
	}

int main(int argc, char **argv)
	{
	mStatus					err;

	ParseCmdLinArgs(argc, argv);

	LogMsg("%s starting", mDNSResponderVersionString);

	err = mDNS_Init(&mDNSStorage, &PlatformStorage, gRRCache, RR_CACHE_SIZE, mDNS_Init_AdvertiseLocalAddresses, 
					mDNS_StatusCallback, mDNS_Init_NoInitCallbackContext); 

	if (mStatus_NoError == err)
		err = udsserver_init(mDNSNULL, 0);
		
	Reconfigure(&mDNSStorage);

	// Now that we're finished with anything privileged, switch over to running as "nobody"
	if (mStatus_NoError == err)
		{
		const struct passwd *pw = getpwnam("nobody");
		if (pw != NULL)
			setuid(pw->pw_uid);
		else
			LogMsg("WARNING: mdnsd continuing as root because user \"nobody\" does not exist");
		}

	if (mStatus_NoError == err)
		err = MainLoop(&mDNSStorage);
 
	LogMsg("%s stopping", mDNSResponderVersionString);

	mDNS_Close(&mDNSStorage);

	if (udsserver_exit() < 0)
		LogMsg("ExitCallback: udsserver_exit failed");
 
 #if MDNS_DEBUGMSGS > 0
	printf("mDNSResponder exiting normally with %ld\n", err);
 #endif
 
	return err;
	}

//		uds_daemon support		////////////////////////////////////////////////////////////

mStatus udsSupportAddFDToEventLoop(int fd, udsEventCallback callback, void *context)
/* Support routine for uds_daemon.c */
	{
	// Depends on the fact that udsEventCallback == mDNSPosixEventCallback
	return mDNSPosixAddFDToEventLoop(fd, callback, context);
	}

mStatus udsSupportRemoveFDFromEventLoop(int fd)		// Note: This also CLOSES the file descriptor
	{
	mStatus err = mDNSPosixRemoveFDFromEventLoop(fd);
	close(fd);
	return err;
	}

mDNSexport void RecordUpdatedNiceLabel(mDNS *const m, mDNSs32 delay)
	{
	(void)m;
	(void)delay;
	// No-op, for now
	}

#if _BUILDING_XCODE_PROJECT_
// If the process crashes, then this string will be magically included in the automatically-generated crash log
const char *__crashreporter_info__ = mDNSResponderVersionString_SCCS + 5;
asm(".desc ___crashreporter_info__, 0x10");
#endif

// For convenience when using the "strings" command, this is the last thing in the file
#if mDNSResponderVersion > 1
mDNSexport const char mDNSResponderVersionString_SCCS[] = "@(#) mDNSResponder-" STRINGIFY(mDNSResponderVersion) " (" __DATE__ " " __TIME__ ")";
#elif MDNS_VERSIONSTR_NODTS
mDNSexport const char mDNSResponderVersionString_SCCS[] = "@(#) mDNSResponder (Engineering Build)";
#else
mDNSexport const char mDNSResponderVersionString_SCCS[] = "@(#) mDNSResponder (Engineering Build) (" __DATE__ " " __TIME__ ")";
#endif
