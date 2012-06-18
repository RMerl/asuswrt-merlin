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
Revision 1.26.2.1  2006/08/29 06:24:22  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

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
#if MDNS_CHECK_PRINTF_STYLE_FUNCTIONS
#define IS_A_PRINTF_STYLE_FUNCTION(F,A) __attribute__ ((format(printf,F,A)))
#else
#define IS_A_PRINTF_STYLE_FUNCTION(F,A)
#endif

#ifdef	__cplusplus
	extern "C" {
#endif

#if MDNS_DEBUGMSGS
#define debugf debugf_
extern void debugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else // If debug breaks are off, use a preprocessor trick to optimize those calls out of the code
	#if (defined(__GNUC__))
		#define	debugf( ARGS... ) ((void)0)
	#elif (defined(__MWERKS__))
		#define	debugf( ... )
	#else
		#define debugf 1 ? ((void)0) : (void)
	#endif
#endif

#if MDNS_DEBUGMSGS > 1
#define verbosedebugf verbosedebugf_
extern void verbosedebugf_(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
#else
	#if (defined(__GNUC__))
		#define	verbosedebugf( ARGS... ) ((void)0)
	#elif (defined(__MWERKS__))
		#define	verbosedebugf( ... )
	#else
		#define verbosedebugf 1 ? ((void)0) : (void)
	#endif
#endif

// LogMsg is used even in shipping code, to write truly serious error messages to syslog (or equivalent)
extern int	mDNS_DebugMode;	// If non-zero, LogMsg() writes to stderr instead of syslog
extern void LogMsg(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);
extern void LogMsgIdent(const char *ident, const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(2,3);
extern void LogMsgNoIdent(const char *format, ...) IS_A_PRINTF_STYLE_FUNCTION(1,2);

// Set this symbol to 1 to answer remote queries for our Address, reverse mapping PTR, and HINFO records
#define ANSWER_REMOTE_HOSTNAME_QUERIES 0

// Set this symbol to 1 to do extra debug checks on malloc() and free()
// Set this symbol to 2 to write a log message for every malloc() and free()
#define MACOSX_MDNS_MALLOC_DEBUGGING 0

#if MACOSX_MDNS_MALLOC_DEBUGGING >= 1
extern void *mallocL(char *msg, unsigned int size);
extern void freeL(char *msg, void *x);
extern void LogMemCorruption(const char *format, ...);
extern void uds_validatelists(void);
#else
#define mallocL(X,Y) malloc(Y)
#define freeL(X,Y) free(Y)
#endif

#if MACOSX_MDNS_MALLOC_DEBUGGING >= 2
#define LogMalloc LogMsg
#else
	#if (defined( __GNUC__ ))
		#define	LogMalloc(ARGS...) ((void)0)
	#elif (defined( __MWERKS__ ))
		#define	LogMalloc( ... )
	#else
		#define LogMalloc 1 ? ((void)0) : (void)
	#endif
#endif

#define LogAllOperations 0

#if LogAllOperations
#define LogOperation LogMsg
#else
#define	LogOperation debugf
#endif

#define ForceAlerts 0

#ifdef	__cplusplus
	}
#endif

#endif
