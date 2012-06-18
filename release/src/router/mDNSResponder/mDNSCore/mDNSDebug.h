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

$Log: mDNSDebug.h,v $
Revision 1.51  2009/06/25 23:36:59  cheshire
To facilitate testing, added command-line switch "-OfferSleepProxyService"
to re-enable the previously-supported mode of operation where we offer
sleep proxy service on desktop Macs that are set to never sleep.

Revision 1.50  2009/05/19 23:34:06  cheshire
Updated comment to show correct metric of 80 for a low-priority sleep proxy

Revision 1.49  2009/04/24 23:32:28  cheshire
To facilitate testing, put back code to be a sleep proxy when set to never sleep, compiled out by compile-time switch

Revision 1.48  2009/04/11 01:43:27  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.47  2009/04/11 00:19:41  jessic2
<rdar://problem/4426780> Daemon: Should be able to turn on LogOperation dynamically

Revision 1.46  2009/02/13 06:36:50  cheshire
Update LogSPS definition

Revision 1.45  2009/02/13 06:03:12  cheshire
Added LogInfo for informational message logging

Revision 1.44  2009/02/12 20:57:25  cheshire
Renamed 'LogAllOperation' switch to 'LogClientOperations'; added new 'LogSleepProxyActions' switch

Revision 1.43  2008/12/10 02:27:14  cheshire
Commented out definitions of LogClientOperations, LogTimeStamps, ForceAlerts and MACOSX_MDNS_MALLOC_DEBUGGING
to allow overriding values to be easily defined in a Makefile or similar build environment

Revision 1.42  2008/11/02 21:22:05  cheshire
Changed mallocL size parameter back to "unsigned int"

Revision 1.41  2008/11/02 21:14:58  cheshire
Fixes to make mallocL/freeL debugging checks work on 64-bit

Revision 1.40  2008/10/24 20:53:37  cheshire
For now, define USE_SEPARATE_UDNS_SERVICE_LIST, so that we use the old service list code for this submission

Revision 1.39  2008/02/26 21:17:11  cheshire
Grouped all user settings together near the start of the file; added LogTimeStamps option

Revision 1.38  2007/12/13 20:27:07  cheshire
Remove unused VerifySameNameAssumptions symbol

Revision 1.37  2007/12/01 00:33:17  cheshire
Fixes from Bob Bradley for building on EFI

Revision 1.36  2007/10/01 19:06:19  cheshire
Defined symbolic constant MDNS_LOG_INITIAL_LEVEL to set the logging level we start out at

Revision 1.35  2007/07/27 20:19:56  cheshire
For now, comment out unused log levels MDNS_LOG_ERROR, MDNS_LOG_WARN, MDNS_LOG_INFO, MDNS_LOG_DEBUG

Revision 1.34  2007/07/24 17:23:33  cheshire
<rdar://problem/5357133> Add list validation checks for debugging

Revision 1.33  2007/06/15 21:54:50  cheshire
<rdar://problem/4883206> Add packet logging to help debugging private browsing over TLS

Revision 1.32  2007/05/25 16:03:03  cheshire
Remove unused LogMalloc

Revision 1.31  2007/04/06 19:50:05  cheshire
Add ProgramName declaration

Revision 1.30  2007/03/24 01:22:44  cheshire
Add validator for uDNS data structures

Revision 1.29  2006/08/14 23:24:23  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.28  2006/07/07 01:09:09  cheshire
<rdar://problem/4472013> Add Private DNS server functionality to dnsextd
Only use mallocL/freeL debugging routines when building mDNSResponder, not dnsextd

Revision 1.27  2006/06/29 07:42:14  cheshire
<rdar://problem/3922989> Performance: Remove unnecessary SameDomainName() checks

Revision 1.26  2005/07/04 22:40:26  cheshire
Additional debugging code to help catch memory corruption

Revision 1.25  2004/12/14 21:34:16  cheshire
Add "#define ANSWER_REMOTE_HOSTNAME_QUERIES 0" and comment

Revision 1.24  2004/09/16 01:58:21  cheshire
Fix compiler warnings

Revision 1.23  2004/05/18 23:51:25  cheshire
Tidy up all checkin comments to use consistent "<rdar://problem/xxxxxxx>" format for bug numbers

