/*
 * Copyright (c) 2011-2012 Quantenna Communications, Inc.
 * All rights reserved.
 */

#ifndef _QTN_GLOBAL_H
#define _QTN_GLOBAL_H

#define QTN_DEFAULT_LEGACY_RETRY_COUNT 4

#define QTN_GLOBAL_INIT_SELECT_GI_ENABLE	2
#define QTN_GLOBAL_INIT_SELECT_PPPC_ENABLE	1

#define QTN_GLOBAL_PSEL_MATRIX_ENABLE		1
#define QTN_GLOBAL_INIT_DEF_MATRIX		1

#define QTN_AC_BE_INHERIT_VO_NO_STA		4
#define QTN_AC_BE_INHERIT_VI_NO_STA		3
#define QTN_AC_BE_INHERIT_VO			2
#define QTN_AC_BE_INHERIT_VI			1
#define QTN_AC_BE_INHERIT_DISABLE		0
#define QTN_AC_BE_INHERIT_Q2Q_ENABLE		1
#define QTN_AC_BE_INHERIT_Q2Q_DISABLE		0
#define QTN_TXBF_IOT_DISABLE                    0
#define QTN_TXBF_IOT_ENABLE                     1
#define QTN_GLOBAL_MU_ENABLE			1
#define QTN_GLOBAL_MU_DISABLE			0
#define QTN_GLOBAL_MU_INITIAL_STATE		QTN_GLOBAL_MU_DISABLE

#define QTN_AUTO_CS_ENABLE			1
#define QTN_AUTO_CS_DISABLE			0

/*
 * The MuC TX Queuing algorithm is selected by setting
 * g_tx_queuing_alg. Values are:
 * 0 = Round robin
 * 1 = Equal airtime
 * 2 = Greedy best
 * 3 = Round robin (filling to make a power of 4)
 * x >= 4: algorithm chosen by (val % 4), with airtime
 * debugs enabled, printed every (val) seconds.
 */
#define QTN_TX_QUEUING_ALG_ROUND_ROBIN		0
#define QTN_TX_QUEUING_ALG_EQUAL_AIRTIME	1
#define QTN_TX_QUEUING_ALG_GREEDY_BEST		2
#define QTN_TX_QUEUING_ALGS			4
#if QTN_TX_QUEUING_ALGS & (QTN_TX_QUEUING_ALGS - 1)
	#error QTN_TX_QUEUING_ALGS should be a power of 2
#endif
#define QTN_GLOBAL_INIT_TX_QUEUING_ALG		QTN_TX_QUEUING_ALG_ROUND_ROBIN

#define QTN_TX_AIRTIME_XMIT_BUMP_USECS		100

#define QTN_TX_BUF_RETURN_MIN			100
/* must be greater than the above to prevent stalling */
#define QDRV_TX_LOW_RATE_TOKENS_MAX		QTN_TX_BUF_RETURN_MIN + 28

#define QTN_GLOBAL_RATE_NSS_MAX		4
#define QTN_2X2_GLOBAL_RATE_NSS_MAX	2
#define QTN_3X3_GLOBAL_RATE_NSS_MAX	3
#define QTN_RX_REORDER_BUF_TIMEOUT_US		200000
#define QTN_RX_REORDER_BUF_TIMEOUT_US_VI	800000
#define QTN_PROBE_RES_MAX_RETRY_COUNT	4
#define QTN_TX_SWRETRY_AGG_MAX		8	/* high value for zero PER */
#define QTN_TX_SWRETRY_NOAGG_MAX	1	/* add tx restrict check if this is increased */
#define QTN_TX_SWRETRY_SUSPEND_XMIT	4	/* sw retry when the sending frames are suspended */
#define QTN_TX_MSDU_EXPIRY		0	/* allow MSDUs to time out? */
#define QTN_TX_AGGREGATION		1	/* allow aggregation? */
#define QTN_CALSTATE_CALIB		1
#define QTN_CALSTATE_PROD		3
#define QTN_CALSTATE_DEFAULT		QTN_CALSTATE_PROD
#define QTN_CALSTATE_IS_PROD()		(likely(g_qtn_params.g_calstate == QTN_CALSTATE_PROD))
#define QTN_CALSTATE_VPD_LOG		0
#define QTN_CALSTATE_VPD_LINEAR		1
#define QTN_CALSTATE_MIN_TX_POWER	7
#define QTN_CALSTATE_MAX_TX_POWER	23
#define QTN_EMI_POWER_SWITCH_ENABLE	1

