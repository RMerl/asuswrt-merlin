/*
 * Copyright (c) 2010 Mark Williams
 * Copyright (c) 2012 Frank Lahm <franklahm@gmail.com>
 *
 * File change event API for netatalk
 *
 * for every detected filesystem change a UDP packet is sent to an arbitrary list
 * of listeners. Each packet contains unix path of modified filesystem element,
 * event reason, and a consecutive event id (32 bit). Technically we are UDP client and are sending
 * out packets synchronuosly as they are created by the afp functions. This should not affect
 * performance measurably. The only delaying calls occur during initialization, if we have to
 * resolve non-IP hostnames to IP. All numeric data inside the packet is network byte order, so use
 * ntohs / ntohl to resolve length and event id. Ideally a listener receives every packet with
 * no gaps in event ids, starting with event id 1 and mode FCE_CONN_START followed by
 * data events from id 2 up to 0xFFFFFFFF, followed by 0 to 0xFFFFFFFF and so on.
 *
 * A gap or not starting with 1 mode FCE_CONN_START or receiving mode FCE_CONN_BROKEN means that
 * the listener has lost at least one filesystem event
 * 
 * All Rights Reserved.  See COPYRIGHT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

#include <atalk/adouble.h>
#include <atalk/vfs.h>
#include <atalk/logger.h>
#include <atalk/afp.h>
#include <atalk/util.h>
#include <atalk/cnid.h>
#include <atalk/unix.h>
#include <atalk/fce_api.h>
#include <atalk/globals.h>

#include "fork.h"
#include "file.h"
#include "directory.h"
#include "desktop.h"
#include "volume.h"

// ONLY USED IN THIS FILE
#include "fce_api_internal.h"

/* We store our connection data here */
static struct udp_entry udp_socket_list[FCE_MAX_UDP_SOCKS];
static int udp_sockets = 0;
static bool udp_initialized = false;
static unsigned long fce_ev_enabled =
    (1 << FCE_FILE_MODIFY) |
    (1 << FCE_FILE_DELETE) |
    (1 << FCE_DIR_DELETE) |
    (1 << FCE_FILE_CREATE) |
    (1 << FCE_DIR_CREATE);

#define MAXIOBUF 1024
static unsigned char iobuf[MAXIOBUF];
static const char *skip_files[] = 
{
	".DS_Store",
	NULL
};
static struct fce_close_event last_close_event;

static char *fce_event_names[] = {
    "",
    "FCE_FILE_MODIFY",
    "FCE_FILE_DELETE",
    "FCE_DIR_DELETE",
    "FCE_FILE_CREATE",
    "FCE_DIR_CREATE"
};

/*
 *
 * Initialize network structs for any listeners
 * We dont give return code because all errors are handled internally (I hope..)
 *
 * */
