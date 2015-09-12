/*
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <sys/time.h>
#include "epivers.h"
#include "trace.h"
#include "dsp.h"
#include "wlu_api.h"
#include "bcm_gas.h"
#include "bcm_encode_ie.h"
#include "bcm_encode_anqp.h"
#include "bcm_encode_hspot_anqp.h"
#include "bcm_encode_wnm.h"
#include "bcm_encode_qos.h"
#include "bcm_decode_qos.h"
#include "bcm_decode_anqp.h"
#include "bcm_decode_hspot_anqp.h"
#include "bcm_decode_ie.h"
#include "tcp_srv.h"
#include "proto/bcmeth.h"
#include "proto/bcmevent.h"
#include "proto/802.11.h"
#include "common_utils.h"
#include "passpoint_nvparse.h"
#include "hspotap.h"

/* ---------------------------------- Constatnts & Macros  ---------------------------------- */
#define BUILT_IN_FILTER

/* enable testing mode */
#define TESTING_MODE			0

#define MAX_IF_COUNT			16

/* query request buffer size */
#define QUERY_REQUEST_BUFFER		(64 * 1024)

/* Proxy ARP for WNM bit mask */
#define WNM_DEFAULT_BITMASK		(WL_WNM_BSSTRANS | WL_WNM_NOTIF)

/* General Strings */
#define TRUE_S				"TRUE"
#define FALSE_S				"FALSE"

/* Mode Strings */
#define HSMODE_HELP			"-help"
#define HSMODE_DEBUG			"-debug"
#define HSMODE_VERBOSE			"-verbose"
#define HSMODE_DGAF			"-dgaf"
#define HSMODE_TCP_PORT			"-tcp_port"
#define HSMODE_TEST			"-test"
#define HSMODE_RESPSIZE			"-respSize"
#define HSMODE_GAS4FRAMESON		"-gas4FramesOn"

/* IOVAR Strings */
#define HSIOV_WL_BLOCK_PING		"wl_block_ping"
#define HSIOV_WL_BLOCK_STA		"wl_block_sta"
#define HSIOV_WL_AP_ISOLATE		"wl_ap_isolate"
#define HSIOV_WL_DHCP_UNICAST		"wl_dhcp_unicast"
#define HSIOV_WL_BLOCK_MULTICAST	"wl_block_multicast"
#define HSIOV_WL_GTK_PER_STA		"wl_gtk_per_sta"
#define HSIOV_WL_WIFIACTION		"wl_wifiaction"
#define HSIOV_WL_WNM_URL		"wl_wnm_url"
#define HSIOV_WL_WNM_BSSTRANS_REQ	"wl_wnm_bsstrans_req"
#define HSIOV_WL_BSSLOAD		"wl_bssload"
#define HSIOV_WL_BSSLOADSTATIC		"wl_bssload_static"
#define HSIOV_WL_DLS			"wl_dls"
#define HSIOV_WL_WNM			"wl_wnm"
#define HSIOV_WL_INTERWORKING		"wl_interworking"
#define HSIOV_WL_PROBERESP_SW		"wl_probresp_sw"
#define HSIOV_WL_BLOCK_TDLS		"wl_block_tdls"
#define HSIOV_WL_DLS_REJECT		"wl_dls_reject"
#define HSIOV_WL_TDLS_ENABLE		"wl_tdls_enable"
#define HSIOV_WL_GRAT_ARP		"wl_grat_arp"
#define HSIOV_WL_PROXY_ARP		"proxy_arp"
#define HSIOV_WL_DHD_PROXY_ARP		"DHD proxy_arp"

/* IE Strings */
#define IE_IW				"IW"
#define IE_AP				"AP"
#define IE_RC				"RC"
#define IE_OSEN				"OSEN"
#define IE_QOSMAPSET			"QOS_MAP_SET"

/* Print Strings */
#define PRNT_FLG			"%s: %d\n"
#define PRNT_STR			"%s: %s\n"
#define PRNT_BYTES_RED_FM_FILE		"read bytes from :%d,%s\n"
#define PRNT_QUERY_REQ			"query request"
#define PRNT_RX_MSG			"received: %s\n"
#define PRNT_TAB_LF			" \t\n"
#define PRNT_ICON_FULLPATH		"Icon Path name:%s \n"
#define PRNT_HSPOTAP_VER		"\n"\
					"Passpoint R2 - version %s\n"\
					"Copyright Broadcom Corporation\n"

#define PRNT_HELP_MODE			"\n"\
			" -debug			enable debug output\n"\
			" -verbose			enable verbose output\n"\
			" -help				print this menu\n"\
			" -dgaf				disable DGAF\n"\
			" -tcp_port <port>	Run hspotap in CLI mode\n"\
			"\n"\
			"To redirect to file use 'tee' "\
			"(eg. %s -d | tee log.txt).\n\n"

#define PRNT_TIMESTAMP_BGN		"HS_TRACE : Begin Timestamp "\
			"for starting process_anqp_query_request : %llu\n"

#define PRNT_TIMESTAMP_END		"HS_TRACE : End   Timestamp "\
			"for starting process_anqp_query_request : %llu\n"\
"===================================================== \n"\
"HS_TRACE : Final: Timestamp for process_anqp_query_request : %llu\n"\
"===================================================== \n"


#define PRNT_HELP			"\n"\
"==============================================================================\n"\
"\t\t\tHspotAP Application - CLI Commands \n"\
"==============================================================================\n"\
" Command 01\t: interface <interface_name> \n"\
	" Example\t: interface eth1 \n"\
	" Purpose\t: Make an interface active, so all following CLI commands \n"\
	" \t\t	goes on this interface, used to make primary \n"\
	" \t\t	interfaces(wl0,wl1) as current interface \n"\
"------------------------------------------------------------------------------\n"\
" Command 02\t: bss <BSSID> \n"\
	" Example\t: bss 00:11:22:33:44:55:66 \n"\
	" Purpose\t: Make a BSSID(MAC) active, so all following CLI commands \n"\
	" \t\t	goes on this interface, used to make virtual \n"\
	" \t\t	interfaces(wl0.1,wl1.1) as current interface \n"\
"------------------------------------------------------------------------------\n"\
" Command 03\t: interworking <0/1> \n"\
	" Example\t: interworking 1 \n"\
	" Purpose\t: Enable/Disable interworking \n"\
"------------------------------------------------------------------------------\n"\
" Command 04\t: accs_net_type <0/1/2/3/4/5/14/15> \n"\
	" Example\t: accs_net_type 3 \n"\
	" Purpose\t: Change Access Network Type \n"\
"------------------------------------------------------------------------------\n"\
" Command 05\t: internet <0/1> \n"\
	" Example\t: internet 1 \n"\
	" Purpose\t: Enable/Disable internet available field in interworking IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 06\t: venue_grp <0/1/2/3/4/5/6/7/8/9/10/11> \n"\
	" Example\t: venue_grp 2 \n"\
	" Purpose\t: Change Venue Group field in interworking IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 07\t: venue_type	<0/1/2/3/4/5/6/7/8/9/10/11> \n"\
	" Example\t: venue_type 8 \n"\
	" Purpose\t: Change Venue Type field in interworking IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 08\t: hessid <Vendor Specific HESSID> \n"\
	" Example\t: hessid 00:11:22:33:44:55:66 \n"\
	" Purpose\t: Change HESSID field in interworking IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 09\t: roaming_cons <oui1> <oui2> ... \n"\
	" Example\t: roaming_cons 506F9A 1122334455 \n"\
	" Purpose\t: List of Roaming Consortium OI in hex separated by \" \", \n"\
	" \t\t	in case of multiple values, String \"Disabled\" is \n"\
	" \t\t	used to Disable Roaming Consortium IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 10\t: anqp <0/1> \n"\
	" Example\t: anqp 1 \n"\
	" Purpose\t: Enable/Disable ANQP in Advertisement Protocol IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 11\t: mih <0/1> \n"\
	" Example\t: mih 0 \n"\
	" Purpose\t: Enable/Disable MIH in Advertisement Protocol IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 12\t: dgaf_disable <0/1> \n"\
	" Example\t: dgaf_disable 0 \n"\
	" Purpose\t: Enable/Disable Downstream Group-Addressed Forwarding bit \n"\
	" \t\t	in Passpoint Vendor IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 13\t: l2_traffic_inspect <0/1> \n"\
	" Example\t: l2_traffic_inspect 1 \n"\
	" Purpose\t: Enable/Disable L2 Traffic Inspection and Filtering \n"\
	" \t\t	(Applies to  APs which support built-in \n"\
	" \t\t	inspection and filtering function)\n"\
"------------------------------------------------------------------------------\n"\
" Command 14\t: icmpv4_echo <0/1> \n"\
	" Example\t: icmpv4_echo 1 \n"\
	" Purpose\t: Filter function for ICMPv4 Echo Requests, \n"\
	" \t\t	Enabled (1) Allow ICMP Echo request, \n"\
	" \t\t	Disabled(0) Deny  ICMP echo request \n"\
"------------------------------------------------------------------------------\n"\
" Command 15\t: plmn_mcc <mcc1> <mcc2> <mcc3> ... \n"\
	" Example\t: plmn_mcc 111 222 333 \n"\
	" Purpose\t: 3GPP Cellular Network infromation : Country Code \n"\
	" \t\t	(list of MCCs separated by \" \", \n"\
	" \t\t	in case of multiple values) \n"\
"------------------------------------------------------------------------------\n"\
" Command 16\t: plmn_mnc <mnc1> <mnc2> <mnc3> ... \n"\
	" Example\t: plmn_mnc 010 011 012 \n"\
	" Purpose\t: 3GPP Cellular Network infromation : Network Code \n"\
	" \t\t	(list of MNCs separated by \" \", \n"\
	" \t\t	in case of multiple values) \n"\
"------------------------------------------------------------------------------\n"\
" Command 17\t: proxy_arp <0/1> \n"\
	" Example\t: proxy_arp 1 \n"\
	" Purpose\t: Enable/Disable ProxyARP \n"\
"------------------------------------------------------------------------------\n"\
" Command 18\t: bcst_uncst <0/1> \n"\
	" Example\t: bcst_uncst 0 \n"\
	" Purpose\t: Broadcast to Unicast conversion functionality. Disabling \n"\
	" \t\t	the conversion is a special mode only required for \n"\
	" \t\t	test bed APs. Enabled(1)/Disabled(0) \n"\
"------------------------------------------------------------------------------\n"\
" Command 19\t: gas_cb_delay <intval> \n"\
	" Example\t: gas_cb_delay 100 \n"\
	" Purpose\t: GAS Comeback Delay in TUs (Applies only to AP that supports \n"\
	" \t\t	4-frame GAS exchange). Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 20\t: 4_frame_gas <0/1> \n"\
	" Example\t: 4_frame_gas 1 \n"\
	" Purpose\t: Enabled(1)/Disabled(0) : Four Frame GAS exchange \n"\
"------------------------------------------------------------------------------\n"\
" Command 21\t: domain_list <domain1> <domain2> ... \n"\
	" Example\t: domain_list wi-fi1.org wi-fi2.org \n"\
	" Purpose\t: Domain Name List separated by \" \", in case multiple values \n"\
"------------------------------------------------------------------------------\n"\
" Command 22\t: hs2 <0/1> \n"\
	" Example\t: hs2 1 \n"\
	" Purpose\t: HS 2.0 Indication element : Enabled(1)/Disabled(0) \n"\
"------------------------------------------------------------------------------\n"\
" Command 23\t: p2p_ie <0/1> \n"\
	" Example\t: p2p_ie 1 \n"\
	" Purpose\t: P2P Indication element : Enabled(1)/Disabled(0) \n"\
"------------------------------------------------------------------------------\n"\
" Command 24\t: p2p_cross_connect <0/1> \n"\
	" Example\t: p2p_cross_connect 0 \n"\
	" Purpose\t: Enable/Disable : P2P Cross Connect field in P2P IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 25\t: osu_provider_list <1/2/3/4/5/6/7/8/9/10/11> \n"\
	" Example\t: osu_provider_list 1 \n"\
	" Purpose\t: Change OSU Provider List #ID ( as per Test Plan). \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 26\t: osu_icon_tag <1/2> \n"\
	" Example\t: osu_icon_tag 1 \n"\
	" Purpose\t: Change icon content to common icon filename \n"\
	" \t\t	for OSU Providers List. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 27\t: osu_server_uri <uri1> <uri2> <uri3> ... \n"\
	" Example\t: osu_server_uri www.ruckus.com www.aruba.com \n"\
	" Purpose\t: OSU Server URIs separated by \" \",  in case of multiple \n"\
	" \t\t	OSU Providers are present. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 28\t: osu_method <method1> <method2> ... \n"\
	" Example\t: osu_method SOAP OMADM SOAP \n"\
	" Purpose\t: OSU Methods List separated by \" \",  in case of multiple \n"\
	" \t\t	OSU Providers are present. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 29\t: osu_ssid <ssid> \n"\
	" Example\t: osu_ssid OSU_Encrypted \n"\
	" Purpose\t: SSID of OSU ESS for OSU Providers List \n"\
"------------------------------------------------------------------------------\n"\
" Command 30\t: anonymous_nai <nai_val> \n"\
	" Example\t: anonymous_nai anonymous.com \n"\
	" Purpose\t: Change Anonymous NAI value \n"\
"------------------------------------------------------------------------------\n"\
" Command 31\t: ip_add_type_avail <ID> \n"\
	" Example\t: ip_add_type_avail 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 32\t: hs_reset \n"\
	" Example\t: hs_reset \n"\
	" Purpose\t: Reset AP. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 33\t: nai_realm_list <ID> \n"\
	" Example\t: nai_realm_list 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 34\t: oper_name <ID> \n"\
	" Example\t: oper_name 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 35\t: venue_name <ID> \n"\
	" Example\t: venue_name 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 36\t: wan_metrics <ID> \n"\
	" Example\t: wan_metrics 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 37\t: conn_cap <ID> \n"\
	" Example\t: conn_cap 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 38\t: oper_class <ID> \n"\
	" Example\t: oper_class 3 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 39\t: net_auth_type <ID> \n"\
	" Example\t: net_auth_type 1 \n"\
	" Purpose\t: ID number. Refer HS2.0 test plan Appdex B.1 for details. \n"\
	" \t\t	Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 40\t: sim <0/1> \n"\
	" Example\t: sim 0 \n"\
	" Purpose\t: Use sim credentials in OSU Provider List \n"\
"------------------------------------------------------------------------------\n"\
" Command 41\t: sr <STA_MAC> <URL> <ServerMethod>\n"\
	" Example\t: sr 00:11:22:33:44:55 www.ruckus.com 0/1 \n"\
	" Purpose\t: Send Subscription Remediation WNM Action Frame to \n"\
	" \t\t	specific associated STA, with URL of the Subscription \n"\
	" \t\t	Remediation Server, Server Method [0 = OMADM, 1 = SOAP] \n"\
"------------------------------------------------------------------------------\n"\
" Command 42\t: di <STA_MAC> <Reason Code> <Reauth Delay> <URL> \n"\
	" Example\t: di 00:11:22:33:44:55 1 100 www.ruckus.com \n"\
	" Purpose\t: Send De-authentication Immenent Notice WNM Action Frame \n"\
	" \t\t	to specific associated STA, with Reason Code as BSS or ESS, \n"\
	" \t\t	delay in seconds that a mobile device waits before attempting \n"\
	" \t\t	re-association to the same BSS/ESS, and Reason URL which provides \n"\
	" \t\t	a webpage explaining why the mobile device was not authorized \n"\
"------------------------------------------------------------------------------\n"\
" Command 43\t: btredi <URL> \n"\
	" Example\t: btredi www.ruckus.com \n"\
	" Purpose\t: Send BSS Transition Request Frame to STA, \n"\
	" \t\t	with session information URL \n"\
"------------------------------------------------------------------------------\n"\
" Command 44\t: qos_map_set <ID> \n"\
	" Example\t: qos_map_set 2 \n"\
	" Purpose\t: Set QoS_Map_Set IE as per ID number. Refer HS2.0 test plan \n"\
	" \t\t	Appdex B.1 for details. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 45\t: bss_load <ID> \n"\
	" Example\t: bss_load 2 \n"\
	" Purpose\t: Set Static BSS_Load value as per ID number. Refer HS2.0 test plan \n"\
	" \t\t	Appdex B.1 for details. Testbed devices only \n"\
"------------------------------------------------------------------------------\n"\
" Command 46\t: osen <0/1> \n"\
	" Example\t: osen 0 \n"\
	" Purpose\t: Enable/Disable OSEN IE \n"\
"------------------------------------------------------------------------------\n"\
" Command 47\t: help \n"\
	" Example\t: help \n"\
	" Purpose\t: Lists CLI Commands used with Hspotap application in CLI mode \n"\
"------------------------------------------------------------------------------\n"\
"==============================================================================\n"\

/* GAS Print Strings */
#define PRNT_GAS_EVNT_TX_FRMT		"%30s  ----->\n"
#define PRNT_GAS_EVNT_RX_FRMT		"%30s  <-----  %s\n"
#define PRNT_GAS_EVNT_INIT_REQ		"request(%d)"
#define PRNT_GAS_EVNT_INIT_RESP		"response(%d)"
#define PRNT_GAS_EVNT_CMBK_REQ		"comeback request(%d)"
#define PRNT_GAS_EVNT_CMBK_RESP		"comeback response(%d, 0x%02x)"
#define PRNT_GAS_EVNT_UNKNOEN		"unknown event type: %d\n"
#define PRNT_GAS_FRAME_UNKNOWN		"unknown"

#define PRNT_GAS_INFO_PEER_MAC		"Peer MAC : %02X:%02X:%02X:%02X:%02X:%02X\n"
#define PRNT_GAS_INFO_DLG_TOKEN		"dialog token : %d\n\n"
#define PRNT_GAS_EVNT_QUERY_REQ		"BCM_GAS_EVENT_QUERY_REQUEST\n"

#define PRNT_GAS_STCD_SUCCESS		"SUCCESS"
#define PRNT_GAS_STCD_FAILURE		"UNSPECIFIED"
#define PRNT_GAS_STCD_ADVPRT_NSUPP	"ADVERTISEMENT_PROTOCOL_NOT_SUPPORTED"
#define PRNT_GAS_STCD_NO_OTD_REQ	"NO_OUTSTANDING_REQUEST"
#define PRNT_GAS_STCD_RSPNRXFM_SRV	"RESPONSE_NOT_RECEIVED_FROM_SERVER"
#define PRNT_GAS_STCD_TIMEOUT		"TIMEOUT"
#define PRNT_GAS_STCD_QRRSP_2LARGE	"QUERY_RESPONSE_TOO_LARGE"
#define PRNT_GAS_STCD_SRV_UNRCHBL	"SERVER_UNREACHABLE"
#define PRNT_GAS_STCD_TX_FAIL		"TRANSMISSION_FAILURE"
#define PRNT_GAS_STCD_UNKNOWN		"unknown GAS status"
#define PRNT_GAS_STCD			"Status = %s\n"
#define PRNT_GAS_PAUSE_4_SRVRESP	"pause for server response: %s\n"
#define PRNT_GAS_QRRSP_BYTES		"%30s  <-----  query response %d bytes %d\n"

/* ---------------------------------- Constatnts & Macros  ---------------------------------- */

/* ----------------------------------- ERROR Strings ------------------------------------ */
#define HSERR_NVRAM_SET_FAILED		"nvram_set failed: %s = %s\n"
#define HSERR_NVRAM_UNSET_FAILED	"nvram_unset failed: %s\n"
#define HSERR_NVRAM_NOT_DEFINED		"NVRAM is not defined: %s\n"
#define HSERR_NVRAM_NOT_DEFINED_EX	"NVRAM is not defined: %s_%s\n"
#define HSERR_ARGV_ERROR		"missing parameter in command %s\n"
#define HSERR_ARGV_MAC_ERROR		"<addr> format is 00:11:22:33:44:55\n"
#define HSERR_URL_LONG_ERROR		"<url> too long\n"
#define HSERR_MALLOC_FAILED		"malloc failed\n"
#define HSERR_ADD_VENDR_IE_FAILED	"failed to add vendor IE\n"
#define HSERR_DEL_VENDR_IE_FAILED	"failed to delete vendor IE\n"
#define HSERR_ADD_IE_FAILED		"failed to add %s IE\n"
#define HSERR_DEL_IE_FAILED		"failed to delete %s IE\n"
#define HSERR_IOVAR_FAILED		"%s failed\n"
#define HSERR_RC_OI_NOT_PRESENT		"Roaming consortium OI is not present\n"
#define HSERR_WRONG_LENGTH_MCC		"wrong MCC length: %d\n"
#define HSERR_WRONG_LENGTH_MNC		"wrong MNC length: %d\n"
#define HSERR_SESINFO_URL_EMPTY		"sess_info_url: length is zero\n"
#define HSERR_WRONG_IFNAME		"wrong interface name: %s\n"
#define HSERR_CHANGE_IFNAME_TO		"change interface to: %s\n"
#define HSERR_OSEN_WITH_HS		"OSEN can't be enabled for HS, Disabling OSEN.\n"
#define HSERR_HS_WTOUT_U11		"U11 must be enabled before HS, Disabling HS.\n"
#define HSERR_CANT_FIND_NV_IFNAME	"can't find NVRAM ifname for: %s\n"
#define HSERR_ARGV_ERROR_IN_MODE	"Not enough arguments for %s option\n"
#define HSERR_MODE_INVALID		"%s invalid\n"
#define HSERR_EVENT_DIS_FALIED		"failed to disable event msg: %d\n"
#define HSERR_CANT_FIND_WLIF		"can't find wl interface\n"
#define HSERR_CANT_FIND_HSPOTAP		"can't find matched hspotap\n"
#define HSERR_GAS_ON			"GAS 4 Frames is ON\n"
#define HSERR_EVENT_GAS_DONE		"HSERR_EVENT_GAS_DONE\n"
#define HSERR_FOPEN_FAILED		"Error opening file : %d = %s"
#define HSERR_DECODE_U11_QRL_FAILED	"Failed to decode the Query list\n"
#define HSERR_DECODE_HS_QRL_FAILED	"Failed to decode passpoint query list\n"
#define HSERR_DECODE_HRQ_FAILED		"failed to decode passpoint hrq\n"
#define HSERR_ICON_FNAME_EMPTY		"Icon file name is empty"
#define HSERR_NULL_CMD			"NULL command\n"
#define HSERR_UNKNOWN_CMD		"unknown command: %s\n"
#define HSERR_OSEN_WTOUT_WPA2		"OSEN can't be enabled if wpa2 not in wl_akm,"\
					" Disabling OSEN.\n"
/* ----------------------------------- ERROR Strings ------------------------------------ */


/* ----------------------------- SIGMA CLI Command Strings ------------------------------ */
#define HSCMD_INTERFACE			"interface"
#define HSCMD_BSS			"bss"
#define HSCMD_INTERWORKING		"interworking"
#define HSCMD_ACCS_NET_TYPE		"accs_net_type"
#define HSCMD_INTERNET			"internet"
#define HSCMD_ASRA			"asra"
#define HSCMD_VENUE_GRP			"venue_grp"
#define HSCMD_VENUE_TYPE		"venue_type"
#define HSCMD_HESSID			"hessid"
#define HSCMD_ROAMING_CONS		"roaming_cons"
#define HSCMD_ANQP			"anqp"
#define HSCMD_MIH			"mih"
#define HSCMD_DGAF_DISABLE		"dgaf_disable"
#define HSCMD_L2_TRAFFIC_INSPECT	"l2_traffic_inspect"
#define HSCMD_ICMPV4_ECHO		"icmpv4_echo"
#define HSCMD_PLMN_MCC			"plmn_mcc"
#define HSCMD_PLMN_MNC			"plmn_mnc"
#define HSCMD_PROXY_ARP			"proxy_arp"
#define HSCMD_BCST_UNCST		"bcst_uncst"
#define HSCMD_GAS_CB_DELAY		"gas_cb_delay"
#define HSCMD_4_FRAME_GAS		"4_frame_gas"
#define HSCMD_DOMAIN_LIST		"domain_list"
#define HSCMD_SESS_INFO_URL		"sess_info_url"
#define HSCMD_DEST			"dest"
#define HSCMD_HS2			"hs2"
#define HSCMD_P2P_IE			"p2p_ie"
#define HSCMD_P2P_CROSS_CONNECT		"p2p_cross_connect"
#define HSCMD_OSU_PROVIDER_LIST		"osu_provider_list"
#define HSCMD_OSU_ICON_TAG		"osu_icon_tag"
#define HSCMD_OSU_SERVER_URI		"osu_server_uri"
#define HSCMD_OSU_METHOD		"osu_method"
#define HSCMD_OSU_SSID			"osu_ssid"
#define HSCMD_ANONYMOUS_NAI		"anonymous_nai"
#define HSCMD_IP_ADD_TYPE_AVAIL		"ip_add_type_avail"
#define HSCMD_HS_RESET			"hs_reset"
#define HSCMD_NAI_REALM_LIST		"nai_realm_list"
#define HSCMD_OPER_NAME			"oper_name"
#define HSCMD_VENUE_NAME		"venue_name"
#define HSCMD_WAN_METRICS		"wan_metrics"
#define HSCMD_CONN_CAP			"conn_cap"
#define HSCMD_OPER_CLASS		"oper_class"
#define HSCMD_NET_AUTH_TYPE		"net_auth_type"
#define HSCMD_PAUSE			"pause"
#define HSCMD_DIS_ANQP_RESPONSE		"dis_anqp_response"
#define HSCMD_SIM			"sim"
#define HSCMD_RESPONSE_SIZE		"response_size"
#define HSCMD_PAUSE_CB_DELAY		"pause_cb_delay"
#define HSCMD_SR			"sr"
#define HSCMD_DI			"di"
#define HSCMD_BTREDI			"btredi"
#define HSCMD_QOS_MAP_SET		"qos_map_set"
#define HSCMD_BSS_LOAD			"bss_load"
#define HSCMD_OSEN			"osen"
#define HSCMD_HELP			"help"
#define HSCMD_RESP_ERROR		"ERROR"
#define HSCMD_RESP_OK			"OK"
/* ----------------------------- SIGMA CLI Command Strings ------------------------------ */


