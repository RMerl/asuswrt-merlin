/*
 * IGSL Command Line Utility: This utility can be used to add/remove
 * snooping capability on desired bridge interface.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsu.c 431686 2013-10-24 10:31:37Z $
 */
#include <stdio.h>
#include <sys/types.h>
#include <typedefs.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#if defined(linux)
#include "igsu_linux.h"
#else /* defined(vxworks) */
#error "Unsupported osl"
#endif 
#include <igs_cfg.h>
#include "igsu.h"

#define MAX_DATA_SIZE  sizeof(igs_cfg_request_t)

static int igs_cfg_bridge_add(char *argv[]);
static int igs_cfg_bridge_del(char *argv[]);
static int igs_cfg_bridge_list(char *argv[]);
static int igs_cfg_sdb_list(char *argv[]);
static int igs_cfg_rtport_list(char *argv[]);
static int igs_cfg_stats_show(char *argv[]);

static igs_cmd_arg_t cmd_args[] =
{
	{
		"add",
		"bridge",
		igs_cfg_bridge_add,
		IGS_ARGC_ADD_BRIDGE
	},
	{
		"del",
		"bridge",
		igs_cfg_bridge_del,
		IGS_ARGC_DEL_BRIDGE
	},
	{
		"list",
		"bridge",
		igs_cfg_bridge_list,
		IGS_ARGC_LIST_BRIDGE
	},
	{
		"list",
		"sdb",
		igs_cfg_sdb_list,
		IGS_ARGC_LIST_IGSDB
	},
	{
		"list",
		"rtport",
		igs_cfg_rtport_list,
		IGS_ARGC_LIST_RTPORT
	},
	{
		"show",
		"stats",
		igs_cfg_stats_show,
		IGS_ARGC_SHOW_STATS
	}
};

static void
igs_usage(FILE *fid)
{
	fprintf(fid, IGS_USAGE);
	return;
}