void fce_init_udp()
{
    int rv;
    struct addrinfo hints, *servinfo, *p;

    if (udp_initialized == true)
        return;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    for (int i = 0; i < udp_sockets; i++) {
        struct udp_entry *udp_entry = udp_socket_list + i;

        /* Close any pending sockets */
        if (udp_entry->sock != -1)
            close(udp_entry->sock);

        if ((rv = getaddrinfo(udp_entry->addr, udp_entry->port, &hints, &servinfo)) != 0) {
            LOG(log_error, logtype_fce, "fce_init_udp: getaddrinfo(%s:%s): %s",
                udp_entry->addr, udp_entry->port, gai_strerror(rv));
            continue;
        }

        /* loop through all the results and make a socket */
        for (p = servinfo; p != NULL; p = p->ai_next) {
            if ((udp_entry->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
                LOG(log_error, logtype_fce, "fce_init_udp: socket(%s:%s): %s",
                    udp_entry->addr, udp_entry->port, strerror(errno));
                continue;
            }
            break;
        }

        if (p == NULL) {
            LOG(log_error, logtype_fce, "fce_init_udp: no socket for %s:%s",
                udp_entry->addr, udp_entry->port);
        }
        udp_entry->addrinfo = *p;
        memcpy(&udp_entry->addrinfo, p, sizeof(struct addrinfo));
        memcpy(&udp_entry->sockaddr, p->ai_addr, sizeof(struct sockaddr_storage));
        freeaddrinfo(servinfo);
    }

    udp_initialized = true;
}

void fce_cleanup()
{
    if (udp_initialized == false )
        return;

    for (int i = 0; i < udp_sockets; i++)
    {
        struct udp_entry *udp_entry = udp_socket_list + i;

        /* Close any pending sockets */
        if (udp_entry->sock != -1)
        {
            close( udp_entry->sock );
            udp_entry->sock = -1;
        }
    }
    udp_initialized = false;
}

/*
 * Construct a UDP packet for our listeners and return packet size
 * */
static ssize_t build_fce_packet( struct fce_packet *packet, const char *path, int event, uint32_t event_id )
{
    size_t pathlen = 0;
    ssize_t data_len = 0;

    /* Set content of packet */
    memcpy(packet->magic, FCE_PACKET_MAGIC, sizeof(packet->magic) );
    packet->version = FCE_PACKET_VERSION;
    packet->mode = event;
   
    packet->event_id = event_id; 

    pathlen = strlen(path); /* exclude string terminator */
    
    /* This should never happen, but before we bust this server, we send nonsense, fce listener has to cope */
    if (pathlen >= MAXPATHLEN)
        pathlen = MAXPATHLEN - 1;

    packet->datalen = pathlen;

    /* This is the payload len. Means: the packet has len bytes more until packet is finished */
    data_len = FCE_PACKET_HEADER_SIZE + pathlen;

    memcpy(packet->data, path, pathlen);

    /* return the packet len */
    return data_len;
}

/*
 * Handle Endianess and write into buffer w/o padding
 **/ 
static void pack_fce_packet(struct fce_packet *packet, unsigned char *buf, int maxlen)
{
    unsigned char *p = buf;

    memcpy(p, &packet->magic[0], sizeof(packet->magic));
    p += sizeof(packet->magic);

    *p = packet->version;
    p++;
    
    *p = packet->mode;
    p++;
    
    uint32_t *id = (uint32_t*)p;
    *id = htonl(packet->event_id);
    p += sizeof(packet->event_id);

    uint16_t *l = ( uint16_t *)p;
    *l = htons(packet->datalen);
    p += sizeof(packet->datalen);

    if (((p - buf) +  packet->datalen) < maxlen) {
        memcpy(p, &packet->data[0], packet->datalen);
    }
}

/*
 * Send the fce information to all (connected) listeners
 * We dont give return code because all errors are handled internally (I hope..)
 * */
static void send_fce_event(const char *path, int event)
{    
    static bool first_event = true;

    struct fce_packet packet;
    static uint32_t event_id = 0; /* the unique packet couter to detect packet/data loss. Going from 0xFFFFFFFF to 0x0 is a valid increment */
    time_t now = time(NULL);

    LOG(log_debug, logtype_fce, "send_fce_event: start");

    /* initialized ? */
    if (first_event == true) {
        first_event = false;
        fce_init_udp();
        /* Notify listeners the we start from the beginning */
        send_fce_event( "", FCE_CONN_START );
    }

    /* build our data packet */
    ssize_t data_len = build_fce_packet( &packet, path, event, ++event_id );
    pack_fce_packet(&packet, iobuf, MAXIOBUF);

    for (int i = 0; i < udp_sockets; i++)
    {
        int sent_data = 0;
        struct udp_entry *udp_entry = udp_socket_list + i;

        /* we had a problem earlier ? */
        if (udp_entry->sock == -1)
        {
            /* We still have to wait ?*/
            if (now < udp_entry->next_try_on_error)
                continue;

            /* Reopen socket */
            udp_entry->sock = socket(udp_entry->addrinfo.ai_family,
                                     udp_entry->addrinfo.ai_socktype,
                                     udp_entry->addrinfo.ai_protocol);
            
            if (udp_entry->sock == -1) {
                /* failed again, so go to rest again */
                LOG(log_error, logtype_fce, "Cannot recreate socket for fce UDP connection: errno %d", errno  );

                udp_entry->next_try_on_error = now + FCE_SOCKET_RETRY_DELAY_S;
                continue;
            }

            udp_entry->next_try_on_error = 0;

            /* Okay, we have a running socket again, send server that we had a problem on our side*/
            data_len = build_fce_packet( &packet, "", FCE_CONN_BROKEN, 0 );
            pack_fce_packet(&packet, iobuf, MAXIOBUF);

            sendto(udp_entry->sock,
                   iobuf,
                   data_len,
                   0,
                   (struct sockaddr *)&udp_entry->sockaddr,
                   udp_entry->addrinfo.ai_addrlen);

            /* Rebuild our original data packet */
            data_len = build_fce_packet(&packet, path, event, event_id);
            pack_fce_packet(&packet, iobuf, MAXIOBUF);
        }

        sent_data = sendto(udp_entry->sock,
                           iobuf,
                           data_len,
                           0,
                           (struct sockaddr *)&udp_entry->sockaddr,
                           udp_entry->addrinfo.ai_addrlen);

        /* Problems ? */
        if (sent_data != data_len) {
            /* Argh, socket broke, we close and retry later */
            LOG(log_error, logtype_fce, "send_fce_event: error sending packet to %s:%s, transfered %d of %d: %s",
                udp_entry->addr, udp_entry->port, sent_data, data_len, strerror(errno));

            close( udp_entry->sock );
            udp_entry->sock = -1;
            udp_entry->next_try_on_error = now + FCE_SOCKET_RETRY_DELAY_S;
        }
    }
}

static int add_udp_socket(const char *target_ip, const char *target_port )
{
    if (target_port == NULL)
        target_port = FCE_DEFAULT_PORT_STRING;

    if (udp_sockets >= FCE_MAX_UDP_SOCKS) {
        LOG(log_error, logtype_fce, "Too many file change api UDP connections (max %d allowed)", FCE_MAX_UDP_SOCKS );
        return AFPERR_PARAM;
    }

    udp_socket_list[udp_sockets].addr = strdup(target_ip);
    udp_socket_list[udp_sockets].port = strdup(target_port);
    udp_socket_list[udp_sockets].sock = -1;
    memset(&udp_socket_list[udp_sockets].addrinfo, 0, sizeof(struct addrinfo));
    memset(&udp_socket_list[udp_sockets].sockaddr, 0, sizeof(struct sockaddr_storage));
    udp_socket_list[udp_sockets].next_try_on_error = 0;

    udp_sockets++;

    return AFP_OK;
}

static void save_close_event(const char *path)
{
    time_t now = time(NULL);

    /* Check if it's a close for the same event as the last one */
    if (last_close_event.time   /* is there any saved event ? */
        && (strcmp(path, last_close_event.path) != 0)) {
        /* no, so send the saved event out now */
        send_fce_event(last_close_event.path, FCE_FILE_MODIFY);
    }

    LOG(log_debug, logtype_fce, "save_close_event: %s", path);

    last_close_event.time = now;
    strncpy(last_close_event.path, path, MAXPATHLEN);
}

/*
 *
 * Dispatcher for all incoming file change events
 *
 * */
int fce_register(fce_ev_t event, const char *path, const char *oldpath, fce_obj_t type)
{
    static bool first_event = true;
    const char *bname;

    if (!(fce_ev_enabled & (1 << event)))
        return AFP_OK;

    AFP_ASSERT(event >= FCE_FIRST_EVENT && event <= FCE_LAST_EVENT);
    AFP_ASSERT(path);

    LOG(log_debug, logtype_fce, "register_fce(path: %s, type: %s, event: %s",
        path, type == fce_dir ? "dir" : "file", fce_event_names[event]);

    bname = basename_safe(path);

    if (udp_sockets == 0)
        /* No listeners configured */
        return AFP_OK;


	/* do some initialization on the fly the first time */
	if (first_event) {
		fce_initialize_history();
        first_event = false;
	}

	/* handle files which should not cause events (.DS_Store atc. ) */
	for (int i = 0; skip_files[i] != NULL; i++) {
		if (strcmp(bname, skip_files[i]) == 0)
			return AFP_OK;
	}

	/* Can we ignore this event based on type or history? */
	if (fce_handle_coalescation(event, path, type)) {
		LOG(log_debug9, logtype_fce, "Coalesced fc event <%d> for <%s>", event, path);
		return AFP_OK;
	}

    switch (event) {
    case FCE_FILE_MODIFY:
        save_close_event(path);
        break;
    default:
        send_fce_event(path, event);
        break;
    }

    return AFP_OK;
}

static void check_saved_close_events(int fmodwait)
{
    time_t now = time(NULL);

    /* check if configured holdclose time has passed */
    if (last_close_event.time && ((last_close_event.time + fmodwait) < now)) {
        LOG(log_debug, logtype_fce, "check_saved_close_events: sending event: %s", last_close_event.path);
        /* yes, send event */
        send_fce_event(&last_close_event.path[0], FCE_FILE_MODIFY);
        last_close_event.path[0] = 0;
        last_close_event.time = 0;
    }
}

/******************** External calls start here **************************/

/*
 * API-Calls for file change api, called form outside (file.c directory.c ofork.c filedir.c)
 * */
void fce_pending_events(AFPObj *obj)
{
    if (!udp_sockets)
        return;
    check_saved_close_events(obj->options.fce_fmodwait);
}

/*
 *
 * Extern connect to afpd parameter, can be called multiple times for multiple listeners (up to MAX_UDP_SOCKS times)
 *
 * */
int fce_add_udp_socket(const char *target)
{
	const char *port = FCE_DEFAULT_PORT_STRING;
	char target_ip[256] = {""};

	strncpy(target_ip, target, sizeof(target_ip) -1);

	char *port_delim = strchr( target_ip, ':' );
	if (port_delim) {
		*port_delim = 0;
		port = port_delim + 1;
	}
	return add_udp_socket(target_ip, port);
}

int fce_set_events(const char *events)
{
    char *e;
    char *p;
    
    if (events == NULL)
        return AFPERR_PARAM;

    e = strdup(events);

    fce_ev_enabled = 0;

    for (p = strtok(e, ", "); p; p = strtok(NULL, ", ")) {
        if (strcmp(p, "fmod") == 0) {
            fce_ev_enabled |= (1 << FCE_FILE_MODIFY);
        } else if (strcmp(p, "fdel") == 0) {
            fce_ev_enabled |= (1 << FCE_FILE_DELETE);
        } else if (strcmp(p, "ddel") == 0) {
            fce_ev_enabled |= (1 << FCE_DIR_DELETE);
        } else if (strcmp(p, "fcre") == 0) {
            fce_ev_enabled |= (1 << FCE_FILE_CREATE);
        } else if (strcmp(p, "dcre") == 0) {
            fce_ev_enabled |= (1 << FCE_DIR_CREATE);
        }
    }

    free(e);

    return AFP_OK;
}

#ifdef FCE_TEST_MAIN


void shortsleep( unsigned int us )
{    
    usleep( us );
}
int main( int argc, char*argv[] )
{
    int c;

    char *port = FCE_DEFAULT_PORT_STRING;
    char *host = "localhost";
    int delay_between_events = 1000;
    int event_code = FCE_FILE_MODIFY;
    char pathbuff[1024];
    int duration_in_seconds = 0; // TILL ETERNITY
    char target[256];
    char *path = getcwd( pathbuff, sizeof(pathbuff) );

    // FULLSPEED TEST IS "-s 1001" -> delay is 0 -> send packets without pause

    while ((c = getopt(argc, argv, "d:e:h:p:P:s:")) != -1) {
        switch(c) {
        case '?':
            fprintf(stdout, "%s: [ -p Port -h Listener1 [ -h Listener2 ...] -P path -s Delay_between_events_in_us -e event_code -d Duration ]\n", argv[0]);
            exit(1);
            break;
        case 'd':
            duration_in_seconds = atoi(optarg);
            break;
        case 'e':
            event_code = atoi(optarg);
            break;
        case 'h':
            host = strdup(optarg);
            break;
        case 'p':
            port = strdup(optarg);
            break;
        case 'P':
            path = strdup(optarg);
            break;
        case 's':
            delay_between_events = atoi(optarg);
            break;
        }
    }

    sprintf(target, "%s:%s", host, port);
    if (fce_add_udp_socket(target) != 0)
        return 1;

    int ev_cnt = 0;
    time_t start_time = time(NULL);
    time_t end_time = 0;

    if (duration_in_seconds)
        end_time = start_time + duration_in_seconds;

    while (1)
    {
        time_t now = time(NULL);
        if (now > start_time)
        {
            start_time = now;
            fprintf( stdout, "%d events/s\n", ev_cnt );
            ev_cnt = 0;
        }
        if (end_time && now >= end_time)
            break;

        fce_register(event_code, path, NULL, 0);
        ev_cnt++;

        
        shortsleep( delay_between_events );
    }
}
#endif /* TESTMAIN*/
