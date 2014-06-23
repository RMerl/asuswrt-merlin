/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2010 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Quantenna Communications Inc                               **
**  File        : shared_params.h                                            **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH1*/

#ifndef _SHARED_DEFS_H_
#define _SHARED_DEFS_H_

/*
 * Default board type is 0 to match the default (fallback) from get_bootval.
 * Script returns 0 if the parameter is not defined.
 */
#define  QTN_RUBY_BOARD_TYPE_DEFAULT			0

#define  QTN_RUBY_BRINGUP_BOARD				0
#define  QTN_RUBY_BRINGUP_BOARD_32_320			1
#define  QTN_RUBY_BRINGUP_BOARD_16_320			2
#define  QTN_RUBY_BRINGUP_BOARD_16_160			3
#define  QTN_RUBY_BRINGUP_BOARD_ETRON			4
#define  QTN_RUBY_BRINGUP_BOARD_ETRON_320		5
#define  QTN_RUBY_BRINGUP_BOARD_ETRON_160		6
#define  QTN_RUBY_BRINGUP_BOARD_16_200			7
#define  QTN_RUBY_BRINGUP_BOARD_32_200			8
#define  QTN_RUBY_BRINGUP_BOARD_PCIE			9
/* diag board ids */
#define  QTN_RUBY_BRINGUP_BOARD_32_160_ARB		10
#define  QTN_RUBY_BRINGUP_BOARD_32_160_ARB_1		11
#define  QTN_RUBY_BRINGUP_BOARD_16_160_ARB_1		12
#define  QTN_RUBY_BRINGUP_BOARD_32_160_ARB_0		13
#define  QTN_RUBY_BRINGUP_BOARD_ETRON_160_EMAC1		14
#define  QTN_RUBY_BRINGUP_BOARD_ETRON_250_EMAC1		15
#define  QTN_RUBY_BRINGUP_BOARD_ETRON_32_320_EMAC1	16
#define  QTN_RUBY_BRINGUP_ETRON32_160			17
#define  QTN_RUBY_BRINGUP_ETRON32_320			18
#define  QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC		19
#define  QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC_MII	20
#define  QTN_RUBY_BRINGUP_BOARD_MICRON_DUALEMAC_LOOPBACK 21
#define  QTN_RUBY_BRINGUP_BOARD_16_160_DUALEMAC		22


#define  QTN_RUBY_REFERENCE_DESIGN_BOARD		1000
#define  QTN_RUBY_REFERENCE_DESIGN_BOARD_250		1001
#define  QTN_RUBY_REF_BOARD_DUAL_CON			1002
#define  QTN_RUBY_REFERENCE_DESIGN_BOARD_320		1003
#define  QTN_RUBY_ETRON_32_320_EMAC1			1004
#define  QTN_RUBY_ETRON_32_250_EMAC1			1005
#define  QTN_RUBY_REFERENCE_DESIGN_BOARD_RGMII_DLL	1006
#define  QTN_RUBY_QHS710_5S5_SIGE_DDR250		1007
#define  QTN_RUBY_QHS710_5S5_SIGE_DDR320		1008
#define  QTN_RUBY_OHS711_PCIE_320DDR			1009
/* pcie reference ids */
#define  QTN_RUBY_QHS713_5S1_PCIERC_DDR160		1170
#define  QTN_RUBY_OHS711_5S13_PCIE_DDR320		1171 /* duplicate of 1009 */
#define  QTN_RUBY_QHS713_5S1_PCIERC_DDR320		1172

