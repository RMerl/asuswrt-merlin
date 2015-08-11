/*
 * Rendezvous support with avahi
 *
 * Copyright (C) 2005 Sebastian Dröge <slomo@ubuntu.com>
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
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <net/if.h>


#include <avahi-client/client.h>
#include <avahi-client/publish.h>

#include <avahi-client/client.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/timeval.h>
#include <avahi-common/malloc.h>

#include "daapd.h"
#include "err.h"

static AvahiClient *mdns_client = NULL;
static AvahiEntryGroup *mdns_group = NULL;
static AvahiSimplePoll *simple_poll = NULL;

typedef struct tag_rend_avahi_group_entry {
    char *name;
    char *type;
    int port;
    char *iface;
    char *txt;
    struct tag_rend_avahi_group_entry *next;
} REND_AVAHI_GROUP_ENTRY;

static REND_AVAHI_GROUP_ENTRY rend_avahi_entries = { NULL, NULL, 0, NULL };

static pthread_t rend_tid;
static pthread_cond_t rend_avahi_cond;
static pthread_mutex_t rend_avahi_mutex;

static void _rend_avahi_signal(void);
static void _rend_avahi_wait_on(void *what);
static void _rend_avahi_lock(void);
static void _rend_avahi_unlock(void);
static int _rend_avahi_create_services(void);

/* add a new group entry node */
static int _rend_avahi_add_group_entry(char *name, char *type, int port, char *iface, char *txt) {
    REND_AVAHI_GROUP_ENTRY *pge;

    pge = (REND_AVAHI_GROUP_ENTRY *)malloc(sizeof(REND_AVAHI_GROUP_ENTRY));
    if(!pge)
        return 0;

    pge->name = strdup(name);
    pge->type = strdup(type);
    pge->iface = strdup(iface);
    pge->txt = strdup(txt);
    pge->port = port;

    _rend_avahi_lock();

    pge->next = rend_avahi_entries.next;
    rend_avahi_entries.next = pge;

    _rend_avahi_unlock();
    return 1;
}

static void *rend_poll(void *arg) {
    int ret;
    while((ret = avahi_simple_poll_iterate(simple_poll,-1)) == 0);

    if(ret < 0) {
        DPRINTF(E_WARN,L_REND,"Avahi poll thread quit iwth error: %s\n",
                avahi_strerror(avahi_client_errno(mdns_client)));
    } else {
        DPRINTF(E_DBG,L_REND,"Avahi poll thread quit\n");
    }

    return NULL;
}

static void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {
    //    assert(g == mdns_group);

    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        DPRINTF(E_DBG, L_REND, "Successfully added mdns services\n");
        _rend_avahi_signal();
        break;
    case AVAHI_ENTRY_GROUP_COLLISION:
        DPRINTF(E_DBG, L_REND, "Group collision\n");
        /*
          new_name = avahi_alternative_service_name(mdns_name);
          DPRINTF(E_WARN, L_REND, "mdns service name collision. Renamed service %s -> %s\n", mdns_name, new_name);
          free(mdns_name);
          mdns_name = new_name;
          add_services(avahi_entry_group_get_client(g));
        */
        break;
    case AVAHI_ENTRY_GROUP_FAILURE :
        avahi_simple_poll_quit(simple_poll);
        break;
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        break;
    }
}

void _rend_avahi_lock(void) {
    if(pthread_mutex_lock(&rend_avahi_mutex))
        DPRINTF(E_FATAL,L_REND,"Could not lock mutex\n");
}

void _rend_avahi_unlock(void) {
    pthread_mutex_unlock(&rend_avahi_mutex);
}

void _rend_avahi_signal(void) {
    /* kick the condition for waiters */
    _rend_avahi_lock();
    pthread_cond_signal(&rend_avahi_cond);
    _rend_avahi_unlock();
}

void _rend_avahi_wait_on(void *what) {
    DPRINTF(E_DBG,L_REND,"Waiting on something...\n");
    if(pthread_mutex_lock(&rend_avahi_mutex))
        DPRINTF(E_FATAL,L_REND,"Could not lock mutex\n");
    while(!what) {
        pthread_cond_wait(&rend_avahi_cond,&rend_avahi_mutex);
    }
    _rend_avahi_unlock();
    DPRINTF(E_DBG,L_REND,"Done waiting.\n");
}

int rend_register(char *name, char *type, int port, char *iface, char *txt) {
    DPRINTF(E_DBG,L_REND,"Adding %s/%s\n",name,type);
    _rend_avahi_add_group_entry(name,type,port,iface,txt);
    if(mdns_group) {
        DPRINTF(E_DBG,L_MISC,"Resetting mdns group\n");
        avahi_entry_group_reset(mdns_group);
    }
    DPRINTF(E_DBG,L_REND,"Creating service group (again?)\n");
    _rend_avahi_create_services();
    return 0;
}


/**
 * register the block of services
 *
 * @returns true if successful, false otherwise
 */
