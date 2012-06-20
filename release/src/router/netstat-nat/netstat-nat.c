/*
#-------------------------------------------------------------------------------
#                                                                                                                         
# $Id: netstat-nat.c,v 1.33 2010/01/09 19:34:24 danny Exp $     
#       
# $Log: netstat-nat.c,v $
# Revision 1.33  2010/01/09 19:34:24  danny
# fix for properly display DNAT over SNAT connection
#
# Revision 1.32  2007/11/24 13:18:48  danny
# - Added '-R' option to show routed connections instead of showing them with '-L'
# - Some allocation & free bugs were squashed
#
# Revision 1.31  2007/05/26 11:48:08  danny
# Added 'nf_conntrack' support
# Added NAT host connection information (Used IP and port for NAT)
#
# Revision 1.30  2006/08/17 17:43:25  danny
# - fix for read-in (ip_conntrack), previous versions could sometimes hang or
#   segfault on some systems.
# - fix for displaying dnat over snat connections.
# - changed my email to danny@tweegy.nl and changed homepage url
#
# Revision 1.29  2005/07/20 19:50:43  mardan
# gcc 2.96 compatability fix
# enlarged readin of ip_conntrack line
#
# Revision 1.28  2005/01/29 15:24:37  mardan
# Some cleanups, bumped to version 1.4.5
#
# Revision 1.27  2005/01/23 16:33:09  mardan
# Added protocol resolving
#
# Revision 1.26  2005/01/21 22:54:14  mardan
# Added some forgotten states
#
# Revision 1.25  2005/01/01 17:02:24  mardan
# Extraction of IPs and ports more dynamicly so it can be used with layer7 and
# maybe others when layout of ip_conntrack changes
# Added autoconf
#
# Revision 1.24  2003/09/01 20:36:52  mardan
# Fixed small bug which didn't allow to display hostnames in expanded mode,
# not enough bytes where allocated.
#
# Revision 1.23  2003/08/31 10:59:15  mardan
# Merged patch from Guomundur D. Haraldsson <gdh@binhex.EU.org> which does a
# more properly memory alloction and saver copies of variables.
# Changed versions to v1.4.3. Ready to release if found stable.
# Changed my e-mail to danny@tweegy.demon.nl
#
# Revision 1.22  2003/02/08 17:41:44  mardan
# made some last minor changes.
# ready to release v1.4.2
#
# Revision 1.21  2003/01/24 21:24:34  mardan
# Added unknown protocol, display as 'raw'
# Fixed hussle up in states when sorting connections
#
# Revision 1.20  2003/01/02 15:40:48  mardan
# Merged patch from Marceln, which removes unused variables, more understandable
# memory allocation error message, check to exit when there are no NAT connections
# and making netstat-nat compatible with uLibC.
# Updated files to v1.4.2
#
# Revision 1.19  2002/09/22 20:10:19  mardan
# Added '-v: print version'
# Added 'uninstall' to Makefile
# Updated all other files.
#
# Revision 1.18  2002/09/22 17:16:08  mardan
# Rewritten connection_table to allocate memory dynamicly.
#
# Revision 1.17  2002/09/12 19:32:12  mardan
# Added display local connections to NAT box self
# Updated README
# Small changes in Makefile
#
# Revision 1.16  2002/09/08 20:23:48  mardan
# Added sort by connection option. (source/destination IP/port)
# Updated README and man-page.
#
# Revision 1.15  2002/08/07 19:25:59  mardan
# Fixed bug, displayed wrong icmp connection in state REPLIED (dest was gateway).
#
# Revision 1.14  2002/08/07 19:02:54  mardan
# Fixed 'icmp' bug. Segmentation fault occured when displaying NATed icmp connections.
#
# Revision 1.13  2002/08/06 19:32:54  mardan
# Added small feature: no header output.
# Lots of code cleanup.
#
# Revision 1.12  2002/08/03 00:22:22  mardan
# Added portname resolving based on the listed names in 'services'.
# Re-arranged the layout.
# Added a Makefile and a header file.
# Updated the README.
#
# Revision 1.11  2002/07/12 20:05:54  mardan
# Added argument for extended view of hostnames.
# Moved display-code into one function.
# Removed most unnessacery code.
# Updated README
#
# Revision 1.10  2002/07/10 19:58:33  mardan
# Added filtering by destination-host, re-arranged some code to work properly.
# Tested DNAT icmp and udp.(pls report if any bugs occur)
# Fixed a few declaration bugs.
#
# Revision 1.9  2002/07/09 20:00:36  mardan
# Added fully DNAT support (udp & icmp not fully tested yet, but should work),
# including argument support for (S)(D)NAT selection.
# Re-arranged layout code, can possible merged into one function.
# Some few minor changes.
# Started to work on destination-host selection.
#
# Revision 1.8  2002/07/07 20:27:47  mardan
# Added display by source host/IP.
# Made a few fixes/changes.
# Updated the REAMDE.
#
# Revision 1.7  2002/06/30 19:55:41  mardan
# Added README and COPYING (license) FILES.
#
# Revision 1.6  2002/06/23 16:27:26  mardan
# Finished udp.
# Maybe some layout changes in future? therwise tool is finished.
#
# Revision 1.5  2002/06/23 14:07:46  mardan
# Added protocol arg option.
# Todo: udp protocol
#
# Revision 1.4  2002/06/23 12:57:35  mardan
# Added ident strings for test :-)
#
# Revision 1.3  2002/06/23 12:47:08  mardan
# Fixed resolved hostname hussle-up/layout
# Moved all source code into netstat-nat.c
#
# Revision 1.2  2002/06/23 11:56:09  mardan
# Added NAT icmp display.
# Still need to do udp (more states possible)
# Really need to fix resolved hostnames display, still hussled up.
#
# Revision 1.1.1.1  2002/05/04 01:08:06  mardan
# Initial import of netstat-nat, the C version.
# Array pointers really needs to be fixed, still lots of other bugs..
# So far only TCP displayed.
# No commandline args for e.g. no_nameresolving, protocol.
#
#
#                                                                                                                  
# Copyright (c) 2006 by D.Wijsman (danny@tweegy.nl). 
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#	       
#                                                                                                                         
#-------------------------------------------------------------------------------
*/


