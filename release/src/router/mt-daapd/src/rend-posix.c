/*
 * $Id: rend-posix.c,v 1.1 2009-06-30 02:31:09 steven Exp $
 *
 * Do the zeroconf/mdns/rendezvous (tm) thing.  This is a hacked version
 * of Apple's Responder.c from the Rendezvous (tm) POSIX implementation
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
  File:       responder.c

  Contains:   Code to implement an mDNS responder on the Posix platform.

  Written by: Quinn

  Copyright:  Copyright (c) 2002 by Apple Computer, Inc., All Rights Reserved.

  Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
  ("Apple") in consideration of your agreement to the following terms, and your
  use, installation, modification or redistribution of this Apple software
  constitutes acceptance of these terms.  If you do not agree with these terms,
  please do not use, install, modify or redistribute this Apple software.

  In consideration of your agreement to abide by the following terms, and subject
  to these terms, Apple grants you a personal, non-exclusive license, under Apple's
  copyrights in this original Apple software (the "Apple Software"), to use,
  reproduce, modify and redistribute the Apple Software, with or without
  modifications, in source and/or binary forms; provided that if you redistribute
  the Apple Software in its entirety and without modifications, you must retain
  this notice and the following text and disclaimers in all such redistributions of
  the Apple Software.  Neither the name, trademarks, service marks or logos of
  Apple Computer, Inc. may be used to endorse or promote products derived from the
  Apple Software without specific prior written permission from Apple.  Except as
  expressly stated in this notice, no other rights or licenses, express or implied,
  are granted by Apple herein, including but not limited to any patent rights that
  may be infringed by your derivative works or by other works in which the Apple
  Software may be incorporated.

  The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
  WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
  WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
  COMBINATION WITH YOUR PRODUCTS.

  IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
  OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
  (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Change History (most recent first):

 $Log: rend-posix.c,v $
 Revision 1.1  2009-06-30 02:31:09  steven
 iTune Server

 Revision 1.23  2005/01/07 06:57:59  rpedde
 fix minor errno problem

 Revision 1.22  2004/12/09 05:05:54  rpedde
 Logging fixes

 Revision 1.21  2004/11/30 04:17:32  rpedde
 use pascal packed string to avoid invalid rdata error

  Revision 1.20  2004/11/30 04:04:17  rpedde
  database id txt record to store settings

  Revision 1.19  2004/11/13 07:14:26  rpedde
  modularize debugging statements

  Revision 1.18  2004/04/19 06:19:46  rpedde
  Starting to fix signal stuff

  Revision 1.17  2004/03/29 19:44:58  rpedde
  Move mdns stuff out of mdns subdir to help compile on older automakes

  Revision 1.16  2004/03/08 19:21:03  rpedde
  start of background scanning

  Revision 1.15  2004/03/02 01:35:31  rpedde
  fix domain

  Revision 1.14  2004/03/02 00:03:37  rpedde
  Merge new rendezvous code

  Revision 1.13  2004/03/01 16:29:42  rpedde
  Fix logging

  Revision 1.12  2004/02/25 16:13:37  rpedde
  More -Wall cleanups

  Revision 1.11  2004/02/09 18:33:59  rpedde
  Pretty up

  Revision 1.10  2004/01/20 04:41:20  rpedde
  merge new-rend-branch

  Revision 1.9.2.1  2004/01/16 20:51:01  rpedde
  Convert rend-posix to message-based system

  Revision 1.9  2004/01/04 05:02:23  rpedde
  fix segfault on dropping privs

  Revision 1.8  2003/12/29 23:39:18  ron
  add priv dropping

  Revision 1.7  2003/12/29 20:41:08  ron
  Make sure all files have GPL notice

  Revision 1.6  2003/11/26 06:12:53  ron
  Exclude from memory checks

  Revision 1.5  2003/11/23 18:13:15  ron
  Exit rather than returning... shouldn't make a difference, but does.  ?

  Revision 1.4  2003/11/20 21:58:22  ron
  More diag logging, move WS_PRIVATE into the WS_CONNINFO

  Revision 1.3  2003/11/17 16:40:09  ron
  add support for named db

  Revision 1.2  2003/11/14 04:54:55  ron
  Use port 53

  Revision 1.1  2003/10/30 22:41:56  ron
  Initial checkin

  Revision 1.3  2002/09/21 20:44:53  zarzycki
  Added APSL info

  Revision 1.2  2002/09/19 04:20:44  cheshire
  Remove high-ascii characters that confuse some systems

  Revision 1.1  2002/09/17 06:24:35  cheshire
  First checkin

*/

