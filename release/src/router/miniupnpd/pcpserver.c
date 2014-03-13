/* $Id: pcpserver.c,v 1.12 2014/02/28 17:50:22 nanard Exp $ */
/* MiniUPnP project
 * Website : http://miniupnp.free.fr/
 * Author : Peter Tatrai

Copyright (c) 2013 by Cisco Systems, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#ifdef ENABLE_PCP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <syslog.h>

#include "pcpserver.h"
#include "macros.h"
#include "upnpglobalvars.h"
#include "pcplearndscp.h"
#include "upnpredirect.h"
#include "commonrdr.h"
#include "getifaddr.h"
#include "asyncsendto.h"
#include "pcp_msg_struct.h"

#ifdef PCP_PEER
/* TODO make this platform independent */
#include "netfilter/iptcrdr.h"
#endif

/* server specific information */
struct pcp_server_info {
	uint8_t server_version;
};

/* default server settings, highest version supported is the default */
static struct pcp_server_info this_server_info = {2};

/* structure holding information from PCP msg*/
/* all variables are in host byte order except IP addresses */
typedef struct pcp_info {
	uint8_t     version;
	uint8_t     opcode;
	uint8_t     result_code;
	uint32_t    lifetime;             /* lifetime of the mapping */
	uint32_t    epochtime;
	/* both MAP and PEER opcode specific information */
	uint8_t     protocol;
	uint16_t    int_port;
	const struct in6_addr    *int_ip; /* in network order */
	uint16_t    ext_port;
	const struct in6_addr    *ext_ip; /* Suggested external IP in network order*/
	/* PEER specific information */
#ifdef PCP_PEER
	uint16_t    peer_port;
	const struct in6_addr    *peer_ip; /* Destination IP in network order */
#endif /* PCP_PEER */

#ifdef PCP_SADSCP
	/* SADSCP specific information */
	uint8_t delay_tolerance;
	uint8_t loss_tolerance;
	uint8_t jitter_tolerance;
	uint8_t app_name_len;
	const char*   app_name;
	uint8_t sadscp_dscp;
	uint8_t matched_name;
	int8_t is_sadscp_op;
#endif

#ifdef PCP_FLOWP
	uint8_t dscp_up;
	uint8_t dscp_down;
	int flowp_present;
#endif
	uint8_t is_map_op;
	uint8_t is_peer_op;
	int thirdp_present; /* indicate presence of the options */
	int pfailure_present;
	char senderaddrstr[INET_ADDRSTRLEN];

} pcp_info_t;


#ifdef PCP_SADSCP
int get_dscp_value(pcp_info_t *pcp_msg_info) {

	unsigned int ind;

	for (ind = 0; ind < num_dscp_values; ind++) {

		if ((dscp_values_list[ind].app_name) &&
		    (!strncmp( dscp_values_list[ind].app_name,
		               pcp_msg_info->app_name, pcp_msg_info->app_name_len)) &&
		    (pcp_msg_info->delay_tolerance == dscp_values_list[ind].delay) &&
		    (pcp_msg_info->loss_tolerance == dscp_values_list[ind].loss) &&
		    (pcp_msg_info->jitter_tolerance == dscp_values_list[ind].jitter)
		   )
		{
			pcp_msg_info->sadscp_dscp = dscp_values_list[ind].dscp_value;
			pcp_msg_info->matched_name = 1;
			return 0;
		} else
		  if ((pcp_msg_info->app_name_len==0) &&
		      (dscp_values_list[ind].app_name_len==0) &&
		      (pcp_msg_info->delay_tolerance == dscp_values_list[ind].delay) &&
		      (pcp_msg_info->loss_tolerance == dscp_values_list[ind].loss) &&
		      (pcp_msg_info->jitter_tolerance == dscp_values_list[ind].jitter)
		     )
		{
			pcp_msg_info->sadscp_dscp = dscp_values_list[ind].dscp_value;
			pcp_msg_info->matched_name = 0;
			return 0;
		} else
		  if ((dscp_values_list[ind].app_name_len==0) &&
		      (pcp_msg_info->delay_tolerance == dscp_values_list[ind].delay) &&
		      (pcp_msg_info->loss_tolerance == dscp_values_list[ind].loss) &&
		      (pcp_msg_info->jitter_tolerance == dscp_values_list[ind].jitter)
		     )
		{
			pcp_msg_info->sadscp_dscp = dscp_values_list[ind].dscp_value;
			pcp_msg_info->matched_name = 0;
			return 0;
		}
	}
	//if nothing matched return Default value i.e. 0
	pcp_msg_info->sadscp_dscp = 0;
	pcp_msg_info->matched_name = 0;
	return 0;
}
#endif
/*
 * Function extracting information from common_req (common request header)
 * into pcp_msg_info.
 * @return : when no problem occurred 0 is returned, 1 otherwise and appropriate
 *          result code is assigned to pcp_msg_info->result_code to indicate
 *          what kind of error occurred
 */
static int parseCommonRequestHeader(pcp_request_t *common_req, pcp_info_t *pcp_msg_info)
{
	pcp_msg_info->version = common_req->ver ;
	pcp_msg_info->opcode = common_req->r_opcode &0x7f ;
	pcp_msg_info->lifetime = ntohl(common_req->req_lifetime);
	pcp_msg_info->int_ip = &common_req->ip;


	if ( (common_req->ver > this_server_info.server_version) ) {
		pcp_msg_info->result_code = PCP_ERR_UNSUPP_VERSION;
		return 1;
	}

	if (pcp_msg_info->lifetime > max_lifetime ) {
		pcp_msg_info->lifetime = max_lifetime;
	}

	if ( (pcp_msg_info->lifetime < min_lifetime) && (pcp_msg_info->lifetime != 0) ) {
		pcp_msg_info->lifetime = min_lifetime;
	}

	return 0;
}

#ifdef DEBUG
static void printMAPOpcodeVersion1(pcp_map_v1_t *map_buf)
{
	char map_addr[INET6_ADDRSTRLEN];
	syslog(LOG_DEBUG, "PCP MAP: v1 Opcode specific information. \n");
	syslog(LOG_DEBUG, "MAP protocol: \t\t %d\n",map_buf->protocol );
	syslog(LOG_DEBUG, "MAP int port: \t\t %d\n", ntohs(map_buf->int_port) );
	syslog(LOG_DEBUG, "MAP ext port: \t\t %d\n", ntohs(map_buf->ext_port) );
	syslog(LOG_DEBUG, "MAP Ext IP: \t\t %s\n", inet_ntop(AF_INET6,
	       &map_buf->ext_ip, map_addr, INET6_ADDRSTRLEN));
}

