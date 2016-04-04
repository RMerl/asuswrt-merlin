/*SH1
*******************************************************************************
**                                                                           **
**         Copyright (c) 2008 - 2009 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  Date        : 01/28/09                                                   **
**  File        : txbf_api.h                                                 **
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

#ifndef _TXBF_API_H_
#define _TXBF_API_H_

/*
 * Enable installation of a fixed Q matrix in place of the derived ones.
 * The matrix should be put in the file drivers/qdrv/fixedqmat.h
 * The format of the data should be word values comma delimited.
 */
#define TXBF_CANNED_QMAT
#undef TXBF_CANNED_QMAT

/* Until we get 2 tx antennas working on a 2x4 station */
#define HAL_2x4STA_USE_4_TX_ANTENNAS

/*
 * Comment this line to disable locking.
 * Or set to 1 to enable manual locking.
 * Or set to 0 to enable hw-centric automatic locking.
 */
#define QTN_TXBF_FFT_LOCK_MANUAL	(1)

/* VHTTXBFTBD - 11ac BF enabled by default */
#define TXBF_ENABLE_VHT_BF

/* Use sw generated VHT BF path */
/* #define TXBF_VHT_SW_FEEDBACK */
/* #define TXBF_VHT_SW_UNCOMPRESSED */	/* for debugging */

/* Expansion Matrix Modes */
#define TXBF_MODE_NO_MATRIX		0
#define TXBF_MODE_DEFAULT_MATRIX	1
#define TXBF_MODE_BBF			2
#define TXBF_MODE_STD_BF		3

/* Enable (1) both hw and sw generated VHT BF feedback, used for debugging */
#define TXBF_VHT_HW_AND_SW_FEEDBACK	0

enum txbf_buff_state
{
	/* Tx BF buffer for the frame is free */
	TXBF_BUFF_FREE		= 0,
	/* The frame is stored in Tx BF buffer for processing and can not be released*/
	TXBF_BUFF_IN_USE	= 1,
	/* Not used */
	TXBF_DMA_FROM_BB	= 2,
	/* NDP only. The frame is being processed by DSP */
	TXBF_DSP_PROC		= 3,
	/* DSP completes frame processing */
	TXBF_DSP_DONE		= 4,
	/* For action frame only. Action frame is stored in action frame cache */
	TXBF_DSP_STORED		= 5
};

#define TXBF_BF_VER1	1	/* Envy */
#define TXBF_BF_VER2	2	/* 2 stream Ruby */
#define TXBF_BF_VER3	3	/* 4 stream non-tone grouping Ruby */
#define TXBF_BF_VER4	4	/* 4 stream tone grouping Ruby and later */
/*
 * Version 4 means the action frames generated are now standards compliant
 * and the BF parameters are derived from the various fields and association
 * exchange, rather than from using a new version for each combination
 */

/*
 * These structures are shared between Linux, MuC and DSP.
 */
struct txbf_ndp_info
{
	char bw_mode;
	char rxgain;
	char MN;
	char hwnoise;
	char max_gain;
	char macaddr[6];
	signed char reg_scale_fac;
	unsigned char Nsts;
	unsigned char Ness;
};

#define TXBF_MUC_DSP_SHMEM_START (0x80060000)

#define QTN_MU_QMAT_MAX_SLOTS		3

/* Beamforming message types */
#define QTN_TXBF_NDP_RX_MSG		1
#define QTN_TXBF_ACT_FRM_TX_MSG		2
#define QTN_TXBF_ACT_FRM_RX_MSG		3
#define QTN_TXBF_ACT_FRM_FREE_MSG	4
#define QTN_TXBF_DEL_MU_NODE_MSG	5
#define QTN_TXBF_MU_GRP_UPD_DONE_MSG	6
#define QTN_TXBF_TRIG_MU_GRP_SEL_MSG	7
#define QTN_TXBF_RATE_TRAIN_MSG		8
#define QTN_TXBF_RATE_TRAIN_HASH_MSG	9
#define QTN_TXBF_NDP_DISCARD_MSG	10

#define QTN_TXBF_ACT_FRM_XTRA_HDR_LEN	10

#define QTN_TXBF_MODE_HT		0
#define QTN_TXBF_MODE_VHT		1