/* ---------------------------------- SIGMA ID Values  ---------------------------------- */
/* Interworking IE */
#define U11EN_ID1			0
#define IWINT_ID1			0
#define IWASRA_ID1			0
#define IWNETTYPE_ID1			"2"
#define VENUEGRP_ID1			"2"
#define VENUETYPE_ID1			"8"
#define HESSID_ID1			"50:6F:9A:00:11:22"

/* IP Address Type Availability */
#define IPV4ADDR_ID1			"1"
#define IPV6ADDR_ID1			"0"

/* Network Authentication List */
#define NETAUTHLIST_ID1_URL "https://tandc-server.wi-fi.org"
#define NETAUTHLIST_ID1 "accepttc=+httpred=https://tandc-server.wi-fi.org"
#define NETAUTHLIST_ID2 "online="

/* Venue Name List */
#define WIFI_VENUE "Wi-Fi Alliance\n2989 Copper Road\nSanta Clara, CA 95051, USA"
#define VENUELIST_ID1 "57692D466920416C6C69616E63650A"\
			"3239383920436F7070657220526F61640A"\
			"53616E746120436C6172612C2043412039"\
			"353035312C2055534121656E677C"\
			"57692D4669E88194E79B9FE5AE9EE9AA8CE5AEA40A"\
			"E4BA8CE4B99DE585ABE4B99DE5B9B4E5BA93E69F8FE8B7AF0A"\
			"E59CA3E5858BE68B89E68B892C20E58AA0E588A9E7A68FE5B0"\
			"BCE4BA9A39353035312C20E7BE8EE59BBD21636869"

/* Roaming Consotium List */
#define OUILIST_ID1			"506F9A:1;001BC504BD:1"

/* 3GPP Cellular Network Information */
#define DEFAULT_CODE			"000"

/* Realm List */
#define REALMLIST_ID1 "mail.example.com+0+21=2,4#5,7?"\
			"cisco.com+0+21=2,4#5,7?"\
			"wi-fi.org+0+21=2,4#5,7;13=5,6?"\
			"example.com+0+13=5,6"

#define REALMLIST_ID1_SIM "cisco.com+0+21=2,4#5,7?"\
			"wi-fi.org+0+21=2,4#5,7;13=5,6?"\
			"example.com+0+13=5,6?"\
			"mail.example.com+0+18=5,2"

#define REALMLIST_ID2 "wi-fi.org+0+21=2,4#5,7"

#define REALMLIST_ID3 "cisco.com+0+21=2,4#5,7?"\
			"wi-fi.org+0+21=2,4#5,7;13=5,6?"\
			"example.com+0+13=5,6"

#define REALMLIST_ID4 "mail.example.com+0+21=2,4#5,7;13=5,6"

#define REALMLIST_ID5 "wi-fi.org+0+21=2,4#5,7?"\
			"ruckuswireless.com+0+21=2,4#5,7"

#define REALMLIST_ID6 "wi-fi.org+0+21=2,4#5,7?"\
			"mail.example.com+0+21=2,4#5,7"

#define REALMLIST_ID7 "wi-fi.org+0+13=5,6;21=2,4#5,7"

/* Passpoint IE */
#define HS2EN_ID1			"0"

/* Passpoint Capability */
#define HS2CAP_ID1			"1"

/* OSEN IE */
#define OSENEN_ID1			0

/* Operating Class Indication */
#define OPERCLS_ID1			"3"

/* Anonnymous NAI */
#define ANONAI_ID1			"anonymous.com"

/* QoS Map IE */
#define QOS_MAP_IE_ID1_EXCEPT "\x35\x02\x16\x06"
#define QOS_MAP_IE_ID1 "35021606+8,15;0,7;255,255;16,31;32,39;255,255;40,47;255,255"
#define QOS_MAP_IE_ID2 "8,15;0,7;255,255;16,31;32,39;255,255;40,47;255,255"

/* WAN Matrics */
#define WANMETRICS_ID1 "1:0:0=2500>384=0>0=0"
#define WANMETRICS_FORMAT "%d:%d:%d=%d>%d=%d>%d=%d"

/* Operator Friendly Name */
#define ENGLISH_FRIENDLY_NAME "Wi-Fi Alliance"
#define CHINESE_FRIENDLY_NAME "\x57\x69\x2d\x46\x69\xe8\x81\x94\xe7\x9b\x9f"

#define OPLIST_ID1 "Wi-Fi Alliance!eng|"\
			"\x57\x69\x2d\x46\x69\xe8\x81\x94\xe7\x9b\x9f!chi"

/* NAI Home Realm List */
#define ENC_RFC4282			"rfc4282"
#define ENC_UTF8			"utf8"

#define HOME_REALM 			"example.com"
#define HOMEQLIST_ID1 			"mail.example.com:rfc4282"

/* Connection Capability List */
#define CONCAPLIST_ID1 "1:0:0;6:20:1;6:22:0;6:80:1;6:443:1;6:1723:0;"\
			"6:5060:0;17:500:1;17:5060:0;17:4500:1;50:0:1"

#define CONCAPLIST_ID2 "6:80:1;6:443:1;17:5060:1;6:5060:1"

#define CONCAPLIST_ID3 "6:80:1;6:443:1"

#define CONCAPLIST_ID4 "6:80:1;6:443:1;6:5060:1;17:5060:1"


/* OSU Provider List */
#define MAIL				"mail.example.com"
#define CISCO				"cisco.com"
#define WIFI				"wi-fi.org"
#define RUCKUS				"ruckuswireless.com"
#define EXAMPLE4			"example.com"

#define ENG_OPNAME_SP_RED		"SP Red Test Only"
#define ENG_OPNAME_SP_BLUE		"SP Blue Test Only"
#define ENG_OPNAME_SP_GREEN		"SP Green Test Only"
#define ENG_OPNAME_SP_ORANGE		"SP Orange Test Only"
#define ENG_OPNAME_WBA			"Wireless Broadband Alliance"

#define ICON_FILENAME_RED_ZXX		"icon_red_zxx.png"
#define ICON_FILENAME_GREEN_ZXX		"icon_green_zxx.png"
#define ICON_FILENAME_BLUE_ZXX		"icon_blue_zxx.png"
#define ICON_FILENAME_ORANGE_ZXX	"icon_orange_zxx.png"
#define ICON_FILENAME_RED		"icon_red_eng.png"
#define ICON_FILENAME_GREEN		"icon_green_eng.png"
#define ICON_FILENAME_BLUE		"icon_blue_eng.png"
#define ICON_FILENAME_ORANGE		"icon_orange_eng.png"
#define ICON_FILENAME_ABGN		"wifi-abgn-logo_270x73.png"

#define ICON_TYPE_ID1			"image/png"
#define OSU_SERVICE_DESC_ID1		"Free service for test purpose"

#define OSU_NAI_TEST_WIFI		"test-anonymous@wi-fi.org"
#define OSU_NAI_ANON_HS			"anonymous@hotspot.net"

#define OPLIST_ID1_OSU_SSID		"OSU"
#define OPLIST_ID8_OSU_SSID		"OSU-Encrypted"
#define OPLIST_ID9_OSU_SSID		"OSU-OSEN"

#define OSUICON_ID_ID1			"1"

#define OPLIST_ID1_OSU_FRNDLY_NAME "SP Red Test Only!eng|"\
				"\x53\x50\x20\xEB\xB9\xA8\xEA\xB0\x95\x20\xED\x85\x8C"\
				"\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor"

#define OPLIST_ID2_OSU_FRNDLY_NAME "Wireless Broadband Allianc!eng|"\
			"\xEC\x99\x80\xEC\x9D\xB4\xEC\x96\xB4\xEB\xA6\xAC\xEC\x8A\xA4\x20"\
			"\xEB\xB8\x8C\xEB\xA1\x9C\xEB\x93\x9C\xEB\xB0\xB4\xEB\x93\x9C\x20"\
			"\xEC\x96\xBC\xEB\x9D\xBC\xEC\x9D\xB4\xEC\x96\xB8\xEC\x8A\xA4!kor"

#define OPLIST_ID3_OSU_FRNDLY_NAME "SP Red Test Only!spa"

#define OPLIST_ID4_OSU_FRNDLY_NAME "SP Orange Test Only!eng|"\
			"\x53\x50\x20\xEC\x98\xA4\xEB\xA0\x8C\xEC\xA7\x80\x20"\
			"\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor"

#define OPLIST_ID5_OSU_FRNDLY_NAME "SP Orange Test Only!eng|"\
			"\x53\x50\x20\xEC\x98\xA4\xEB\xA0\x8C\xEC\xA7\x80\x20"\
			"\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor"

#define OPLIST_ID6_OSU_FRNDLY_NAME "SP Red Test Only!eng|"\
			"\x53\x50\x20\xEB\xB9\xA8\xEA\xB0\x95\x20\xED\x85\x8C"\
			"\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor;"\
			"SP Orange Test Only!eng|"\
			"\x53\x50\x20\xEC\x98\xA4\xEB\xA0\x8C\xEC\xA7\x80\x20\xED"\
			"\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor"

#define OPLIST_ID7_OSU_FRNDLY_NAME "SP Orange Test Only!eng|"\
			"\x53\x50\x20\xEC\x98\xA4\xEB\xA0\x8C\xEC\xA7\x80\x20"\
			"\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8\x20\xEC\xA0\x84\xEC\x9A\xA9!kor"

#define OPLIST_ID9_OSU_FRNDLY_NAME "SP Orange Test Only!eng"

#define OPLIST_ID1_OSU_ICONS "icon_red_zxx.png+icon_red_eng.png"
#define OPLIST_ID2_OSU_ICONS "icon_orange_zxx.png"
#define OPLIST_ID3_OSU_ICONS "icon_red_zxx.png"
#define OPLIST_ID4_OSU_ICONS "icon_orange_zxx.png+icon_orange_eng.png"
#define OPLIST_ID5_OSU_ICONS "icon_orange_zxx.png"
#define OPLIST_ID6_OSU_ICONS "icon_red_zxx.png;icon_orange_zxx.png"
#define OPLIST_ID7_OSU_ICONS "icon_orange_zxx.png+icon_orange_eng.png"
#define OPLIST_ID9_OSU_ICONS "icon_orange_zxx.png"

#define OPLIST_ID1_OSU_URI "https://osu-server.r2-testbed.wi-fi.org/"
#define OPLIST_ID6_OSU_URI "https://osu-server.r2-testbed.wi-fi.org/;"\
			"https://osu-server.r2-testbed.wi-fi.org/"

#define OPLIST_ID6_OSU_METHOD "0;0"

#define OPLIST_ID1_OSU_SERV_DESC "Free service for test purpose!eng|"\
			"\xED\x85\x8C\xEC\x8A\xA4\xED\x8A\xB8\x20\xEB\xAA\xA9"\
			"\xEC\xA0\x81\xEC\x9C\xBC\xEB\xA1\x9C\x20\xEB\xAC\xB4"\
			"\xEB\xA3\x8C\x20\xEC\x84\x9C\xEB\xB9\x84\xEC\x8A\xA4!kor"

#define OPLIST_ID3_OSU_SERV_DESC "Free service for test purpose!spa"

#define OPLIST_ID8_OSU_NAI "anonymous@hotspot.net"
#define OPLIST_ID9_OSU_NAI "test-anonymous@wi-fi.org"
/* ---------------------------------- SIGMA ID Values  ---------------------------------- */


/* -------------------------------- Structure Definitions ---------------------------------- */
typedef struct {
	uint32 pktflag;
	int ieLength;
	uint8 ieData[VNDR_IE_MAX_LEN];
} vendorIeT;

typedef struct
{
	/* wl interface */
	void *ifr;

	/* wl prefix */
	char prefix[MAX_IF_COUNT];

	/* dialog token */
	uint8 dialog_token;

	/* Passpoint Vendor IE, Flag, Capability */
	vendorIeT vendorIeHSI;
	bool hs_ie_enabled;
	int hs_capable;

	/* P2P vendor IE */
	vendorIeT vendorIeP2P;

	/* for testing */
	int gas_cb_delay;
	int isGasPauseForServerResponse;
	int testResponseSize;

	/* BSS transition request */
	uint8 *url;			/* session information URL */
	uint8 url_len;			/* session information URL length */
	uint8 req_token;

	/* QoS Map Set IE */
	bcm_decode_qos_map_t qosMapSetIe;

	/* BSS Load IE id */
	uint8 bssload_id;

	/* OSEN IE flag */
	bool osen_ie_enabled;

	/* Interworking IE */
	bcm_decode_interworking_t iwIe;
	bool iw_ie_enabled;

	/* IP Address Type Availability */
	bcm_decode_anqp_ip_type_t ipaddrAvail;

	/* Network Authentication List */
	bcm_decode_anqp_network_authentication_type_t netauthlist;

	/* NAI Realm List */
	bcm_decode_anqp_nai_realm_list_ex_t realmlist;

	/* Venue List */
	bcm_decode_anqp_venue_name_t venuelist;

	/* Roaming Consortium List */
	bcm_decode_anqp_roaming_consortium_ex_t ouilist;

	/* 3GPP Cellular Info List */
	bcm_decode_anqp_3gpp_cellular_network_t gpp3list;

	/* Domain Name List */
	bcm_decode_anqp_domain_name_list_t domainlist;

	/* Operating Class */
	bcm_decode_hspot_anqp_operating_class_indication_t opclass;

	/* Operator Friendly Name List */
	bcm_decode_hspot_anqp_operator_friendly_name_t oplist;

	/* OSU Provider List Info */
	bcm_decode_hspot_anqp_osu_provider_list_ex_t osuplist;

	/* Annonymous NAI */
	bcm_decode_hspot_anqp_anonymous_nai_t anonai;

	/* WAN Metrics */
	bcm_decode_hspot_anqp_wan_metrics_t wanmetrics;

	/* Connection Capability List */
	bcm_decode_hspot_anqp_connection_capability_t concaplist;

	/* NAI Home Realm Query List */
	bcm_decode_hspot_anqp_nai_home_realm_query_t homeqlist;

} hspotApT;
/* -------------------------------- Structure Definitions ---------------------------------- */


/* ---------------------------------- Global Variables ------------------------------------ */
int hspot_debug_level = HSPOT_DEBUG_ERROR;

static hspotApT *hspotaps[MAX_WLIF_NUM];
static int hspotap_num = 0;
static hspotApT *current_hspotap = NULL;

/* tcp server for remote control */
static int tcpServerEnabled = 0;
static int tcpServerPort;

uint8 chinese_venue_name[] =
	{0x57, 0x69, 0x2d, 0x46, 0x69, 0xe8, 0x81, 0x94,
	0xe7, 0x9b, 0x9f, 0xe5, 0xae, 0x9e, 0xe9, 0xaa,
	0x8c, 0xe5, 0xae, 0xa4, 0x0a, 0xe4, 0xba, 0x8c,
	0xe4, 0xb9, 0x9d, 0xe5, 0x85, 0xab, 0xe4, 0xb9,
	0x9d, 0xe5, 0xb9, 0xb4, 0xe5, 0xba, 0x93, 0xe6,
	0x9f, 0x8f, 0xe8, 0xb7, 0xaf, 0x0a, 0xe5, 0x9c,
	0xa3, 0xe5, 0x85, 0x8b, 0xe6, 0x8b, 0x89, 0xe6,
	0x8b, 0x89, 0x2c, 0x20, 0xe5, 0x8a, 0xa0, 0xe5,
	0x88, 0xa9, 0xe7, 0xa6, 0x8f, 0xe5, 0xb0, 0xbc,
	0xe4, 0xba, 0x9a, 0x39, 0x35, 0x30, 0x35, 0x31,
	0x2c, 0x20, 0xe7, 0xbe, 0x8e, 0xe5, 0x9b, 0xbd};

uint8 kor_opname_sp_red[] =
	{0x53, 0x50, 0x20, 0xEB, 0xB9, 0xA8, 0xEA, 0xB0,
	0x95, 0x20, 0xED, 0x85, 0x8C, 0xEC, 0x8A, 0xA4,
	0xED, 0x8A, 0xB8, 0x20, 0xEC, 0xA0, 0x84, 0xEC,
	0x9A, 0xA9};

uint8 kor_opname_sp_blu[] =
	{0x53, 0x50, 0x20, 0xED, 0x8C, 0x8C, 0xEB, 0x9E,
	0x91, 0x20, 0xED, 0x85, 0x8C, 0xEC, 0x8A, 0xA4,
	0xED, 0x8A, 0xB8, 0x20, 0xEC, 0xA0, 0x84, 0xEC,
	0x9A, 0xA9};

uint8 kor_opname_sp_grn[] =
	{0x53, 0x50, 0x20, 0xEC, 0xB4, 0x88, 0xEB, 0xA1,
	0x9D, 0x20, 0xED, 0x85, 0x8C, 0xEC, 0x8A, 0xA4,
	0xED, 0x8A, 0xB8, 0x20, 0xEC, 0xA0, 0x84, 0xEC,
	0x9A, 0xA9};

uint8 kor_opname_sp_orng[] =
	{0x53, 0x50, 0x20, 0xEC, 0x98, 0xA4, 0xEB, 0xA0,
	0x8C, 0xEC, 0xA7, 0x80, 0x20, 0xED, 0x85, 0x8C,
	0xEC, 0x8A, 0xA4, 0xED, 0x8A, 0xB8, 0x20, 0xEC,
	0xA0, 0x84, 0xEC, 0x9A, 0xA9};

uint8 kor_opname_wba[] =
	{0xEC, 0x99, 0x80, 0xEC, 0x9D, 0xB4, 0xEC, 0x96,
	0xB4, 0xEB, 0xA6, 0xAC, 0xEC, 0x8A, 0xA4, 0x20,
	0xEB, 0xB8, 0x8C, 0xEB, 0xA1, 0x9C, 0xEB, 0x93,
	0x9C, 0xEB, 0xB0, 0xB4, 0xEB, 0x93, 0x9C, 0x20,
	0xEC, 0x96, 0xBC, 0xEB, 0x9D, 0xBC, 0xEC, 0x9D,
	0xB4, 0xEC, 0x96, 0xB8, 0xEC, 0x8A, 0xA4};

uint8 kor_desc_name_id1[] =
	{0xED, 0x85, 0x8C, 0xEC, 0x8A, 0xA4, 0xED, 0x8A,
	0xB8, 0x20, 0xEB, 0xAA, 0xA9, 0xEC, 0xA0, 0x81,
	0xEC, 0x9C, 0xBC, 0xEB, 0xA1, 0x9C, 0x20, 0xEB,
	0xAC, 0xB4, 0xEB, 0xA3, 0x8C, 0x20, 0xEC, 0x84,
	0x9C, 0xEB, 0xB9, 0x84, 0xEC, 0x8A, 0xA4};
/* ---------------------------------- Global Variables ------------------------------------ */


/* ------------------------------ Static Function Declaration -------------------------------- */
static void addIes_u11(hspotApT *hspotap);
static void addIes_hspot(hspotApT *hspotap);
static void addIes(hspotApT *hspotap);

static void deleteIes_u11(hspotApT *hspotap);
static void deleteIes_hspot(hspotApT *hspotap);
static void deleteIes(hspotApT *hspotap);

static int update_iw_ie(hspotApT *hspotap, bool disable);
static int update_rc_ie(hspotApT *hspotap);
static int update_ap_ie(hspotApT *hspotap);
static int update_qosmap_ie(hspotApT *hspotap, bool enable);
static int update_osen_ie(hspotApT *hspotap, bool disable);
static int update_bssload_ie(hspotApT *hspotap, bool isStatic, bool enable);

static int update_proxy_arp(hspotApT *hspotap);
static int update_dgaf_disable(hspotApT *hspotap);
static int update_l2_traffic_inspect(hspotApT *hspotap);
/* ------------------------------ Static Function Declaration -------------------------------- */


/* ========================= UTILITY FUNCTIONS ============================= */
static bool
strToEther(char *str, struct ether_addr *bssid)
{
	int hex[ETHER_ADDR_LEN] = {0};
	int i;

	if (sscanf(str, MAC_FORMAT,
		&hex[0], &hex[1], &hex[2], &hex[3], &hex[4], &hex[5]) != 6)
		return FALSE;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		bssid->octet[i] = hex[i];

	return TRUE;
}

static int
is_primary_radio_on(int primaryInx)
{
	char prefix[MAX_IF_COUNT] = {0};
	char *ptr; int err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	snprintf(prefix, sizeof(prefix), NVFMT_WLIF_PRIM, primaryInx);

	ptr = nvram_get(strcat_r(prefix, NVNM_RADIO, varname));
	if (ptr) {
		return atoi(ptr);
	} else {
		HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);
		err = -1;
	}

	return err;
}

static bool
is_hs_enabled_on_primary_bss(hspotApT *hspotap)
{
	int primaryInx = 0;
	char prefix[MAX_IF_COUNT] = {0};

	sscanf(hspotap->prefix, "wl%d", &primaryInx);

	snprintf(prefix, sizeof(prefix), NVFMT_WLIF_PRIM, primaryInx);

	return get_hspot_flag(prefix, HSFLG_HS_EN);
}

static void
hspotap_free(void)
{
	int i, primaryInx = 0;

	for (i = 0; i < hspotap_num; i++) {

		sscanf(hspotaps[i]->prefix, "wl%d", &primaryInx);

		if (hspotaps[i]->prefix && hspotaps[i]->ifr &&
			is_primary_radio_on(primaryInx)) {

			/* delete IEs */
			deleteIes(hspotaps[i]);

			wl_wnm_url(hspotaps[i]->ifr, 0, 0);

			if (hspotaps[i]->url_len)
				free(hspotaps[i]->url);

			free(hspotaps[i]);
		}
	}

	hspotap_num = 0;

	wlFree();
}

static hspotApT*
get_hspotap_by_wlif(void *ifr)
{
	int i;

	for (i = 0; i < hspotap_num; i++) {
		if (hspotaps[i]->ifr == ifr)
			return hspotaps[i];
	}
	return NULL;
}

static hspotApT*
get_hspotap_by_ifname(char *ifname)
{
	int i;

	if (ifname == NULL)
		return hspotaps[0];

	for (i = 0; i < hspotap_num; i++) {
		if (strcmp(wl_ifname(hspotaps[i]->ifr), ifname) == 0)
			return hspotaps[i];
	}
	return NULL;
}

static hspotApT*
get_hspotap_by_bssid(char *HESSIDstr)
{
	struct ether_addr da;
	struct ether_addr sa;
	int i;

	if (HESSIDstr == NULL)
		return hspotaps[0];

	if (!strToEther(HESSIDstr, &da)) {
		HS20_ERROR(HSERR_ARGV_MAC_ERROR);
		return hspotaps[0];
	}

	for (i = 0; i < hspotap_num; i++) {
		wl_cur_etheraddr(hspotaps[i]->ifr, DEFAULT_BSSCFG_INDEX, &sa);
		if (bcmp(da.octet, sa.octet, ETHER_ADDR_LEN) == 0)
			return hspotaps[i];
	}

	return hspotaps[0];
}
/*
static hspotApT*
get_hspotap_by_ssid(char *ssid)
{
	int i;
	char tmp[BUFF_256] = {0};
	char* wl_ssid;

	if (ssid == NULL)
		return hspotaps[0];

	for (i = 0; i < hspotap_num; i++) {

		wl_ssid = nvram_safe_get(strcat_r(hspotaps[i]->prefix, "ssid", tmp));

		if (strcmp(wl_ssid, ssid) == 0)
			return hspotaps[i];
	}
	return NULL;
}
 */

static void
decode_hspot_flags(char *prefix, hspotApT *hspotap, char* err_nvram)
{
	assert(prefix);
	assert(hspotap);
	assert(err_nvram);

	hspotap->hs_ie_enabled = FALSE;
	hspotap->iw_ie_enabled = FALSE;
	hspotap->osen_ie_enabled = FALSE;

	char *ptr = NULL;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	/* 802.11u Enabled --------------------------------------------------- */
	hspotap->iw_ie_enabled = get_hspot_flag(prefix, HSFLG_U11_EN);

	/* Passpoint Enabled -------------------------------------------------- */
	hspotap->hs_ie_enabled = get_hspot_flag(prefix, HSFLG_HS_EN);
	/* U11 must be enabled before HS, Disabling HS */
	if (!hspotap->iw_ie_enabled && hspotap->hs_ie_enabled) {
		hspotap->hs_ie_enabled = 0;
		set_hspot_flag(prefix, HSFLG_HS_EN, 0);
		HS20_ERROR(HSERR_HS_WTOUT_U11);
	}
	HS20_INFO(PRNT_FLG, varname, hspotap->hs_ie_enabled);

	/* OSEN Enabled ----------------------------------------------------- */
	hspotap->osen_ie_enabled = get_hspot_flag(prefix, HSFLG_OSEN);
	/* OSEN can't be enabled for HS, Disabling OSEN */
	if (hspotap->osen_ie_enabled && hspotap->hs_ie_enabled) {
		hspotap->osen_ie_enabled = 0;
		set_hspot_flag(prefix, HSFLG_OSEN, 0);
		HS20_WARNING(HSERR_OSEN_WITH_HS);
	}
	/* OSEN can't be enabled if wpa2 not in wl_akm, Disabling OSEN */
	ptr = nvram_get(strcat_r(prefix, NVNM_AKM, varname));
	if (ptr) {
	if (hspotap->osen_ie_enabled && !find_in_list(ptr, NVVAL_WPA2)) {
		hspotap->osen_ie_enabled = 0;
		set_hspot_flag(prefix, HSFLG_OSEN, 0);
		HS20_WARNING(HSERR_OSEN_WTOUT_WPA2);
	}
	}
	update_osen_ie(hspotap, TRUE);
	HS20_INFO(PRNT_FLG, varname, hspotap->osen_ie_enabled);

	/* GAS CB Delay flag ------------------------------------------------- */
	ptr = nvram_get(strcat_r(prefix, NVNM_GASCBDEL, varname));
	if (ptr) {
		hspotap->gas_cb_delay = atoi(ptr);
		if (hspotap->gas_cb_delay) {
			hspotap->isGasPauseForServerResponse = FALSE;
		}
		HS20_INFO(PRNT_FLG, varname, hspotap->gas_cb_delay);
	} else {
		HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);
	}

	/* 4 GAS FRAME flag ------------------------------------------------- */
	hspotap->isGasPauseForServerResponse
		= (get_hspot_flag(prefix, HSFLG_4FRAMEGAS) == 0);

	/* Passpoint Capability ----------------------------------------------- */
	ptr = nvram_get(strcat_r(prefix, NVNM_HS2CAP, varname));
	if (ptr) {
		hspotap->hs_capable = atoi(ptr);
		HS20_INFO(PRNT_FLG, varname, hspotap->hs_capable);
	} else {
		HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);
	}

}
/* ========================= UTILITY FUNCTIONS ============================= */


