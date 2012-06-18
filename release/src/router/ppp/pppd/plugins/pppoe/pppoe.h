!UNUSED!
/* PPPoE support library "libpppoe"
 *
 * Copyright 2000 Michal Ostrowski <mostrows@styx.uwaterloo.ca>,
 *		  Jamal Hadi Salim <hadi@cyberus.ca>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 */

#ifndef PPPOE_H
#define PPPOE_H	1
#include <stdio.h>		/* stdio               */
#include <stdlib.h>		/* strtoul(), realloc() */
#include <unistd.h>		/* STDIN_FILENO,exec    */
#include <string.h>		/* memcpy()             */
#include <errno.h>		/* errno                */
#include <signal.h>
#include <getopt.h>
#include <stdarg.h>
#include <syslog.h>
#include <paths.h>

#include <sys/types.h>		/* socket types         */
#include <asm/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>		/* ioctl()              */
#include <sys/select.h>
#include <sys/socket.h>		/* socket()             */
#include <net/if.h>		/* ifreq struct         */
#include <net/if_arp.h>
#include <netinet/in.h>

#if __GLIBC__ >= 2 && __GLIBC_MINOR >= 1
#include <netpacket/packet.h>
//#include <net/ethernet.h>
#else
#include <asm/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#endif


#include <asm/byteorder.h>

/*
  jamal: we really have to change this
  to make it compatible to the 2.2 and
  2.3 kernel
*/

#include <linux/if_pppox.h>


#define CONNECTED 1
#define DISCONNECTED 0

#ifndef _PATH_PPPD
#define _PATH_PPPD "/usr/sbin/pppd"
#endif

#ifndef LOG_PPPOE
#define LOG_PPPOE LOG_DAEMON
#endif


#define VERSION_MAJOR 0
#define VERSION_MINOR 4
#define VERSION_DATE 991120

/* Bigger than the biggest ethernet packet we'll ever see, in bytes */
#define MAX_PACKET      2000

/* references: RFC 2516 */
/* ETHER_TYPE fields for PPPoE */

#define ETH_P_PPPOE_DISC 0x8863	/* discovery stage */
#define ETH_P_PPPOE_SESS 0x8864

/* ethernet broadcast address */
#define MAC_BCAST_ADDR "\xff\xff\xff\xff\xff\xff"

/* Format for parsing long device-name */
#define _STR(x) #x
#define FMTSTRING(size) "%x:%x:%x:%x:%x:%x/%x/%" _STR(size) "s"

/* maximum payload length */
#define MAX_PAYLOAD 1484



/* PPPoE tag types */
#define MAX_TAGS		11


/* PPPoE packet; includes Ethernet headers and such */
struct pppoe_packet{
	struct sockaddr_ll addr;
	struct pppoe_tag *tags[MAX_TAGS];
	struct pppoe_hdr *hdr;
	char buf[MAX_PAYLOAD];		/* buffer in which tags are held */
};
/* Defines meaning of each "tags" element */

#define TAG_SRV_NAME	0
#define TAG_AC_NAME	1
#define TAG_HOST_UNIQ	2
#define TAG_AC_COOKIE	3
#define TAG_VENDOR 	4
#define TAG_RELAY_SID	5
#define TAG_SRV_ERR     6
#define TAG_SYS_ERR  	7
#define TAG_GEN_ERR  	8
#define TAG_EOL		9

static int tag_map[] = { PTT_SRV_NAME,
			 PTT_AC_NAME,
			 PTT_HOST_UNIQ,
			 PTT_AC_COOKIE,
			 PTT_VENDOR,
			 PTT_RELAY_SID,
			 PTT_SRV_ERR,
			 PTT_SYS_ERR,
			 PTT_GEN_ERR,
			 PTT_EOL
};


/* Debug flags */
int DEB_DISC,DEB_DISC2;
/*
  #define DEB_DISC		(opt_debug & 0x0002)
  #define DEB_DISC2		(opt_debug & 0x0004)
*/
#define MAX_FNAME		256


struct session;

/* return <0 --> fatal error; abor
   return =0 --> ok, proceed
   return >0 --> ok, qui
*/
typedef int (*packet_cb_t)(struct session* ses,
			   struct pppoe_packet *p_in,
			   struct pppoe_packet **p_out);

/* various override filter tags */
struct filter {
	struct pppoe_tag *stag;  /* service name tag override */
	struct pppoe_tag *ntag;  /*AC name override */
	struct pppoe_tag *htag;  /* hostuniq override */
	int num_restart;
	int peermode;
	char *fname;
	char *pppd;
} __attribute__ ((packed));


struct pppoe_tag *make_filter_tag(short type, short length, char* data);

/* Session type definitions */
#define SESSION_CLIENT	0
#define SESSION_SERVER	1
#define SESSION_RELAY	2

struct session {

	/* Administrative */
	int type;
	int opt_debug;
	int detached;
	int np;
	int log_to_fd;
	int ifindex;			/* index of device */
	char name[IFNAMSIZ];		/*dev name */
	struct pppoe_packet curr_pkt;

	packet_cb_t init_disc;
	packet_cb_t rcv_pado;
	packet_cb_t rcv_padi;
	packet_cb_t rcv_pads;
	packet_cb_t rcv_padr;
	packet_cb_t rcv_padt;
	packet_cb_t timeout;


	/* Generic */
	struct filter *filt;
	struct sockaddr_ll local;
	struct sockaddr_ll remote;
	struct sockaddr_pppox sp;
	int fd;				/* fd of PPPoE socket */


	/* For client */
	int retransmits;		/* Number of retransmission performed
					   if < 0 , retransmissions disabled */
	int retries;
	int state;
	int opt_daemonize;

	/* For server */
	int fork;

	/* For forwarding */
	int fwd_sock;
	char fwd_name[IFNAMSIZ];	/* Name of device to forward to */
} __attribute__ ((packed));

/*
  retransmit retries for the PADR and PADI packets
  during discovery
*/
int PADR_ret;
int PADI_ret;

int log_to_fd;
int ctrl_fd;
int opt_debug;
int opt_daemonize;


/* Structure for keeping track of connection relays */
struct pppoe_con{
	struct pppoe_con *next;
	int id;
	int connected;
	int  cl_sock;
	int  sv_sock;
	int ref_count;
	char client[ETH_ALEN];
	char server[ETH_ALEN];
	char key_len;
	char key[32];
};

/* Functions exported from utils.c. */

/* Functions exported from pppoehash.c */
struct pppoe_con *get_con(int len, char *key);
int store_con(struct pppoe_con *pc);
struct pppoe_con *delete_con(unsigned long len, char *key);

/* exported by lib.c */

extern int init_lib();

extern int get_sockaddr_ll(const char *devnam,struct sockaddr_ll* sll);

extern int client_init_ses (struct session *ses, char* devnam);
extern int relay_init_ses(struct session *ses, char* from, char* to);
extern int srv_init_ses(struct session *ses, char* from);
extern int session_connect(struct session *ses);
extern int session_disconnect(struct session*ses);

extern int verify_packet( struct session *ses, struct pppoe_packet *p);

extern void copy_tag(struct pppoe_packet *dest, struct pppoe_tag *pt);
extern struct pppoe_tag *get_tag(struct pppoe_hdr *ph, u_int16_t idx);
extern int send_disc(struct session *ses, struct pppoe_packet *p);


extern int add_client(char *addr);

/* Make connections (including spawning pppd) as server/client */
extern ppp_connect(struct session *ses);

#endif
