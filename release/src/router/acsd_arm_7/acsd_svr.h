/*
 * ACSD server include file
 *
 * Copyright (C) 2014, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_svr.h 452750 2014-01-31 23:12:13Z $
 */

#ifndef _acsd_srv_h_
#define _acsd_srv_h_

#include "acsd.h"

#define ACSD_OK	0
#define ACSD_FAIL -1
#define ACSD_ERR_NO_FRM 		-2
#define ACSD_ERR_NOT_ACTVS		-3
#define ACSD_ERR_NOT_DCSREQ		-4

#define ACSD_IFNAME_SIZE		16
#define ACSD_MAX_INTERFACES		2
#define ACS_MAX_IF_NUM ACSD_MAX_INTERFACES

#define ACSD_DFLT_POLL_INTERVAL 1  /* default polling interval */

#define DCS_CSA_COUNT		20
#define FCS_CSA_COUNT		2

/* acs config flags */
#define ACS_FLAGS_INTF_THRES_CCA	0x1
#define ACS_FLAGS_INTF_THRES_BGN	0x2
#define ACS_FLAGS_NOBIAS_11B		0x4
#define ACS_FLAGS_LASTUSED_CHK		0x8  /* check connectivity for cs scan */
#define ACS_FLAGS_CI_SCAN			0x10 /* run ci scan constantly */
#define ACS_FLAGS_FAST_DCS          0x20 /* fast channel decision based on updated ci */
#define ACS_FLAGS_SCAN_TIMER_OFF	0x40 /* do not check scan timer */

#define CI_SCAN(c_info) ((c_info)->flags & ACS_FLAGS_CI_SCAN)
#define SCAN_TIMER_ON(c_info) (((c_info)->flags & ACS_FLAGS_SCAN_TIMER_OFF) == 0)
#define ACS_FCS_MODE(c_info) ((c_info)->acs_fcs_mode)

#define ACS_NOT_ALIGN_WT	2

#define ACS_MIN_SCORE NBITVAL(31)
#define ACS_BGNOISE_BASE	-95

#define ACS_BW_20	0
#define ACS_BW_40	1
#define ACS_BW_80	2
#define ACS_BW_MAX	3

#define ACS_BSS_TYPE_11G	1
#define ACS_BSS_TYPE_11B	2
#define ACS_BSS_TYPE_11A	3
#define ACS_BSS_TYPE_11N	8

/* scan parameter */
#define ACS_CS_SCAN_DWELL	250 /* ms */
#define ACS_CI_SCAN_DWELL	10  /* ms */
#define ACS_CI_SCAN_WINDOW	5   /* sec: how often for ci scan */
#define ACS_CS_SCAN_TIMER_MIN	60  /* sec */
#define ACS_DFLT_CS_SCAN_TIMER	900  /* sec */
#define ACS_DFLT_CI_SCAN_TIMER  5 /* sec */
#define ACS_CS_SCAN_MIN_RSSI -80 /* dBm */
#define ACS_CI_SCAN_MIN_RSSI -80 /* dBm */
#define ACS_CI_SCAN_EXPIRE	300  /* sec: how long to expire an scan entry */

/* scan channel flags */
#define ACS_CI_SCAN_CHAN_PREF	0x01  /* chan preffered flag */
#define ACS_CI_SCAN_CHAN_EXCL	0x02  /* chan exclude flag */

/* Scan running status */
#define ACS_CI_SCAN_RUNNING_PREF	0x01
#define ACS_CI_SCAN_RUNNING_NORM	0x02

#define ACS_NUMCHANNELS	64

/* mode define */
#define ACS_MODE_DISABLE	0
#define ACS_MODE_MONITOR	1
#define ACS_MODE_SELECT		2
#define ACS_MODE_COEXCHECK	3
#define ACS_MODE_11H		4
#define AUTOCHANNEL(c_info) ((c_info)->mode == ACS_MODE_SELECT)
#define COEXCHECK(c_info)	((c_info)->mode == ACS_MODE_COEXCHECK)
#define ACS11H(c_info)		((c_info)->mode == ACS_MODE_11H)

#define ACS_STATUS_POLL	5

/* Predefined policy indices. These index into the table of predefined policies, with the
 * exception of ACS_POLICY_USER which is defined through the "acs_pol" nvram variable. */
typedef enum {
	ACS_POLICY_DEFAULT	= 0,
	ACS_POLICY_LEGACY	= 1,
	ACS_POLICY_INTF		= 2,
	ACS_POLICY_INTF_BUSY	= 3,
	ACS_POLICY_OPTIMIZED	= 4,
	ACS_POLICY_CUSTOMIZED1	= 5,
	ACS_POLICY_CUSTOMIZED2	= 6,
	ACS_POLICY_FCS		= 7,
	ACS_POLICY_USER		= 8,
	ACS_POLICY_MAX		= ACS_POLICY_USER
} acs_policy_index;

