/**
 * $Id: $
 *
 * Implementation of functions for UPnP discovery
 *
 * Copyright (C) 2007 Ron Pedde (ron@pedde.com)
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
# include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include "conf.h"
#include "daapd.h"
#include "err.h"
#include "os.h"
#include "util.h"
#include "upnp.h"

#define UPNP_MAX_PACKET 1500
#define UPNP_CACHE_DURATION 1800
#define UPNP_ADDR "239.255.255.250"
#define UPNP_PORT 1900

#define UPNP_TYPE_ALIVE    1
#define UPNP_TYPE_BYEBYE   2
#define UPNP_TYPE_RESPONSE 3

#define UPNP_UUID "12345678-1234-1234-1234-123456789013"

#define UPNP_SELECT_TIMEOUT  1

typedef struct upnp_adinfo_t {
    int type;         /** one of the UPNP_AD_ values */
    int version;      /** numeric version.  0 if unversioned */
    char *location;   /** url path to descriptor.  e.g. /upnp-device.xml */
    char *namespace;  /** usually "upnp" or "urn:schemas-upnp-org" */
    char *name;       /** MediaServer, ConnectionManager, rootdevice, etc */
    char *body;       /** body of a alive packet (postinfo) */
} UPNP_ADINFO;

typedef struct upnp_packetinfo_t {
    char *group_id;
    UPNP_ADINFO *padinfo;
    struct upnp_packetinfo_t *next;
} UPNP_PACKETINFO;

typedef struct upnp_disco_t {
    int seconds_remaining;
    char *query;
    struct sockaddr_in to;
    struct upnp_disco_t *next;
} UPNP_DISCO;

/* Globals */
UPNP_PACKETINFO upnp_packetlist;
OS_SOCKETTYPE upnp_socket;
UPNP_DISCO upnp_disco;

pthread_t upnp_listener_tid;
int upnp_quitflag = 0;
int upnp_thread_started = 0;
char *upnp_broadcast_types[] = {
    "none",
    "SSDP Alive",
    "SSDP Byebye",
    "SSDP Discovery Response"
};

/* Forwards */
int upnp_strcat(char *what, char *where, int bytes_left);
void upnp_build_packet(char *packet, int len, int type, UPNP_PACKETINFO *pi,
                       UPNP_DISCO *pdisco);
void upnp_broadcast(int type, UPNP_DISCO *pdisco);
void *upnp_listener(void *arg);
void upnp_process_packet(void);
void upnp_process_queries(void);

/**
 * return the current upnp uuid
 */
char *upnp_uuid(void) {
    return UPNP_UUID;
}


/**
 * add a upnp packet too the root list.
 */
void upnp_add_packet(char *group_id, int type, char *location,
                     char *namespace, char *name, int version,
                     char *body) {
    UPNP_PACKETINFO *pnew;
    UPNP_ADINFO *pnewinfo;

    pnew = (UPNP_PACKETINFO *)malloc(sizeof(UPNP_PACKETINFO));
    if(!pnew)
        DPRINTF(E_FATAL,L_MISC,"Malloc error\n");

    pnewinfo = (UPNP_ADINFO *)malloc(sizeof(UPNP_ADINFO));
    if(!pnewinfo)
        DPRINTF(E_FATAL,L_MISC,"Malloc error\n");

    memset(pnew,0,sizeof(UPNP_PACKETINFO));
    memset(pnewinfo,0,sizeof(UPNP_ADINFO));

    if(group_id)
        pnew->group_id = strdup(group_id);

    pnewinfo->type = type;
    pnewinfo->version = version;
    if(location) pnewinfo->location = strdup(location);
    if(namespace) pnewinfo->namespace = strdup(namespace);
    if(name) pnewinfo->name = strdup(name);
    if(body) pnewinfo->body = strdup(body);

    pnew->padinfo = pnewinfo;

    util_mutex_lock(l_upnp);
    pnew->next = upnp_packetlist.next;
    upnp_packetlist.next = pnew;
    util_mutex_unlock(l_upnp);
}

int upnp_strcat(char *what, char *where, int bytes_left) {
    if(!what)
        return bytes_left;

    if(strlen(what) < bytes_left) {
        strcat(where,what);
        return bytes_left - strlen(what);
    }

    return bytes_left;
}