/* ========================== RESET FUNCTIONS ============================= */
static int
reset_hspot_flag(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_HSFLAG, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	}
	else
	{
		hspotap->hs_ie_enabled = 0;
		hspotap->iw_ie_enabled = 0;
		hspotap->osen_ie_enabled = 0;
		snprintf(varvalue, sizeof(varvalue), "%s", "1aa0");
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_HSFLAG, varname), varvalue);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
			err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_iw_ie(hspotApT *hspotap, bool bInit, unsigned int flag)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	memset(&hspotap->iwIe, 0, sizeof(bcm_decode_interworking_t));

	if (!bInit) {
		if (flag & 0x0001) {
			hspotap->iw_ie_enabled = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_U11_EN, 0);
		}
		if (flag & 0x0002) {
			hspotap->iwIe.isInternet = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_IWINT_EN, 0);
		}
		if (flag & 0x0004) {
			hspotap->iwIe.accessNetworkType = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_IWNETTYPE, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		if (flag & 0x0008) {
			hspotap->iwIe.isHessid = FALSE;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_HESSID, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		if (flag & 0x0010) {
			hspotap->iwIe.isAsra = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_IWASRA_EN, 0);
		}
	} else {
		if (flag & 0x0001) {
			hspotap->iw_ie_enabled = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_U11_EN, U11EN_ID1);
		}
		if (flag & 0x0002) {
			hspotap->iwIe.isInternet = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_IWINT_EN, IWINT_ID1);
		}
		if (flag & 0x0004) {
			  hspotap->iwIe.accessNetworkType = 2;
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IWNETTYPE, varname),
				IWNETTYPE_ID1);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, IWNETTYPE_ID1);
				err = -1;
			}
		}
		if (flag & 0x0008) {
			hspotap->iwIe.isHessid = TRUE;
			strToEther(HESSID_ID1, &hspotap->iwIe.hessid);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_HESSID, varname),
				HESSID_ID1);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, HESSID_ID1);
				err = -1;
			}
		}
		if (flag & 0x0010) {
			hspotap->iwIe.isAsra = FALSE;
			set_hspot_flag(hspotap->prefix, HSFLG_IWASRA_EN, IWASRA_ID1);
		}
	}
	nvram_commit();
	return err;
}

static int
reset_qosmap_ie(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	memset(&hspotap->qosMapSetIe, 0, sizeof(bcm_decode_qos_map_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_QOSMAPIE, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->qosMapSetIe.exceptCount = 2;
		hspotap->qosMapSetIe.except[0].dscp = 0x35;
		hspotap->qosMapSetIe.except[0].up = 0x02;
		hspotap->qosMapSetIe.except[1].dscp = 0x16;
		hspotap->qosMapSetIe.except[1].up = 0x06;
		hspotap->qosMapSetIe.up[0].low = 8;
		hspotap->qosMapSetIe.up[0].high = 15;
		hspotap->qosMapSetIe.up[1].low = 0;
		hspotap->qosMapSetIe.up[1].high = 7;
		hspotap->qosMapSetIe.up[2].low = 255;
		hspotap->qosMapSetIe.up[2].high = 255;
		hspotap->qosMapSetIe.up[3].low = 16;
		hspotap->qosMapSetIe.up[3].high = 31;
		hspotap->qosMapSetIe.up[4].low = 32;
		hspotap->qosMapSetIe.up[4].high = 39;
		hspotap->qosMapSetIe.up[5].low = 255;
		hspotap->qosMapSetIe.up[5].high = 255;
		hspotap->qosMapSetIe.up[6].low = 40;
		hspotap->qosMapSetIe.up[6].high = 47;
		hspotap->qosMapSetIe.up[7].low = 255;
		hspotap->qosMapSetIe.up[7].high = 255;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_QOSMAPIE, varname), QOS_MAP_IE_ID1);
		if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, QOS_MAP_IE_ID1);
		err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_osen_ie(hspotApT *hspotap, bool bInit)
{
	int err = 0;

	if (!bInit) {
		hspotap->osen_ie_enabled = FALSE;
		err = set_hspot_flag(hspotap->prefix, HSFLG_OSEN, 0);
	} else {
		hspotap->osen_ie_enabled = FALSE;
		err = set_hspot_flag(hspotap->prefix, HSFLG_OSEN, OSENEN_ID1);
	}

	return err;
}

static int
reset_hs_cap(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_HS2CAP, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->hs_capable = 1;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_HS2CAP, varname), HS2CAP_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, HS2CAP_ID1);
			err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_u11_ipaddr_typeavail(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->ipaddrAvail, 0, sizeof(bcm_decode_anqp_ip_type_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_IPV4ADDR, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_IPV6ADDR, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->ipaddrAvail.isDecodeValid = TRUE;
		hspotap->ipaddrAvail.ipv4 = IPA_IPV4_SINGLE_NAT;
		hspotap->ipaddrAvail.ipv6 = IPA_IPV6_NOT_AVAILABLE;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IPV4ADDR, varname), IPV4ADDR_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, IPV4ADDR_ID1);
			err = -1;
		}
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IPV6ADDR, varname), IPV6ADDR_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, IPV6ADDR_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_u11_netauth_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->netauthlist, 0, sizeof(bcm_decode_anqp_network_authentication_type_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_NETAUTHLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->netauthlist.isDecodeValid = TRUE;
		hspotap->netauthlist.numAuthenticationType = 2;

		hspotap->netauthlist.unit[0].type = (uint8)NATI_ACCEPTANCE_OF_TERMS_CONDITIONS;
		hspotap->netauthlist.unit[0].urlLen = 0;
		strncpy_n((char*)hspotap->netauthlist.unit[0].url,
			EMPTY_STR, BCM_DECODE_ANQP_MAX_URL_LENGTH + 1);

		hspotap->netauthlist.unit[1].type = (uint8)NATI_HTTP_HTTPS_REDIRECTION;
		hspotap->netauthlist.unit[1].urlLen = strlen(NETAUTHLIST_ID1_URL);
		strncpy_n((char*)hspotap->netauthlist.unit[1].url,
			NETAUTHLIST_ID1_URL, BCM_DECODE_ANQP_MAX_URL_LENGTH + 1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_NETAUTHLIST, varname),
			NETAUTHLIST_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, NETAUTHLIST_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_u11_realm_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0, useSim = 0, iR = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 auth_MSCHAPV2[1]		= { (uint8)REALM_MSCHAPV2 };
	uint8 auth_UNAMPSWD[1]		= { (uint8)REALM_USERNAME_PASSWORD };
	uint8 auth_CERTIFICATE[1]	= { (uint8)REALM_CERTIFICATE };
	uint8 auth_SIM[1]		= { (uint8)REALM_SIM };

	memset(&hspotap->realmlist, 0, sizeof(bcm_decode_anqp_nai_realm_list_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {

	/* Fill the bcm_decode_anqp_nai_realm_list_t structure for Realm_id = 1 */
	/* And set NVRAM value for wl_realmlist */

	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 4;

	useSim = get_hspot_flag(hspotap->prefix, HSFLG_USE_SIM);
	iR = 0;

	if (!useSim) {
	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(MAIL);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 1.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	iR++;
	/* -------------------------------------------------- */
	}

	/* Realm 2 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(CISCO);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, CISCO,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 2.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 2.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 2.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	iR++;
	/* -------------------------------------------------- */

	/* Realm 3 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 2;
	/* EAP 3.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 3.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 3.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* EAP 3.2 */
	hspotap->realmlist.realm[iR].eap[1].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[iR].eap[1].authCount = 1;
	/* Auth 3.2.1 */
	hspotap->realmlist.realm[iR].eap[1].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[1].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[iR].eap[1].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	iR++;
	/* -------------------------------------------------- */

	/* Realm 4 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(EXAMPLE4);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, EXAMPLE4,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 4.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 1;
	/* Auth 4.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	iR++;
	/* -------------------------------------------------- */

	if (useSim) {
	/* Realm 4 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(MAIL);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 4.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_SIM;
	hspotap->realmlist.realm[iR].eap[0].authCount = 1;
	/* Auth 4.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_SIM);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_SIM, sizeof(auth_SIM));
	}
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname),
	useSim ? REALMLIST_ID1_SIM : REALMLIST_ID1);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname,
			useSim ? REALMLIST_ID1_SIM : REALMLIST_ID1);
		err = -1;
	}
	}
	nvram_commit();
	return err;
}

static int
reset_u11_venue_list(hspotApT *hspotap, bool bInit, unsigned int flag)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (!bInit) {
		memset(&hspotap->venuelist, 0, sizeof(bcm_decode_anqp_venue_name_t));
		if (flag & 0x0001) {
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_VENUETYPE, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		if (flag & 0x0002) {
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_VENUEGRP, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		if (flag & 0x0004) {
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_VENUELIST, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
	} else {
		hspotap->venuelist.isDecodeValid = TRUE;
		hspotap->iwIe.isVenue = TRUE;

		if (flag & 0x0001) {
			hspotap->venuelist.type = 8;
			hspotap->iwIe.venueType = 8;
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_VENUETYPE, varname),
				VENUETYPE_ID1);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, VENUETYPE_ID1);
				err = -1;
			}
		}
		if (flag & 0x0002) {
			hspotap->venuelist.group = 2;
			hspotap->iwIe.venueGroup = 2;
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_VENUEGRP, varname),
				VENUEGRP_ID1);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, VENUEGRP_ID1);
				err = -1;
			}
		}
		if (flag & 0x0004) {

			memset(hspotap->venuelist.venueName, 0,
				sizeof(hspotap->venuelist.venueName));

			hspotap->venuelist.numVenueName = 2;

			strncpy_n(hspotap->venuelist.venueName[0].name, WIFI_VENUE,
				VENUE_NAME_SIZE + 1);
			hspotap->venuelist.venueName[0].nameLen = strlen(WIFI_VENUE);
			strncpy_n(hspotap->venuelist.venueName[0].lang, ENGLISH,
				VENUE_LANGUAGE_CODE_SIZE + 1);
			hspotap->venuelist.venueName[0].langLen = strlen(ENGLISH);

			strncpy_n(hspotap->venuelist.venueName[1].name,
				(char *)chinese_venue_name, VENUE_NAME_SIZE + 1);
			hspotap->venuelist.venueName[1].nameLen = sizeof(chinese_venue_name);
			strncpy_n(hspotap->venuelist.venueName[1].lang,
				CHINESE, VENUE_LANGUAGE_CODE_SIZE +1);
			hspotap->venuelist.venueName[1].langLen = strlen(CHINESE);

			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_VENUELIST, varname),
				VENUELIST_ID1);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, VENUELIST_ID1);
				err = -1;
			}
		}
	}

	nvram_commit();
	return err;
}

static int
reset_u11_oui_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->ouilist, 0, sizeof(bcm_decode_anqp_roaming_consortium_ex_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OUILIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->ouilist.numOi = 2;
		memcpy(hspotap->ouilist.oi[0].oi, WFA_OUI, strlen(WFA_OUI));
		hspotap->ouilist.oi[0].isBeacon = 1;
		hspotap->ouilist.oi[0].oiLen = strlen(WFA_OUI);
		hspotap->ouilist.oi[1].oi[0] = 0x00;
		hspotap->ouilist.oi[1].oi[1] = 0x1B;
		hspotap->ouilist.oi[1].oi[2] = 0xC5;
		hspotap->ouilist.oi[1].oi[3] = 0x04;
		hspotap->ouilist.oi[1].oi[4] = 0xBD;
		hspotap->ouilist.oi[1].isBeacon = 1;
		hspotap->ouilist.oi[1].oiLen = 5;
		hspotap->ouilist.isDecodeValid = TRUE;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OUILIST, varname), OUILIST_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OUILIST_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_u11_3gpp_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->gpp3list, 0, sizeof(bcm_decode_anqp_3gpp_cellular_network_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_3GPPLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->gpp3list.isDecodeValid = TRUE;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_3GPPLIST, varname), EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_u11_domain_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->domainlist, 0, sizeof(bcm_decode_anqp_domain_name_list_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_DOMAINLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->domainlist.isDecodeValid = TRUE;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_DOMAINLIST, varname), EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_hspot_oper_class(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OPERCLS, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		uint8 opClass3[2] = {OPCLS_2G, OPCLS_5G};
		hspotap->opclass.opClassLen = sizeof(opClass3);
		memcpy(hspotap->opclass.opClass, opClass3, sizeof(opClass3));

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OPERCLS, varname), OPERCLS_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPERCLS_ID1);
			err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_hspot_anonai(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->anonai, 0, sizeof(bcm_decode_hspot_anqp_anonymous_nai_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_ANONAI, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->anonai.isDecodeValid = TRUE;

		strncpy_n(hspotap->anonai.nai, ANONAI_ID1, BCM_DECODE_HSPOT_ANQP_MAX_NAI_SIZE + 1);
		hspotap->anonai.naiLen = strlen(hspotap->anonai.nai);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_ANONAI, varname), ANONAI_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, ANONAI_ID1);
			err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_hspot_wan_metrics(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->wanmetrics, 0, sizeof(bcm_decode_hspot_anqp_wan_metrics_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_WANMETRICS, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->wanmetrics.isDecodeValid	= TRUE;
		hspotap->wanmetrics.linkStatus		= HSPOT_WAN_LINK_UP;
		hspotap->wanmetrics.symmetricLink	= HSPOT_WAN_NOT_SYMMETRIC_LINK;
		hspotap->wanmetrics.atCapacity		= HSPOT_WAN_NOT_AT_CAPACITY;
		hspotap->wanmetrics.dlinkSpeed		= 2500;
		hspotap->wanmetrics.ulinkSpeed		= 384;
		hspotap->wanmetrics.dlinkLoad		= 0;
		hspotap->wanmetrics.ulinkLoad		= 0;
		hspotap->wanmetrics.lmd			= 0;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_WANMETRICS, varname),
			WANMETRICS_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, WANMETRICS_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_hspot_op_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->oplist, 0, sizeof(bcm_decode_hspot_anqp_operator_friendly_name_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OPLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->oplist.isDecodeValid = TRUE;
		hspotap->oplist.numName = 2;

		strncpy_n(hspotap->oplist.duple[0].name,
		ENGLISH_FRIENDLY_NAME, VENUE_NAME_SIZE + 1);
		hspotap->oplist.duple[0].nameLen = strlen(ENGLISH_FRIENDLY_NAME);
		strncpy_n(hspotap->oplist.duple[0].lang, ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->oplist.duple[0].langLen = strlen(ENGLISH);

		strncpy_n(hspotap->oplist.duple[1].name,
		CHINESE_FRIENDLY_NAME, VENUE_NAME_SIZE + 1);
		hspotap->oplist.duple[1].nameLen = strlen(CHINESE_FRIENDLY_NAME);
		strncpy_n(hspotap->oplist.duple[1].lang, CHINESE, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->oplist.duple[1].langLen = strlen(CHINESE);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OPLIST, varname), OPLIST_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_hspot_homeq_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->homeqlist, 0, sizeof(bcm_decode_hspot_anqp_nai_home_realm_query_t));

	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_HOMEQLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->homeqlist.isDecodeValid = TRUE;
		hspotap->homeqlist.count = 1;
		strncpy_n(hspotap->homeqlist.data[0].name, HOME_REALM, VENUE_NAME_SIZE + 1);
		hspotap->homeqlist.data[0].nameLen = strlen(HOME_REALM);
		hspotap->homeqlist.data[0].encoding = (uint8)REALM_ENCODING_RFC4282;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_HOMEQLIST, varname), HOMEQLIST_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, HOMEQLIST_ID1);
			err = -1;
		}
	}

	nvram_commit();
	return err;
}

static int
reset_hspot_conncap_list(hspotApT *hspotap, bool bInit)
{
	int ret, err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	memset(&hspotap->concaplist, 0, sizeof(bcm_decode_hspot_anqp_connection_capability_t));
	if (!bInit) {
		ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_CONCAPLIST, varname));
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
			err = -1;
		}
	} else {
		hspotap->concaplist.isDecodeValid = TRUE;
		hspotap->concaplist.numConnectCap = 11;
		hspotap->concaplist.tuple[0].ipProtocol = (uint8)HSPOT_CC_IPPROTO_ICMP;
		hspotap->concaplist.tuple[0].portNumber = (uint16)HSPOT_CC_PORT_RESERVED;
		hspotap->concaplist.tuple[0].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[1].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[1].portNumber = (uint16)HSPOT_CC_PORT_FTP;
		hspotap->concaplist.tuple[1].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[2].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[2].portNumber = (uint16)HSPOT_CC_PORT_SSH;
		hspotap->concaplist.tuple[2].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[3].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[3].portNumber = (uint16)HSPOT_CC_PORT_HTTP;
		hspotap->concaplist.tuple[3].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[4].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[4].portNumber = (uint16)HSPOT_CC_PORT_HTTPS;
		hspotap->concaplist.tuple[4].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[5].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[5].portNumber = (uint16)HSPOT_CC_PORT_PPTP;
		hspotap->concaplist.tuple[5].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[6].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[6].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[6].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[7].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[7].portNumber = (uint16)HSPOT_CC_PORT_ISAKMP;
		hspotap->concaplist.tuple[7].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[8].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[8].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[8].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[9].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[9].portNumber = (uint16)HSPOT_CC_PORT_IPSEC;
		hspotap->concaplist.tuple[9].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[10].ipProtocol = (uint8)HSPOT_CC_IPPROTO_ESP;
		hspotap->concaplist.tuple[10].portNumber = (uint16)HSPOT_CC_PORT_RESERVED;
		hspotap->concaplist.tuple[10].status = (uint8)HSPOT_CC_STATUS_OPEN;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_CONCAPLIST, varname),
			CONCAPLIST_ID1);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, CONCAPLIST_ID1);
			err = -1;
		}
	}
	nvram_commit();
	return err;
}

static int
reset_hspot_osup_list(hspotApT *hspotap, bool bInit, unsigned int flag)
{
	int ret, err = 0, iter = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 osu_method[1] = {0};

	if (!bInit) {
		/* OSU_SSID */
		if (flag & 0x0001) {
			hspotap->osuplist.isDecodeValid = FALSE;
			memset(hspotap->osuplist.osuSsid, 0, sizeof(hspotap->osuplist.osuSsid));
			hspotap->osuplist.osuSsidLength = 0;
			hspotap->osuplist.osuProviderCount = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_Friendly_Name */
		if (flag & 0x0002) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++)
				memset(&hspotap->osuplist.osuProvider[iter].name, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].name));
			hspotap->osuplist.osuProviderCount = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_Server_URI */
		if (flag & 0x0004) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].uri, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].uri));
				hspotap->osuplist.osuProvider[iter].uriLength = 0;
			}
			hspotap->osuplist.osuProviderCount = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_Method */
		if (flag & 0x0008) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].method, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].method));
				hspotap->osuplist.osuProvider[iter].methodLength = 0;
			}
			hspotap->osuplist.osuProviderCount = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_Icons */
		if (flag & 0x0010) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].iconMetadata, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].iconMetadata));
				hspotap->osuplist.osuProvider[iter].iconMetadataCount = 0;
			}
			hspotap->osuplist.osuProviderCount = 0;
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_NAI */
		if (flag & 0x0020) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].nai, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].nai));
				hspotap->osuplist.osuProvider[iter].naiLength = 0;
			}
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_Server_Desc */
		if (flag & 0x0040) {
			for (iter = 0; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(&hspotap->osuplist.osuProvider[iter].desc, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].desc));
			}
			ret = nvram_unset(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname));
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_UNSET_FAILED, varname);
				err = -1;
			}
		}
		/* OSU_ICON_ID */
		if (flag & 0x0080) {
			hspotap->osuplist.osuicon_id = 1;
			set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);
		}
	} else {
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_SSID */
		if (flag & 0x0001) {
			strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
				BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
			hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
				OPLIST_ID1_OSU_SSID);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
				err = -1;
			}
		}
		/* OSU_Friendly_Name */
		if (flag & 0x0002) {
			hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
			hspotap->osuplist.osuProvider[0].name.numName = 2;
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
				ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
			hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
				strlen(ENGLISH);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
				ENG_OPNAME_SP_RED, VENUE_NAME_SIZE + 1);
			hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
				strlen(ENG_OPNAME_SP_RED);
			strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
				KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
			hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
				strlen(KOREAN);
			strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
				(char *)kor_opname_sp_red, VENUE_NAME_SIZE + 1);
			hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
				sizeof(kor_opname_sp_red);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
				OPLIST_ID1_OSU_FRNDLY_NAME);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname,
					OPLIST_ID1_OSU_FRNDLY_NAME);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++)
				memset(&hspotap->osuplist.osuProvider[iter].name, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].name));
		}
		/* OSU_Server_URI */
		if (flag & 0x0004) {
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
				BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].uriLength =
				strlen(OPLIST_ID1_OSU_URI);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
				OPLIST_ID1_OSU_URI);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].uri, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].uri));
				hspotap->osuplist.osuProvider[iter].uriLength = 0;
			}
		}
		/* OSU_Method */
		if (flag & 0x0008) {
			osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
			memcpy(hspotap->osuplist.osuProvider[0].method,
				osu_method, sizeof(osu_method));
			hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
				SOAP_NVVAL);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].method, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].method));
				hspotap->osuplist.osuProvider[iter].methodLength = 0;
			}
		}
		/* OSU_Icons */
		if (flag & 0x0010) {
			hspotap->osuplist.osuProvider[0].iconMetadataCount = 2;
			hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
			hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
				LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
				ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength
						= strlen(ICON_TYPE_ID1);
			strncpy_n((char*)
			hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
				ICON_FILENAME_RED_ZXX,
				BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
				strlen(ICON_FILENAME_RED_ZXX);
			/* Icon Metadata 2 */
			hspotap->osuplist.osuProvider[0].iconMetadata[1].width = 160;
			hspotap->osuplist.osuProvider[0].iconMetadata[1].height = 76;
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].lang,
				ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].type,
				ICON_TYPE_ID1,
				BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].iconMetadata[1].typeLength
					= strlen(ICON_TYPE_ID1);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].filename,
				ICON_FILENAME_RED,
				BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].iconMetadata[1].filenameLength =
				strlen(ICON_FILENAME_RED);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
				OPLIST_ID1_OSU_ICONS);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_ICONS);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].iconMetadata, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].iconMetadata));
				hspotap->osuplist.osuProvider[iter].iconMetadataCount = 0;
			}
		}
		/* OSU_NAI */
		if (flag & 0x0020) {
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
				BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
			hspotap->osuplist.osuProvider[0].naiLength = 0;
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
				EMPTY_STR);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(hspotap->osuplist.osuProvider[iter].nai, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].nai));
				hspotap->osuplist.osuProvider[iter].naiLength = 0;
			}
		}
		/* OSU_Server_Desc */
		if (flag & 0x0040) {
			hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
			hspotap->osuplist.osuProvider[0].desc.numName = 2;
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
				ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
			hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
				strlen(ENGLISH);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
				OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
			hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
				strlen(OSU_SERVICE_DESC_ID1);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
				KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
			hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
				strlen(KOREAN);
			strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
				(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
			hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
				sizeof(kor_desc_name_id1);
			ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
				OPLIST_ID1_OSU_SERV_DESC);
			if (ret) {
				HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname,
				OPLIST_ID1_OSU_SERV_DESC);
				err = -1;
			}
			for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++) {
				memset(&hspotap->osuplist.osuProvider[iter].desc, 0,
					sizeof(hspotap->osuplist.osuProvider[iter].desc));
			}
		}
		/* OSU_ICON_ID */
		if (flag & 0x0080) {
			hspotap->osuplist.osuicon_id = 1;
			set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);
		}
	}
	nvram_commit();
	return err;
}
/* ========================== RESET FUNCTIONS ============================= */


/* ======================== ENCODING FUNCTIONS ============================ */
static void
encode_u11_cap_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t vendor;
	uint8 vendorCap[] = {
		HSPOT_SUBTYPE_QUERY_LIST,
		HSPOT_SUBTYPE_CAPABILITY_LIST,
		HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME,
		HSPOT_SUBTYPE_WAN_METRICS,
		HSPOT_SUBTYPE_CONNECTION_CAPABILITY,
		HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY,
		HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION,
		HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS,
		HSPOT_SUBTYPE_ANONYMOUS_NAI,
		HSPOT_SUBTYPE_ICON_REQUEST,
		HSPOT_SUBTYPE_ICON_BINARY_FILE };

	uint16 cap[] = {
		ANQP_ID_QUERY_LIST,
		ANQP_ID_CAPABILITY_LIST,
		ANQP_ID_VENUE_NAME_INFO,
		ANQP_ID_EMERGENCY_CALL_NUMBER_INFO,
		ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO,
		ANQP_ID_ROAMING_CONSORTIUM_LIST,
		ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO,
		ANQP_ID_NAI_REALM_LIST,
		ANQP_ID_G3PP_CELLULAR_NETWORK_INFO,
		ANQP_ID_AP_GEOSPATIAL_LOCATION,
		ANQP_ID_AP_CIVIC_LOCATION,
		ANQP_ID_AP_LOCATION_PUBLIC_ID_URI,
		ANQP_ID_DOMAIN_NAME_LIST,
		ANQP_ID_EMERGENCY_ALERT_ID_URI,
		ANQP_ID_EMERGENCY_NAI };

	/* encode vendor specific capability */
	bcm_encode_init(&vendor, sizeof(buffer), buffer);
	bcm_encode_hspot_anqp_capability_list(&vendor,
	sizeof(vendorCap) / sizeof(uint8), vendorCap);

	/* encode capability with vendor specific appended */
	bcm_encode_anqp_capability_list(pkt, sizeof(cap) / sizeof(uint16), cap,
	bcm_encode_length(&vendor), bcm_encode_buf(&vendor));
}

static void
encode_u11_ipaddr_typeavail(hspotApT *hspotap, bcm_encode_t *pkt)
{
	bcm_encode_anqp_ip_type_availability(pkt,
		hspotap->ipaddrAvail.ipv6, hspotap->ipaddrAvail.ipv4);
}