Revision 1.22  2004/04/22 04:27:42  cheshire
Spacing tidyup

Revision 1.21  2004/04/14 23:21:41  ksekar
Removed accidental checkin of MALLOC_DEBUGING flag in 1.20

Revision 1.20  2004/04/14 23:09:28  ksekar
Support for TSIG signed dynamic updates.

Revision 1.19  2004/03/15 18:57:59  cheshire
Undo last checkin that accidentally made verbose debugging the default for all targets

Revision 1.18  2004/03/13 01:57:33  ksekar
<rdar://problem/3192546>: DynDNS: Dynamic update of service records

Revision 1.17  2004/01/28 21:14:23  cheshire
Reconcile debug_mode and gDebugLogging into a single flag (mDNS_DebugMode)

Revision 1.16  2003/12/09 01:30:06  rpantos
Fix usage of ARGS... macros to build properly on Windows.

Revision 1.15  2003/12/08 20:55:26  rpantos
Move some definitions here from mDNSMacOSX.h.

Revision 1.14  2003/08/12 19:56:24  cheshire
Update to APSL 2.0

Revision 1.13  2003/07/02 21:19:46  cheshire
<rdar://problem/3313413> Update copyright notices, etc., in source code comments

Revision 1.12  2003/05/26 03:01:27  cheshire
<rdar://problem/3268904> sprintf/vsprintf-style functions are unsafe; use snprintf/vsnprintf instead

Revision 1.11  2003/05/21 17:48:10  cheshire
Add macro to enable GCC's printf format string checking

Revision 1.10  2003/04/26 02:32:57  cheshire
Add extern void LogMsg(const char *format, ...);

Revision 1.9  2002/09/21 20:44:49  zarzycki
Added APSL info

Revision 1.8  2002/09/19 04:20:43  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.7  2002/09/16 18:41:42  cheshire
Merge in license terms from Quinn's copy, in preparation for Darwin release

*/

#ifndef __mDNSDebug_h
#define __mDNSDebug_h

// Set MDNS_DEBUGMSGS to 0 to optimize debugf() calls out of the compiled code
// Set MDNS_DEBUGMSGS to 1 to generate normal debugging messages
// Set MDNS_DEBUGMSGS to 2 to generate verbose debugging messages
// MDNS_DEBUGMSGS is normally set in the project options (or makefile) but can also be set here if desired
// (If you edit the file here to turn on MDNS_DEBUGMSGS while you're debugging some code, be careful
// not to accidentally check-in that change by mistake when you check in your other changes.)

//#undef MDNS_DEBUGMSGS
//#define MDNS_DEBUGMSGS 2

// Set MDNS_CHECK_PRINTF_STYLE_FUNCTIONS to 1 to enable extra GCC compiler warnings
// Note: You don't normally want to do this, because it generates a bunch of
// spurious warnings for the following custom extensions implemented by mDNS_vsnprintf:
//    warning: `#' flag used with `%s' printf format    (for %#s              -- pascal string format)
//    warning: repeated `#' flag in format              (for %##s             -- DNS name string format)
//    warning: double format, pointer arg (arg 2)       (for %.4a, %.16a, %#a -- IP address formats)
#define MDNS_CHECK_PRINTF_STYLE_FUNCTIONS 0

typedef enum
	{
	MDNS_LOG_MSG,
	MDNS_LOG_OPERATION,
	MDNS_LOG_SPS,
	MDNS_LOG_INFO,
	MDNS_LOG_DEBUG,
	} mDNSLogLevel_t;

// Set this symbol to 1 to answer remote queries for our Address, reverse mapping PTR, and HINFO records
#define ANSWER_REMOTE_HOSTNAME_QUERIES 0

// Set this symbol to 1 to do extra debug checks on malloc() and free()
// Set this symbol to 2 to write a log message for every malloc() and free()
//#define MACOSX_MDNS_MALLOC_DEBUGGING 1

//#define ForceAlerts 1
//#define LogTimeStamps 1

#define USE_SEPARATE_UDNS_SERVICE_LIST 1

// Developer-settings section ends here

#if MDNS_CHECK_PRINTF_STYLE_FUNCTIONS
#define IS_A_PRINTF_STYLE_FUNCTION(F,A) __attribute__ ((format(printf,F,A)))
#else
#define IS_A_PRINTF_STYLE_FUNCTION(F,A)
#endif