#define QTN_TXBF_NO_EXPMAT		0xFFFF

#define MU_NDPA_TOKEN_MASK		0x1F

/* Number of 10ms timeslots used on the DSP to process feedback */
#define QTN_TXBF_SU_DSP_TIMESLOTS	1
#define QTN_TXBF_MU_DSP_TIMESLOTS	2

/* We leave backward compatibility here. As SU token value was randomly chosen as 0x33
we now say when bit 5 is set it indicates SU sounding. */
#define MU_NDPA_SU_MASK			0x20
#define MU_NDPA_GRP_SND_MASK		0x10
#define IS_MU_GRP_SND(token)		((token) & MU_NDPA_GRP_SND_MASK)

/* TODO: Needs reworking. Some fields (at least mu_grp_id) are used only by
distinct message type */
struct txbf_pkts
{
	unsigned msg_type;
	unsigned state;
	unsigned bf_ver;
	unsigned bf_mode;
	unsigned act_frame_phys;
	unsigned buffer_start;
	unsigned act_frame_len;
	unsigned skb;
	unsigned qmat_offset;
	unsigned inst_1ss_def_mat;
	unsigned success;
	unsigned ndp_phys;
	unsigned nstream;
	unsigned bf_mimo_nc;
	unsigned bf_mimo_nr;
	unsigned bf_tone_grp;
	unsigned bf_coeff_size;
	unsigned bf_nss_snr[4];
	unsigned bf_compressed;
	unsigned bf_codebook;
	unsigned pkt_indx;
	unsigned short aid;
	unsigned node_bw;
	unsigned bw_pri_40m_lower;
	unsigned bw_pri_20m_lower;
	unsigned txbf_skip_dftmat_flag;
	unsigned txbf_2x4sta_flag;
	unsigned txbf_qmat_install_wait;
	struct txbf_ndp_info ndp_info;
	char act_frame_sa[6];
	char act_frame_bssid[6];
	char slot;
	uint8_t mu_grp_id[QTN_MU_QMAT_MAX_SLOTS];
	uint8_t vapid;
	unsigned counter;
};

#define IEEE80211_ADDR_LEN	6
struct qtn_rate_train_info
{
	unsigned msg_type;
	unsigned state;
	char src[IEEE80211_ADDR_LEN];
	char dst[IEEE80211_ADDR_LEN];
	unsigned ver;
	unsigned nonce;
	unsigned hash;
	unsigned stamp;
	unsigned ni;
	void *next; /* Chaining for retry on mbox busy */
	int index;
	char devid;
	char padding[15]; /* Cache aligned */
};

struct txbf_ctrl {
	unsigned bf_tone_grp;
	unsigned svd_mode;
	unsigned bfoff_thresh;
};

typedef struct
{
	signed int pad1:4;
	signed int im:12;
	signed int pad2:4;
	signed int re:12;
} ndp_format;

typedef union
{
	ndp_format ndp;
	int wrd;
} bbmem_ndp;


typedef struct
{
	/* Compq format BB0 20 bits */
	signed int bb0_re_s0:8;
	signed int bb0_im_s0:8;
	signed int bb0_re_s1_lo:4;
	/* Interleaved BB1 12 bits */
	signed int bb1_re_s1_hi:4;
	signed int bb1_im_s1:8;
	/* Compq format BB0 12 bits */
	signed int bb0_re_s1_hi:4;
	signed int bb0_im_s1:8;
	/* Interleaved BB1 20 bits */
	signed int bb1_re_s0:8;
	signed int bb1_im_s0:8;
	signed int bb1_re_s1_lo:4;
}st_format;

typedef union
{
	st_format st;
	unsigned wrd[2];
} bbmem_st;

/*
 * Maximum streams supported for different matrix types
 */
#define QTN_MAX_STREAMS			4
#define QTN_MAX_40M_VHT_STREAMS		2
#define QTN_MAX_20M_VHT_STREAMS		2
#define QTN_MAX_IOT_QMAT_STREAMS	3

/*
 * Default decimation used for matrices in Q memory. Some matrices
 * may use more decimation if space is a problem
 */
#define QTN_TXBF_DEFAULT_QMAT_NG	1
#define QTN_TXBF_MAX_QMAT_NG		2

