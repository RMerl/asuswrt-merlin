/*
 * EMFL Command Line Utility: This utility can be used to start/stop
 * EMF, enable/disable BSS forwarding, add/delete/list the Static
 * MFDB entries.
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfu.c 437582 2013-11-19 10:34:24Z $
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
#include <shutils.h>
#include <emf_cfg.h>
#include "emfu.h"
#if defined(linux)
#include "emfu_linux.h"
#include <arpa/inet.h>
#else /* defined(vxworks) */
#error "Unsupported osl"
#endif 

#define MAX_DATA_SIZE  sizeof(emf_cfg_request_t)

static int emf_cfg_enable(char *argv[]);
static int emf_cfg_disable(char *argv[]);
static int emf_cfg_status_get(char *argv[]);
static int emf_cfg_bridge_add(char *argv[]);
static int emf_cfg_bridge_del(char *argv[]);
static int emf_cfg_iface_add(char *argv[]);
static int emf_cfg_iface_del(char *argv[]);
static int emf_cfg_iface_list(char *argv[]);
static int emf_cfg_uffp_add(char *argv[]);
static int emf_cfg_uffp_del(char *argv[]);
static int emf_cfg_uffp_list(char *argv[]);
static int emf_cfg_mfdb_add(char *argv[]);
static int emf_cfg_mfdb_del(char *argv[]);
static int emf_cfg_mfdb_clear(char *argv[]);
static int emf_cfg_mfdb_list(char *argv[]);
static int emf_cfg_stats_show(char *argv[]);

static emf_cmd_arg_t cmd_args[] =
{
	{
		"start",
		NULL,
		emf_cfg_enable,
		EMF_ARGC_ENABLE_FWD
	},
	{
		"stop",
		NULL,
		emf_cfg_disable,
		EMF_ARGC_DISABLE_FWD
	},
	{
		"status",
		NULL,
		emf_cfg_status_get,
		EMF_ARGC_GET_FWD
	},
	{
		"add",
		"bridge",
		emf_cfg_bridge_add,
		EMF_ARGC_ADD_BRIDGE
	},
	{
		"del",
		"bridge",
		emf_cfg_bridge_del,
		EMF_ARGC_DEL_BRIDGE
	},
	{
		"add",
		"iface",
		emf_cfg_iface_add,
		EMF_ARGC_ADD_IF
	},
	{
		"del",
		"iface",
		emf_cfg_iface_del,
		EMF_ARGC_DEL_IF
	},
	{
		"list",
		"iface",
		emf_cfg_iface_list,
		EMF_ARGC_LIST_IF
	},
	{
		"add",
		"uffp",
		emf_cfg_uffp_add,
		EMF_ARGC_ADD_UFFP
	},
	{
		"del",
		"uffp",
		emf_cfg_uffp_del,
		EMF_ARGC_DEL_UFFP
	},
	{
		"list",
		"uffp",
		emf_cfg_uffp_list,
		EMF_ARGC_LIST_UFFP
	},
	{
		"add",
		"rtport",
		emf_cfg_uffp_add,
		EMF_ARGC_ADD_RTPORT
	},
	{
		"del",
		"rtport",
		emf_cfg_uffp_del,
		EMF_ARGC_DEL_RTPORT
	},
	{
		"list",
		"rtport",
		emf_cfg_uffp_list,
		EMF_ARGC_LIST_RTPORT
	},
	{
		"add",
		"mfdb",
		emf_cfg_mfdb_add,
		EMF_ARGC_ADD_MFDB
	},
	{
		"del",
		"mfdb",
		emf_cfg_mfdb_del,
		EMF_ARGC_DEL_MFDB
	},
	{
		"clear",
		"mfdb",
		emf_cfg_mfdb_clear,
		EMF_ARGC_CLEAR_MFDB
	},
	{
		"list",
		"mfdb",
		emf_cfg_mfdb_list,
		EMF_ARGC_LIST_MFDB
	},
	{
		"show",
		"stats",
		emf_cfg_stats_show,
		EMF_ARGC_SHOW_STATS
	}
};

static void
emf_usage(FILE *fid)
{
	fprintf(fid, EMF_USAGE);
	return;
}