#define  QTN_RUBY_ODM_BOARD_0				1200
#define  QTN_RUBY_ODM_BOARD_1				1201
#define  QTN_RUBY_ODM_BOARD_2				1202
#define  QTN_RUBY_ODM_BOARD_3				1203
#define  QTN_RUBY_ODM_BOARD_4				1204
#define  QTN_RUBY_ODM_BOARD_5				1205
#define  QTN_RUBY_ODM_BOARD_6				1206
#define  QTN_RUBY_ODM_BOARD_7				1207
#define  QTN_RUBY_ODM_BOARD_8				1208
#define  QTN_RUBY_ODM_BOARD_9				1209
#define  QTN_RUBY_ODM_BOARD_10				1210
#define  QTN_RUBY_ODM_BOARD_11				1211
#define  QTN_RUBY_ODM_BOARD_12				1212
#define  QTN_RUBY_ODM_BOARD_13				1213
#define  QTN_RUBY_ODM_BOARD_14				1214
#define  QTN_RUBY_ODM_BOARD_15				1215
#define  QTN_RUBY_ODM_BOARD_16				1216
#define  QTN_RUBY_ODM_BOARD_17				1217
#define  QTN_RUBY_ODM_BOARD_18				1218
#define  QTN_RUBY_ODM_BOARD_19				1219
#define  QTN_RUBY_ODM_BOARD_20				1220
#define  QTN_RUBY_ODM_BOARD_21				1221
#define  QTN_RUBY_ODM_BOARD_22				1222
#define  QTN_TOPAZ_FPGAA_BOARD				1223
#define  QTN_TOPAZ_FPGAB_BOARD				1224
#define  QTN_TOPAZ_DUAL_EMAC_FPGAA_BOARD		1225
#define  QTN_TOPAZ_DUAL_EMAC_FPGAB_BOARD		1226
#define  QTN_TOPAZ_RC_BOARD				1227
#define  QTN_TOPAZ_EP_BOARD				1228
#define  QTN_TOPAZ_BB_BOARD				1229
#define  QTN_TOPAZ_RF_BOARD				1230
#define  QTN_TOPAZ_QHS840_5S1				1231

#define		QTN_RUBY_AUTOCONFIG_ID				32768
#define		QTN_RUBY_UNIVERSAL_BOARD_ID			65535

#define  QTN_RUBY_NOSUCH_BOARD_TYPE			-1

#define  QTN_RUBY_BRINGUP_RWPA				0
#define  QTN_RUBY_REF_RWPA				1
#define  QTN_RUBY_SIGE					2
#define  QTN_RUBY_UNDEFINED				3
#define  QTN_RUBY_WIFI_NONE				4
#define	 QTN_TPZ_SE5003L1				5
#define	 QTN_TPZ_SE5003L1_INV				6
#define  QTN_TPZ_SKY85703				7
#define  QTN_TPZ_SKY85405_BPF840			8

#ifdef TOPAZ_PLATFORM
#define QTN_SWITCH_CHANNEL_TIME_AVG	3750	/* microseconds */
#else
#define QTN_SWITCH_CHANNEL_TIME_AVG	3500	/* microseconds */
#endif

#define IEEE80211_MAX_NAV	32767

/* SCS (ACI/CCI Detection and Mitigation) APIs */
enum qtn_vap_scs_cmds {
	IEEE80211_SCS_SET_ENABLE = 1,
	IEEE80211_SCS_SET_DEBUG_ENABLE,
	IEEE80211_SCS_SET_SAMPLE_ENABLE,
	IEEE80211_SCS_SET_SAMPLE_DWELL_TIME,
	IEEE80211_SCS_SET_SAMPLE_INTERVAL,
	IEEE80211_SCS_SET_THRSHLD_SMPL_PKTNUM,
	IEEE80211_SCS_SET_THRSHLD_PRI_CCA,
	IEEE80211_SCS_SET_THRSHLD_SEC_CCA,
	IEEE80211_SCS_SET_THRSHLD_SMPL_AIRTIME,
	IEEE80211_SCS_SET_WF_CCA,
	IEEE80211_SCS_SET_WF_RSSI,
	IEEE80211_SCS_SET_WF_CRC_ERR,
	IEEE80211_SCS_SET_WF_LPRE,
	IEEE80211_SCS_SET_WF_SPRE,
	IEEE80211_SCS_SET_WF_RETRIES,
	IEEE80211_SCS_SET_WF_DFS,
	IEEE80211_SCS_SET_WF_MAX_TX_PWR,
	IEEE80211_SCS_SET_REPORT_ONLY,
	IEEE80211_SCS_SET_CCA_INTF_RATIO,
	IEEE80211_SCS_SET_CCA_IDLE_THRSHLD,
	IEEE80211_SCS_SET_CCA_INTF_LO_THR,
	IEEE80211_SCS_SET_CCA_INTF_HI_THR,
	IEEE80211_SCS_SET_CCA_SMPL_DUR,
	IEEE80211_SCS_GET_REPORT,
	IEEE80211_SCS_GET_INTERNAL_STATS,
	IEEE80211_SCS_SET_CCA_INTF_SMTH_FCTR,
	IEEE80211_SCS_RESET_RANKING_TABLE,
	IEEE80211_SCS_SET_CHAN_MTRC_MRGN,
	IEEE80211_SCS_SET_RSSI_SMTH_FCTR,
	IEEE80211_SCS_SET_ATTEN_ADJUST,
	IEEE80211_SCS_SET_THRSHLD_ATTEN_INC,
	IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY,
	IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY_MINRATE,
	IEEE80211_SCS_SET_PMBL_ERR_SMTH_FCTR,
	IEEE80211_SCS_SET_PMBL_ERR_RANGE,
	IEEE80211_SCS_SET_PMBL_ERR_MAPPED_INTF_RANGE,
	IEEE80211_SCS_SET_THRSHLD_LOAD,
	IEEE80211_SCS_SET_PMBL_ERR_WF,
	IEEE80211_SCS_SET_THRSHLD_AGING_NOR,
	IEEE80211_SCS_SET_THRSHLD_AGING_DFSREENT,
	IEEE80211_SCS_SET_THRSHLD_DFS_REENTRY_INTF,
	IEEE80211_SCS_SET_PMP_RPT_CCA_SMTH_FCTR,
	IEEE80211_SCS_SET_PMP_RX_TIME_SMTH_FCTR,
	IEEE80211_SCS_SET_PMP_TX_TIME_SMTH_FCTR,
	IEEE80211_SCS_SET_PMP_STATS_STABLE_PERCENT,
	IEEE80211_SCS_SET_PMP_STATS_STABLE_RANGE,
	IEEE80211_SCS_SET_PMP_STATS_CLEAR_INTERVAL,
	IEEE80211_SCS_SET_PMP_TXTIME_COMPENSATION,
	IEEE80211_SCS_SET_PMP_RXTIME_COMPENSATION,
	IEEE80211_SCS_SET_SWITCH_CHANNEL_MANUALLY,
	IEEE80211_SCS_SET_AS_RX_TIME_SMTH_FCTR,
	IEEE80211_SCS_SET_AS_TX_TIME_SMTH_FCTR,
	IEEE80211_SCS_SET_MAX
};

