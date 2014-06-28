/* $Id: upnpglobalvars.h,v 1.38 2014/03/10 11:04:53 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef UPNPGLOBALVARS_H_INCLUDED
#define UPNPGLOBALVARS_H_INCLUDED

#include <time.h>
#include "upnppermissions.h"
#include "miniupnpdtypes.h"
#include "config.h"

/* name of the network interface used to acces internet */
extern const char * ext_if_name;

/* file to store all leases */
#ifdef ENABLE_LEASEFILE
extern const char * lease_file;
#endif

/* forced ip address to use for this interface
 * when NULL, getifaddr() is used */
extern const char * use_ext_ip_addr;

/* parameters to return to upnp client when asked */
extern unsigned long downstream_bitrate;
extern unsigned long upstream_bitrate;

/* statup time */
extern time_t startup_time;

extern unsigned long int min_lifetime;
extern unsigned long int max_lifetime;

/* runtime boolean flags */
extern int runtime_flags;
#define LOGPACKETSMASK		0x0001
#define SYSUPTIMEMASK		0x0002
#ifdef ENABLE_NATPMP
#define ENABLENATPMPMASK	0x0004
#endif
#define CHECKCLIENTIPMASK	0x0008
#define SECUREMODEMASK		0x0010

#define ENABLEUPNPMASK		0x0020

#ifdef PF_ENABLE_FILTER_RULES
#define PFNOQUICKRULESMASK	0x0040
#endif
#ifdef ENABLE_IPV6
#define IPV6DISABLEDMASK	0x0080
#endif
#ifdef ENABLE_6FC_SERVICE
#define IPV6FCFWDISABLEDMASK		0x0100
#define IPV6FCINBOUNDDISALLOWEDMASK	0x0200
#endif
#ifdef ENABLE_PCP
#define PCP_ALLOWTHIRDPARTYMASK	0x0400
#endif

#define SETFLAG(mask)	runtime_flags |= mask
#define GETFLAG(mask)	(runtime_flags & mask)
#define CLEARFLAG(mask)	runtime_flags &= ~mask

extern const char * pidfilename;

extern char uuidvalue_igd[];	/* uuid of root device (IGD) */
extern char uuidvalue_wan[];	/* uuid of WAN Device */
extern char uuidvalue_wcd[];	/* uuid of WAN Connection Device */

#define SERIALNUMBER_MAX_LEN (48)
extern char serialnumber[];

#define MODELNUMBER_MAX_LEN (48)
extern char modelnumber[];

#define PRESENTATIONURL_MAX_LEN (64)
extern char presentationurl[];

#ifdef ENABLE_MANUFACTURER_INFO_CONFIGURATION
#define FRIENDLY_NAME_MAX_LEN (64)
extern char friendly_name[];

#define MANUFACTURER_NAME_MAX_LEN (64)
extern char manufacturer_name[];

#define MANUFACTURER_URL_MAX_LEN (64)
extern char manufacturer_url[];

#define MODEL_NAME_MAX_LEN (64)
extern char model_name[];

#define MODEL_DESCRIPTION_MAX_LEN (64)
extern char model_description[];

#define MODEL_URL_MAX_LEN (64)
extern char model_url[];
#endif

/* UPnP permission rules : */
extern struct upnpperm * upnppermlist;
extern unsigned int num_upnpperm;

#ifdef PCP_SADSCP
extern struct dscp_values* dscp_values_list;
extern unsigned int num_dscp_values;
#endif

/* For automatic removal of expired rules (with LeaseDuration) */
extern unsigned int nextruletoclean_timestamp;

#ifdef USE_PF
extern const char * anchor_name;
/* queue and tag for PF rules */
extern const char * queue;
extern const char * tag;
#endif

#ifdef USE_NETFILTER
extern const char * miniupnpd_nat_chain;
extern const char * miniupnpd_peer_chain;
extern const char * miniupnpd_forward_chain;
#ifdef ENABLE_UPNPPINHOLE
extern const char * miniupnpd_v6_filter_chain;
#endif
#endif

#ifdef ENABLE_NFQUEUE
extern int nfqueue;
extern int n_nfqix;
extern unsigned nfqix[];
#endif

/* lan addresses to listen to SSDP traffic */
extern struct lan_addr_list lan_addrs;

#ifdef ENABLE_IPV6
/* ipv6 address used for HTTP */
extern char ipv6_addr_for_http_with_brackets[64];

/* address used to bind local services */
extern struct in6_addr ipv6_bind_addr;

#endif

extern const char * minissdpdsocketpath;

/* BOOTID.UPNP.ORG and CONFIGID.UPNP.ORG */
extern unsigned int upnp_bootid;
extern unsigned int upnp_configid;

#endif