static void
encode_u11_netauth_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t network;

	bcm_encode_init(&network, sizeof(buffer), buffer);

	int i;
	for (i = 0; i < hspotap->netauthlist.numAuthenticationType; i++) {
		bcm_encode_anqp_network_authentication_unit(&network,
			hspotap->netauthlist.unit[i].type, hspotap->netauthlist.unit[i].urlLen,
			(char*)hspotap->netauthlist.unit[i].url);
	}

	bcm_encode_anqp_network_authentication_type(pkt,
		bcm_encode_length(&network), bcm_encode_buf(&network));
}

static void
encode_u11_realm_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 RealmBuf[BUFF_256], EapBuf[BUFF_256], AuthBuf[BUFF_256];
	bcm_encode_t Realm, Eap, Auth;
	int iR, iE, iA;

	/* Initialize Realm Buffer */
	bcm_encode_init(&Realm, sizeof(RealmBuf), RealmBuf);

	for (iR = 0; iR < hspotap->realmlist.realmCount; iR++) {

		/* Initialize Eap Buffer */
		bcm_encode_init(&Eap, sizeof(EapBuf), EapBuf);

		for (iE = 0; iE < hspotap->realmlist.realm[iR].eapCount; iE++) {

			/* Initialize Auth Buffer */
			bcm_encode_init(&Auth, sizeof(AuthBuf), AuthBuf);

			for (iA = 0; iA < hspotap->realmlist.realm[iR].eap[iE].authCount; iA++) {

				bcm_encode_anqp_authentication_subfield(&Auth,
					hspotap->realmlist.realm[iR].eap[iE].auth[iA].id,
					hspotap->realmlist.realm[iR].eap[iE].auth[iA].len,
					hspotap->realmlist.realm[iR].eap[iE].auth[iA].value);
				}

			bcm_encode_anqp_eap_method_subfield(&Eap,
				hspotap->realmlist.realm[iR].eap[iE].eapMethod,
				hspotap->realmlist.realm[iR].eap[iE].authCount,
				bcm_encode_length(&Auth), bcm_encode_buf(&Auth));
			}

		bcm_encode_anqp_nai_realm_data(&Realm,
			hspotap->realmlist.realm[iR].encoding,
			hspotap->realmlist.realm[iR].realmLen,
			hspotap->realmlist.realm[iR].realm,
			hspotap->realmlist.realm[iR].eapCount,
			bcm_encode_length(&Eap), bcm_encode_buf(&Eap));
	}

	bcm_encode_anqp_nai_realm(pkt, hspotap->realmlist.realmCount,
		bcm_encode_length(&Realm), bcm_encode_buf(&Realm));
}

static void
encode_u11_venue_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t duple;

	bcm_encode_init(&duple, sizeof(buffer), buffer);

	int i;
	for (i = 0; i < hspotap->venuelist.numVenueName; i++) {
		bcm_encode_anqp_venue_duple(&duple,
			hspotap->venuelist.venueName[i].langLen,
			hspotap->venuelist.venueName[i].lang,
			hspotap->venuelist.venueName[i].nameLen,
			hspotap->venuelist.venueName[i].name);
	}

	bcm_encode_anqp_venue_name(pkt, hspotap->venuelist.group,
		hspotap->venuelist.type, bcm_encode_length(&duple), bcm_encode_buf(&duple));
}

static void
encode_u11_oui_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t oi;

	bcm_encode_init(&oi, sizeof(buffer), buffer);

	int i;
	for (i = 0; i < hspotap->ouilist.numOi; i++) {
		bcm_encode_anqp_oi_duple(&oi, hspotap->ouilist.oi[i].oiLen,
			hspotap->ouilist.oi[i].oi);
	}

	bcm_encode_anqp_roaming_consortium(pkt,
		bcm_encode_length(&oi), bcm_encode_buf(&oi));
}

static void
encode_u11_3gpp_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 plmnBuf[BUFF_256] = {0};
	bcm_encode_t plmn;

	bcm_encode_init(&plmn, BUFF_256, plmnBuf);

	int i;
	for (i = 0; i < hspotap->gpp3list.plmnCount; i++) {
		bcm_encode_anqp_plmn(&plmn, hspotap->gpp3list.plmn[i].mcc,
			hspotap->gpp3list.plmn[i].mnc);
	}

	bcm_encode_anqp_3gpp_cellular_network(pkt,
		hspotap->gpp3list.plmnCount, bcm_encode_length(&plmn), bcm_encode_buf(&plmn));
}

static void
encode_u11_domain_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t name;

	bcm_encode_init(&name, sizeof(buffer), buffer);

	if (hspotap->domainlist.numDomain) {
		int i;
		for (i = 0; i < hspotap->domainlist.numDomain; i++) {
			bcm_encode_anqp_domain_name(&name, hspotap->domainlist.domain[i].len,
				hspotap->domainlist.domain[i].name);
		}
	}

	bcm_encode_anqp_domain_name_list(pkt,
		bcm_encode_length(&name), bcm_encode_buf(&name));
}

static void
encode_hspot_cap_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 cap[] = {
		HSPOT_SUBTYPE_QUERY_LIST,
		HSPOT_SUBTYPE_CAPABILITY_LIST,
		HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME,
		HSPOT_SUBTYPE_WAN_METRICS,
		HSPOT_SUBTYPE_CONNECTION_CAPABILITY,
		HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY,
		HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION,
		HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS,
		HSPOT_SUBTYPE_ANONYMOUS_NAI,
		HSPOT_SUBTYPE_ICON_REQUEST,
		HSPOT_SUBTYPE_ICON_BINARY_FILE };

	bcm_encode_hspot_anqp_capability_list(pkt, sizeof(cap) / sizeof(uint8), cap);
}

static void
encode_hspot_oper_class(hspotApT *hspotap, bcm_encode_t *pkt)
{
	bcm_encode_hspot_anqp_operating_class_indication(pkt,
		hspotap->opclass.opClassLen, hspotap->opclass.opClass);
}

static void
encode_hspot_anonai(hspotApT *hspotap, bcm_encode_t *pkt)
{
	bcm_encode_hspot_anqp_anonymous_nai(pkt,
		hspotap->anonai.naiLen, (uint8 *)hspotap->anonai.nai);
}

static void
encode_hspot_wan_metrics(hspotApT *hspotap, bcm_encode_t *pkt)
{
	bcm_encode_hspot_anqp_wan_metrics(pkt, hspotap->wanmetrics.linkStatus,
		hspotap->wanmetrics.symmetricLink, hspotap->wanmetrics.atCapacity,
		hspotap->wanmetrics.dlinkSpeed, hspotap->wanmetrics.ulinkSpeed,
		hspotap->wanmetrics.dlinkLoad, hspotap->wanmetrics.ulinkLoad,
		hspotap->wanmetrics.lmd);
}

static void
encode_hspot_op_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t name; int i;

	bcm_encode_init(&name, sizeof(buffer), buffer);

	for (i = 0; i < hspotap->oplist.numName; i++) {
		bcm_encode_hspot_anqp_operator_name_duple(&name,
			hspotap->oplist.duple[i].langLen, hspotap->oplist.duple[i].lang,
			hspotap->oplist.duple[i].nameLen, hspotap->oplist.duple[i].name);
	}

	bcm_encode_hspot_anqp_operator_friendly_name(pkt,
		bcm_encode_length(&name), bcm_encode_buf(&name));
}

static void
encode_hspot_homeq_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t name; int i;

	bcm_encode_init(&name, sizeof(buffer), buffer);

	for (i = 0; i < hspotap->homeqlist.count; i++) {
		bcm_encode_hspot_anqp_nai_home_realm_name(&name,
			hspotap->homeqlist.data[i].encoding,
			hspotap->homeqlist.data[i].nameLen,
			hspotap->homeqlist.data[i].name);
	}

	pktEncodeHspotAnqpNaiHomeRealmQuery(pkt, hspotap->homeqlist.count,
		bcm_encode_length(&name), bcm_encode_buf(&name));
}

static void
encode_hspot_homeq_list_ex(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 tlsAuthBuf[BUFF_256] = {0};
	bcm_encode_t tlsAuth;
	uint8 tlsEapBuf[BUFF_256] = {0};
	bcm_encode_t tlsEap;
	uint8 realmBuf[BUFF_256] = {0};
	bcm_encode_t realm;
	uint8 credential;

	/* TLS - certificate */
	bcm_encode_init(&tlsAuth, sizeof(tlsAuthBuf), tlsAuthBuf);
	credential = REALM_CERTIFICATE;
	bcm_encode_anqp_authentication_subfield(&tlsAuth,
		REALM_CREDENTIAL, sizeof(credential), &credential);
	bcm_encode_init(&tlsEap, sizeof(tlsEapBuf), tlsEapBuf);
	bcm_encode_anqp_eap_method_subfield(&tlsEap, REALM_EAP_TLS,
		1, bcm_encode_length(&tlsAuth), bcm_encode_buf(&tlsAuth));

	bcm_encode_init(&realm, sizeof(realmBuf), realmBuf);

	/* example */
	bcm_encode_anqp_nai_realm_data(&realm, REALM_ENCODING_RFC4282,
		strlen(HOME_REALM), (uint8 *)HOME_REALM, 1,
		bcm_encode_length(&tlsEap), bcm_encode_buf(&tlsEap));
	bcm_encode_anqp_nai_realm(pkt, 1,
		bcm_encode_length(&realm), bcm_encode_buf(&realm));
}

static void
encode_hspot_conncap_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 buffer[BUFF_256] = {0};
	bcm_encode_t cap;
	int iter;

	bcm_encode_init(&cap, sizeof(buffer), buffer);
	for (iter = 0; iter < hspotap->concaplist.numConnectCap; iter++) {
		bcm_encode_hspot_anqp_proto_port_tuple(&cap,
			hspotap->concaplist.tuple[iter].ipProtocol,
			hspotap->concaplist.tuple[iter].portNumber,
			hspotap->concaplist.tuple[iter].status);

		bcm_encode_hspot_anqp_connection_capability(pkt,
			bcm_encode_length(&cap), bcm_encode_buf(&cap));
	}
}

static void
encode_hspot_osup_list(hspotApT *hspotap, bcm_encode_t *pkt)
{
	uint8 osuBuf[BUFF_4K], iconBuf[BUFF_256];
	uint8 nameBuf[BUFF_256], descBuf[BUFF_256];
	bcm_encode_t osu, icon, name, desc;
	int ip = 0, ic = 0, in = 0, id = 0;

	bcm_encode_init(&osu, sizeof(osuBuf), osuBuf);
	for (ip = 0; ip < hspotap->osuplist.osuProviderCount; ip++) {

		/* Encode OSU_Friendly_Name */
		bcm_encode_init(&name, sizeof(nameBuf), nameBuf);
		for (in = 0; in < hspotap->osuplist.osuProvider[ip].name.numName; in++) {
			bcm_encode_hspot_anqp_operator_name_duple(&name,
				hspotap->osuplist.osuProvider[ip].name.duple[in].langLen,
				(char *)hspotap->osuplist.osuProvider[ip].name.duple[in].lang,
				hspotap->osuplist.osuProvider[ip].name.duple[in].nameLen,
				(char *)hspotap->osuplist.osuProvider[ip].name.duple[in].name);
		}
		/* Encode OSU_Icons */
		bcm_encode_init(&icon, sizeof(iconBuf), iconBuf);
		for (ic = 0; ic < hspotap->osuplist.osuProvider[ip].iconMetadataCount; ic++) {
			bcm_encode_hspot_anqp_icon_metadata(&icon,
				hspotap->osuplist.osuProvider[ip].iconMetadata[ic].width,
				hspotap->osuplist.osuProvider[ip].iconMetadata[ic].height,
				(char *)hspotap->osuplist.osuProvider[ip].iconMetadata[ic].lang,
				hspotap->osuplist.osuProvider[ip].iconMetadata[ic].typeLength,
				(uint8 *)hspotap->osuplist.osuProvider[ip].iconMetadata[ic].type,
				hspotap->osuplist.osuProvider[ip].iconMetadata[ic].filenameLength,
				(uint8 *)
				hspotap->osuplist.osuProvider[ip].iconMetadata[ic].filename);
		}
		/* Encode OSU_Friendly_Name */
		bcm_encode_init(&desc, sizeof(descBuf), descBuf);
		for (id = 0; id < hspotap->osuplist.osuProvider[ip].desc.numName; id++) {
			bcm_encode_hspot_anqp_operator_name_duple(&desc,
				hspotap->osuplist.osuProvider[ip].desc.duple[id].langLen,
				(char *)hspotap->osuplist.osuProvider[ip].desc.duple[id].lang,
				hspotap->osuplist.osuProvider[ip].desc.duple[id].nameLen,
				(char *)hspotap->osuplist.osuProvider[ip].desc.duple[id].name);
		}

		/* Encode Provider */
		bcm_encode_hspot_anqp_osu_provider(&osu,
			bcm_encode_length(&name), bcm_encode_buf(&name),
			hspotap->osuplist.osuProvider[ip].uriLength,
			(uint8 *)hspotap->osuplist.osuProvider[ip].uri,
			hspotap->osuplist.osuProvider[ip].methodLength,
			(uint8 *)hspotap->osuplist.osuProvider[ip].method,
			bcm_encode_length(&icon), bcm_encode_buf(&icon),
			hspotap->osuplist.osuProvider[ip].naiLength,
			(uint8 *)hspotap->osuplist.osuProvider[ip].nai,
			bcm_encode_length(&desc), bcm_encode_buf(&desc));
	}

	/* Encode Providers List */
	bcm_encode_hspot_anqp_osu_provider_list(pkt,
		strlen((char*)hspotap->osuplist.osuSsid),
		(uint8 *)hspotap->osuplist.osuSsid,
		hspotap->osuplist.osuProviderCount,
		bcm_encode_length(&osu), bcm_encode_buf(&osu));
}
/* ======================== ENCODING FUNCTIONS ============================ */


