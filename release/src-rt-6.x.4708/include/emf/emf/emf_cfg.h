/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emf_cfg.h 390526 2013-03-12 15:53:29Z $
 */

#ifndef _EMF_CFG_H_
#define _EMF_CFG_H_

#define SUCCESS                     0
#define FAILURE                     -1

#define EMFCFG_MAX_ARG_SIZE         1024

#define EMFCFG_CMD_EMF_ENABLE       1
#define EMFCFG_CMD_MFDB_ADD         2
#define EMFCFG_CMD_MFDB_DEL         3
#define EMFCFG_CMD_MFDB_CLEAR       4
#define EMFCFG_CMD_MFDB_LIST        5
#define EMFCFG_CMD_BR_ADD           6
#define EMFCFG_CMD_BR_DEL           7
#define EMFCFG_CMD_BR_LIST          8
#define EMFCFG_CMD_IF_ADD           9
#define EMFCFG_CMD_IF_DEL           10
#define EMFCFG_CMD_IF_LIST          11
#define EMFCFG_CMD_UFFP_ADD         12
#define EMFCFG_CMD_UFFP_DEL         13
#define EMFCFG_CMD_UFFP_LIST        14
#define EMFCFG_CMD_RTPORT_ADD       15
#define EMFCFG_CMD_RTPORT_DEL       16
#define EMFCFG_CMD_RTPORT_LIST      17
#define EMFCFG_CMD_EMF_STATS        18
#define EMFCFG_CMD_MC_DATA_IND      19

#define EMFCFG_OPER_TYPE_GET        1
#define EMFCFG_OPER_TYPE_SET        2

#define EMFCFG_STATUS_SUCCESS       1
#define EMFCFG_STATUS_FAILURE       2
#define EMFCFG_STATUS_CMD_UNKNOWN   3
#define EMFCFG_STATUS_OPER_UNKNOWN  4
#define EMFCFG_STATUS_INVALID_IF    5

typedef struct emf_cfg_request
{
	uint8   inst_id[16];            /* Bridge name as instance identifier */
	uint32  command_id;             /* Command identifier */
	uint32  oper_type;              /* Operation type: GET, SET */
	uint32  status;                 /* Command status */
	uint32  size;                   /* Size of the argument */
	                                /* Command arguments */
	uint8   arg[EMFCFG_MAX_ARG_SIZE];
} emf_cfg_request_t;

typedef struct emf_cfg_mfdb
{
	uint32  mgrp_ip;                /* Multicast Group address */
	uint8   if_name[16];            /* Interface on which members are present */
	void    *if_ptr;                /* Interface pointer (kernel only) */
} emf_cfg_mfdb_t;

typedef struct emf_cfg_mfdb_list
{
	uint32  num_entries;            /* Number of entries in MFDB */
	struct mfdb_entry
	{
		uint32  mgrp_ip;        /* Multicast Group address */
		uint8   if_name[16];    /* Interface name */
		uint32  pkts_fwd;       /* Number of packets forwarded */
		void    *if_ptr;        /* Interface pointer (kernel only) */
	} mfdb_entry[0];
} emf_cfg_mfdb_list_t;

typedef struct emf_cfg_if
{
	uint8   if_name[16];            /* Name of the interface */
} emf_cfg_if_t;

typedef struct emf_cfg_if_list
{
	uint32        num_entries;      /* Number of entries in EMF if list */
	emf_cfg_if_t  if_entry[0];      /* Interface entry data */
} emf_cfg_if_list_t;

typedef struct emf_cfg_uffp
{
	uint8   if_name[16];            /* Name of the interface */
	void    *if_ptr;                /* Interface pointer (kernel only) */
} emf_cfg_uffp_t;

typedef struct emf_cfg_uffp_list
{
	uint32          num_entries;    /* Number of entries in UFFP list */
	emf_cfg_uffp_t  uffp_entry[0];  /* Interface entry data */
} emf_cfg_uffp_list_t;

typedef struct emf_cfg_rtport
{
	uint8   if_name[16];            /* Name of the interface */
	void    *if_ptr;                /* Interface pointer (kernel only) */
} emf_cfg_rtport_t;

typedef struct emf_cfg_rtport_list
{
	uint32           num_entries;     /* Number of entries in UFFP list */
	emf_cfg_rtport_t rtport_entry[0]; /* Interface entry data */
} emf_cfg_rtport_list_t;

/*
 * Multicast Forwarding Layer Statistics
 */
typedef struct emf_stats
{
	uint32	igmp_frames;            /* IGMP frames received */
	uint32	igmp_frames_fwd;        /* IGMP membership quries received */
	uint32	igmp_frames_sentup;     /* IGMP membership reports seen */
	uint32	igmp_frames_dropped;    /* IGMP frames dropped */
	uint32	mcast_data_frames;      /* Multicast data frames received */
	uint32	mcast_data_fwd;         /* Multicast data frames forwarded */
	uint32	mcast_data_flooded;     /* Multicast data frames flooded */
	uint32	mcast_data_sentup;      /* Multicast data frames sent up */
	uint32	mcast_data_dropped;     /* Multicast data frames dropped */
	uint32	mfdb_cache_hits;        /* MFDB cache hits */
	uint32	mfdb_cache_misses;      /* MFDB cache misses */
} emf_stats_t;

extern void emf_cfg_request_process(emf_cfg_request_t *cfg);

#endif /* _EMF_CFG_H_ */