#define NDP_TO_STVEC_SIZE_RATIO 4

#define NDP_START_DELAY		2		/* in seconds  */

#define STVEC_SIZE_BYTES_1STRM_20M 0x100	/* Assumes NG 1 matrices */
#define STVEC_MAX_NODES		10

/*
 * Matrix sizes for NG 1 matrices
 */
#define STVEC_SIZE_BYTES_1STRM_40M (STVEC_SIZE_BYTES_1STRM_20M << 1)
#define STVEC_SIZE_BYTES_1STRM_80M (STVEC_SIZE_BYTES_1STRM_40M << 1)
#define STVEC_SIZE_BYTES_2STRM_20M (STVEC_SIZE_BYTES_1STRM_20M << 1)
#define STVEC_SIZE_BYTES_2STRM_40M (STVEC_SIZE_BYTES_2STRM_20M << 1)
#define STVEC_SIZE_BYTES_2STRM_80M (STVEC_SIZE_BYTES_2STRM_40M << 1)
#define STVEC_SIZE_BYTES_3STRM_20M (STVEC_SIZE_BYTES_2STRM_20M + STVEC_SIZE_BYTES_1STRM_20M)
#define STVEC_SIZE_BYTES_3STRM_40M (STVEC_SIZE_BYTES_3STRM_20M << 1)
#define STVEC_SIZE_BYTES_3STRM_80M (STVEC_SIZE_BYTES_3STRM_40M << 1)
#define STVEC_SIZE_BYTES_4STRM_20M (STVEC_SIZE_BYTES_2STRM_20M << 1)
#define STVEC_SIZE_BYTES_4STRM_40M (STVEC_SIZE_BYTES_2STRM_40M << 1)
#define STVEC_SIZE_BYTES_4STRM_80M (STVEC_SIZE_BYTES_4STRM_40M << 1)
#define STVEC_SIZE_BYTES_1STRM_MAX STVEC_SIZE_BYTES_1STRM_80M
#define STVEC_SIZE_BYTES_2STRM_MAX STVEC_SIZE_BYTES_2STRM_80M
#define STVEC_SIZE_BYTES_3STRM_MAX STVEC_SIZE_BYTES_3STRM_80M
#define STVEC_SIZE_BYTES_4STRM_MAX STVEC_SIZE_BYTES_4STRM_80M

#ifndef QTN_BW_20M
# define QTN_BW_20M 0
# define QTN_BW_40M 1
# define QTN_BW_80M 2
#endif
#define QTN_BW_SW_MAX QTN_BW_80M

#define NDP_SIZE_BYTES_BASE	1024
#define NDP_SIZE_BYTES_20M	(NDP_SIZE_BYTES_BASE << QTN_BW_20M)
#define NDP_SIZE_BYTES_40M	(NDP_SIZE_BYTES_BASE << QTN_BW_40M)
#define NDP_SIZE_BYTES_80M	(NDP_SIZE_BYTES_BASE << QTN_BW_80M)
#define NDP_SIZE_BYTES_MAX	(NDP_SIZE_BYTES_BASE << QTN_BW_SW_MAX)

/*
 * Q matrix defines for 80 MHz nodes using NG 1
 */
#define QTN_TXBF_QMAT80_1STRM_OFFSET(offset)		(offset)
#define QTN_TXBF_QMAT80_1STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_1STRM_80M + \
							STVEC_SIZE_BYTES_1STRM_40M + \
							STVEC_SIZE_BYTES_1STRM_20M)
#define QTN_TXBF_QMAT80_2STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_2STRM_80M + \
							STVEC_SIZE_BYTES_2STRM_40M + \
							STVEC_SIZE_BYTES_2STRM_20M)
#define QTN_TXBF_QMAT80_3STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_3STRM_80M)
#define QTN_TXBF_QMAT80_4STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_4STRM_80M)
#define QTN_TXBF_QMAT80_1STRM_40M_OFFSET(offset)	(offset + \
							STVEC_SIZE_BYTES_1STRM_80M)
#define QTN_TXBF_QMAT80_1STRM_20M_OFFSET(offset)	(offset + \
							STVEC_SIZE_BYTES_1STRM_80M + \
							STVEC_SIZE_BYTES_1STRM_40M)