static int
emf_cfg_enable(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[2], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_EMF_ENABLE;
	req.size = sizeof(bool);
	req.oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req.arg = TRUE;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_disable(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[2], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_EMF_ENABLE;
	req.size = sizeof(bool);
	req.oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req.arg = FALSE;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_status_get(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[2], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_EMF_ENABLE;
	req.oper_type = EMFCFG_OPER_TYPE_GET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%sUnable to read Multicast forwarding status\n", req.arg);
		return (FAILURE);
	}

	fprintf(stdout, "Multicast forwarding status: %s",
	        (*(bool *)req.arg ? "ENABLED\n" : "DISABLED\n"));

	return (SUCCESS);
}

static int
emf_cfg_bridge_add(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id) - 1);
	req.command_id = EMFCFG_CMD_BR_ADD;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_bridge_del(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_BR_DEL;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_iface_add(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_if_t *cfg_if;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_IF_ADD;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	cfg_if = (emf_cfg_if_t *)req.arg;
	strncpy((char *)cfg_if->if_name, argv[4], 16);
	cfg_if->if_name[15] = 0;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_iface_del(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_if_t *cfg_if;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_IF_DEL;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	cfg_if = (emf_cfg_if_t *)req.arg;
	strncpy((char *)cfg_if->if_name, argv[4], 16);
	cfg_if->if_name[15] = 0;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_iface_list(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_if_list_t *list;
	int32 i;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_IF_LIST;
	req.oper_type = EMFCFG_OPER_TYPE_GET;
	req.size = sizeof(req.arg);

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	fprintf(stdout, "EMF enabled interfaces on %s: ", argv[3]);

	list = (emf_cfg_if_list_t *)req.arg;
	for (i = 0; i < list->num_entries; i++)
	{
		fprintf(stdout, "%s ", list->if_entry[i].if_name);
	}

	fprintf(stdout, "\n");

	return (SUCCESS);
}

static int
emf_cfg_uffp_add(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_uffp_t *cfg_uffp;
	emf_cfg_rtport_t *cfg_rtport;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);

	if (strncmp(argv[2], "uffp", 4) == 0)
	{
		req.command_id = EMFCFG_CMD_UFFP_ADD;
		cfg_uffp = (emf_cfg_uffp_t *)req.arg;
		strncpy((char *)cfg_uffp->if_name, argv[4], 16);
		cfg_uffp->if_name[15] = 0;
	}
	else
	{
		req.command_id = EMFCFG_CMD_RTPORT_ADD;
		cfg_rtport = (emf_cfg_rtport_t *)req.arg;
		strncpy((char *)cfg_rtport->if_name, argv[4], 16);
		cfg_rtport->if_name[15] = 0;
	}

	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_uffp_del(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_uffp_t *cfg_uffp;
	emf_cfg_rtport_t *cfg_rtport;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);

	if (strncmp(argv[2], "uffp", 4) == 0)
	{
		req.command_id = EMFCFG_CMD_UFFP_DEL;
		cfg_uffp = (emf_cfg_uffp_t *)req.arg;
		strncpy((char *)cfg_uffp->if_name, argv[4], 16);
		cfg_uffp->if_name[15] = 0;
	}
	else
	{
		req.command_id = EMFCFG_CMD_RTPORT_DEL;
		cfg_rtport = (emf_cfg_rtport_t *)req.arg;
		strncpy((char *)cfg_rtport->if_name, argv[4], 16);
		cfg_rtport->if_name[15] = 0;
	}

	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_uffp_list(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_uffp_list_t *uffp_list;
	emf_cfg_rtport_list_t *rtport_list;
	int32 i;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);

	if (strncmp(argv[2], "uffp", 4) == 0)
		req.command_id = EMFCFG_CMD_UFFP_LIST;
	else
		req.command_id = EMFCFG_CMD_RTPORT_LIST;

	req.oper_type = EMFCFG_OPER_TYPE_GET;
	req.size = sizeof(req.arg);

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%s\n", req.arg);
		return (FAILURE);
	}

	if (strncmp(argv[2], "uffp", 4) == 0)
	{
		fprintf(stdout, "UFFP list of %s: ", argv[3]);

		uffp_list = (emf_cfg_uffp_list_t *)req.arg;
		for (i = 0; i < uffp_list->num_entries; i++)
		{
			fprintf(stdout, "%s ", uffp_list->uffp_entry[i].if_name);
		}
	}
	else
	{
		fprintf(stdout, "RTPORT list of %s: ", argv[3]);

		rtport_list = (emf_cfg_rtport_list_t *)req.arg;
		for (i = 0; i < rtport_list->num_entries; i++)
		{
			fprintf(stdout, "%s ", rtport_list->rtport_entry[i].if_name);
		}
	}

	fprintf(stdout, "\n");

	return (SUCCESS);
}

static int
emf_cfg_mfdb_add(char *argv[])
{
	emf_cfg_mfdb_t *mfdb_entry;
	emf_cfg_request_t req;
	struct in_addr mgrp_ip;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_MFDB_ADD;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Convert the multicast group address to binary data */
	mfdb_entry = (emf_cfg_mfdb_t *)req.arg;
	if (inet_aton(argv[4], &mgrp_ip) == 0)
	{
		fprintf(stderr, "Invalid group address\n");
		return (FAILURE);
	}

	mfdb_entry->mgrp_ip = ntohl(mgrp_ip.s_addr);
	if (strlen(argv[5]) >= 16) {
		fprintf(stderr, "if_name %s too long(must be less than 15 chars)\n", argv[5]);
		return (FAILURE);
	}
	strncpy((char *)mfdb_entry->if_name, argv[5], 16);

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%sMFDB add failed\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_mfdb_del(char *argv[])
{
	emf_cfg_mfdb_t *mfdb_entry;
	emf_cfg_request_t req;
	struct in_addr mgrp_ip;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_MFDB_DEL;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Convert the multicast group address to binary data */
	mfdb_entry = (emf_cfg_mfdb_t *)req.arg;
	if (inet_aton(argv[4], &mgrp_ip) == 0)
	{
		fprintf(stderr, "Invalid group address\n");
		return (FAILURE);
	}

	mfdb_entry->mgrp_ip = ntohl(mgrp_ip.s_addr);
	if (strlen(argv[5]) >= 16) {
		fprintf(stderr, "emf_cfg_mfdb_add: if_name %s is too long (must be <15 chars)\n",
		argv[5]);
		return (FAILURE);
	}
	else
		strncpy((char *)mfdb_entry->if_name, argv[5], 16);

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%sMFDB delete failed\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_mfdb_clear(char *argv[])
{
	emf_cfg_request_t req;

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.command_id = EMFCFG_CMD_MFDB_CLEAR;
	req.oper_type = EMFCFG_OPER_TYPE_SET;

	/* Send request to kernel */
	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "%sMFDB clear failed\n", req.arg);
		return (FAILURE);
	}

	return (SUCCESS);
}

static int
emf_cfg_mfdb_list(char *argv[])
{
	emf_cfg_request_t req;
	emf_cfg_mfdb_list_t *list;
	int32 i;

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.inst_id[sizeof(req.inst_id)-1] = '\0';

	req.command_id = EMFCFG_CMD_MFDB_LIST;
	req.oper_type = EMFCFG_OPER_TYPE_GET;
	req.size = sizeof(req.arg);
	list = (emf_cfg_mfdb_list_t *)req.arg;

	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "Unable to get the MFBD list\n");
		return (FAILURE);
	}

	fprintf(stdout, "Group           Interface      Pkts\n");

	for (i = 0; i < list->num_entries; i++)
	{
		fprintf(stdout, "%08x        ", list->mfdb_entry[i].mgrp_ip);
		fprintf(stdout, "%-15s", list->mfdb_entry[i].if_name);
		fprintf(stdout, "%d\n", list->mfdb_entry[i].pkts_fwd);
	}

	return (SUCCESS);
}