static void printMAPOpcodeVersion2(pcp_map_v2_t *map_buf)
{
	char map_addr[INET6_ADDRSTRLEN];
	syslog(LOG_DEBUG, "PCP MAP: v2 Opcode specific information. \n");
	syslog(LOG_DEBUG, "MAP protocol: \t\t %d\n",map_buf->protocol );
	syslog(LOG_DEBUG, "MAP int port: \t\t %d\n", ntohs(map_buf->int_port) );
	syslog(LOG_DEBUG, "MAP ext port: \t\t %d\n", ntohs(map_buf->ext_port) );
	syslog(LOG_DEBUG, "MAP Ext IP: \t\t %s\n", inet_ntop(AF_INET6,
	       &map_buf->ext_ip, map_addr, INET6_ADDRSTRLEN));
}
#endif /* DEBUG */

static int parsePCPMAP_version1(pcp_map_v1_t *map_v1,
		pcp_info_t *pcp_msg_info)
{
	pcp_msg_info->is_map_op = 1;
	pcp_msg_info->protocol = map_v1->protocol;
	pcp_msg_info->int_port = ntohs(map_v1->int_port);
	pcp_msg_info->ext_port = ntohs(map_v1->ext_port);

	pcp_msg_info->ext_ip = &(map_v1->ext_ip);

	if (pcp_msg_info->protocol == 0 && pcp_msg_info->int_port !=0 ){
		syslog(LOG_ERR, "PCP MAP: Protocol was ZERO, but internal port has non-ZERO value.");
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}
	return 0;
}

static int parsePCPMAP_version2(pcp_map_v2_t *map_v2,
		pcp_info_t *pcp_msg_info)
{
	pcp_msg_info->is_map_op = 1;
	pcp_msg_info->protocol = map_v2->protocol;
	pcp_msg_info->int_port = ntohs(map_v2->int_port);
	pcp_msg_info->ext_port = ntohs(map_v2->ext_port);

	pcp_msg_info->ext_ip = &(map_v2->ext_ip);

	if (pcp_msg_info->protocol == 0 && pcp_msg_info->int_port !=0 ) {
		syslog(LOG_ERR, "PCP MAP: Protocol was ZERO, but internal port has non-ZERO value.");
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return PCP_ERR_MALFORMED_REQUEST;
	}

	return 0;
}

#ifdef PCP_PEER
#ifdef DEBUG
static void printPEEROpcodeVersion1(pcp_peer_v1_t *peer_buf)
{
	char ext_addr[INET6_ADDRSTRLEN];
	char peer_addr[INET6_ADDRSTRLEN];
	syslog(LOG_DEBUG, "PCP PEER: v1 Opcode specific information. \n");
	syslog(LOG_DEBUG, "Protocol: \t\t %d\n",peer_buf->protocol );
	syslog(LOG_DEBUG, "Internal port: \t\t %d\n", ntohs(peer_buf->int_port) );
	syslog(LOG_DEBUG, "External IP: \t\t %s\n", inet_ntop(AF_INET6, &peer_buf->ext_ip,
	       ext_addr,INET6_ADDRSTRLEN));
	syslog(LOG_DEBUG, "External port port: \t\t %d\n", ntohs(peer_buf->ext_port) );
	syslog(LOG_DEBUG, "PEER IP: \t\t %s\n", inet_ntop(AF_INET6, &peer_buf->peer_ip,
	       peer_addr,INET6_ADDRSTRLEN));
	syslog(LOG_DEBUG, "PEER port port: \t\t %d\n", ntohs(peer_buf->peer_port) );
}

static void printPEEROpcodeVersion2(pcp_peer_v2_t *peer_buf)
{
	char ext_addr[INET6_ADDRSTRLEN];
	char peer_addr[INET6_ADDRSTRLEN];

	syslog(LOG_DEBUG, "PCP PEER: v2 Opcode specific information. \n");
	syslog(LOG_DEBUG, "Protocol: \t\t %d\n",peer_buf->protocol );
	syslog(LOG_DEBUG, "Internal port: \t\t %d\n", ntohs(peer_buf->int_port) );
	syslog(LOG_DEBUG, "External IP: \t\t %s\n", inet_ntop(AF_INET6, &peer_buf->ext_ip,
	       ext_addr,INET6_ADDRSTRLEN));
	syslog(LOG_DEBUG, "External port port: \t\t %d\n", ntohs(peer_buf->ext_port) );
	syslog(LOG_DEBUG, "PEER IP: \t\t %s\n", inet_ntop(AF_INET6, &peer_buf->peer_ip,
	       peer_addr,INET6_ADDRSTRLEN));
	syslog(LOG_DEBUG, "PEER port port: \t\t %d\n", ntohs(peer_buf->peer_port) );
}
#endif /* DEBUG */

/*
 * Function extracting information from peer_buf to pcp_msg_info
 * @return : when no problem occurred 0 is returned, 1 otherwise
 */
static int parsePCPPEER_version1(pcp_peer_v1_t *peer_buf, \
		pcp_info_t *pcp_msg_info)
{
	pcp_msg_info->is_peer_op = 1;
	pcp_msg_info->protocol = peer_buf->protocol;
	pcp_msg_info->int_port = ntohs(peer_buf->int_port);
	pcp_msg_info->ext_port = ntohs(peer_buf->ext_port);
	pcp_msg_info->peer_port = ntohs(peer_buf->peer_port);

        pcp_msg_info->ext_ip = &peer_buf->ext_ip;
        pcp_msg_info->peer_ip = &peer_buf->peer_ip;

	if (pcp_msg_info->protocol == 0 && pcp_msg_info->int_port !=0 ){
		syslog(LOG_ERR, "PCP PEER: protocol was ZERO, but internal port has non-ZERO value.");
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}
	return 0;
}

/*
 * Function extracting information from peer_buf to pcp_msg_info
 * @return : when no problem occurred 0 is returned, 1 otherwise
 */