static int
igs_cfg_bridge_add(char *argv[])
{
	igs_cfg_request_t req;

	bzero((char *)&req, sizeof(igs_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = IGSCFG_CMD_BR_ADD;
	req.oper_type = IGSCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (igs_cfg_request_send(&req, sizeof(igs_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to IGS\n");
		return (FAILURE);
	}

	if (req.status != IGSCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
igs_cfg_bridge_del(char *argv[])
{
	igs_cfg_request_t req;

	bzero((char *)&req, sizeof(igs_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = IGSCFG_CMD_BR_DEL;
	req.oper_type = IGSCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (igs_cfg_request_send(&req, sizeof(igs_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to IGS\n");
		return (FAILURE);
	}

	if (req.status != IGSCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
igs_cfg_bridge_list(char *argv[])
{
	fprintf(stdout, "TBD\n");
	return (SUCCESS);
}

static int
igs_cfg_sdb_list(char *argv[])
{
	igs_cfg_request_t req;
	igs_cfg_sdb_list_t *list;
	bool first_row = TRUE;
	int32 i;

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.inst_id[sizeof(req.inst_id)-1] = '\0';

	req.command_id = IGSCFG_CMD_IGSDB_LIST;
	req.oper_type = IGSCFG_OPER_TYPE_GET;
	req.size = sizeof(req.arg);
	list = (igs_cfg_sdb_list_t *)req.arg;

	if (igs_cfg_request_send(&req, sizeof(igs_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to IGS\n");
		return (FAILURE);
	}

	if (req.status != IGSCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "Unable to get the IGSDB list\n");
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	fprintf(stdout, "Group           Members         Interface\n");

	for (i = 0; i < list->num_entries; i++)
	{
		first_row = TRUE;
		fprintf(stdout, "%08x        ", list->sdb_entry[i].mgrp_ip);
		if (first_row)
		{
			fprintf(stdout, "%08x        ", list->sdb_entry[i].mh_ip);
			fprintf(stdout, "%s\n", list->sdb_entry[i].if_name);
			first_row = FALSE;
			continue;
		}
		fprintf(stdout, "                ");
		fprintf(stdout, "%08x        ", list->sdb_entry[i].mh_ip);
		fprintf(stdout, "%s\n", list->sdb_entry[i].if_name);
	}

	return (SUCCESS);
}

static int
igs_cfg_rtport_list(char *argv[])
{
	igs_cfg_request_t req;
	igs_cfg_rtport_list_t *list;
	int32 i;

	bzero((char *)&req, sizeof(igs_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = IGSCFG_CMD_RTPORT_LIST;
	req.oper_type = IGSCFG_OPER_TYPE_GET;
	req.size = sizeof(req.arg);

	/* Send request to kernel */
	if (igs_cfg_request_send(&req, sizeof(igs_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to IGS\n");
		return (FAILURE);
	}

	if (req.status != IGSCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	fprintf(stdout, "Router          Interface\n");

	list = (igs_cfg_rtport_list_t *)req.arg;
	for (i = 0; i < list->num_entries; i++)
	{
		fprintf(stdout, "%08x        ", list->rtport_entry[i].mr_ip);
		fprintf(stdout, "%-15s", list->rtport_entry[i].if_name);
	}

	fprintf(stdout, "\n");

	return (SUCCESS);
}

static int
igs_cfg_stats_show(char *argv[])
{
	igs_cfg_request_t req;
	igs_stats_t *igss;

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.inst_id[sizeof(req.inst_id)-1] = '\0';

	req.command_id = IGSCFG_CMD_IGS_STATS;
	req.oper_type = IGSCFG_OPER_TYPE_GET;
	req.size = sizeof(igs_stats_t);
	igss = (igs_stats_t *)req.arg;

	if (igs_cfg_request_send(&req, sizeof(igs_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to IGS\n");
		return (FAILURE);
	}

	if (req.status != IGSCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "Unable to get the IGS stats\n");
		return (FAILURE);
	}

	fprintf(stdout, "IgmpPkts        IgmpQueries     "
	        "IgmpReports     IgmpV2Reports   IgmpLeaves\n");
	fprintf(stdout, "%-15d %-15d %-15d %-15d %d\n",
	        igss->igmp_packets, igss->igmp_queries,
	        igss->igmp_reports, igss->igmp_v2reports,
	        igss->igmp_leaves);
	fprintf(stdout, "IgmpNotHandled  McastGroups     "
	        "McastMembers    MemTimeouts\n");
	fprintf(stdout, "%-15d %-15d %-15d %d\n",
	        igss->igmp_not_handled, igss->igmp_mcast_groups,
	        igss->igmp_mcast_members, igss->igmp_mem_timeouts);

	return (SUCCESS);
}

int
#if defined(linux)
main(int argc, char *argv[])
#else /* defined(vxworks) */
#error "Unsupported osl"
#endif 
{
	int j, ret;
	bool cmd_syntax = FALSE;

	if (argc < 2)
	{
		igs_usage(stdout);
		return (SUCCESS);
	}

	/* Find the command type */
	for (j = 0; j < sizeof(cmd_args)/sizeof(cmd_args[0]); j++)
	{
		if ((strcmp(argv[1], cmd_args[j].cmd_oper_str) == 0) &&
		    (argc - 1 == cmd_args[j].arg_count) &&
		    ((cmd_args[j].cmd_id_str == NULL) ||
		    ((argv[2] != NULL) &&
		    (strcmp(argv[2], cmd_args[j].cmd_id_str) == 0))))
		{
			cmd_syntax = TRUE;
			break;
		}
	}

	if (!cmd_syntax)
	{
		igs_usage(stdout);
		return (SUCCESS);
	}

	/* Call the command processing function. This function parses
	 * prepare command request and sends it kernel.
	 */
	if ((ret = cmd_args[j].input(argv)) < 0)
	{
		fprintf(stderr, "Command failed\n");
		return (FAILURE);
	}

	return (SUCCESS);
}
