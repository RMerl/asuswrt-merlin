/*
#-------------------------------------------------------------------------------
#                                                                                                                         
# $Id: netstat-nat.h,v 1.14 2007/11/24 13:18:48 danny Exp $     
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <regex.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <strings.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define ROWS			8
#define IP_CONNTRACK_LOCATION	"/proc/net/ip_conntrack"
#define NF_CONNTRACK_LOCATION	"/proc/net/nf_conntrack"


int get_protocol(char *line, char *protocol);
int get_connection_state(char *line, char *state);
void process_entry(char *line);
void check_src_dst(char *protocol, char *src_ip, char *dst_ip, char *src_port, char *dst_port, char *nathostip, char *nathostport, char *status);
void store_data(char *protocol, char *src_ip, char *dst_ip, char *src_port, char *dst_port, char *nathostip, char *nathostport, char *status);
void extract_ip(char *gen_buffer);
void display_help();
int lookup_hostname(char **r_host);
int lookup_ip(char *hostname, size_t hostname_size);
//int match(char *string, char *pattern);
int check_if_source(char *host);
int check_if_destination(char *host);
void lookup_portname(char **port, char *proto);
void oopsy(int size);
static void *xrealloc(void *oldbuf, size_t newbufsize);
static void *xcalloc(size_t bufsize);
void get_protocol_name(char *protocol_name, int protocol_nr);
char *xstrdup (const char *dup);
void ip_addresses_add(struct _ip_addresses **list, const char *dev, const char *ip);
int ip_addresses_search(struct _ip_addresses *list, const char *ip);
void ip_addresses_free(struct _ip_addresses **list);


#define strcopy(dst, dst_size, src) \
	strncpy(dst, src, (dst_size - 1)); 

/* The End */