typedef struct _ip_addresses
{
    char ip[16];
    char dev[16];
    struct _ip_addresses *prev;
    struct _ip_addresses *next;
} sIpAddresses;
#include "netstat-nat.h"

static char const rcsid[] = "$Id: netstat-nat.c,v 1.33 2010/01/09 19:34:24 danny Exp $";
char SRC_IP[50];
char DST_IP[50];
int SNAT = 1;
int DNAT = 1;
int LOCAL = 0;
int ROUTED = 0;
static char PROTOCOL[4];
int connection_index = 0;
char ***connection_table;
struct _ip_addresses *IpAddresses = NULL;


int main(int argc, char *argv[])
    {
    const char *args = "hnp:s:d:SDxor:L?vNR";
    static int SORT_ROW = 1;
    static int EXT_VIEW = 0;
    static int RESOLVE = 1;
    static int no_hdr = 0;
    static int NAT_HOP = 0;
    FILE *f;
    char line[350];
    char src[50];
    char dst[50];
    char host[50];
    char buf[100];
    char buf2[100];
    char from[50] = "NATed Address";
    char nathost[50] = "NAT-host Address";
    char dest[50] = "Destination Address";
    char *ret;
    
    char ***pa;
    char *store;
    int index, a, b, c, j, r;

    /* variables to display routed and/or local connections */
    struct ifconf ifc;
    struct ifreq *req;
    struct sockaddr_in *ipaddr;
    char *ifbuf, *facename,  *ptr;
    int facefound, lastlen, len, sock;
    //IpAddresses = NULL;

    // check parameters
    while ((c = getopt(argc, argv, args)) != -1) {
	switch (c) {
	case 'h':
	    display_help();
	    return 1;
	case '?':
	    display_help();
	    return 1;
	case 'v':
	    printf("Version %s\n", VERSION);
	    return(0);
	case 'n':
	    RESOLVE = 0;
	    break;
	case 'p':
	    strcopy(PROTOCOL, sizeof(PROTOCOL), optarg);
	    break;
	case 's':
	    strcopy(SRC_IP, sizeof(SRC_IP), optarg);
	    lookup_ip(SRC_IP, sizeof(SRC_IP));
	    break;
	case 'd':
	    strcopy(DST_IP, sizeof(DST_IP), optarg);
	    lookup_ip(DST_IP, sizeof(DST_IP));
	    break;    
	case 'S':
	    DNAT = 0;
	    break;
	case 'D':
	    SNAT = 0;
	    break;
	case 'L':
	    SNAT = 0;
	    DNAT = 0;
	    LOCAL = 1;
	    ROUTED = 0;
	    break;
	case 'R':
	    SNAT = 0;
	    DNAT = 0;
	    LOCAL = 0;
	    ROUTED = 1;	    
	    break;
	case 'x':
	    EXT_VIEW = 1;
	    break;
	case 'o':
	    no_hdr = 1;
	    break;
	case 'N':
	    NAT_HOP = 1;
	    break;
	case 'r':
	    if (optarg == NULL || optarg == '\0') {
		display_help();
		return 1;
		}
	    if (strcmp(optarg, "scr") == 0) SORT_ROW = 1; //default
	    if (strcmp(optarg, "dst") == 0) SORT_ROW = 2;
	    if (strcmp(optarg, "src-port") == 0) SORT_ROW = 3; 
	    if (strcmp(optarg, "dst-port") == 0) SORT_ROW = 4; 
	    if (strcmp(optarg, "state") == 0) SORT_ROW = 5;
	    break; 
	}
    }
    // some param checks
    if (LOCAL == 1 || ROUTED == 1) {
	SNAT = 0;
	DNAT = 0;
	NAT_HOP = 0;
    }
    // get local IP addresses 
    if (ROUTED || LOCAL) {
	// find all interfaces

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	lastlen = 0;
	len = 100 * sizeof(struct ifreq);
	for(;;) {
	    if((ifbuf = malloc(len)) == NULL) {
    		perror("malloc");
    		exit(EXIT_FAILURE);
	    }
	    ifc.ifc_buf = ifbuf;
	    ifc.ifc_len = len;

	    if(ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
    		if(errno != EINVAL || lastlen != 0) {
    		    perror("ioctl:SIOCGIFCONF");
    		    exit(EXIT_FAILURE);
    		}
	    } else {
		if(ifc.ifc_len == lastlen)
		// success //
    		    break;
    		lastlen = ifc.ifc_len;
	    }
	    // increment buffer //
	    len += 10 * sizeof(struct ifreq);
	    free(ifbuf);
	}
	// store all local addresses in memory //
        for(ptr = ifbuf; ptr < ifbuf + ifc.ifc_len; ) {
	    req = (struct ifreq *)ptr;
	    ipaddr = (struct sockaddr_in *) &req->ifr_addr;
//	    printf("The IP address associated with %s is %s\n", req->ifr_name, inet_ntoa(ipaddr->sin_addr));
	    ip_addresses_add(&IpAddresses, req->ifr_name, inet_ntoa(ipaddr->sin_addr));
	    ptr += sizeof(struct ifreq);
	}
	if (ifbuf) {
	    free(ifbuf);
	}
    }

    connection_table = (char ***) xcalloc((1) * sizeof(char **));
    // some checking for IPTables and read file
    if ((f = fopen(NF_CONNTRACK_LOCATION, "r")) == NULL) {
	if ((f = fopen(IP_CONNTRACK_LOCATION, "r")) == NULL) {
	    printf("Could not read info about connections from the kernel, make sure netfilter is enabled in kernel or by modules.\n");
	    return 1;
	}
    }
    
    // process conntrack table
    if (!no_hdr) {
	if (LOCAL || ROUTED) {
	    strcopy(from, sizeof(from), "Source Address");
	    strcopy(dest, sizeof(dest), "Destination Address");
	    }
	if (!EXT_VIEW) {
	    printf("%-6s%-36s", "Proto", from);
	    if (NAT_HOP && !LOCAL) {
		printf("%-36s", nathost);
	    }
	    printf("%-36s%-6s" ,dest, "State");
	    printf("\n");
	} else {
	    printf("%-6s%-41s", "Proto", from);
	    if (NAT_HOP && !LOCAL) {
		printf("%-41s", nathost);
	    }
	    printf("%-41s%-6s" ,dest, "State");
	    printf("\n");
	    } 
	}

    // bugfix for proper read-in on some systems, provided by Supaflyster
    while (!feof(f)) 
    {
	ret = fgets(line, sizeof(line), f);
	if (strlen(line) > 0) {
    	    process_entry(line);
	}
	memset(line, 0, sizeof(line));
    }
    fclose(f);
        
    // create index of arrays pointed to main connection array
    if (connection_index == 0) {
	// There are no connections at this moment! free mem and exit
	free(connection_table);
        ip_addresses_free(&IpAddresses);
	return (0);
    }
    
    pa = (char ***) xcalloc((connection_index) * sizeof(char **));

    for (index = 0; index < connection_index; index++) {
	pa[index] = (char **) xcalloc((ROWS) * sizeof(char *));

	for (j = 0; j < ROWS; j++) {
	    pa[index][j] = &connection_table[index][j][0];
	    }
	}
    // sort by protocol and defined row
    for (a = 0; a < connection_index - 1; a++) {
	for (b = a + 1; b < connection_index; b++) {
	    r = strcmp(pa[a][0], pa[b][0]);
	    if (r > 0) {
		for (j = 0; j < ROWS; j++) {
		    store = pa[a][j];
		    pa[a][j] = pa[b][j];
		    pa[b][j] = store;
		    }
		}
	    if (r == 0) {
		if (strcmp(pa[a][SORT_ROW], pa[b][SORT_ROW]) > 0) {
		    for (j = 0; j < ROWS; j++) {
			store = pa[a][j];
			pa[a][j] = pa[b][j];
			pa[b][j] = store;
			}
		    }
		}
	    }
	}

    // print connections
    for (index = 0; index < connection_index; index++) {  
	if (RESOLVE) {
	    lookup_hostname(&pa[index][1]);
	    lookup_hostname(&pa[index][2]);
	    lookup_hostname(&pa[index][6]);
	    if (strlen(pa[index][3]) > 0 || strlen(pa[index][4]) > 0 || strlen(pa[index][7]) > 0) {
		lookup_portname(&pa[index][3], pa[index][0]);
		lookup_portname(&pa[index][4], pa[index][0]);
		lookup_portname(&pa[index][7], pa[index][0]);
	    	}
	    }
	if (!EXT_VIEW) {
	    strcopy(buf, sizeof(buf), ""); 
	    strncat(buf, pa[index][1], 34 - strlen(pa[index][3]));    
	    if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
                snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][3]);            
	    }
            else {
                snprintf(buf2, sizeof(buf2), "%s", buf);
            }
            snprintf(src, sizeof(src),  "%-36s", buf2);
	    strcopy(buf, sizeof(buf), ""); 
	    strncat(buf, pa[index][2], 34 - strlen(pa[index][4]));    
	    if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
                snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][4]);            
	    }
            else {
	        snprintf(buf2, sizeof(buf2), "%s", buf);
            }
	    snprintf(dst, sizeof(dst), "%-36s", buf2);
	    if (NAT_HOP) {
		strcopy(buf, sizeof(buf), ""); 
		strncat(buf, pa[index][6], 34 - strlen(pa[index][7]));    
		if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
            	    snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][7]);            
		}
        	else {
	    	    snprintf(buf2, sizeof(buf2), "%s", buf);
        	}
		snprintf(host, sizeof(dst), "%-36s", buf2);
	    }
	} else {
	    strcopy(buf, sizeof(buf), ""); 
	    strncat(buf, pa[index][1], 39 - strlen(pa[index][3]));    
	    if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
	        snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][3]);
	    }
            else {
	        snprintf(buf2, sizeof(buf2), "%s", buf);
	    }
            snprintf(src , sizeof(src), "%-41s", buf2);
	    strcopy(buf, sizeof(buf), ""); 
	    strncat(buf, pa[index][2], 39 - strlen(pa[index][4]));    
	    if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
	        snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][4]);
	    }
            else {
	        snprintf(buf2, sizeof(buf2), "%s", buf);
	    }
            snprintf(dst, sizeof(dst), "%-41s", buf2);
	    if (NAT_HOP) {
		strcopy(buf, sizeof(buf), ""); 
		strncat(buf, pa[index][6], 39 - strlen(pa[index][7]));    
		if (!strcmp(pa[index][0], "tcp") || !strcmp(pa[index][0], "udp")) {
            	    snprintf(buf2, sizeof(buf2), "%s:%s", buf, pa[index][7]);            
		}
        	else {
	    	    snprintf(buf2, sizeof(buf2), "%s", buf);
        	}
		snprintf(host, sizeof(dst), "%-41s", buf2);
	    }
	}
	printf("%-6s%s", pa[index][0], src);
	if (NAT_HOP) {
	    printf("%s", host);
	}
	printf("%s%-11s", dst, pa[index][5]);
	printf("\n");

    }

    ip_addresses_free(&IpAddresses);

    for (a = 0; a < connection_index; a++) {
	for (j = 0; j < ROWS; j++) {
	    if (connection_table[a][j] != NULL) free(connection_table[a][j]);
	}
	free(connection_table[a]);
	free(pa[a]);
    }
    free(connection_table);
    free(pa);
    return(0);
}