void upnp_build_packet(char *packet, int len, int type,
                       UPNP_PACKETINFO *pi, UPNP_DISCO *pdisco) {
    char buffer[256];
    char hostname[256];
    *packet = '\0';
    int port;

    port = conf_get_int("general","port",0);

    if(type == UPNP_TYPE_RESPONSE) {
        len = upnp_strcat("HTTP/1.1 200 OK\r\n",packet,len);
    } else {
        len = upnp_strcat("NOTIFY * HTTP/1.1\r\n",packet,len);
    }

    if(pi->padinfo->location) {
        /* Note that UPnP spec says this SHOULD be ip address,
         * not FQDN.  Roku SB, for example, doesn't like FQDN. */
        gethostname(hostname,sizeof(hostname));
        snprintf(buffer,sizeof(buffer),"LOCATION: http://%s:%d%s\r\n",
                 hostname,port,pi->padinfo->location);

        len=upnp_strcat(buffer,packet,len);
    }

    if(type != UPNP_TYPE_RESPONSE)
        len=upnp_strcat("HOST: 239.255.255.250:1900\r\n",packet,len);

    len=upnp_strcat("SERVER: POSIX, UPnP/1.0, Firefly/" VERSION "\r\n",
                     packet,len);

    if(type == UPNP_TYPE_ALIVE) {
        len=upnp_strcat("NTS: ssdp:alive\r\n",packet,len);
    } else if(type == UPNP_TYPE_BYEBYE) {
        len=upnp_strcat("NTS: ssdp:byebye\r\n",packet,len);
    } else if ((type == UPNP_TYPE_RESPONSE) && (pdisco)) {
        /* we need the original ST */
        snprintf(buffer,sizeof(buffer),"ST: %s\r\n",pdisco->query);
        len = upnp_strcat(buffer,packet,len);
    }

    /* USN & NT */
    switch(pi->padinfo->type) {
    case UPNP_AD_UUID:
        snprintf(buffer,sizeof(buffer),"USN: uuid:%s\r\n",upnp_uuid());
        len=upnp_strcat(buffer,packet,len);
        if(type != UPNP_TYPE_RESPONSE) { /* no NT for responses */
            snprintf(buffer,sizeof(buffer),"NT: uuid:%s\r\n",upnp_uuid());
            len=upnp_strcat(buffer,packet,len);
        }
        break;

    case UPNP_AD_DEVICE:
    case UPNP_AD_SERVICE:
    case UPNP_AD_ROOT:
        snprintf(buffer,sizeof(buffer),"USN: uuid:%s::%s:%s%s",upnp_uuid(),
                 pi->padinfo->namespace,
                 (pi->padinfo->type == UPNP_AD_DEVICE) ? "device:" :
                 ((pi->padinfo->type == UPNP_AD_SERVICE) ? "service:" : ""),
                 pi->padinfo->name);
        len=upnp_strcat(buffer,packet,len);
        if(pi->padinfo->version) {
            snprintf(buffer,sizeof(buffer),":%d",pi->padinfo->version);
            len=upnp_strcat(buffer,packet,len);
        }
        len=upnp_strcat("\r\n",packet,len);

        if(type != UPNP_TYPE_RESPONSE) { /* no NT for responses */
            snprintf(buffer,sizeof(buffer),"NT: %s:%s%s",
                     pi->padinfo->namespace,
                     (pi->padinfo->type == UPNP_AD_DEVICE) ? "device:" :
                     ((pi->padinfo->type == UPNP_AD_SERVICE) ? "service:":""),
                     pi->padinfo->name);

            len=upnp_strcat(buffer,packet,len);
            if(pi->padinfo->version) {
                snprintf(buffer,sizeof(buffer),":%d",pi->padinfo->version);
                len=upnp_strcat(buffer,packet,len);
            }
            len=upnp_strcat("\r\n",packet,len);
        }
        break;
    default:
        break;
    }

    snprintf(buffer,sizeof(buffer),"CACHE-CONTROL: max-age=%d\r\n",
             UPNP_CACHE_DURATION);
    len=upnp_strcat(buffer,packet,len);

    if((pi->padinfo->body) && (type != UPNP_TYPE_RESPONSE)) {
        snprintf(buffer,sizeof(buffer),"Content-Length: %d\r\n\r\n",
                (int)strlen(pi->padinfo->body));
        len=upnp_strcat(buffer,packet,len);
        len=upnp_strcat(pi->padinfo->body,packet,len);
    } else {
        len=upnp_strcat("Content-Length: 0\r\n\r\n",packet,len);
    }
}

