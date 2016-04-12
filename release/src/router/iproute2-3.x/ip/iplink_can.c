/*
 * iplink_can.c	CAN device support
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Wolfgang Grandegger <wg@grandegger.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/can/netlink.h>

#include "rt_names.h"
#include "utils.h"
#include "ip_common.h"

static void print_usage(FILE *f)
{
	fprintf(f,
		"Usage: ip link set DEVICE type can\n"
		"\t[ bitrate BITRATE [ sample-point SAMPLE-POINT] ] | \n"
		"\t[ tq TQ prop-seg PROP_SEG phase-seg1 PHASE-SEG1\n "
		"\t  phase-seg2 PHASE-SEG2 [ sjw SJW ] ]\n"
		"\n"
		"\t[ dbitrate BITRATE [ dsample-point SAMPLE-POINT] ] | \n"
		"\t[ dtq TQ dprop-seg PROP_SEG dphase-seg1 PHASE-SEG1\n "
		"\t  dphase-seg2 PHASE-SEG2 [ dsjw SJW ] ]\n"
		"\n"
		"\t[ loopback { on | off } ]\n"
		"\t[ listen-only { on | off } ]\n"
		"\t[ triple-sampling { on | off } ]\n"
		"\t[ one-shot { on | off } ]\n"
		"\t[ berr-reporting { on | off } ]\n"
		"\t[ fd { on | off } ]\n"
		"\t[ fd-non-iso { on | off } ]\n"
		"\t[ presume-ack { on | off } ]\n"
		"\n"
		"\t[ restart-ms TIME-MS ]\n"
		"\t[ restart ]\n"
		"\n"
		"\tWhere: BITRATE	:= { 1..1000000 }\n"
		"\t	  SAMPLE-POINT	:= { 0.000..0.999 }\n"
		"\t	  TQ		:= { NUMBER }\n"
		"\t	  PROP-SEG	:= { 1..8 }\n"
		"\t	  PHASE-SEG1	:= { 1..8 }\n"
		"\t	  PHASE-SEG2	:= { 1..8 }\n"
		"\t	  SJW		:= { 1..4 }\n"
		"\t	  RESTART-MS	:= { 0 | NUMBER }\n"
		);
}

static void usage(void)
{
	print_usage(stderr);
}

static int get_float(float *val, const char *arg)
{
	float res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtof(arg, &ptr);
	if (!ptr || ptr == arg || *ptr)
		return -1;
	*val = res;
	return 0;
}

static void set_ctrlmode(char* name, char *arg,
			 struct can_ctrlmode *cm, __u32 flags)
{
	if (strcmp(arg, "on") == 0) {
		cm->flags |= flags;
	} else if (strcmp(arg, "off") != 0) {
		fprintf(stderr,
			"Error: argument of \"%s\" must be \"on\" or \"off\", not \"%s\"\n",
			name, arg);
		exit(-1);
	}
	cm->mask |= flags;
}

static void print_ctrlmode(FILE *f, __u32 cm)
{
	fprintf(f, "<");
#define _PF(cmflag, cmname)					\
	if (cm & cmflag) {					\
		cm &= ~cmflag;					\
		fprintf(f, "%s%s", cmname, cm ? "," : "");	\
	}
	_PF(CAN_CTRLMODE_LOOPBACK, "LOOPBACK");
	_PF(CAN_CTRLMODE_LISTENONLY, "LISTEN-ONLY");
	_PF(CAN_CTRLMODE_3_SAMPLES, "TRIPLE-SAMPLING");
	_PF(CAN_CTRLMODE_ONE_SHOT, "ONE-SHOT");
	_PF(CAN_CTRLMODE_BERR_REPORTING, "BERR-REPORTING");
	_PF(CAN_CTRLMODE_FD, "FD");
	_PF(CAN_CTRLMODE_FD_NON_ISO, "FD-NON-ISO");
	_PF(CAN_CTRLMODE_PRESUME_ACK, "PRESUME-ACK");
#undef _PF
	if (cm)
		fprintf(f, "%x", cm);
	fprintf(f, "> ");
}

static int can_parse_opt(struct link_util *lu, int argc, char **argv,
			 struct nlmsghdr *n)
{
	struct can_bittiming bt, dbt;
	struct can_ctrlmode cm = {0, 0};

	memset(&bt, 0, sizeof(bt));
	memset(&dbt, 0, sizeof(dbt));
	while (argc > 0) {
		if (matches(*argv, "bitrate") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.bitrate, *argv, 0))
				invarg("invalid \"bitrate\" value\n", *argv);
		} else if (matches(*argv, "sample-point") == 0) {
			float sp;

			NEXT_ARG();
			if (get_float(&sp, *argv))
				invarg("invalid \"sample-point\" value\n",
				       *argv);
			bt.sample_point = (__u32)(sp * 1000);
		} else if (matches(*argv, "tq") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.tq, *argv, 0))
				invarg("invalid \"tq\" value\n", *argv);
		} else if (matches(*argv, "prop-seg") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.prop_seg, *argv, 0))
				invarg("invalid \"prop-seg\" value\n", *argv);
		} else if (matches(*argv, "phase-seg1") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.phase_seg1, *argv, 0))
				invarg("invalid \"phase-seg1\" value\n", *argv);
		} else if (matches(*argv, "phase-seg2") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.phase_seg2, *argv, 0))
				invarg("invalid \"phase-seg2\" value\n", *argv);
		} else if (matches(*argv, "sjw") == 0) {
			NEXT_ARG();
			if (get_u32(&bt.sjw, *argv, 0))
				invarg("invalid \"sjw\" value\n", *argv);
		} else if (matches(*argv, "dbitrate") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.bitrate, *argv, 0))
				invarg("invalid \"dbitrate\" value\n", *argv);
		} else if (matches(*argv, "dsample-point") == 0) {
			float sp;

			NEXT_ARG();
			if (get_float(&sp, *argv))
				invarg("invalid \"dsample-point\" value\n", *argv);
			dbt.sample_point = (__u32)(sp * 1000);
		} else if (matches(*argv, "dtq") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.tq, *argv, 0))
				invarg("invalid \"dtq\" value\n", *argv);
		} else if (matches(*argv, "dprop-seg") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.prop_seg, *argv, 0))
				invarg("invalid \"dprop-seg\" value\n", *argv);
		} else if (matches(*argv, "dphase-seg1") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.phase_seg1, *argv, 0))
				invarg("invalid \"dphase-seg1\" value\n", *argv);
		} else if (matches(*argv, "dphase-seg2") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.phase_seg2, *argv, 0))
				invarg("invalid \"dphase-seg2\" value\n", *argv);
		} else if (matches(*argv, "dsjw") == 0) {
			NEXT_ARG();
			if (get_u32(&dbt.sjw, *argv, 0))
				invarg("invalid \"dsjw\" value\n", *argv);
		} else if (matches(*argv, "loopback") == 0) {
			NEXT_ARG();
			set_ctrlmode("loopback", *argv, &cm,
				     CAN_CTRLMODE_LOOPBACK);
		} else if (matches(*argv, "listen-only") == 0) {
			NEXT_ARG();
			set_ctrlmode("listen-only", *argv, &cm,
				     CAN_CTRLMODE_LISTENONLY);
		} else if (matches(*argv, "triple-sampling") == 0) {
			NEXT_ARG();
			set_ctrlmode("triple-sampling", *argv, &cm,
				     CAN_CTRLMODE_3_SAMPLES);
		} else if (matches(*argv, "one-shot") == 0) {
			NEXT_ARG();
			set_ctrlmode("one-shot", *argv, &cm,
				     CAN_CTRLMODE_ONE_SHOT);
		} else if (matches(*argv, "berr-reporting") == 0) {
			NEXT_ARG();
			set_ctrlmode("berr-reporting", *argv, &cm,
				     CAN_CTRLMODE_BERR_REPORTING);
		} else if (matches(*argv, "fd") == 0) {
			NEXT_ARG();
			set_ctrlmode("fd", *argv, &cm,
				     CAN_CTRLMODE_FD);
		} else if (matches(*argv, "fd-non-iso") == 0) {
			NEXT_ARG();
			set_ctrlmode("fd-non-iso", *argv, &cm,
				     CAN_CTRLMODE_FD_NON_ISO);
		} else if (matches(*argv, "presume-ack") == 0) {
			NEXT_ARG();
			set_ctrlmode("presume-ack", *argv, &cm,
				     CAN_CTRLMODE_PRESUME_ACK);
		} else if (matches(*argv, "restart") == 0) {
			__u32 val = 1;

			addattr32(n, 1024, IFLA_CAN_RESTART, val);
		} else if (matches(*argv, "restart-ms") == 0) {
			__u32 val;

			NEXT_ARG();
			if (get_u32(&val, *argv, 0))
				invarg("invalid \"restart-ms\" value\n", *argv);
			addattr32(n, 1024, IFLA_CAN_RESTART_MS, val);
		} else if (matches(*argv, "help") == 0) {
			usage();
			return -1;
		} else {
			fprintf(stderr, "can: unknown option \"%s\"\n", *argv);
			usage();
			return -1;
		}
		argc--, argv++;
	}

	if (bt.bitrate || bt.tq)
		addattr_l(n, 1024, IFLA_CAN_BITTIMING, &bt, sizeof(bt));
	if (dbt.bitrate || dbt.tq)
		addattr_l(n, 1024, IFLA_CAN_DATA_BITTIMING, &dbt, sizeof(dbt));
	if (cm.mask)
		addattr_l(n, 1024, IFLA_CAN_CTRLMODE, &cm, sizeof(cm));

	return 0;
}

static const char *can_state_names[] = {
	[CAN_STATE_ERROR_ACTIVE] = "ERROR-ACTIVE",
	[CAN_STATE_ERROR_WARNING] = "ERROR-WARNING",
	[CAN_STATE_ERROR_PASSIVE] = "ERROR-PASSIVE",
	[CAN_STATE_BUS_OFF] = "BUS-OFF",
	[CAN_STATE_STOPPED] = "STOPPED",
	[CAN_STATE_SLEEPING] = "SLEEPING"
};

static void can_print_opt(struct link_util *lu, FILE *f, struct rtattr *tb[])
{
	if (!tb)
		return;

	if (tb[IFLA_CAN_CTRLMODE]) {
		struct can_ctrlmode *cm = RTA_DATA(tb[IFLA_CAN_CTRLMODE]);

		if (cm->flags)
			print_ctrlmode(f, cm->flags);
	}

	if (tb[IFLA_CAN_STATE]) {
		int *state = RTA_DATA(tb[IFLA_CAN_STATE]);

		fprintf(f, "state %s ", *state <= CAN_STATE_MAX ?
			can_state_names[*state] : "UNKNOWN");
	}

	if (tb[IFLA_CAN_BERR_COUNTER]) {
		struct can_berr_counter *bc =
			RTA_DATA(tb[IFLA_CAN_BERR_COUNTER]);

		fprintf(f, "(berr-counter tx %d rx %d) ", bc->txerr, bc->rxerr);
	}

	if (tb[IFLA_CAN_RESTART_MS]) {
		__u32 *restart_ms = RTA_DATA(tb[IFLA_CAN_RESTART_MS]);

		fprintf(f, "restart-ms %d ", *restart_ms);
	}

	if (tb[IFLA_CAN_BITTIMING]) {
		struct can_bittiming *bt = RTA_DATA(tb[IFLA_CAN_BITTIMING]);

		fprintf(f, "\n	  "
			"bitrate %d sample-point %.3f ",
			bt->bitrate, (float)bt->sample_point / 1000.);
		fprintf(f, "\n	  "
			"tq %d prop-seg %d phase-seg1 %d phase-seg2 %d sjw %d",
			bt->tq, bt->prop_seg, bt->phase_seg1, bt->phase_seg2,
			bt->sjw);
	}

	if (tb[IFLA_CAN_BITTIMING_CONST]) {
		struct can_bittiming_const *btc =
			RTA_DATA(tb[IFLA_CAN_BITTIMING_CONST]);

		fprintf(f, "\n	  "
			"%s: tseg1 %d..%d tseg2 %d..%d "
			"sjw 1..%d brp %d..%d brp-inc %d",
			btc->name, btc->tseg1_min, btc->tseg1_max,
			btc->tseg2_min, btc->tseg2_max, btc->sjw_max,
			btc->brp_min, btc->brp_max, btc->brp_inc);
	}

	if (tb[IFLA_CAN_DATA_BITTIMING]) {
		struct can_bittiming *dbt =
			RTA_DATA(tb[IFLA_CAN_DATA_BITTIMING]);

		fprintf(f, "\n	  "
			"dbitrate %d dsample-point %.3f ",
			dbt->bitrate, (float)dbt->sample_point / 1000.);
		fprintf(f, "\n	  "
			"dtq %d dprop-seg %d dphase-seg1 %d "
			"dphase-seg2 %d dsjw %d",
			dbt->tq, dbt->prop_seg, dbt->phase_seg1,
			dbt->phase_seg2, dbt->sjw);
	}

	if (tb[IFLA_CAN_DATA_BITTIMING_CONST]) {
		struct can_bittiming_const *dbtc =
			RTA_DATA(tb[IFLA_CAN_DATA_BITTIMING_CONST]);

		fprintf(f, "\n	  "
			"%s: dtseg1 %d..%d dtseg2 %d..%d "
			"dsjw 1..%d dbrp %d..%d dbrp-inc %d",
			dbtc->name, dbtc->tseg1_min, dbtc->tseg1_max,
			dbtc->tseg2_min, dbtc->tseg2_max, dbtc->sjw_max,
			dbtc->brp_min, dbtc->brp_max, dbtc->brp_inc);
	}

	if (tb[IFLA_CAN_CLOCK]) {
		struct can_clock *clock = RTA_DATA(tb[IFLA_CAN_CLOCK]);

		fprintf(f, "\n	  clock %d", clock->freq);
	}

}

static void can_print_xstats(struct link_util *lu,
			     FILE *f, struct rtattr *xstats)
{
	struct can_device_stats *stats;

	if (xstats && RTA_PAYLOAD(xstats) == sizeof(*stats)) {
		stats = RTA_DATA(xstats);
		fprintf(f, "\n	  "
			"re-started bus-errors arbit-lost "
			"error-warn error-pass bus-off");
		fprintf(f, "\n	  %-10d %-10d %-10d %-10d %-10d %-10d",
			stats->restarts, stats->bus_error,
			stats->arbitration_lost, stats->error_warning,
			stats->error_passive, stats->bus_off);
	}
}

static void can_print_help(struct link_util *lu, int argc, char **argv,
	FILE *f)
{
	print_usage(f);
}

struct link_util can_link_util = {
	.id		= "can",
	.maxattr	= IFLA_CAN_MAX,
	.parse_opt	= can_parse_opt,
	.print_opt	= can_print_opt,
	.print_xstats	= can_print_xstats,
	.print_help	= can_print_help,
};