// get protocol
int get_protocol(char *line, char *protocol)
{
    int i,j, protocol_nr;
    char protocol_name[11] = "";
    char protocol_raw[6] = "";
    
    if (string_search(line, "tcp")) {
        memcpy(protocol, "tcp", 3);
    }
    else if (string_search(line, "udp")) {
        memcpy(protocol, "udp", 3);
    }
    else if (string_search(line, "icmp")) {
        memcpy(protocol, "icmp", 4);
    }
    else {
        // here we search for protocol number and give it a name (get_protocol_name)
        for (i = 0; i < strlen(line); i++ ) {
            if(!strncmp(&line[i], "unknown  ", 9)) {
                i += 9;
                for (j = i; j < strlen(line); j++) {
                    if (line[j] == ' ') {
                        break;
                    }
                    strncat(protocol_raw, &line[j], 1);
                }
                protocol_nr = atoi(protocol_raw);
                get_protocol_name(protocol_name, protocol_nr);
                memcpy(protocol, protocol_name, 5);
                break;
            }
        }
        //memcpy(protocol, "raw", 3);
    }
//    printf("PROTO: %s\n", protocol);
    return(0);
}

// get connection status
int get_connection_state(char *line, char *state)
{
    if (string_search(line, "ESTABLISHED")) {
        memcpy(state, "ESTABLISHED", 11);
    }
    else if (string_search(line, "TIME_WAIT")) {
        memcpy(state, "TIME_WAIT", 9);
    }    
    else if (string_search(line, "FIN_WAIT")) {
        memcpy(state, "FIN_WAIT", 8);
    }    
    else if (string_search(line, "SYN_RECV")) {
        memcpy(state, "SYN_RECV", 8);
    }    
    else if (string_search(line, "SYN_SENT")) {
        memcpy(state, "SYN_SENT", 8);
    }    
    else if (string_search(line, "UNREPLIED")) {
        memcpy(state, "UNREPLIED", 9);
    }    
    else if (string_search(line, "CLOSE")) {
        memcpy(state, "CLOSE", 5);
    }    
    else if (string_search(line, "ASSURED")) {
        memcpy(state, "ASSURED", 7);
    }
    else {
        if (string_search(line, "udp")) {
            memcpy(state, "UNREPLIED", 9);
        }
        else {
            memcpy(state, " ", 1);
        }
    }    
//    printf("STATE: %s\n", state);
    return(0);
}