#define QTN_TXBF_QMAT80_2STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT80_1STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_2STRM_40M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							STVEC_SIZE_BYTES_2STRM_80M)
#define QTN_TXBF_QMAT80_2STRM_20M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							STVEC_SIZE_BYTES_2STRM_80M + \
							STVEC_SIZE_BYTES_2STRM_40M)
#define QTN_TXBF_QMAT80_3STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_2STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_4STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_3STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_TOTAL_SIZE			(QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_3STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_4STRM_MAT_TOTAL)

/*
 * Q matrix defines for 80 MHz nodes using NG 2
 */
#define QTN_TXBF_QMAT80_NG2_1STRM_OFFSET(offset)	(offset)
#define QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL		((STVEC_SIZE_BYTES_1STRM_80M / 2) + \
							( STVEC_SIZE_BYTES_1STRM_40M / 2) + \
							(STVEC_SIZE_BYTES_1STRM_20M / 2))
#define QTN_TXBF_QMAT80_NG2_2STRM_MAT_TOTAL		((STVEC_SIZE_BYTES_2STRM_80M / 2) + \
							(STVEC_SIZE_BYTES_2STRM_40M / 2) + \
							(STVEC_SIZE_BYTES_2STRM_20M / 2))
#define QTN_TXBF_QMAT80_NG2_3STRM_MAT_TOTAL		(STVEC_SIZE_BYTES_3STRM_80M / 2)
#define QTN_TXBF_QMAT80_NG2_4STRM_MAT_TOTAL		(STVEC_SIZE_BYTES_4STRM_80M / 2)
#define QTN_TXBF_QMAT80_NG2_1STRM_40M_OFFSET(offset)	(offset + \
							(STVEC_SIZE_BYTES_1STRM_80M / 2))
#define QTN_TXBF_QMAT80_NG2_1STRM_20M_OFFSET(offset)	(offset + \
							(STVEC_SIZE_BYTES_1STRM_80M / 2) + \
							(STVEC_SIZE_BYTES_1STRM_40M / 2))
#define QTN_TXBF_QMAT80_NG2_2STRM_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_NG2_2STRM_40M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL + \
							(STVEC_SIZE_BYTES_2STRM_80M / 2))
#define QTN_TXBF_QMAT80_NG2_2STRM_20M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL + \
							(STVEC_SIZE_BYTES_2STRM_80M / 2) + \
							(STVEC_SIZE_BYTES_2STRM_40M / 2))
#define QTN_TXBF_QMAT80_NG2_3STRM_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_2STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_NG2_4STRM_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT80_NG2_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_3STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT80_NG2_TOTAL_SIZE			(QTN_TXBF_QMAT80_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_3STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT80_NG2_4STRM_MAT_TOTAL)
/*
 * Q matrix defines for 40 MHz nodes using NG 1
 */
#define QTN_TXBF_QMAT40_1STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_1STRM_40M + \
							STVEC_SIZE_BYTES_1STRM_20M)
#define QTN_TXBF_QMAT40_2STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_2STRM_40M + \
							STVEC_SIZE_BYTES_2STRM_20M)
#define QTN_TXBF_QMAT40_3STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_3STRM_40M)
#define QTN_TXBF_QMAT40_4STRM_MAT_TOTAL			(STVEC_SIZE_BYTES_4STRM_40M)
#define QTN_TXBF_QMAT40_1STRM_OFFSET(offset)		(offset)
#define QTN_TXBF_QMAT40_1STRM_40M_OFFSET(offset)	(offset)
#define QTN_TXBF_QMAT40_1STRM_20M_OFFSET(offset)	(offset + \
							STVEC_SIZE_BYTES_1STRM_40M)
#define QTN_TXBF_QMAT40_2STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT40_1STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT40_2STRM_40M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT40_1STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT40_2STRM_20M_OFFSET(offset)	(offset + \
							QTN_TXBF_QMAT40_1STRM_MAT_TOTAL + \
							STVEC_SIZE_BYTES_2STRM_40M)