#define IEEE80211_SCS_STATE_INIT                       0
#define IEEE80211_SCS_STATE_RESET                      1
#define IEEE80211_SCS_STATE_MEASUREMENT_CHANGE_CLEAN   2    /* either channel switch or param change */
#define IEEE80211_SCS_STATE_PERIOD_CLEAN               3

#define IEEE80211_SCS_COMPARE_INIT_TIMER	5
#define IEEE80211_SCS_COMPARE_TIMER_INTVAL	2
#define IEEE80211_CCA_SAMPLE_DUR		IEEE80211_SCS_COMPARE_TIMER_INTVAL /* seconds */
#define IEEE80211_SCS_CHAN_CURRENT		0
#define IEEE80211_SCS_CHAN_ALL			0xFF
#define IEEE80211_SCS_THRSHLD_MAX		100	/* metric */
#define IEEE80211_SCS_THRSHLD_MIN		1	/* metric */
#define IEEE80211_SCS_SMPL_DWELL_TIME_MAX	24	/* milliseconds, limited by max NAV reservation */
#define IEEE80211_SCS_SMPL_DWELL_TIME_MIN	5	/* milliseconds */
#define IEEE80211_SCS_SMPL_DWELL_TIME_DEFAULT	10	/* milliseconds */
#define IEEE80211_SCS_SMPL_INTV_MAX		3600	/* seconds */
#define IEEE80211_SCS_SMPL_INTV_MIN		1	/* seconds */
#define IEEE80211_SCS_SMPL_INTV_DEFAULT		5	/* seconds */
#define IEEE80211_SCS_THRSHLD_SMPL_PKTNUM_DEFAULT	16	/* packet number */
#define IEEE80211_SCS_THRSHLD_SMPL_PKTNUM_MAX	1000	/* packet number */
#define IEEE80211_SCS_THRSHLD_SMPL_PKTNUM_MIN	1	/* packet number */
#ifdef TOPAZ_PLATFORM
#define IEEE80211_SCS_THRSHLD_SMPL_AIRTIME_DEFAULT	100	/* ms */
#else
#define IEEE80211_SCS_THRSHLD_SMPL_AIRTIME_DEFAULT	300	/* ms */
#endif
#define IEEE80211_SCS_THRSHLD_SMPL_AIRTIME_MAX	1000	/* ms */
#define IEEE80211_SCS_THRSHLD_SMPL_AIRTIME_MIN	1	/* ms */
/*
 * Packet rate threshold is determined by how many packets we can hold in buffer without drop
 * during off-channel period. It is limited by:
 * - sw queue length of each node/tid
 * - global resource shared by all node/tid, such as tqew descriptors and msdu headers.
 * Current value doesn't apply to the scenario when tqew descriptors are already used up by large
 * number of stations.
 */