void process_entry(char *line)
{
    int count = 0;
    char srcip_f[16] = "";
    char dstip_f[16] = "";
    char srcip_s[16] = "";
    char dstip_s[16] = "";
    char srcport[6] = "";
    char dstport[6] = "";
    char srcport_s[6] = "";
    char dstport_s[6] = "";
    char protocol[5] = "";
    char state[12] = "";

    search_first_hit("src=", line, srcip_f);    
    search_first_hit("dst=", line, dstip_f);    
    search_sec_hit("src=", line, srcip_s);    
    search_sec_hit("dst=", line, dstip_s);    
    search_first_hit("sport=", line, srcport);    
    search_first_hit("dport=", line, dstport);    
    search_sec_hit("sport=", line, srcport_s);
    search_sec_hit("dport=", line, dstport_s);

    get_protocol(line, protocol);
    if (strcmp(PROTOCOL, "")) {
        if (strncmp(PROTOCOL, protocol, 3)) {
//            printf("RETURN\n");
            return;
        }
    }
    get_connection_state(line, state);
    if (SNAT) {
	if ((!strcmp(srcip_f, dstip_s) == 0) && (strcmp(dstip_f, srcip_s) == 0)) {		
  	    check_src_dst(protocol, srcip_f, dstip_f, srcport, dstport, dstip_s, dstport_s, state);
	    }
    }
    if (DNAT) {
	if ((strcmp(srcip_f, dstip_s) == 0) && (!strcmp(dstip_f, srcip_s) == 0)) {		
	    check_src_dst(protocol, srcip_f, srcip_s, srcport, srcport_s, dstip_f, dstport_s, state);
	}
    }
    // bugfix for displaying dnat over snat connections, submitted by Supaflyster (intercepted traffic to DNAT) (2 interfaces)
    if (DNAT || SNAT) {
	if ((!strcmp(srcip_f, srcip_s) == 0) && (!strcmp(srcip_f, dstip_s) == 0) && (!strcmp(dstip_f, srcip_s) == 0) && (!strcmp(dstip_f, dstip_s) == 0) ) {
	    check_src_dst(protocol, srcip_f, srcip_s, srcport, srcport_s, dstip_s, dstport_s, state);
	}    
    }
    // (DNAT) (1 interface)
    if (DNAT) {
	if ((!strcmp(srcip_f, srcip_s) == 0) && (!strcmp(srcip_f, dstip_s) == 0) && (!strcmp(dstip_f, srcip_s) == 0) && (strcmp(dstip_f, dstip_s) == 0) ) {
	    check_src_dst(protocol, srcip_f, srcip_s, srcport, srcport_s, dstip_s, dstport_s, state);
	}    
    }
    if (LOCAL) {
        if ((strcmp(srcip_f, dstip_s) == 0) && (strcmp(dstip_f, srcip_s) == 0)
	    && ((ip_addresses_search(IpAddresses, srcip_f) == 1) || (ip_addresses_search(IpAddresses, srcip_s) == 1) 
	    || (ip_addresses_search(IpAddresses, dstip_f) == 1) || (ip_addresses_search(IpAddresses, dstip_s) == 1))) {		
            check_src_dst(protocol, srcip_f, srcip_s, srcport, dstport, "", "", state);
	}
    }
    if (ROUTED) {
        if ((strcmp(srcip_f, dstip_s) == 0) && (strcmp(dstip_f, srcip_s) == 0)
	    && (ip_addresses_search(IpAddresses, srcip_f) == 0) && (ip_addresses_search(IpAddresses, srcip_s) == 0) 
	    && (ip_addresses_search(IpAddresses, dstip_f) == 0) && (ip_addresses_search(IpAddresses, dstip_s) == 0)) {		
            check_src_dst(protocol, srcip_f, srcip_s, srcport, dstport, "", "", state);
	}
    }
//    printf("%s %s %s %s %s %s\n", protocol, srcip_f, dstip_f, srcip_s, dstip_s, state);
}