#define QTN_TXBF_QMAT40_3STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT40_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_2STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT40_4STRM_OFFSET(offset)		(offset + \
							QTN_TXBF_QMAT40_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_3STRM_MAT_TOTAL)
#define QTN_TXBF_QMAT40_TOTAL_SIZE			(QTN_TXBF_QMAT40_1STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_2STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_3STRM_MAT_TOTAL + \
							QTN_TXBF_QMAT40_4STRM_MAT_TOTAL)

/*
 * Defines for dividing Q memory into slots for nodes using standard BF
 */
#define QTN_TXBF_QMAT_SLOT_SIZE			MAX(QTN_TXBF_QMAT40_TOTAL_SIZE, \
							QTN_TXBF_QMAT80_TOTAL_SIZE / 2)

#define QTN_TXBF_QMAT_SLOTS_USED(qn)	(MAX(1, ((qn)->qn_node.ni_bw_cap >>		\
					MAX(((qn)->qn_expmat.ng - QTN_TXBF_DEFAULT_QMAT_NG), 0))))

#define QTN_TXBF_QMAT_SLOT(idx)		((idx) * QTN_TXBF_QMAT_SLOT_SIZE)

#define QTN_TXBF_QMAT_OFFSET_SHIFT		6

/*
 * Defines for fixed matrix (BBF and default) sizes
 */
#define QTN_TXBF_QMAT_MIN_OFFSET		(1 << QTN_TXBF_QMAT_OFFSET_SHIFT)

#define QTN_TXBF_1SS_WORDS_PER_TONE		2
#define QTN_TXBF_2SS_WORDS_PER_TONE		4
#define QTN_TXBF_3SS_WORDS_PER_TONE		6
#define QTN_TXBF_4SS_WORDS_PER_TONE		8
#define QTN_TXBF_IOT_QMAT_1SS_WORDS		(QTN_TXBF_IOT_QMAT_TONES * \
							QTN_TXBF_1SS_WORDS_PER_TONE)
#define QTN_TXBF_IOT_QMAT_2SS_WORDS		(QTN_TXBF_IOT_QMAT_TONES * \
							QTN_TXBF_2SS_WORDS_PER_TONE)
#define QTN_TXBF_IOT_QMAT_3SS_WORDS		(QTN_TXBF_IOT_QMAT_TONES * \
							QTN_TXBF_3SS_WORDS_PER_TONE)
#define QTN_TXBF_IOT_QMAT_4SS_WORDS		(QTN_TXBF_IOT_QMAT_TONES * \
							QTN_TXBF_4SS_WORDS_PER_TONE)
#define QTN_TXBF_IOT_QMAT_1SS_MEM		MAX((QTN_TXBF_IOT_QMAT_1SS_WORDS * 4), \
                                                        QTN_TXBF_QMAT_MIN_OFFSET)
#define QTN_TXBF_IOT_QMAT_2SS_MEM		MAX((QTN_TXBF_IOT_QMAT_2SS_WORDS * 4), \
                                                        QTN_TXBF_QMAT_MIN_OFFSET)
#define QTN_TXBF_IOT_QMAT_3SS_MEM		MAX((QTN_TXBF_IOT_QMAT_3SS_WORDS * 4), \
                                                        QTN_TXBF_QMAT_MIN_OFFSET)
#define QTN_TXBF_IOT_QMAT_4SS_MEM		MAX((QTN_TXBF_IOT_QMAT_4SS_WORDS * 4), \
                                                        QTN_TXBF_QMAT_MIN_OFFSET)

#define QTN_TXBF_QMAT_FIXED_MAT_START		(QTN_TXBF_QMAT_SLOT_SIZE * STVEC_MAX_NODES)

/*
 * Fixed 2x4 node matrix definitions
 */
/*
 * 80MHz 2x4 matrices need to start in normal BF area to fit,
 * this is OK, as they are used on the station only at present
 */
#define QTN_TXBF_QMAT_2x4STA_1_STRM_OFFSET	(QTN_TXBF_QMAT_SLOT_SIZE * (STVEC_MAX_NODES - 1))
#define QTN_TXBF_QMAT_2x4STA_2_STRM_OFFSET	(QTN_TXBF_QMAT_2x4STA_1_STRM_OFFSET + \
						STVEC_SIZE_BYTES_1STRM_80M)

