/******************************************************************************
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2007 - 2010 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110,
 * USA
 *
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * Contact Information:
 *  Intel Linux Wireless <ilw@linux.intel.com>
 * Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497
 *
 * BSD LICENSE
 *
 * Copyright(c) 2005 - 2010 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name Intel Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 * Please use this file (iwl-agn-hw.h) only for hardware-related definitions.
 */

#ifndef __iwl_agn_hw_h__
#define __iwl_agn_hw_h__

#define IWLAGN_RTC_INST_LOWER_BOUND		(0x000000)
#define IWLAGN_RTC_INST_UPPER_BOUND		(0x020000)

#define IWLAGN_RTC_DATA_LOWER_BOUND		(0x800000)
#define IWLAGN_RTC_DATA_UPPER_BOUND		(0x80C000)

#define IWLAGN_RTC_INST_SIZE (IWLAGN_RTC_INST_UPPER_BOUND - \
				IWLAGN_RTC_INST_LOWER_BOUND)
#define IWLAGN_RTC_DATA_SIZE (IWLAGN_RTC_DATA_UPPER_BOUND - \
				IWLAGN_RTC_DATA_LOWER_BOUND)

/* RSSI to dBm */
#define IWLAGN_RSSI_OFFSET	44

/* PCI registers */
#define PCI_CFG_RETRY_TIMEOUT	0x041

/* PCI register values */
#define PCI_CFG_LINK_CTRL_VAL_L0S_EN	0x01
#define PCI_CFG_LINK_CTRL_VAL_L1_EN	0x02

#define IWLAGN_DEFAULT_TX_RETRY  15

/* Limit range of txpower output target to be between these values */
#define IWLAGN_TX_POWER_TARGET_POWER_MIN	(0)	/* 0 dBm: 1 milliwatt */
#define IWLAGN_TX_POWER_TARGET_POWER_MAX	(16)	/* 16 dBm */

/* EEPROM */
#define IWLAGN_EEPROM_IMG_SIZE		2048

#define IWLAGN_CMD_FIFO_NUM		7
#define IWLAGN_NUM_QUEUES		20
#define IWLAGN_NUM_AMPDU_QUEUES		10
#define IWLAGN_FIRST_AMPDU_QUEUE	10

/* Fixed (non-configurable) rx data from phy */

/**
 * struct iwlagn_schedq_bc_tbl scheduler byte count table
 *	base physical address provided by SCD_DRAM_BASE_ADDR
 * @tfd_offset  0-12 - tx command byte count
 *	       12-16 - station index
 */
struct iwlagn_scd_bc_tbl {
	__le16 tfd_offset[TFD_QUEUE_BC_SIZE];
} __packed;


#endif /* __iwl_agn_hw_h__ */