#define QTN_TX_AMSDU_DISABLED		0
#define QTN_TX_AMSDU_ADAPTIVE		1
#define QTN_TX_AMSDU_FIXED		0xff

#define QTN_SEL_PPPC_STEP_DEF		1
#define QTN_SEL_PPPC_MAX_STEPS		4

#define QTN_TX_AUC_DEFAULT		1

#define QTN_INST_1SS_DEF_MAT_THRESH_DEFAULT	2	/* dbm */

#define QTN_FLAG_ACT_FRAME_RTS_CTS		0x00000001
#define QTN_FLAG_ACT_FRAME_NO_LDPC		0x00000002
#define QTN_FLAG_MCS_UEQM_DISABLE		0x00000004
#define QTN_FLAG_AUC_TX				0x00000008
#define QTN_FLAG_RA_BW_SWITCHING_ENABLE_11N	0x00000010
#define QTN_FLAG_RA_BW_SWITCHING_ENABLE_11AC	0x00000020
#define QTN_GLOBAL_MUC_FLAGS_DEFAULT		QTN_FLAG_RA_BW_SWITCHING_ENABLE_11N | \
						QTN_FLAG_RA_BW_SWITCHING_ENABLE_11AC
#define QTN_NDPA_IN_HT_VHT_FORMAT	0
#define QTN_NDPA_IN_LEGACY_FORMAT	1

#define QTN_DBG_MODE_SEND_PWR_MGT		0x00000001
#define QTN_DBG_MODE_ACCEPT_PWR_MGT		0x00000002
#define QTN_DBG_MODE_TX_PKT_LOSS		0x00000004
#define QTN_DBG_MODE_DELBA_ON_TX_PKT_LOSS	0x00000008
#define QTN_DBG_MODE_CCA_FORCE			0x00000010
#define QTN_DBG_MODE_INJECT_INV_NDP		0x00000020

#define QTN_DBG_FD_CHECK_PERIODIC	0x00000001
#define QTN_DBG_FD_DUMP_OLD		0x00000002
#define QTN_DBG_FD_CHECK_ONESHOT	0x00000004
#define QTN_DBG_FD_DUMP_BCN_FAIL	0x00000008
#define QTN_DBG_FD_DUMP_VERBOSE	0x00000010 /* + top byte is the FD to dump */
#define QTN_DBG_DUMP_SC		0x00000020
#define QTN_DBG_DUMP_AGEQ		0x00000040
#define QTN_DBG_FD_FLAG_MASK		0x0000FFFF

#define QTN_HW_UPDATE_NDPA_DUR  0x0
#define	QTN_SU_TXBF_TX_CNT_DEF_THRSHLD 2
#define QTN_MU_TXBF_TX_CNT_DEF_THRSHLD 2

#define QTN_RX_BAR_SYNC_DISABLE	0
#define QTN_RX_BAR_SYNC_QTN	1
#define QTN_RX_BAR_SYNC_ALL	2

#if (defined(MUC_BUILD) || defined(SYSTEM_BUILD))

#define QTN_RX_GAIN_MIN_THRESHOLD		16
#define QTN_RX_GAIN_MAX_THRESHOLD		44
#define QTN_RX_GAIN_TIMER_INTV			1000 /* msecs */
/* counter for delay in RFIC6 500Mhz */
#define QTN_RFIC6_DELAY_MICRO_S			500
#define QTN_RFIC6_DELAY_MILI_S			QTN_RFIC6_DELAY_MICRO_S * 1000

#define SIZE_D1(x)	(sizeof(x)/sizeof(x[0]))
#define SIZE_D2(x)	(sizeof(x[0])/sizeof(x[0][0]))