/**
 * broadcast out the upnp packets
 */
void upnp_broadcast(int type, UPNP_DISCO *pdisco) {
    UPNP_PACKETINFO *pi;
    struct sockaddr_in sin;
    char packet[UPNP_MAX_PACKET];
    int pass;
    char passes;
    int send_packet;
    int elements=0;
    int recognized=0;
    char **argv = NULL;

    memset(&sin, 0, sizeof(sin));
    if(type == UPNP_TYPE_RESPONSE) {
        memcpy((void*)&sin,(void*)&pdisco->to,sizeof(struct sockaddr_in));
    } else {
        sin.sin_addr.s_addr = inet_addr(UPNP_ADDR);
        sin.sin_family = AF_INET;
        sin.sin_port = htons(UPNP_PORT);
    }

    util_mutex_lock(l_upnp);

    DPRINTF(E_DBG,L_MISC,"Sending upnp broadcast of type: %d (%s)\n",type,
            upnp_broadcast_types[type]);

    passes = 2;
    if(type == UPNP_TYPE_RESPONSE) {
        passes = 1;
        if(strcasecmp(pdisco->query,"ssdp:all")==0)
            passes = 3;

        elements = util_split(pdisco->query,":",&argv);
    }

    for(pass=0; pass < passes; pass++) {
        pi=upnp_packetlist.next;
        while(pi) {
            send_packet = 1;
            if(type == UPNP_TYPE_RESPONSE) {
                send_packet = 0;
                /* if it's a request for rootdevice, only respond
                   with root device.  */
                if(strcasecmp(pdisco->query,"ssdp:all")==0) {
                    send_packet = 1;
                } else if((strcasecmp(pdisco->query,"upnp:rootdevice")==0) &&
                          (pi->padinfo->type == UPNP_AD_ROOT)) {
                    send_packet = 1;
                }
                else if((strncasecmp(pdisco->query,"uuid:",5) == 0) &&
                        (!strcasecmp((char*)&pdisco->query[5],upnp_uuid())) &&
                        (pi->padinfo->type == UPNP_AD_UUID)) {
                    send_packet = 1;
                } else if((elements == 5) &&
                          (strcasecmp(argv[2],"device")==0)) {
                    if((pi->padinfo->type == UPNP_AD_DEVICE) &&
                       (strcasecmp((char*)&pi->padinfo->namespace[4],argv[1])==0) &&
                       (strcasecmp(pi->padinfo->name,argv[3])==0) &&
                       (pi->padinfo->version >= atoi(argv[4]))) {
                        send_packet = 1;
                    }

                } else if((elements == 5) &&
                          (strcasecmp(argv[2],"service")==0)) {
                    if((pi->padinfo->type == UPNP_AD_SERVICE) &&
                       (strcasecmp((char*)&pi->padinfo->namespace[4],argv[1])==0) &&
                       (strcasecmp(pi->padinfo->name,argv[3])==0) &&
                       (pi->padinfo->version >= atoi(argv[4]))) {
                        send_packet = 1;
                    }
                }
            }

            if(send_packet) {
                usleep(100);
                recognized = 1;
                upnp_build_packet(packet, UPNP_MAX_PACKET, type, pi, pdisco);
                sendto(upnp_socket,packet,strlen(packet),0,
                       (struct sockaddr *)&sin, sizeof(sin));
            }

            pi = pi->next;
        }
    }

    if(type == UPNP_TYPE_RESPONSE) {
        printf("%s respond to query %s\n", recognized ? "Did" : "Did not",
               pdisco->query);
    }

    if(argv) {
        util_dispose_split(argv);
    }

    util_mutex_unlock(l_upnp);
}

/**
 * start UPnP broadcaster.  We'll want to send at least a rootdevice
 * announcement with a location pointer to the admin page
 */