static int parsePCPPEER_version2(pcp_peer_v2_t *peer_buf, \
		pcp_info_t *pcp_msg_info)
{
	pcp_msg_info->is_peer_op = 1;
	pcp_msg_info->protocol = peer_buf->protocol;
	pcp_msg_info->int_port = ntohs(peer_buf->int_port);
	pcp_msg_info->ext_port = ntohs(peer_buf->ext_port);
	pcp_msg_info->peer_port = ntohs(peer_buf->peer_port);

        pcp_msg_info->ext_ip = &peer_buf->ext_ip;
        pcp_msg_info->peer_ip = &peer_buf->peer_ip;


	if (pcp_msg_info->protocol == 0 && pcp_msg_info->int_port !=0 ){
		syslog(LOG_ERR, "PCP PEER: protocol was ZERO, but internal port has non-ZERO value.");
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}
	return 0;
}
#endif /* PCP_PEER */

#ifdef PCP_SADSCP
#ifdef DEBUG
static void printSADSCPOpcode(pcp_sadscp_req_t *sadscp) {
	unsigned char sadscp_tol;
	sadscp_tol = sadscp->tolerance_fields;

	syslog(LOG_DEBUG, "PCP SADSCP: Opcode specific information.\n");
	syslog(LOG_DEBUG, "Delay tolerance %d \n", (sadscp_tol>>6)&3 );
	syslog(LOG_DEBUG, "Loss tolerance %d \n",  (sadscp_tol>>4)&3);
	syslog(LOG_DEBUG, "Jitter tolerance %d \n",  (sadscp_tol>>2)&3);
	syslog(LOG_DEBUG, "RRR %d \n", sadscp_tol&3);
	syslog(LOG_DEBUG, "AppName Length %d \n", sadscp->app_name_length);
	if (sadscp->app_name) {
		syslog(LOG_DEBUG, "Application name %.*s \n", sadscp->app_name_length,
		        sadscp->app_name);
	}
}
#endif //DEBUG

static int parseSADSCP(pcp_sadscp_req_t *sadscp, pcp_info_t *pcp_msg_info) {

	pcp_msg_info->delay_tolerance = (sadscp->tolerance_fields>>6)&3;
	pcp_msg_info->loss_tolerance = (sadscp->tolerance_fields>>4)&3;
	pcp_msg_info->jitter_tolerance = (sadscp->tolerance_fields>>2)&3;

	if (pcp_msg_info->delay_tolerance == 3 ) {
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}
	if ( pcp_msg_info->loss_tolerance == 3 ) {
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}
	if ( pcp_msg_info->jitter_tolerance == 3 ) {
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1;
	}

	pcp_msg_info->app_name = sadscp->app_name;
	pcp_msg_info->app_name_len = sadscp->app_name_length;

	return 0;
}
#endif

static int parsePCPOptions(void* pcp_buf, int* remainingSize,
                           int* processedSize, pcp_info_t *pcp_msg_info)
{
	int remain = *remainingSize;
	int processed = *processedSize;
#ifdef DEBUG
	char third_addr[INET6_ADDRSTRLEN];
#endif
	unsigned short option_length;

	pcp_3rd_party_option_t* opt_3rd;
#ifdef PCP_FLOWP
	pcp_flow_priority_option_t* opt_flp;
#endif
	pcp_filter_option_t* opt_filter;
	pcp_prefer_fail_option_t* opt_prefail;
	pcp_options_hdr_t* opt_hdr;

	opt_hdr = (pcp_options_hdr_t*)(pcp_buf + processed);
	option_length = 0;

	switch (opt_hdr->code){

	case PCP_OPTION_3RD_PARTY:

		opt_3rd = (pcp_3rd_party_option_t*) (pcp_buf + processed);
		option_length = ntohs(opt_3rd->len);

		if (option_length != (sizeof(pcp_3rd_party_option_t) - sizeof(pcp_options_hdr_t)) ||
		    (int)sizeof(pcp_3rd_party_option_t) > remain) {
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			remain = 0;
			break;
		}
#ifdef DEBUG
		syslog(LOG_DEBUG, "PCP OPTION: \t Third party \n");
		syslog(LOG_DEBUG, "Third PARTY IP: \t %s\n", inet_ntop(AF_INET6,
		       &(opt_3rd->ip), third_addr, INET6_ADDRSTRLEN));
#endif
		if (pcp_msg_info->thirdp_present != 0 ) {

			syslog(LOG_ERR, "PCP: THIRD PARTY OPTION was already present. \n");
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
		}
		else {
			pcp_msg_info->thirdp_present = 1;
		}

		processed += sizeof(pcp_3rd_party_option_t);
		remain -= sizeof(pcp_3rd_party_option_t);
		break;

	case PCP_OPTION_PREF_FAIL:

		opt_prefail = (pcp_prefer_fail_option_t*)(pcp_buf+processed);
		option_length = ntohs(opt_prefail->len);

		if ( option_length != ( sizeof(pcp_prefer_fail_option_t) - sizeof(pcp_options_hdr_t)) ||
		    (int)sizeof(pcp_prefer_fail_option_t) > remain) {
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			remain = 0;
			break;
		}
#ifdef DEBUG
		syslog(LOG_DEBUG, "PCP OPTION: \t Prefer failure \n");
#endif
		if (pcp_msg_info->opcode != PCP_OPCODE_MAP) {
			syslog(LOG_DEBUG, "PCP: Unsupported OPTION for given OPCODE.\n");
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		}
		if (pcp_msg_info->pfailure_present != 0 ) {
			syslog(LOG_DEBUG, "PCP: PREFER FAILURE OPTION was already present. \n");
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
		}
		else {
			pcp_msg_info->pfailure_present = 1;
			processed += sizeof(pcp_prefer_fail_option_t);
			remain -= sizeof(pcp_prefer_fail_option_t);
		}
		break;

	case PCP_OPTION_FILTER:
		/* TODO fully implement filter */
		opt_filter = (pcp_filter_option_t*) (pcp_buf + processed);
		option_length = ntohs(opt_filter->len);

		if ( option_length != ( sizeof(pcp_filter_option_t) - sizeof(pcp_options_hdr_t)) ||
		     (int)sizeof(pcp_filter_option_t) > remain) {
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			remain = 0;
			break;
		}
#ifdef DEBUG
		syslog(LOG_DEBUG, "PCP OPTION: \t Filter\n");
#endif
		if (pcp_msg_info->opcode != PCP_OPCODE_MAP) {
			syslog(LOG_ERR, "PCP: Unsupported OPTION for given OPCODE.\n");
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		}
		processed += sizeof(pcp_filter_option_t);
		remain -= sizeof(pcp_filter_option_t);
		break;

#ifdef PCP_FLOWP
	case PCP_OPTION_FLOW_PRIORITY:

#ifdef DEBUG
		syslog(LOG_DEBUG, "PCP OPTION: \t Flow priority\n");
#endif
		opt_flp = (pcp_flow_priority_option_t*) (pcp_buf + processed);
		option_length = ntohs(opt_flp->len);

		if ( option_length != ( sizeof(pcp_flow_priority_option_t) - sizeof(pcp_options_hdr_t)) ||
		    ((int)sizeof(pcp_flow_priority_option_t) > remain) ) {
			syslog(LOG_ERR, "PCP: Error processing DSCP. sizeof %d and remaining %d . flow len %d \n",
			       (int)sizeof(pcp_flow_priority_option_t), remain, opt_flp->len);
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			remain = 0;
			break;
		}

#ifdef DEBUG
		syslog(LOG_DEBUG, "DSCP UP: \t %d \n", opt_flp->dscp_up);
		syslog(LOG_DEBUG, "DSCP DOWN: \t %d \n", opt_flp->dscp_down);
#endif
		pcp_msg_info->dscp_up = opt_flp->dscp_up;
		pcp_msg_info->dscp_down = opt_flp->dscp_down;
		pcp_msg_info->flowp_present = 1;

		processed += sizeof(pcp_flow_priority_option_t);
		remain -= sizeof(pcp_flow_priority_option_t);
		break;
#endif
	default:
		syslog(LOG_ERR, "PCP: Unrecognized PCP OPTION: %d \n", opt_hdr->code);
		remain = 0;
		break;
	}

	/* shift processed and remaining values to new values */
	*remainingSize = remain;
	*processedSize = processed;
	return pcp_msg_info->result_code;
}