// -- Internal used functions
// Check filtering by source and destination IP
void check_src_dst(char *protocol, char *src_ip, char *dst_ip, char *src_port, char *dst_port, char *nathostip, char *nathostport, char *status) 
    {
    if ((check_if_source(src_ip)) && (strcmp(DST_IP, "") == 0)) {
	store_data(protocol, src_ip, dst_ip, src_port, dst_port, nathostip, nathostport, status);
	}
    else if ((check_if_destination(dst_ip)) && (strcmp(SRC_IP, "") == 0)) {
	store_data(protocol, src_ip, dst_ip, src_port, dst_port, nathostip, nathostport, status);
	}
    else if ((check_if_destination(dst_ip)) && (check_if_source(src_ip))) {
	store_data(protocol, src_ip, dst_ip, src_port, dst_port, nathostip, nathostport, status);
	}
    }

void store_data(char *protocol, char *src_ip, char *dst_ip, char *src_port, char *dst_port, char *nathostip, char *nathostport, char *status)  
    {
    
    connection_table = (char ***) xrealloc(connection_table, (connection_index +1) * sizeof(char **));
    connection_table[connection_index] = (char **) xcalloc(200 * sizeof(char *));
    connection_table[connection_index][0] = (char *) xcalloc(10);
    connection_table[connection_index][1] = (char *) xcalloc(60);
    connection_table[connection_index][2] = (char *) xcalloc(60); 
    connection_table[connection_index][3] = (char *) xcalloc(20);
    connection_table[connection_index][4] = (char *) xcalloc(20);
    connection_table[connection_index][5] = (char *) xcalloc(15);
    connection_table[connection_index][6] = (char *) xcalloc(60); 
    connection_table[connection_index][7] = (char *) xcalloc(20);
    
    strcopy(connection_table[connection_index][3], 20, src_port);
    strcopy(connection_table[connection_index][4], 20, dst_port);
    strcopy(connection_table[connection_index][1], 60, src_ip);
    strcopy(connection_table[connection_index][2], 60, dst_ip);
    strcopy(connection_table[connection_index][0], 10, protocol);
    strcopy(connection_table[connection_index][5], 15, status);
    strcopy(connection_table[connection_index][6], 60, nathostip);
    strcopy(connection_table[connection_index][7], 20, nathostport);
    connection_index++;
    }

