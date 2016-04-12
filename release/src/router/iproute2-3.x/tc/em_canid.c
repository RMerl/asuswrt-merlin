/*
 * em_canid.c  Ematch rule to match CAN frames according to their CAN identifiers
 *
 *             This program is free software; you can distribute it and/or
 *             modify it under the terms of the GNU General Public License
 *             as published by the Free Software Foundation; either version
 *             2 of the License, or (at your option) any later version.
 *
 * Idea:       Oliver Hartkopp <oliver.hartkopp@volkswagen.de>
 * Copyright:  (c) 2011 Czech Technical University in Prague
 *             (c) 2011 Volkswagen Group Research
 * Authors:    Michal Sojka <sojkam1@fel.cvut.cz>
 *             Pavel Pisa <pisa@cmp.felk.cvut.cz>
 *             Rostislav Lisovy <lisovy@gmail.cz>
 * Funded by:  Volkswagen Group Research
 *
 * Documentation: http://rtime.felk.cvut.cz/can/socketcan-qdisc-final.pdf
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
#include <errno.h>
#include <linux/can.h>
#include <inttypes.h>
#include "m_ematch.h"

#define EM_CANID_RULES_MAX 400 /* Main reason for this number is Nelink
	message size limit equal to Single memory page size. When dump()
	is invoked, there are even some ematch related headers sent from
	kernel to userspace together with em_canid configuration --
	400*sizeof(struct can_filter) should fit without any problems */

extern struct ematch_util canid_ematch_util;
struct rules {
	struct can_filter *rules_raw;
	int rules_capacity;	/* Size of array allocated for rules_raw */
	int rules_cnt;		/* Actual number of rules stored in rules_raw */
};

static void canid_print_usage(FILE *fd)
{
	fprintf(fd,
		"Usage: canid(IDLIST)\n" \
		"where: IDLIST := IDSPEC [ IDLIST ]\n" \
		"       IDSPEC := { ’sff’ CANID | ’eff’ CANID }\n" \
		"       CANID := ID[:MASK]\n" \
		"       ID, MASK := hexadecimal number (i.e. 0x123)\n" \
		"Example: canid(sff 0x123 sff 0x124 sff 0x125:0xf)\n");
}

static int canid_parse_rule(struct rules *rules, struct bstr *a, int iseff)
{
	unsigned int can_id = 0;
	unsigned int can_mask = 0;

	if (sscanf(a->data, "%"SCNx32 ":" "%"SCNx32, &can_id, &can_mask) != 2) {
		if (sscanf(a->data, "%"SCNx32, &can_id) != 1) {
			return -1;
		} else {
			can_mask = (iseff) ? CAN_EFF_MASK : CAN_SFF_MASK;
		}
	}

	/* Stretch rules array up to EM_CANID_RULES_MAX if necessary */
	if (rules->rules_cnt == rules->rules_capacity) {
		if (rules->rules_capacity <= EM_CANID_RULES_MAX/2) {
			rules->rules_capacity *= 2;
			rules->rules_raw = realloc(rules->rules_raw,
				sizeof(struct can_filter) * rules->rules_capacity);
		} else {
			return -2;
		}
	}

	rules->rules_raw[rules->rules_cnt].can_id =
		can_id | ((iseff) ? CAN_EFF_FLAG : 0);
	rules->rules_raw[rules->rules_cnt].can_mask =
		can_mask | CAN_EFF_FLAG;

	rules->rules_cnt++;

	return 0;
}

static int canid_parse_eopt(struct nlmsghdr *n, struct tcf_ematch_hdr *hdr,
			  struct bstr *args)
{
	int iseff = 0;
	int ret = 0;
	struct rules rules = {
		.rules_capacity = 25, /* Denominator of EM_CANID_RULES_MAX
			Will be multiplied by 2 to calculate the size for realloc() */
		.rules_cnt = 0
	};

#define PARSE_ERR(CARG, FMT, ARGS...) \
	em_parse_error(EINVAL, args, CARG, &canid_ematch_util, FMT, ##ARGS)

	if (args == NULL)
		return PARSE_ERR(args, "canid: missing arguments");

	rules.rules_raw = malloc(sizeof(struct can_filter) * rules.rules_capacity);
	memset(rules.rules_raw, 0, sizeof(struct can_filter) * rules.rules_capacity);

	do {
		if (!bstrcmp(args, "sff")) {
			iseff = 0;
		} else if (!bstrcmp(args, "eff")) {
			iseff = 1;
		} else {
			ret = PARSE_ERR(args, "canid: invalid key");
			goto exit;
		}

		args = bstr_next(args);
		if (args == NULL) {
			ret = PARSE_ERR(args, "canid: missing argument");
			goto exit;
		}

		ret = canid_parse_rule(&rules, args, iseff);
		if (ret == -1) {
			ret = PARSE_ERR(args, "canid: Improperly formed CAN ID & mask\n");
			goto exit;
		} else if (ret == -2) {
			ret = PARSE_ERR(args, "canid: Too many arguments on input\n");
			goto exit;
		}
	} while ((args = bstr_next(args)) != NULL);

	addraw_l(n, MAX_MSG, hdr, sizeof(*hdr));
	addraw_l(n, MAX_MSG, rules.rules_raw,
		sizeof(struct can_filter) * rules.rules_cnt);

#undef PARSE_ERR
exit:
	free(rules.rules_raw);
	return ret;
}

static int canid_print_eopt(FILE *fd, struct tcf_ematch_hdr *hdr, void *data,
			  int data_len)
{
	struct can_filter *conf = data; /* Array with rules */
	int rules_count;
	int i;

	rules_count = data_len / sizeof(struct can_filter);

	for (i = 0; i < rules_count; i++) {
		struct can_filter *pcfltr = &conf[i];

		if (pcfltr->can_id & CAN_EFF_FLAG) {
			if (pcfltr->can_mask == (CAN_EFF_FLAG | CAN_EFF_MASK))
				fprintf(fd, "eff 0x%"PRIX32,
						pcfltr->can_id & CAN_EFF_MASK);
			else
				fprintf(fd, "eff 0x%"PRIX32":0x%"PRIX32,
						pcfltr->can_id & CAN_EFF_MASK,
						pcfltr->can_mask & CAN_EFF_MASK);
		} else {
			if (pcfltr->can_mask == (CAN_EFF_FLAG | CAN_SFF_MASK))
				fprintf(fd, "sff 0x%"PRIX32,
						pcfltr->can_id & CAN_SFF_MASK);
			else
				fprintf(fd, "sff 0x%"PRIX32":0x%"PRIX32,
						pcfltr->can_id & CAN_SFF_MASK,
						pcfltr->can_mask & CAN_SFF_MASK);
		}

		if ((i + 1) < rules_count)
			fprintf(fd, " ");
	}

	return 0;
}

struct ematch_util canid_ematch_util = {
	.kind = "canid",
	.kind_num = TCF_EM_CANID,
	.parse_eopt = canid_parse_eopt,
	.print_eopt = canid_print_eopt,
	.print_usage = canid_print_usage
};
