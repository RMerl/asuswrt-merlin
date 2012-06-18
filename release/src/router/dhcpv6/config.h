/*	$KAME: config.h,v 1.39 2005/04/01 12:43:36 jinmei Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* definitions of tail-queue types */
TAILQ_HEAD(ia_conflist, ia_conf);
TAILQ_HEAD(pifc_list, prefix_ifconf);

struct dhcp6_poolspec {
	char* name;
	u_int32_t pltime;
	u_int32_t vltime;
};

struct dhcp6_range {
	struct in6_addr min;
	struct in6_addr max;
};

struct pool_conf {
	struct pool_conf *next;

	char* name;

	struct in6_addr min;
	struct in6_addr max;
};

/* per-interface information */
struct dhcp6_if {
	struct dhcp6_if *next;

	/* timer for the interface */
	struct dhcp6_timer *timer;

	/* event queue */
	TAILQ_HEAD(, dhcp6_event) event_list;

	/* static parameters of the interface */
	char *ifname;
	unsigned int ifid;
	u_int32_t linkid;	/* to send link-local packets */
	/* multiple global address configuration is not supported now */
	struct in6_addr addr; 	/* global address */

	/* configuration parameters */
	u_long send_flags;
	u_long allow_flags;
#define DHCIFF_INFO_ONLY 0x1
#define DHCIFF_RAPID_COMMIT 0x2

	int server_pref;	/* server preference (server only) */
	struct dhcp6_poolspec pool;	/* address pool (server only) */
	char *scriptpath;	/* path to config script (client only) */

	struct dhcp6_list reqopt_list;
	struct ia_conflist iaconf_list;

	/* authentication information */
	int authproto;		/* protocol */
	/* the followings are valid only if authproto is not UNDEF */
	int authalgorithm;	/* algorithm */
	int authrdm;		/* replay attack detection method */
};

/* run-time authentication parameters */
struct authparam {
	int authproto;
	int authalgorithm;
	int authrdm;
	struct keyinfo *key;
	int flags;
#define AUTHPARAM_FLAGS_NOPREVRD	0x1

	u_int64_t prevrd;	/* previous RD value provided by the peer */
};

struct dhcp6_event {
	TAILQ_ENTRY(dhcp6_event) link;

	struct dhcp6_if *ifp;
	struct dhcp6_timer *timer;

	struct duid serverid;

	struct timeval tv_start; /* timestamp when the 1st msg is sent */

	/* internal timer parameters */
	long retrans;
	long init_retrans;
	long max_retrans_cnt;
	long max_retrans_time;
	long max_retrans_dur;
	int timeouts;		/* number of timeouts */

	u_int32_t xid;		/* current transaction ID */
	int state;

	/* list of known servers */
	struct dhcp6_serverinfo *current_server;
	struct dhcp6_serverinfo *servers;

	/* authentication parameters */
	struct authparam *authparam;

	TAILQ_HEAD(, dhcp6_eventdata) data_list;
};

typedef enum { DHCP6_EVDATA_IAPD, DHCP6_EVDATA_IANA } dhcp6_eventdata_t;

struct dhcp6_eventdata {
	TAILQ_ENTRY(dhcp6_eventdata) link;

	struct dhcp6_event *event;
	dhcp6_eventdata_t type;
	void *data;

	void (*destructor) __P((struct dhcp6_eventdata *));
	void *privdata;
};

struct dhcp6_serverinfo {
	struct dhcp6_serverinfo *next;

	/* option information provided in the advertisement */
	struct dhcp6_optinfo optinfo;

	int pref;		/* preference */
	int active;		/* bool; if this server is active or not */

	struct authparam *authparam; /* authentication parameters */

	/* TODO: remember available information from the server */
};

/* client status code */
enum {DHCP6S_INIT, DHCP6S_SOLICIT, DHCP6S_INFOREQ, DHCP6S_REQUEST,
      DHCP6S_RENEW, DHCP6S_REBIND, DHCP6S_RELEASE, DHCP6S_IDLE};

struct prefix_ifconf {
	TAILQ_ENTRY(prefix_ifconf) link;

	char *ifname;		/* interface name such as ne0 */
	int sla_len;		/* SLA ID length in bits */
	u_int32_t sla_id;	/* need more than 32bits? */
	int ifid_len;		/* interface ID length in bits */
	int ifid_type;		/* EUI-64 and manual (unused?) */
	char ifid[16];		/* Interface ID, up to 128bits */
};
#define IFID_LEN_DEFAULT 64
#define SLA_LEN_DEFAULT 16

typedef enum { IATYPE_PD, IATYPE_NA } iatype_t;
struct ia_conf {
	TAILQ_ENTRY(ia_conf) link;
	/*struct ia_conf *next;*/
	iatype_t type;
	u_int32_t iaid;

	TAILQ_HEAD(, ia) iadata; /* struct ia is an opaque type */

	/* type dependent values follow */
};

struct iapd_conf {
	struct ia_conf iapd_ia;

	/* type dependent values follow */
	struct dhcp6_list iapd_prefix_list;
	struct pifc_list iapd_pif_list;
};
#define iapd_next iapd_ia.next
#define iapd_type iapd_ia.type
#define iapd_id iapd_ia.iaid

struct iana_conf {
	struct ia_conf iana_ia;

	/* type dependent values follow */
	struct dhcp6_list iana_address_list;
};
#define iana_next iana_ia.next