#define IEEE80211_SCS_THRSHLD_SMPL_TX_PKTRATE	(1024 - 128)	/* margin = 128 + hw ring size */
#define IEEE80211_SCS_THRSHLD_SMPL_RX_PKTRATE	IEEE80211_SCS_THRSHLD_SMPL_TX_PKTRATE /* assume qtn peer */
#define IEEE80211_SCS_THRSHLD_ATTEN_INC_DFT	5	/* db */
#define IEEE80211_SCS_THRSHLD_ATTEN_INC_MIN     0       /* db */
#define IEEE80211_SCS_THRSHLD_ATTEN_INC_MAX     20      /* db */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_DFT	60	/* seconds */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MIN   0       /* seconds */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MAX   0xffff  /* seconds */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_INTF_MIN   0
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_INTF_MAX   100
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_INTF_DFT   40
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MINRATE_UNIT	100	/* kbps */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MINRATE_DFT	5	/* unit: 100kbps */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MINRATE_MIN   0       /* unit: 100kbps */
#define IEEE80211_SCS_THRSHLD_DFS_REENTRY_MINRATE_MAX   0xffff  /* unit: 100kbps */
#define IEEE80211_SCS_THRSHLD_AGING_MIN         0
#define IEEE80211_SCS_THRSHLD_AGING_MAX         0xFFFF
#define IEEE80211_SCS_THRSHLD_AGING_NOR_DFT     (60 * 6)
#define IEEE80211_SCS_THRSHLD_AGING_DFSREENT_DFT  5
#define IEEE80211_SCS_CCA_DUR_MAX		10	/* seconds */
#define IEEE80211_SCS_CCA_DUR_MIN		2	/* seconds */
#define IEEE80211_SCS_CCA_INTF_SCALE		1000	/* milliseconds */
#define IEEE80211_SCS_SENDING_QOSNULL_TIME_AVG	1000	/* microseconds */
#define IEEE80211_SCS_SMPL_TIME_MARGIN		2000	/* microseconds */
#define IEEE80211_SCS_SMPL_TIME_OFFSET_SEND_QOSNULL	5000	/* microseconds */
#define IEEE80211_SCS_SMPL_TIME_SENDING_ALL_BEACONS	25000	/* microseconds, the time duration for transmitting all beacons */
#define IEEE80211_CCA_INTF_SMTH_FCTR_NOXP_DFT	75
#define IEEE80211_CCA_INTF_SMTH_FCTR_XPED_DFT	90
#define IEEE80211_CCA_INTF_SMTH_FCTR_MIN	0
#define IEEE80211_CCA_INTF_SMTH_FCTR_MAX	100
#define IEEE80211_SCS_CHAN_MTRC_MRGN_MAX	100
#define IEEE80211_SCS_CHAN_MTRC_MRGN_DFT	5
#define IEEE80211_SCS_RSSI_SMTH_FCTR_UP_DFT	75
#define IEEE80211_SCS_RSSI_SMTH_FCTR_DOWN_DFT	25
#define IEEE80211_SCS_RSSI_SMTH_FCTR_MAX	100
#define IEEE80211_SCS_ATTEN_ADJUST_MIN		-20
#define IEEE80211_SCS_ATTEN_ADJUST_MAX		20
#define IEEE80211_SCS_ATTEN_ADJUST_DFT		5
#define IEEE80211_SCS_BRCM_RXGLITCH_THRSHLD_SCALE_DFT    40
#define IEEE80211_SCS_PMBL_ERR_SMTH_FCTR_MIN    0
#define IEEE80211_SCS_PMBL_ERR_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_PMBL_ERR_SMTH_FCTR_DFT    50
#define IEEE80211_SCS_PMP_RPT_CCA_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_PMP_RPT_CCA_SMTH_FCTR_DFT    50
#define IEEE80211_SCS_PMP_RX_TIME_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_PMP_RX_TIME_SMTH_FCTR_DFT    50
#define IEEE80211_SCS_PMP_TX_TIME_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_PMP_TX_TIME_SMTH_FCTR_DFT    50
#define IEEE80211_SCS_PMP_STATS_STABLE_PERCENT_MAX  100
#define IEEE80211_SCS_PMP_STATS_STABLE_PERCENT_DFT  30
#define IEEE80211_SCS_PMP_STATS_STABLE_RANGE_MAX    1000
#define IEEE80211_SCS_PMP_STATS_STABLE_RANGE_DFT    50
#define IEEE80211_SCS_PMP_STATS_CLEAR_INTERVAL_MAX  3600 /* seconds */
#define IEEE80211_SCS_PMP_STATS_CLEAR_INTERVAL_DFT  60 /* seconds */
#define IEEE80211_SCS_AS_RX_TIME_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_AS_RX_TIME_SMTH_FCTR_DFT    50
#define IEEE80211_SCS_AS_TX_TIME_SMTH_FCTR_MAX    100
#define IEEE80211_SCS_AS_TX_TIME_SMTH_FCTR_DFT    50

