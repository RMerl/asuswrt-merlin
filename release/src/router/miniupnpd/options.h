/* $Id: options.h,v 1.21 2012/06/29 19:26:09 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * author: Ryan Wagoner
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED

#include "config.h"

#ifndef DISABLE_CONFIG_FILE
/* enum of option available in the miniupnpd.conf */
enum upnpconfigoptions {
	UPNP_INVALID = 0,
	UPNPEXT_IFNAME = 1,		/* ext_ifname */
	UPNPEXT_IP,				/* ext_ip */
	UPNPLISTENING_IP,		/* listening_ip */
#ifdef ENABLE_IPV6
	UPNPIPV6_LISTENING_IP,		/* listening address for IPv6 */
#endif /* ENABLE_IPV6 */
	UPNPPORT,				/* "port" / "http_port" */
#ifdef ENABLE_HTTPS
	UPNPHTTPSPORT,			/* "https_port" */
#endif
	UPNPBITRATE_UP,			/* "bitrate_up" */
	UPNPBITRATE_DOWN,		/* "bitrate_down" */
	UPNPPRESENTATIONURL,	/* presentation_url */
#ifdef ENABLE_MANUFACTURER_INFO_CONFIGURATION
	UPNPFRIENDLY_NAME,		/* "friendly_name" */
	UPNPMANUFACTURER_NAME,	/* "manufacturer_name" */
	UPNPMANUFACTURER_URL,	/* "manufacturer_url" */
	UPNPMODEL_NAME,	/* "model_name" */
	UPNPMODEL_DESCRIPTION,	/* "model_description" */
	UPNPMODEL_URL,	/* "model_url" */
#endif
	UPNPNOTIFY_INTERVAL,	/* notify_interval */
	UPNPSYSTEM_UPTIME,		/* "system_uptime" */
	UPNPPACKET_LOG,			/* "packet_log" */
	UPNPUUID,				/* uuid */
	UPNPSERIAL,				/* serial */
	UPNPMODEL_NUMBER,		/* model_number */
	UPNPCLEANTHRESHOLD,		/* clean_ruleset_threshold */
	UPNPCLEANINTERVAL,		/* clean_ruleset_interval */
	UPNPENABLENATPMP,		/* enable_natpmp */
	UPNPPCPMINLIFETIME,		/* minimum lifetime for PCP mapping */
	UPNPPCPMAXLIFETIME,		/* maximum lifetime for PCP mapping */
	UPNPPCPALLOWTHIRDPARTY,		/* allow third-party requests */
#ifdef USE_NETFILTER
	UPNPFORWARDCHAIN,
	UPNPNATCHAIN,
#endif
#ifdef USE_PF
	UPNPANCHOR,				/* anchor */
	UPNPQUEUE,				/* queue */
	UPNPTAG,				/* tag */
#endif
#ifdef PF_ENABLE_FILTER_RULES
	UPNPQUICKRULES,			/* quickrules */
#endif
	UPNPSECUREMODE,			/* secure_mode */
#ifdef ENABLE_LEASEFILE
	UPNPLEASEFILE,			/* lease_file */
#endif
	UPNPMINISSDPDSOCKET,	/* minissdpdsocket */
	UPNPENABLE				/* enable_upnp */
};

/* readoptionsfile()
 * parse and store the option file values
 * returns: 0 success, -1 failure */
int
readoptionsfile(const char * fname);

/* freeoptions()
 * frees memory allocated to option values */
void
freeoptions(void);

struct option
{
	enum upnpconfigoptions id;
	const char * value;
};

extern struct option * ary_options;
extern unsigned int num_options;

#endif /* DISABLE_CONFIG_FILE */

#endif /* OPTIONS_H_INCLUDED */