/* state defines */
#define CHANIM_STATE_DETECTING	0
#define CHANIM_STATE_DETECTED	1
#define CHANIM_STATE_ACTON		2
#define CHANIM_STATE_LOCKOUT   	3

/* Default chanim config values */
#define CHANIM_DFLT_SAMPLE_PERIOD	5
#define CHANIM_DFLT_THRESHOLD_TIME	2
#define CHANIM_DFLT_MAX_ACS			1
#define CHANIM_DFLT_LOCKOUT_PERIOD	28800 /* 8 hours */

#define CHANIM_DFLT_SCB_MAX_PROBE	20
#define CHANIM_DFLT_SCB_TIMEOUT		2
#define CHANIM_DFLT_SCB_ACTIVITY_TIME 5

#define CHANIM_TXOP_BASE			40

/* Default range values */
#define CHANIM_SAMPLE_PERIOD_MIN	1
#define CHANIM_SAMPLE_PERIOD_MAX	30

#define CHANIM_THRESHOLD_TIME_MIN	1
#define CHANIM_THRESHOLD_TIME_MAX	10

#define CHANIM_MAX_ACS_MIN			1
#define CHANIM_MAX_ACS_MAX			10

#define CHANIM_LOCKOUT_PERIOD_MIN	60
#define CHANIM_LOCKOUT_PERIOD_MAX	(uint)-1

#define CHANIM_STATS_RECORD 16
#define CHANIM_CHECK_START 5 /* when to start check chanim so the bgnoise is good */

#define CHANIM_FLAGS_RELATIVE_THRES		0x1
#define CHANIM_FLAGS_USE_CRSGLITCH		0x2

#define ACS_MAX_VECTOR_LEN			(ACS_NUMCHANNELS * 6) /* hex format */
#define ACS_MAX_LIST_LEN			ACS_NUMCHANNELS

#define ACS_TXDELAY_PERIOD			1
#define ACS_TXDELAY_CNT				1
#define ACS_TXDELAY_RATIO			30
/* acs_dfs settings: disabled, enabled, reentry */
#define ACS_DFS_DISABLED			0
#define ACS_DFS_ENABLED				1
#define ACS_DFS_REENTRY				2
#define ACS_LOWBAND_LEAST_RSSI		-70
#define ACS_NOFCS_LEAST_RSSI		-60
#define ACS_CHAN_DWELL_TIME			30
#define ACS_TX_IDLE_CNT				300		/* around 3.5Mbps */
#define ACS_CI_SCAN_TIMEOUT			300		/* 5 min */
#define ACS_SCAN_CHANIM_STATS		70
#define ACS_FCS_CHANIM_STATS		50 /* do pref ci scan if TXOP threshold */
#define ACS_FCS_MODE_DEFAULT		0
#define ACS_CHAN_FLOP_PERIOD		300 /* least time gap to dcs same chan */

#define ACS_INTFER_SAMPLE_PERIOD		1
#define ACS_INTFER_SAMPLE_COUNT			3
#define ACS_INTFER_TXFAIL_THRESH		16
#define ACS_INTFER_TCPTXFAIL_THRESH		5


#define ACS_WPS_RUNNING	(nvram_match("wps_proc_status", "1") || \
	nvram_match("wps_proc_status", "9"))

/* chanim data structure */
/* transient counters/stamps */
typedef struct {
	time_t detecttime;
	bool detected;
	uint8 state;
	uint8 wl_sample_period; /* sample time inside driver */
	uint8 stats_idx;  	/* where the next stats entry should locate */
	uint8 record_idx;	/* where the next acs record should locate */
	uint scb_max_probe; /* original number of scb probe to conduct */
	uint scb_timeout; /* the storage for the original scb timeout that is swapped */
	uint scb_activity_time; /* original activity time */
	int best_score; /* best score for channel in use */
} chanim_mark_t;

/* configurable parameters */
typedef struct {
	uint32 flags;
	uint8 sample_period;	/* in seconds, time to do a sampling measurement */
	uint8 threshold_time;	/* number of sample period to trigger an action */
	uint8 max_acs;			/* the maximum acs scans for one lockout period */
	uint32 lockout_period;	/* in seconds, time for one lockout period */
	uint scb_timeout;
	uint scb_max_probe; /* when triggered by intf, how many times to probe */
	uint scb_activity_time;
	int8 acs_trigger_var;
} chanim_config_t;

typedef struct {
	int min_val;
	int max_val;
} range_t;