#define QTN_TXBF_2x4STA_1SS_TONE_DATA	{0x00400040, 0x00000000}
#define QTN_TXBF_2x4STA_2SS_TONE_DATA	{0x00000040, 0x00400000, 0x00000000, 0x00000000}

/*
 * Fixed default matrix offset definitions
 */
#define QTN_TXBF_QMAT_STD_START			QTN_TXBF_QMAT_FIXED_MAT_START
#define QTN_TXBF_QMAT_STD_1_STRM_OFFSET		QTN_TXBF_QMAT_STD_START
#define QTN_TXBF_QMAT_STD_2_STRM_OFFSET		(QTN_TXBF_QMAT_STD_1_STRM_OFFSET + \
						QTN_TXBF_IOT_QMAT_1SS_MEM)
#define QTN_TXBF_QMAT_STD_3_STRM_OFFSET		(QTN_TXBF_QMAT_STD_2_STRM_OFFSET + \
						QTN_TXBF_IOT_QMAT_2SS_MEM)
#define QTN_TXBF_QMAT_STD_4_STRM_OFFSET		(QTN_TXBF_QMAT_STD_3_STRM_OFFSET + \
						QTN_TXBF_IOT_QMAT_3SS_MEM)

#define QTN_TXBF_IOT_QMAT_START			(QTN_TXBF_QMAT_STD_4_STRM_OFFSET + \
						QTN_TXBF_IOT_QMAT_4SS_MEM)
#define QTN_TXBF_IOT_QMAT_TONES			2	/* number of tones for fixed matrices */

/*
 * BBF slot and matrix offset definitions
 *
 * For each slot there is space for the probed matrix, plus the 1, 2 and 3 streams
 * matrices for the index being used by that node
 */
#define QTN_TXBF_IOT_QMAT_MAX_SLOTS		9
#define QTN_TXBF_IOT_QMAT_NG			7
#define QTN_TXBF_IOT_QMAT_PER_SS		18
#define QTN_TXBF_IOT_QMAT_PROBE_MEM		QTN_TXBF_IOT_QMAT_3SS_MEM
#define QTN_TXBF_IOT_QMAT_SLOT_SIZE		(QTN_TXBF_IOT_QMAT_1SS_MEM + \
						QTN_TXBF_IOT_QMAT_2SS_MEM + \
						QTN_TXBF_IOT_QMAT_3SS_MEM + \
						QTN_TXBF_IOT_QMAT_PROBE_MEM)
#define QTN_TXBF_IOT_QMAT_BASE_OFFSET(slot)	(QTN_TXBF_IOT_QMAT_START + \
						(QTN_TXBF_IOT_QMAT_SLOT_SIZE * (slot)))
#define QTN_TXBF_IOT_QMAT_1SS_OFFSET(slot)	(QTN_TXBF_IOT_QMAT_BASE_OFFSET(slot))
#define QTN_TXBF_IOT_QMAT_2SS_OFFSET(slot)	(QTN_TXBF_IOT_QMAT_BASE_OFFSET(slot) + \
						QTN_TXBF_IOT_QMAT_1SS_MEM)
#define QTN_TXBF_IOT_QMAT_3SS_OFFSET(slot)	(QTN_TXBF_IOT_QMAT_BASE_OFFSET(slot) + \
						QTN_TXBF_IOT_QMAT_1SS_MEM + \
						QTN_TXBF_IOT_QMAT_2SS_MEM)
#define QTN_TXBF_IOT_QMAT_PROBE_OFFSET(slot)	(QTN_TXBF_IOT_QMAT_BASE_OFFSET(slot) + \
						QTN_TXBF_IOT_QMAT_1SS_MEM + \
						QTN_TXBF_IOT_QMAT_2SS_MEM + \
						QTN_TXBF_IOT_QMAT_3SS_MEM)

#define FFT_TONE_20M_LO 1
#define FFT_TONE_20M_HI 28

#define FFT_TONE_40M_LO 2
#define FFT_TONE_40M_HI 58

#define QTN_TXBF_TONES_PER_CHAN 64
#define QTN_TXBF_MAX_TONES	128
#define QTN_TXBF_MIN_TONES	1
#define	QTN_TXBF_MODE	4 /* 80MHz Mode */