#ifdef __cplusplus
	extern "C" {
#endif

// Variable argument macro support. Use ANSI C99 __VA_ARGS__ where possible. Otherwise, use the next best thing.

#if (defined(__GNUC__))
	#if ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2)))
		#define MDNS_C99_VA_ARGS        1
		#define MDNS_GNU_VA_ARGS        0
	#else
		#define MDNS_C99_VA_ARGS        0
		#define MDNS_GNU_VA_ARGS        1
	#endif
	#define MDNS_HAS_VA_ARG_MACROS      1
#elif (_MSC_VER >= 1400) // Visual Studio 2005 and later
	#define MDNS_C99_VA_ARGS            1
	#define MDNS_GNU_VA_ARGS            0
	#define MDNS_HAS_VA_ARG_MACROS      1
#elif (defined(__MWERKS__))
	#define MDNS_C99_VA_ARGS            1
	#define MDNS_GNU_VA_ARGS            0
	#define MDNS_HAS_VA_ARG_MACROS      1
#else
	#define MDNS_C99_VA_ARGS            0
	#define MDNS_GNU_VA_ARGS            0
	#define MDNS_HAS_VA_ARG_MACROS      0
#endif

#if (MDNS_HAS_VA_ARG_MACROS)
	#if (MDNS_C99_VA_ARGS)
		#define debug_noop( ... ) ((void)0)
		#define LogMsg( ... ) LogMsgWithLevel(MDNS_LOG_MSG, __VA_ARGS__)
		#define LogOperation( ... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_OPERATION, __VA_ARGS__); } while (0)
		#define LogSPS( ... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_SPS, __VA_ARGS__); } while (0)
		#define LogInfo( ... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_INFO, __VA_ARGS__); } while (0)
	#elif (MDNS_GNU_VA_ARGS)
		#define	debug_noop( ARGS... ) ((void)0)
		#define	LogMsg( ARGS... ) LogMsgWithLevel(MDNS_LOG_MSG, ARGS)
		#define	LogOperation( ARGS... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_OPERATION, ARGS); } while (0)
		#define	LogSPS( ARGS... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_SPS, ARGS); } while (0)
		#define	LogInfo( ARGS... ) do { if (mDNS_LoggingEnabled) LogMsgWithLevel(MDNS_LOG_INFO, ARGS); } while (0)
	#else
		#error Unknown variadic macros
	#endif
#else
	// If your platform does not support variadic macros, you need to define the following variadic functions.
	// See mDNSShared/mDNSDebug.c for sample implementation
	extern void debug_noop(const char *format, ...);
	#define LogMsg LogMsg_
	#define LogOperation mDNS_LoggingEnabled == 0 ? ((void)0) : LogOperation_
	#define LogSPS mDNS_LoggingEnabled == 0 ? ((void)0) : LogSPS_
	#define LogInfo mDNS_LoggingEnabled == 0 ? ((void)0) : LogInfo_
	extern void LogMsg_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
	extern void LogOperation_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
	extern void LogSPS_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
	extern void LogInfo_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#endif

#if MDNS_DEBUGMSGS
#define debugf debugf_
extern void debugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else
#define debugf debug_noop
#endif

#if MDNS_DEBUGMSGS > 1
#define verbosedebugf verbosedebugf_
extern void verbosedebugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else
#define verbosedebugf debug_noop
#endif

extern int	      mDNS_LoggingEnabled;
extern int	      mDNS_PacketLoggingEnabled;
extern int        mDNS_DebugMode;	// If non-zero, LogMsg() writes to stderr instead of syslog
extern const char ProgramName[];

extern void LogMsgWithLevel(mDNSLogLevel_t logLevel, const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(2,3);
#define LogMsgNoIdent LogMsg

#if APPLE_OSX_mDNSResponder && MACOSX_MDNS_MALLOC_DEBUGGING >= 1
extern void *mallocL(char *msg, unsigned int size);
extern void freeL(char *msg, void *x);
extern void LogMemCorruption(const char *format, ...);
extern void uds_validatelists(void);
extern void udns_validatelists(void *const v);
#else
#define mallocL(X,Y) malloc(Y)
#define freeL(X,Y) free(Y)
#endif

#ifdef __cplusplus
	}
#endif

#endif