typedef struct {
	range_t sample_period;
	range_t threshold_time;	/* number of sample period to trigger an action */
	range_t max_acs;			/* the maximum acs scans for one lockout period */
	range_t lockout_period;	/* in seconds, time for one lockout period */
	range_t crsglitch_thres;
	range_t ccastats_thres;
	range_t ccastats_rel_thres;
	range_t bgnoise_thres; /* background noise threshold */
	range_t bgnoise_rel_thres;
	range_t good_channel_var;
	range_t acs_trigger_var;
} chanim_range_t;

typedef struct {
	uint ticks;
	chanim_mark_t mark;
	chanim_config_t config;
	chanim_range_t range;
	chanim_stats_t base;  /* base value for the existing stats */
	chanim_stats_t stats[CHANIM_STATS_RECORD];
	chanim_acs_record_t record[CHANIM_ACS_RECORD];
} chanim_info_t;

#define chanim_mark(ch_info)	(ch_info)->mark
#define chanim_config(ch_info)	(ch_info)->config
#define chanim_range(ch_info) (ch_info)->range
#define chanim_base(ch_info) (ch_info)->base
#define chanim_act_delay(ch_info) \
	(chanim_config(ch_info).sample_period * chanim_config(ch_info).threshold_time)

typedef struct ifname_idx_map {
	char name[16];
    uint8 idx;
	bool in_use;
} ifname_idx_map_t;

typedef struct {
	uint num_cmds;  /* total incoming cmds from the client */
	uint valid_cmds; /* valid cmds */
	uint num_events; /* total event from the driver */
	uint valid_events; /* valid events */
} acsd_stats_t;

struct acs_chaninfo;
typedef chanspec_t (*acs_selector_t)(struct acs_chaninfo *c_info, int bw);
typedef struct acs_policy_s {
	int8 bgnoise_thres;
	uint8 intf_threshold;
	int acs_weight[CH_SCORE_MAX];
	acs_selector_t chan_selector;
} acs_policy_t;

/* a reduced version of wl_bss_info, keep it small, add items as needed */
typedef struct acs_bss_info_sm_s {
	struct ether_addr BSSID;
	uint8 SSID[32];
	uint8 SSID_len;
	chanspec_t chanspec;
	int16 RSSI;
	uint type;
} acs_bss_info_sm_t;

typedef struct acs_bss_info_entry_s {
	acs_bss_info_sm_t binfo_local;
	time_t timestamp;
	struct acs_bss_info_entry_s * next;
} acs_bss_info_entry_t;

typedef struct scan_chspec_elemt_s {
	chanspec_t chspec;
	uint32 chspec_info;
	uint32 flags;
} scan_chspec_elemt_t;

typedef struct acs_scan_chspec_s {
	uint8 count;
	uint8 idx;
	uint8 pref_count;	/* chan count of prefer chan list */
	uint8 excl_count;	/* chan count of exclusive chan list */
	uint8 ci_scan_running;	/* is ci_scan running */
	bool ci_pref_scan_request; /* need to start ci scan for pref chan? */
	scan_chspec_elemt_t* chspec_list;
} acs_scan_chspec_t;

typedef struct fcs_conf_chspec_s {
	uint16 count;
	chanspec_t clist[ACS_MAX_LIST_LEN];
} fcs_conf_chspec_t;

typedef struct acs_fcs_s {
	fcs_conf_chspec_t pref_chans; /* Prefer chan list */
	fcs_conf_chspec_t excl_chans; /* Exclude chan list */
	dfsr_context_t	*acs_dfsr;	/* DFS Re-Entry related per interface data */
	uint32 timestamp_acs_scan;	/* timestamp of last scan */
	uint32 timestamp_tx_idle;	/* timestamp of last tx idle check */
	uint8 acs_ci_scan_count;	/* how many channel left for current ci_scan loop */
	uint32 acs_ci_scan_timeout;	/* start ci scan if ci_scan timeout */
	uint32 acs_tx_idle_cnt;		/* no of tx frames in tx_idle_period */
	uint32 acs_txframe;			/* current txframe */

	int acs_lowband_least_rssi;	/* least rssi value for select low power chan */
	int acs_nofcs_least_rssi;	/* least rssi value to stop fcs */

	int acs_scan_chanim_stats;	/* chanim_stats to trigger ci_scan  */
	int acs_fcs_chanim_stats;	/* chanim_stats value to mark chan is busy */
	uint8 acs_chan_dwell_time;	/* least chan dwell time */
	uint16 acs_chan_flop_period;	/* least interval to reselect same chan */
	uint8 acs_dfs;		/* enable/disable DFS chan as first chan and DFS Reentry */

	/* params for txdelay trigger */
	uint8 acs_txdelay_period;	/* txdelay sample period */
	uint8 acs_txdelay_cnt;		/* txdelay sample count */
	int16 acs_txdelay_ratio;	/* txdelay jump ratio */
	wl_intfer_params_t intfparams; /* intfer configuration parametres */

	/* csa mode */
	uint8 acs_dcs_csa;
} acs_fcs_t;