/* ====================== COMMAND HANDLER FUNCTIONS ======================== */
static int
hspot_cmd_interface_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	hspotApT *new_hspotap = NULL;

	new_hspotap = get_hspotap_by_ifname(argv[0]);
	if (new_hspotap == NULL) {
		HS20_ERROR(HSERR_WRONG_IFNAME, argv[0]);
		err = -1;
	} else {
		current_hspotap = new_hspotap;
		err = 0;
		HS20_ERROR(HSERR_CHANGE_IFNAME_TO, wl_ifname(current_hspotap->ifr));
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_bss_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	hspotApT *new_hspotap = NULL;

	new_hspotap = get_hspotap_by_bssid(argv[0]);
	if (new_hspotap == NULL) {
		HS20_ERROR(HSERR_WRONG_IFNAME, argv[0]);
		err = -1;
	} else {
		current_hspotap = new_hspotap;
		err = 0;
		HS20_ERROR(HSERR_CHANGE_IFNAME_TO, wl_ifname(current_hspotap->ifr));
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_interworking_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool enabled = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_INTERWORKING);
		return -1;
	}

	enabled = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_INTERWORKING, enabled);

	if (hspotap->iw_ie_enabled != enabled) {

		if (hspotap->iw_ie_enabled) {
			deleteIes_hspot(hspotap);
			deleteIes_u11(hspotap);

			hspotap->hs_ie_enabled = enabled;
			err = set_hspot_flag(hspotap->prefix, HSFLG_HS_EN, hspotap->hs_ie_enabled);
		}

		hspotap->iw_ie_enabled = enabled;

		if (enabled) {
			addIes_u11(hspotap);
		}
		err = set_hspot_flag(hspotap->prefix, HSFLG_U11_EN, hspotap->iw_ie_enabled);
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_accs_net_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ACCS_NET_TYPE);
		return -1;
	}

	hspotap->iwIe.accessNetworkType = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_ACCS_NET_TYPE, hspotap->iwIe.accessNetworkType);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iwIe.accessNetworkType);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IWNETTYPE, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();

	err = update_iw_ie(hspotap, FALSE);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_internet_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_INTERNET);
		return -1;
	}

	hspotap->iwIe.isInternet = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_INTERNET, hspotap->iwIe.isInternet);
	set_hspot_flag(hspotap->prefix, HSFLG_IWINT_EN, hspotap->iwIe.isInternet);
	err = update_iw_ie(hspotap, FALSE);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_asra_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ASRA);
		return -1;
	}

	hspotap->iwIe.isAsra = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_ASRA, hspotap->iwIe.isAsra);
	set_hspot_flag(hspotap->prefix, HSFLG_IWASRA_EN, hspotap->iwIe.isAsra);
	err = update_iw_ie(hspotap, FALSE);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_venue_grp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_VENUE_GRP);
		return -1;
	}

	hspotap->venuelist.group = atoi(argv[0]);
	hspotap->iwIe.venueGroup = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_VENUE_GRP, hspotap->venuelist.group);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->venuelist.group);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_VENUEGRP, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();

	err = update_iw_ie(hspotap, FALSE);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_venue_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_VENUE_TYPE);
		return -1;
	}

	hspotap->venuelist.type = atoi(argv[0]);
	hspotap->iwIe.venueType = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_VENUE_TYPE, hspotap->venuelist.type);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->venuelist.type);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_VENUETYPE, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();

	err = update_iw_ie(hspotap, FALSE);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_hessid_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		hspotap->iwIe.isHessid = FALSE;
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_HESSID);
	} else {
		hspotap->iwIe.isHessid = TRUE;
		if (!strToEther(argv[0], &hspotap->iwIe.hessid)) {
			HS20_ERROR(HSERR_ARGV_MAC_ERROR);
			return -1;
		}

		snprintf(varvalue, sizeof(varvalue), MAC_FORMAT,
			hspotap->iwIe.hessid.octet[0], hspotap->iwIe.hessid.octet[1],
			hspotap->iwIe.hessid.octet[2], hspotap->iwIe.hessid.octet[3],
			hspotap->iwIe.hessid.octet[4], hspotap->iwIe.hessid.octet[5]);
		HS20_INFO(PRNT_STR, HSCMD_HESSID, varvalue);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_HESSID, varname), varvalue);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
			err = -1;
		}
		nvram_commit();
	}

	err = update_iw_ie(hspotap, FALSE);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_roaming_cons_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, i = 0;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	int ret, data_len = 0;


	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ROAMING_CONS);
		return -1;
	}

	if (!strncasecmp(argv[0], DISABLED_S, strlen(DISABLED_S))) {
		HS20_INFO(HSERR_RC_OI_NOT_PRESENT);
		reset_u11_oui_list(hspotap, FALSE);
	} else {
		strncpy_n(varvalue, EMPTY_STR, NVRAM_MAX_VALUE_LEN);
		memset(&hspotap->ouilist, 0, sizeof(bcm_decode_anqp_roaming_consortium_ex_t));
		hspotap->ouilist.isDecodeValid = TRUE;
		hspotap->ouilist.numOi = 0;

		while (argv[i]) {
			data_len = strlen(argv[i]) / 2;

			if (data_len && (data_len <= BCM_DECODE_ANQP_MAX_OI_LENGTH)) {

				get_hex_data((uchar *)argv[i], hspotap->ouilist.oi[i].oi, data_len);
				hspotap->ouilist.oi[i].oiLen = data_len;
				hspotap->ouilist.oi[i].isBeacon = (i < 3) ? 1 : 0;

				if (i)
					strncat(varvalue, ";", min(1,
					NVRAM_MAX_VALUE_LEN-strlen(varvalue)));
				strncat(varvalue, argv[i], min(strlen(argv[i]),
					NVRAM_MAX_VALUE_LEN-strlen(varvalue)));
				strncat(varvalue, (i < 3) ? ":1" : ":0", min(2,
					NVRAM_MAX_VALUE_LEN - strlen(varvalue)));

				i++;
			}
		}
		hspotap->ouilist.numOi = i;

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OUILIST, varname), varvalue);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
			err = -1;
		}
		nvram_commit();
	}

	err = update_rc_ie(hspotap);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_anqp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ap_ANQPenabled;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ANQP);
		return -1;
	}

	ap_ANQPenabled = (atoi(argv[0]) != 0);
	set_hspot_flag(hspotap->prefix, HSFLG_ANQP, ap_ANQPenabled);
	HS20_INFO(PRNT_FLG, HSCMD_ANQP, ap_ANQPenabled);
	err = update_ap_ie(hspotap);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_mih_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ap_MIHenabled;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_MIH);
		return -1;
	}

	ap_MIHenabled = (atoi(argv[0]) != 0);
	set_hspot_flag(hspotap->prefix, HSFLG_MIH, ap_MIHenabled);
	HS20_INFO(PRNT_FLG, HSCMD_MIH, ap_MIHenabled);

	err = update_ap_ie(hspotap);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_dgaf_disable_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	bcm_encode_t ie;
	int err = 0;
	bool inflag, isDgafDisabled;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_DGAF_DISABLE);
		return -1;
	}

	isDgafDisabled = get_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS);
	inflag = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_DGAF_DISABLE, inflag);

	if (isDgafDisabled != inflag) {
		set_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS, inflag);

		/* delete Passpoint vendor IE */
		if (hspotap->hs_ie_enabled)
			wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

		/* encode Passpoint vendor IE */
		bcm_encode_init(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
		bcm_encode_ie_hotspot_indication2(&ie, inflag, hspotap->hs_capable,
			FALSE, 0, FALSE, 0);

		if (hspotap->hs_ie_enabled) {
			/* don't need first 2 bytes (0xdd + len) */
			if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0) {
				HS20_ERROR(HSERR_ADD_VENDR_IE_FAILED);
			}
		}

		err = update_dgaf_disable(hspotap);
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_l2_traffic_inspect_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, l2_traffic_inspect;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_L2_TRAFFIC_INSPECT);
		return -1;
	}

	l2_traffic_inspect = (atoi(argv[0]) != 0);
	set_hspot_flag(hspotap->prefix, HSFLG_L2_TRF, l2_traffic_inspect);
	HS20_INFO(PRNT_FLG, HSCMD_L2_TRAFFIC_INSPECT, l2_traffic_inspect);
	err = update_l2_traffic_inspect(hspotap);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_icmpv4_echo_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, icmpv4_echo, l2_traffic_inspect;
	l2_traffic_inspect = get_hspot_flag(hspotap->prefix, HSFLG_L2_TRF);

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ICMPV4_ECHO);
		return -1;
	}

	icmpv4_echo = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_ICMPV4_ECHO, icmpv4_echo);
	set_hspot_flag(hspotap->prefix, HSFLG_ICMPV4_ECHO, icmpv4_echo);

	if (l2_traffic_inspect) {
		if (wl_block_ping(hspotap->ifr, !icmpv4_echo) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_PING);
		}
	}

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_plmn_mcc_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, i = 0;
	char item_value[NVRAM_MAX_VALUE_LEN] = {0};
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_PLMN_MCC);
		return -1;
	}

	hspotap->gpp3list.plmnCount = 0;
	while ((i < BCM_DECODE_ANQP_MAX_PLMN) && argv[i]) {

		memset(item_value, 0, sizeof(item_value));

		if (strlen(argv[i]) > BCM_DECODE_ANQP_MCC_LENGTH) {
			HS20_ERROR(HSERR_WRONG_LENGTH_MCC, strlen(argv[i]));
			hspotap->gpp3list.plmnCount = 0;
			return -1;
		}

		strncpy_n(hspotap->gpp3list.plmn[i].mcc, argv[i], BCM_DECODE_ANQP_MCC_LENGTH + 1);
		HS20_INFO(PRNT_STR, HSCMD_PLMN_MCC, hspotap->gpp3list.plmn[i].mcc);

		if (!strlen(hspotap->gpp3list.plmn[i].mnc))
			strncpy_n(hspotap->gpp3list.plmn[i].mnc, DEFAULT_CODE,
			BCM_DECODE_ANQP_MCC_LENGTH + 1);

		strncat(item_value, hspotap->gpp3list.plmn[i].mcc,
			min(BCM_DECODE_ANQP_MCC_LENGTH, NVRAM_MAX_VALUE_LEN-strlen(item_value)));
		strncat(item_value, ":", min(1, NVRAM_MAX_VALUE_LEN-strlen(item_value)));
		strncat(item_value, hspotap->gpp3list.plmn[i].mnc,
			min(BCM_DECODE_ANQP_MNC_LENGTH, NVRAM_MAX_VALUE_LEN-strlen(item_value)));

		if (i)
			strncat(varvalue, ";", min(1,
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));
		strncat(varvalue, item_value, min(strlen(item_value),
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));

		i++;
	}
	hspotap->gpp3list.plmnCount = i;

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_3GPPLIST, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_plmn_mnc_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, i = 0;
	char item_value[NVRAM_MAX_VALUE_LEN] = {0};
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_PLMN_MNC);
		return -1;
	}

	hspotap->gpp3list.plmnCount = 0;
	while ((i < BCM_DECODE_ANQP_MAX_PLMN) && argv[i]) {

		memset(item_value, 0, sizeof(item_value));

		if (strlen(argv[i]) > BCM_DECODE_ANQP_MNC_LENGTH) {
			HS20_ERROR(HSERR_WRONG_LENGTH_MNC, strlen(argv[i]));
			hspotap->gpp3list.plmnCount = 0;
			return -1;
		}

		strncpy_n(hspotap->gpp3list.plmn[i].mnc, argv[i], BCM_DECODE_ANQP_MNC_LENGTH + 1);
		HS20_INFO(PRNT_STR, HSCMD_PLMN_MNC, hspotap->gpp3list.plmn[i].mnc);

		if (!strlen(hspotap->gpp3list.plmn[i].mcc))
			strncpy_n(hspotap->gpp3list.plmn[i].mcc, DEFAULT_CODE,
			BCM_DECODE_ANQP_MNC_LENGTH + 1);

		strncat(item_value, hspotap->gpp3list.plmn[i].mcc,
			min(BCM_DECODE_ANQP_MCC_LENGTH, NVRAM_MAX_VALUE_LEN-strlen(item_value)));
		strncat(item_value, ":", min(1, NVRAM_MAX_VALUE_LEN-strlen(item_value)));
		strncat(item_value, hspotap->gpp3list.plmn[i].mnc,
			min(BCM_DECODE_ANQP_MNC_LENGTH, NVRAM_MAX_VALUE_LEN-strlen(item_value)));

		if (i)
			strncat(varvalue, ";", min(1,
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));
		strncat(varvalue, item_value, min(strlen(item_value),
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));

		i++;
	}
	hspotap->gpp3list.plmnCount = i;

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_3GPPLIST, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_proxy_arp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, proxy_arp = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_PROXY_ARP);
		return -1;
	}

	proxy_arp = (atoi(argv[0]) != 0);
	set_hspot_flag(hspotap->prefix, HSFLG_PROXY_ARP, proxy_arp);
	HS20_INFO(PRNT_FLG, HSCMD_PROXY_ARP, proxy_arp);
	err = update_proxy_arp(hspotap);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_bcst_uncst_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool bcst_uncst;
	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_BCST_UNCST);
		return -1;
	}

	bcst_uncst = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_BCST_UNCST, bcst_uncst);

	if (wl_dhcp_unicast(hspotap->ifr, bcst_uncst) < 0) {
		err = -1;
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DHCP_UNICAST);
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_gas_cb_delay_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int gas_cb_delay;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	int ret;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_GAS_CB_DELAY);
		return -1;
	}

	gas_cb_delay = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_GAS_CB_DELAY, gas_cb_delay);

	if (gas_cb_delay) {
		hspotap->isGasPauseForServerResponse = FALSE;
		bcm_gas_set_if_cb_delay_unpause(gas_cb_delay, hspotap->ifr);
	} else {
		hspotap->isGasPauseForServerResponse = TRUE;
	}
	bcm_gas_set_if_gas_pause(hspotap->isGasPauseForServerResponse, hspotap->ifr);

	hspotap->gas_cb_delay = gas_cb_delay;

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->gas_cb_delay);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_GASCBDEL, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_4_frame_gas_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_4_FRAME_GAS);
		return -1;
	}

	hspotap->isGasPauseForServerResponse = (atoi(argv[0]) == 0);
	HS20_INFO(PRNT_FLG, HSCMD_4_FRAME_GAS, !(hspotap->isGasPauseForServerResponse));
	bcm_gas_set_if_gas_pause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
	err = set_hspot_flag(hspotap->prefix, HSFLG_4FRAMEGAS,
		!(hspotap->isGasPauseForServerResponse));
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_domain_list_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, i = 0;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	int ret;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_DOMAIN_LIST);
		return -1;
	}

	strncpy_n(varvalue, EMPTY_STR, NVRAM_MAX_VALUE_LEN);
	memset(&hspotap->domainlist, 0, sizeof(bcm_decode_anqp_domain_name_list_t));
	hspotap->domainlist.isDecodeValid = TRUE;
	hspotap->domainlist.numDomain = 0;

	while (argv[i]) {
		strncpy_n(hspotap->domainlist.domain[i].name, argv[i],
			BCM_DECODE_ANQP_MAX_DOMAIN_NAME_SIZE+1);
		hspotap->domainlist.domain[i].len = strlen(argv[i]);

		if (i)
			strncat(varvalue, " ", min(1,
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));
		strncat(varvalue, argv[i], min(strlen(argv[i]),
			NVRAM_MAX_VALUE_LEN-strlen(varvalue)));

		i++;
	}
	hspotap->domainlist.numDomain = i;

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_DOMAINLIST, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_sess_info_url_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_SESS_INFO_URL);
		return -1;
	}

	if (hspotap->url_len)
		free(hspotap->url);

	hspotap->url_len = strlen(argv[0]);
	if (hspotap->url_len == 0) {
		HS20_INFO(HSERR_SESINFO_URL_EMPTY);
		wl_wnm_url(hspotap->ifr, 0, 0);
		return err;
	}

	hspotap->url = malloc(hspotap->url_len + 1);
	if (hspotap->url == NULL) {
		hspotap->url_len = 0;
		wl_wnm_url(hspotap->ifr, 0, 0);
		HS20_ERROR(HSERR_MALLOC_FAILED);
		return -1;
	}

	strncpy_n((char *)hspotap->url, argv[0], hspotap->url_len + 1);
	err = wl_wnm_url(hspotap->ifr, hspotap->url_len, hspotap->url);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_send_BTM_Req_frame(hspotApT *hspotap, struct ether_addr *da)
{
	int err = 0;
	dot11_bsstrans_req_t *transreq;
	wnm_url_t *url;
	uint16 len;

	len = DOT11_BSSTRANS_REQ_LEN + hspotap->url_len + 1;
	transreq = (dot11_bsstrans_req_t *)malloc(len);
	if (transreq == NULL) {
		HS20_ERROR(HSERR_MALLOC_FAILED);
		return -1;
	}
	transreq->category = DOT11_ACTION_CAT_WNM;
	transreq->action = DOT11_WNM_ACTION_BSSTRANS_REQ;
	transreq->token = hspotap->req_token;
	transreq->reqmode = DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT;
	transreq->disassoc_tmr = 0;
	transreq->validity_intrvl = 0;
	url = (wnm_url_t *)&transreq->data[0];
	url->len = hspotap->url_len;
	if (hspotap->url_len) {
		memcpy(url->data, hspotap->url, hspotap->url_len);
	}

	if (wl_wifiaction(hspotap->ifr, (uint32)hspotap, da, len, (uint8 *)transreq) < 0) {
		err = -1;
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_WIFIACTION);
	}

	hspotap->req_token++;
	if (hspotap->req_token == 0)
		hspotap->req_token = 1;

	free(transreq);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_dest_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	struct ether_addr da;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_DEST);
		return -1;
	}

	if (!strToEther(argv[0], &da)) {
		HS20_ERROR(HSERR_ARGV_MAC_ERROR);
		return -1;
	}

	err = hspot_send_BTM_Req_frame(hspotap, &da);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_hs2_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool enabled = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_HS2);
		return -1;
	}

	enabled = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_HS2, enabled);

	if (hspotap->hs_ie_enabled != enabled) {
		if (hspotap->hs_ie_enabled) {
			deleteIes_hspot(hspotap);
		}

		hspotap->hs_ie_enabled = enabled;

		if (enabled) {
			hspotap->iw_ie_enabled = enabled;
			addIes(hspotap);
			set_hspot_flag(hspotap->prefix, HSFLG_U11_EN, hspotap->iw_ie_enabled);
		}
		set_hspot_flag(hspotap->prefix, HSFLG_HS_EN, hspotap->hs_ie_enabled);
	}

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_p2p_ie_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	int err = 0, p2p_ie_enabled, p2p_cross_enabled;
	bool enabled = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_P2P_IE);
		return -1;
	}

	p2p_ie_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P);
	p2p_cross_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS);
	enabled = (atoi(argv[0]) != 0);

	HS20_INFO(PRNT_FLG, HSCMD_P2P_IE, enabled);
	if (p2p_ie_enabled != enabled) {

		/* delete P2P vendor IE */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = p2p_cross_enabled ? 0x03 : 0x01;

		if (enabled) {
			if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
				vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0) {
				err = -1;
				HS20_ERROR(HSERR_ADD_VENDR_IE_FAILED);
			}
		}
		set_hspot_flag(hspotap->prefix, HSFLG_P2P, enabled);
	}

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_p2p_cross_connect_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	int err = 0, p2p_cross_enabled;
	bool enabled = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_P2P_CROSS_CONNECT);
		return -1;
	}

	p2p_cross_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS);
	enabled = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_P2P_CROSS_CONNECT, enabled);

	if (p2p_cross_enabled != enabled) {
		/* delete P2P vendor IE */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = enabled ? 0x03 : 0x01;

		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0) {
			err = -1;
			HS20_ERROR(HSERR_ADD_VENDR_IE_FAILED);
		}
		set_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS, enabled);
	}

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osu_provider_list_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret, osup_id = 1, iter = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 osu_method[1] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSU_PROVIDER_LIST);
		osup_id = 1;
	} else {
		osup_id = atoi(argv[0]);
		HS20_INFO(PRNT_FLG, HSCMD_OSU_PROVIDER_LIST, osup_id);
	}

	memset(&hspotap->osuplist, 0, sizeof(hspotap->osuplist));

	switch (osup_id)
	{
	case 1:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;

		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);
		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_RED, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_RED);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_red, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_red);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID1_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname,
				OPLIST_ID1_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 2;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength
					= strlen(ICON_TYPE_ID1);
		strncpy_n((char*)
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_RED_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_RED_ZXX);
		/* Icon Metadata 2 */
		hspotap->osuplist.osuProvider[0].iconMetadata[1].width = 160;
		hspotap->osuplist.osuProvider[0].iconMetadata[1].height = 76;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].type,
			ICON_TYPE_ID1,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].typeLength
				= strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].filename,
			ICON_FILENAME_RED,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].filenameLength =
			strlen(ICON_FILENAME_RED);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID1_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname,
			OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 2:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;

		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);
		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_WBA, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_WBA);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_wba, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_wba);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID2_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID2_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID2_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID2_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 3:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 1;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			SPANISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(SPANISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_RED, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_RED);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID3_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID3_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_RED_ZXX, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_RED_ZXX);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID3_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID3_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 1;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			SPANISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(SPANISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID3_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID3_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 4:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_ORANGE, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_ORANGE);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_orng, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_orng);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID4_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID4_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 2;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);
		/* Icon Metadata 2 */
		hspotap->osuplist.osuProvider[0].iconMetadata[1].width = 160;
		hspotap->osuplist.osuProvider[0].iconMetadata[1].height = 76;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].filename,
			ICON_FILENAME_ORANGE, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].filenameLength =
			strlen(ICON_FILENAME_ORANGE);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID4_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID4_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 5:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_ORANGE, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_ORANGE);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_orng, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_orng);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID5_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID5_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID5_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID5_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 6:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 2;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* PROVIDER 1 */
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_RED, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_RED);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_red, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_red);
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_OMA_DM;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_RED_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_RED_ZXX);
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		/* OSU_Server_Desc */
		memset(&hspotap->osuplist.osuProvider[0].desc, 0,
			sizeof(hspotap->osuplist.osuProvider[0].desc));

		/* PROVIDER 2 */
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[1].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[1].name.numName = 2;
		strncpy_n(hspotap->osuplist.osuProvider[1].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[1].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n(hspotap->osuplist.osuProvider[1].name.duple[0].name,
			ENG_OPNAME_SP_ORANGE, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[1].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_ORANGE);
		strncpy_n(hspotap->osuplist.osuProvider[1].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[1].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[1].name.duple[1].name,
			(char *)kor_opname_sp_orng, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[1].name.duple[1].nameLen =
			sizeof(kor_opname_sp_orng);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID6_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID6_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[1].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[1].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID6_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID6_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_OMA_DM;
		memcpy(hspotap->osuplist.osuProvider[1].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[1].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			OPLIST_ID6_OSU_METHOD);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID6_OSU_METHOD);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[1].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[1].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[1].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[1].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[1].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[1].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[1].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[1].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID6_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID6_OSU_ICONS);
			err = -1;
		}

		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[1].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[1].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		memset(&hspotap->osuplist.osuProvider[1].desc, 0,
			sizeof(hspotap->osuplist.osuProvider[1].desc));

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
	}
	break;

	case 7:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID1_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID1_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_ORANGE, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_ORANGE);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_orng, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_orng);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID7_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID7_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_OMA_DM;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			OMADM_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OMADM_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 2;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);
		/* Icon Metadata 2 */
		hspotap->osuplist.osuProvider[0].iconMetadata[1].width = 160;
		hspotap->osuplist.osuProvider[0].iconMetadata[1].height = 76;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[1].filename,
			ICON_FILENAME_ORANGE, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[1].filenameLength =
			strlen(ICON_FILENAME_ORANGE);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID7_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID7_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, EMPTY_STR,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = 0;
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 8:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID8_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID8_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID8_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID8_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_RED, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_RED);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n(hspotap->osuplist.osuProvider[0].name.duple[1].name,
			(char *)kor_opname_sp_red, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[1].nameLen =
			sizeof(kor_opname_sp_red);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID1_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_RED_ZXX, BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_RED_ZXX);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID3_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID3_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, OSU_NAI_ANON_HS,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = strlen(OSU_NAI_ANON_HS);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			OPLIST_ID8_OSU_NAI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID8_OSU_NAI);
			err = -1;
		}
		/* OSU_Server_Desc */
		hspotap->osuplist.osuProvider[0].desc.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].desc.numName = 2;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[0].name,
			OSU_SERVICE_DESC_ID1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[0].nameLen =
			strlen(OSU_SERVICE_DESC_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].lang,
			KOREAN, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].langLen =
			strlen(KOREAN);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].desc.duple[1].name,
			(char *)kor_desc_name_id1, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].desc.duple[1].nameLen =
			sizeof(kor_desc_name_id1);

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			OPLIST_ID1_OSU_SERV_DESC);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_SERV_DESC);
			err = -1;
		}
	}
	break;

	case 9:
	{
		hspotap->osuplist.isDecodeValid = TRUE;
		hspotap->osuplist.osuProviderCount = 1;
		/* OSU_ICON_ID */
		hspotap->osuplist.osuicon_id = 1;
		set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID, 0);

		/* OSU_SSID */
		strncpy_n((char*)hspotap->osuplist.osuSsid, OPLIST_ID9_OSU_SSID,
			BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID9_OSU_SSID);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
			OPLIST_ID9_OSU_SSID);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID9_OSU_SSID);
			err = -1;
		}
		/* OSU_Friendly_Name */
		hspotap->osuplist.osuProvider[0].name.isDecodeValid = TRUE;
		hspotap->osuplist.osuProvider[0].name.numName = 1;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].lang,
			ENGLISH, VENUE_LANGUAGE_CODE_SIZE +1);
		hspotap->osuplist.osuProvider[0].name.duple[0].langLen =
			strlen(ENGLISH);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].name.duple[0].name,
			ENG_OPNAME_SP_ORANGE, VENUE_NAME_SIZE + 1);
		hspotap->osuplist.osuProvider[0].name.duple[0].nameLen =
			strlen(ENG_OPNAME_SP_ORANGE);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_FRNDNAME, varname),
			OPLIST_ID9_OSU_FRNDLY_NAME);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID9_OSU_FRNDLY_NAME);
			err = -1;
		}
		/* OSU_Server_URI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].uri, OPLIST_ID1_OSU_URI,
			BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].uriLength =
			strlen(OPLIST_ID1_OSU_URI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname),
			OPLIST_ID1_OSU_URI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID1_OSU_URI);
			err = -1;
		}
		/* OSU_Method */
		osu_method[0] = HSPOT_OSU_METHOD_SOAP_XML;
		memcpy(hspotap->osuplist.osuProvider[0].method,
			osu_method, sizeof(osu_method));
		hspotap->osuplist.osuProvider[0].methodLength = sizeof(osu_method);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname),
			SOAP_NVVAL);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, SOAP_NVVAL);
			err = -1;
		}
		/* OSU_Icons */
		hspotap->osuplist.osuProvider[0].iconMetadataCount = 1;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].width = 128;
		hspotap->osuplist.osuProvider[0].iconMetadata[0].height = 61;
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].lang,
			LANG_ZXX, VENUE_LANGUAGE_CODE_SIZE +1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].type,
			ICON_TYPE_ID1, BCM_DECODE_HSPOT_ANQP_MAX_ICON_TYPE_LENGTH +1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].typeLength = strlen(ICON_TYPE_ID1);
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].iconMetadata[0].filename,
			ICON_FILENAME_ORANGE_ZXX,
			BCM_DECODE_HSPOT_ANQP_MAX_ICON_FILENAME_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].iconMetadata[0].filenameLength =
			strlen(ICON_FILENAME_ORANGE_ZXX);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_ICONS, varname),
			OPLIST_ID9_OSU_ICONS);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID9_OSU_ICONS);
			err = -1;
		}
		/* OSU_NAI */
		strncpy_n((char*)hspotap->osuplist.osuProvider[0].nai, OSU_NAI_TEST_WIFI,
			BCM_DECODE_HSPOT_ANQP_MAX_NAI_LENGTH + 1);
		hspotap->osuplist.osuProvider[0].naiLength = strlen(OSU_NAI_TEST_WIFI);
		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_NAI, varname),
			OPLIST_ID9_OSU_NAI);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, OPLIST_ID9_OSU_NAI);
			err = -1;
		}
		/* OSU_Server_Desc */
		memset(&hspotap->osuplist.osuProvider[0].desc, 0,
			sizeof(hspotap->osuplist.osuProvider[0].desc));

		ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SERVDESC, varname),
			EMPTY_STR);
		if (ret) {
			HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
			err = -1;
		}
	}
	break;
	}

	for (iter = hspotap->osuplist.osuProviderCount; iter < MAX_OSU_PROVIDERS; iter++)
		memset(&hspotap->osuplist.osuProvider[iter], 0,
			sizeof(hspotap->osuplist.osuProvider[iter]));

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osu_icon_tag_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSU_ICON_TAG);
		hspotap->osuplist.osuicon_id = 1;
	} else {
		hspotap->osuplist.osuicon_id = atoi(argv[0]);
		HS20_INFO(PRNT_FLG, HSCMD_OSU_ICON_TAG, hspotap->osuplist.osuicon_id);
	}
	err = set_hspot_flag(hspotap->prefix, HSFLG_OSUICON_ID,
		hspotap->osuplist.osuicon_id - 1);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osu_server_uri_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, i = 0, ret, iter;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	char varvalue[BUFF_512] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSU_SERVER_URI);
		strncpy_n(varvalue, OPLIST_ID1_OSU_URI, BUFF_512);
	} else {
		while (argv[i]) {
			strncpy_n((char*)hspotap->osuplist.osuProvider[i].uri,
				argv[i], BCM_DECODE_HSPOT_ANQP_MAX_URI_LENGTH + 1);

			hspotap->osuplist.osuProvider[i].uriLength = strlen(argv[i]);

			if (i)
				strncat(varvalue, ";",
					min(1, BUFF_512 - strlen(varvalue)));

			strncat(varvalue, argv[i],
				min(strlen(argv[i]), BUFF_512 - strlen(varvalue)));

			i++;
		}
	}

	for (iter = i; iter < MAX_OSU_PROVIDERS; iter++) {
		memset(hspotap->osuplist.osuProvider[iter].uri, 0,
			sizeof(hspotap->osuplist.osuProvider[iter].uri));
		hspotap->osuplist.osuProvider[iter].uriLength = 0;
	}

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_URI, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osu_method_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, i = 0, ret, iter;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	char varvalue[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 osu_method[1] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSU_METHOD);
		strncpy_n(varvalue, SOAP_S, NVRAM_MAX_PARAM_LEN);
	} else {
		while (argv[i]) {
			osu_method[0] = (!strncasecmp(argv[i], OMADM_S, strlen(OMADM_S))) ?
				HSPOT_OSU_METHOD_OMA_DM : HSPOT_OSU_METHOD_SOAP_XML;
			memcpy(hspotap->osuplist.osuProvider[i].method,
				osu_method, sizeof(osu_method));
			hspotap->osuplist.osuProvider[i].methodLength = sizeof(osu_method);
			if (i)
				strncat(varvalue, ";",
					min(1, NVRAM_MAX_PARAM_LEN - strlen(varvalue)));

			strncat(varvalue, (!strncasecmp(argv[i], OMADM_S, strlen(OMADM_S))) ?
				OMADM_NVVAL : SOAP_NVVAL,
				min(1, NVRAM_MAX_PARAM_LEN - strlen(varvalue)));

			i++;
		}
	}

	for (iter = i; iter < MAX_OSU_PROVIDERS; iter++) {
		memset(hspotap->osuplist.osuProvider[iter].method, 0,
			sizeof(hspotap->osuplist.osuProvider[iter].method));
		hspotap->osuplist.osuProvider[iter].methodLength = 0;
	}

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_METHOD, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osu_ssid_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	hspotap->osuplist.isDecodeValid = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSU_SSID);
		strncpy_n((char*)hspotap->osuplist.osuSsid,
			OPLIST_ID1_OSU_SSID, BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(OPLIST_ID1_OSU_SSID);
	} else {
		strncpy_n((char*)hspotap->osuplist.osuSsid,
			argv[0], BCM_DECODE_HSPOT_ANQP_MAX_OSU_SSID_LENGTH + 1);
		hspotap->osuplist.osuSsidLength = strlen(argv[0]);
	}

	HS20_INFO(PRNT_STR, HSCMD_OSU_SSID, hspotap->osuplist.osuSsid);

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OSU_SSID, varname),
		(char*)hspotap->osuplist.osuSsid);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, hspotap->osuplist.osuSsid);
		err = -1;
	}
	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_anonymous_nai_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	int ret;

	memset(&hspotap->anonai, 0, sizeof(bcm_decode_hspot_anqp_anonymous_nai_t));
	hspotap->anonai.isDecodeValid = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_ANONYMOUS_NAI);
		strncpy_n(hspotap->anonai.nai, ANONAI_ID1, BCM_DECODE_HSPOT_ANQP_MAX_NAI_SIZE + 1);
	} else {
		HS20_INFO(PRNT_STR, HSCMD_ANONYMOUS_NAI, argv[0]);
		strncpy_n(hspotap->anonai.nai, argv[0], BCM_DECODE_HSPOT_ANQP_MAX_NAI_SIZE + 1);
	}

	hspotap->anonai.naiLen = strlen(hspotap->anonai.nai);

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_ANONAI, varname), argv[0]);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, argv[0]);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_ip_add_type_avail_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, ipa_id = 1;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_IP_ADD_TYPE_AVAIL);
		return -1;
	}

	ipa_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_IP_ADD_TYPE_AVAIL, ipa_id);

	memset(&hspotap->ipaddrAvail, 0, sizeof(bcm_decode_anqp_ip_type_t));

	if (ipa_id == 1)
	{
		hspotap->ipaddrAvail.isDecodeValid = TRUE;
		hspotap->ipaddrAvail.ipv4 = IPA_IPV4_SINGLE_NAT;
		hspotap->ipaddrAvail.ipv6 = IPA_IPV6_NOT_AVAILABLE;
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->ipaddrAvail.ipv4);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IPV4ADDR, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->ipaddrAvail.ipv6);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_IPV6ADDR, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_hs_reset_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	bcm_encode_t ie;
	int err = 0, p2p_cross_enabled, isDgafDisabled;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	int ret;

	HS20_INFO(HSCMD_HS_RESET);

	reset_iw_ie(hspotap, TRUE, 0x001F);
	reset_u11_venue_list(hspotap, TRUE, 0x0007);
	update_iw_ie(hspotap, TRUE);

	reset_u11_oui_list(hspotap, TRUE);
	update_rc_ie(hspotap);

	set_hspot_flag(hspotap->prefix, HSFLG_ANQP, TRUE);
	set_hspot_flag(hspotap->prefix, HSFLG_MIH, FALSE);
	update_ap_ie(hspotap);

	reset_qosmap_ie(hspotap, TRUE);
	update_qosmap_ie(hspotap, FALSE);

	hspotap->bssload_id = 1;
	update_bssload_ie(hspotap, FALSE, TRUE);

	reset_osen_ie(hspotap, TRUE);
	update_osen_ie(hspotap, TRUE);

	reset_hspot_osup_list(hspotap, TRUE, 0x001F);

	reset_hs_cap(hspotap, TRUE);

	isDgafDisabled = get_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS);

	if (isDgafDisabled) {
		set_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS, FALSE);
		/* delete Passpoint vendor IE */
		if (hspotap->hs_ie_enabled)
			wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);
		/* encode Passpoint vendor IE */
		bcm_encode_init(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
		bcm_encode_ie_hotspot_indication2(&ie,
			isDgafDisabled, hspotap->hs_capable, FALSE, 0, FALSE, 0);
		hspotap->hs_ie_enabled = FALSE;
		update_dgaf_disable(hspotap);
	}

	if (hspotap->hs_ie_enabled == FALSE) {
		hspotap->hs_ie_enabled = TRUE;
		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0) {
			HS20_ERROR(HSERR_ADD_VENDR_IE_FAILED);
		}
	}

	reset_u11_3gpp_list(hspotap, TRUE);

	set_hspot_flag(hspotap->prefix, HSFLG_PROXY_ARP, FALSE);
	update_proxy_arp(hspotap);

	bcm_gas_set_if_cb_delay_unpause(1000, hspotap->ifr);
	bcm_gas_set_comeback_delay_response_pause(1);
	hspotap->isGasPauseForServerResponse = TRUE;
	bcm_gas_set_if_gas_pause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
	hspotap->gas_cb_delay = 0;

	reset_u11_domain_list(hspotap, TRUE);

	wl_wnm_url(hspotap->ifr, 0, 0);
	if (hspotap->url_len)
		free(hspotap->url);
	hspotap->url_len = 0;

	p2p_cross_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS);

	if (p2p_cross_enabled) {
		/* delete P2P vendor IE */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = 0x01;

		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0) {
			HS20_ERROR(HSERR_ADD_VENDR_IE_FAILED);
		}
		set_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS, FALSE);
	}

	set_hspot_flag(hspotap->prefix, HSFLG_ICMPV4_ECHO, TRUE);
	set_hspot_flag(hspotap->prefix, HSFLG_L2_TRF, TRUE);
	update_l2_traffic_inspect(hspotap);

	set_hspot_flag(hspotap->prefix, HSFLG_HS_EN, hspotap->hs_ie_enabled);

	reset_hspot_oper_class(hspotap, TRUE);
	reset_hspot_anonai(hspotap, TRUE);

	/* ---- Passpoint Flags  ---------------------------------------- */
	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->gas_cb_delay);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_GASCBDEL, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	set_hspot_flag(hspotap->prefix, HSFLG_4FRAMEGAS, !(hspotap->isGasPauseForServerResponse));
	set_hspot_flag(hspotap->prefix, HSFLG_USE_SIM, FALSE);

	reset_hspot_op_list(hspotap, TRUE);
	reset_hspot_wan_metrics(hspotap, TRUE);
	reset_hspot_homeq_list(hspotap, TRUE);
	reset_hspot_conncap_list(hspotap, TRUE);

	/* ---- 802.11u ---------------------------------------- */
	reset_u11_ipaddr_typeavail(hspotap, TRUE);
	reset_u11_netauth_list(hspotap, TRUE);
	reset_u11_realm_list(hspotap, TRUE);

	nvram_commit();

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_nai_realm_list_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret = 0, useSim = 0, realm_id = 1, iR = 0;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 auth_MSCHAPV2[1]		= { (uint8)REALM_MSCHAPV2 };
	uint8 auth_UNAMPSWD[1]		= { (uint8)REALM_USERNAME_PASSWORD };
	uint8 auth_CERTIFICATE[1]	= { (uint8)REALM_CERTIFICATE };
	uint8 auth_SIM[1]		= { (uint8)REALM_SIM };

	memset(&hspotap->realmlist, 0, sizeof(bcm_decode_anqp_nai_realm_list_t));

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_NAI_REALM_LIST);
		return -1;
	}

	realm_id = atoi(argv[0]);
	HS20_ERROR(PRNT_FLG, HSCMD_NAI_REALM_LIST, realm_id);

	switch (realm_id)
	{

	case 1:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 4;

	useSim = get_hspot_flag(hspotap->prefix, HSFLG_USE_SIM);
	iR = 0;

	if (!useSim) {
	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(MAIL);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 1.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	iR++;
	/* -------------------------------------------------- */
	}

	/* Realm 2 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(CISCO);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, CISCO,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 2.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 2.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 2.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	iR++;
	/* -------------------------------------------------- */

	/* Realm 3 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 2;
	/* EAP 3.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 2;
	/* Auth 3.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 3.1.2 */
	hspotap->realmlist.realm[iR].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* EAP 3.2 */
	hspotap->realmlist.realm[iR].eap[1].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[iR].eap[1].authCount = 1;
	/* Auth 3.2.1 */
	hspotap->realmlist.realm[iR].eap[1].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[1].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[iR].eap[1].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	iR++;
	/* -------------------------------------------------- */

	/* Realm 4 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(EXAMPLE4);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, EXAMPLE4,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 4.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[iR].eap[0].authCount = 1;
	/* Auth 4.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	iR++;
	/* -------------------------------------------------- */

	if (useSim) {
	/* Realm 4 --------------------------------------------- */
	hspotap->realmlist.realm[iR].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[iR].realmLen = strlen(MAIL);
	strncpy_n((char*)hspotap->realmlist.realm[iR].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[iR].eapCount = 1;
	/* EAP 4.1 */
	hspotap->realmlist.realm[iR].eap[0].eapMethod = (uint8)REALM_EAP_SIM;
	hspotap->realmlist.realm[iR].eap[0].authCount = 1;
	/* Auth 4.1.1 */
	hspotap->realmlist.realm[iR].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[iR].eap[0].auth[0].len = sizeof(auth_SIM);
	memcpy(hspotap->realmlist.realm[iR].eap[0].auth[0].value,
		auth_SIM, sizeof(auth_SIM));
	}
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname),
	useSim ? REALMLIST_ID1_SIM : REALMLIST_ID1);

	break;

	case 2:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 1;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 1;
	/* EAP 1.1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[0].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID2);
	break;

	case 3:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 3;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(CISCO);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, CISCO,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 1;
	/* EAP 1.1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[0].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* Realm 2 --------------------------------------------- */
	hspotap->realmlist.realm[1].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[1].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[1].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[1].eapCount = 2;
	/* EAP 2.1 */
	hspotap->realmlist.realm[1].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[1].eap[0].authCount = 2;
	/* Auth 2.1.1 */
	hspotap->realmlist.realm[1].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[1].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 2.1.2 */
	hspotap->realmlist.realm[1].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[1].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* EAP 2.2 */
	hspotap->realmlist.realm[1].eap[1].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[1].eap[1].authCount = 1;
	/* Auth 2.2.1 */
	hspotap->realmlist.realm[1].eap[1].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[1].eap[1].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[1].eap[1].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	/* -------------------------------------------------- */

	/* Realm 3 --------------------------------------------- */
	hspotap->realmlist.realm[2].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[2].realmLen = strlen(EXAMPLE4);
	strncpy_n((char*)hspotap->realmlist.realm[2].realm, EXAMPLE4,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[2].eapCount = 1;
	/* EAP 3.1 */
	hspotap->realmlist.realm[2].eap[0].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[2].eap[0].authCount = 1;
	/* Auth 3.1.1 */
	hspotap->realmlist.realm[2].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[2].eap[0].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[2].eap[0].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID3);
	break;

	case 4:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 1;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(MAIL);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 2;
	/* EAP 1.1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[0].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* EAP 1.2 */
	hspotap->realmlist.realm[0].eap[1].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[0].eap[1].authCount = 1;
	/* Auth 1.2.1 */
	hspotap->realmlist.realm[0].eap[1].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[1].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[0].eap[1].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID4);
	break;

	case 5:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 2;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 1;
	/* EAP 1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 2;
	/* Auth 1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[0].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* Realm 2 --------------------------------------------- */
	hspotap->realmlist.realm[1].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[1].realmLen = strlen(RUCKUS);
	strncpy_n((char*)hspotap->realmlist.realm[1].realm, RUCKUS,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[1].eapCount = 1;
	/* EAP 2.1 */
	hspotap->realmlist.realm[1].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[1].eap[0].authCount = 2;
	/* Auth 2.1.1 */
	hspotap->realmlist.realm[1].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[1].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 2.1.2 */
	hspotap->realmlist.realm[1].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[1].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID5);
	break;

	case 6:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 2;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 1;
	/* EAP 1.1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 2;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.1.2 */
	hspotap->realmlist.realm[0].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* Realm 2 --------------------------------------------- */
	hspotap->realmlist.realm[1].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[1].realmLen = strlen(MAIL);
		strncpy_n((char*)hspotap->realmlist.realm[1].realm, MAIL,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[1].eapCount = 1;
	/* EAP 2.1 */
	hspotap->realmlist.realm[1].eap[0].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[1].eap[0].authCount = 2;
	/* Auth 2.1.1 */
	hspotap->realmlist.realm[1].eap[0].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[1].eap[0].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 2.1.2 */
	hspotap->realmlist.realm[1].eap[0].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[1].eap[0].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[1].eap[0].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID6);
	break;

	case 7:
	hspotap->realmlist.isDecodeValid = TRUE;
	hspotap->realmlist.realmCount = 1;

	/* Realm 1 --------------------------------------------- */
	hspotap->realmlist.realm[0].encoding = (uint8)REALM_ENCODING_RFC4282;
	hspotap->realmlist.realm[0].realmLen = strlen(WIFI);
	strncpy_n((char*)hspotap->realmlist.realm[0].realm, WIFI,
		BCM_DECODE_ANQP_MAX_REALM_LENGTH + 1);
	hspotap->realmlist.realm[0].eapCount = 2;
	/* EAP 1.1 */
	hspotap->realmlist.realm[0].eap[0].eapMethod = (uint8)REALM_EAP_TLS;
	hspotap->realmlist.realm[0].eap[0].authCount = 1;
	/* Auth 1.1.1 */
	hspotap->realmlist.realm[0].eap[0].auth[0].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[0].auth[0].len = sizeof(auth_CERTIFICATE);
	memcpy(hspotap->realmlist.realm[0].eap[0].auth[0].value,
		auth_CERTIFICATE, sizeof(auth_CERTIFICATE));
	/* EAP 1.2 */
	hspotap->realmlist.realm[0].eap[1].eapMethod = (uint8)REALM_EAP_TTLS;
	hspotap->realmlist.realm[0].eap[1].authCount = 2;
	/* Auth 1.2.1 */
	hspotap->realmlist.realm[0].eap[1].auth[0].id = (uint8)REALM_NON_EAP_INNER_AUTHENTICATION;
	hspotap->realmlist.realm[0].eap[1].auth[0].len = sizeof(auth_MSCHAPV2);
	memcpy(hspotap->realmlist.realm[0].eap[1].auth[0].value,
		auth_MSCHAPV2, sizeof(auth_MSCHAPV2));
	/* Auth 1.2.2 */
	hspotap->realmlist.realm[0].eap[1].auth[1].id = (uint8)REALM_CREDENTIAL;
	hspotap->realmlist.realm[0].eap[1].auth[1].len = sizeof(auth_UNAMPSWD);
	memcpy(hspotap->realmlist.realm[0].eap[1].auth[1].value,
		auth_UNAMPSWD, sizeof(auth_UNAMPSWD));
	/* -------------------------------------------------- */

	/* set NVRAM value */
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_REALMLIST, varname), REALMLIST_ID7);
	break;
	}

	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, EMPTY_STR);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_oper_name_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, oper_id = 1;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OPER_NAME);
		return -1;
	}

	oper_id = atoi(argv[0]);
	HS20_ERROR(PRNT_FLG, HSCMD_OPER_NAME, oper_id);

	if (oper_id == 1)
		err = reset_hspot_op_list(hspotap, TRUE);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_venue_name_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, venue_id = 1;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_VENUE_NAME);
		return -1;
	}

	venue_id = atoi(argv[0]);
	HS20_ERROR(PRNT_FLG, HSCMD_VENUE_NAME, venue_id);

	if (venue_id == 1)
		err = reset_u11_venue_list(hspotap, TRUE, 0x0004);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_wan_metrics_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, wanm_id = 1;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_WAN_METRICS);
		return -1;
	}

	wanm_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_WAN_METRICS, wanm_id);

	memset(&hspotap->wanmetrics, 0, sizeof(bcm_decode_hspot_anqp_wan_metrics_t));

	hspotap->wanmetrics.isDecodeValid	= TRUE;
	hspotap->wanmetrics.linkStatus		= HSPOT_WAN_LINK_UP;
	hspotap->wanmetrics.symmetricLink	= HSPOT_WAN_NOT_SYMMETRIC_LINK;
	hspotap->wanmetrics.atCapacity		= HSPOT_WAN_NOT_AT_CAPACITY;
	hspotap->wanmetrics.lmd			= 10;

	if (wanm_id == 1) {
		hspotap->wanmetrics.dlinkSpeed	= 2500;
		hspotap->wanmetrics.ulinkSpeed	= 384;
		hspotap->wanmetrics.dlinkLoad	= 0;
		hspotap->wanmetrics.ulinkLoad	= 0;
	} else if (wanm_id == 2) {
		hspotap->wanmetrics.dlinkSpeed	= 1500;
		hspotap->wanmetrics.ulinkSpeed	= 384;
		hspotap->wanmetrics.dlinkLoad	= 20;
		hspotap->wanmetrics.ulinkLoad	= 20;
	} else if (wanm_id == 3) {
		hspotap->wanmetrics.dlinkSpeed	= 2000;
		hspotap->wanmetrics.ulinkSpeed	= 1000;
		hspotap->wanmetrics.dlinkLoad	= 20;
		hspotap->wanmetrics.ulinkLoad	= 20;
	} else if (wanm_id == 4) {
		hspotap->wanmetrics.dlinkSpeed	= 8000;
		hspotap->wanmetrics.ulinkSpeed	= 1000;
		hspotap->wanmetrics.dlinkLoad	= 20;
		hspotap->wanmetrics.ulinkLoad	= 20;
	} else if (wanm_id == 5) {
		hspotap->wanmetrics.dlinkSpeed	= 9000;
		hspotap->wanmetrics.ulinkSpeed	= 5000;
		hspotap->wanmetrics.dlinkLoad	= 20;
		hspotap->wanmetrics.ulinkLoad	= 20;
	}

	memset(varname, 0, sizeof(varname));
	snprintf(varvalue, sizeof(varvalue), WANMETRICS_FORMAT,
		(int)hspotap->wanmetrics.linkStatus, (int)hspotap->wanmetrics.symmetricLink,
		(int)hspotap->wanmetrics.atCapacity, (int)hspotap->wanmetrics.dlinkSpeed,
		(int)hspotap->wanmetrics.ulinkSpeed, (int)hspotap->wanmetrics.dlinkLoad,
		(int)hspotap->wanmetrics.ulinkLoad, (int)hspotap->wanmetrics.lmd);

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_WANMETRICS, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_conn_cap_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret, conn_id = 1;
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_CONN_CAP);
		return -1;
	}

	conn_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_CONN_CAP, conn_id);

	memset(&hspotap->concaplist, 0, sizeof(bcm_decode_hspot_anqp_connection_capability_t));
	memset(varname, 0, sizeof(varname));
	hspotap->concaplist.isDecodeValid = TRUE;

	if (conn_id == 1) {
		hspotap->concaplist.numConnectCap = 11;
		hspotap->concaplist.tuple[0].ipProtocol = (uint8)HSPOT_CC_IPPROTO_ICMP;
		hspotap->concaplist.tuple[0].portNumber = (uint16)HSPOT_CC_PORT_RESERVED;
		hspotap->concaplist.tuple[0].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[1].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[1].portNumber = (uint16)HSPOT_CC_PORT_FTP;
		hspotap->concaplist.tuple[1].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[2].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[2].portNumber = (uint16)HSPOT_CC_PORT_SSH;
		hspotap->concaplist.tuple[2].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[3].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[3].portNumber = (uint16)HSPOT_CC_PORT_HTTP;
		hspotap->concaplist.tuple[3].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[4].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[4].portNumber = (uint16)HSPOT_CC_PORT_HTTPS;
		hspotap->concaplist.tuple[4].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[5].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[5].portNumber = (uint16)HSPOT_CC_PORT_PPTP;
		hspotap->concaplist.tuple[5].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[6].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[6].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[6].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[7].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[7].portNumber = (uint16)HSPOT_CC_PORT_ISAKMP;
		hspotap->concaplist.tuple[7].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[8].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[8].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[8].status = (uint8)HSPOT_CC_STATUS_CLOSED;
		hspotap->concaplist.tuple[9].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[9].portNumber = (uint16)HSPOT_CC_PORT_IPSEC;
		hspotap->concaplist.tuple[9].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[10].ipProtocol = (uint8)HSPOT_CC_IPPROTO_ESP;
		hspotap->concaplist.tuple[10].portNumber = (uint16)HSPOT_CC_PORT_RESERVED;
		hspotap->concaplist.tuple[10].status = (uint8)HSPOT_CC_STATUS_OPEN;

		strncpy_n(varvalue, CONCAPLIST_ID1, NVRAM_MAX_VALUE_LEN);
	} else if (conn_id == 2) {
		hspotap->concaplist.numConnectCap = 4;
		hspotap->concaplist.tuple[0].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[0].portNumber = (uint16)HSPOT_CC_PORT_HTTP;
		hspotap->concaplist.tuple[0].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[1].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[1].portNumber = (uint16)HSPOT_CC_PORT_HTTPS;
		hspotap->concaplist.tuple[1].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[2].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[2].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[2].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[3].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[3].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[3].status = (uint8)HSPOT_CC_STATUS_OPEN;

		strncpy_n(varvalue, CONCAPLIST_ID2, NVRAM_MAX_VALUE_LEN);
	} else if (conn_id == 3) {
		hspotap->concaplist.numConnectCap = 2;
		hspotap->concaplist.tuple[0].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[0].portNumber = (uint16)HSPOT_CC_PORT_HTTP;
		hspotap->concaplist.tuple[0].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[1].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[1].portNumber = (uint16)HSPOT_CC_PORT_HTTPS;
		hspotap->concaplist.tuple[1].status = (uint8)HSPOT_CC_STATUS_OPEN;

		strncpy_n(varvalue, CONCAPLIST_ID3, NVRAM_MAX_VALUE_LEN);
	} else if (conn_id == 4) {
		hspotap->concaplist.numConnectCap = 4;
		hspotap->concaplist.tuple[0].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[0].portNumber = (uint16)HSPOT_CC_PORT_HTTP;
		hspotap->concaplist.tuple[0].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[1].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[1].portNumber = (uint16)HSPOT_CC_PORT_HTTPS;
		hspotap->concaplist.tuple[1].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[2].ipProtocol = (uint8)HSPOT_CC_IPPROTO_TCP;
		hspotap->concaplist.tuple[2].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[2].status = (uint8)HSPOT_CC_STATUS_OPEN;
		hspotap->concaplist.tuple[3].ipProtocol = (uint8)HSPOT_CC_IPPROTO_UDP;
		hspotap->concaplist.tuple[3].portNumber = (uint16)HSPOT_CC_PORT_SIP;
		hspotap->concaplist.tuple[3].status = (uint8)HSPOT_CC_STATUS_OPEN;

		strncpy_n(varvalue, CONCAPLIST_ID4, NVRAM_MAX_VALUE_LEN);
	}

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_CONCAPLIST, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_oper_class_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, ret, operating_class = 0;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	uint8 opClass1[1] = {OPCLS_2G};
	uint8 opClass2[1] = {OPCLS_5G};
	uint8 opClass3[2] = {OPCLS_2G, OPCLS_5G};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OPER_CLASS);
		return -1;
	}

	memset(&hspotap->opclass, 0, sizeof(bcm_decode_hspot_anqp_operating_class_indication_t));
	hspotap->opclass.isDecodeValid = TRUE;

	operating_class = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_OPER_CLASS, operating_class);

	if (operating_class == 3) {
		hspotap->opclass.opClassLen = sizeof(opClass3);
		memcpy(hspotap->opclass.opClass, opClass3, sizeof(opClass3));
	} else if (operating_class == 2) {
		hspotap->opclass.opClassLen = sizeof(opClass2);
		memcpy(hspotap->opclass.opClass, opClass2, sizeof(opClass2));
	} else if (operating_class == 1) {
		hspotap->opclass.opClassLen = sizeof(opClass1);
		memcpy(hspotap->opclass.opClass, opClass1, sizeof(opClass1));
	}

	snprintf(varvalue, sizeof(varvalue), "%d", operating_class);
	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_OPERCLS, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}
	nvram_commit();

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_net_auth_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, nat_id = 1;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_NET_AUTH_TYPE);
		return -1;
	}

	nat_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_NET_AUTH_TYPE, nat_id);

	memset(varvalue, 0, sizeof(varvalue));
	memset(&hspotap->netauthlist, 0, sizeof(bcm_decode_anqp_network_authentication_type_t));
	hspotap->netauthlist.isDecodeValid = TRUE;

	if (nat_id == 1)
	{
		hspotap->netauthlist.numAuthenticationType = 2;

		hspotap->netauthlist.unit[0].type = (uint8)NATI_ACCEPTANCE_OF_TERMS_CONDITIONS;
		hspotap->netauthlist.unit[0].urlLen = 0;
		strncpy_n((char*)hspotap->netauthlist.unit[0].url, EMPTY_STR,
			BCM_DECODE_ANQP_MAX_URL_LENGTH + 1);

		hspotap->netauthlist.unit[1].type = (uint8)NATI_HTTP_HTTPS_REDIRECTION;
		hspotap->netauthlist.unit[1].urlLen = strlen(NETAUTHLIST_ID1_URL);
		strncpy_n((char*)hspotap->netauthlist.unit[1].url, NETAUTHLIST_ID1_URL,
			BCM_DECODE_ANQP_MAX_URL_LENGTH + 1);

		strncpy_n(varvalue, NETAUTHLIST_ID1, NVRAM_MAX_VALUE_LEN);
	} else if (nat_id == 2)
	{
		hspotap->netauthlist.numAuthenticationType = 1;

		hspotap->netauthlist.unit[0].type = (uint8)NATI_ONLINE_ENROLLMENT_SUPPORTED;
		hspotap->netauthlist.unit[0].urlLen = 0;
		strncpy_n((char*)hspotap->netauthlist.unit[0].url, EMPTY_STR,
			BCM_DECODE_ANQP_MAX_URL_LENGTH + 1);

		strncpy_n(varvalue, NETAUTHLIST_ID2, NVRAM_MAX_VALUE_LEN);
	}

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_NETAUTHLIST, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_pause_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_PAUSE);
		return -1;
	}

	hspotap->isGasPauseForServerResponse = (atoi(argv[0]) != 0);
	bcm_gas_set_if_gas_pause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
	HS20_INFO(PRNT_FLG, HSCMD_PAUSE, hspotap->isGasPauseForServerResponse);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_dis_anqp_response_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, disable_ANQP_response;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_DIS_ANQP_RESPONSE);
		return -1;
	}

	disable_ANQP_response = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_DIS_ANQP_RESPONSE, disable_ANQP_response);
	set_hspot_flag(hspotap->prefix, HSFLG_DS_ANQP_RESP, disable_ANQP_response);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_sim_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, useSim;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_SIM);
		return -1;
	}

	useSim = (atoi(argv[0]) != 0);
	set_hspot_flag(hspotap->prefix, HSFLG_USE_SIM, useSim);
	HS20_INFO(PRNT_FLG, HSCMD_SIM, useSim);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_response_size_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_RESPONSE_SIZE);
		return -1;
	}

	hspotap->testResponseSize = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_RESPONSE_SIZE, hspotap->testResponseSize);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_pause_cb_delay_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int pause_cb_delay;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_PAUSE_CB_DELAY);
		return -1;
	}

	pause_cb_delay = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_PAUSE_CB_DELAY, pause_cb_delay);
	bcm_gas_set_comeback_delay_response_pause(pause_cb_delay);
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_sr_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	struct ether_addr addr, bssid;
	char *url;
	int urlLength;
	int serverMethod;
	bcm_encode_t enc;
	uint8 buffer[BUFF_256] = {0};

	if ((argv[0] == NULL) || (argv[1] == NULL) || (argv[2] == NULL)) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_SR);
		return err;
	}

	if (!strToEther(argv[0], &addr)) {
		HS20_ERROR(HSERR_ARGV_MAC_ERROR);
		return err;
	}

	url = argv[1];
	urlLength = strlen(url);
	if (urlLength > 255) {
		HS20_ERROR(HSERR_URL_LONG_ERROR);
		return err;
	}

	serverMethod = atoi(argv[2]);

	bcm_encode_init(&enc, sizeof(buffer), buffer);
	bcm_encode_wnm_subscription_remediation(&enc,
		hspotap->dialog_token++, urlLength, url, serverMethod);

	/* get bssid */
	wl_cur_etheraddr(hspotap->ifr, DEFAULT_BSSCFG_INDEX, &bssid);
	/* send action frame */
	wl_actframe(hspotap->ifr, DEFAULT_BSSCFG_INDEX,
		(uint32)bcm_encode_buf(&enc), 0, 250, &bssid, &addr,
		bcm_encode_length(&enc), bcm_encode_buf(&enc));

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_di_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	struct ether_addr addr, bssid;
	uint8 reason;
	int16 reauthDelay;
	char *url = 0;
	int urlLength = 0;
	bcm_encode_t enc;
	uint8 buffer[BUFF_256] = {0};

	if ((argv[0] == NULL) || (argv[1] == NULL) || (argv[2] == NULL))  {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_DI);
		return err;
	}
	if (!strToEther(argv[0], &addr)) {
		HS20_ERROR(HSERR_ARGV_MAC_ERROR);
		return err;
	}
	reason = atoi(argv[1]);
	reauthDelay = atoi(argv[2]);

	if (argv[3] != NULL) {
		url = argv[3];
		urlLength = strlen(url);
	}

	if (urlLength > 255) {
		HS20_ERROR(HSERR_URL_LONG_ERROR);
		return err;
	}

	/* Sending WNM Deauthentication Imminent Frame */
	bcm_encode_init(&enc, sizeof(buffer), buffer);
	bcm_encode_wnm_deauthentication_imminent(&enc,
		hspotap->dialog_token++, reason, reauthDelay, urlLength, url);

	/* get bssid */
	wl_cur_etheraddr(hspotap->ifr, DEFAULT_BSSCFG_INDEX, &bssid);

	/* send action frame */
	wl_actframe(hspotap->ifr, DEFAULT_BSSCFG_INDEX,
		(uint32)bcm_encode_buf(&enc), 0, 250, &bssid, &addr,
		bcm_encode_length(&enc), bcm_encode_buf(&enc));

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_btredi_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char *url;
	uint16 disassocTimer = 100;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_BTREDI);
		return -1;
	}

	url = argv[0];

	if (strlen(url) > 255) {
		HS20_ERROR(HSERR_URL_LONG_ERROR);
		return err;
	}

	if (wl_wnm_url(hspotap->ifr, strlen(url), (uchar *)url) < 0) {
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_WNM_URL);
		return err;
	}

	/* Sending BSS Transition Request Frame */
	if (wl_wnm_bsstrans_req(hspotap->ifr,
		/* DOT11_BSSTRANS_REQMODE_PREF_LIST_INCL | */
		DOT11_BSSTRANS_REQMODE_DISASSOC_IMMINENT |
		DOT11_BSSTRANS_REQMODE_ESS_DISASSOC_IMNT,
		disassocTimer, 0, TRUE) < 0)
	{
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_WNM_BSSTRANS_REQ);
		return err;
	}
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_qos_map_set_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int ret, err = 0, sta_mac = 0, qos_id = 1;
	char varvalue[NVRAM_MAX_VALUE_LEN] = {0};
	char varname[NVRAM_MAX_PARAM_LEN] = {0};
	struct ether_addr addr, bssid;
	bcm_encode_t enc;
	uint8 buffer[BUFF_256] = {0};

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_QOS_MAP_SET);
		return err;
	}

	qos_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_QOS_MAP_SET, qos_id);

	if (argv[1] != NULL) {
		if (!strToEther(argv[1], &addr)) {
			HS20_ERROR(HSERR_ARGV_MAC_ERROR);
			return -1;
		}
		sta_mac = 1;
	}

	memset(varvalue, 0, sizeof(varvalue));
	memset(&hspotap->qosMapSetIe, 0, sizeof(bcm_decode_qos_map_t));
	bcm_encode_init(&enc, sizeof(buffer), buffer);

	hspotap->qosMapSetIe.up[0].low = 8;
	hspotap->qosMapSetIe.up[0].high = 15;
	hspotap->qosMapSetIe.up[1].low = 0;
	hspotap->qosMapSetIe.up[1].high = 7;
	hspotap->qosMapSetIe.up[2].low = 255;
	hspotap->qosMapSetIe.up[2].high = 255;
	hspotap->qosMapSetIe.up[3].low = 16;
	hspotap->qosMapSetIe.up[3].high = 31;
	hspotap->qosMapSetIe.up[4].low = 32;
	hspotap->qosMapSetIe.up[4].high = 39;
	hspotap->qosMapSetIe.up[5].low = 255;
	hspotap->qosMapSetIe.up[5].high = 255;
	hspotap->qosMapSetIe.up[6].low = 40;
	hspotap->qosMapSetIe.up[6].high = 47;

	if (qos_id == 2) {
		hspotap->qosMapSetIe.exceptCount = 0;

		hspotap->qosMapSetIe.up[7].low = 48;
		hspotap->qosMapSetIe.up[7].high = 63;

		strncpy_n(varvalue, QOS_MAP_IE_ID2, NVRAM_MAX_VALUE_LEN);

		bcm_encode_qos_map(&enc, 0, NULL,
			8, 15, 0, 7, 255, 255, 16, 31, 32, 39, 255, 255, 40, 47, 48, 63);
	} else {
		hspotap->qosMapSetIe.exceptCount = 2;
		hspotap->qosMapSetIe.except[0].dscp = 0x35;
		hspotap->qosMapSetIe.except[0].up = 0x02;
		hspotap->qosMapSetIe.except[1].dscp = 0x16;
		hspotap->qosMapSetIe.except[1].up = 0x06;

		hspotap->qosMapSetIe.up[7].low = 255;
		hspotap->qosMapSetIe.up[7].high = 255;

		strncpy_n(varvalue, QOS_MAP_IE_ID1, NVRAM_MAX_VALUE_LEN);

		bcm_encode_qos_map(&enc, 4, (uint8 *)QOS_MAP_IE_ID1_EXCEPT,
			8, 15, 0, 7, 255, 255, 16, 31, 32, 39, 255, 255, 40, 47, 255, 255);
	}

	ret = nvram_set(strcat_r(hspotap->prefix, NVNM_QOSMAPIE, varname), varvalue);
	if (ret) {
		HS20_ERROR(HSERR_NVRAM_SET_FAILED, varname, varvalue);
		err = -1;
	}

	err = update_qosmap_ie(hspotap, TRUE);

	/* Sending QoS Map Configure frame */
	/* get bssid */
	wl_cur_etheraddr(hspotap->ifr, DEFAULT_BSSCFG_INDEX, &bssid);

	if (sta_mac) {
		/* send action frame */
		wl_actframe(hspotap->ifr, DEFAULT_BSSCFG_INDEX,
			(uint32)bcm_encode_buf(&enc), 0, 250, &bssid, &addr,
			bcm_encode_length(&enc), bcm_encode_buf(&enc));
	}

	nvram_commit();
	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_bss_load_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0, bssload_id = 0;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_BSS_LOAD);
		return -1;
	}

	hspotap->bssload_id = bssload_id = atoi(argv[0]);
	HS20_INFO(PRNT_FLG, HSCMD_BSS_LOAD, hspotap->bssload_id);

	if (bssload_id < 0)
		err = update_bssload_ie(hspotap, FALSE, FALSE);
	else if (bssload_id == 0)
		err = update_bssload_ie(hspotap, FALSE, TRUE);
	else if (bssload_id > 0)
		err = update_bssload_ie(hspotap, TRUE, TRUE);

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_osen_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool enabled = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_ARGV_ERROR, HSCMD_OSEN);
		return -1;
	}

	enabled = (atoi(argv[0]) != 0);
	HS20_INFO(PRNT_FLG, HSCMD_OSEN, enabled);

	if (hspotap->osen_ie_enabled != enabled) {

		hspotap->osen_ie_enabled = enabled;
		set_hspot_flag(hspotap->prefix, HSFLG_OSEN, hspotap->osen_ie_enabled);

		/* OSEN enabled and DGAF disabled are always done together */
		if (hspotap->osen_ie_enabled) {
			set_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS, hspotap->osen_ie_enabled);
			err = update_dgaf_disable(hspotap);
		}

		err = update_osen_ie(hspotap, TRUE);
	}

	return err;
}