static int CheckExternalAddress(pcp_info_t* pcp_msg_info)
{
	static struct in6_addr external_addr;

	if(use_ext_ip_addr) {
		if (inet_pton(AF_INET, use_ext_ip_addr,
		              ((uint32_t*)external_addr.s6_addr)+3) == 1) {
			((uint32_t*)external_addr.s6_addr)[0] = 0;
			((uint32_t*)external_addr.s6_addr)[1] = 0;
			((uint32_t*)external_addr.s6_addr)[2] = htonl(0xFFFF);
		} else if (inet_pton(AF_INET6, use_ext_ip_addr, external_addr.s6_addr)
		        != 1) {
			pcp_msg_info->result_code = PCP_ERR_NETWORK_FAILURE;
			return -1;
		}
	} else {
		if(!ext_if_name || ext_if_name[0]=='\0') {
			pcp_msg_info->result_code = PCP_ERR_NETWORK_FAILURE;
			return -1;
		}
#ifdef ENABLE_IPV6
		if(getifaddr_in6(ext_if_name, &external_addr) < 0) {
			pcp_msg_info->result_code = PCP_ERR_NETWORK_FAILURE;
			return -1;
		}
#endif
	}

	if (pcp_msg_info->ext_ip == NULL || IN6_IS_ADDR_UNSPECIFIED(pcp_msg_info->ext_ip)) {

		pcp_msg_info->ext_ip = &external_addr;

		return 0;
	}

	if (!IN6_ARE_ADDR_EQUAL(pcp_msg_info->ext_ip, &external_addr)) {
		syslog(LOG_ERR,
		        "PCP: External IP in request didn't match interface IP \n");
#ifdef DEBUG
		{
			char s[INET6_ADDRSTRLEN];
			syslog(LOG_DEBUG, "Interface IP %s \n",
			       inet_ntop(AF_INET6, &external_addr.s6_addr, s, sizeof(s)));
			syslog(LOG_DEBUG, "IP in the PCP request %s \n",
			       inet_ntop(AF_INET6, pcp_msg_info->ext_ip, s, sizeof(s)));
		}
#endif

		if (pcp_msg_info->pfailure_present) {
			pcp_msg_info->result_code = PCP_ERR_CANNOT_PROVIDE_EXTERNAL;
			return -1;
		} else {
			pcp_msg_info->ext_ip = &external_addr;
		}

	}

	return 0;
}


#ifdef PCP_PEER
static void FillSA(struct sockaddr *sa, const struct in6_addr *in6,
		uint16_t port)
{
	if (IN6_IS_ADDR_V4MAPPED(in6)) {
		struct sockaddr_in *sa4 = (struct sockaddr_in *)sa;
		sa4->sin_family = AF_INET;
		sa4->sin_addr.s_addr = ((uint32_t*)(in6)->s6_addr)[3];
		sa4->sin_port = htons(port);
	} else {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
		sa6->sin6_family = AF_INET6;
		sa6->sin6_addr = *in6;
		sa6->sin6_port = htons(port);
	}
}

static const char* inet_satop(struct sockaddr* sa, char* buf, size_t buf_len)
{
	if (sa->sa_family == AF_INET) {
		return inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), buf, buf_len);
	} else {
		return inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), buf, buf_len);
	}
}

static const char* inet_n46top(struct in6_addr* in, char* buf, size_t buf_len)
{
	if (IN6_IS_ADDR_V4MAPPED(in)) {
		return inet_ntop(AF_INET, ((uint32_t*)(in->s6_addr))+3, buf, buf_len);
	} else {
		return inet_ntop(AF_INET6, in, buf, buf_len);
	}
}