/* radio setting info */
typedef struct acs_rsi {
	int band_type;
	int bw_cap;
	bool coex_enb;
	bool reg_11h;
	chanspec_t pref_chspec;
} acs_rsi_t;

/* acs main data structure */
typedef struct acs_chaninfo {
	char name[16];
	int mode;
	chanspec_t selected_chspec;
	chanspec_t cur_chspec;
	bool cur_is_dfs;
	acs_rsi_t rs_info;
	acs_scan_chspec_t scan_chspec_list;
	wl_chanim_stats_t* chanim_stats; /* chanim_stats from scan */
	ch_candidate_t * candidate[ACS_BW_MAX];
	int c_count[ACS_BW_MAX];
	chanim_info_t* chanim_info; /* chanim monitor/triggering struct */
	uint acs_cs_scan_timer;	/* cs scan timer */
	uint acs_ci_scan_timer; /* ci scan timer */
	wl_scan_results_t *scan_results; /* raw scan results */
	acs_bss_info_entry_t *acs_bss_info_q; /* up-to-date parsed scan result queue */
	acs_chan_bssinfo_t* ch_bssinfo;
	uint32 flags; /* config flags */
	uint32 acs_scan_entry_expire; /* sec: how long to expire an scan entry */
	acs_policy_index policy_index;
	acs_policy_t acs_policy; /* the current policy in use */
	uint8 acs_fcs_mode; /* 0: disable fcs mode, 1:  enable fcs mode */
	acs_fcs_t acs_fcs;
} acs_chaninfo_t;

#define ACS_DFSR_CTX(ci) ((ci)->acs_fcs.acs_dfsr)

typedef struct {
	acs_chaninfo_t* chan_info[ACS_MAX_IF_NUM];
	ifname_idx_map_t acs_ifmap[ACS_MAX_IF_NUM];
} acs_info_t;

typedef struct acsd_wksp_s {
	int version;
	char ifnames[ACSD_IFNAME_SIZE*ACSD_MAX_INTERFACES]; /* interface names */
	uint8 packet[ACSD_BUFSIZE_4K];
	fd_set fdset;
	int fdmax;
	int event_fd;
	int listen_fd; /* server listen fd */
	char* cmd_buf; /* CLI buf */
	int conn_fd; /* client connection fd */
	uint poll_interval; /* polling interval */
	uint ticks;			/* number of polling intervals */
	acsd_stats_t stats;
	acs_info_t *acs_info;
} acsd_wksp_t;

extern void acs_init_run(acs_info_t ** acs_info_p);
extern void acs_cleanup(acs_info_t ** acs_info_p);
extern int acs_do_ci_update(uint ticks, acs_chaninfo_t *c_info);
extern int acs_update_status(acs_chaninfo_t * c_info);

extern int acs_idx_from_map(char *name);
extern int acs_run_cs_scan(acs_chaninfo_t *c_info);
extern int acs_request_data(acs_chaninfo_t *c_info);
extern void acs_default_policy(acs_policy_t *a_pol, uint index);

extern void acs_set_chspec(acs_chaninfo_t * c_info);
extern void acs_select_chspec(acs_chaninfo_t *c_info);
extern int acs_scan_timer_or_dfsr_check(acs_chaninfo_t * c_info);
extern int acs_fcs_ci_scan_check(acs_chaninfo_t * c_info);
extern int acs_fcs_ci_scan_pref(acs_chaninfo_t * c_info);
extern int acs_update_driver(acs_chaninfo_t * c_info);
extern void acs_dump_scan_entry(acs_chaninfo_t *c_info);

extern int acsd_chanim_init(acs_chaninfo_t *c_info);
extern void acsd_chanim_check(uint ticks, acs_chaninfo_t *c_info);
extern int acsd_chanim_query(acs_chaninfo_t * c_info, uint32 count, uint32 ticks);
extern void chanim_upd_acs_record(chanim_info_t *ch_info, chanspec_t selected,
	uint8 trigger);
extern uint chanim_scb_lastused(acs_chaninfo_t* c_info);
extern int chanim_txop_to_noise(uint8 txop);

extern int dcs_parse_actframe(dot11_action_wifi_vendor_specific_t *actfrm,
	wl_bcmdcs_data_t *dcs_data);
extern int dcs_handle_request(char* ifname, wl_bcmdcs_data_t *dcs_data, uint8 mode,
	uint8 count, uint8 csa_mode);
extern int acsd_proc_cmd(acsd_wksp_t* d_info, char* buf, uint size,
	uint* resp_size);
extern int acs_intfer_config(acs_chaninfo_t *c_info);
#endif  /* _acsd_srv_h_ */
