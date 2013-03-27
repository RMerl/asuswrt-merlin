/*
 * m_pedit.c		packet editor: IPV4/6 header
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 *
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
#include "m_pedit.h"

static int
parse_ip(int *argc_p, char ***argv_p,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey)
{
	int res = -1;
	int argc = *argc_p;
	char **argv = *argv_p;

	if (argc < 2)
		return -1;

	if (strcmp(*argv, "src") == 0) {
		NEXT_ARG();
		tkey->off = 12;
		res = parse_cmd(&argc, &argv, 4, TIPV4,RU32,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "dst") == 0) {
		NEXT_ARG();
		tkey->off = 16;
		res = parse_cmd(&argc, &argv, 4, TIPV4,RU32,sel,tkey);
		goto done;
	}
	/* jamal - look at these and make them either old or new
	** scheme given diffserv
	** dont forget the CE bit
	*/
	if (strcmp(*argv, "tos") == 0 || matches(*argv, "dsfield") == 0) {
		NEXT_ARG();
		tkey->off = 1;
		res = parse_cmd(&argc, &argv,  1, TU32,RU8,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "ihl") == 0) {
		NEXT_ARG();
		tkey->off = 0;
		res = parse_cmd(&argc, &argv, 1, TU32,RU8,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "protocol") == 0) {
		NEXT_ARG();
		tkey->off = 9;
		res = parse_cmd(&argc, &argv, 1, TU32,RU8,sel,tkey);
		goto done;
	}
	/* jamal - fix this */
	if (matches(*argv, "precedence") == 0) {
		NEXT_ARG();
		tkey->off = 1;
		res = parse_cmd(&argc, &argv, 1, TU32,RU8,sel,tkey);
		goto done;
	}
	/* jamal - validate this at some point */
	if (strcmp(*argv, "nofrag") == 0) {
		NEXT_ARG();
		tkey->off = 6;
		res = parse_cmd(&argc, &argv, 1, TU32,0x3F,sel,tkey);
		goto done;
	}
	/* jamal - validate this at some point */
	if (strcmp(*argv, "firstfrag") == 0) {
		NEXT_ARG();
		tkey->off = 6;
		res = parse_cmd(&argc, &argv, 1, TU32,0x1F,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "ce") == 0) {
		NEXT_ARG();
		tkey->off = 6;
		res = parse_cmd(&argc, &argv, 1, TU32,0x80,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "df") == 0) {
		NEXT_ARG();
		tkey->off = 6;
		res = parse_cmd(&argc, &argv, 1, TU32,0x40,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "mf") == 0) {
		NEXT_ARG();
		tkey->off = 6;
		res = parse_cmd(&argc, &argv, 1, TU32,0x20,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "dport") == 0) {
		NEXT_ARG();
		tkey->off = 22;
		res = parse_cmd(&argc, &argv, 2, TU32,RU16,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "sport") == 0) {
		NEXT_ARG();
		tkey->off = 20;
		res = parse_cmd(&argc, &argv, 2, TU32,RU16,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "icmp_type") == 0) {
		NEXT_ARG();
		tkey->off = 20;
		res = parse_cmd(&argc, &argv, 1, TU32,RU8,sel,tkey);
		goto done;
	}
	if (strcmp(*argv, "icmp_code") == 0) {
		NEXT_ARG();
		tkey->off = 20;
		res = parse_cmd(&argc, &argv, 1, TU32,RU8,sel,tkey);
		goto done;
	}
	return -1;

      done:
	*argc_p = argc;
	*argv_p = argv;
	return res;
}

static int
parse_ip6(int *argc_p, char ***argv_p,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey)
{
	int res = -1;
	return res;
}

struct m_pedit_util p_pedit_ip = {
	NULL,
	"ip",
	parse_ip,
};


struct m_pedit_util p_pedit_ip6 = {
	NULL,
	"ip6",
	parse_ip6,
};