static int CreatePCPPeer(pcp_info_t *pcp_msg_info)
{
	struct sockaddr_storage intip;
	struct sockaddr_storage peerip;
	struct sockaddr_storage extip;
	struct sockaddr_storage ret_extip;

	uint8_t  proto = pcp_msg_info->protocol;

	uint16_t eport = pcp_msg_info->ext_port;  /* public port */

	FillSA((struct sockaddr*)&intip, pcp_msg_info->int_ip,
	    pcp_msg_info->int_port);
	FillSA((struct sockaddr*)&peerip, pcp_msg_info->peer_ip,
	    pcp_msg_info->peer_port);
	FillSA((struct sockaddr*)&extip, pcp_msg_info->ext_ip,
	    eport);

	/* check if connection with given peer exists, if it was */
	/* already established use this external port */
	if (get_nat_ext_addr( (struct sockaddr*)&intip, (struct sockaddr*)&peerip,
	    proto, (struct sockaddr*)&ret_extip) == 1) {
		if (ret_extip.ss_family == AF_INET) {
			struct sockaddr_in* ret_ext4 = (struct sockaddr_in*)&ret_extip;
			uint16_t ret_eport = ntohs(ret_ext4->sin_port);
			eport = ret_eport;
		} else if (ret_extip.ss_family == AF_INET6) {
			struct sockaddr_in6* ret_ext6 = (struct sockaddr_in6*)&ret_extip;
			uint16_t ret_eport = ntohs(ret_ext6->sin6_port);
			eport = ret_eport;
		} else {
			pcp_msg_info->result_code = PCP_ERR_CANNOT_PROVIDE_EXTERNAL;
			return 0;
		}
	}
	/* Create Peer Mapping */
	{
		char desc[64];
		char peerip_s[INET_ADDRSTRLEN], extip_s[INET_ADDRSTRLEN];
		time_t timestamp = time(NULL) + pcp_msg_info->lifetime;

		if (eport == 0) {
			eport = pcp_msg_info->int_port;
		}

		snprintf(desc, sizeof(desc), "PCP %hu %s",
		    eport, (proto==IPPROTO_TCP)?"tcp":"udp");

		inet_satop((struct sockaddr*)&peerip, peerip_s, sizeof(peerip_s));
		inet_satop((struct sockaddr*)&extip, extip_s, sizeof(extip_s));

#ifdef PCP_FLOWP
		if (pcp_msg_info->flowp_present && pcp_msg_info->dscp_up) {
			if (add_peer_dscp_rule2(ext_if_name, peerip_s,
			        pcp_msg_info->peer_port, pcp_msg_info->dscp_up,
			        pcp_msg_info->senderaddrstr, pcp_msg_info->int_port,
			        proto, desc, timestamp) < 0 ) {
				syslog(LOG_ERR, "PCP: failed to add flowp upstream mapping %s %s:%hu->%s:%hu '%s'",
				        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
				        pcp_msg_info->senderaddrstr,
				        pcp_msg_info->int_port,
				        peerip_s,
				        pcp_msg_info->peer_port,
				        desc);
				pcp_msg_info->result_code = PCP_ERR_NO_RESOURCES;
			}
		}

		if (pcp_msg_info->flowp_present && pcp_msg_info->dscp_down) {
			if (add_peer_dscp_rule2(ext_if_name,  pcp_msg_info->senderaddrstr,
			        pcp_msg_info->int_port, pcp_msg_info->dscp_down,
			        peerip_s, pcp_msg_info->peer_port, proto, desc, timestamp)
			          < 0 ) {
				syslog(LOG_ERR, "PCP: failed to add flowp downstream mapping %s %s:%hu->%s:%hu '%s'",
				        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
				        pcp_msg_info->senderaddrstr,
				        pcp_msg_info->int_port,
				        peerip_s,
				        pcp_msg_info->peer_port,
				        desc);
				pcp_msg_info->result_code = PCP_ERR_NO_RESOURCES;
			}
		}
#endif
		/* TODO: add upnp function for PI */
		if (add_peer_redirect_rule2(ext_if_name,
		        peerip_s,
		        pcp_msg_info->peer_port,
		        extip_s,
		        eport,
		        pcp_msg_info->senderaddrstr,
		        pcp_msg_info->int_port,
		        pcp_msg_info->protocol,
		        desc,
		        timestamp) < 0 ) {

			syslog(LOG_ERR, "PCP PEER: failed to add peer mapping %s %s:%hu(%hu)->%s:%hu '%s'",
			        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
			        pcp_msg_info->senderaddrstr,
			        pcp_msg_info->int_port,
			        eport,
			        peerip_s,
			        pcp_msg_info->peer_port,
			        desc);

			pcp_msg_info->result_code = PCP_ERR_NO_RESOURCES;

			return 0;
		} else {
			pcp_msg_info->ext_port = eport;
			syslog(LOG_INFO, "PCP PEER: added mapping %s %s:%hu(%hu)->%s:%hu '%s'",
				        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
				        pcp_msg_info->senderaddrstr,
				        pcp_msg_info->int_port,
				        eport,
				        peerip_s,
				        pcp_msg_info->peer_port,
				        desc);
		}
	}

	return 1;
}

static void DeletePCPPeer(pcp_info_t *pcp_msg_info)
{
	uint16_t iport = pcp_msg_info->int_port;  /* private port */
	uint16_t rport = pcp_msg_info->peer_port;  /* private port */
	uint8_t  proto = pcp_msg_info->protocol;
	char rhost[INET6_ADDRSTRLEN];
	int r=-1;

	/* remove requested mappings for this client */
	int index = 0;
	unsigned short eport2, iport2, rport2;
	char iaddr2[INET6_ADDRSTRLEN], rhost2[INET6_ADDRSTRLEN];
	int proto2;
	char desc[64];
	unsigned int timestamp;

	inet_n46top((struct in6_addr*)pcp_msg_info->peer_ip, rhost, sizeof(rhost));

	while(get_peer_rule_by_index(index, 0,
		  &eport2, iaddr2, sizeof(iaddr2),
		  &iport2, &proto2,
		  desc, sizeof(desc),
		  rhost2, sizeof(rhost2), &rport2, &timestamp, 0, 0) >= 0) {
		if((0 == strncmp(iaddr2, pcp_msg_info->senderaddrstr, sizeof(iaddr2)))
		  && (0 == strncmp(rhost2, rhost, sizeof(rhost2)))
		  && (proto2==proto)
		  && (0 == strncmp(desc, "PCP", sizeof("PCP")-1))
		  && (iport2==iport) && (rport2==rport)) {
			r = _upnp_delete_redir(eport2, proto2);
			if(r<0) {
				syslog(LOG_ERR, "PCP PEER: failed to remove peer mapping");
				index++;
			} else {
				syslog(LOG_INFO, "PCP PEER: %s port %hu peer mapping removed",
				       proto2==IPPROTO_TCP?"TCP":"UDP", eport2);
			}
		} else {
			index++;
		}
	}
	if (r==-1) {
		syslog(LOG_ERR, "PCP PEER: Failed to remove PCP mapping internal port %hu, protocol %s",
		       iport, (pcp_msg_info->protocol == IPPROTO_TCP)?"TCP":"UDP");
		pcp_msg_info->result_code = PCP_ERR_NO_RESOURCES;
	}
}
#endif /* PCP_PEER */