void lookup_portname(char **port, char *proto)
    {
    char buf_port[10];
    int portnr;
    struct servent *service;
    size_t port_size;
    
    strcopy(buf_port, sizeof(buf_port), *port);
    portnr = htons(atoi(buf_port));
    
    if ((service = getservbyport(portnr, proto))) {
	//port_size = strlen(service->s_name) + 8;
        //*port = xrealloc(*port, port_size); hmm double alloction
	strcopy(*port, 20, service->s_name);
	}
    }

void extract_ip(char *gen_buffer) 
    {
    char *split;
    split = strtok(gen_buffer, "=");
    split = strtok(NULL, "=");
    strcpy(gen_buffer, split);
    }

int lookup_hostname(char **r_host) 
    {
    int addr;
    struct hostent *hp;
    char **p;
    size_t r_host_size;

    addr = inet_addr(*r_host);
    if ((hp = gethostbyaddr((char *) &addr, sizeof(addr), AF_INET)) == NULL)
	return 0;

    for (p = hp->h_addr_list; *p != 0; p++){
	struct in_addr in;
	(void)memcpy(&in.s_addr, *p, sizeof(in.s_addr));
	//r_host_size = strlen(*r_host) + 25;
	//*r_host = xrealloc(*r_host, r_host_size); hmm double allocation
	strcopy(*r_host, 60, hp->h_name);
	}
    return 0;
    }