struct qtn_gain_settings {
	uint8_t	gain_flags;
/* Enable SW workaround for short range association */
#define QTN_AUTO_PWR_ADJUST_EN		0x1
/* Hardware supports automatic RX gain */
#define QTN_SHORTRANGE_SCANCNT_HW	0x2
	uint32_t	gain_cumulative; /* Cumulative gain for all rx pkts */
	uint32_t	gain_num_pkts;	 /* Number of pkts for which cumulative gain was considered */
	uint32_t	gain_timer;
	uint32_t	gain_min_thresh;
	uint32_t	gain_max_thresh;
	uint32_t	gain_timer_intv;
	uint32_t	gain_low_txpow;
	int		ext_lna_gain;
	int		ext_lna_bypass_gain;
};

#define QTN_SCS_MAX_OC_STATS	32
/* off channel params */
struct qtn_scs_oc_stats {
	uint32_t	oc_pri_chan;
	uint32_t	oc_bw_sel;
	uint32_t	oc_crc_cnt;
	uint32_t	oc_lp_cnt;
	uint32_t	oc_sp_cnt;
	uint32_t	oc_cca_pri;
	uint32_t	oc_cca_sec;
	uint32_t	oc_cca_sec40;
	uint32_t	oc_cca_busy;
	uint32_t	oc_cca_smpl;
	uint32_t	oc_cca_try;
	uint32_t	oc_bcn_recvd;
};

struct qtn_cca_counts {
	uint32_t	cca_pri_cnt;
	uint32_t	cca_sec_cnt;
	uint32_t	cca_sec40_cnt;
	uint32_t	cca_busy_cnt;
	uint32_t	cca_sample_cnt;
	uint32_t	cca_try_cnt;
	uint32_t	cca_csw_cnt;
	uint32_t	cca_off_pri_cnt;
	uint32_t	cca_off_sec_cnt;
	uint32_t	cca_off_sec40_cnt;
	uint32_t	cca_off_busy_cnt;
	uint32_t	cca_off_sample_cnt;
	uint32_t	cca_off_res_cnt;
	uint32_t	cca_off_try_cnt;
	uint32_t	cca_meas_cnt;
};

struct qtn_cca_stats {
	uint32_t	cca_sample_period;
	uint32_t	cca_pri_cnt;
	uint32_t	cca_sec_cnt;
	uint32_t	cca_sec40_cnt;
	uint32_t	cca_sample_cnt;
	uint32_t	cca_try_cnt;
	uint32_t	cca_csw_cnt;
	uint32_t	cca_off_try_cnt;
	uint32_t	cca_meas_cnt;
	uint32_t	cca_busy_cnt;
	uint32_t	cca_idle_cnt;
	uint32_t	cca_tx_cnt;
	uint32_t	rx_time_cnt;
	uint32_t	tx_time_cnt;
	uint32_t	cca_pri;
	uint32_t	cca_sec;
	uint32_t	cca_sec40;
	uint32_t	cca_busy;
	uint32_t	cca_fat;
	uint32_t	cca_intf;
	uint32_t	cca_trfc;
};

struct qtn_scs_params {
	uint32_t	cca_pri_cnt;
	uint32_t	cca_sec_cnt;
	uint32_t	cca_sec40_cnt;
	uint32_t	cca_busy_cnt;
	uint32_t	cca_sample_cnt;
	uint32_t	cca_try_cnt;
	uint32_t	cca_csw_cnt;
	uint32_t	cca_off_res_cnt;
	uint32_t	cca_off_try_cnt;
	uint32_t	cca_meas_cnt;
	uint32_t	tx_usecs;
	uint32_t	rx_usecs;
	uint32_t	bcn_recvd;
	uint32_t	oc_stats_index;
	struct qtn_scs_oc_stats oc_stats[QTN_SCS_MAX_OC_STATS];
};