static int
emf_cfg_stats_show(char *argv[])
{
	emf_cfg_request_t req;
	emf_stats_t *emfs;

	strncpy((char *)req.inst_id, argv[3], sizeof(req.inst_id)-1);
	req.inst_id[sizeof(req.inst_id)-1] = '\0';

	req.command_id = EMFCFG_CMD_EMF_STATS;
	req.oper_type = EMFCFG_OPER_TYPE_GET;
	req.size = sizeof(emf_stats_t);
	emfs = (emf_stats_t *)req.arg;

	if (emf_cfg_request_send(&req, sizeof(emf_cfg_request_t)) < 0)
	{
		fprintf(stderr, "Unable to send request to EMF\n");
		return (FAILURE);
	}

	if (req.status != EMFCFG_STATUS_SUCCESS)
	{
		fprintf(stderr, "Unable to get the EMF stats\n");
		return (FAILURE);
	}

	fprintf(stdout, "McastDataPkts   McastDataFwd    McastFlooded    "
	        "McastDataSentUp McastDataDropped\n");
	fprintf(stdout, "%-15d %-15d %-15d %-15d %d\n",
	        emfs->mcast_data_frames, emfs->mcast_data_fwd,
	        emfs->mcast_data_flooded, emfs->mcast_data_sentup,
	        emfs->mcast_data_dropped);
	fprintf(stdout, "IgmpPkts        IgmpPktsFwd     "
	        "IgmpPktsSentUp  MFDBCacheHits   MFDBCacheMisses\n");
	fprintf(stdout, "%-15d %-15d %-15d %-15d %d\n",
	        emfs->igmp_frames, emfs->igmp_frames_fwd,
	        emfs->igmp_frames_sentup, emfs->mfdb_cache_hits,
	        emfs->mfdb_cache_misses);

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
		emf_usage(stdout);
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
		emf_usage(stdout);
		return (SUCCESS);
	}

	/* Call the command processing function. This function parses
	 * prepare command request and sends it kernel.
	 */
	if ((ret = cmd_args[j].input(argv)) < 0)
	{
		return (FAILURE);
	}

	return (SUCCESS);
}
