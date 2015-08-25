/*
 * $Id: rend-osx.c 1443 2006-11-27 00:00:00Z rpedde $
 * Rendezvous - OSX style
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>
#include <pwd.h>
#include <pthread.h>
#include <sys/types.h>

#include <libc.h>
#include <arpa/nameser.h>
#include <CoreFoundation/CoreFoundation.h>
#include <DNSServiceDiscovery/DNSServiceDiscovery.h>

#include "daapd.h"
#include "err.h"
#include "os-unix.h"
#include "rend-unix.h"

CFRunLoopRef rend_runloop;
CFRunLoopSourceRef rend_rls;
pthread_t rend_tid;

/* Forwards */
void *rend_pipe_monitor(void* arg);

/*
 * rend_stoprunloop
 */
static void rend_stoprunloop(void) {
    CFRunLoopStop(rend_runloop);
}

/*
 * rend_sigint
 */
/*
static void rend_sigint(int sigraised) {
    DPRINTF(E_INF,L_REND,"SIGINT\n");
    rend_stoprunloop();
}
*/

/*
 * rend_handler
 */
static void rend_handler(CFMachPortRef port, void *msg, CFIndex size, void *info) {
    DNSServiceDiscovery_handleReply(msg);
}

/*
 * rend_addtorunloop
 */
static int rend_addtorunloop(dns_service_discovery_ref client) {
    mach_port_t port=DNSServiceDiscoveryMachPort(client);

    if(!port)
        return -1;
    else {
        CFMachPortContext context = { 0, 0, NULL, NULL, NULL };
        Boolean shouldFreeInfo;
        CFMachPortRef cfMachPort=CFMachPortCreateWithPort(kCFAllocatorDefault,
                                                          port, rend_handler,
                                                          &context, &shouldFreeInfo);

        CFRunLoopSourceRef rls=CFMachPortCreateRunLoopSource(NULL,cfMachPort,0);
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                           rls,kCFRunLoopDefaultMode);
        CFRelease(rls);
        return 0;
    }
}

/*
 * rend_reply
 */
static void rend_reply(DNSServiceRegistrationReplyErrorType errorCode, void *context) {
    switch(errorCode) {
    case kDNSServiceDiscoveryNoError:
        DPRINTF(E_DBG,L_REND,"Registered successfully\n");
        break;
    case kDNSServiceDiscoveryNameConflict:
        DPRINTF(E_WARN,L_REND,"Error - name in use\n");
        break;
    default:
        DPRINTF(E_WARN,L_REND,"Error %d\n",errorCode);
        break;
    }
}

/*
 * rend_pipe_monitor
 */
void *rend_pipe_monitor(void* arg) {
    fd_set rset;
    int result;


    while(1) {
        DPRINTF(E_DBG,L_REND,"Waiting for data\n");
        FD_ZERO(&rset);
        FD_SET(rend_pipe_to[RD_SIDE],&rset);

        /* sit in a select spin until there is data on the to fd */
        while(((result=select(rend_pipe_to[RD_SIDE] + 1,&rset,NULL,NULL,NULL)) != -1) &&
            errno != EINTR) {
            if(FD_ISSET(rend_pipe_to[RD_SIDE],&rset)) {
                DPRINTF(E_DBG,L_REND,"Received a message from daap server\n");
                CFRunLoopSourceSignal(rend_rls);
                CFRunLoopWakeUp(rend_runloop);
                sleep(1);  /* force a reschedule, hopefully */
            }
        }

        DPRINTF(E_DBG,L_REND,"Select error!\n");
        /* should really bail here */
    }
}


/**
 * Add a text stanza to the buffer (pascal-style multistring)
 *
 * @param buffer where to put the text info
 * @param string what pascal string to append
 */
void rend_add_text(char *buffer, char *string) {
    char *ptr=&buffer[strlen(buffer)];
    *ptr=strlen(string);
    strcpy(ptr+1,string);
}

/*
 * rend_callback
 *
 * This gets called from the main thread when there is a
 * message waiting to be processed.
 */
void rend_callback(void *info) {
    REND_MESSAGE msg;
    unsigned short usPort;
    dns_service_discovery_ref dns_ref=NULL;
    char *src,*dst;
    int len;

    /* here, we've seen the message, now we have to process it */
    if(rend_read_message(&msg) != sizeof(msg)) {
        DPRINTF(E_FATAL,L_REND,"Rendezvous socket closed (daap server crashed?)  Aborting.\n");
        exit(EXIT_FAILURE);
    }

    src=dst=msg.txt;
    while(src && (*src) && (src - msg.txt < MAX_TEXT_LEN)) {
        len = (*src);
        if((src + len + 1) - msg.txt < MAX_TEXT_LEN) {
            memmove(dst,src+1,len);
            dst += len;
            if(*src) {
                *dst++ = '\001';
            } else {
                *dst='\0';
            }
        }
        src += len + 1;
    }

    switch(msg.cmd) {
    case REND_MSG_TYPE_REGISTER:
        DPRINTF(E_DBG,L_REND,"Registering %s.%s (%d)\n",msg.type,msg.name,msg.port);
        usPort=htons(msg.port);
        dns_ref=DNSServiceRegistrationCreate(msg.name,msg.type,"",usPort,msg.txt,rend_reply,nil);
        if(rend_addtorunloop(dns_ref)) {
            DPRINTF(E_WARN,L_REND,"Add to runloop failed\n");
            rend_send_response(-1);
        } else {
            rend_send_response(0); /* success */
        }
        break;
    case REND_MSG_TYPE_UNREGISTER:
        DPRINTF(E_WARN,L_REND,"Unsupported function: UNREGISTER\n");
        rend_send_response(-1); /* error */
        break;
    case REND_MSG_TYPE_STOP:
        DPRINTF(E_DBG,L_REND,"Stopping mDNS\n");
        rend_send_response(0);
        rend_stoprunloop();
        break;
    case REND_MSG_TYPE_STATUS:
        DPRINTF(E_DBG,L_REND,"Status inquiry -- returning 1\n");
        rend_send_response(1); /* success */
        break;
    default:
        break;
    }
}

/*
 * rend_private_init
 *
 * start up the rendezvous services
 */
int rend_private_init(char *user) {
    CFRunLoopSourceContext context;

    if(os_drop_privs(user)) /* shouldn't be running as root anyway */
        return -1;

    /* need a sigint handler */
    DPRINTF(E_DBG,L_REND,"Starting rendezvous services\n");

    memset((void*)&context,0,sizeof(context));
    context.perform = rend_callback;

    rend_runloop = CFRunLoopGetCurrent();
    rend_rls = CFRunLoopSourceCreate(NULL,0,&context);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),rend_rls,kCFRunLoopDefaultMode);

    DPRINTF(E_DBG,L_REND,"Starting polling thread\n");

    if(pthread_create(&rend_tid,NULL,rend_pipe_monitor,NULL)) {
        DPRINTF(E_FATAL,L_REND,"Could not start thread.  Terminating\n");
        /* should kill parent, too */
        exit(EXIT_FAILURE);
    }

    DPRINTF(E_DBG,L_REND,"Starting runloop\n");

    CFRunLoopRun();

    DPRINTF(E_DBG,L_REND,"Exiting runloop\n");

    CFRelease(rend_rls);
    pthread_cancel(rend_tid);
    close(rend_pipe_to[RD_SIDE]);
    close(rend_pipe_from[WR_SIDE]);
    return 0;
}

