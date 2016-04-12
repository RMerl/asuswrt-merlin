/*
 * m_simple.c	simple action
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	J Hadi Salim <jhs@mojatatu.com>
 *
 * Pedagogical example. Adds a string that will be printed every time
 * the simple instance is hit.
 * Use this as a skeleton action and keep modifying it to meet your needs.
 * Look at linux/tc_act/tc_defact.h for the different components ids and
 * definitions used in  this actions
 *
 * example use, yell "Incoming ICMP!" every time you see an incoming ICMP on
 * eth0. Steps are:
 * 1) Add an ingress qdisc point to eth0
 * 2) Start a chain on ingress of eth0 that first matches ICMP then invokes
 *    the simple action to shout.
 * 3) display stats and show that no packet has been seen by the action
 * 4) Send one ping packet to google (expect to receive a response back)
 * 5) grep the logs to see the logged message
 * 6) display stats again and observe increment by 1
 *
  hadi@noma1:$ tc qdisc add dev eth0 ingress
  hadi@noma1:$tc filter add dev eth0 parent ffff: protocol ip prio 5 \
	 u32 match ip protocol 1 0xff flowid 1:1 action simple "Incoming ICMP"

  hadi@noma1:$ sudo tc -s filter ls  dev eth0 parent ffff:
   filter protocol ip pref 5 u32
   filter protocol ip pref 5 u32 fh 800: ht divisor 1
   filter protocol ip pref 5 u32 fh 800::800 order 2048 key ht 800 bkt 0 flowid 1:1
     match 00010000/00ff0000 at 8
	action order 1: Simple <Incoming ICMP>
	 index 4 ref 1 bind 1 installed 29 sec used 29 sec
	 Action statistics:
		Sent 0 bytes 0 pkt (dropped 0, overlimits 0 requeues 0)
		backlog 0b 0p requeues 0


  hadi@noma1$ ping -c 1 www.google.ca
  PING www.google.ca (74.125.225.120) 56(84) bytes of data.
  64 bytes from ord08s08-in-f24.1e100.net (74.125.225.120): icmp_req=1 ttl=53 time=31.3 ms

  --- www.google.ca ping statistics ---
  1 packets transmitted, 1 received, 0% packet loss, time 0ms
  rtt min/avg/max/mdev = 31.316/31.316/31.316/0.000 ms

  hadi@noma1$ dmesg | grep simple
  [135354.473951] simple: Incoming ICMP_1

  hadi@noma1$ sudo tc/tc -s filter ls  dev eth0 parent ffff:
  filter protocol ip pref 5 u32
  filter protocol ip pref 5 u32 fh 800: ht divisor 1
  filter protocol ip pref 5 u32 fh 800::800 order 2048 key ht 800 bkt 0 flowid 1:1
    match 00010000/00ff0000 at 8
	action order 1: Simple <Incoming ICMP>
	 index 4 ref 1 bind 1 installed 206 sec used 67 sec
	Action statistics:
	Sent 84 bytes 1 pkt (dropped 0, overlimits 0 requeues 0)
	backlog 0b 0p requeues 0
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include "utils.h"
#include "tc_util.h"
#include <linux/tc_act/tc_defact.h>

#ifndef SIMP_MAX_DATA
#define SIMP_MAX_DATA   32
#endif
static void explain(void)
{
	fprintf(stderr, "Usage: ... simple STRING\n"
		"STRING being an arbitrary string\n"
		"example: \"simple blah\"\n");
}

static void usage(void)
{
	explain();
	exit(-1);
}

static int
parse_simple(struct action_util *a, int *argc_p, char ***argv_p, int tca_id,
	     struct nlmsghdr *n)
{
	struct tc_defact sel = {};
	int argc = *argc_p;
	char **argv = *argv_p;
	int ok = 0;
	struct rtattr *tail;
	char *simpdata = NULL;


	while (argc > 0) {
		if (matches(*argv, "simple") == 0) {
			NEXT_ARG();
			simpdata = *argv;
			ok = 1;
			argc--;
			argv++;
			break;
		} else if (matches(*argv, "help") == 0) {
			usage();
		} else {
			break;
		}

	}

	if (!ok) {
		explain();
		return -1;
	}

	if (argc) {
		if (matches(*argv, "index") == 0) {
			NEXT_ARG();
			if (get_u32(&sel.index, *argv, 10)) {
				fprintf(stderr, "simple: Illegal \"index\"\n");
				return -1;
			}
			argc--;
			argv++;
		}
	}

	if (strlen(simpdata) > (SIMP_MAX_DATA - 1)) {
		fprintf(stderr, "simple: Illegal string len %ld <%s> \n",
			strlen(simpdata), simpdata);
		return -1;
	}

	sel.action = TC_ACT_PIPE;

	tail = NLMSG_TAIL(n);
	addattr_l(n, MAX_MSG, tca_id, NULL, 0);
	addattr_l(n, MAX_MSG, TCA_DEF_PARMS, &sel, sizeof(sel));
	addattr_l(n, MAX_MSG, TCA_DEF_DATA, simpdata, SIMP_MAX_DATA);
	tail->rta_len = (char *)NLMSG_TAIL(n) - (char *)tail;

	*argc_p = argc;
	*argv_p = argv;
	return 0;
}

static int print_simple(struct action_util *au, FILE * f, struct rtattr *arg)
{
	struct tc_defact *sel;
	struct rtattr *tb[TCA_DEF_MAX + 1];
	char *simpdata;

	if (arg == NULL)
		return -1;

	parse_rtattr_nested(tb, TCA_DEF_MAX, arg);

	if (tb[TCA_DEF_PARMS] == NULL) {
		fprintf(f, "[NULL simple parameters]");
		return -1;
	}
	sel = RTA_DATA(tb[TCA_DEF_PARMS]);

	if (tb[TCA_DEF_DATA] == NULL) {
		fprintf(f, "[missing simple string]");
		return -1;
	}

	simpdata = RTA_DATA(tb[TCA_DEF_DATA]);

	fprintf(f, "Simple <%s>\n", simpdata);
	fprintf(f, "\t index %d ref %d bind %d", sel->index,
		sel->refcnt, sel->bindcnt);

	if (show_stats) {
		if (tb[TCA_DEF_TM]) {
			struct tcf_t *tm = RTA_DATA(tb[TCA_DEF_TM]);
			print_tm(f, tm);
		}
	}
	fprintf(f, "\n");

	return 0;
}

struct action_util simple_action_util = {
	.id = "simple",
	.parse_aopt = parse_simple,
	.print_aopt = print_simple,
};
