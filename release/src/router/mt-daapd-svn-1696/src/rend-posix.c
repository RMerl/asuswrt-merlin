/* -*- Mode: C; tab-width: 4 -*-
 * $Id: rend-posix.c 1633 2007-08-12 06:30:00Z rpedde $
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
 *
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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

$Log: Responder.c,v $
Revision 1.33  2007/04/16 20:49:39  cheshire
Fix compile errors for mDNSPosix build

Revision 1.32  2006/08/14 23:24:46  cheshire
Re-licensed mDNSResponder daemon source code under Apache License, Version 2.0

Revision 1.31  2006/06/12 18:22:42  cheshire
<4580067> mDNSResponder building warnings under Red Hat 64-bit (LP64) Linux

Revision 1.30  2005/10/26 22:21:16  cheshire
<4149841> Potential buffer overflow in mDNSResponderPosix

Revision 1.29  2005/03/04 21:35:33  cheshire
<4037201> Services.txt file not parsed properly when it contains more than one service

Revision 1.28  2005/01/11 01:55:26  ksekar
Fix compile errors in Posix debug build

Revision 1.27  2004/12/01 04:28:43  cheshire
<3872803> Darwin patches for Solaris and Suse
Use version of daemon() provided in mDNSUNP.c instead of local copy

Revision 1.26  2004/11/30 22:37:01  cheshire
Update copyright dates and add "Mode: C; tab-width: 4" headers

Revision 1.25  2004/11/11 02:00:51  cheshire
Minor fixes to getopt, error message

Revision 1.24  2004/11/09 19:32:10  rpantos
Suggestion from Ademar de Souza Reis Jr. to allow comments in services file

Revision 1.23  2004/09/17 01:08:54  cheshire
Renamed mDNSClientAPI.h to mDNSEmbeddedAPI.h
  The name "mDNSClientAPI.h" is misleading to new developers looking at this code. The interfaces
  declared in that file are ONLY appropriate to single-address-space embedded applications.
  For clients on general-purpose computers, the interfaces defined in dns_sd.h should be used.

Revision 1.22  2004/09/16 01:58:22  cheshire
Fix compiler warnings

Revision 1.21  2004/06/15 03:48:07  cheshire
Update mDNSResponderPosix to take multiple name=val arguments in a sane way

Revision 1.20  2004/05/18 23:51:26  cheshire
Tidy up all checkin comments to use consistent "<xxxxxxx>" format for bug numbers

Revision 1.19  2004/03/12 08:03:14  cheshire
Update comments

Revision 1.18  2004/01/25 00:00:55  cheshire
Change to use mDNSOpaque16fromIntVal() instead of shifting and masking

Revision 1.17  2003/12/11 19:11:55  cheshire
Fix compiler warning

Revision 1.16  2003/08/14 02:19:55  cheshire
<3375491> Split generic ResourceRecord type into two separate types: AuthRecord and CacheRecord

Revision 1.15  2003/08/12 19:56:26  cheshire
Update to APSL 2.0

Revision 1.14  2003/08/06 18:20:51  cheshire
Makefile cleanup

Revision 1.13  2003/07/23 00:00:04  cheshire
Add comments

Revision 1.12  2003/07/15 01:55:16  cheshire
<3315777> Need to implement service registration with subtypes

Revision 1.11  2003/07/14 18:11:54  cheshire
Fix stricter compiler warnings

Revision 1.10  2003/07/10 20:27:31  cheshire
<3318717> mDNSResponder Posix version is missing a 'b' in the getopt option string

Revision 1.9  2003/07/02 21:19:59  cheshire
<3313413> Update copyright notices, etc., in source code comments

Revision 1.8  2003/06/18 05:48:41  cheshire
Fix warnings

Revision 1.7  2003/05/06 00:00:50  cheshire
<3248914> Rationalize naming of domainname manipulation functions

Revision 1.6  2003/03/08 00:35:56  cheshire
Switched to using new "mDNS_Execute" model (see "mDNSCore/Implementer Notes.txt")

Revision 1.5  2003/02/20 06:48:36  cheshire
<3169535> Xserve RAID needs to do interface-specific registrations
Reviewed by: Josh Graessley, Bob Bradley

Revision 1.4  2003/01/28 03:07:46  cheshire
Add extra parameter to mDNS_RenameAndReregisterService(),
and add support for specifying a domain other than dot-local.

Revision 1.3  2002/09/21 20:44:53  zarzycki
Added APSL info

Revision 1.2  2002/09/19 04:20:44  cheshire
Remove high-ascii characters that confuse some systems

Revision 1.1  2002/09/17 06:24:35  cheshire
First checkin

*/

#include "mDNSEmbeddedAPI.h"// Defines the interface to the client layer above
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform

#include <assert.h>
#include <stdio.h>			// For printf()
#include <stdlib.h>			// For exit() etc.
#include <string.h>			// For strlen() etc.
#include <unistd.h>			// For select()
#include <errno.h>			// For errno, EINTR
#include <signal.h>
#include <fcntl.h>

