/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <syslog.h>

#include <net/if.h>
#include <shutils.h>
#include <notify_rc.h>
#include <bcmnvram.h>

#include "rc.h"
#ifdef RTCONFIG_USB
#include <disk_io_tools.h>
#endif

#define SCAN_INTERVAL 5
#define TCPCHECK_TIMEOUT 3
#define PING_RESULT_FILE "/tmp/ping_success"
#define RX_THRESHOLD 40


#define MAX_WAIT_TIME 60
#define MAX_DISCONN_COUNT MAX_WAIT_TIME/SCAN_INTERVAL

int max_wait_time = MAX_WAIT_TIME;
int max_disconn_count = MAX_DISCONN_COUNT;

#define PROC_NET_DEV "/proc/net/dev"
#define WANDUCK_PID_FILE "/var/run/wanduck.pid"
#define DETECT_FILE "/tmp/internet_check_result"

#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

#define MAXLINE				2048
#define PATHLEN				256
#define MAX_USER			100

#define DFL_HTTP_SERV_PORT	"18017"
#define DFL_DNS_SERV_PORT	"18018"
#define T_HTTP	'H'
#define T_DNS		'D'

#define DISCONN	0
#define CONNED	1
#define C2D			3
#define D2C			4
#define PHY_RECONN	5

#define CASE_NONE          0
#define CASE_DISWAN        1
#define CASE_PPPFAIL       2
#define CASE_DHCPFAIL      3
#define CASE_MISROUTE      4
#define CASE_OTHERS        5
#define CASE_THESAMESUBNET 6
#define CASE_PPPNOIP       7
#define CASE_AP_DISCONN    8

#pragma pack(1) // let struct be neat by byte.

typedef struct Flags_Pack {
	uint16_t reply_1:4,     // bit:0-3
	non_auth_ok:1,          // bit:4
	answer_auth:1,          // bit:5
	reserved:1,             // bit:6
	recur_avail:1,          // bit:7
	recur_desired:1,        // bit:8
	truncated:1,            // bit:9
	authori:1,              // bit:10
	opcode:4,               // bit:11-14
	response:1;             // bit:15
} flag_pack;

typedef struct DNS_HEADER {
	uint16_t tid;
	union {
		uint16_t flag_num;
		flag_pack flags;
	} flag_set;
	uint16_t questions;
	uint16_t answer_rrs;
	uint16_t auth_rrs;
	uint16_t additional_rss;
} dns_header;

typedef struct QUERIES {
	char name[PATHLEN];
	uint16_t type;
	uint16_t ip_class;
} dns_queries;

typedef struct ANSWER{
	uint16_t name;
	uint16_t type;
	uint16_t ip_class;
	uint32_t ttl;	// time to live
	uint16_t data_len;
	uint32_t addr;
} dns_answer;

typedef struct DNS_REQUEST{
	dns_header header;
	dns_queries queries;
} dns_query_packet;

typedef struct DNS_RESPONSE{
	dns_header header;
	char *q_name;
	uint16_t q_type;
	uint16_t q_class;
	dns_answer answers;
} dns_response_packet;

typedef struct REQCLIENT{
	int sfd;
	char type;
} clients;

#pragma pack() // End.

// var
#define DUT_DOMAIN_NAME "www.asusnetwork.net"
char router_name[PATHLEN];
int sw_mode, isFirstUse;
#ifdef RTCONFIG_DUALWAN
char dualwan_mode[8];
#endif

int http_sock, dns_sock, maxfd;
clients client[MAX_USER];
fd_set rset, allset;
int fd_i, cur_sockfd;
char dst_url[256];

#define S_IDLE -1
#define S_COUNT 0
int conn_changed_state[WAN_UNIT_MAX], changed_count[WAN_UNIT_MAX];

int conn_state_old[WAN_UNIT_MAX], conn_state[WAN_UNIT_MAX];
int cross_state = 0;
int disconn_case_old[WAN_UNIT_MAX], disconn_case[WAN_UNIT_MAX];
int ppp_fail_state;
int rule_setup;
int link_setup[WAN_UNIT_MAX], link_wan[WAN_UNIT_MAX];
int got_notify;

char prefix_lan[8];
int current_lan_unit = 0;
char current_lan_ifname[16];
char current_lan_proto[16];
char current_lan_ipaddr[16];
char current_lan_netmask[16];
char current_lan_gateway[16];
char current_lan_dns[256];
char current_lan_subnet[11];

int current_wan_unit = WAN_UNIT_FIRST;
int other_wan_unit = WAN_UNIT_SECOND;
int current_state[WAN_UNIT_MAX];

char nvram_state[WAN_UNIT_MAX][16], nvram_sbstate[WAN_UNIT_MAX][16], nvram_auxstate[WAN_UNIT_MAX][16];

#ifdef RTCONFIG_WIRELESSREPEATER
int setAP, wlc_state = 0;
#endif

// func
void handle_wan_line(int wan_unit, int action);
void record_wan_state_nvram(int wan_unit, int state, int sbstate, int auxstate);