#include "mDNSClientAPI.h"// Defines the interface to the client layer above
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform

#include <assert.h>
#include <stdio.h>			// For printf()
#include <stdlib.h>			// For exit() etc.
#include <string.h>			// For strlen() etc.
#include <unistd.h>			// For select()
#include <errno.h>			// For errno, EINTR
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>

#define __IN_ERR__
#include "daapd.h"
#include "err.h"
#include "rend.h"
#include "rend-unix.h"


static mDNS mDNSStorage;       // mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage;  // Stores this platform's globals


static volatile mDNSBool gStopNow;

/* modified signal handling code - rep 21 Oct 2k3 */

// o SIGINT  causes an orderly shutdown of the program.
// o SIGQUIT causes a somewhat orderly shutdown (direct but dangerous)
//
// There are fatal race conditions in our signal handling, but there's not much 
// we can do about them while remaining within the Posix space.  Specifically, 
// if a signal arrives after we test the globals its sets but before we call 
// select, the signal will be dropped.  The user will have to send the signal 
// again.  Unfortunately, Posix does not have a "sigselect" to atomically 
// modify the signal mask and start a select.

static void HandleSigInt(int sigraised)
// A handler for SIGINT that causes us to break out of the 
// main event loop when the user types ^C.  This has the 
// effect of quitting the program.
{
    assert(sigraised == SIGINT);

    DPRINTF(E_INF,L_REND,"SIGINT\n");
    gStopNow = mDNStrue;
}

static void HandleSigQuit(int sigraised)
// If we get a SIGQUIT the user is desperate and we 
// just call mDNS_Close directly.  This is definitely 
// not safe (because it could reenter mDNS), but 
// we presume that the user has already tried the safe 
// alternatives.
{
    assert(sigraised == SIGQUIT);

    DPRINTF(E_INF,L_REND,"SIGQUIT\n");

    mDNS_Close(&mDNSStorage);
    exit(0);
}




/* get rid of pidfile handling - rep - 21 Oct 2k3 */
static const char kDefaultServiceType[] = "_http._tcp.";
enum {
    kDefaultPortNumber = 80
};

typedef struct PosixService PosixService;

struct PosixService {
    ServiceRecordSet coreServ;
    PosixService *next;
    int serviceID;
};

static PosixService *gServiceList = NULL;