#include "daapd.h"
#include "err.h"
#include "os-unix.h"
#include "rend.h"
#include "rend-unix.h"

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Globals
#endif

static mDNS mDNSStorage;       // mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage;  // Stores this platform's globals

mDNSexport const char ProgramName[] = "mDNSResponderPosix";


static volatile mDNSBool gStopNow;

// We support 4 signals. (2, now -- rp)
//
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
    
    if (gMDNSPlatformPosixVerboseLevel > 0) {
        fprintf(stderr, "\nSIGINT\n");
    }
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

    if (gMDNSPlatformPosixVerboseLevel > 0) {
        fprintf(stderr, "\nSIGQUIT\n");
    }
    mDNS_Close(&mDNSStorage);
    exit(0);
}


static const char kDefaultServiceType[] = "_http._tcp.";
static const char kDefaultServiceDomain[] = "local.";
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
            DPRINTF(E_DBG,L_REND,"Callback: %##s Name Registered\n",   thisRegistration->RR_SRV.resrec.name->c); 
            // Do nothing; our name was successfully registered.  We may 
            // get more call backs in the future.
            break;

        case mStatus_NameConflict: 
            DPRINTF(E_DBG,L_REND,"Callback: %##s Name Conflict\n",     thisRegistration->RR_SRV.resrec.name->c); 

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
            DPRINTF(E_DBG,L_REND,"Callback: %##s Memory Free\n",       thisRegistration->RR_SRV.resrec.name->c); 
            
            // When debugging is enabled, make sure that thisRegistration 
            // is not on our gServiceList.
            
            #if !defined(NDEBUG)
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
            DPRINTF(E_DBG,L_REND,"Callback: %##s Unknown Status %ld\n", thisRegistration->RR_SRV.resrec.name->c, status); 
            break;
    }
}

static int gServiceID = 0;

static mStatus RegisterOneService(const char *  richTextName, 
                                  const char *  serviceType, 
                                  const char *  serviceDomain, 
                                  const mDNSu8  text[],
                                  mDNSu16       textLen,
                                  long          portNumber,
                                  mDNSInterfaceID id)
{
    mStatus             status;
    PosixService *      thisServ;
    domainlabel         name;
    domainname          type;
    domainname          domain;
    
    status = mStatus_NoError;
    thisServ = (PosixService *) malloc(sizeof(*thisServ));
    if (thisServ == NULL) {
        status = mStatus_NoMemoryErr;
    }
    if (status == mStatus_NoError) {
        MakeDomainLabelFromLiteralString(&name,  richTextName);
        MakeDomainNameFromDNSNameString(&type, serviceType);
        MakeDomainNameFromDNSNameString(&domain, serviceDomain);
        
        status = mDNS_RegisterService(&mDNSStorage, &thisServ->coreServ,
                &name, &type, &domain,				// Name, type, domain
                NULL, mDNSOpaque16fromIntVal(portNumber),
                text, textLen,						// TXT data, length
                NULL, 0,							// Subtypes
                id,					// Interface ID
                RegistrationCallback, thisServ);	// Callback and context
    }
    if (status == mStatus_NoError) {
        thisServ->serviceID = gServiceID;
        gServiceID += 1;

        thisServ->next = gServiceList;
        gServiceList = thisServ;
        DPRINTF(E_DBG,L_REND, 
            "Registered service %d, name '%s', type '%s', port %ld\n", 
            thisServ->serviceID, 
            richTextName,
            serviceType,
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

        DPRINTF(E_DBG,L_REND, 
            "Deregistered service %d\n",
            thisServ->serviceID);
    }
}

mDNSInterfaceID rend_get_interface_id(char *iface) {
    PosixNetworkInterface *pni;

    if(!iface)
        return mDNSInterface_Any;

    if(!strlen(iface))
        return mDNSInterface_Any;

    DPRINTF(E_LOG,L_REND,"Searching for interface %s\n",iface);

    pni=(PosixNetworkInterface*)mDNSStorage.HostInterfaces;
    while(pni) {
        DPRINTF(E_INF,L_REND,"Found interface %s, index %d\n",pni->intfName,
                pni->index);
        if(strcasecmp(pni->intfName,iface) == 0) {
            DPRINTF(E_INF,L_REND,"Found interface id: %d\n",pni->coreIntf.InterfaceID);
            return pni->coreIntf.InterfaceID;
        }
        pni=(PosixNetworkInterface*)(pni->coreIntf.next);
    }

    DPRINTF(E_INF,L_REND,"Could not find interface.\n");
    return mDNSInterface_Any;
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
    mDNSInterfaceID id;

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
        id=rend_get_interface_id(msg.iface);
        DPRINTF(E_DBG,L_REND,"Registering %s.%s (%d)\n",msg.name,msg.type,msg.port);
        RegisterOneService(msg.name,msg.type,"local.",msg.txt,strlen(msg.txt),
                           msg.port,id);
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

    if(os_drop_privs(user))
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
        DPRINTF(E_SPAM,L_REND,"select(%d, %d.%06d)\n", nfds, 
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