#define IEEE80211_SCS_PMBL_ERR_RANGE_MIN        1000
#define IEEE80211_SCS_PMBL_ERR_RANGE_MAX        0xFFFF
#define IEEE80211_SCS_PMBL_ERR_RANGE_DFT        5000
#define IEEE80211_SCS_PMBL_ERR_MAPPED_INTF_RANGE_MIN  0
#define IEEE80211_SCS_PMBL_ERR_MAPPED_INTF_RANGE_MAX  100
#define IEEE80211_SCS_PMBL_ERR_MAPPED_INTF_RANGE_DFT  40
#define IEEE80211_SCS_PMBL_ERR_WF_MIN           0
#define IEEE80211_SCS_PMBL_ERR_WF_MAX           100
#define IEEE80211_SCS_PMBL_SHORT_WF_DFT         0
#define IEEE80211_SCS_PMBL_LONG_WF_DFT          100
#define IEEE80211_SCS_THRSHLD_LOADED_MIN        0
#define IEEE80211_SCS_THRSHLD_LOADED_MAX        1000
#define IEEE80211_SCS_THRSHLD_LOADED_DFT        20

#define IEEE80211_SCS_CHAN_POWER_CUTPOINT       15
#define IEEE80211_SCS_NORMALIZE(_v, _duration)       (((_v) < (0xFFFFFFFF / IEEE80211_SCS_CCA_INTF_SCALE)) ?  \
							((_v) * IEEE80211_SCS_CCA_INTF_SCALE / (_duration)) : \
							((_v) / (_duration) * IEEE80211_SCS_CCA_INTF_SCALE))

#define IEEE80211_SCS_SMOOTH(_old, _new, _fctr)	(((_old) * (_fctr) + (_new) * (100 - (_fctr))) / 100)

#define IEEE80211_SCS_OFFCHAN_WHOLE_DUR(_dwell_us)	((_dwell_us) +					\
							(2 * QTN_SWITCH_CHANNEL_TIME_AVG) +		\
							IEEE80211_SCS_SENDING_QOSNULL_TIME_AVG +	\
							IEEE80211_SCS_SMPL_TIME_MARGIN)

#define IEEE80211_SCS_VALUE_S			0
#define IEEE80211_SCS_VALUE_M			0xffff
#define IEEE80211_SCS_WF_VALUE_M		0xff
#define IEEE80211_SCS_COMMAND_S			16
#define IEEE80211_SCS_COMMAND_M			0xffff

#define IEEE80211_SCS_STA_CCA_REQ_CC		0x1
#define IEEE80211_SCS_SELF_CCA_CC               0x2
#define IEEE80211_SCS_ATTEN_INC_CC		0x4
#define IEEE80211_SCS_BRCM_STA_TRIGGER_CC	0x8
#define IEEE80211_SCS_CCA_INTF_CC               (IEEE80211_SCS_STA_CCA_REQ_CC | IEEE80211_SCS_SELF_CCA_CC)
#define IEEE80211_SCS_INTF_CC                   (IEEE80211_SCS_CCA_INTF_CC | IEEE80211_SCS_BRCM_STA_TRIGGER_CC)

#define SCSLOG_CRIT                             0
#define SCSLOG_NOTICE                           1
#define SCSLOG_INFO                             2
#define SCSLOG_VERBOSE                          3
#define SCSLOG_LEVEL_MAX                        3
#if !defined(MUC_BUILD) && !defined(DSP_BUILD) && !defined(AUC_BUILD)
#define SCSDBG(_level, _fmt, ...)            do {               \
		if (ic->ic_scs.scs_debug_enable >= (_level)) {  \
			DBGFN("SCS: " _fmt, ##__VA_ARGS__);     \
		}                                               \
	} while (0)