static void RegistrationCallback(mDNS *const m, ServiceRecordSet *const thisRegistration, mStatus status)
// mDNS core calls this routine to tell us about the status of 
// our registration.  The appropriate action to take depends 
// entirely on the value of status.
{
    switch (status) {

    case mStatus_NoError:      
	DPRINTF(E_DBG,L_REND,"Callback: Name Registered\n");
	// Do nothing; our name was successfully registered.  We may 
	// get more call backs in the future.
	break;

    case mStatus_NameConflict: 
	DPRINTF(E_WARN,L_REND,"Callback: Name Conflict\n");

	// In the event of a conflict, this sample RegistrationCallback 
	// just calls mDNS_RenameAndReregisterService to automatically 
	// pick a new unique name for the service. For a device such as a 
	// printer, this may be appropriate.  For a device with a user 
	// interface, and a screen, and a keyboard, the appropriate response 
	// may be to prompt the user and ask them to choose a new name for 
	// the service.
	//
	// Also, what do we do if mDNS_RenameAndReregisterService returns an 
	// error.  Right now I have no place to send that error to.
            
	status = mDNS_RenameAndReregisterService(m, thisRegistration, mDNSNULL);
	assert(status == mStatus_NoError);
	break;

    case mStatus_MemFree:      
	DPRINTF(E_WARN,L_REND,"Callback: Memory Free\n");
            
	// When debugging is enabled, make sure that thisRegistration 
	// is not on our gServiceList.
            
#if defined(DEBUG)
	{
	    PosixService *cursor;
                    
	    cursor = gServiceList;
	    while (cursor != NULL) {
		assert(&cursor->coreServ != thisRegistration);
		cursor = cursor->next;
	    }
	}
#endif
	free(thisRegistration);
	break;

    default:                   
	DPRINTF(E_WARN,L_REND,"Callback: Unknown Status %d\n",status); 
	break;
    }
}

static int gServiceID = 0;

static mStatus RegisterOneService(const char *  richTextHostName, 
                                  const char *  serviceType, 
				  const char *  serviceDomain,
                                  const mDNSu8  text[],
                                  mDNSu16       textLen,
                                  long          portNumber)
{
    mStatus             status;
    PosixService *      thisServ;
    mDNSOpaque16        port;
    domainlabel         name;
    domainname          type;
    domainname          domain;
    
    status = mStatus_NoError;
    thisServ = (PosixService *) malloc(sizeof(*thisServ));
    if (thisServ == NULL) {
        status = mStatus_NoMemoryErr;
    }
    if (status == mStatus_NoError) {
        MakeDomainLabelFromLiteralString(&name,  richTextHostName);
        MakeDomainNameFromDNSNameString(&type, serviceType);
        MakeDomainNameFromDNSNameString(&domain, serviceDomain);

        port.b[0] = (portNumber >> 8) & 0x0FF;
        port.b[1] = (portNumber >> 0) & 0x0FF;;
        status = mDNS_RegisterService(&mDNSStorage, &thisServ->coreServ,
				      &name, &type, &domain,
				      NULL,
				      port, 
				      text, textLen,
				      NULL, 0,
				      mDNSInterface_Any,
				      RegistrationCallback, thisServ);
    }
    if (status == mStatus_NoError) {
        thisServ->serviceID = gServiceID;
        gServiceID += 1;

        thisServ->next = gServiceList;
        gServiceList = thisServ;

	DPRINTF(E_DBG,L_REND,
		"Registered service %d, name '%s', type '%s', domain '%s', port %ld\n", 
		thisServ->serviceID, 
		richTextHostName,
		serviceType,
		serviceDomain,
		portNumber);
    } else {
        if (thisServ != NULL) {
            free(thisServ);
        }
    }
    return status;
}

static void DeregisterOurServices(void)
{
    PosixService *thisServ;
    int thisServID;
    
    while (gServiceList != NULL) {
        thisServ = gServiceList;
        gServiceList = thisServ->next;

        thisServID = thisServ->serviceID;
        
        mDNS_DeregisterService(&mDNSStorage, &thisServ->coreServ);

	DPRINTF(E_DBG,L_REND,"Deregistered service %d\n",
		thisServ->serviceID);
    }
}

/*
 * rend_callback
 *
 * This is borrowed from the OSX rend client
 */