enum {
	SVD_MODE_STREAM_MIXING =0,
	SVD_MODE_TWO_STREAM,
	SVD_MODE_PER_ANT_SCALE,
	SVD_MODE_CHANNEL_INV,
	SVD_MODE_BYPASS,
};

#define SVD_MODE_GET(X,S) 	(( X >> S) & 1)
#define SVD_MODE_SET(S) 	(1 << S)

#if !defined(QTN_TXBF_FFT_LOCK_MANUAL) || (QTN_TXBF_FFT_LOCK_MANUAL == 1)
	/* Locking is disabled or manual locking */
	#define QT3_BB_MIMO_BF_RX_INIT_VAL	(0x0A)
	#define QT4_BB_MIMO_BF_RX_INIT_VAL	(QT3_BB_MIMO_BF_RX_INIT_VAL)
#else
	/* Automatic, hw-centric locking. */
	#define QT3_BB_MIMO_BF_RX_INIT_VAL	(0x0B)
	#define QT4_BB_MIMO_BF_RX_INIT_VAL	(QT3_BB_MIMO_BF_RX_INIT_VAL)
#endif

/* should be 64 bytes aligned (expmat ptr in TxVector drops lower 5 bits)*/
struct phys_qmat_layout {
	uint64_t length;
	int8_t body[0];
} __packed;

// TODO: describe bodies as 2 dimensional arrays
struct phys_qmat_1x1 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_2STRM_80M];
} __packed;

struct phys_qmat_2x1 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_3STRM_80M];
} __packed;

struct phys_qmat_3x1 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_4STRM_80M];
} __packed;

struct phys_qmat_1x2 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_3STRM_80M];
} __packed;

struct phys_qmat_1x3 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_4STRM_80M];
} __packed;

struct phys_qmat_2x2 {
	uint64_t length;
	int8_t body[STVEC_SIZE_BYTES_4STRM_80M];
} __packed;

struct phys_qmat_dummy {
	uint64_t length;
	int8_t body[0x10];
} __packed;

/* structure to hold MU group qmatrix info */
struct qtn_sram_qmat {
	uint8_t valid;   /* set to 1 when it is occupied, 0 indicates it is free */
	uint8_t grp_id;  /* MU group id */
	uint16_t tk;	/* Token */
	uint16_t u0_aid;  /* user position 0 AID */
	uint16_t u1_aid;  /* user position 1 AID */
	int32_t rank;
/* Number of following Q matrix elements */
#define MU_QMAT_ELEM_NUM	6
	/* matrix starting addresses in sram */
	uint32_t u0_1ss_u1_1ss;
	uint32_t u0_2ss_u1_1ss;
	uint32_t u0_3ss_u1_1ss;
	uint32_t u0_1ss_u1_2ss;
	uint32_t u0_1ss_u1_3ss;
	uint32_t u0_2ss_u1_2ss;
	uint32_t dsp_cnt;
} __packed;

struct qtn_grp_rank {
	uint16_t u0_aid;
	uint16_t u1_aid;
	int32_t rank;
} __packed;

#define TXBF_MAX_NC 4
#define TXBF_MAX_NR 4
#define TXBF_MAX_NG 3
#define TXBF_MAX_BW 4	/* Multiple of 20 MHz channels */

#define TXBF_EXPMAT_TYPE_0_BYPASS 0
#define TXBF_EXPMAT_TYPE_2_QMEM_MODE 2
#define TXBF_EXPMAT_TYPE_5_QREG_MODE 5

#define QTN_TXBF_QMAT_STD_1SS_TONE_DATA	{0x00400040, 0x00400040, 0x00400040, 0x00400040}
#define QTN_TXBF_QMAT_STD_2SS_TONE_DATA	{0x002D002D, 0x2D00002D, 0x00D3002D, 0xD300002D, \
					0x002D002D, 0x2D00002D, 0x00D3002D, 0xD300002D}
#define QTN_TXBF_QMAT_STD_3SS_TONE_DATA	{0x00250025, 0x00250025, 0x00DB2500, 0x00DB0025, \
					0x00250025, 0x00DBDB00, 0x00250025, 0x00250025, \
					0x00DB2500, 0x00DB0025, 0x00250025, 0x00DBDB00}