#endif


/* OCAC (Off-channel CAC) APIs */
enum qtn_ocac_cmds {
	IEEE80211_OCAC_SET_ENABLE = 1,
	IEEE80211_OCAC_SET_DISABLE,
	IEEE80211_OCAC_SET_DEBUG_LEVEL,
	IEEE80211_OCAC_SET_DWELL_TIME,
	IEEE80211_OCAC_SET_DURATION,
	IEEE80211_OCAC_SET_THRESHOLD_FAT,
	IEEE80211_OCAC_SET_DUMP_COUNTS,
	IEEE80211_OCAC_SET_CAC_TIME,
	IEEE80211_OCAC_SET_THRESHOLD_TRAFFIC,
	IEEE80211_OCAC_SET_TIMER_INTERVAL,
	IEEE80211_OCAC_SET_DUMP_TSFLOG,
	IEEE80211_OCAC_SET_DUMP_CFG,
	IEEE80211_OCAC_SET_TRAFFIC_CONTROL,
	IEEE80211_OCAC_SET_THRESHOLD_CCA_INTF,
	IEEE80211_OCAC_SET_REPORT_ONLY,
	IEEE80211_OCAC_SET_DUMP_CCA_COUNTS,
	IEEE80211_OCAC_SET_OFFSET_TXHALT,
	IEEE80211_OCAC_SET_OFFSET_OFFCHAN,
	IEEE80211_OCAC_SET_THRESHOLD_FAT_DEC,
	IEEE80211_OCAC_SET_TIMER_EXPIRE_INIT,
	IEEE80211_OCAC_SET_SECURE_DWELL_TIME,
	IEEE80211_OCAC_SET_BEACON_INTERVAL,
	IEEE80211_OCAC_SET_MAX
};

#define IEEE80211_OCAC_CLEAN_STATS_STOP		0
#define IEEE80211_OCAC_CLEAN_STATS_START	1
#define IEEE80211_OCAC_CLEAN_STATS_RESET	2


#define IEEE80211_OCAC_DWELL_TIME_MIN		5	/* milliseconds */
#define IEEE80211_OCAC_DWELL_TIME_MAX		200	/* milliseconds */
#define IEEE80211_OCAC_DWELL_TIME_DEFAULT	50	/* milliseconds */

#define IEEE80211_OCAC_SECURE_DWELL_TIME_MIN		5	/* milliseconds */
#define IEEE80211_OCAC_SECURE_DWELL_TIME_MAX		23	/* milliseconds */
#define IEEE80211_OCAC_SECURE_DWELL_TIME_DEFAULT	23	/* milliseconds */

#define IEEE80211_OCAC_DURATION_MIN		1	/* seconds */
#define IEEE80211_OCAC_DURATION_MAX		64800	/* seconds */
#define IEEE80211_OCAC_DURATION_DEFAULT		360	/* seconds */

#define IEEE80211_OCAC_CAC_TIME_MIN		1	/* seconds */
#define IEEE80211_OCAC_CAC_TIME_MAX		64800	/* seconds */
#define IEEE80211_OCAC_CAC_TIME_DEFAULT		145	/* seconds */

#define IEEE80211_OCAC_THRESHOLD_FAT_MIN	1	/* percent */
#define IEEE80211_OCAC_THRESHOLD_FAT_MAX	100	/* percent */
#define IEEE80211_OCAC_THRESHOLD_FAT_DEFAULT	65	/* percent */

#define IEEE80211_OCAC_THRESHOLD_TRAFFIC_MIN		1	/* percent */
#define IEEE80211_OCAC_THRESHOLD_TRAFFIC_MAX		100	/* percent */
#define IEEE80211_OCAC_THRESHOLD_TRAFFIC_DEFAULT	35	/* percent */

#define IEEE80211_OCAC_OFFSET_TXHALT_MIN		2	/* milliseconds */
#define IEEE80211_OCAC_OFFSET_TXHALT_MAX		80	/* milliseconds */
#define IEEE80211_OCAC_OFFSET_TXHALT_DEFAULT		10	/* milliseconds */