static void CreatePCPMap(pcp_info_t *pcp_msg_info)
{
	char desc[64];
	char iaddr_old[INET_ADDRSTRLEN];
	uint16_t iport_old;
	unsigned int timestamp;
	int r=0;

	if (pcp_msg_info->ext_port == 0) {
		pcp_msg_info->ext_port = pcp_msg_info->int_port;
	}
	do {
		r = get_redirect_rule(ext_if_name,
		                  pcp_msg_info->ext_port,
		                  pcp_msg_info->protocol,
		                  iaddr_old, sizeof(iaddr_old),
		                  &iport_old, 0, 0, 0, 0,
		                  &timestamp, 0, 0);

		if(r==0) {
			if((strncmp(pcp_msg_info->senderaddrstr, iaddr_old,
			   sizeof(iaddr_old))!=0)
			   || (pcp_msg_info->int_port != iport_old)) {
				/* redirection already existing */
				if (pcp_msg_info->pfailure_present) {
					pcp_msg_info->result_code = PCP_ERR_CANNOT_PROVIDE_EXTERNAL;
					return;
				}
			} else {
				syslog(LOG_INFO, "port %hu %s already redirected to %s:%hu, replacing",
				       pcp_msg_info->ext_port, (pcp_msg_info->protocol==IPPROTO_TCP)?"tcp":"udp",
				       iaddr_old, iport_old);
				/* remove and then add again */
				if (_upnp_delete_redir(pcp_msg_info->ext_port,
						pcp_msg_info->protocol)==0) {
					break;
				}
			}
			pcp_msg_info->ext_port++;
		}
	} while (r==0);

	timestamp = time(NULL) + pcp_msg_info->lifetime;

	if ((pcp_msg_info->ext_port == 0) ||
	    (IN6_IS_ADDR_V4MAPPED(pcp_msg_info->int_ip) &&
	      (!check_upnp_rule_against_permissions(upnppermlist,
	               num_upnpperm, pcp_msg_info->ext_port,
	               ((struct in_addr*)pcp_msg_info->int_ip->s6_addr)[3],
	               pcp_msg_info->int_port)))) {
		pcp_msg_info->result_code = PCP_ERR_CANNOT_PROVIDE_EXTERNAL;
		return;
	}

	snprintf(desc, sizeof(desc), "PCP %hu %s",
	     pcp_msg_info->ext_port,
	     (pcp_msg_info->protocol==IPPROTO_TCP)?"tcp":"udp");

	if(upnp_redirect_internal(NULL,
	        pcp_msg_info->ext_port,
	        pcp_msg_info->senderaddrstr,
	        pcp_msg_info->int_port,
	        pcp_msg_info->protocol,
	        desc,
	        timestamp) < 0) {

		syslog(LOG_ERR, "PCP MAP: Failed to add mapping %s %hu->%s:%hu '%s'",
		        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
		        pcp_msg_info->ext_port,
		        pcp_msg_info->senderaddrstr,
		        pcp_msg_info->int_port,
		        desc);

	} else {
		syslog(LOG_INFO, "PCP MAP: added mapping %s %hu->%s:%hu '%s'",
		        (pcp_msg_info->protocol==IPPROTO_TCP)?"TCP":"UDP",
		        pcp_msg_info->ext_port,
		        pcp_msg_info->senderaddrstr,
		        pcp_msg_info->int_port,
		        desc);
	}
}

static void DeletePCPMap(pcp_info_t *pcp_msg_info)
{
	uint16_t iport = pcp_msg_info->int_port;  /* private port */
	uint8_t  proto = pcp_msg_info->protocol;
	int r=-1;
	/* remove the mapping */
	/* remove all the mappings for this client */
	int index = 0;
	unsigned short eport2, iport2;
	char iaddr2[16];
	int proto2;
	char desc[64];
	unsigned int timestamp;

	/* iterate through all rules and delete the requested ones */
	while(get_redirect_rule_by_index(index, 0,
	      &eport2, iaddr2, sizeof(iaddr2),
	      &iport2, &proto2,
	      desc, sizeof(desc),
	      0, 0, &timestamp, 0, 0) >= 0) {

		if(0 == strncmp(iaddr2, pcp_msg_info->senderaddrstr, sizeof(iaddr2))
		  && (proto2==proto)
		  && (0 == strncmp(desc, "PCP", 3)) /* starts with PCP */
		  && ((iport2==iport) || (iport==0))) {

			r = _upnp_delete_redir(eport2, proto2);
			if(r<0) {
				syslog(LOG_ERR, "PCP: failed to remove port mapping");
				index++;
			} else {
				syslog(LOG_INFO, "PCP: %s port %hu mapping removed",
				       proto2==IPPROTO_TCP?"TCP":"UDP", eport2);
			}
		} else {
			index++;
		}
	}
	if (r==-1) {
		syslog(LOG_ERR, "Failed to remove PCP mapping internal port %hu, protocol %s",
		       iport, (pcp_msg_info->protocol == IPPROTO_TCP)?"TCP":"UDP");
		pcp_msg_info->result_code = PCP_ERR_NO_RESOURCES;
	}
}

static int ValidatePCPMsg(pcp_info_t *pcp_msg_info)
{
	if (pcp_msg_info->result_code) {
		return 0;
	}

	if (pcp_msg_info->protocol == 0 && pcp_msg_info->int_port !=0 ){
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 0;
	}

	if (pcp_msg_info->pfailure_present) {
		if ( (IN6_IS_ADDR_UNSPECIFIED(pcp_msg_info->ext_ip) ||
		      ((IN6_IS_ADDR_V4MAPPED(pcp_msg_info->ext_ip)) &&
		       (((uint32_t*)pcp_msg_info->ext_ip->s6_addr)[3] == 0))) &&
		     (pcp_msg_info->ext_port == 0)
		   )
		{
			pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			return 0;
		}
	}

	if (CheckExternalAddress(pcp_msg_info)) {
		return 0;
	}

	return 1;
}

/*
 * return value indicates whether the request is valid or not.
 * Based on the return value simple response can be formed.
 */
