/*
 * Copyright 2012 Broadcom Corporation
 *
 * This program is the proprietary software of Broadcom Corporation and/or
 * its licensors, and may only be used, duplicated, modified or distributed
 * pursuant to the terms and conditions of a separate, written license
 * agreement executed between you and Broadcom (an "Authorized License").
 * Except as set forth in an Authorized License, Broadcom grants no license
 * (express or implied), right to use, or waiver of any kind with respect to
 * the Software, and Broadcom expressly reserves all rights in and to the
 * Software and all intellectual property rights therein.  IF YOU HAVE NO
 * AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY,
 * AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE
 * SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 *
 * 1. This program, including its structure, sequence and organization,
 * constitutes the valuable trade secrets of Broadcom, and you shall use
 * all reasonable efforts to protect the confidentiality thereof, and to
 * use this information only in connection with your use of Broadcom
 * integrated circuit products.
 *
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED
 * "AS IS" AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES,
 * REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR
 * OTHERWISE, WITH RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS
 * ANY AND ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT,
 * FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS,
 * QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU
 * ASSUME THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
 *
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR
 * ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL,
 * INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY
 * RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM
 * HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN
 * EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1,
 * WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY
 * FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *
 * $Id$
 */


#ifndef	_bcm5301xotp_h_
#define	_bcm5301xotp_h_
/*
struct bcmstrbuf {
	char *buf;
	unsigned int size;
	char *origbuf;
	unsigned int origsize;
};
*/

/* DMU register definition */
#define  DMU_OTP_CPU_CTRL0                                            0x1800c240
#define  DMU_OTP_CPU_CTRL0_BASE                                            0x240
#define  DMU_OTP_CPU_CTRL0_DATAMASK                                   0xffffffff
#define  DMU_OTP_CPU_CTRL0_RDWRMASK                                   0x00000000
#define  DMU_OTP_CPU_CTRL1                                            0x1800c244
#define  DMU_OTP_CPU_CTRL1_BASE                                            0x244
#define  DMU_OTP_CPU_CTRL1_DATAMASK                                   0xffffffff
#define  DMU_OTP_CPU_CTRL1_RDWRMASK                                   0x00000000
#define  DMU_OTP_CPU_ADDR                                             0x1800c24c
#define  DMU_OTP_CPU_ADDR_BASE                                             0x24c
#define  DMU_OTP_CPU_ADDR_DATAMASK                                    0x0000ffff
#define  DMU_OTP_CPU_ADDR_RDWRMASK                                    0x00000000
#define  DMU_OTP_CPU_BITSEL                                           0x1800c250
#define  DMU_OTP_CPU_BITSEL_BASE                                           0x250
#define  DMU_OTP_CPU_BITSEL_DATAMASK                                  0x0000ffff
#define  DMU_OTP_CPU_BITSEL_RDWRMASK                                  0x00000000
#define  DMU_OTP_CPU_WRITE_DATA                                       0x1800c254
#define  DMU_OTP_CPU_WRITE_DATA_BASE                                       0x254
#define  DMU_OTP_CPU_WRITE_DATA_DATAMASK                              0xffffffff
#define  DMU_OTP_CPU_WRITE_DATA_RDWRMASK                              0x00000000
#define  DMU_OTP_CPU_CONFIG                                           0x1800c258
#define  DMU_OTP_CPU_CONFIG_BASE                                           0x258
#define  DMU_OTP_CPU_CONFIG_DATAMASK                                  0x00000003
#define  DMU_OTP_CPU_CONFIG_RDWRMASK                                  0x00000000
#define  DMU_OTP_CPU_READ_DATA                                        0x1800c25c
#define  DMU_OTP_CPU_READ_DATA_BASE                                        0x25c
#define  DMU_OTP_CPU_READ_DATA_DATAMASK                               0xffffffff
#define  DMU_OTP_CPU_READ_DATA_RDWRMASK                               0x00000000
#define  DMU_OTP_CPU_STS                                              0x1800c260
#define  DMU_OTP_CPU_STS_BASE                                              0x260
#define  DMU_OTP_CPU_STS_DATAMASK                                     0x0000ffff
#define  DMU_OTP_CPU_STS_RDWRMASK                                     0x00000000

/* fields in DMU_OTP_CPU_CTRL1 */
#define OTPCPU_CTRL1_START_MASK		0x00000001
#define OTPCPU_CTRL1_START_SHIFT	0
#define OTPCPU_CTRL1_CMD_MASK		0x0000003e
#define OTPCPU_CTRL1_CMD_SHIFT		1
#define OTPCPU_CTRL1_2XFUSE_MASK	0x00001000
#define OTPCPU_CTRL1_2XFUSE_SHIFT	12
#define OTPCPU_CTRL1_COF_MASK		0x00080000
#define OTPCPU_CTRL1_COF_SHIFT		19
#define OTPCPU_CTRL1_PROG_EN_MASK		0x00200000
#define OTPCPU_CTRL1_PROG_EN_SHIFT		21
#define OTPCPU_CTRL1_ACCESS_MODE_MASK		0x00c00000
#define OTPCPU_CTRL1_ACCESS_MODE_SHIFT		22

/* Opcodes for OTPP_OC field */
#define OTPCPU_CMD_READ		0
#define OTPCPU_CMD_PROG_EN		1
#define OTPCPU_CMD_PROG_DIS		2
#define OTPCPU_CMD_BIT_PROG		10
#define OTPCPU_CMD_WORD_PROG		11

/* fields in DMU_OTP_CPU_CONFIG */
#define OTPCPU_CFG_CPU_MODE_MASK		0x00000001
#define OTPCPU_CFG_CPU_MODE_SHIFT		0
#define OTPCPU_CFG_CPU_DISABLE_OTP_ACCESS_MASK		0x00000002
#define OTPCPU_CFG_CPU_DISABLE_OTP_ACCESS_SHIFT		1

/* fields in DMU_OTP_CPU_STS */
#define OTPCPU_STS_CMD_DONE_MASK		0x00000001
#define OTPCPU_STS_CMD_DONE_SHIFT		0
#define OTPCPU_STS_PROG_OK_MASK		0x00001000
#define OTPCPU_STS_PROG_OK_SHIFT		12

#endif /* _bcm5301xotp_h_ */