#define IEEE80211_OCAC_OFFSET_OFFCHAN_MIN		2	/* milliseconds */
#define IEEE80211_OCAC_OFFSET_OFFCHAN_MAX		80	/* milliseconds */
#define IEEE80211_OCAC_OFFSET_OFFCHAN_DEFAULT		5	/* milliseconds */

#define IEEE80211_OCAC_TRAFFIC_CTRL_DEFAULT		1	/* on */

#define IEEE80211_OCAC_THRESHOLD_CCA_INTF_MIN		1	/* percent */
#define IEEE80211_OCAC_THRESHOLD_CCA_INTF_MAX		100	/* percent */
#define IEEE80211_OCAC_THRESHOLD_CCA_INTF_DEFAULT	20	/* percent */

#define IEEE80211_OCAC_THRESHOLD_FAT_DEC_MIN		1	/* percent */
#define IEEE80211_OCAC_THRESHOLD_FAT_DEC_MAX		100	/* percent */
#define IEEE80211_OCAC_THRESHOLD_FAT_DEC_DEFAULT	10	/* percent */

#define IEEE80211_OCAC_TIMER_INTERVAL_MIN		1	/* seconds */
#define IEEE80211_OCAC_TIMER_INTERVAL_MAX		100	/* seconds */
#define IEEE80211_OCAC_TIMER_INTERVAL_DEFAULT		2	/* seconds */

#define IEEE80211_OCAC_BEACON_INTERVAL_MIN		100	/* TUs */
#define IEEE80211_OCAC_BEACON_INTERVAL_MAX		1000	/* TUs */
#define IEEE80211_OCAC_BEACON_INTERVAL_DEFAULT		100	/* TUs */

#define IEEE80211_OCAC_TIMER_EXPIRE_INIT_MIN		1	/* seconds */
#define IEEE80211_OCAC_TIMER_EXPIRE_INIT_MAX		65000	/* seconds */
#define IEEE80211_OCAC_TIMER_EXPIRE_INIT_DEFAULT	2	/* seconds */


#define IEEE80211_OCAC_VALUE_S			0
#define IEEE80211_OCAC_VALUE_M			0xffff
#define IEEE80211_OCAC_COMMAND_S		16
#define IEEE80211_OCAC_COMMAND_M		0xffff

#define IEEE80211_OCAC_TIME_MARGIN		2000	/* microseconds */

#define OCACLOG_CRIT				0
#define OCACLOG_WARNING				1
#define OCACLOG_NOTICE				2
#define OCACLOG_INFO				3
#define OCACLOG_VERBOSE				4
#define OCACLOG_LEVEL_MAX			4
#if !defined(MUC_BUILD) && !defined(DSP_BUILD) && !defined(AUC_BUILD)
#define OCACDBG(_level, _fmt, ...)            do {               \
		if (ic->ic_ocac.ocac_cfg.ocac_debug_level >= (_level)) {  \
			DBGFN("OCAC: " _fmt, ##__VA_ARGS__);     \
		}                                               \
        } while (0)
#endif

#define QTN_M2A_EVENT_TYPE_DTIM 1


/* Common definitions for flags used to indicate ieee80211_node's states */
#define	IEEE80211_NODE_AUTH	0x0001		/* authorized for data */
#define	IEEE80211_NODE_QOS	0x0002		/* QoS enabled */
#define	IEEE80211_NODE_ERP	0x0004		/* ERP enabled */
#define	IEEE80211_NODE_HT	0x0008		/* HT enabled */
/* NB: this must have the same value as IEEE80211_FC1_PWR_MGT */
#define	IEEE80211_NODE_PWR_MGT	0x0010		/* power save mode enabled */
#define	IEEE80211_NODE_PS_DELIVERING	0x0040	/* STA out of PS, getting delivery */
#define	IEEE80211_NODE_PS_POLL	0x0080		/* power save ps poll mode */
#define	IEEE80211_NODE_AREF	0x0020		/* authentication ref held */
#define	IEEE80211_NODE_TX_RESTRICTED	0x0100	/* restricted tx enabled */
#define	IEEE80211_NODE_TX_RESTRICT_RTS	0x0200	/* use RTS to confirm node is lost */
#define IEEE80211_NODE_2_TX_CHAINS      0x0400  /* this node needs to use 2 TX chain only, for IOT purpose */
#define IEEE80211_NODE_UAPSD_TRIG	0x0800
#define IEEE80211_NODE_UAPSD		0x1000
#define IEEE80211_NODE_WDS_PEER		0x2000	/* this node is the wds peer in a wds vap */
#define IEEE80211_NODE_VHT		0x4000	/* VHT enabled */
#define IEEE80211_NODE_TPC		0x8000	/* indicate tpc capability */

