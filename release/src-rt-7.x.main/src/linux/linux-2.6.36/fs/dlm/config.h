/******************************************************************************
*******************************************************************************
**
**  Copyright (C) Sistina Software, Inc.  1997-2003  All rights reserved.
**  Copyright (C) 2004-2007 Red Hat, Inc.  All rights reserved.
**
**  This copyrighted material is made available to anyone wishing to use,
**  modify, copy, or redistribute it subject to the terms and conditions
**  of the GNU General Public License v.2.
**
*******************************************************************************
******************************************************************************/

#ifndef __CONFIG_DOT_H__
#define __CONFIG_DOT_H__

#define DLM_MAX_ADDR_COUNT 3

struct dlm_config_info {
	int ci_tcp_port;
	int ci_buffer_size;
	int ci_rsbtbl_size;
	int ci_lkbtbl_size;
	int ci_dirtbl_size;
	int ci_recover_timer;
	int ci_toss_secs;
	int ci_scan_secs;
	int ci_log_debug;
	int ci_protocol;
	int ci_timewarn_cs;
};

extern struct dlm_config_info dlm_config;

int dlm_config_init(void);
void dlm_config_exit(void);
int dlm_node_weight(char *lsname, int nodeid);
int dlm_nodeid_list(char *lsname, int **ids_out, int *ids_count_out,
		    int **new_out, int *new_count_out);
int dlm_nodeid_to_addr(int nodeid, struct sockaddr_storage *addr);
int dlm_addr_to_nodeid(struct sockaddr_storage *addr, int *nodeid);
int dlm_our_nodeid(void);
int dlm_our_addr(struct sockaddr_storage *addr, int num);

#endif				/* __CONFIG_DOT_H__ */