static int processPCPRequest(void * req, int req_size, pcp_info_t *pcp_msg_info)
{
	int remainingSize;
	int processedSize;

	pcp_request_t* common_req;
	pcp_map_v1_t* map_v1;
	pcp_map_v2_t* map_v2;
#ifdef PCP_PEER
	pcp_peer_v1_t* peer_v1;
	pcp_peer_v2_t* peer_v2;
#endif

#ifdef PCP_SADSCP
	pcp_sadscp_req_t* sadscp;
#endif
	/* start with PCP_SUCCESS as result code, if everything is OK value will be unchanged */
	pcp_msg_info->result_code = PCP_SUCCESS;

	remainingSize = req_size;
	processedSize = 0;

	/* discard request that exceeds maximal length,
	   or that is shorter than PCP_MIN_LEN (=24)
	   or that is not the multiple of 4 */
	if (req_size < 3)
		return 0; /* ignore msg */

	if (req_size < PCP_MIN_LEN) {
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1; /* send response */
	}

	if ( (req_size > PCP_MAX_LEN) || ( (req_size & 3) != 0)) {
		syslog(LOG_ERR, "PCP: Size of PCP packet(%d) is larger than %d bytes or "
		       "the size is not multiple of 4.\n", req_size, PCP_MAX_LEN);
		pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
		return 1; /* send response */
	}

	/* first print out info from common request header */
	common_req = (pcp_request_t*)req;

	if (parseCommonRequestHeader(common_req, pcp_msg_info) ) {
		return 1;
	}

	remainingSize -= sizeof(pcp_request_t);
	processedSize += sizeof(pcp_request_t);

	if (common_req->ver == 1) {
		/* legacy PCP version 1 support */
		switch ( common_req->r_opcode & 0x7F ) {
		case PCP_OPCODE_MAP:

			remainingSize -= sizeof(pcp_map_v1_t);
			if (remainingSize < 0) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
				return pcp_msg_info->result_code;
			}

			map_v1 = (pcp_map_v1_t*)(req + processedSize);
#ifdef DEBUG
			printMAPOpcodeVersion1(map_v1);
#endif /* DEBUG */
			if ( parsePCPMAP_version1(map_v1, pcp_msg_info) ) {
				return pcp_msg_info->result_code;
			}

			processedSize += sizeof(pcp_map_v1_t);

			while (remainingSize > 0) {
				parsePCPOptions(req, &remainingSize, &processedSize, pcp_msg_info);
			}
			if (ValidatePCPMsg(pcp_msg_info)) {
				if (pcp_msg_info->lifetime == 0) {
					DeletePCPMap(pcp_msg_info);
				} else {
					CreatePCPMap(pcp_msg_info);
				}
			} else {
				syslog(LOG_ERR, "PCP: Invalid PCP v1 MAP message.");
			}


			break;

#ifdef PCP_PEER
		case PCP_OPCODE_PEER:

			remainingSize -= sizeof(pcp_peer_v1_t);
			if (remainingSize < 0) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
				return pcp_msg_info->result_code;
			}
			peer_v1 = (pcp_peer_v1_t*)(req + processedSize);

#ifdef DEBUG
			printPEEROpcodeVersion1(peer_v1);
#endif /* DEBUG */
			if ( parsePCPPEER_version1(peer_v1, pcp_msg_info) ) {
				 return pcp_msg_info->result_code;
			}

			processedSize += sizeof(pcp_peer_v1_t);

			while (remainingSize > 0) {
				parsePCPOptions(req, &remainingSize, &processedSize, pcp_msg_info);
			}

			if (ValidatePCPMsg(pcp_msg_info)) {
				if (pcp_msg_info->lifetime == 0) {
					DeletePCPPeer(pcp_msg_info);
				} else {
					CreatePCPPeer(pcp_msg_info);
				}
			} else {
				syslog(LOG_ERR, "PCP: Invalid PCP v1 PEER message.");
			}


			break;
#endif /* PCP_PEER */
		default:
			pcp_msg_info->result_code = PCP_ERR_UNSUPP_OPCODE;
			break;
		}

	} else if (common_req->ver == 2) {
		/* RFC 6887 PCP support
		 * http://tools.ietf.org/html/rfc6887 */
		switch ( common_req->r_opcode & 0x7F) {
		case PCP_OPCODE_ANNOUNCE:
			/* should check PCP Client's IP Address in request */
			/* see http://tools.ietf.org/html/rfc6887#section-14.1 */
			break;
		case PCP_OPCODE_MAP:

			remainingSize -= sizeof(pcp_map_v2_t);
			if (remainingSize < 0) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
				return pcp_msg_info->result_code;
			}

			map_v2 = (pcp_map_v2_t*)(req + processedSize);

#ifdef DEBUG
			printMAPOpcodeVersion2(map_v2);
#endif /* DEBUG */

			if (parsePCPMAP_version2(map_v2, pcp_msg_info) ) {
				return pcp_msg_info->result_code;
			}
			processedSize += sizeof(pcp_map_v2_t);

			while (remainingSize > 0) {
				parsePCPOptions(req, &remainingSize, &processedSize, pcp_msg_info);
			}

			if (ValidatePCPMsg(pcp_msg_info)) {
				if (pcp_msg_info->lifetime == 0) {
					DeletePCPMap(pcp_msg_info);
				} else {
					CreatePCPMap(pcp_msg_info);
				}
			} else {
				syslog(LOG_ERR, "PCP: Invalid PCP v2 MAP message.");
			}


			break;

#ifdef PCP_PEER
		case PCP_OPCODE_PEER:

			remainingSize -= sizeof(pcp_peer_v2_t);
			if (remainingSize < 0) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
				return pcp_msg_info->result_code;
			}
			peer_v2 = (pcp_peer_v2_t*)(req + processedSize);

#ifdef DEBUG
			printPEEROpcodeVersion2(peer_v2);
#endif /* DEBUG */
			parsePCPPEER_version2(peer_v2, pcp_msg_info);
			processedSize += sizeof(pcp_peer_v2_t);

			if (pcp_msg_info->result_code != 0) {
				return pcp_msg_info->result_code;
			}

			while (remainingSize > 0) {
				parsePCPOptions(req, &remainingSize, &processedSize, pcp_msg_info);
			}

			if (ValidatePCPMsg(pcp_msg_info)) {
				if (pcp_msg_info->lifetime == 0) {
					DeletePCPPeer(pcp_msg_info);
				} else {
					CreatePCPPeer(pcp_msg_info);
				}
			} else {
				syslog(LOG_ERR, "PCP: Invalid PCP v2 PEER message.");
			}

		break;
#endif /* PCP_PEER */

#ifdef PCP_SADSCP
		case PCP_OPCODE_SADSCP:
			remainingSize -= sizeof(pcp_sadscp_req_t);
			if (remainingSize < 0) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_REQUEST;
				return pcp_msg_info->result_code;
			}

			sadscp = (pcp_sadscp_req_t*)(req + processedSize);

			if (sadscp->app_name_length > remainingSize) {
				pcp_msg_info->result_code = PCP_ERR_MALFORMED_OPTION;
			}