#define QTN_VAP_PRIORITY_RESERVED	2	/* reserve the low values for internal use */
#define QTN_VAP_PRIORITY_NUM		4
#define QTN_VAP_PRIORITY_MGMT		(QTN_VAP_PRIORITY_RESERVED + QTN_VAP_PRIORITY_NUM)
#define QTN_TACMAP_HW_PRI_NUM		8	/* hw limitation for 128 node mode */
#define QTN_TACMAP_PRI_PER_VAP		8	/* for maximum 8 TIDs */
#define QTN_TACMAP_SW_PRI_BASE		64	/* values below this are used for "bad apple" nodes */

/* Quantenna specific flags (ni_qtn_flags) */
#define QTN_IS_BCM_NODE			0x0000001
#define QTN_IS_IPAD_NODE		0x0000002
#define QTN_IS_IPHONE5_NODE		0x0000004
#define QTN_IS_IPAD3_NODE		0x0000008
#define QTN_IS_INTEL_5100_NODE		0x0000010
#define QTN_IS_INTEL_5300_NODE		0x0000020
#define QTN_IS_SAMSUNG_GALAXY_NODE	0x0000040
#define QTN_IS_NOT_4ADDR_CAPABLE_NODE	0x0000080
#define QTN_AC_BE_INHERITANCE_UPTO_VO	0x0000100
#define QTN_AC_BE_INHERITANCE_UPTO_VI	0x0000200
#define QTN_IS_INTEL_NODE		0x0000400
#define QTN_IS_IPAD_AIR_NODE		0x0000800
#define QTN_IS_IPAD4_NODE		0x0001000
#define QTN_DYN_ENABLE_RTS		0x0002000

/*
 * Definitions relating to individual fields from phy_stats,
 * shared between the Q driver and the APIs.
 */

/*
 * Error Sum needs to be reported together with the corresponding Number of
 * Symbols; getting them in separate operations would introduce a race condition
 * where the Error Sum and the Number of Symbols came from different
 * PHY stat blocks.
 */

#define QTN_PHY_AVG_ERROR_SUM_NSYM_NAME			"avg_error_sum_nsym"

#define QTN_PHY_EVM_MANTISSA_SHIFT		5
#define QTN_PHY_EVM_EXPONENT_MASK		0x1f

enum qtn_phy_stat_field {
	QTN_PHY_NOSUCH_FIELD = -1,
	QTN_PHY_AVG_ERROR_SUM_NSYM_FIELD,
};

/* only for little endian */
#if defined(AUC_BUILD)
#define U64_LOW32(_v)		((uint32_t)(_v))
#define U64_HIGH32(_v)		((uint32_t)((_v) >> 32))
#else
#define U64_LOW32(_v)		(((uint32_t*)&(_v))[0])
#define U64_HIGH32(_v)		(((uint32_t*)&(_v))[1])
#endif

#define U64_COMPARE_GE(_a, _b)	((U64_HIGH32(_a) > U64_HIGH32(_b)) ||	\
				((U64_HIGH32(_a) == U64_HIGH32(_b)) && (U64_LOW32(_a) >= U64_LOW32(_b))))

#define U64_COMPARE_GT(_a, _b)	((U64_HIGH32(_a) > U64_HIGH32(_b)) ||	\
				((U64_HIGH32(_a) == U64_HIGH32(_b)) && (U64_LOW32(_a) > U64_LOW32(_b))))

#define U64_COMPARE_LE(_a, _b)	((U64_HIGH32(_a) < U64_HIGH32(_b)) ||	\
				((U64_HIGH32(_a) == U64_HIGH32(_b)) && (U64_LOW32(_a) <= U64_LOW32(_b))))

#define U64_COMPARE_LT(_a, _b)	((U64_HIGH32(_a) < U64_HIGH32(_b)) ||	\
				((U64_HIGH32(_a) == U64_HIGH32(_b)) && (U64_LOW32(_a) < U64_LOW32(_b))))

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#define MACSTRL "%02x:%02x:%02x:%02x:%02x:%02x"	/* for MuC and Auc which don't support "X" */
#endif

#endif /* _SHARED_DEFS_H_ */
