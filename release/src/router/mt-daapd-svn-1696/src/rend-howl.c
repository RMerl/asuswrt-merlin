/*
 * $Id: rend-howl.c 1484 2007-01-17 01:06:16Z rpedde $
 * Rendezvous for SwampWolf's Howl (http://www.swampwolf.com)
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

#include <errno.h>
#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <howl.h>
#include <pthread.h>

#include "daapd.h"
#include "err.h"
#include "os-unix.h"
#include "rend-unix.h"

pthread_t rend_tid;
sw_discovery rend_handle;

/* Forwards */
void *rend_pipe_monitor(void* arg);
void rend_callback(void);

/*
 * rend_howl_reply
 *
 * Callback function for mDNS stuff
 */
static sw_result rend_howl_reply(sw_discovery discovery,
                                 sw_discovery_publish_status status,
                                 sw_discovery_oid oid,
                                 sw_opaque extra) {
    static sw_string status_text[] = {
        "started",
        "stopped",
        "name collision",
        "invalid"
    };

    DPRINTF(E_DBG,L_REND,"Publish reply: %s\n",status_text[status]);
    return SW_OKAY;
}


/*
 * rend_private_init
 *
 * Initialize howl and start runloop
 */
int rend_private_init(char *user) {
    DPRINTF(E_DBG,L_REND,"Starting rendezvous services\n");
    signal(SIGHUP,  SIG_IGN);           // SIGHUP might happen from a request to reload the daap server

    if(sw_discovery_init(&rend_handle) != SW_OKAY) {
        DPRINTF(E_WARN,L_REND,"Error initializing howl\n");
        errno=EINVAL;
        return -1;
    }

    if(os_drop_privs(user))
        return -1;

    DPRINTF(E_DBG,L_REND,"Starting polling thread\n");

    if(pthread_create(&rend_tid,NULL,rend_pipe_monitor,NULL)) {
        DPRINTF(E_FATAL,L_REND,"Could not start thread.  Terminating\n");
        /* should kill parent, too */
        exit(EXIT_FAILURE);
    }

    DPRINTF(E_DBG,L_REND,"Entering runloop\n");

    sw_discovery_run(rend_handle);

    DPRINTF(E_DBG,L_REND,"Exiting runloop\n");

    return 0;
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
                rend_callback();
            }
        }

        DPRINTF(E_DBG,L_REND,"Select error!\n");
        /* should really bail here */
    }
}


/*
 * rend_callback
 *
 * This gets called from the main thread when there is a
 * message waiting to be processed.
 */
void rend_callback(void) {
    REND_MESSAGE msg;
    sw_discovery_oid rend_oid;
    sw_result result;

    /* here, we've seen the message, now we have to process it */

    if(rend_read_message(&msg) != sizeof(msg)) {
        DPRINTF(E_FATAL,L_REND,"Rendezvous socket closed (daap server crashed?)  Aborting.\n");
        exit(EXIT_FAILURE);
    }

    switch(msg.cmd) {
    case REND_MSG_TYPE_REGISTER:
        DPRINTF(E_DBG,L_REND,"Registering %s.%s (%d)\n",msg.type,msg.name,msg.port);
        if((result=sw_discovery_publish(rend_handle,
                                        0, /* interface handle */
                                        msg.name,
                                        msg.type,
                                        NULL, /* domain */
                                        NULL, /* host */
                                        msg.port,
                                        (sw_octets) msg.txt,
                                        strlen(msg.txt),
                                        rend_howl_reply,
                                        NULL,
                                        &rend_oid)) != SW_OKAY) {
            DPRINTF(E_WARN,L_REND,"Error registering name\n");
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
        //sw_rendezvous_stop_publish(rend_handle);
        sw_discovery_fina(rend_handle);
        break;
    case REND_MSG_TYPE_STATUS:
        DPRINTF(E_DBG,L_REND,"Status inquiry -- returning 0\n");
        rend_send_response(0); /* success */
        break;
    default:
        break;
    }
}