int _rend_avahi_create_services(void) {
    int ret = 0;
    REND_AVAHI_GROUP_ENTRY *pentry;
    AvahiStringList *psl;
    unsigned char count=0;
    unsigned char *key,*nextkey;
    unsigned char *newtxt;

    DPRINTF(E_DBG,L_REND,"Creting service group\n");

    if(!rend_avahi_entries.next) {
        DPRINTF(E_DBG,L_REND,"No entries yet... skipping service create\n");
        return 1;
    }

    if (mdns_group == NULL) {
        if (!(mdns_group = avahi_entry_group_new(mdns_client,
                                                 entry_group_callback,
                                                 NULL))) {
            DPRINTF(E_WARN, L_REND, "Could not create AvahiEntryGroup: %s\n",
                    avahi_strerror(avahi_client_errno(mdns_client)));
            return 0;
        }
    }

    /* wait for entry group to be created */
    _rend_avahi_wait_on(mdns_group);

    _rend_avahi_lock();
    pentry = rend_avahi_entries.next;
    while(pentry) {
        /* TODO: honor iface parameter */
        DPRINTF(E_DBG,L_REND,"Re-registering %s/%s\n",pentry->name,pentry->type);

        /* build a string list */
        psl = NULL;
        newtxt = (unsigned char *)strdup(pentry->txt);
        if(!newtxt)
            DPRINTF(E_FATAL,L_REND,"malloc\n");

        key=nextkey=newtxt;
        if(*nextkey)
            count = *nextkey;

        DPRINTF(E_DBG,L_REND,"Found key of size %d\n",count);
        while((*nextkey)&&(nextkey < (newtxt + strlen(pentry->txt)))) {
            key = nextkey + 1;
            nextkey += (count+1);
            count = *nextkey;
            *nextkey = '\0';
            psl=avahi_string_list_add(psl,(char*)key);
            DPRINTF(E_DBG,L_REND,"Added key %s\n",key);
            *nextkey=count;
        }

        free(newtxt);

        if ((ret = avahi_entry_group_add_service_strlst(mdns_group,
                                                        AVAHI_IF_UNSPEC,
                                                 AVAHI_PROTO_UNSPEC, 0,
                                                 avahi_strdup(pentry->name),
                                                 avahi_strdup(pentry->type),
                                                 NULL, NULL,pentry->port,
                                                 psl)) < 0) {
            DPRINTF(E_WARN, L_REND, "Could not add mdns services: %s\n", avahi_strerror(ret));
            avahi_string_list_free(psl);
            _rend_avahi_unlock();
            return 0;
        }
        pentry = pentry->next;
    }

    _rend_avahi_unlock();

    if ((ret = avahi_entry_group_commit(mdns_group)) < 0) {
        DPRINTF(E_WARN, L_REND, "Could not commit mdns services: %s\n",
                avahi_strerror(avahi_client_errno(mdns_client)));
        return 0;
    }

    return 1;
}

static void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) {
    assert(c);
    switch(state) {
    case AVAHI_CLIENT_S_RUNNING:
        DPRINTF(E_LOG,L_REND,"Client running\n");
        if(!mdns_group)
            _rend_avahi_create_services();
        break;
    case AVAHI_CLIENT_S_COLLISION:
        DPRINTF(E_LOG,L_REND,"Client collision\n");
        if(mdns_group)
            avahi_entry_group_reset(mdns_group);
        break;
    case AVAHI_CLIENT_FAILURE:
        DPRINTF(E_LOG,L_REND,"Client failure\n");
        avahi_simple_poll_quit(simple_poll);
        break;
    case AVAHI_CLIENT_S_REGISTERING:
        DPRINTF(E_LOG,L_REND,"Client registering\n");
        if(mdns_group)
            avahi_entry_group_reset(mdns_group);
        break;
    case AVAHI_CLIENT_CONNECTING:
        break;
    }
}

int rend_init(char *user) {
    int error;

    if(pthread_cond_init(&rend_avahi_cond,NULL)) {
        DPRINTF(E_LOG,L_REND,"Could not initialize rendezvous condition\n");
        return -1;
    }

    if(pthread_mutex_init(&rend_avahi_mutex,NULL)) {
        DPRINTF(E_LOG,L_REND,"Could not initialize rendezvous mutex\n");
        return -1;
    }

    DPRINTF(E_DBG, L_REND, "Initializing avahi\n");
    if(!(simple_poll = avahi_simple_poll_new())) {
        DPRINTF(E_LOG,L_REND,"Error starting poll thread\n");
        return -1;
    }

    /*
        mdns_name = strdup(name);
        mdns_port = port;
        //      if ((interface != NULL) && (if_nametoindex(interface) != 0))
        //              mdns_interface = if_nametoindex(interface);
        //      else
        mdns_interface = AVAHI_IF_UNSPEC;
    */

    if (!(mdns_client = avahi_client_new(avahi_simple_poll_get(simple_poll),
                                         0,client_callback,NULL,&error))) {
        DPRINTF(E_WARN, L_REND, "avahi_client_new: Error in avahi: %s\n",
                avahi_strerror(avahi_client_errno(mdns_client)));
        avahi_simple_poll_free(simple_poll);
        return -1;
    }

    DPRINTF(E_DBG, L_REND, "Starting avahi polling thread\n");
    if(pthread_create(&rend_tid, NULL, rend_poll, NULL)) {
        DPRINTF(E_FATAL,L_REND,"Could not start avahi polling thread.\n");
    }

    return 0;
}

int rend_stop() {
    avahi_simple_poll_quit(simple_poll);
    if (mdns_client != NULL)
        avahi_client_free(mdns_client);
    return 0;
}

int rend_running(void) {
    return 1;
}

int rend_unregister(char *name, char *type, int port) {
    return 0;
}