/* ------------------------------------------------------------------------------ */
static int
hspot_cmd_help_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	HS20_ERROR(PRNT_HELP);
	return err;
}
/* ====================== COMMAND HANDLER FUNCTIONS ======================== */


/* ========================= WL DRIVER FUNCTIONS =========================== */
static int
update_proxy_arp(hspotApT *hspotap)
{
	int err = 0, proxy_arp;
	proxy_arp = get_hspot_flag(hspotap->prefix, HSFLG_PROXY_ARP);
	int mask = proxy_arp ? WL_WNM_PROXYARP : 0;

#ifdef __CONFIG_DHDAP__
	int is_dhd = !dhd_probe(hspotap->ifr);
	if (is_dhd) {
		int index = -1;
		get_ifname_unit(hspotap->ifr, NULL, &index);
		if (dhd_bssiovar_setint(hspotap->ifr, HSIOV_WL_PROXY_ARP,
			((index == -1) ? 0 : index), proxy_arp) < 0)
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DHD_PROXY_ARP);
	} else
#endif
	if ((wl_wnm(hspotap->ifr, WNM_DEFAULT_BITMASK | mask) < 0) ||
		(wl_wnm_parp_discard(hspotap->ifr, proxy_arp) < 0) ||
		(wl_wnm_parp_allnode(hspotap->ifr, proxy_arp) < 0)) {
		err = -1;
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_PROXY_ARP);
	}

	if (wl_grat_arp(hspotap->ifr, proxy_arp) < 0) {
		err = -1;
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_GRAT_ARP);
	}

	return err;
}