#ifdef DEBUG
			printSADSCPOpcode(sadscp);
#endif
			if (parseSADSCP(sadscp, pcp_msg_info)) {
				return pcp_msg_info->result_code;
			}

			get_dscp_value(pcp_msg_info);

			processedSize += sizeof(pcp_sadscp_req_t);

			break;
#endif
		default:
			pcp_msg_info->result_code = PCP_ERR_UNSUPP_OPCODE;
			break;
		}
	} else {
		pcp_msg_info->result_code = PCP_ERR_UNSUPP_VERSION;
		return pcp_msg_info->result_code;
	}
	return 1;
}


static void createPCPResponse(unsigned char *response, pcp_info_t *pcp_msg_info)
{
	pcp_response_t *resp = (pcp_response_t*)response;

	resp->reserved = 0;
	resp->reserved1[0]=0;
	resp->reserved1[1]=0;
	resp->reserved1[2]=0;
	if (pcp_msg_info->result_code == PCP_ERR_UNSUPP_VERSION ) {
		/* highest supported version */
		resp->ver = this_server_info.server_version;
	} else {
		resp->ver = pcp_msg_info->version;
	}

	resp->r_opcode |= 0x80;
	resp->result_code = pcp_msg_info->result_code;
	resp->epochtime = htonl(time(NULL) - startup_time);
	switch (pcp_msg_info->result_code) {
	/*long lifetime errors*/
	case PCP_ERR_UNSUPP_VERSION:
	case PCP_ERR_NOT_AUTHORIZED:
	case PCP_ERR_MALFORMED_REQUEST:
	case PCP_ERR_UNSUPP_OPCODE:
	case PCP_ERR_UNSUPP_OPTION:
	case PCP_ERR_MALFORMED_OPTION:
	case PCP_ERR_UNSUPP_PROTOCOL:
	case PCP_ERR_ADDRESS_MISMATCH:
	case PCP_ERR_CANNOT_PROVIDE_EXTERNAL:
	case PCP_ERR_EXCESSIVE_REMOTE_PEERS:
		resp->lifetime = 0;
		break;

	case PCP_ERR_NETWORK_FAILURE:
	case PCP_ERR_NO_RESOURCES:
	case PCP_ERR_USER_EX_QUOTA:
		resp->lifetime = htonl(30);
		break;

	case PCP_SUCCESS:
	default:
		resp->lifetime = htonl(pcp_msg_info->lifetime);
		break;
	}

	if (resp->r_opcode == 0x81) { /* MAP response */
		if (resp->ver == 1 ) {
			pcp_map_v1_t *mapr = (pcp_map_v1_t *)resp->next_data;
			mapr->ext_ip = *pcp_msg_info->ext_ip;
			mapr->ext_port = htons(pcp_msg_info->ext_port);
			mapr->int_port = htons(pcp_msg_info->int_port);
		}
		else if (resp->ver == 2 ) {
			pcp_map_v2_t *mapr = (pcp_map_v2_t *)resp->next_data;
			mapr->ext_ip = *pcp_msg_info->ext_ip;
			mapr->ext_port = htons(pcp_msg_info->ext_port);
			mapr->int_port = htons(pcp_msg_info->int_port);
		}
	}
#ifdef PCP_PEER
	else if (resp->r_opcode == 0x82) { /* PEER response */
		if (resp->ver == 1 ){
			pcp_peer_v1_t* peer_resp = (pcp_peer_v1_t*)resp->next_data;
			peer_resp->ext_port = htons(pcp_msg_info->ext_port);
			peer_resp->int_port = htons(pcp_msg_info->int_port);
			peer_resp->peer_port = htons(pcp_msg_info->peer_port);
			peer_resp->ext_ip = *pcp_msg_info->ext_ip;
		}
		else if (resp->ver == 2 ){
			pcp_peer_v2_t* peer_resp = (pcp_peer_v2_t*)resp->next_data;
			peer_resp->ext_port = htons(pcp_msg_info->ext_port);
			peer_resp->int_port = htons(pcp_msg_info->int_port);
			peer_resp->peer_port = htons(pcp_msg_info->peer_port);
			peer_resp->ext_ip = *pcp_msg_info->ext_ip;
		}
	}
#endif /* PCP_PEER */

#ifdef PCP_SADSCP
	else if (resp->r_opcode == 0x83) { /*SADSCP response*/
		pcp_sadscp_resp_t *sadscp_resp = (pcp_sadscp_resp_t*)resp->next_data;
		sadscp_resp->a_r_dscp_value = pcp_msg_info->matched_name<<7;
		sadscp_resp->a_r_dscp_value &= ~(1<<6);
		sadscp_resp->a_r_dscp_value |= (pcp_msg_info->sadscp_dscp & PCP_SADSCP_MASK);
		memset(sadscp_resp->reserved, 0, sizeof(sadscp_resp->reserved));
	}
#endif /* PCP_SADSCP */
}

int ProcessIncomingPCPPacket(int s, unsigned char *buff, int len,
                             struct sockaddr_in *senderaddr)
{
	pcp_info_t pcp_msg_info;

	memset(&pcp_msg_info, 0, sizeof(pcp_info_t));

	if(!inet_ntop(AF_INET, &senderaddr->sin_addr,
	              pcp_msg_info.senderaddrstr,
	              sizeof(pcp_msg_info.senderaddrstr))) {
		syslog(LOG_ERR, "inet_ntop(pcpserver): %m");
	}
	syslog(LOG_DEBUG, "PCP request received from %s:%hu %dbytes",
	       pcp_msg_info.senderaddrstr, ntohs(senderaddr->sin_port), len);

	if(buff[1] & 128) {
		/* discarding PCP responses silently */
		return 0;
	}

	if (processPCPRequest(buff, len, &pcp_msg_info) ) {

		createPCPResponse(buff, &pcp_msg_info);

		if(len < PCP_MIN_LEN)
			len = PCP_MIN_LEN;
		else
			len = (len + 3) & ~3;	/* round up resp. length to multiple of 4 */
		len = sendto_or_schedule(s, buff, len, 0,
		           (struct sockaddr *)senderaddr, sizeof(struct sockaddr_in));
		if( len < 0 ) {
			syslog(LOG_ERR, "sendto(pcpserver): %m");
		}
	}

	return 0;
}
#endif /*ENABLE_PCP*/
