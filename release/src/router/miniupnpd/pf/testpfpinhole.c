/* $Id: testpfpinhole.c,v 1.10 2012/04/22 23:12:51 nanard Exp $ */
/* MiniUPnP project
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * (c) 2012 Thomas Bernard
 * This software is subject to the conditions detailed
 * in the LICENCE file provided within the distribution */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>

#include "../config.h"
#include "obsdrdr.h"
#include "pfpinhole.h"

int runtime_flags = 0;
const char * tag = NULL;

const char * anchor_name = "miniupnpd";
const char * queue = NULL;

static int print_pinhole(int uid)
{
	int r;
	char rem_host[64];
	unsigned short rem_port;
	char int_client[64];
	unsigned short int_port;
	int proto;
	unsigned int timestamp;
	u_int64_t packets, bytes;

	r = get_pinhole((unsigned short)uid,
	                rem_host, sizeof(rem_host), &rem_port,
	                int_client, sizeof(int_client), &int_port,
	                &proto, &timestamp,
	                &packets, &bytes);
	if(r < 0) {
		fprintf(stderr, "get_pinhole(%d) returned %d\n", uid, r);
	} else {
		printf("pinhole %d : [%s]:%hu => [%s]:%hu proto=%d ts=%u\n",
		       uid, rem_host, rem_port, int_client, int_port,
		       proto, timestamp);
		printf("    packets=%llu bytes=%llu\n", packets, bytes);
	}
	return r;
}

int main(int argc, char * *argv)
{
#ifndef ENABLE_IPV6
	fprintf(stderr,"nothing to test, ENABLE_IPV6 is not defined in config.h\n");
	return 1;
#else
	int uid;
	int ret;

	openlog("testpfpinhole", LOG_PERROR, LOG_USER);
	if(init_redirect() < 0) {
		fprintf(stderr, "init_redirect() failed\n");
		return 1;
	}

	uid = add_pinhole("ep0", "2001::1:2:3", 12345, "123::ff", 54321, IPPROTO_UDP, 424242);
	if(uid < 0) {
		fprintf(stderr, "add_pinhole() failed\n");
	}
	printf("add_pinhole() returned %d\n", uid);
	uid = add_pinhole("ep0", NULL, 0, "dead:beef::42:42", 8080, IPPROTO_UDP, 4321000);
	if(uid < 0) {
		fprintf(stderr, "add_pinhole() failed\n");
	}
	printf("add_pinhole() returned %d\n", uid);

	print_pinhole(1);
	print_pinhole(2);
	clean_pinhole_list(NULL);

	ret = delete_pinhole(1);
	printf("delete_pinhole() returned %d\n", ret);
	ret = delete_pinhole(2);
	printf("delete_pinhole() returned %d\n", ret);
#endif
	return 0;
}