/* per-host configuration */
struct host_conf {
	struct host_conf *next;

	char *name;		/* host name to identify the host */
	struct duid duid;	/* DUID for the host */

	/* prefixes to be delegated to the host */
	struct dhcp6_list prefix_list;
	/* address to be assigned for the host */
	struct dhcp6_list addr_list;
	/* address pool from which addresses are assigned for the host */
	struct dhcp6_poolspec pool;

	/* secret key shared with the client for delayed authentication */
	struct keyinfo *delayedkey;
	/* previous replay detection value from the client */
	int saw_previous_rd;	/* if we remember the previous value */
	u_int64_t previous_rd;
};

/* DHCPv6 authentication information */
struct authinfo {
	struct authinfo *next;

	char *name;		/* auth info name */

	int protocol;		/* authentication protocol */
	int algorithm;		/* authentication algorithm */
	int rdm;		/* random attack detection method */

	/* keys specific to this info? */
};

/* structures and definitions used in the config file parser */
struct cf_namelist {
	struct cf_namelist *next;
	char *name;
	int line;		/* the line number of the config file */
	struct cf_list *params;
};

struct cf_list {
	struct cf_list *next;
	struct cf_list *tail;
	int type;
	int line;		/* the line number of the config file */

	/* type dependent values: */
	long long num;
	struct cf_list *list;
	void *ptr;
};

enum { DECL_SEND, DECL_ALLOW, DECL_INFO_ONLY, DECL_REQUEST, DECL_DUID,
       DECL_PREFIX, DECL_PREFERENCE, DECL_SCRIPT, DECL_DELAYEDKEY,
       DECL_ADDRESS,
       DECL_RANGE, DECL_ADDRESSPOOL,
       IFPARAM_SLA_ID, IFPARAM_SLA_LEN,
       DHCPOPT_RAPID_COMMIT, DHCPOPT_AUTHINFO,
       DHCPOPT_DNS, DHCPOPT_DNSNAME,
       DHCPOPT_IA_PD, DHCPOPT_IA_NA, DHCPOPT_NTP,
       DHCPOPT_REFRESHTIME,
       DHCPOPT_NIS, DHCPOPT_NISNAME, 
       DHCPOPT_NISP, DHCPOPT_NISPNAME, 
       DHCPOPT_BCMCS, DHCPOPT_BCMCSNAME, 
       CFLISTENT_GENERIC,
       IACONF_PIF, IACONF_PREFIX, IACONF_ADDR,
       DHCPOPT_SIP, DHCPOPT_SIPNAME,
       AUTHPARAM_PROTO, AUTHPARAM_ALG, AUTHPARAM_RDM, AUTHPARAM_KEY,
       KEYPARAM_REALM, KEYPARAM_KEYID, KEYPARAM_SECRET, KEYPARAM_EXPIRE };

typedef enum {DHCP6_MODE_SERVER, DHCP6_MODE_CLIENT, DHCP6_MODE_RELAY }
dhcp6_mode_t;

extern const dhcp6_mode_t dhcp6_mode;

extern struct dhcp6_if *dhcp6_if;
extern struct dhcp6_ifconf *dhcp6_iflist;
extern struct prefix_ifconf *prefix_ifconflist;
extern struct dhcp6_list siplist;
extern struct dhcp6_list sipnamelist;
extern struct dhcp6_list dnslist;
extern struct dhcp6_list dnsnamelist;
extern struct dhcp6_list ntplist;
extern struct dhcp6_list nislist;
extern struct dhcp6_list nisnamelist;
extern struct dhcp6_list nisplist;
extern struct dhcp6_list nispnamelist;
extern struct dhcp6_list bcmcslist;
extern struct dhcp6_list bcmcsnamelist;
extern long long optrefreshtime;

extern struct dhcp6_if *ifinit __P((char *));
extern int ifreset __P((struct dhcp6_if *));
extern int configure_interface __P((struct cf_namelist *));
extern int configure_host __P((struct cf_namelist *));
extern int configure_keys __P((struct cf_namelist *));
extern int configure_authinfo __P((struct cf_namelist *));
extern int configure_ia __P((struct cf_namelist *, iatype_t));
extern int configure_global_option __P((void));
extern void configure_cleanup __P((void));
extern void configure_commit __P((void));
extern int cfparse __P((char *));
extern struct dhcp6_if *find_ifconfbyname __P((char *));
extern struct dhcp6_if *find_ifconfbyid __P((unsigned int));
extern struct prefix_ifconf *find_prefixifconf __P((char *));
extern struct host_conf *find_hostconf __P((struct duid *));
extern struct authinfo *find_authinfo __P((struct authinfo *, char *));
extern struct dhcp6_prefix *find_prefix6 __P((struct dhcp6_list *,
					      struct dhcp6_prefix *));
extern struct ia_conf *find_iaconf __P((struct ia_conflist *, int, u_int32_t));
extern struct keyinfo *find_key __P((char *, size_t, u_int32_t));
extern int configure_pool __P((struct cf_namelist *));
extern struct pool_conf *find_pool __P((const char *));
extern int is_available_in_pool __P((struct pool_conf *, struct in6_addr *));
extern int get_free_address_from_pool __P((struct pool_conf *,
	struct in6_addr *));
struct host_conf *create_dynamic_hostconf __P((struct duid *,
	struct dhcp6_poolspec *));
extern char *qstrdup __P((char *));