struct qtn_vsp_params {
	uint32_t	vsp_flags;
#define QTN_VSP_VSP_EN		0x01
#define QTN_VSP_FAT_DEBUG	0x02
#define QTN_VSP_NDCST_DEBUG	0x04
#define QTN_VSP_INTF_DEBUG	0x08
#define QTN_VSP_RTS_CTS_IN_USE	0x10
	uint32_t	timer_intv;
	uint32_t	check_secs;
	uint32_t	check_scale;
	uint32_t	fat_last;
	uint32_t	intf_ms;
	uint32_t	cca_pri_cnt;
	uint32_t	cca_sec_cnt;
	uint32_t	cca_sec40_cnt;
	uint32_t	cca_busy_cnt;
	uint32_t	cca_sample_cnt;
	uint32_t	cca_try_cnt;
	uint32_t	cca_csw_cnt;
	uint32_t	cca_off_res_cnt;
	uint32_t	cca_off_try_cnt;
	uint32_t	cca_meas_cnt;
	uint32_t	tot_tx_ms;
};


#define QTN_AUTO_CCA_SHORT_PREAMBLE_THRESHOLD	15000	/* If higher than this value, increase the CCA threshold */
#define QTN_AUTO_CCA_INTF_THRESHOLD		250	/* If higher than this value, increase the CCA threshold */

#define QTN_AUTO_CCA_THRESHOLD_MAX		0x10000	/* The max cca threshold we can set */


struct qtn_auto_cca_params {
#define QTN_AUTO_CCA_FLAGS_DISABLE			0x0
#define QTN_AUTO_CCA_FLAGS_ENABLE			0x1
#define QTN_AUTO_CCA_FLAGS_DEBUG			0x2
#define QTN_AUTO_CCA_FLAGS_SAMPLE_ONLY			0x4
	uint32_t	flags;

	uint32_t	spre_threshold;
	uint32_t	cca_intf_threshold;
	uint32_t	cca_threshold_max;
};

struct qtn_wowlan_params {
	uint16_t	host_state;
	uint16_t	wowlan_match;
	uint16_t	l2_ether_type;
	uint16_t	l3_udp_port;
};

#define QTN_AUTO_CCA_PARARMS_DEFAULT		\
	{ QTN_AUTO_CCA_FLAGS_ENABLE, QTN_AUTO_CCA_SHORT_PREAMBLE_THRESHOLD, \
		QTN_AUTO_CCA_INTF_THRESHOLD, QTN_AUTO_CCA_THRESHOLD_MAX}