int lookup_ip(char *hostname, size_t hostname_size)
    {
    char *ip;
    struct hostent *hp;
    struct in_addr ip_addr;
    
    if ((hp = gethostbyname(hostname)) == NULL) {
	printf("Unknown host: %s\n", hostname);
	exit(-1);
	}

    ip_addr = *(struct in_addr *)(hp->h_addr);
    ip = inet_ntoa(*(struct in_addr *)(hp->h_addr));
    strcopy(hostname, hostname_size, ip);
    return 1;
    }
/*
int match(char *string, char *pattern) 
    {
    int i;
    regex_t re;
    char buf[200];
    
    i = regcomp(&re, pattern, REG_EXTENDED|REG_NOSUB);

    if (i != 0) {
	(void)regerror(i, &re, buf, sizeof(buf));
	return 0;                       
	}
    
    i = regexec(&re, string, (size_t) 0, NULL, 0);
    regfree(&re);

    if (i != 0) {
	(void)regerror(i, &re, buf, sizeof(buf));
	return 0;                       
	}

    return 1;
    }
*/
int check_if_source(char *host) 
    {
    if ((strcmp(host, SRC_IP) == 0) || (strcmp(SRC_IP, "") == 0)) {
	return 1;
	}
    return 0;
    }

int check_if_destination(char *host) 
    {
    if ((strcmp(host, DST_IP) == 0) || (strcmp(DST_IP, "") == 0)) {
	return 1;
	}
    return 0;
    }


static void *xcalloc(size_t bufsize) 
    {
    void *buf;
	
    if ((buf = calloc(1, bufsize)) != NULL) {
	return buf;
    } else {
	printf("Could not allocate memory (%i bytes); %s.\n -- Exiting.\n", bufsize, strerror(errno));
	exit(1);
	}
    }


static void *xrealloc(void *oldbuf, size_t newbufsize) 
    {
    void *newbuf;
	
    if ((newbuf = realloc(oldbuf, newbufsize)) != NULL) {
	return newbuf;
    } else {
	printf("Could not allocate memory (%i bytes); %s.\n -- Exiting.\n", newbufsize, strerror(errno));
	exit(1);
	}
    }

char *xstrdup (const char *dup)
{
    char *ret;
    if ((ret = strdup(dup)) == NULL) {
	printf("Could not set value into struct (%s); %s.\n -- Exiting.\n", dup, strerror(errno));
	exit(EXIT_FAILURE);
    }	
    return ret;
}

void ip_addresses_add(struct _ip_addresses **list, const char *dev, const char *ip)
{
    struct _ip_addresses *new = malloc(sizeof * new);
    if (new != NULL) {
	strncpy(new->ip, ip, 15);
	strncpy(new->dev, dev, 15);
	new->next = NULL;
	if (*list == NULL) {
	    *list = new;
	}
	else {
	    struct _ip_addresses *tail = *list;
	    while (tail->next != NULL) 
	    {
		tail = tail->next;
	    }
	    tail->next = new;
	}
    }
}