#define QTN_TXBF_QMAT_STD_4SS_TONE_DATA	{0x00000040, 0x00000000, 0x00400000, 0x00000000, \
					0x00000000, 0x00000040, 0x00000000, 0x00400000, \
					0x00000040, 0x00000000, 0x00400000, 0x00000000, \
					0x00000000, 0x00000040, 0x00000000, 0x00400000}

static __inline__ uint8_t qtn_txbf_get_bf_qmat_offsets(uint32_t *expmat_ss, uint8_t max,
		uint32_t qmat_base_offset, uint8_t node_bw, uint8_t bw, uint8_t ng)
{
	if ((bw == QTN_BW_40M) && (node_bw == QTN_BW_80M)) {
		if (ng == QTN_TXBF_DEFAULT_QMAT_NG) {
			expmat_ss[0] = QTN_TXBF_QMAT80_1STRM_40M_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_2STRM_40M_OFFSET(qmat_base_offset);
		} else {
			expmat_ss[0] = QTN_TXBF_QMAT80_NG2_1STRM_40M_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_NG2_2STRM_40M_OFFSET(qmat_base_offset);
		}
		return (QTN_MAX_40M_VHT_STREAMS - 1);
	} else if ((bw == QTN_BW_20M) && (node_bw > QTN_BW_20M)) {
		if ((node_bw == QTN_BW_80M) && (ng == QTN_TXBF_DEFAULT_QMAT_NG)) {
			expmat_ss[0] = QTN_TXBF_QMAT80_1STRM_20M_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_2STRM_20M_OFFSET(qmat_base_offset);
		} else if (node_bw == QTN_BW_80M) {
			expmat_ss[0] = QTN_TXBF_QMAT80_NG2_1STRM_20M_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_NG2_2STRM_20M_OFFSET(qmat_base_offset);
		} else {
			expmat_ss[0] = QTN_TXBF_QMAT40_1STRM_20M_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT40_2STRM_20M_OFFSET(qmat_base_offset);
		}
		return (QTN_MAX_20M_VHT_STREAMS - 1);
	}
	if (node_bw == QTN_BW_80M) {
		if (ng == QTN_TXBF_DEFAULT_QMAT_NG) {
			expmat_ss[0] = QTN_TXBF_QMAT80_1STRM_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_2STRM_OFFSET(qmat_base_offset);
		} else {
			expmat_ss[0] = QTN_TXBF_QMAT80_NG2_1STRM_OFFSET(qmat_base_offset);
			expmat_ss[1] = QTN_TXBF_QMAT80_NG2_2STRM_OFFSET(qmat_base_offset);
		}
		if ((max == QTN_MAX_STREAMS) && (ng == QTN_TXBF_DEFAULT_QMAT_NG)) {
			expmat_ss[2] = QTN_TXBF_QMAT80_3STRM_OFFSET(qmat_base_offset);
			expmat_ss[3] = QTN_TXBF_QMAT80_4STRM_OFFSET(qmat_base_offset);
		} else if (max == QTN_MAX_STREAMS) {
			expmat_ss[2] = QTN_TXBF_QMAT80_NG2_3STRM_OFFSET(qmat_base_offset);
			expmat_ss[3] = QTN_TXBF_QMAT80_NG2_4STRM_OFFSET(qmat_base_offset);
		}
	} else {
		expmat_ss[0] = QTN_TXBF_QMAT40_1STRM_OFFSET(qmat_base_offset);
		expmat_ss[1] = QTN_TXBF_QMAT40_2STRM_OFFSET(qmat_base_offset);
		if (max == QTN_MAX_STREAMS) {
			expmat_ss[2] = QTN_TXBF_QMAT40_3STRM_OFFSET(qmat_base_offset);
			expmat_ss[3] = QTN_TXBF_QMAT40_4STRM_OFFSET(qmat_base_offset);
		}
	}

	return (QTN_MAX_STREAMS - 1);
}

unsigned dsp_rt_hash(volatile struct qtn_rate_train_info *p_rate_info);

#endif /*TXBF_COMMON_H_*/