struct qtn_global_param {
	uint32_t	g_legacy_retry_count;
	uint32_t	g_dbg_check_flags;
	uint32_t	g_dbg_stop_flags;
	uint32_t	g_dbg_mode_flags;
	uint8_t		g_select_gi_enable;
	uint8_t		g_select_pppc_enable;
	uint8_t		g_rate_ht_nss_max;
	uint8_t		g_rate_vht_nss_max;
	uint32_t	g_rx_agg_timeout;
	uint32_t	g_muc_flags;
	uint32_t	g_probe_res_retries;
	struct qtn_scs_params scs_params;
	struct qtn_vsp_params vsp_params;
	uint8_t		g_slow_eth_war;
	uint8_t		g_tx_swretry_agg_max;
	uint8_t		g_tx_swretry_noagg_max;
	uint8_t		g_tx_swretry_suspend_xmit;
	uint8_t		g_tx_msdu_expiry;
	uint8_t		g_tx_aggregation;
	uint32_t	g_iot_tweaks;
	uint8_t		g_calstate;
	uint8_t		g_psel_mat_enable;
	uint32_t	g_ack_policy;
	uint32_t	g_dbg_fd_flags;
	uint32_t	g_qtn_disassoc_fd_threshold;
	uint32_t	g_qtn_qn_fd_threshold;
	int32_t         g_2_tx_chains_mimo_mode;
	uint8_t		g_calstate_tx_power;
	uint8_t		g_min_tx_power;
	uint8_t		g_max_tx_power;
	uint8_t		g_emi_power_switch_enable;
	uint8_t		g_dyn_agg_timeout;
	int32_t		g_sifs_mode;
	uint8_t		g_tx_amsdu;
	uint32_t	g_ralg_dbg_aid;
	uint8_t		g_select_pppc_step_option;
	uint8_t         g_11g_erp;
	uint8_t		g_single_agg_queuing;
	uint8_t		g_def_matrix;
	uint32_t	g_tx_restrict;
	uint32_t	g_tx_restrict_fd_limit;
	uint32_t	g_tx_restrict_rate;	/* Max packets per second in Tx restrict mode */
	uint32_t	g_tx_restrict_attempts;
	uint32_t        g_rts_threshold;        /* RTS threshold */
	uint8_t		g_tx_queuing_alg;
	uint8_t         g_1bit_enable;          /* enable/disable 1bit */
	uint32_t        g_carrier_id;		/* Get/Set carrier ID */
	uint8_t		g_rx_accelerate;
	uint8_t		g_rx_accel_lu_sa;
	uint8_t		g_tx_auc;
	uint16_t	g_tx_maxmpdu;
	uint8_t		g_tx_ac_inheritance;	/* promote AC_BE traffic to vo/vi */
	uint8_t         g_txbf_iot;             /* turn on/off TxBF IOT with non QTN node */
	uint8_t		g_tx_ac_q2q_inheritance;/* promote AC_BE traffic to vo/vi */
	uint8_t		g_tx_1ss_amsdu_supp;	/* enable-disable 1ss AMSDU support - Non-qtn clients */
	uint32_t        g_vht_ndpa_dur;         /* manual update VHT NDPA duration, if it is 0, then HW auto update */
	uint32_t        g_su_txbf_pkt_cnt;      /* Tx operation count threshold to a SU TxBF station */
	uint32_t        g_mu_txbf_pkt_cnt;      /* Tx operation count threshold to a MU TxBF station */
	struct qtn_auto_cca_params	g_auto_cca_params;
	struct qtn_wowlan_params wowlan_params;
	uint8_t		g_rx_optim;
	uint8_t		g_airfair;
	uint8_t		g_cca_fixed;
	uint8_t		g_ndpa_legacy_format;	/* Configure HT-VHT / Legacy frame format for NDP announcements */
	uint8_t		g_inst_1ss_def_mat_en;		/* enable default 1ss matrix feature */
	uint8_t		g_inst_1ss_def_mat_thresh;	/* the threshold to install defalut 1ss matrix */
	uint32_t        g_mu_enable;            /* enable/disable MU Tx */
	uint8_t		g_l2_ext_filter;	/* L2 external filter */
	uint8_t		g_l2_ext_filter_port;	/* L2 external filter port */
	uint8_t		g_rate_train_dbg;
	uint8_t		g_rx_optim_pkt_stats;
	uint8_t		g_mrc_enable;
	uint32_t	g_auto_cs_enable;	/* enable/disable auto cs threshold */
	uint8_t		g_beaconing_scheme;
	uint32_t	g_muc_sys_dbg;
	uint32_t	g_rx_bar_sync;		/* sync rx reorder window on receiving BAR */
	char		*g_last_field;		/* Add all new fields before this one */
};