static int
update_dgaf_disable(hspotApT *hspotap)
{
	int err = 0, isDgafDisabled;
	isDgafDisabled = get_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS);

	if (isDgafDisabled) {
		if (wl_dhcp_unicast(hspotap->ifr, 1) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DHCP_UNICAST);
		}
		if (wl_block_multicast(hspotap->ifr, 1) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_MULTICAST);
		}
		if (wl_gtk_per_sta(hspotap->ifr, 1) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_GTK_PER_STA);
		}
	} else {
		if (wl_gtk_per_sta(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_GTK_PER_STA);
		}
		if (wl_block_multicast(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_MULTICAST);
		}
		if (wl_dhcp_unicast(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DHCP_UNICAST);
		}
	}

	return err;
}

static int
update_l2_traffic_inspect(hspotApT *hspotap)
{
	int err = 0, icmpv4_echo, l2_traffic_inspect;
	l2_traffic_inspect = get_hspot_flag(hspotap->prefix, HSFLG_L2_TRF);
	icmpv4_echo = get_hspot_flag(hspotap->prefix, HSFLG_ICMPV4_ECHO);

	if (l2_traffic_inspect) {
		if (wl_block_ping(hspotap->ifr, !icmpv4_echo) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_PING);
		}
		if (wl_block_sta(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_STA);
		}
		if (wl_ap_isolate(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_AP_ISOLATE);
		}
	} else {
		if (wl_block_ping(hspotap->ifr, 0) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_PING);
		}
		if (wl_block_sta(hspotap->ifr, 1) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_STA);
		}
		if (wl_ap_isolate(hspotap->ifr, 1) < 0) {
			err = -1;
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_AP_ISOLATE);
		}
	}
	return err;
}

static void
init_wlan_hspot(hspotApT *hspotap)
{
	if (hspotap->hs_ie_enabled) {
		if (wl_bssload(hspotap->ifr, 1) < 0)
			HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BSSLOAD);
	} else if (wl_bssload(hspotap->ifr, 0) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BSSLOAD);

	if (wl_dls(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DLS);

	if (wl_wnm(hspotap->ifr, WNM_DEFAULT_BITMASK) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_WNM);

	if (wl_interworking(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_INTERWORKING);

	if (wl_probresp_sw(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_PROBERESP_SW);

	if (wl_block_tdls(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BLOCK_TDLS);
	if (wl_dls_reject(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_DLS_REJECT);

	/* L2 Traffic Inspect & Filtering is supported per Radio */
	update_l2_traffic_inspect(hspotap);
}
/* ========================= WL DRIVER FUNCTIONS =========================== */


/* ======================= IE MANIPULATION FUNCTIONS ======================== */
static void
addIes_u11(hspotApT *hspotap)
{
	/* enable interworking */
	if (wl_interworking(hspotap->ifr, 1) < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_INTERWORKING);

	update_iw_ie(hspotap, TRUE);
}

static void
addIes_hspot(hspotApT *hspotap)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;

	int p2p_ie_enabled, p2p_cross_enabled, isDgafDisabled;
	p2p_ie_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P);
	p2p_cross_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P_CRS);
	isDgafDisabled = get_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS);

	bcm_encode_t ie;

	/* encode Passpoint vendor IE */
	bcm_encode_init(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
	bcm_encode_ie_hotspot_indication2(&ie, !isDgafDisabled, hspotap->hs_capable,
		FALSE, 0, FALSE, 0);
	vendorIeHSI->ieLength = bcm_encode_length(&ie);

	/* add to beacon and probe response */
	vendorIeHSI->pktflag = VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG;

	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	bcm_encode_init(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
	bcm_encode_ie_hotspot_indication2(&ie, isDgafDisabled, hspotap->hs_capable,
		FALSE, 0, FALSE, 0);

	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	/* don't need first 2 bytes (0xdd + len) */
	if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0)
		HS20_WARNING(HSERR_ADD_VENDR_IE_FAILED);


	if (p2p_ie_enabled) {

		/* encode P2P vendor IE */
		/* P2P Manageability attribute with P2P Device Management bit (B0) set to 1 and */
		/* the Cross Connection Permitted bit (B1) set to zero */
		vendorIeP2P->ieLength = 10;
		vendorIeP2P->ieData[0] = 0xDD;
		vendorIeP2P->ieData[1] = 0x08;
		vendorIeP2P->ieData[2] = 0x50;
		vendorIeP2P->ieData[3] = 0x6f;
		vendorIeP2P->ieData[4] = 0x9a;
		vendorIeP2P->ieData[5] = 0x09;
		vendorIeP2P->ieData[6] = 0x0a;
		vendorIeP2P->ieData[7] = 0x01;
		vendorIeP2P->ieData[8] = 0x00;
		vendorIeP2P->ieData[9] = 0x03;

		/* add to beacon and probe response */
		vendorIeP2P->pktflag = VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG;

		/* delete IEs first if not a clean shutdown */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = 0x01;
		/* delete IEs first if not a clean shutdown */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);
		if (p2p_cross_enabled)
			vendorIeP2P->ieData[9] = 0x03;
		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0)
			HS20_WARNING(HSERR_ADD_VENDR_IE_FAILED);
	}

	update_rc_ie(hspotap);
	update_ap_ie(hspotap);
	update_qosmap_ie(hspotap, FALSE);
	update_bssload_ie(hspotap, FALSE, TRUE);
	update_osen_ie(hspotap, TRUE);

	/* Handling Hotspot related IOVARs on primary BSS only once */
	init_wlan_hspot(hspotap);
}

static void
addIes(hspotApT *hspotap)
{
	if (hspotap->iw_ie_enabled)
		addIes_u11(hspotap);

	if (hspotap->hs_ie_enabled)
		addIes_hspot(hspotap);
}

static void
deleteIes_u11(hspotApT *hspotap)
{
	/* delete interworking IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID, 0, 0) < 0)
		HS20_WARNING(HSERR_DEL_VENDR_IE_FAILED);

	/* disable interworking - need to make it per BSS */
	/* if (wl_interworking(hspotap->ifr, 0) < 0) */
	/*	HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_INTERWORKING); */
}

static void
deleteIes_hspot(hspotApT *hspotap)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	int p2p_ie_enabled;
	p2p_ie_enabled = get_hspot_flag(hspotap->prefix, HSFLG_P2P);

	/* delete Passpoint vendor IE */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	/* delete P2P vendor IE */
	if (p2p_ie_enabled) {
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);
	}

	/* delete advertisement protocol IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID, 0, 0) < 0)
		HS20_WARNING(HSERR_DEL_IE_FAILED, IE_AP);

	/* delete roaming consortium IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID, 0, 0) < 0)
		HS20_WARNING(HSERR_DEL_IE_FAILED, IE_RC);

	/* delete QoS Map IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_QOS_MAP_ID, 0, 0) < 0)
		HS20_WARNING(HSERR_DEL_IE_FAILED, IE_QOSMAPSET);

	/* delete BSS Load IE */
	hspotap->bssload_id = 0;
	update_bssload_ie(hspotap, FALSE, FALSE);

	/* disable OSEN */
	if (wl_osen(hspotap->ifr, 0) < 0)
		HS20_WARNING(HSERR_DEL_IE_FAILED, IE_OSEN);

}

static void
deleteIes(hspotApT *hspotap)
{
	if (hspotap->hs_ie_enabled)
		deleteIes_hspot(hspotap);

	if (hspotap->iw_ie_enabled)
		deleteIes_u11(hspotap);
}

static int
update_iw_ie(hspotApT *hspotap, bool disable)
{
	int err = 0;

	if (hspotap->iw_ie_enabled) {
		bcm_encode_t ie;
		uint8 buffer[BUFF_256] = {0};

		/* encode interworking IE */
		bcm_encode_init(&ie, sizeof(buffer), buffer);
		bcm_encode_ie_interworking(&ie, hspotap->iwIe.accessNetworkType,
			hspotap->iwIe.isInternet, hspotap->iwIe.isAsra, FALSE, FALSE,
			hspotap->iwIe.isVenue,
			hspotap->iwIe.venueGroup, hspotap->iwIe.venueType,
			hspotap->iwIe.isHessid ? &hspotap->iwIe.hessid : 0);

		/* add interworking IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID,
		bcm_encode_length(&ie) - 2, bcm_encode_buf(&ie) + 2);
		if (err)
			HS20_ERROR(HSERR_ADD_IE_FAILED, IE_IW);
	} else if (disable) {
		/* delete interworking IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID, 0, 0);
		if (err)
			HS20_ERROR(HSERR_DEL_IE_FAILED, IE_IW);
	}
	return err;
}

static int
update_rc_ie(hspotApT *hspotap)
{
	int err = 0, iter = 0, iter_isbeacon = 0;
	bcm_decode_anqp_roaming_consortium_ex_t rc;

	memset(&rc, 0, sizeof(rc));

	if (hspotap->ouilist.numOi) {

		bcm_encode_t ie;
		uint8 buffer[BUFF_256] = {0};
		/* encode roaming consortium IE */
		bcm_encode_init(&ie, sizeof(buffer), buffer);

		/* Get OUIs from Roaming Consortium Table with is_beacon TRUE */
		for (iter = 0; iter < hspotap->ouilist.numOi; iter++) {
			if (hspotap->ouilist.oi[iter].isBeacon) {
				memcpy(rc.oi[iter_isbeacon].oi, hspotap->ouilist.oi[iter].oi,
					sizeof(hspotap->ouilist.oi[iter].oi));
				rc.oi[iter_isbeacon].oiLen = hspotap->ouilist.oi[iter].oiLen;
				iter_isbeacon++;
			}
		}

		/* Add Roaming Consortium IE, if any OUI found with is_beacon TRUE */
		if ((iter_isbeacon > 0) && (iter_isbeacon <= 3)) {
		bcm_encode_ie_roaming_consortium(&ie,
			hspotap->ouilist.numOi - iter_isbeacon,
			iter_isbeacon > 0 ? rc.oi[0].oiLen : 0, rc.oi[0].oi,
			iter_isbeacon > 1 ? rc.oi[1].oiLen : 0, rc.oi[1].oi,
			iter_isbeacon > 2 ? rc.oi[2].oiLen : 0, rc.oi[2].oi);

		/* Add roaming consortium IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID,
		bcm_encode_length(&ie) - 2, bcm_encode_buf(&ie) + 2);
		if (err)
			HS20_ERROR(HSERR_ADD_IE_FAILED, IE_RC);
		} else {
			/* Delete roaming cons IE, if no OUI found with is_beacon TRUE */
			err = wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID, 0, 0);
			if (err)
				HS20_ERROR(HSERR_DEL_IE_FAILED, IE_RC);
		}
	} else {
		/* delete roaming consortium IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID, 0, 0);
		if (err)
			HS20_ERROR(HSERR_DEL_IE_FAILED, IE_RC);
	}
	return err;
}

static int
update_ap_ie(hspotApT *hspotap)
{
	int ap_ANQPenabled = get_hspot_flag(hspotap->prefix, HSFLG_ANQP);
	int ap_MIHenabled  = get_hspot_flag(hspotap->prefix, HSFLG_MIH);

	int err = 0;
	if (ap_ANQPenabled || ap_MIHenabled) {
		bcm_encode_t ie;
		uint8 buffer[BUFF_256] = {0};
		uint8 adBuffer[BUFF_256] = {0};
		bcm_encode_t ad;

		/* encode advertisement protocol IE */
		bcm_encode_init(&ie, sizeof(buffer), buffer);
		bcm_encode_init(&ad, sizeof(adBuffer), adBuffer);
		bcm_encode_ie_advertisement_protocol_tuple(&ad, 0x7f, FALSE,
			ap_ANQPenabled ? ADVP_ANQP_PROTOCOL_ID : ADVP_MIH_PROTOCOL_ID);
		bcm_encode_ie_advertisement_protocol_from_tuple(&ie,
			bcm_encode_length(&ad), bcm_encode_buf(&ad));

		/* add advertisement protocol IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID,
		bcm_encode_length(&ie) - 2, bcm_encode_buf(&ie) + 2);
		if (err)
			HS20_ERROR(HSERR_ADD_IE_FAILED, IE_AP);
	} else {
		/* delete advertisement protocol IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID, 0, 0);
		if (err)
			HS20_ERROR(HSERR_DEL_IE_FAILED, IE_AP);
	}
	return err;
}

static int
update_qosmap_ie(hspotApT *hspotap, bool enable)
{
	int err = 0, iter = 0;
	uint8 except_data[BUFF_512], except_length = 0;
	memset(except_data, 0, sizeof(except_data));

	if (enable) {
		bcm_encode_t ie;
		uint8 buffer[BUFF_256] = {0};

		/* encode QoS Map IE */
		bcm_encode_init(&ie, sizeof(buffer), buffer);

		except_length = hspotap->qosMapSetIe.exceptCount * 2;

		for (iter = 0; iter < hspotap->qosMapSetIe.exceptCount; iter++) {
			memcpy(&except_data[(iter*2)],
				&hspotap->qosMapSetIe.except[iter].dscp, sizeof(uint8));
			memcpy(&except_data[(iter*2)+1],
				&hspotap->qosMapSetIe.except[iter].up, sizeof(uint8));
		}

		bcm_encode_qos_map(&ie, except_length, (uint8 *)except_data,
			hspotap->qosMapSetIe.up[0].low, hspotap->qosMapSetIe.up[0].high,
			hspotap->qosMapSetIe.up[1].low, hspotap->qosMapSetIe.up[1].high,
			hspotap->qosMapSetIe.up[2].low, hspotap->qosMapSetIe.up[2].high,
			hspotap->qosMapSetIe.up[3].low, hspotap->qosMapSetIe.up[3].high,
			hspotap->qosMapSetIe.up[4].low, hspotap->qosMapSetIe.up[4].high,
			hspotap->qosMapSetIe.up[5].low, hspotap->qosMapSetIe.up[5].high,
			hspotap->qosMapSetIe.up[6].low, hspotap->qosMapSetIe.up[6].high,
			hspotap->qosMapSetIe.up[7].low, hspotap->qosMapSetIe.up[7].high);

		/* add QoS Map IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_QOS_MAP_ID,
		bcm_encode_length(&ie) - 4, bcm_encode_buf(&ie) + 4);

		if (err)
			HS20_ERROR(HSERR_ADD_IE_FAILED, IE_QOSMAPSET);
	} else {
		/* delete QoS Map IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_QOS_MAP_ID, 0, 0);
		if (err)
			HS20_ERROR(HSERR_DEL_IE_FAILED, IE_QOSMAPSET);
	}
	return err;
}

static int
update_osen_ie(hspotApT *hspotap, bool disable)
{
	if (hspotap->osen_ie_enabled) {
		/* Enable OSEN */
		if (wl_osen(hspotap->ifr, hspotap->osen_ie_enabled) < 0) {
			HS20_WARNING(HSERR_ADD_IE_FAILED, IE_OSEN);
		}
	} else if (disable) {
		/* Disable OSEN */
		if (wl_osen(hspotap->ifr, 0) < 0) {
			HS20_WARNING(HSERR_DEL_IE_FAILED, IE_OSEN);
		}
	}
	return 0;
}

static int
update_bssload_ie(hspotApT *hspotap, bool isStatic, bool enable)
{
	int err = 0;

	wl_bssload_static_t bssload;
	memset(&bssload, 0, sizeof(bssload));

	if (enable && isStatic) {

		/* Static BSS Load IE Enabled */
		bssload.is_static = TRUE;
		bssload.sta_count = 1;
		bssload.aac = 65535;
		bssload.chan_util =
			(hspotap->bssload_id == 1 ? 50 : (hspotap->bssload_id == 2 ? 200 : 75));
	}

	/* Static /Dynamic BSS Load IE choosen */
	err = wl_bssload_static(hspotap->ifr, bssload.is_static,
		bssload.sta_count, bssload.chan_util, bssload.aac);
	if (err < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BSSLOADSTATIC);

	/* BSS Load IE Enabled /Disabled */
	err = wl_bssload(hspotap->ifr, enable);
	if (err < 0)
		HS20_ERROR(HSERR_IOVAR_FAILED, HSIOV_WL_BSSLOAD);

	return err;
}

/* ======================= IE MANIPULATION FUNCTIONS ======================== */


/* =========================== TCP_PORT SUPPORT ========================== */
static int
process_cli_command(hspotApT *hspotap, char **argv, char *txData)
{
	int err = 0;
	bool set_tx_data = TRUE;

	if (argv[0] == NULL) {
		HS20_ERROR(HSERR_NULL_CMD);
		err = -1;
	} else if (strcmp(argv[0], HSCMD_INTERFACE) == 0) {
		argv++;
		err = hspot_cmd_interface_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_BSS) == 0) {
		argv++;
		err = hspot_cmd_bss_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_INTERWORKING) == 0) {
		argv++;
		err = hspot_cmd_interworking_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ACCS_NET_TYPE) == 0) {
		argv++;
		err = hspot_cmd_accs_net_type_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_INTERNET) == 0) {
		argv++;
		err = hspot_cmd_internet_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ASRA) == 0) {
		argv++;
		err = hspot_cmd_asra_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_VENUE_GRP) == 0) {
		argv++;
		err = hspot_cmd_venue_grp_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_VENUE_TYPE) == 0) {
		argv++;
		err = hspot_cmd_venue_type_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_HESSID) == 0) {
		argv++;
		err = hspot_cmd_hessid_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ROAMING_CONS) == 0) {
		argv++;
		err = hspot_cmd_roaming_cons_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ANQP) == 0) {
		argv++;
		err = hspot_cmd_anqp_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_MIH) == 0) {
		argv++;
		err = hspot_cmd_mih_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_DGAF_DISABLE) == 0) {
		argv++;
		err = hspot_cmd_dgaf_disable_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_L2_TRAFFIC_INSPECT) == 0) {
		argv++;
		err = hspot_cmd_l2_traffic_inspect_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ICMPV4_ECHO) == 0) {
		argv++;
		err = hspot_cmd_icmpv4_echo_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_PLMN_MCC) == 0) {
		argv++;
		err = hspot_cmd_plmn_mcc_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_PLMN_MNC) == 0) {
		argv++;
		err = hspot_cmd_plmn_mnc_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_PROXY_ARP) == 0) {
		argv++;
		err = hspot_cmd_proxy_arp_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_BCST_UNCST) == 0) {
		argv++;
		err = hspot_cmd_bcst_uncst_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_GAS_CB_DELAY) == 0) {
		argv++;
		err = hspot_cmd_gas_cb_delay_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_4_FRAME_GAS) == 0) {
		argv++;
		err = hspot_cmd_4_frame_gas_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_DOMAIN_LIST) == 0) {
		argv++;
		err = hspot_cmd_domain_list_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_SESS_INFO_URL) == 0) {
		argv++;
		err = hspot_cmd_sess_info_url_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_DEST) == 0) {
		argv++;
		err = hspot_cmd_dest_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_HS2) == 0) {
		argv++;
		err = hspot_cmd_hs2_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_P2P_IE) == 0) {
		argv++;
		err = hspot_cmd_p2p_ie_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_P2P_CROSS_CONNECT) == 0) {
		argv++;
		err = hspot_cmd_p2p_cross_connect_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSU_PROVIDER_LIST) == 0) {
		argv++;
		err = hspot_cmd_osu_provider_list_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSU_ICON_TAG) == 0) {
		argv++;
		err = hspot_cmd_osu_icon_tag_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSU_SERVER_URI) == 0) {
		argv++;
		err = hspot_cmd_osu_server_uri_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSU_METHOD) == 0) {
		argv++;
		err = hspot_cmd_osu_method_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSU_SSID) == 0) {
		argv++;
		err = hspot_cmd_osu_ssid_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_ANONYMOUS_NAI) == 0) {
		argv++;
		err = hspot_cmd_anonymous_nai_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_IP_ADD_TYPE_AVAIL) == 0) {
		argv++;
		err = hspot_cmd_ip_add_type_avail_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_HS_RESET) == 0) {
		argv++;
		err = hspot_cmd_hs_reset_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_NAI_REALM_LIST) == 0) {
		argv++;
		err = hspot_cmd_nai_realm_list_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OPER_NAME) == 0) {
		argv++;
		err = hspot_cmd_oper_name_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_VENUE_NAME) == 0) {
		argv++;
		err = hspot_cmd_venue_name_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_WAN_METRICS) == 0) {
		argv++;
		err = hspot_cmd_wan_metrics_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_CONN_CAP) == 0) {
		argv++;
		err = hspot_cmd_conn_cap_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OPER_CLASS) == 0) {
		argv++;
		err = hspot_cmd_oper_class_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_NET_AUTH_TYPE) == 0) {
		argv++;
		err = hspot_cmd_net_auth_type_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_PAUSE) == 0) {
		argv++;
		err = hspot_cmd_pause_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_DIS_ANQP_RESPONSE) == 0) {
		argv++;
		err = hspot_cmd_dis_anqp_response_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_SIM) == 0) {
		argv++;
		err = hspot_cmd_sim_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_RESPONSE_SIZE) == 0) {
		argv++;
		err = hspot_cmd_response_size_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_PAUSE_CB_DELAY) == 0) {
		argv++;
		err = hspot_cmd_pause_cb_delay_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_SR) == 0) {
		argv++;
		err = hspot_cmd_sr_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_DI) == 0) {
		argv++;
		err = hspot_cmd_di_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_BTREDI) == 0) {
		argv++;
		err = hspot_cmd_btredi_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_QOS_MAP_SET) == 0) {
		argv++;
		err = hspot_cmd_qos_map_set_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_BSS_LOAD) == 0) {
		argv++;
		err = hspot_cmd_bss_load_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_OSEN) == 0) {
		argv++;
		err = hspot_cmd_osen_handler(hspotap, argv, txData, &set_tx_data);
	} else if (strcmp(argv[0], HSCMD_HELP) == 0) {
		argv++;
		err = hspot_cmd_help_handler(hspotap, argv, txData, &set_tx_data);
	} else {
		HS20_ERROR(HSERR_UNKNOWN_CMD, argv[0]);
		err = -1;
	}

	if (set_tx_data) {
		if (err) {
			strcpy(txData, HSCMD_RESP_ERROR);
		} else {
			strcpy(txData, HSCMD_RESP_OK);
		}
	}

	return err;
}