int upnp_init(void) {
    int ttl = 3;
    int reuse=1;
    int result;
    int err;
    struct sockaddr_in addr;
    struct ip_mreq mreq;

    srand((unsigned)time(NULL));

    memset(&upnp_packetlist,0,sizeof(upnp_packetlist));
    memset(&upnp_disco,0,sizeof(upnp_disco));

    upnp_add_packet("base,", UPNP_AD_UUID, "/upnp-basic.xml",
                    NULL, NULL, 0, NULL);
    upnp_add_packet("base,", UPNP_AD_DEVICE, "/upnp-basic.xml",
                    "urn:schemas-upnp-org","MediaServer", 1, NULL);
    upnp_add_packet("base,", UPNP_AD_SERVICE, "/upnp-basic.xml",
                    "urn:schemas-upnp-org","ContentDirectory", 1, NULL);
    upnp_add_packet("base,", UPNP_AD_SERVICE, "/upnp-basic.xml",
                    "urn:schemas-upnp-org","ConnectionManager", 1, NULL);
    upnp_add_packet("base,", UPNP_AD_ROOT, "/upnp-basic.xml",
                    "upnp","rootdevice",0,NULL);

    upnp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    result = setsockopt(upnp_socket, IPPROTO_IP, IP_MULTICAST_TTL,
                        &ttl, sizeof(ttl));
    if(result == -1) {
        close(upnp_socket);
        DPRINTF(E_LOG,L_MISC,"Error setting IP_MULTICAST_TTL\n");
        return FALSE;
    }

    result = setsockopt(upnp_socket,SOL_SOCKET,SO_REUSEADDR,
                        &reuse,sizeof(reuse));
    if(result == -1) {
        close(upnp_socket);
        DPRINTF(E_LOG,L_MISC,"Error setting SO_REUSEADDR\n");
        return FALSE;
    }


    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_port=htons(UPNP_PORT);

    if(bind(upnp_socket,(struct sockaddr *)&addr,sizeof(addr)) < 0) {
        DPRINTF(E_LOG,L_MISC,"Error binding to upnp port\n");
        close(upnp_socket);
        return FALSE;
    }

    mreq.imr_multiaddr.s_addr=inet_addr(UPNP_ADDR);
    mreq.imr_interface.s_addr=htonl(INADDR_ANY);
    if(setsockopt(upnp_socket,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
        DPRINTF(E_LOG,L_MISC,"Error joining UPnP multicast group\n");
        close(upnp_socket);
        return FALSE;
    }

    /* got the socket, let's start up the listener thread */
    DPRINTF(E_DBG,L_MISC,"Starting UPnP listener thread\n");
    if((err=pthread_create(&upnp_listener_tid,NULL,upnp_listener,NULL))) {
        DPRINTF(E_LOG,L_MISC,"Warning: could not start upnp listener: %s\n",
                strerror(err));
        upnp_thread_started=0;
        return TRUE;
    }

    return TRUE;
}

/**
 * walk the query chain and see if there are any queries we need
 * to respond to.
 */
void upnp_process_queries(void) {
    UPNP_DISCO *plast, *pcurrent;

    plast=&upnp_disco;
    pcurrent = upnp_disco.next;

    /* don't need locks here since this ist he same thread as the listening
     * thread */

    while(pcurrent) {
        if(pcurrent->seconds_remaining - UPNP_SELECT_TIMEOUT < 1) {
            /* do the broadcast, and cull it */
            upnp_broadcast(UPNP_TYPE_RESPONSE,pcurrent);
            pcurrent = pcurrent->next;
            free(plast->next->query);
            free(plast->next);
            plast->next = pcurrent;
        } else {
            pcurrent->seconds_remaining -= UPNP_SELECT_TIMEOUT;
            plast = pcurrent;
            pcurrent = pcurrent->next;
        }
    }
}


/**
 * listener thread for upnp query requests
 */
void *upnp_listener(void *arg) {
    int result;
    static time_t last_broadcast=0;
    fd_set readset;
    struct timeval timeout;

    upnp_thread_started = 1;
    DPRINTF(E_DBG,L_MISC,"upnp listener thread started\n");

    /* sit in a tight select loop until upnp_deinit sets the
     * quitflag */
    while(!upnp_quitflag) {
        FD_ZERO(&readset);
        FD_SET(upnp_socket,&readset); /* win32? */

        /* this is kind of lame, but will defend from an OS
         * that doesn't return a select error when an underlying
         * file handle is closed */
        timeout.tv_sec=UPNP_SELECT_TIMEOUT;
        timeout.tv_usec=0;

        while((result=select(upnp_socket+1,&readset,NULL,NULL,&timeout)==-1) &&
              (errno = EINTR)) {
        }

        if(result == -1) {
            /* this is a real error */
            DPRINTF(E_LOG,L_MISC,"Error in select in upnp listener: %s\n",
                    strerror(errno));
            upnp_thread_started=0;
            pthread_exit(NULL);
        }

        if(FD_ISSET(upnp_socket,&readset)) {
            upnp_process_packet();
        }

        if((time(NULL) - last_broadcast) > (UPNP_CACHE_DURATION/3)) {
            DPRINTF(E_DBG,L_MISC,"Time for alive message!\n");
            upnp_broadcast(UPNP_TYPE_ALIVE,NULL);
            last_broadcast = time(NULL);
        }

        upnp_process_queries();
    }

    upnp_thread_started=0;
    DPRINTF(E_DBG,L_MISC,"upnp listener exiting\n");
    pthread_exit(NULL);
}

/**
 * process a upnp packet.  This is either a discovery or an advertisement
 * from another device on the local netowrk.
 */
void upnp_process_packet(void) {
    char upnp_packet[UPNP_MAX_PACKET];
    ssize_t bytes_read;
    char *line_start, *line_end;
    int discovery = 0;
    int mx = 0;
    char *query=NULL;
    struct sockaddr_in from;
    socklen_t from_len;
    UPNP_DISCO *pdisco;

    from_len = sizeof(from);
    bytes_read = recvfrom(upnp_socket,&upnp_packet,sizeof(upnp_packet),0,
                          (struct sockaddr *)&from, &from_len);

    /* now, parse the packet.  If it's a upnp query, it's a
     * packet with a ssdp of "discover".
     */

    line_start = line_end = upnp_packet;
    while((line_end - upnp_packet) < bytes_read) {
        while(((*line_start=='\n')||(*line_start=='\r')) &&
              (line_end - upnp_packet < bytes_read)) {
            /* absorb empty cr/lf pairs */
            line_start++;
            line_end++;
        }

        while((*line_end != '\n') && (*line_end != '\r') &&
              (line_end - upnp_packet < bytes_read))
            line_end++;

        if((*line_end == '\n') || (*line_end == '\r')) {
            /* we have a full line */
            *line_end = '\0';
            line_end++;

            /* okay... see what it is */
            if(strncasecmp(line_start,"M-SEARCH",8) == 0) {
                DPRINTF(E_DBG,L_MISC,"upnp_packet: M-SEARCH from %s\n",
                        inet_ntoa(from.sin_addr));

                discovery=1;
            }

            if(strncasecmp(line_start,"ST:",3)==0) {
                /* There is a search term specified */
                if(query)
                    free(query);

                line_start += 3;
                while(*line_start && *line_start == ' ')
                    line_start++;

                query = strdup(line_start);
                DPRINTF(E_DBG,L_MISC,"upnp_packet: Query: %s\n",query);
            }
            if(strncasecmp(line_start,"MX:",3)==0) {
                mx = atoi(line_start + 3);
            }
        }

        line_start=line_end;
        /* we could process the packet, and advertise, if necessary */

    }

    if(discovery) {
        DPRINTF(E_DBG,L_MISC,"Responding to query request...\n");

        /* do we care?  Let's check the query and see if it is interesting */
        discovery = 0;

        pdisco = (UPNP_DISCO *)malloc(sizeof(UPNP_DISCO));
        if(!pdisco)
            DPRINTF(E_FATAL,L_MISC,"malloc error");

        memset(pdisco,0,sizeof(UPNP_DISCO));
        if(mx) {
            pdisco->seconds_remaining = (rand() / (RAND_MAX/mx)) + 1;
            if(mx + 1 > UPNP_SELECT_TIMEOUT)
                pdisco->seconds_remaining = 0;

            DPRINTF(E_DBG,L_MISC,"Responding in %d (of %d) seconds\n",
                    pdisco->seconds_remaining, mx);
        }

        pdisco->query = query;
        query = NULL;
        memcpy((void*)&pdisco->to, (void*)&from,sizeof(struct sockaddr_in));
        pdisco->next = upnp_disco.next;
        upnp_disco.next = pdisco;
    }

    if(query) {
        free(query);
        query = NULL;
    }
}



/**
 * turn off any upnp services.  Should really de-register
 * all registered devices, etc.
 */
int upnp_deinit(void) {
    void *result_ptr;

    upnp_quitflag=1;
    upnp_broadcast(UPNP_TYPE_BYEBYE,NULL);

    if(upnp_socket)
        close(upnp_socket);

    /* socket is dead, quitflag is set, listener should terminate
     * reasonably quickly */

    if(upnp_thread_started) {
        upnp_thread_started=0;
        pthread_join(upnp_listener_tid, &result_ptr);
    }

    return TRUE;
}
