/* $Id: upnpglobalvars.c,v 1.35 2014/03/10 11:04:53 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2006-2014 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <sys/types.h>
#include <netinet/in.h>

#include "config.h"
#include "upnpglobalvars.h"
#include "upnpdescstrings.h"

/* network interface for internet */
const char * ext_if_name = 0;

/* file to store leases */
#ifdef ENABLE_LEASEFILE
const char* lease_file = 0;
#endif

/* forced ip address to use for this interface
 * when NULL, getifaddr() is used */
const char * use_ext_ip_addr = 0;

unsigned long downstream_bitrate = 0;
unsigned long upstream_bitrate = 0;

/* startup time */
time_t startup_time = 0;

#ifdef ENABLE_PCP
/* for PCP */
unsigned long int min_lifetime = 120;
unsigned long int max_lifetime = 86400;
#endif

int runtime_flags = 0;

const char * pidfilename = "/var/run/miniupnpd.pid";

char uuidvalue_igd[] = "uuid:00000000-0000-0000-0000-000000000000";
char uuidvalue_wan[] = "uuid:00000000-0000-0000-0000-000000000000";
char uuidvalue_wcd[] = "uuid:00000000-0000-0000-0000-000000000000";
char serialnumber[SERIALNUMBER_MAX_LEN] = "00000000";

char modelnumber[MODELNUMBER_MAX_LEN] = "1";

/* presentation url :
 * http://nnn.nnn.nnn.nnn:ppppp/  => max 30 bytes including terminating 0 */
char presentationurl[PRESENTATIONURL_MAX_LEN];

#ifdef ENABLE_MANUFACTURER_INFO_CONFIGURATION
/* friendly name for root devices in XML description */
char friendly_name[FRIENDLY_NAME_MAX_LEN] = OS_NAME " router";

/* manufacturer name for root devices in XML description */
char manufacturer_name[MANUFACTURER_NAME_MAX_LEN] = ROOTDEV_MANUFACTURER;

/* manufacturer url for root devices in XML description */
char manufacturer_url[MANUFACTURER_URL_MAX_LEN] = ROOTDEV_MANUFACTURERURL;

/* model name for root devices in XML description */
char model_name[MODEL_NAME_MAX_LEN] = ROOTDEV_MODELNAME;

/* model description for root devices in XML description */
char model_description[MODEL_DESCRIPTION_MAX_LEN] = ROOTDEV_MODELDESCRIPTION;

/* model url for root devices in XML description */
char model_url[MODEL_URL_MAX_LEN] = ROOTDEV_MODELURL;
#endif

/* UPnP permission rules : */
struct upnpperm * upnppermlist = 0;
unsigned int num_upnpperm = 0;

#ifdef PCP_SADSCP
struct dscp_values* dscp_values_list = 0;
unsigned int num_dscp_values = 0;
#endif /*PCP_SADSCP*/

/* For automatic removal of expired rules (with LeaseDuration) */
unsigned int nextruletoclean_timestamp = 0;

#ifdef USE_PF
const char * anchor_name = "miniupnpd";
const char * queue = 0;
const char * tag = 0;
#endif

#ifdef USE_NETFILTER
/* chain name to use, both in the nat table
 * and the filter table */
const char * miniupnpd_nat_chain = "UPNP";
const char * miniupnpd_forward_chain = "UPNP";
const char * miniupnpd_peer_chain = "UPNP-PCP-PEER";
#ifdef ENABLE_6FC_SERVICE
const char * miniupnpd_v6_filter_chain = "UPNP";
#endif

#endif
#ifdef ENABLE_NFQUEUE
int nfqueue = -1;
int n_nfqix = 0;
unsigned nfqix[MAX_LAN_ADDR];
#endif
struct lan_addr_list lan_addrs;

#ifdef ENABLE_IPV6
/* ipv6 address used for HTTP */
char ipv6_addr_for_http_with_brackets[64];
#endif

/* Path of the Unix socket used to communicate with MiniSSDPd */
const char * minissdpdsocketpath = "/var/run/minissdpd.sock";

/* BOOTID.UPNP.ORG and CONFIGID.UPNP.ORG */
unsigned int upnp_bootid = 1;
unsigned int upnp_configid = 1337;