static void
tcp_receive_handler(void *handle, char *rxData, char *txData)
{
	(void)handle;

	/* receive and send back with OK
	   test with test.tcl test ap_set_hs2 to see what strings are being passed
	 */

	int argc = 0;
	char *argv[64], *token;
	int status;

	HS20_INFO(PRNT_RX_MSG, rxData);
	/* sprintf(txData, "OK %s", rxData); */

	/* convert input to argc/argv format */
	while ((argc < (int)(sizeof(argv) / sizeof(char *) - 1)) &&
		((token = strtok(argc ? NULL : rxData, PRNT_TAB_LF)) != NULL)) {
		argv[argc++] = token;
	}
	argv[argc] = NULL;

	status = process_cli_command(current_hspotap, argv, txData);
}
/* =========================== TCP_PORT SUPPORT ========================== */


/* ========================== GAS ENGINE SUPPORT ========================== */

static char*
gas_actframe_to_str(char *buf, int af, int length, uint8 fragmentId)
{
	switch (af)
	{
	case GAS_REQUEST_ACTION_FRAME:
		snprintf(buf, BUFF_PRINTGASEVENT,
			PRNT_GAS_EVNT_INIT_REQ, length);
		break;
	case GAS_RESPONSE_ACTION_FRAME:
		snprintf(buf, BUFF_PRINTGASEVENT,
			PRNT_GAS_EVNT_INIT_RESP, length);
		break;
	case GAS_COMEBACK_REQUEST_ACTION_FRAME:
		snprintf(buf, BUFF_PRINTGASEVENT,
			PRNT_GAS_EVNT_CMBK_REQ, length);
		break;
	case GAS_COMEBACK_RESPONSE_ACTION_FRAME:
		snprintf(buf, BUFF_PRINTGASEVENT,
			PRNT_GAS_EVNT_CMBK_RESP, length, fragmentId);
		break;
	default:
		strncpy_n(buf, PRNT_GAS_FRAME_UNKNOWN, BUFF_PRINTGASEVENT+1);
		break;
	}

	return buf;
}

static void
gas_print_event(bcm_gas_event_t *event)
{
	char buf[BUFF_PRINTGASEVENT + 1] = {0};

	if ((event->type == BCM_GAS_EVENT_TX &&
		event->tx.gasActionFrame == GAS_REQUEST_ACTION_FRAME) ||
		(event->type == BCM_GAS_EVENT_RX &&
		event->rx.gasActionFrame == GAS_REQUEST_ACTION_FRAME)) {
		HS20_INFO(PRNT_GAS_INFO_PEER_MAC,
		event->peer.octet[0], event->peer.octet[1], event->peer.octet[2],
		event->peer.octet[3], event->peer.octet[4], event->peer.octet[5]);
		HS20_INFO(PRNT_GAS_INFO_DLG_TOKEN, event->dialogToken);
	}

	if (event->type == BCM_GAS_EVENT_QUERY_REQUEST) {
		HS20_INFO(PRNT_GAS_EVNT_QUERY_REQ);
	} else if (event->type == BCM_GAS_EVENT_TX) {
		HS20_INFO(PRNT_GAS_EVNT_TX_FRMT,
		gas_actframe_to_str(buf, event->tx.gasActionFrame,
		event->tx.length, event->tx.fragmentId));
	} else if (event->type == BCM_GAS_EVENT_RX) {
		HS20_INFO(PRNT_GAS_EVNT_RX_FRMT, EMPTY_STR,
		gas_actframe_to_str(buf, event->rx.gasActionFrame,
		event->rx.length, event->rx.fragmentId));
	} else if (event->type == BCM_GAS_EVENT_STATUS) {
		char *str;

		switch (event->status.statusCode)
		{
		case DOT11_SC_SUCCESS:
			str = PRNT_GAS_STCD_SUCCESS;
			break;
		case DOT11_SC_FAILURE:
			str = PRNT_GAS_STCD_FAILURE;
			break;
		case DOT11_SC_ADV_PROTO_NOT_SUPPORTED:
			str = PRNT_GAS_STCD_ADVPRT_NSUPP;
			break;
		case DOT11_SC_NO_OUTSTAND_REQ:
			str = PRNT_GAS_STCD_NO_OTD_REQ;
			break;
		case DOT11_SC_RSP_NOT_RX_FROM_SERVER:
			str = PRNT_GAS_STCD_RSPNRXFM_SRV;
			break;
		case DOT11_SC_TIMEOUT:
			str = PRNT_GAS_STCD_TIMEOUT;
			break;
		case DOT11_SC_QUERY_RSP_TOO_LARGE:
			str = PRNT_GAS_STCD_QRRSP_2LARGE;
			break;
		case DOT11_SC_SERVER_UNREACHABLE:
			str = PRNT_GAS_STCD_SRV_UNRCHBL;
			break;
		case DOT11_SC_TRANSMIT_FAILURE:
			str = PRNT_GAS_STCD_TX_FAIL;
			break;
		default:
			str = PRNT_GAS_STCD_UNKNOWN;
			break;
		}
		HS20_INFO(PRNT_GAS_STCD, str);
	} else {
		HS20_INFO(PRNT_GAS_EVNT_UNKNOEN, event->type);
	}
}

static int
read_icon_to_buffer(char *filename, int *bufSize, uint8 **buf)
{
	int ret = FALSE;
	FILE *fp;
	long int size;
	uint8 *buffer;
	size_t result;

	fp = fopen(filename, "rb");
	if (fp == 0) {
		HS20_ERROR(HSERR_FOPEN_FAILED, errno, filename);
		return ret;
	}

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	rewind(fp);

	/* malloc'ed buffer returned must be freed by caller */
	buffer = malloc(size);
	if (buffer == 0) {
		HS20_ERROR(HSERR_MALLOC_FAILED);
		goto done;
	}

	result = fread(buffer, 1, size, fp);
	if ((int)result != size) {
		goto done;
	}
	HS20_INFO(PRNT_BYTES_RED_FM_FILE, (int)size, filename);
	*bufSize = size;
	*buf = buffer;
	ret = TRUE;

	done:
	fclose(fp);
	return ret;
}

static void
process_anqp_query_request(hspotApT *hspotap,
	bcm_gas_t *gas, int len, uint8 *data)
{
	/* Timestamp Begin */
	unsigned long long now = getTimestamp();
	HS20_INFO(PRNT_TIMESTAMP_BGN, now);

	int bufferSize = QUERY_REQUEST_BUFFER;
	uint8 *buffer;
	bcm_decode_t pkt;
	bcm_decode_anqp_t anqp;
	bcm_encode_t rsp;
	int responseSize, disable_ANQP_response;

	TRACE_HEX_DUMP(TRACE_DEBUG, PRNT_QUERY_REQ, len, data);

	if (hspotap->testResponseSize > QUERY_REQUEST_BUFFER) {
		bufferSize = hspotap->testResponseSize;
	}

	buffer = malloc(bufferSize);
	if (buffer == 0) {
		HS20_ERROR(HSERR_MALLOC_FAILED);
		return;
	}

	memset(buffer, 0, bufferSize);

	bcm_encode_init(&rsp, bufferSize, buffer);

	/* decode ANQP */
	bcm_decode_init(&pkt, len, data);
	bcm_decode_anqp(&pkt, &anqp);

	/* decode query list and encode response */
	if (anqp.anqpQueryListLength > 0) {
		bcm_decode_t ie;
		bcm_decode_anqp_query_list_t queryList;
		int i;

		bcm_decode_init(&ie, anqp.anqpQueryListLength, anqp.anqpQueryListBuffer);
		if (bcm_decode_anqp_query_list(&ie, &queryList))
			bcm_decode_anqp_query_list_print(&queryList);
		else {
			HS20_ERROR(HSERR_DECODE_U11_QRL_FAILED);
		}

		for (i = 0; i < queryList.queryLen; i++) {
			switch (queryList.queryId[i])
			{
			case ANQP_ID_QUERY_LIST:
				break;
			case ANQP_ID_CAPABILITY_LIST:
				encode_u11_cap_list(hspotap, &rsp);
				break;
			case ANQP_ID_VENUE_NAME_INFO:
				encode_u11_venue_list(hspotap, &rsp);
				break;
			case ANQP_ID_EMERGENCY_CALL_NUMBER_INFO:
				break;
			case ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO:
				encode_u11_netauth_list(hspotap, &rsp);
				break;
			case ANQP_ID_ROAMING_CONSORTIUM_LIST:
				encode_u11_oui_list(hspotap, &rsp);
				break;
			case ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO:
				encode_u11_ipaddr_typeavail(hspotap, &rsp);
				break;
			case ANQP_ID_NAI_REALM_LIST:
				encode_u11_realm_list(hspotap, &rsp);
				break;
			case ANQP_ID_G3PP_CELLULAR_NETWORK_INFO:
				encode_u11_3gpp_list(hspotap, &rsp);
				break;
			case ANQP_ID_AP_GEOSPATIAL_LOCATION:
				break;
			case ANQP_ID_AP_CIVIC_LOCATION:
				break;
			case ANQP_ID_AP_LOCATION_PUBLIC_ID_URI:
				break;
			case ANQP_ID_DOMAIN_NAME_LIST:
				encode_u11_domain_list(hspotap, &rsp);
				break;
			case ANQP_ID_EMERGENCY_ALERT_ID_URI:
				break;
			case ANQP_ID_EMERGENCY_NAI:
				break;
			case ANQP_ID_VENDOR_SPECIFIC_LIST:
				break;
			default:
				break;
			}
		}
	}

	if (anqp.hspot.queryListLength > 0) {
		bcm_decode_t ie;
		bcm_decode_hspot_anqp_query_list_t queryList;
		int i;

		bcm_decode_init(&ie, anqp.hspot.queryListLength, anqp.hspot.queryListBuffer);
		if (bcm_decode_hspot_anqp_query_list(&ie, &queryList))
			bcm_decode_hspot_anqp_query_list_print(&queryList);
		else {
			HS20_ERROR(HSERR_DECODE_HS_QRL_FAILED);
		}
		for (i = 0; i < queryList.queryLen; i++) {
			switch (queryList.queryId[i])
			{
			case HSPOT_SUBTYPE_QUERY_LIST:
				break;
			case HSPOT_SUBTYPE_CAPABILITY_LIST:
				encode_hspot_cap_list(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME:
				encode_hspot_op_list(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_WAN_METRICS:
				encode_hspot_wan_metrics(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_CONNECTION_CAPABILITY:
				encode_hspot_conncap_list(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY:
				encode_hspot_homeq_list(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION:
				encode_hspot_oper_class(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS:
				encode_hspot_osup_list(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_ANONYMOUS_NAI:
				encode_hspot_anonai(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_ICON_BINARY_FILE:
				break;
			default:
				break;
			}
		}
	}

	if (anqp.hspot.naiHomeRealmQueryLength > 0) {

		bcm_decode_t ie;
		bcm_decode_hspot_anqp_nai_home_realm_query_t realm;
		int i;
		int isMatch = FALSE;

		bcm_decode_init(&ie, anqp.hspot.naiHomeRealmQueryLength,
			anqp.hspot.naiHomeRealmQueryBuffer);
		if (bcm_decode_hspot_anqp_nai_home_realm_query(&ie, &realm))
			bcm_decode_hspot_anqp_nai_home_realm_query_print(&realm);
		else {
			HS20_ERROR(HSERR_DECODE_HRQ_FAILED);
		}

		for (i = 0; i < realm.count; i++) {
			if (strcmp(realm.data[i].name, HOME_REALM) == 0)
				isMatch = TRUE;
		}

		if (isMatch)
			encode_hspot_homeq_list_ex(hspotap, &rsp);
		else
			bcm_encode_anqp_nai_realm(&rsp, 0, 0, 0);
	}

	if (anqp.hspot.iconRequestLength > 0) {
		bcm_decode_t ie;
		bcm_decode_hspot_anqp_icon_request_t request;

		bcm_decode_init(&ie, anqp.hspot.iconRequestLength,
			anqp.hspot.iconRequestBuffer);
		if (bcm_decode_hspot_anqp_icon_request(&ie, &request)) {
			int size;
			uint8 *buf;

			bcm_decode_hspot_anqp_icon_request_print(&request);

			char fullpath[NVRAM_MAX_VALUE_LEN] = {0};
			char filename[NVRAM_MAX_PARAM_LEN] = {0};
			memset(filename, 0, sizeof(filename));
			memset(fullpath, 0, sizeof(fullpath));
			if (hspotap->osuplist.osuicon_id == 2) {
				strncpy_n(filename, ICON_FILENAME_ABGN, NVRAM_MAX_PARAM_LEN);
			} else {
				strncpy_n(filename, request.filename, NVRAM_MAX_PARAM_LEN);
			}
			HS20_INFO(PRNT_ICON_FULLPATH, filename);
			if (filename != NULL) {
				strncpy_n(fullpath, ICONPATH, NVRAM_MAX_VALUE_LEN);
				strncat(fullpath, filename, min(strlen(filename),
					NVRAM_MAX_VALUE_LEN-strlen(fullpath)));
				if (read_icon_to_buffer(fullpath, &size, &buf)) {
					bcm_encode_hspot_anqp_icon_binary_file(&rsp,
						HSPOT_ICON_STATUS_SUCCESS, 9,
						(uint8 *)ICON_TYPE_ID1, size, buf);
					free(buf);
				} else {
					bcm_encode_hspot_anqp_icon_binary_file(&rsp,
					HSPOT_ICON_STATUS_FILE_NOT_FOUND, 0, 0, 0, 0);
				}
			} else {
				  HS20_INFO(HSERR_ICON_FNAME_EMPTY);
			}
		}
	}

	responseSize = bcm_encode_length(&rsp);

	/* pad response to testResponseSize */
	if (hspotap->testResponseSize > responseSize) {
		responseSize = hspotap->testResponseSize;
	}

	disable_ANQP_response = get_hspot_flag(hspotap->prefix, HSFLG_DS_ANQP_RESP);

	HS20_INFO(PRNT_GAS_QRRSP_BYTES, EMPTY_STR, responseSize, disable_ANQP_response);

	if (!disable_ANQP_response)
		bcm_gas_set_query_response(gas, responseSize, bcm_encode_buf(&rsp));

	free(buffer);

	/* Timestamp End */
	unsigned long long then = getTimestamp();
	HS20_INFO(PRNT_TIMESTAMP_END, then, then-now);

}

static int
gas_event_handler(hspotApT *hspotap, bcm_gas_event_t *event, uint16 *status)
{
	gas_print_event(event);

	if (event->type == BCM_GAS_EVENT_QUERY_REQUEST) {
		process_anqp_query_request(hspotap, event->gas,
			event->queryReq.len, event->queryReq.data);
	} else if (event->type == BCM_GAS_EVENT_STATUS) {
#if TESTING_MODE
		/* toggle setting */
		hspotap->isGasPauseForServerResponse =
			hspotap->isGasPauseForServerResponse ? FALSE : TRUE;
		TRACE(TRACE_DEBUG, PRNT_GAS_PAUSE_4_SRVRESP,
			hspotap->isGasPauseForServerResponse ? TRUE_S : FALSE_S);
		bcm_gas_set_if_gas_pause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
#endif
		if (status != 0)
			*status = event->status.statusCode;
		return TRUE;
	}

	return FALSE;
}

static void
gas_event_callback(void *context, bcm_gas_t *gas, bcm_gas_event_t *event)
{
	(void)context;
	hspotApT *hspotap;
	hspotap = get_hspotap_by_wlif(bcm_gas_get_drv(gas));
	if (hspotap == NULL) {
		HS20_INFO(HSERR_CANT_FIND_HSPOTAP);
		return;
	}

	if (gas_event_handler(hspotap, event, 0)) {
		HS20_INFO(HSERR_EVENT_GAS_DONE);
	}
}
/* ========================== GAS ENGINE SUPPORT ========================== */


/* ==================================================================== */
/* ------------------------------------ MAIN FUNCTION ------------------------------------- */
/* ==================================================================== */
int
main(int argc, char **argv)
{
	int i, total_ifr = 0;
	void *ifr;
	char* hspot_dbg_level = nvram_get(NVNM_HS2_DEBUG_LEVEL);
	TRACE_LEVEL_SET(TRACE_ERROR);

	hspot_debug_level =  hspot_dbg_level ? atoi(hspot_dbg_level) : HSPOT_DEBUG_ERROR;
	HS20_INFO(PRNT_HSPOTAP_VER, EPI_VERSION_STR);

	if (wl() == NULL) {
		HS20_ERROR(HSERR_CANT_FIND_WLIF);
		exit(1);
	}

	/* look for enabled/disabled radio interfaces */
	int prim;
	int radio[MAX_IF_COUNT] = {0};
	for (prim = 0; prim < MAX_IF_COUNT; prim++) {
		radio[prim] = is_primary_radio_on(prim);
	}

	while ((ifr = wlif(total_ifr)) != NULL) {
		char *osifname = NULL;
		char varname[NVRAM_MAX_PARAM_LEN] = {0};
		int pri = 0, sec = 0;
		bool find = FALSE, conti_flag = FALSE;
		char prefix[MAX_IF_COUNT] = {0};
		hspotApT *hspotap = NULL;

		/* Get me the next interface, anyhow */
		total_ifr++;
		osifname = wl_ifname(ifr);
		find = FALSE;
		conti_flag = FALSE;

		/* look for interface name on the primary interfaces first */
		for (pri = 0; pri < MAX_IF_COUNT; pri++) {
			snprintf(varname, sizeof(varname),
				NVFMT_IFNAME_PRIM, pri);

			if (nvram_match(varname, osifname)) {
				find = TRUE;
				snprintf(prefix, sizeof(prefix), NVFMT_WLIF_PRIM, pri);

				if (!radio[pri])
					conti_flag = TRUE;

				break;
			}
		}
		if (conti_flag)
			continue;

		/* look for interface name on the multi-instance interfaces */
		if (!find) {
			for (pri = 0; pri < MAX_IF_COUNT; pri++) {
				for (sec = 0; sec < MAX_IF_COUNT; sec++) {
					snprintf(varname, sizeof(varname),
						NVFMT_IFNAME_SECO, pri, sec);
					if (nvram_match(varname, osifname)) {
						find = TRUE;
						snprintf(prefix, sizeof(prefix),
							NVFMT_WLIF_SECO, pri, sec);

					if (!radio[pri])
						conti_flag = TRUE;

					break;
					}
				}

			}
		}
		if (conti_flag || !find)
			continue;

		hspotap = malloc(sizeof(hspotApT));
		if (!hspotap) {
			HS20_ERROR(HSERR_MALLOC_FAILED);
			hspotap_free();
			exit(1);
		}
		memset(hspotap, 0, sizeof(hspotApT));
		hspotaps[hspotap_num] = hspotap;
		hspotap_num ++;

		hspotap->ifr = ifr;

		/* token start from 1 */
		hspotap->dialog_token = 1;
		hspotap->req_token = 1;
		hspotap->bssload_id = 1;
		hspotap->isGasPauseForServerResponse = TRUE;

		if (find) {

		/* Passpoint Flags ------------------------------------------------- */
		decode_hspot_flags(prefix, hspotap, varname);

		/* Interworking IE ------------------------------------------------- */
		if (!decode_iw_ie(prefix, &hspotap->iwIe, varname))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);

		/* QoS Map Set IE ------------------------------------------------- */
		if (!decode_qosmap_ie(prefix, &hspotap->qosMapSetIe))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_QOSMAPIE);

		/* Venue List ----------------------------------------------------- */
		if (!decode_u11_venue_list(prefix, &hspotap->venuelist, varname)) {
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);
		} else {
			hspotap->iwIe.isVenue = TRUE;
			hspotap->iwIe.venueType = hspotap->venuelist.type;
			hspotap->iwIe.venueGroup = hspotap->venuelist.group;
		}

		/* Network Authentication List ---------------------------------------- */
		if (!decode_u11_netauth_list(prefix, &hspotap->netauthlist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_NETAUTHLIST);

		/* Roaming Consortium List ------------------------------------------ */
		if (!decode_u11_oui_list(prefix, &hspotap->ouilist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_OUILIST);

		/* IP Address Type Availability ---------------------------------------- */
		if (!decode_u11_ipaddr_typeavail(prefix, &hspotap->ipaddrAvail, varname))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);

		/* Realm List ------------------------------------------------------ */
		if (!decode_u11_realm_list(prefix, &hspotap->realmlist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_REALMLIST);

		/* 3GPP Cellular Info List -------------------------------------------- */
		if (!decode_u11_3gpp_list(prefix, &hspotap->gpp3list))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_3GPPLIST);

		/* Domain Name List ------------------------------------------------ */
		if (!decode_u11_domain_list(prefix, &hspotap->domainlist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_DOMAINLIST);

		/* Operating Class -------------------------------------------------- */
		if (!decode_hspot_oper_class(prefix, &hspotap->opclass))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_OPERCLS);

		/* Operator Friendly Name List ---------------------------------------- */
		if (!decode_hspot_op_list(prefix, &hspotap->oplist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_OPLIST);

		/* OSU Provider List Info --------------------------------------------- */
		if (!decode_hspot_osup_list(prefix, &hspotap->osuplist, varname))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED, varname);

		/* Annonymous NAI ------------------------------------------------- */
		if (!decode_hspot_anonai(prefix, &hspotap->anonai))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_ANONAI);

		/* WAN Metrics ----------------------------------------------------- */
		if (!decode_hspot_wan_metrics(prefix, &hspotap->wanmetrics))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_WANMETRICS);

		/* Connection Capability List ------------------------------------------ */
		if (!decode_hspot_conncap_list(prefix, &hspotap->concaplist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_CONCAPLIST);

		/* NAI Home Realm Query List ---------------------------------------- */
		if (!decode_hspot_homeq_list(prefix, &hspotap->homeqlist))
			HS20_WARNING(HSERR_NVRAM_NOT_DEFINED_EX, prefix, NVNM_HOMEQLIST);

		/* Copy Prefix */
		strncpy_n(hspotap->prefix, prefix, MAX_IF_COUNT);

		} else {
			HS20_WARNING(HSERR_CANT_FIND_NV_IFNAME, osifname);
		}

		/* Handle Command Line Args to run app in various modes */
		if (hspotap_num == 1) {

		for (i = 1; i < argc; i++) {
			if (strcmp(argv[i], HSMODE_HELP) == 0) {
				HS20_INFO(PRNT_HELP_MODE, argv[0]);
				hspotap_free();
				exit(1);
			} else if (strcmp(argv[i], HSMODE_DEBUG) == 0) {
				TRACE_LEVEL_SET(TRACE_ERROR | TRACE_DEBUG | TRACE_PACKET);
			} else if (strcmp(argv[i], HSMODE_VERBOSE) == 0) {
				TRACE_LEVEL_SET(TRACE_ALL);
			} else if (strcmp(argv[i], HSMODE_DGAF) == 0) {
				set_hspot_flag(hspotap->prefix, HSFLG_DGAF_DS, TRUE);
			} else if (strcmp(argv[i], HSMODE_TCP_PORT) == 0) {
				if (i == (argc - 1)) {
					HS20_INFO(HSERR_ARGV_ERROR_IN_MODE, HSMODE_TCP_PORT);
					hspotap_free();
					exit(1);
				}
				tcpServerPort = atol(argv[i+1]);
				tcpServerEnabled = 1;
				i++;
			} else if (strcmp(argv[i], HSMODE_TEST) == 0) {
				if (i == (argc - 1)) {
					HS20_INFO(HSERR_ARGV_ERROR_IN_MODE, HSMODE_TEST);
					hspotap_free();
					exit(1);
				}
				i++;
			} else if (strcmp(argv[i], HSMODE_RESPSIZE) == 0) {
				if (i == (argc - 1)) {
					HS20_INFO(HSERR_ARGV_ERROR_IN_MODE, HSMODE_RESPSIZE);
					hspotap_free();
					exit(1);
				}
				hspotap->testResponseSize = atoi(argv[i+1]);
				i++;
			} else if (strcmp(argv[i], HSMODE_GAS4FRAMESON) == 0) {
				HS20_INFO(HSERR_GAS_ON);
				hspotap->testResponseSize = 20000;
				hspotap->isGasPauseForServerResponse = FALSE;
				hspotap->gas_cb_delay = 1000;
			} else {
				HS20_INFO(HSERR_MODE_INVALID, argv[i]);
				hspotap_free();
				exit(1);
			}
		}
		}

		if (wl_disable_event_msg(ifr, WLC_E_P2P_PROBREQ_MSG) < 0)
			HS20_ERROR(HSERR_EVENT_DIS_FALIED, WLC_E_P2P_PROBREQ_MSG);

		/* add IEs */
		addIes(hspotap);

		/* Handling Hotspot related IOVARs on both primary & secondary BSS */
		if (is_hs_enabled_on_primary_bss(hspotap)) {
			update_proxy_arp(hspotap);
			update_dgaf_disable(hspotap);
		}

		wl_wnm_url(ifr, 0, 0);

		if (hspotap_num >= MAX_WLIF_NUM)
			break;
	}

	current_hspotap = hspotaps[0];

	/* initialize GAS protocol */
	bcm_gas_subscribe_event(0, gas_event_callback);
	bcm_gas_init_dsp();
	bcm_gas_init_wlan_handler();

	if (tcpServerEnabled) {
		tcpSubscribeTcpHandler(0, tcp_receive_handler);
		tcpSrvCreate(tcpServerPort);
	}

	for (i = 0; i < hspotap_num; i++) {
		if (hspotaps[i]->gas_cb_delay) {
			bcm_gas_set_if_cb_delay_unpause(
				hspotaps[i]->gas_cb_delay, hspotaps[i]->ifr);
		}
		bcm_gas_set_if_gas_pause(
			hspotaps[i]->isGasPauseForServerResponse, hspotaps[i]->ifr);
	}
	dspStart(dsp());

	/* deinitialize GAS protocol */
	bcm_gas_deinitialize();
	bcm_gas_unsubscribe_event(gas_event_callback);

	/* terminate dispatcher */
	dspFree();

	if (tcpServerEnabled) {
		tcpSubscribeTcpHandler(0, NULL);
		tcpSrvDestroy();
		tcpServerEnabled = 0;
	}

	hspotap_free();
	return 0;
}
