/*
 * Layer Two Tunnelling Protocol Daemon
 * Copyright (C) 1998 Adtran, Inc.
 * Copyright (C) 2002 Jeff McAdams
 *
 * Mark Spencer
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * File format handling header file
 *
 */

#ifndef _FILE_H
#define _FILE_H

#define STRLEN 80               /* Length of a string */

/* Definition of a keyword */
struct keyword
{
    char *keyword;
    int (*handler) (char *word, char *value, int context, void *item);
};

struct iprange
{
    unsigned int start;
    unsigned int end;
    int sense;
    struct iprange *next;
};

struct host
{
    char hostname[STRLEN];
    int port;
    struct host *next;
};


#define CONTEXT_GLOBAL 	1
#define CONTEXT_LNS	   	2
#define CONTEXT_LAC		3
#define CONTEXT_DEFAULT	256

#define SENSE_ALLOW -1
#define SENSE_DENY 0

#ifndef DEFAULT_AUTH_FILE
#define DEFAULT_AUTH_FILE "/etc/l2tp-secrets"
#endif
#ifndef DEFAULT_CONFIG_FILE
#define DEFAULT_CONFIG_FILE "/etc/xl2tpd.conf"
#endif
#define ALT_DEFAULT_AUTH_FILE ""
#define ALT_DEFAULT_CONFIG_FILE ""
#define DEFAULT_PID_FILE "/var/run/xl2tpd.pid"

/* Definition of an LNS */
struct lns
{
    struct lns *next;
    int exclusive;              /* Only one tunnel per host? */
    int active;                 /* Is this actively in use? */
    unsigned int localaddr;     /* Local IP for PPP connections */
    int tun_rws;                /* Receive window size (tunnel) */
    int call_rws;               /* Call rws */
    int rxspeed;		/* Tunnel rx speed */
    int txspeed;		/* Tunnel tx speed */
    int hbit;                   /* Permit hidden AVP's? */
    int lbit;                   /* Use the length field? */
    int challenge;              /* Challenge authenticate the peer? */
    int authpeer;               /* Authenticate our peer? */
    int authself;               /* Authenticate ourselves? */
    char authname[STRLEN];      /* Who we authenticate as */
    char peername[STRLEN];      /* Force peer name to this */
    char hostname[STRLEN];      /* Hostname to report */
    char entname[STRLEN];       /* Name of this entry */
    struct iprange *lacs;       /* Hosts permitted to connect */
    struct iprange *range;      /* Range of IP's we provide */
    int assign_ip;              /* Do we actually provide IP addresses? */
    int passwdauth;             /* Authenticate by passwd file? (or PAM) */
    int pap_require;            /* Require PAP auth for PPP */
    int chap_require;           /* Require CHAP auth for PPP */
    int pap_refuse;             /* Refuse PAP authentication for us */
    int chap_refuse;            /* Refuse CHAP authentication for us */
    int idle;                   /* Idle timeout in seconds */
    unsigned int pridns;        /* Primary DNS server */
    unsigned int secdns;        /* Secondary DNS server */
    unsigned int priwins;       /* Primary WINS server */
    unsigned int secwins;       /* Secondary WINS server */
    int proxyarp;               /* Use proxy-arp? */
    int proxyauth;              /* Allow proxy authentication? */
    int debug;                  /* Debug PPP? */
    char pppoptfile[STRLEN];    /* File containing PPP options */
    struct tunnel *t;           /* Tunnel of this, if it's ready */
};

struct lac
{
    struct lac *next;
    struct host *lns;           /* LNS's we can connect to */
    struct schedule_entry *rsched;
    int tun_rws;                /* Receive window size (tunnel) */
    int call_rws;               /* Call rws */
    int rxspeed;		/* Tunnel rx speed */
    int txspeed;		/* Tunnel tx speed */
    int active;                 /* Is this connection in active use? */
    int hbit;                   /* Permit hidden AVP's? */
    int lbit;                   /* Use the length field? */
    int challenge;              /* Challenge authenticate the peer? */
    unsigned int localaddr;     /* Local IP address */
    unsigned int remoteaddr;    /* Force remote address to this */
    char authname[STRLEN];      /* Who we authenticate as */
    char password[STRLEN];      /* Password to authenticate with */
    char peername[STRLEN];      /* Force peer name to this */
    char hostname[STRLEN];      /* Hostname to report */
    char entname[STRLEN];       /* Name of this entry */
    int authpeer;               /* Authenticate our peer? */
    int authself;               /* Authenticate ourselves? */
    int pap_require;            /* Require PAP auth for PPP */
    int chap_require;           /* Require CHAP auth for PPP */
    int pap_refuse;             /* Refuse PAP authentication for us */
    int chap_refuse;            /* Refuse CHAP authentication for us */
    int idle;                   /* Idle timeout in seconds */
    int autodial;               /* Try to dial immediately? */
    int defaultroute;           /* Use as default route? */
    int redial;                 /* Redial if disconnected */
    int rmax;                   /* Maximum # of consecutive redials */
    int rtries;                 /* # of tries so far */
    int rtimeout;               /* Redial every this many # of seconds */
    char pppoptfile[STRLEN];    /* File containing PPP options */
    int debug;
    struct tunnel *t;           /* Our tunnel */
    struct call *c;             /* Our call */
};

struct global
{
    unsigned int listenaddr;    /* IP address to bind to */ 
    int port;                   /* Port number to listen to */
    char authfile[STRLEN];      /* File containing authentication info */
    char altauthfile[STRLEN];   /* File containing authentication info */
    char configfile[STRLEN];    /* File containing configuration info */
    char altconfigfile[STRLEN]; /* File containing configuration info */
    char pidfile[STRLEN];       /* File containing the pid number*/
    char controlfile[STRLEN];   /* Control file name (named pipe) */
    int daemon;                 /* Use daemon mode? */
    int accesscontrol;          /* Use access control? */
    int forceuserspace;         /* Force userspace? */
    int packet_dump;		/* Dump (print) all packets? */
    int debug_avp;		/* Print AVP debugging info? */
    int debug_network;		/* Print network debugging info? */
    int debug_tunnel;		/* Print tunnel debugging info? */
    int debug_state;		/* Print FSM debugging info? */
    int ipsecsaref;
};

extern struct global gconfig;   /* Global configuration options */

extern struct lns *lnslist;     /* All LNS entries */
extern struct lac *laclist;     /* All LAC entries */
extern struct lns *deflns;      /* Default LNS config */
extern struct lac *deflac;      /* Default LAC config */
extern int init_config ();      /* Read in the config file */
#endif