/* Please keep this structure in sync with qtn_global_param */
#define G_PARAMS_INIT	{			\
	QTN_DEFAULT_LEGACY_RETRY_COUNT,		\
	0,					\
	0,					\
	0,					\
	QTN_GLOBAL_INIT_SELECT_GI_ENABLE,	\
	QTN_GLOBAL_INIT_SELECT_PPPC_ENABLE,	\
	QTN_GLOBAL_RATE_NSS_MAX,		\
	QTN_GLOBAL_RATE_NSS_MAX,		\
	QTN_RX_REORDER_BUF_TIMEOUT_US,		\
	QTN_GLOBAL_MUC_FLAGS_DEFAULT,		\
	QTN_PROBE_RES_MAX_RETRY_COUNT,		\
	{0,},					\
	{0,},					\
	0,					\
	QTN_TX_SWRETRY_AGG_MAX,			\
	QTN_TX_SWRETRY_NOAGG_MAX,		\
	QTN_TX_SWRETRY_SUSPEND_XMIT,		\
	QTN_TX_MSDU_EXPIRY,			\
	QTN_TX_AGGREGATION,			\
	QTN_IOT_DEFAULT_TWEAK,			\
	QTN_CALSTATE_DEFAULT,			\
	QTN_GLOBAL_PSEL_MATRIX_ENABLE,		\
	1,					\
	0,					\
	50,					\
	64,					\
	1,					\
	QTN_CALSTATE_VPD_LOG,			\
	QTN_CALSTATE_MIN_TX_POWER,		\
	QTN_CALSTATE_MAX_TX_POWER,		\
	QTN_EMI_POWER_SWITCH_ENABLE,		\
	0,					\
	2,					\
	QTN_TX_AMSDU_ADAPTIVE,			\
	0,					\
	QTN_SEL_PPPC_STEP_DEF,			\
	0,					\
	0,					\
	QTN_GLOBAL_INIT_DEF_MATRIX,		\
	1,					\
	IEEE80211_NODE_TX_RESTRICT_LIMIT,	\
	IEEE80211_TX_RESTRICT_RATE,		\
	IEEE80211_NODE_TX_RESTRICT_RETRY,	\
	IEEE80211_RTS_THRESH_OFF,		\
	QTN_GLOBAL_INIT_TX_QUEUING_ALG,		\
	1,					\
	0,					\
	1,					\
	1,					\
	QTN_TX_AUC_DEFAULT,			\
	IEEE80211_VHTCAP_MAX_MPDU_11454,	\
	QTN_AC_BE_INHERIT_VO,			\
	QTN_TXBF_IOT_ENABLE,			\
	QTN_AC_BE_INHERIT_Q2Q_ENABLE,		\
	QTN_TX_AMSDU_DISABLED,			\
	QTN_HW_UPDATE_NDPA_DUR,			\
	QTN_SU_TXBF_TX_CNT_DEF_THRSHLD,	        \
	QTN_MU_TXBF_TX_CNT_DEF_THRSHLD,	        \
	QTN_AUTO_CCA_PARARMS_DEFAULT,		\
	{0, 0, 0x0842, 0xffff},			\
	0,					\
	QTN_AUC_AIRFAIR_DFT,			\
	0,					\
	QTN_NDPA_IN_LEGACY_FORMAT,		\
	1,					\
	QTN_INST_1SS_DEF_MAT_THRESH_DEFAULT,	\
	QTN_GLOBAL_MU_INITIAL_STATE,		\
	0,					\
	TOPAZ_TQE_EMAC_0_PORT,			\
	0,					\
	0,					\
	1,					\
	QTN_AUTO_CS_ENABLE,			\
	0,					\
	0,					\
	QTN_RX_BAR_SYNC_QTN,			\
	"end"					\
}

extern struct qtn_global_param g_qtn_params;
extern volatile __uncached__ struct qtn_gain_settings g_gain;
extern struct qtn_cca_counts g_cca_counts;
extern struct qtn_cca_stats g_qtn_cca_stats;
extern uint32_t g_qtn_rxtime_usecs;
extern uint32_t g_qtn_txtime_usecs;
extern uint32_t g_rf_xmit_status;
extern int vlan_enabled_bus;

#endif	/* defined(MUC_BUILD) */

/*
 * SKBs on the power save queue are tagged with an age and timed out.  We reuse the
 * hardware checksum field in the mbuf packet header to store this data.
 */
#define skb_age csum_offset

#define M_AGE_SET(skb,v)	(skb->skb_age = v)
#define M_AGE_GET(skb)		(skb->skb_age)
#define M_AGE_SUB(skb,adj)	(skb->skb_age -= adj)

#define QTN_2G_FIRST_OPERATING_CHAN 1
#define QTN_2G_LAST_OPERATING_CHAN  14
#define QTN_5G_FIRST_OPERATING_CHAN 36
#define QTN_5G_LAST_UNII1_OPERATING_CHAN 48
#define QTN_5G_LAST_UNII2_OPERATING_CHAN 140
#define QTN_5G_LAST_OPERATING_CHAN  169

/* RFIC chip ID */
#define RFIC5_EAGLE_PROJ_ID (2)
#define RFIC6_PROJ_ID (3)

/* MU-MIMO WMAC index */
enum {
	WMAC_ID_0	= 0,
	WMAC_ID_1	= 1,
	WMAC_ID_MAX
};

#endif	/* _QTN_GLOBAL_MUC_H */

