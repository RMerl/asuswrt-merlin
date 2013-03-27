/*
 * m_pedit.h		generic packet editor actions module
 *
 *		This program is free software; you can distribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:  J Hadi Salim (hadi@cyberus.ca)
 *
 */

#ifndef _ACT_PEDIT_H_
#define _ACT_PEDIT_H_ 1

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
#include <linux/tc_act/tc_pedit.h>

#define MAX_OFFS 128

#define TIPV4 1
#define TIPV6 2
#define TINT 3
#define TU32 4

#define RU32 0xFFFFFFFF
#define RU16 0xFFFF
#define RU8 0xFF

#define PEDITKINDSIZ 16

struct m_pedit_util
{
	struct m_pedit_util *next;
	char    id[PEDITKINDSIZ];
	int     (*parse_peopt)(int *argc_p, char ***argv_p,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
};


extern int parse_cmd(int *argc_p, char ***argv_p, __u32 len, int type,__u32 retain,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int pack_key(struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int pack_key32(__u32 retain,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int pack_key16(__u32 retain,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int pack_key8(__u32 retain,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int parse_val(int *argc_p, char ***argv_p, __u32 * val, int type);
extern int parse_cmd(int *argc_p, char ***argv_p, __u32 len, int type,__u32 retain,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
extern int parse_offset(int *argc_p, char ***argv_p,struct tc_pedit_sel *sel,struct tc_pedit_key *tkey);
int parse_pedit(struct action_util *a, int *argc_p, char ***argv_p, int tca_id, struct nlmsghdr *n);
extern int print_pedit(struct action_util *au,FILE * f, struct rtattr *arg);
extern int pedit_print_xstats(struct action_util *au, FILE *f, struct rtattr *xstats);

#endif