void rend_callback(void) {
    REND_MESSAGE msg;
    int result;
    int err;

    DPRINTF(E_DBG,L_REND,"Processing rendezvous message\n");

    /* here, we've seen the message, now we have to process it */

    if((result=rend_read_message(&msg)) != sizeof(msg)) {
	err=errno;
	DPRINTF(E_FATAL,L_REND,"Rendezvous socket closed (daap server crashed?) Aborting.\n");
	gStopNow=mDNStrue;
	return;
    }

    switch(msg.cmd) {
    case REND_MSG_TYPE_REGISTER:
	DPRINTF(E_DBG,L_REND,"Registering %s.%s (%d)\n",msg.name,msg.type,msg.port);
	RegisterOneService(msg.name,msg.type,"local.","\011txtvers=1\034Database ID=beddab1edeadbea7",39,
			   msg.port);
	rend_send_response(0); /* success */
	break;
    case REND_MSG_TYPE_UNREGISTER:
	rend_send_response(1); /* error */
	break;
    case REND_MSG_TYPE_STOP:
	DPRINTF(E_INF,L_REND,"Stopping mDNS\n");
	gStopNow = mDNStrue;
	rend_send_response(0);
	break;
    case REND_MSG_TYPE_STATUS:
	rend_send_response(1);
	break;
    default:
	break;
    }
}



int rend_private_init(char *user) {
    mStatus status;
    mDNSBool result;

    status = mDNS_Init(&mDNSStorage, &PlatformStorage,
		       mDNS_Init_NoCache, mDNS_Init_ZeroCacheSize,
		       mDNS_Init_AdvertiseLocalAddresses,
		       mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
    
    if (status != mStatus_NoError) {
	DPRINTF(E_FATAL,L_REND,"mDNS Error %d\n",status);
	return(-1);
    }

    if(drop_privs(user))
	return -1;

    signal(SIGINT,  HandleSigInt);      // SIGINT is what you get for a Ctrl-C
    signal(SIGQUIT, HandleSigQuit);     // SIGQUIT is what you get for a Ctrl-\ (indeed)
    signal(SIGHUP,  SIG_IGN);           // SIGHUP might happen from a request to reload the daap server

    while (!gStopNow) {
	int nfds = 1;
	fd_set readfds;
	struct timeval timeout;
	int result;
	
	// 1. Set up the fd_set as usual here.
	// This example client has no file descriptors of its own,
	// but a real application would call FD_SET to add them to the set here
	FD_ZERO(&readfds);
	FD_SET(rend_pipe_to[RD_SIDE],&readfds);
	
	// 2. Set up the timeout.
	// This example client has no other work it needs to be doing,
	// so we set an effectively infinite timeout
	timeout.tv_sec = 0x3FFFFFFF;
	timeout.tv_usec = 0;
	
	// 3. Give the mDNSPosix layer a chance to add its information to the fd_set and timeout
	mDNSPosixGetFDSet(&mDNSStorage, &nfds, &readfds, &timeout);
	
	// 4. Call select as normal
	DPRINTF(E_DBG,L_REND,"select(%d, %d.%06d)\n", nfds, 
		timeout.tv_sec, timeout.tv_usec);
	
	result = select(nfds, &readfds, NULL, NULL, &timeout);
	
	if (result < 0) {
	    if (errno != EINTR) gStopNow = mDNStrue;
	    DPRINTF(E_WARN,L_REND,"select() returned %d errno %d\n", result, errno);
	} else {
	    // 5. Call mDNSPosixProcessFDSet to let the mDNSPosix layer do its work
	    mDNSPosixProcessFDSet(&mDNSStorage, &readfds);
	    
	    // 6. This example client has no other work it needs to be doing,
	    // but a real client would do its work here
	    // ... (do work) ...
	    if(FD_ISSET(rend_pipe_to[RD_SIDE],&readfds)) {
		rend_callback();
	    }
	}
    }
    
    DPRINTF(E_DBG,L_REND,"Exiting\n");
    
    DeregisterOurServices();
    mDNS_Close(&mDNSStorage);
    
    if (status == mStatus_NoError) {
        result = 0;
    } else {
        result = 2;
    }
    DPRINTF(E_DBG,L_REND, "Finished with status %ld, result %d\n", 
	    status, result);

    exit(result);
}