int ip_addresses_search(struct _ip_addresses *list, const char *ip)
{
    struct _ip_addresses *akt = list;
    if (list == NULL) return 0;
    while (akt != NULL) 
    {
	if (strcmp (akt->ip, ip) == 0) {
	    return 1;
	}
	akt = akt->next;
    }
    return 0;
}

void ip_addresses_free(struct _ip_addresses **node)
{
    struct _ip_addresses *this = *node;
    struct _ip_addresses *temp;
    while (this != NULL) 
    {
	temp = this->next;
	free(this);
	this = temp;
    }
    *node = NULL;
}

int string_search(char *string, char *search)
{
    int searchLen;
    int i;
    searchLen = strlen(search);
    if (searchLen > strlen(string)) {
	return(0); // this can't match 
    }
    for (i = 0; i < strlen(string) - searchLen + 1; i++) {
	if (!strncasecmp((char *)&string[i], search, searchLen)) {
	    return(1); // we got hit
	}
    }
    return(0);
}


int search_first_hit(char *search, char *line, char *ret)
{
    unsigned int searchLen;
    unsigned int i;
    unsigned int j;
    unsigned int lineLen;
    
    lineLen = strlen(line);
    searchLen = strlen(search);

    if (searchLen > lineLen) {
	return(1); // this can't match, invalid data?
    }
    for (i = 0; i < lineLen - searchLen + 1; i++) {
	if (!strncasecmp((char *)&line[i], search, searchLen)) {
	    break; // we got hit
	}
    }
    for (j = i + searchLen; j < i + 15 + searchLen; j++) {
        if (j > lineLen) {
            return(1); // incomplete data
        }
        if (line[j] == ' ') {
            break; // we reach _space_ delimiter
        }
    } 
    memcpy(ret, &line[i + searchLen], j - i - searchLen);
    return(0);
}


int search_sec_hit(char *search, char *line, char *ret)
{
    unsigned int searchLen;
    unsigned int i;
    unsigned int j;
    unsigned int got_first = 0;
    unsigned int lineLen;
    
    lineLen = strlen(line);
    searchLen = strlen(search);

    if (searchLen > lineLen) {
	return(1); // this can't match, invalid data?
    }
    for (i = 0; i < lineLen - searchLen + 1; i++) {
	if (!strncasecmp((char *)&line[i], search, searchLen)) {
	    if (got_first) {
                break; // we got hit (second)
            }
            got_first = 1;
	}
    }
    for (j = i + searchLen; j < i + 15 + searchLen; j++) {
        if (j > lineLen) {
            return(1); // incomplete data
        }
        if (line[j] == ' ') {
            break; // we reach _space_ delimiter
        }
    } 
    memcpy(ret, &line[i + searchLen], j - i - searchLen);
    return(0);
}


void get_protocol_name(char *protocol_name, int protocol_nr)
{
    struct protoent *proto_struct;
    char strconvers[10] = "";
    proto_struct = getprotobynumber(protocol_nr);
    if (proto_struct != NULL) {
        memcpy(protocol_name, proto_struct->p_name, 5);
    }
    else {
        snprintf(strconvers, 6, "%d", protocol_nr);
        memcpy(protocol_name, strconvers, 5);
    }
}


void display_help()
{
    printf("args: -h: displays this help\n");
    printf("      -n: don't resolve host/portnames\n");
    printf("      -p <protocol>        : display connections by protocol\n");
    printf("      -s <source-host>     : display connections by source\n");
    printf("      -d <destination-host>: display connections by destination\n");
    printf("      -S: display SNAT connections\n");
    printf("      -D: display DNAT connections (default: SNAT & DNAT)\n");
    printf("      -L: display only connections to NAT box itself (doesn't show SNAT & DNAT)\n"); 
    printf("      -R: display only connections routed through the NAT box (doesn't show SNAT & DNAT)\n"); 
    printf("      -x: extended hostnames view\n");
    printf("      -r src | dst | src-port | dst-port | state : sort connections\n");
    printf("      -o: strip output header\n");
    printf("      -N: display NAT box connection information (only valid with SNAT & DNAT)\n");
    printf("      -v: print version\n");
    printf("\n");
    printf("      netstat-nat [-S|-D|-L|-R] [-no]\n");
    printf("      netstat-nat [-nxo]\n");
}

// -- End of internal used functions

// -- The End --
