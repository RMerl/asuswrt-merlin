/*
 * PMC-Sierra SPC 8001 SAS/SATA based host adapters driver
 *
 * Copyright (c) 2008-2009 USI Co., Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 */
 #include <linux/slab.h>
 #include "pm8001_sas.h"
 #include "pm8001_hwi.h"
 #include "pm8001_chips.h"
 #include "pm8001_ctl.h"

/**
 * read_main_config_table - read the configure table and save it.
 * @pm8001_ha: our hba card information
 */
static void __devinit read_main_config_table(struct pm8001_hba_info *pm8001_ha)
{
	void __iomem *address = pm8001_ha->main_cfg_tbl_addr;
	pm8001_ha->main_cfg_tbl.signature	= pm8001_mr32(address, 0x00);
	pm8001_ha->main_cfg_tbl.interface_rev	= pm8001_mr32(address, 0x04);
	pm8001_ha->main_cfg_tbl.firmware_rev	= pm8001_mr32(address, 0x08);
	pm8001_ha->main_cfg_tbl.max_out_io	= pm8001_mr32(address, 0x0C);
	pm8001_ha->main_cfg_tbl.max_sgl		= pm8001_mr32(address, 0x10);
	pm8001_ha->main_cfg_tbl.ctrl_cap_flag	= pm8001_mr32(address, 0x14);
	pm8001_ha->main_cfg_tbl.gst_offset	= pm8001_mr32(address, 0x18);
	pm8001_ha->main_cfg_tbl.inbound_queue_offset =
		pm8001_mr32(address, MAIN_IBQ_OFFSET);
	pm8001_ha->main_cfg_tbl.outbound_queue_offset =
		pm8001_mr32(address, MAIN_OBQ_OFFSET);
	pm8001_ha->main_cfg_tbl.hda_mode_flag	=
		pm8001_mr32(address, MAIN_HDA_FLAGS_OFFSET);

	/* read analog Setting offset from the configuration table */
	pm8001_ha->main_cfg_tbl.anolog_setup_table_offset =
		pm8001_mr32(address, MAIN_ANALOG_SETUP_OFFSET);

	/* read Error Dump Offset and Length */
	pm8001_ha->main_cfg_tbl.fatal_err_dump_offset0 =
		pm8001_mr32(address, MAIN_FATAL_ERROR_RDUMP0_OFFSET);
	pm8001_ha->main_cfg_tbl.fatal_err_dump_length0 =
		pm8001_mr32(address, MAIN_FATAL_ERROR_RDUMP0_LENGTH);
	pm8001_ha->main_cfg_tbl.fatal_err_dump_offset1 =
		pm8001_mr32(address, MAIN_FATAL_ERROR_RDUMP1_OFFSET);
	pm8001_ha->main_cfg_tbl.fatal_err_dump_length1 =
		pm8001_mr32(address, MAIN_FATAL_ERROR_RDUMP1_LENGTH);
}

/**
 * read_general_status_table - read the general status table and save it.
 * @pm8001_ha: our hba card information
 */
static void __devinit
read_general_status_table(struct pm8001_hba_info *pm8001_ha)
{
	void __iomem *address = pm8001_ha->general_stat_tbl_addr;
	pm8001_ha->gs_tbl.gst_len_mpistate	= pm8001_mr32(address, 0x00);
	pm8001_ha->gs_tbl.iq_freeze_state0	= pm8001_mr32(address, 0x04);
	pm8001_ha->gs_tbl.iq_freeze_state1	= pm8001_mr32(address, 0x08);
	pm8001_ha->gs_tbl.msgu_tcnt		= pm8001_mr32(address, 0x0C);
	pm8001_ha->gs_tbl.iop_tcnt		= pm8001_mr32(address, 0x10);
	pm8001_ha->gs_tbl.reserved		= pm8001_mr32(address, 0x14);
	pm8001_ha->gs_tbl.phy_state[0]	= pm8001_mr32(address, 0x18);
	pm8001_ha->gs_tbl.phy_state[1]	= pm8001_mr32(address, 0x1C);
	pm8001_ha->gs_tbl.phy_state[2]	= pm8001_mr32(address, 0x20);
	pm8001_ha->gs_tbl.phy_state[3]	= pm8001_mr32(address, 0x24);
	pm8001_ha->gs_tbl.phy_state[4]	= pm8001_mr32(address, 0x28);
	pm8001_ha->gs_tbl.phy_state[5]	= pm8001_mr32(address, 0x2C);
	pm8001_ha->gs_tbl.phy_state[6]	= pm8001_mr32(address, 0x30);
	pm8001_ha->gs_tbl.phy_state[7]	= pm8001_mr32(address, 0x34);
	pm8001_ha->gs_tbl.reserved1		= pm8001_mr32(address, 0x38);
	pm8001_ha->gs_tbl.reserved2		= pm8001_mr32(address, 0x3C);
	pm8001_ha->gs_tbl.reserved3		= pm8001_mr32(address, 0x40);
	pm8001_ha->gs_tbl.recover_err_info[0]	= pm8001_mr32(address, 0x44);
	pm8001_ha->gs_tbl.recover_err_info[1]	= pm8001_mr32(address, 0x48);
	pm8001_ha->gs_tbl.recover_err_info[2]	= pm8001_mr32(address, 0x4C);
	pm8001_ha->gs_tbl.recover_err_info[3]	= pm8001_mr32(address, 0x50);
	pm8001_ha->gs_tbl.recover_err_info[4]	= pm8001_mr32(address, 0x54);
	pm8001_ha->gs_tbl.recover_err_info[5]	= pm8001_mr32(address, 0x58);
	pm8001_ha->gs_tbl.recover_err_info[6]	= pm8001_mr32(address, 0x5C);
	pm8001_ha->gs_tbl.recover_err_info[7]	= pm8001_mr32(address, 0x60);
}

/**
 * read_inbnd_queue_table - read the inbound queue table and save it.
 * @pm8001_ha: our hba card information
 */
static void __devinit
read_inbnd_queue_table(struct pm8001_hba_info *pm8001_ha)
{
	int inbQ_num = 1;
	int i;
	void __iomem *address = pm8001_ha->inbnd_q_tbl_addr;
	for (i = 0; i < inbQ_num; i++) {
		u32 offset = i * 0x20;
		pm8001_ha->inbnd_q_tbl[i].pi_pci_bar =
		      get_pci_bar_index(pm8001_mr32(address, (offset + 0x14)));
		pm8001_ha->inbnd_q_tbl[i].pi_offset =
			pm8001_mr32(address, (offset + 0x18));
	}
}

/**
 * read_outbnd_queue_table - read the outbound queue table and save it.
 * @pm8001_ha: our hba card information
 */
static void __devinit
read_outbnd_queue_table(struct pm8001_hba_info *pm8001_ha)
{
	int outbQ_num = 1;
	int i;
	void __iomem *address = pm8001_ha->outbnd_q_tbl_addr;
	for (i = 0; i < outbQ_num; i++) {
		u32 offset = i * 0x24;
		pm8001_ha->outbnd_q_tbl[i].ci_pci_bar =
		      get_pci_bar_index(pm8001_mr32(address, (offset + 0x14)));
		pm8001_ha->outbnd_q_tbl[i].ci_offset =
			pm8001_mr32(address, (offset + 0x18));
	}
}

/**
 * init_default_table_values - init the default table.
 * @pm8001_ha: our hba card information
 */
static void __devinit
init_default_table_values(struct pm8001_hba_info *pm8001_ha)
{
	int qn = 1;
	int i;
	u32 offsetib, offsetob;
	void __iomem *addressib = pm8001_ha->inbnd_q_tbl_addr;
	void __iomem *addressob = pm8001_ha->outbnd_q_tbl_addr;

	pm8001_ha->main_cfg_tbl.inbound_q_nppd_hppd			= 0;
	pm8001_ha->main_cfg_tbl.outbound_hw_event_pid0_3 		= 0;
	pm8001_ha->main_cfg_tbl.outbound_hw_event_pid4_7		= 0;
	pm8001_ha->main_cfg_tbl.outbound_ncq_event_pid0_3		= 0;
	pm8001_ha->main_cfg_tbl.outbound_ncq_event_pid4_7		= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_ITNexus_event_pid0_3	= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_ITNexus_event_pid4_7	= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_ssp_event_pid0_3	= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_ssp_event_pid4_7	= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_smp_event_pid0_3	= 0;
	pm8001_ha->main_cfg_tbl.outbound_tgt_smp_event_pid4_7	= 0;

	pm8001_ha->main_cfg_tbl.upper_event_log_addr		=
		pm8001_ha->memoryMap.region[AAP1].phys_addr_hi;
	pm8001_ha->main_cfg_tbl.lower_event_log_addr		=
		pm8001_ha->memoryMap.region[AAP1].phys_addr_lo;
	pm8001_ha->main_cfg_tbl.event_log_size	= PM8001_EVENT_LOG_SIZE;
	pm8001_ha->main_cfg_tbl.event_log_option		= 0x01;
	pm8001_ha->main_cfg_tbl.upper_iop_event_log_addr	=
		pm8001_ha->memoryMap.region[IOP].phys_addr_hi;
	pm8001_ha->main_cfg_tbl.lower_iop_event_log_addr	=
		pm8001_ha->memoryMap.region[IOP].phys_addr_lo;
	pm8001_ha->main_cfg_tbl.iop_event_log_size	= PM8001_EVENT_LOG_SIZE;
	pm8001_ha->main_cfg_tbl.iop_event_log_option		= 0x01;
	pm8001_ha->main_cfg_tbl.fatal_err_interrupt		= 0x01;
	for (i = 0; i < qn; i++) {
		pm8001_ha->inbnd_q_tbl[i].element_pri_size_cnt	=
			0x00000100 | (0x00000040 << 16) | (0x00<<30);
		pm8001_ha->inbnd_q_tbl[i].upper_base_addr	=
			pm8001_ha->memoryMap.region[IB].phys_addr_hi;
		pm8001_ha->inbnd_q_tbl[i].lower_base_addr	=
		pm8001_ha->memoryMap.region[IB].phys_addr_lo;
		pm8001_ha->inbnd_q_tbl[i].base_virt		=
			(u8 *)pm8001_ha->memoryMap.region[IB].virt_ptr;
		pm8001_ha->inbnd_q_tbl[i].total_length		=
			pm8001_ha->memoryMap.region[IB].total_len;
		pm8001_ha->inbnd_q_tbl[i].ci_upper_base_addr	=
			pm8001_ha->memoryMap.region[CI].phys_addr_hi;
		pm8001_ha->inbnd_q_tbl[i].ci_lower_base_addr	=
			pm8001_ha->memoryMap.region[CI].phys_addr_lo;
		pm8001_ha->inbnd_q_tbl[i].ci_virt		=
			pm8001_ha->memoryMap.region[CI].virt_ptr;
		offsetib = i * 0x20;
		pm8001_ha->inbnd_q_tbl[i].pi_pci_bar		=
			get_pci_bar_index(pm8001_mr32(addressib,
				(offsetib + 0x14)));
		pm8001_ha->inbnd_q_tbl[i].pi_offset		=
			pm8001_mr32(addressib, (offsetib + 0x18));
		pm8001_ha->inbnd_q_tbl[i].producer_idx		= 0;
		pm8001_ha->inbnd_q_tbl[i].consumer_index	= 0;
	}
	for (i = 0; i < qn; i++) {
		pm8001_ha->outbnd_q_tbl[i].element_size_cnt	=
			256 | (64 << 16) | (1<<30);
		pm8001_ha->outbnd_q_tbl[i].upper_base_addr	=
			pm8001_ha->memoryMap.region[OB].phys_addr_hi;
		pm8001_ha->outbnd_q_tbl[i].lower_base_addr	=
			pm8001_ha->memoryMap.region[OB].phys_addr_lo;
		pm8001_ha->outbnd_q_tbl[i].base_virt		=
			(u8 *)pm8001_ha->memoryMap.region[OB].virt_ptr;
		pm8001_ha->outbnd_q_tbl[i].total_length		=
			pm8001_ha->memoryMap.region[OB].total_len;
		pm8001_ha->outbnd_q_tbl[i].pi_upper_base_addr	=
			pm8001_ha->memoryMap.region[PI].phys_addr_hi;
		pm8001_ha->outbnd_q_tbl[i].pi_lower_base_addr	=
			pm8001_ha->memoryMap.region[PI].phys_addr_lo;
		pm8001_ha->outbnd_q_tbl[i].interrup_vec_cnt_delay	=
			0 | (10 << 16) | (0 << 24);
		pm8001_ha->outbnd_q_tbl[i].pi_virt		=
			pm8001_ha->memoryMap.region[PI].virt_ptr;
		offsetob = i * 0x24;
		pm8001_ha->outbnd_q_tbl[i].ci_pci_bar		=
			get_pci_bar_index(pm8001_mr32(addressob,
			offsetob + 0x14));
		pm8001_ha->outbnd_q_tbl[i].ci_offset		=
			pm8001_mr32(addressob, (offsetob + 0x18));
		pm8001_ha->outbnd_q_tbl[i].consumer_idx		= 0;
		pm8001_ha->outbnd_q_tbl[i].producer_index	= 0;
	}
}

/**
 * update_main_config_table - update the main default table to the HBA.
 * @pm8001_ha: our hba card information
 */
static void __devinit
update_main_config_table(struct pm8001_hba_info *pm8001_ha)
{
	void __iomem *address = pm8001_ha->main_cfg_tbl_addr;
	pm8001_mw32(address, 0x24,
		pm8001_ha->main_cfg_tbl.inbound_q_nppd_hppd);
	pm8001_mw32(address, 0x28,
		pm8001_ha->main_cfg_tbl.outbound_hw_event_pid0_3);
	pm8001_mw32(address, 0x2C,
		pm8001_ha->main_cfg_tbl.outbound_hw_event_pid4_7);
	pm8001_mw32(address, 0x30,
		pm8001_ha->main_cfg_tbl.outbound_ncq_event_pid0_3);
	pm8001_mw32(address, 0x34,
		pm8001_ha->main_cfg_tbl.outbound_ncq_event_pid4_7);
	pm8001_mw32(address, 0x38,
		pm8001_ha->main_cfg_tbl.outbound_tgt_ITNexus_event_pid0_3);
	pm8001_mw32(address, 0x3C,
		pm8001_ha->main_cfg_tbl.outbound_tgt_ITNexus_event_pid4_7);
	pm8001_mw32(address, 0x40,
		pm8001_ha->main_cfg_tbl.outbound_tgt_ssp_event_pid0_3);
	pm8001_mw32(address, 0x44,
		pm8001_ha->main_cfg_tbl.outbound_tgt_ssp_event_pid4_7);
	pm8001_mw32(address, 0x48,
		pm8001_ha->main_cfg_tbl.outbound_tgt_smp_event_pid0_3);
	pm8001_mw32(address, 0x4C,
		pm8001_ha->main_cfg_tbl.outbound_tgt_smp_event_pid4_7);
	pm8001_mw32(address, 0x50,
		pm8001_ha->main_cfg_tbl.upper_event_log_addr);
	pm8001_mw32(address, 0x54,
		pm8001_ha->main_cfg_tbl.lower_event_log_addr);
	pm8001_mw32(address, 0x58, pm8001_ha->main_cfg_tbl.event_log_size);
	pm8001_mw32(address, 0x5C, pm8001_ha->main_cfg_tbl.event_log_option);
	pm8001_mw32(address, 0x60,
		pm8001_ha->main_cfg_tbl.upper_iop_event_log_addr);
	pm8001_mw32(address, 0x64,
		pm8001_ha->main_cfg_tbl.lower_iop_event_log_addr);
	pm8001_mw32(address, 0x68, pm8001_ha->main_cfg_tbl.iop_event_log_size);
	pm8001_mw32(address, 0x6C,
		pm8001_ha->main_cfg_tbl.iop_event_log_option);
	pm8001_mw32(address, 0x70,
		pm8001_ha->main_cfg_tbl.fatal_err_interrupt);
}

/**
 * update_inbnd_queue_table - update the inbound queue table to the HBA.
 * @pm8001_ha: our hba card information
 */
static void __devinit
update_inbnd_queue_table(struct pm8001_hba_info *pm8001_ha, int number)
{
	void __iomem *address = pm8001_ha->inbnd_q_tbl_addr;
	u16 offset = number * 0x20;
	pm8001_mw32(address, offset + 0x00,
		pm8001_ha->inbnd_q_tbl[number].element_pri_size_cnt);
	pm8001_mw32(address, offset + 0x04,
		pm8001_ha->inbnd_q_tbl[number].upper_base_addr);
	pm8001_mw32(address, offset + 0x08,
		pm8001_ha->inbnd_q_tbl[number].lower_base_addr);
	pm8001_mw32(address, offset + 0x0C,
		pm8001_ha->inbnd_q_tbl[number].ci_upper_base_addr);
	pm8001_mw32(address, offset + 0x10,
		pm8001_ha->inbnd_q_tbl[number].ci_lower_base_addr);
}

/**
 * update_outbnd_queue_table - update the outbound queue table to the HBA.
 * @pm8001_ha: our hba card information
 */
static void __devinit
update_outbnd_queue_table(struct pm8001_hba_info *pm8001_ha, int number)
{
	void __iomem *address = pm8001_ha->outbnd_q_tbl_addr;
	u16 offset = number * 0x24;
	pm8001_mw32(address, offset + 0x00,
		pm8001_ha->outbnd_q_tbl[number].element_size_cnt);
	pm8001_mw32(address, offset + 0x04,
		pm8001_ha->outbnd_q_tbl[number].upper_base_addr);
	pm8001_mw32(address, offset + 0x08,
		pm8001_ha->outbnd_q_tbl[number].lower_base_addr);
	pm8001_mw32(address, offset + 0x0C,
		pm8001_ha->outbnd_q_tbl[number].pi_upper_base_addr);
	pm8001_mw32(address, offset + 0x10,
		pm8001_ha->outbnd_q_tbl[number].pi_lower_base_addr);
	pm8001_mw32(address, offset + 0x1C,
		pm8001_ha->outbnd_q_tbl[number].interrup_vec_cnt_delay);
}

/**
 * bar4_shift - function is called to shift BAR base address
 * @pm8001_ha : our hba card information
 * @shiftValue : shifting value in memory bar.
 */
static int bar4_shift(struct pm8001_hba_info *pm8001_ha, u32 shiftValue)
{
	u32 regVal;
	u32 max_wait_count;

	/* program the inbound AXI translation Lower Address */
	pm8001_cw32(pm8001_ha, 1, SPC_IBW_AXI_TRANSLATION_LOW, shiftValue);

	/* confirm the setting is written */
	max_wait_count = 1 * 1000 * 1000;  /* 1 sec */
	do {
		udelay(1);
		regVal = pm8001_cr32(pm8001_ha, 1, SPC_IBW_AXI_TRANSLATION_LOW);
	} while ((regVal != shiftValue) && (--max_wait_count));

	if (!max_wait_count) {
		PM8001_INIT_DBG(pm8001_ha,
			pm8001_printk("TIMEOUT:SPC_IBW_AXI_TRANSLATION_LOW"
			" = 0x%x\n", regVal));
		return -1;
	}
	return 0;
}

/**
 * mpi_set_phys_g3_with_ssc
 * @pm8001_ha: our hba card information
 * @SSCbit: set SSCbit to 0 to disable all phys ssc; 1 to enable all phys ssc.
 */
static void __devinit
mpi_set_phys_g3_with_ssc(struct pm8001_hba_info *pm8001_ha, u32 SSCbit)
{
	u32 value, offset, i;

#define SAS2_SETTINGS_LOCAL_PHY_0_3_SHIFT_ADDR 0x00030000
#define SAS2_SETTINGS_LOCAL_PHY_4_7_SHIFT_ADDR 0x00040000
#define SAS2_SETTINGS_LOCAL_PHY_0_3_OFFSET 0x1074
#define SAS2_SETTINGS_LOCAL_PHY_4_7_OFFSET 0x1074
#define PHY_G3_WITHOUT_SSC_BIT_SHIFT 12
#define PHY_G3_WITH_SSC_BIT_SHIFT 13
#define SNW3_PHY_CAPABILITIES_PARITY 31

   /*
    * Using shifted destination address 0x3_0000:0x1074 + 0x4000*N (N=0:3)
    * Using shifted destination address 0x4_0000:0x1074 + 0x4000*(N-4) (N=4:7)
    */
	if (-1 == bar4_shift(pm8001_ha, SAS2_SETTINGS_LOCAL_PHY_0_3_SHIFT_ADDR))
		return;

	for (i = 0; i < 4; i++) {
		offset = SAS2_SETTINGS_LOCAL_PHY_0_3_OFFSET + 0x4000 * i;
		pm8001_cw32(pm8001_ha, 2, offset, 0x80001501);
	}
	/* shift membase 3 for SAS2_SETTINGS_LOCAL_PHY 4 - 7 */
	if (-1 == bar4_shift(pm8001_ha, SAS2_SETTINGS_LOCAL_PHY_4_7_SHIFT_ADDR))
		return;
	for (i = 4; i < 8; i++) {
		offset = SAS2_SETTINGS_LOCAL_PHY_4_7_OFFSET + 0x4000 * (i-4);
		pm8001_cw32(pm8001_ha, 2, offset, 0x80001501);
	}
	/*************************************************************
	Change the SSC upspreading value to 0x0 so that upspreading is disabled.
	Device MABC SMOD0 Controls
	Address: (via MEMBASE-III):
	Using shifted destination address 0x0_0000: with Offset 0xD8

	31:28 R/W Reserved Do not change
	27:24 R/W SAS_SMOD_SPRDUP 0000
	23:20 R/W SAS_SMOD_SPRDDN 0000
	19:0  R/W  Reserved Do not change
	Upon power-up this register will read as 0x8990c016,
	and I would like you to change the SAS_SMOD_SPRDUP bits to 0b0000
	so that the written value will be 0x8090c016.
	This will ensure only down-spreading SSC is enabled on the SPC.
	*************************************************************/
	value = pm8001_cr32(pm8001_ha, 2, 0xd8);
	pm8001_cw32(pm8001_ha, 2, 0xd8, 0x8000C016);

	/*set the shifted destination address to 0x0 to avoid error operation */
	bar4_shift(pm8001_ha, 0x0);
	return;
}

/**
 * mpi_set_open_retry_interval_reg
 * @pm8001_ha: our hba card information
 * @interval - interval time for each OPEN_REJECT (RETRY). The units are in 1us.
 */
static void __devinit
mpi_set_open_retry_interval_reg(struct pm8001_hba_info *pm8001_ha,
				u32 interval)
{
	u32 offset;
	u32 value;
	u32 i;

#define OPEN_RETRY_INTERVAL_PHY_0_3_SHIFT_ADDR 0x00030000
#define OPEN_RETRY_INTERVAL_PHY_4_7_SHIFT_ADDR 0x00040000
#define OPEN_RETRY_INTERVAL_PHY_0_3_OFFSET 0x30B4
#define OPEN_RETRY_INTERVAL_PHY_4_7_OFFSET 0x30B4
#define OPEN_RETRY_INTERVAL_REG_MASK 0x0000FFFF

	value = interval & OPEN_RETRY_INTERVAL_REG_MASK;
	/* shift bar and set the OPEN_REJECT(RETRY) interval time of PHY 0 -3.*/
	if (-1 == bar4_shift(pm8001_ha,
			     OPEN_RETRY_INTERVAL_PHY_0_3_SHIFT_ADDR))
		return;
	for (i = 0; i < 4; i++) {
		offset = OPEN_RETRY_INTERVAL_PHY_0_3_OFFSET + 0x4000 * i;
		pm8001_cw32(pm8001_ha, 2, offset, value);
	}

	if (-1 == bar4_shift(pm8001_ha,
			     OPEN_RETRY_INTERVAL_PHY_4_7_SHIFT_ADDR))
		return;
	for (i = 4; i < 8; i++) {
		offset = OPEN_RETRY_INTERVAL_PHY_4_7_OFFSET + 0x4000 * (i-4);
		pm8001_cw32(pm8001_ha, 2, offset, value);
	}
	/*set the shifted destination address to 0x0 to avoid error operation */
	bar4_shift(pm8001_ha, 0x0);
	return;
}

/**
 * mpi_init_check - check firmware initialization status.
 * @pm8001_ha: our hba card information
 */
static int mpi_init_check(struct pm8001_hba_info *pm8001_ha)
{
	u32 max_wait_count;
	u32 value;
	u32 gst_len_mpistate;
	/* Write bit0=1 to Inbound DoorBell Register to tell the SPC FW the
	table is updated */
	pm8001_cw32(pm8001_ha, 0, MSGU_IBDB_SET, SPC_MSGU_CFG_TABLE_UPDATE);
	/* wait until Inbound DoorBell Clear Register toggled */
	max_wait_count = 1 * 1000 * 1000;/* 1 sec */
	do {
		udelay(1);
		value = pm8001_cr32(pm8001_ha, 0, MSGU_IBDB_SET);
		value &= SPC_MSGU_CFG_TABLE_UPDATE;
	} while ((value != 0) && (--max_wait_count));

	if (!max_wait_count)
		return -1;
	/* check the MPI-State for initialization */
	gst_len_mpistate =
		pm8001_mr32(pm8001_ha->general_stat_tbl_addr,
		GST_GSTLEN_MPIS_OFFSET);
	if (GST_MPI_STATE_INIT != (gst_len_mpistate & GST_MPI_STATE_MASK))
		return -1;
	/* check MPI Initialization error */
	gst_len_mpistate = gst_len_mpistate >> 16;
	if (0x0000 != gst_len_mpistate)
		return -1;
	return 0;
}

/**
 * check_fw_ready - The LLDD check if the FW is ready, if not, return error.
 * @pm8001_ha: our hba card information
 */
static int check_fw_ready(struct pm8001_hba_info *pm8001_ha)
{
	u32 value, value1;
	u32 max_wait_count;
	/* check error state */
	value = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1);
	value1 = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2);
	/* check AAP error */
	if (SCRATCH_PAD1_ERR == (value & SCRATCH_PAD_STATE_MASK)) {
		/* error state */
		value = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_0);
		return -1;
	}

	/* check IOP error */
	if (SCRATCH_PAD2_ERR == (value1 & SCRATCH_PAD_STATE_MASK)) {
		/* error state */
		value1 = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_3);
		return -1;
	}

	/* bit 4-31 of scratch pad1 should be zeros if it is not
	in error state*/
	if (value & SCRATCH_PAD1_STATE_MASK) {
		/* error case */
		pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_0);
		return -1;
	}

	/* bit 2, 4-31 of scratch pad2 should be zeros if it is not
	in error state */
	if (value1 & SCRATCH_PAD2_STATE_MASK) {
		/* error case */
		return -1;
	}

	max_wait_count = 1 * 1000 * 1000;/* 1 sec timeout */

	/* wait until scratch pad 1 and 2 registers in ready state  */
	do {
		udelay(1);
		value = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1)
			& SCRATCH_PAD1_RDY;
		value1 = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2)
			& SCRATCH_PAD2_RDY;
		if ((--max_wait_count) == 0)
			return -1;
	} while ((value != SCRATCH_PAD1_RDY) || (value1 != SCRATCH_PAD2_RDY));
	return 0;
}

static void init_pci_device_addresses(struct pm8001_hba_info *pm8001_ha)
{
	void __iomem *base_addr;
	u32	value;
	u32	offset;
	u32	pcibar;
	u32	pcilogic;

	value = pm8001_cr32(pm8001_ha, 0, 0x44);
	offset = value & 0x03FFFFFF;
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("Scratchpad 0 Offset: %x \n", offset));
	pcilogic = (value & 0xFC000000) >> 26;
	pcibar = get_pci_bar_index(pcilogic);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("Scratchpad 0 PCI BAR: %d \n", pcibar));
	pm8001_ha->main_cfg_tbl_addr = base_addr =
		pm8001_ha->io_mem[pcibar].memvirtaddr + offset;
	pm8001_ha->general_stat_tbl_addr =
		base_addr + pm8001_cr32(pm8001_ha, pcibar, offset + 0x18);
	pm8001_ha->inbnd_q_tbl_addr =
		base_addr + pm8001_cr32(pm8001_ha, pcibar, offset + 0x1C);
	pm8001_ha->outbnd_q_tbl_addr =
		base_addr + pm8001_cr32(pm8001_ha, pcibar, offset + 0x20);
}

/**
 * pm8001_chip_init - the main init function that initialize whole PM8001 chip.
 * @pm8001_ha: our hba card information
 */
static int __devinit pm8001_chip_init(struct pm8001_hba_info *pm8001_ha)
{
	/* check the firmware status */
	if (-1 == check_fw_ready(pm8001_ha)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Firmware is not ready!\n"));
		return -EBUSY;
	}

	/* Initialize pci space address eg: mpi offset */
	init_pci_device_addresses(pm8001_ha);
	init_default_table_values(pm8001_ha);
	read_main_config_table(pm8001_ha);
	read_general_status_table(pm8001_ha);
	read_inbnd_queue_table(pm8001_ha);
	read_outbnd_queue_table(pm8001_ha);
	/* update main config table ,inbound table and outbound table */
	update_main_config_table(pm8001_ha);
	update_inbnd_queue_table(pm8001_ha, 0);
	update_outbnd_queue_table(pm8001_ha, 0);
	mpi_set_phys_g3_with_ssc(pm8001_ha, 0);
	mpi_set_open_retry_interval_reg(pm8001_ha, 7);
	/* notify firmware update finished and check initialization status */
	if (0 == mpi_init_check(pm8001_ha)) {
		PM8001_INIT_DBG(pm8001_ha,
			pm8001_printk("MPI initialize successful!\n"));
	} else
		return -EBUSY;
	/*This register is a 16-bit timer with a resolution of 1us. This is the
	timer used for interrupt delay/coalescing in the PCIe Application Layer.
	Zero is not a valid value. A value of 1 in the register will cause the
	interrupts to be normal. A value greater than 1 will cause coalescing
	delays.*/
	pm8001_cw32(pm8001_ha, 1, 0x0033c0, 0x1);
	pm8001_cw32(pm8001_ha, 1, 0x0033c4, 0x0);
	return 0;
}

static int mpi_uninit_check(struct pm8001_hba_info *pm8001_ha)
{
	u32 max_wait_count;
	u32 value;
	u32 gst_len_mpistate;
	init_pci_device_addresses(pm8001_ha);
	/* Write bit1=1 to Inbound DoorBell Register to tell the SPC FW the
	table is stop */
	pm8001_cw32(pm8001_ha, 0, MSGU_IBDB_SET, SPC_MSGU_CFG_TABLE_RESET);

	/* wait until Inbound DoorBell Clear Register toggled */
	max_wait_count = 1 * 1000 * 1000;/* 1 sec */
	do {
		udelay(1);
		value = pm8001_cr32(pm8001_ha, 0, MSGU_IBDB_SET);
		value &= SPC_MSGU_CFG_TABLE_RESET;
	} while ((value != 0) && (--max_wait_count));

	if (!max_wait_count) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("TIMEOUT:IBDB value/=0x%x\n", value));
		return -1;
	}

	/* check the MPI-State for termination in progress */
	/* wait until Inbound DoorBell Clear Register toggled */
	max_wait_count = 1 * 1000 * 1000;  /* 1 sec */
	do {
		udelay(1);
		gst_len_mpistate =
			pm8001_mr32(pm8001_ha->general_stat_tbl_addr,
			GST_GSTLEN_MPIS_OFFSET);
		if (GST_MPI_STATE_UNINIT ==
			(gst_len_mpistate & GST_MPI_STATE_MASK))
			break;
	} while (--max_wait_count);
	if (!max_wait_count) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk(" TIME OUT MPI State = 0x%x\n",
				gst_len_mpistate & GST_MPI_STATE_MASK));
		return -1;
	}
	return 0;
}

/**
 * soft_reset_ready_check - Function to check FW is ready for soft reset.
 * @pm8001_ha: our hba card information
 */
static u32 soft_reset_ready_check(struct pm8001_hba_info *pm8001_ha)
{
	u32 regVal, regVal1, regVal2;
	if (mpi_uninit_check(pm8001_ha) != 0) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("MPI state is not ready\n"));
		return -1;
	}
	/* read the scratch pad 2 register bit 2 */
	regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2)
		& SCRATCH_PAD2_FWRDY_RST;
	if (regVal == SCRATCH_PAD2_FWRDY_RST) {
		PM8001_INIT_DBG(pm8001_ha,
			pm8001_printk("Firmware is ready for reset .\n"));
	} else {
	/* Trigger NMI twice via RB6 */
		if (-1 == bar4_shift(pm8001_ha, RB6_ACCESS_REG)) {
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("Shift Bar4 to 0x%x failed\n",
					RB6_ACCESS_REG));
			return -1;
		}
		pm8001_cw32(pm8001_ha, 2, SPC_RB6_OFFSET,
			RB6_MAGIC_NUMBER_RST);
		pm8001_cw32(pm8001_ha, 2, SPC_RB6_OFFSET, RB6_MAGIC_NUMBER_RST);
		/* wait for 100 ms */
		mdelay(100);
		regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2) &
			SCRATCH_PAD2_FWRDY_RST;
		if (regVal != SCRATCH_PAD2_FWRDY_RST) {
			regVal1 = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1);
			regVal2 = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2);
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("TIMEOUT:MSGU_SCRATCH_PAD1"
				"=0x%x, MSGU_SCRATCH_PAD2=0x%x\n",
				regVal1, regVal2));
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD0 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_0)));
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD3 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_3)));
			return -1;
		}
	}
	return 0;
}

/**
 * pm8001_chip_soft_rst - soft reset the PM8001 chip, so that the clear all
 * the FW register status to the originated status.
 * @pm8001_ha: our hba card information
 * @signature: signature in host scratch pad0 register.
 */
static int
pm8001_chip_soft_rst(struct pm8001_hba_info *pm8001_ha, u32 signature)
{
	u32	regVal, toggleVal;
	u32	max_wait_count;
	u32	regVal1, regVal2, regVal3;

	/* step1: Check FW is ready for soft reset */
	if (soft_reset_ready_check(pm8001_ha) != 0) {
		PM8001_FAIL_DBG(pm8001_ha, pm8001_printk("FW is not ready\n"));
		return -1;
	}

	/* step 2: clear NMI status register on AAP1 and IOP, write the same
	value to clear */
	/* map 0x60000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, MBIC_AAP1_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Shift Bar4 to 0x%x failed\n",
			MBIC_AAP1_ADDR_BASE));
		return -1;
	}
	regVal = pm8001_cr32(pm8001_ha, 2, MBIC_NMI_ENABLE_VPE0_IOP);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("MBIC - NMI Enable VPE0 (IOP)= 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 2, MBIC_NMI_ENABLE_VPE0_IOP, 0x0);
	/* map 0x70000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, MBIC_IOP_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Shift Bar4 to 0x%x failed\n",
			MBIC_IOP_ADDR_BASE));
		return -1;
	}
	regVal = pm8001_cr32(pm8001_ha, 2, MBIC_NMI_ENABLE_VPE0_AAP1);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("MBIC - NMI Enable VPE0 (AAP1)= 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 2, MBIC_NMI_ENABLE_VPE0_AAP1, 0x0);

	regVal = pm8001_cr32(pm8001_ha, 1, PCIE_EVENT_INTERRUPT_ENABLE);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("PCIE -Event Interrupt Enable = 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 1, PCIE_EVENT_INTERRUPT_ENABLE, 0x0);

	regVal = pm8001_cr32(pm8001_ha, 1, PCIE_EVENT_INTERRUPT);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("PCIE - Event Interrupt  = 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 1, PCIE_EVENT_INTERRUPT, regVal);

	regVal = pm8001_cr32(pm8001_ha, 1, PCIE_ERROR_INTERRUPT_ENABLE);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("PCIE -Error Interrupt Enable = 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 1, PCIE_ERROR_INTERRUPT_ENABLE, 0x0);

	regVal = pm8001_cr32(pm8001_ha, 1, PCIE_ERROR_INTERRUPT);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("PCIE - Error Interrupt = 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 1, PCIE_ERROR_INTERRUPT, regVal);

	/* read the scratch pad 1 register bit 2 */
	regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1)
		& SCRATCH_PAD1_RST;
	toggleVal = regVal ^ SCRATCH_PAD1_RST;

	/* set signature in host scratch pad0 register to tell SPC that the
	host performs the soft reset */
	pm8001_cw32(pm8001_ha, 0, MSGU_HOST_SCRATCH_PAD_0, signature);

	/* read required registers for confirmming */
	/* map 0x0700000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, GSM_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Shift Bar4 to 0x%x failed\n",
			GSM_ADDR_BASE));
		return -1;
	}
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x0(0x00007b88)-GSM Configuration and"
		" Reset = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET)));

	/* step 3: host read GSM Configuration and Reset register */
	regVal = pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET);
	/* Put those bits to low */
	/* GSM XCBI offset = 0x70 0000
	0x00 Bit 13 COM_SLV_SW_RSTB 1
	0x00 Bit 12 QSSP_SW_RSTB 1
	0x00 Bit 11 RAAE_SW_RSTB 1
	0x00 Bit 9 RB_1_SW_RSTB 1
	0x00 Bit 8 SM_SW_RSTB 1
	*/
	regVal &= ~(0x00003b00);
	/* host write GSM Configuration and Reset register */
	pm8001_cw32(pm8001_ha, 2, GSM_CONFIG_RESET, regVal);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x0 (0x00007b88 ==> 0x00004088) - GSM "
		"Configuration and Reset is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET)));

	/* step 4: */
	/* disable GSM - Read Address Parity Check */
	regVal1 = pm8001_cr32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700038 - Read Address Parity Check "
		"Enable = 0x%x\n", regVal1));
	pm8001_cw32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK, 0x0);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700038 - Read Address Parity Check Enable"
		"is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK)));

	/* disable GSM - Write Address Parity Check */
	regVal2 = pm8001_cr32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700040 - Write Address Parity Check"
		" Enable = 0x%x\n", regVal2));
	pm8001_cw32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK, 0x0);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700040 - Write Address Parity Check "
		"Enable is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK)));

	/* disable GSM - Write Data Parity Check */
	regVal3 = pm8001_cr32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x300048 - Write Data Parity Check"
		" Enable = 0x%x\n", regVal3));
	pm8001_cw32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK, 0x0);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x300048 - Write Data Parity Check Enable"
		"is set to = 0x%x\n",
	pm8001_cr32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK)));

	/* step 5: delay 10 usec */
	udelay(10);
	/* step 5-b: set GPIO-0 output control to tristate anyway */
	if (-1 == bar4_shift(pm8001_ha, GPIO_ADDR_BASE)) {
		PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("Shift Bar4 to 0x%x failed\n",
				GPIO_ADDR_BASE));
		return -1;
	}
	regVal = pm8001_cr32(pm8001_ha, 2, GPIO_GPIO_0_0UTPUT_CTL_OFFSET);
		PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("GPIO Output Control Register:"
				" = 0x%x\n", regVal));
	/* set GPIO-0 output control to tri-state */
	regVal &= 0xFFFFFFFC;
	pm8001_cw32(pm8001_ha, 2, GPIO_GPIO_0_0UTPUT_CTL_OFFSET, regVal);

	/* Step 6: Reset the IOP and AAP1 */
	/* map 0x00000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, SPC_TOP_LEVEL_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("SPC Shift Bar4 to 0x%x failed\n",
			SPC_TOP_LEVEL_ADDR_BASE));
		return -1;
	}
	regVal = pm8001_cr32(pm8001_ha, 2, SPC_REG_RESET);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("Top Register before resetting IOP/AAP1"
		":= 0x%x\n", regVal));
	regVal &= ~(SPC_REG_RESET_PCS_IOP_SS | SPC_REG_RESET_PCS_AAP1_SS);
	pm8001_cw32(pm8001_ha, 2, SPC_REG_RESET, regVal);

	/* step 7: Reset the BDMA/OSSP */
	regVal = pm8001_cr32(pm8001_ha, 2, SPC_REG_RESET);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("Top Register before resetting BDMA/OSSP"
		": = 0x%x\n", regVal));
	regVal &= ~(SPC_REG_RESET_BDMA_CORE | SPC_REG_RESET_OSSP);
	pm8001_cw32(pm8001_ha, 2, SPC_REG_RESET, regVal);

	/* step 8: delay 10 usec */
	udelay(10);

	/* step 9: bring the BDMA and OSSP out of reset */
	regVal = pm8001_cr32(pm8001_ha, 2, SPC_REG_RESET);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("Top Register before bringing up BDMA/OSSP"
		":= 0x%x\n", regVal));
	regVal |= (SPC_REG_RESET_BDMA_CORE | SPC_REG_RESET_OSSP);
	pm8001_cw32(pm8001_ha, 2, SPC_REG_RESET, regVal);

	/* step 10: delay 10 usec */
	udelay(10);

	/* step 11: reads and sets the GSM Configuration and Reset Register */
	/* map 0x0700000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, GSM_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("SPC Shift Bar4 to 0x%x failed\n",
			GSM_ADDR_BASE));
		return -1;
	}
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x0 (0x00007b88)-GSM Configuration and "
		"Reset = 0x%x\n", pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET)));
	regVal = pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET);
	/* Put those bits to high */
	/* GSM XCBI offset = 0x70 0000
	0x00 Bit 13 COM_SLV_SW_RSTB 1
	0x00 Bit 12 QSSP_SW_RSTB 1
	0x00 Bit 11 RAAE_SW_RSTB 1
	0x00 Bit 9   RB_1_SW_RSTB 1
	0x00 Bit 8   SM_SW_RSTB 1
	*/
	regVal |= (GSM_CONFIG_RESET_VALUE);
	pm8001_cw32(pm8001_ha, 2, GSM_CONFIG_RESET, regVal);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM (0x00004088 ==> 0x00007b88) - GSM"
		" Configuration and Reset is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_CONFIG_RESET)));

	/* step 12: Restore GSM - Read Address Parity Check */
	regVal = pm8001_cr32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK);
	/* just for debugging */
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700038 - Read Address Parity Check Enable"
		" = 0x%x\n", regVal));
	pm8001_cw32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK, regVal1);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700038 - Read Address Parity"
		" Check Enable is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_READ_ADDR_PARITY_CHECK)));
	/* Restore GSM - Write Address Parity Check */
	regVal = pm8001_cr32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK);
	pm8001_cw32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK, regVal2);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700040 - Write Address Parity Check"
		" Enable is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_WRITE_ADDR_PARITY_CHECK)));
	/* Restore GSM - Write Data Parity Check */
	regVal = pm8001_cr32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK);
	pm8001_cw32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK, regVal3);
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("GSM 0x700048 - Write Data Parity Check Enable"
		"is set to = 0x%x\n",
		pm8001_cr32(pm8001_ha, 2, GSM_WRITE_DATA_PARITY_CHECK)));

	/* step 13: bring the IOP and AAP1 out of reset */
	/* map 0x00000 to BAR4(0x20), BAR2(win) */
	if (-1 == bar4_shift(pm8001_ha, SPC_TOP_LEVEL_ADDR_BASE)) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Shift Bar4 to 0x%x failed\n",
			SPC_TOP_LEVEL_ADDR_BASE));
		return -1;
	}
	regVal = pm8001_cr32(pm8001_ha, 2, SPC_REG_RESET);
	regVal |= (SPC_REG_RESET_PCS_IOP_SS | SPC_REG_RESET_PCS_AAP1_SS);
	pm8001_cw32(pm8001_ha, 2, SPC_REG_RESET, regVal);

	/* step 14: delay 10 usec - Normal Mode */
	udelay(10);
	/* check Soft Reset Normal mode or Soft Reset HDA mode */
	if (signature == SPC_SOFT_RESET_SIGNATURE) {
		/* step 15 (Normal Mode): wait until scratch pad1 register
		bit 2 toggled */
		max_wait_count = 2 * 1000 * 1000;/* 2 sec */
		do {
			udelay(1);
			regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1) &
				SCRATCH_PAD1_RST;
		} while ((regVal != toggleVal) && (--max_wait_count));

		if (!max_wait_count) {
			regVal = pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_1);
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("TIMEOUT : ToggleVal 0x%x,"
				"MSGU_SCRATCH_PAD1 = 0x%x\n",
				toggleVal, regVal));
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD0 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_0)));
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD2 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_2)));
			PM8001_FAIL_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD3 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_3)));
			return -1;
		}

		/* step 16 (Normal) - Clear ODMR and ODCR */
		pm8001_cw32(pm8001_ha, 0, MSGU_ODCR, ODCR_CLEAR_ALL);
		pm8001_cw32(pm8001_ha, 0, MSGU_ODMR, ODMR_CLEAR_ALL);

		/* step 17 (Normal Mode): wait for the FW and IOP to get
		ready - 1 sec timeout */
		/* Wait for the SPC Configuration Table to be ready */
		if (check_fw_ready(pm8001_ha) == -1) {
			regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_1);
			/* return error if MPI Configuration Table not ready */
			PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("FW not ready SCRATCH_PAD1"
				" = 0x%x\n", regVal));
			regVal = pm8001_cr32(pm8001_ha, 0, MSGU_SCRATCH_PAD_2);
			/* return error if MPI Configuration Table not ready */
			PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("FW not ready SCRATCH_PAD2"
				" = 0x%x\n", regVal));
			PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD0 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_0)));
			PM8001_INIT_DBG(pm8001_ha,
				pm8001_printk("SCRATCH_PAD3 value = 0x%x\n",
				pm8001_cr32(pm8001_ha, 0,
				MSGU_SCRATCH_PAD_3)));
			return -1;
		}
	}

	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("SPC soft reset Complete\n"));
	return 0;
}

static void pm8001_hw_chip_rst(struct pm8001_hba_info *pm8001_ha)
{
	u32 i;
	u32 regVal;
	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("chip reset start\n"));

	/* do SPC chip reset. */
	regVal = pm8001_cr32(pm8001_ha, 1, SPC_REG_RESET);
	regVal &= ~(SPC_REG_RESET_DEVICE);
	pm8001_cw32(pm8001_ha, 1, SPC_REG_RESET, regVal);

	/* delay 10 usec */
	udelay(10);

	/* bring chip reset out of reset */
	regVal = pm8001_cr32(pm8001_ha, 1, SPC_REG_RESET);
	regVal |= SPC_REG_RESET_DEVICE;
	pm8001_cw32(pm8001_ha, 1, SPC_REG_RESET, regVal);

	/* delay 10 usec */
	udelay(10);

	/* wait for 20 msec until the firmware gets reloaded */
	i = 20;
	do {
		mdelay(1);
	} while ((--i) != 0);

	PM8001_INIT_DBG(pm8001_ha,
		pm8001_printk("chip reset finished\n"));
}

/**
 * pm8001_chip_iounmap - which maped when initialized.
 * @pm8001_ha: our hba card information
 */
static void pm8001_chip_iounmap(struct pm8001_hba_info *pm8001_ha)
{
	s8 bar, logical = 0;
	for (bar = 0; bar < 6; bar++) {
		/*
		** logical BARs for SPC:
		** bar 0 and 1 - logical BAR0
		** bar 2 and 3 - logical BAR1
		** bar4 - logical BAR2
		** bar5 - logical BAR3
		** Skip the appropriate assignments:
		*/
		if ((bar == 1) || (bar == 3))
			continue;
		if (pm8001_ha->io_mem[logical].memvirtaddr) {
			iounmap(pm8001_ha->io_mem[logical].memvirtaddr);
			logical++;
		}
	}
}

/**
 * pm8001_chip_interrupt_enable - enable PM8001 chip interrupt
 * @pm8001_ha: our hba card information
 */
static void
pm8001_chip_intx_interrupt_enable(struct pm8001_hba_info *pm8001_ha)
{
	pm8001_cw32(pm8001_ha, 0, MSGU_ODMR, ODMR_CLEAR_ALL);
	pm8001_cw32(pm8001_ha, 0, MSGU_ODCR, ODCR_CLEAR_ALL);
}

 /**
  * pm8001_chip_intx_interrupt_disable- disable PM8001 chip interrupt
  * @pm8001_ha: our hba card information
  */
static void
pm8001_chip_intx_interrupt_disable(struct pm8001_hba_info *pm8001_ha)
{
	pm8001_cw32(pm8001_ha, 0, MSGU_ODMR, ODMR_MASK_ALL);
}

/**
 * pm8001_chip_msix_interrupt_enable - enable PM8001 chip interrupt
 * @pm8001_ha: our hba card information
 */
static void
pm8001_chip_msix_interrupt_enable(struct pm8001_hba_info *pm8001_ha,
	u32 int_vec_idx)
{
	u32 msi_index;
	u32 value;
	msi_index = int_vec_idx * MSIX_TABLE_ELEMENT_SIZE;
	msi_index += MSIX_TABLE_BASE;
	pm8001_cw32(pm8001_ha, 0, msi_index, MSIX_INTERRUPT_ENABLE);
	value = (1 << int_vec_idx);
	pm8001_cw32(pm8001_ha, 0,  MSGU_ODCR, value);

}

/**
 * pm8001_chip_msix_interrupt_disable - disable PM8001 chip interrupt
 * @pm8001_ha: our hba card information
 */
static void
pm8001_chip_msix_interrupt_disable(struct pm8001_hba_info *pm8001_ha,
	u32 int_vec_idx)
{
	u32 msi_index;
	msi_index = int_vec_idx * MSIX_TABLE_ELEMENT_SIZE;
	msi_index += MSIX_TABLE_BASE;
	pm8001_cw32(pm8001_ha, 0,  msi_index, MSIX_INTERRUPT_DISABLE);

}
/**
 * pm8001_chip_interrupt_enable - enable PM8001 chip interrupt
 * @pm8001_ha: our hba card information
 */
static void
pm8001_chip_interrupt_enable(struct pm8001_hba_info *pm8001_ha)
{
#ifdef PM8001_USE_MSIX
	pm8001_chip_msix_interrupt_enable(pm8001_ha, 0);
	return;
#endif
	pm8001_chip_intx_interrupt_enable(pm8001_ha);

}

/**
 * pm8001_chip_intx_interrupt_disable- disable PM8001 chip interrupt
 * @pm8001_ha: our hba card information
 */
static void
pm8001_chip_interrupt_disable(struct pm8001_hba_info *pm8001_ha)
{
#ifdef PM8001_USE_MSIX
	pm8001_chip_msix_interrupt_disable(pm8001_ha, 0);
	return;
#endif
	pm8001_chip_intx_interrupt_disable(pm8001_ha);

}

/**
 * mpi_msg_free_get- get the free message buffer for transfer inbound queue.
 * @circularQ: the inbound queue  we want to transfer to HBA.
 * @messageSize: the message size of this transfer, normally it is 64 bytes
 * @messagePtr: the pointer to message.
 */
static int mpi_msg_free_get(struct inbound_queue_table *circularQ,
			    u16 messageSize, void **messagePtr)
{
	u32 offset, consumer_index;
	struct mpi_msg_hdr *msgHeader;
	u8 bcCount = 1; /* only support single buffer */

	/* Checks is the requested message size can be allocated in this queue*/
	if (messageSize > 64) {
		*messagePtr = NULL;
		return -1;
	}

	/* Stores the new consumer index */
	consumer_index = pm8001_read_32(circularQ->ci_virt);
	circularQ->consumer_index = cpu_to_le32(consumer_index);
	if (((circularQ->producer_idx + bcCount) % 256) ==
		circularQ->consumer_index) {
		*messagePtr = NULL;
		return -1;
	}
	/* get memory IOMB buffer address */
	offset = circularQ->producer_idx * 64;
	/* increment to next bcCount element */
	circularQ->producer_idx = (circularQ->producer_idx + bcCount) % 256;
	/* Adds that distance to the base of the region virtual address plus
	the message header size*/
	msgHeader = (struct mpi_msg_hdr *)(circularQ->base_virt	+ offset);
	*messagePtr = ((void *)msgHeader) + sizeof(struct mpi_msg_hdr);
	return 0;
}

/**
 * mpi_build_cmd- build the message queue for transfer, update the PI to FW
 * to tell the fw to get this message from IOMB.
 * @pm8001_ha: our hba card information
 * @circularQ: the inbound queue we want to transfer to HBA.
 * @opCode: the operation code represents commands which LLDD and fw recognized.
 * @payload: the command payload of each operation command.
 */
static int mpi_build_cmd(struct pm8001_hba_info *pm8001_ha,
			 struct inbound_queue_table *circularQ,
			 u32 opCode, void *payload)
{
	u32 Header = 0, hpriority = 0, bc = 1, category = 0x02;
	u32 responseQueue = 0;
	void *pMessage;

	if (mpi_msg_free_get(circularQ, 64, &pMessage) < 0) {
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("No free mpi buffer \n"));
		return -1;
	}
	BUG_ON(!payload);
	/*Copy to the payload*/
	memcpy(pMessage, payload, (64 - sizeof(struct mpi_msg_hdr)));

	/*Build the header*/
	Header = ((1 << 31) | (hpriority << 30) | ((bc & 0x1f) << 24)
		| ((responseQueue & 0x3F) << 16)
		| ((category & 0xF) << 12) | (opCode & 0xFFF));

	pm8001_write_32((pMessage - 4), 0, cpu_to_le32(Header));
	/*Update the PI to the firmware*/
	pm8001_cw32(pm8001_ha, circularQ->pi_pci_bar,
		circularQ->pi_offset, circularQ->producer_idx);
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("after PI= %d CI= %d \n", circularQ->producer_idx,
		circularQ->consumer_index));
	return 0;
}

static u32 mpi_msg_free_set(struct pm8001_hba_info *pm8001_ha, void *pMsg,
			    struct outbound_queue_table *circularQ, u8 bc)
{
	u32 producer_index;
	struct mpi_msg_hdr *msgHeader;
	struct mpi_msg_hdr *pOutBoundMsgHeader;

	msgHeader = (struct mpi_msg_hdr *)(pMsg - sizeof(struct mpi_msg_hdr));
	pOutBoundMsgHeader = (struct mpi_msg_hdr *)(circularQ->base_virt +
				circularQ->consumer_idx * 64);
	if (pOutBoundMsgHeader != msgHeader) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("consumer_idx = %d msgHeader = %p\n",
			circularQ->consumer_idx, msgHeader));

		/* Update the producer index from SPC */
		producer_index = pm8001_read_32(circularQ->pi_virt);
		circularQ->producer_index = cpu_to_le32(producer_index);
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("consumer_idx = %d producer_index = %d"
			"msgHeader = %p\n", circularQ->consumer_idx,
			circularQ->producer_index, msgHeader));
		return 0;
	}
	/* free the circular queue buffer elements associated with the message*/
	circularQ->consumer_idx = (circularQ->consumer_idx + bc) % 256;
	/* update the CI of outbound queue */
	pm8001_cw32(pm8001_ha, circularQ->ci_pci_bar, circularQ->ci_offset,
		circularQ->consumer_idx);
	/* Update the producer index from SPC*/
	producer_index = pm8001_read_32(circularQ->pi_virt);
	circularQ->producer_index = cpu_to_le32(producer_index);
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk(" CI=%d PI=%d\n", circularQ->consumer_idx,
		circularQ->producer_index));
	return 0;
}

/**
 * mpi_msg_consume- get the MPI message from  outbound queue message table.
 * @pm8001_ha: our hba card information
 * @circularQ: the outbound queue  table.
 * @messagePtr1: the message contents of this outbound message.
 * @pBC: the message size.
 */
static u32 mpi_msg_consume(struct pm8001_hba_info *pm8001_ha,
			   struct outbound_queue_table *circularQ,
			   void **messagePtr1, u8 *pBC)
{
	struct mpi_msg_hdr	*msgHeader;
	__le32	msgHeader_tmp;
	u32 header_tmp;
	do {
		/* If there are not-yet-delivered messages ... */
		if (circularQ->producer_index != circularQ->consumer_idx) {
			/*Get the pointer to the circular queue buffer element*/
			msgHeader = (struct mpi_msg_hdr *)
				(circularQ->base_virt +
				circularQ->consumer_idx * 64);
			/* read header */
			header_tmp = pm8001_read_32(msgHeader);
			msgHeader_tmp = cpu_to_le32(header_tmp);
			if (0 != (msgHeader_tmp & 0x80000000)) {
				if (OPC_OUB_SKIP_ENTRY !=
					(msgHeader_tmp & 0xfff)) {
					*messagePtr1 =
						((u8 *)msgHeader) +
						sizeof(struct mpi_msg_hdr);
					*pBC = (u8)((msgHeader_tmp >> 24) &
						0x1f);
					PM8001_IO_DBG(pm8001_ha,
						pm8001_printk(": CI=%d PI=%d "
						"msgHeader=%x\n",
						circularQ->consumer_idx,
						circularQ->producer_index,
						msgHeader_tmp));
					return MPI_IO_STATUS_SUCCESS;
				} else {
					circularQ->consumer_idx =
						(circularQ->consumer_idx +
						((msgHeader_tmp >> 24) & 0x1f))
						% 256;
					msgHeader_tmp = 0;
					pm8001_write_32(msgHeader, 0, 0);
					/* update the CI of outbound queue */
					pm8001_cw32(pm8001_ha,
						circularQ->ci_pci_bar,
						circularQ->ci_offset,
						circularQ->consumer_idx);
				}
			} else {
				circularQ->consumer_idx =
					(circularQ->consumer_idx +
					((msgHeader_tmp >> 24) & 0x1f)) % 256;
				msgHeader_tmp = 0;
				pm8001_write_32(msgHeader, 0, 0);
				/* update the CI of outbound queue */
				pm8001_cw32(pm8001_ha, circularQ->ci_pci_bar,
					circularQ->ci_offset,
					circularQ->consumer_idx);
				return MPI_IO_STATUS_FAIL;
			}
		} else {
			u32 producer_index;
			void *pi_virt = circularQ->pi_virt;
			/* Update the producer index from SPC */
			producer_index = pm8001_read_32(pi_virt);
			circularQ->producer_index = cpu_to_le32(producer_index);
		}
	} while (circularQ->producer_index != circularQ->consumer_idx);
	/* while we don't have any more not-yet-delivered message */
	/* report empty */
	return MPI_IO_STATUS_BUSY;
}

static void pm8001_work_fn(struct work_struct *work)
{
	struct pm8001_work *pw = container_of(work, struct pm8001_work, work);
	struct pm8001_device *pm8001_dev;
	struct domain_device *dev;

	switch (pw->handler) {
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		pm8001_dev = pw->data;
		dev = pm8001_dev->sas_device;
		pm8001_I_T_nexus_reset(dev);
		break;
	case IO_OPEN_CNX_ERROR_STP_RESOURCES_BUSY:
		pm8001_dev = pw->data;
		dev = pm8001_dev->sas_device;
		pm8001_I_T_nexus_reset(dev);
		break;
	case IO_DS_IN_ERROR:
		pm8001_dev = pw->data;
		dev = pm8001_dev->sas_device;
		pm8001_I_T_nexus_reset(dev);
		break;
	case IO_DS_NON_OPERATIONAL:
		pm8001_dev = pw->data;
		dev = pm8001_dev->sas_device;
		pm8001_I_T_nexus_reset(dev);
		break;
	}
	kfree(pw);
}

static int pm8001_handle_event(struct pm8001_hba_info *pm8001_ha, void *data,
			       int handler)
{
	struct pm8001_work *pw;
	int ret = 0;

	pw = kmalloc(sizeof(struct pm8001_work), GFP_ATOMIC);
	if (pw) {
		pw->pm8001_ha = pm8001_ha;
		pw->data = data;
		pw->handler = handler;
		INIT_WORK(&pw->work, pm8001_work_fn);
		queue_work(pm8001_wq, &pw->work);
	} else
		ret = -ENOMEM;

	return ret;
}

/**
 * mpi_ssp_completion- process the event that FW response to the SSP request.
 * @pm8001_ha: our hba card information
 * @piomb: the message contents of this outbound message.
 *
 * When FW has completed a ssp request for example a IO request, after it has
 * filled the SG data with the data, it will trigger this event represent
 * that he has finished the job,please check the coresponding buffer.
 * So we will tell the caller who maybe waiting the result to tell upper layer
 * that the task has been finished.
 */
static void
mpi_ssp_completion(struct pm8001_hba_info *pm8001_ha , void *piomb)
{
	struct sas_task *t;
	struct pm8001_ccb_info *ccb;
	unsigned long flags;
	u32 status;
	u32 param;
	u32 tag;
	struct ssp_completion_resp *psspPayload;
	struct task_status_struct *ts;
	struct ssp_response_iu *iu;
	struct pm8001_device *pm8001_dev;
	psspPayload = (struct ssp_completion_resp *)(piomb + 4);
	status = le32_to_cpu(psspPayload->status);
	tag = le32_to_cpu(psspPayload->tag);
	ccb = &pm8001_ha->ccb_info[tag];
	pm8001_dev = ccb->device;
	param = le32_to_cpu(psspPayload->param);

	t = ccb->task;

	if (status && status != IO_UNDERFLOW)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("sas IO status 0x%x\n", status));
	if (unlikely(!t || !t->lldd_task || !t->dev))
		return;
	ts = &t->task_status;
	switch (status) {
	case IO_SUCCESS:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_SUCCESS"
			",param = %d \n", param));
		if (param == 0) {
			ts->resp = SAS_TASK_COMPLETE;
			ts->stat = SAM_STAT_GOOD;
		} else {
			ts->resp = SAS_TASK_COMPLETE;
			ts->stat = SAS_PROTO_RESPONSE;
			ts->residual = param;
			iu = &psspPayload->ssp_resp_iu;
			sas_ssp_task_response(pm8001_ha->dev, t, iu);
		}
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_ABORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_ABORTED IOMB Tag \n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_ABORTED_TASK;
		break;
	case IO_UNDERFLOW:
		/* SSP Completion with error */
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_UNDERFLOW"
			",param = %d \n", param));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_UNDERRUN;
		ts->residual = param;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_NO_DEVICE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_NO_DEVICE\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_PHY_DOWN;
		break;
	case IO_XFER_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_XFER_ERROR_PHY_NOT_READY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PHY_NOT_READY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_EPROTO;
		break;
	case IO_OPEN_CNX_ERROR_ZONE_VIOLATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_ZONE_VIOLATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		if (!t->uldd_task)
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
		break;
	case IO_OPEN_CNX_ERROR_BAD_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BAD_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_BAD_DEST;
		break;
	case IO_OPEN_CNX_ERROR_CONNECTION_RATE_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_CONNECTION_RATE_"
			"NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_CONN_RATE;
		break;
	case IO_OPEN_CNX_ERROR_WRONG_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_WRONG_DESTINATION\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_WRONG_DEST;
		break;
	case IO_XFER_ERROR_NAK_RECEIVED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_NAK_RECEIVED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_XFER_ERROR_ACK_NAK_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_ACK_NAK_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_ERROR_DMA:
		PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("IO_XFER_ERROR_DMA\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_XFER_OPEN_RETRY_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_OPEN_RETRY_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_XFER_ERROR_OFFSET_MISMATCH:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_OFFSET_MISMATCH\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_PORT_IN_RESET:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_PORT_IN_RESET\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_DS_NON_OPERATIONAL:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_NON_OPERATIONAL\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		if (!t->uldd_task)
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_DS_NON_OPERATIONAL);
		break;
	case IO_DS_IN_RECOVERY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_IN_RECOVERY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_TM_TAG_NOT_FOUND:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_TM_TAG_NOT_FOUND\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_SSP_EXT_IU_ZERO_LEN_ERROR:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_SSP_EXT_IU_ZERO_LEN_ERROR\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	case IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
	default:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("Unknown status 0x%x\n", status));
		/* not allowed case. Therefore, return failed status */
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		break;
	}
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("scsi_status = %x \n ",
		psspPayload->ssp_resp_iu.status));
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	if (unlikely((t->task_state_flags & SAS_TASK_STATE_ABORTED))) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		PM8001_FAIL_DBG(pm8001_ha, pm8001_printk("task 0x%p done with"
			" io_status 0x%x resp 0x%x "
			"stat 0x%x but aborted by upper layer!\n",
			t, status, ts->resp, ts->stat));
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
	} else {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/* in order to force CPU ordering */
		t->task_done(t);
	}
}

/*See the comments for mpi_ssp_completion */
static void mpi_ssp_event(struct pm8001_hba_info *pm8001_ha , void *piomb)
{
	struct sas_task *t;
	unsigned long flags;
	struct task_status_struct *ts;
	struct pm8001_ccb_info *ccb;
	struct pm8001_device *pm8001_dev;
	struct ssp_event_resp *psspPayload =
		(struct ssp_event_resp *)(piomb + 4);
	u32 event = le32_to_cpu(psspPayload->event);
	u32 tag = le32_to_cpu(psspPayload->tag);
	u32 port_id = le32_to_cpu(psspPayload->port_id);
	u32 dev_id = le32_to_cpu(psspPayload->device_id);

	ccb = &pm8001_ha->ccb_info[tag];
	t = ccb->task;
	pm8001_dev = ccb->device;
	if (event)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("sas IO status 0x%x\n", event));
	if (unlikely(!t || !t->lldd_task || !t->dev))
		return;
	ts = &t->task_status;
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("port_id = %x,device_id = %x\n",
		port_id, dev_id));
	switch (event) {
	case IO_OVERFLOW:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_UNDERFLOW\n");)
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		ts->residual = 0;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_XFER_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_INTERRUPTED;
		break;
	case IO_XFER_ERROR_PHY_NOT_READY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PHY_NOT_READY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_PROTOCOL_NOT"
			"_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_EPROTO;
		break;
	case IO_OPEN_CNX_ERROR_ZONE_VIOLATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_ZONE_VIOLATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		if (!t->uldd_task)
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
		break;
	case IO_OPEN_CNX_ERROR_BAD_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BAD_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_BAD_DEST;
		break;
	case IO_OPEN_CNX_ERROR_CONNECTION_RATE_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_CONNECTION_RATE_"
			"NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_CONN_RATE;
		break;
	case IO_OPEN_CNX_ERROR_WRONG_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_OPEN_CNX_ERROR_WRONG_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_WRONG_DEST;
		break;
	case IO_XFER_ERROR_NAK_RECEIVED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_NAK_RECEIVED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_XFER_ERROR_ACK_NAK_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_ACK_NAK_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_OPEN_RETRY_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_OPEN_RETRY_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_XFER_ERROR_UNEXPECTED_PHASE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_UNEXPECTED_PHASE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_ERROR_XFER_RDY_OVERRUN:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_XFER_RDY_OVERRUN\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_ERROR_XFER_RDY_NOT_EXPECTED:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_XFER_ERROR_XFER_RDY_NOT_EXPECTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_ERROR_CMD_ISSUE_ACK_NAK_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("IO_XFER_ERROR_CMD_ISSUE_ACK_NAK_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_ERROR_OFFSET_MISMATCH:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_OFFSET_MISMATCH\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_ERROR_XFER_ZERO_DATA_LEN:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_XFER_ZERO_DATA_LEN\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	case IO_XFER_CMD_FRAME_ISSUED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("  IO_XFER_CMD_FRAME_ISSUED\n"));
		return;
	default:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("Unknown status 0x%x\n", event));
		/* not allowed case. Therefore, return failed status */
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		break;
	}
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	if (unlikely((t->task_state_flags & SAS_TASK_STATE_ABORTED))) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		PM8001_FAIL_DBG(pm8001_ha, pm8001_printk("task 0x%p done with"
			" event 0x%x resp 0x%x "
			"stat 0x%x but aborted by upper layer!\n",
			t, event, ts->resp, ts->stat));
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
	} else {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/* in order to force CPU ordering */
		t->task_done(t);
	}
}

/*See the comments for mpi_ssp_completion */
static void
mpi_sata_completion(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct sas_task *t;
	struct pm8001_ccb_info *ccb;
	unsigned long flags = 0;
	u32 param;
	u32 status;
	u32 tag;
	struct sata_completion_resp *psataPayload;
	struct task_status_struct *ts;
	struct ata_task_resp *resp ;
	u32 *sata_resp;
	struct pm8001_device *pm8001_dev;

	psataPayload = (struct sata_completion_resp *)(piomb + 4);
	status = le32_to_cpu(psataPayload->status);
	tag = le32_to_cpu(psataPayload->tag);

	ccb = &pm8001_ha->ccb_info[tag];
	param = le32_to_cpu(psataPayload->param);
	t = ccb->task;
	ts = &t->task_status;
	pm8001_dev = ccb->device;
	if (status)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("sata IO status 0x%x\n", status));
	if (unlikely(!t || !t->lldd_task || !t->dev))
		return;

	switch (status) {
	case IO_SUCCESS:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_SUCCESS\n"));
		if (param == 0) {
			ts->resp = SAS_TASK_COMPLETE;
			ts->stat = SAM_STAT_GOOD;
		} else {
			u8 len;
			ts->resp = SAS_TASK_COMPLETE;
			ts->stat = SAS_PROTO_RESPONSE;
			ts->residual = param;
			PM8001_IO_DBG(pm8001_ha,
				pm8001_printk("SAS_PROTO_RESPONSE len = %d\n",
				param));
			sata_resp = &psataPayload->sata_resp[0];
			resp = (struct ata_task_resp *)ts->buf;
			if (t->ata_task.dma_xfer == 0 &&
			t->data_dir == PCI_DMA_FROMDEVICE) {
				len = sizeof(struct pio_setup_fis);
				PM8001_IO_DBG(pm8001_ha,
				pm8001_printk("PIO read len = %d\n", len));
			} else if (t->ata_task.use_ncq) {
				len = sizeof(struct set_dev_bits_fis);
				PM8001_IO_DBG(pm8001_ha,
					pm8001_printk("FPDMA len = %d\n", len));
			} else {
				len = sizeof(struct dev_to_host_fis);
				PM8001_IO_DBG(pm8001_ha,
				pm8001_printk("other len = %d\n", len));
			}
			if (SAS_STATUS_BUF_SIZE >= sizeof(*resp)) {
				resp->frame_len = len;
				memcpy(&resp->ending_fis[0], sata_resp, len);
				ts->buf_valid_size = sizeof(*resp);
			} else
				PM8001_IO_DBG(pm8001_ha,
					pm8001_printk("response to large \n"));
		}
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_ABORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_ABORTED IOMB Tag \n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_ABORTED_TASK;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
		/* following cases are to do cases */
	case IO_UNDERFLOW:
		/* SATA Completion with error */
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_UNDERFLOW param = %d\n", param));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_UNDERRUN;
		ts->residual =  param;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_NO_DEVICE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_NO_DEVICE\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_PHY_DOWN;
		break;
	case IO_XFER_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_INTERRUPTED;
		break;
	case IO_XFER_ERROR_PHY_NOT_READY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PHY_NOT_READY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_PROTOCOL_NOT"
			"_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_EPROTO;
		break;
	case IO_OPEN_CNX_ERROR_ZONE_VIOLATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_ZONE_VIOLATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_CONT0;
		break;
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
			ts->resp = SAS_TASK_UNDELIVERED;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/*in order to force CPU ordering*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_OPEN_CNX_ERROR_BAD_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BAD_DESTINATION\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_BAD_DEST;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
			ts->resp = SAS_TASK_UNDELIVERED;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/*ditto*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_OPEN_CNX_ERROR_CONNECTION_RATE_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_CONNECTION_RATE_"
			"NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_CONN_RATE;
		break;
	case IO_OPEN_CNX_ERROR_STP_RESOURCES_BUSY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_STP_RESOURCES"
			"_BUSY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_STP_RESOURCES_BUSY);
			ts->resp = SAS_TASK_UNDELIVERED;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/* ditto*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_OPEN_CNX_ERROR_WRONG_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_OPEN_CNX_ERROR_WRONG_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_WRONG_DEST;
		break;
	case IO_XFER_ERROR_NAK_RECEIVED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_NAK_RECEIVED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_ERROR_ACK_NAK_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_ACK_NAK_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_ERROR_DMA:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_DMA\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_ABORTED_TASK;
		break;
	case IO_XFER_ERROR_SATA_LINK_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_SATA_LINK_TIMEOUT\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	case IO_XFER_ERROR_REJECTED_NCQ_MODE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_REJECTED_NCQ_MODE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_UNDERRUN;
		break;
	case IO_XFER_OPEN_RETRY_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_OPEN_RETRY_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_PORT_IN_RESET:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_PORT_IN_RESET\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	case IO_DS_NON_OPERATIONAL:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_NON_OPERATIONAL\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha, pm8001_dev,
				    IO_DS_NON_OPERATIONAL);
			ts->resp = SAS_TASK_UNDELIVERED;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/*ditto*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_DS_IN_RECOVERY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("  IO_DS_IN_RECOVERY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	case IO_DS_IN_ERROR:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_IN_ERROR\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha, pm8001_dev,
				    IO_DS_IN_ERROR);
			ts->resp = SAS_TASK_UNDELIVERED;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/*ditto*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
	default:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("Unknown status 0x%x\n", status));
		/* not allowed case. Therefore, return failed status */
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	}
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	if (unlikely((t->task_state_flags & SAS_TASK_STATE_ABORTED))) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("task 0x%p done with io_status 0x%x"
			" resp 0x%x stat 0x%x but aborted by upper layer!\n",
			t, status, ts->resp, ts->stat));
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
	} else if (t->uldd_task) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/* ditto */
		spin_unlock_irqrestore(&pm8001_ha->lock, flags);
		t->task_done(t);
		spin_lock_irqsave(&pm8001_ha->lock, flags);
	} else if (!t->uldd_task) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/*ditto*/
		spin_unlock_irqrestore(&pm8001_ha->lock, flags);
		t->task_done(t);
		spin_lock_irqsave(&pm8001_ha->lock, flags);
	}
}

/*See the comments for mpi_ssp_completion */
static void mpi_sata_event(struct pm8001_hba_info *pm8001_ha , void *piomb)
{
	struct sas_task *t;
	unsigned long flags = 0;
	struct task_status_struct *ts;
	struct pm8001_ccb_info *ccb;
	struct pm8001_device *pm8001_dev;
	struct sata_event_resp *psataPayload =
		(struct sata_event_resp *)(piomb + 4);
	u32 event = le32_to_cpu(psataPayload->event);
	u32 tag = le32_to_cpu(psataPayload->tag);
	u32 port_id = le32_to_cpu(psataPayload->port_id);
	u32 dev_id = le32_to_cpu(psataPayload->device_id);

	ccb = &pm8001_ha->ccb_info[tag];
	t = ccb->task;
	pm8001_dev = ccb->device;
	if (event)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("sata IO status 0x%x\n", event));
	if (unlikely(!t || !t->lldd_task || !t->dev))
		return;
	ts = &t->task_status;
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("port_id = %x,device_id = %x\n",
		port_id, dev_id));
	switch (event) {
	case IO_OVERFLOW:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_UNDERFLOW\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		ts->residual = 0;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_XFER_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_INTERRUPTED;
		break;
	case IO_XFER_ERROR_PHY_NOT_READY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PHY_NOT_READY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_PROTOCOL_NOT"
			"_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_EPROTO;
		break;
	case IO_OPEN_CNX_ERROR_ZONE_VIOLATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_ZONE_VIOLATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_CONT0;
		break;
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_DEV_NO_RESPONSE;
		if (!t->uldd_task) {
			pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
			ts->resp = SAS_TASK_COMPLETE;
			ts->stat = SAS_QUEUE_FULL;
			pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
			mb();/*ditto*/
			spin_unlock_irqrestore(&pm8001_ha->lock, flags);
			t->task_done(t);
			spin_lock_irqsave(&pm8001_ha->lock, flags);
			return;
		}
		break;
	case IO_OPEN_CNX_ERROR_BAD_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BAD_DESTINATION\n"));
		ts->resp = SAS_TASK_UNDELIVERED;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_BAD_DEST;
		break;
	case IO_OPEN_CNX_ERROR_CONNECTION_RATE_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_CONNECTION_RATE_"
			"NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_CONN_RATE;
		break;
	case IO_OPEN_CNX_ERROR_WRONG_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_OPEN_CNX_ERROR_WRONG_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_WRONG_DEST;
		break;
	case IO_XFER_ERROR_NAK_RECEIVED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_NAK_RECEIVED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_ERROR_PEER_ABORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PEER_ABORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_NAK_R_ERR;
		break;
	case IO_XFER_ERROR_REJECTED_NCQ_MODE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_REJECTED_NCQ_MODE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_UNDERRUN;
		break;
	case IO_XFER_OPEN_RETRY_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_OPEN_RETRY_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_ERROR_UNEXPECTED_PHASE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_UNEXPECTED_PHASE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_ERROR_XFER_RDY_OVERRUN:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_XFER_RDY_OVERRUN\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_ERROR_XFER_RDY_NOT_EXPECTED:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_XFER_ERROR_XFER_RDY_NOT_EXPECTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_ERROR_OFFSET_MISMATCH:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_OFFSET_MISMATCH\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_ERROR_XFER_ZERO_DATA_LEN:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_XFER_ZERO_DATA_LEN\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	case IO_XFER_CMD_FRAME_ISSUED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_CMD_FRAME_ISSUED\n"));
		break;
	case IO_XFER_PIO_SETUP_ERROR:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_PIO_SETUP_ERROR\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	default:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("Unknown status 0x%x\n", event));
		/* not allowed case. Therefore, return failed status */
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_TO;
		break;
	}
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	if (unlikely((t->task_state_flags & SAS_TASK_STATE_ABORTED))) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("task 0x%p done with io_status 0x%x"
			" resp 0x%x stat 0x%x but aborted by upper layer!\n",
			t, event, ts->resp, ts->stat));
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
	} else if (t->uldd_task) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/* ditto */
		spin_unlock_irqrestore(&pm8001_ha->lock, flags);
		t->task_done(t);
		spin_lock_irqsave(&pm8001_ha->lock, flags);
	} else if (!t->uldd_task) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/*ditto*/
		spin_unlock_irqrestore(&pm8001_ha->lock, flags);
		t->task_done(t);
		spin_lock_irqsave(&pm8001_ha->lock, flags);
	}
}

/*See the comments for mpi_ssp_completion */
static void
mpi_smp_completion(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	u32 param;
	struct sas_task *t;
	struct pm8001_ccb_info *ccb;
	unsigned long flags;
	u32 status;
	u32 tag;
	struct smp_completion_resp *psmpPayload;
	struct task_status_struct *ts;
	struct pm8001_device *pm8001_dev;

	psmpPayload = (struct smp_completion_resp *)(piomb + 4);
	status = le32_to_cpu(psmpPayload->status);
	tag = le32_to_cpu(psmpPayload->tag);

	ccb = &pm8001_ha->ccb_info[tag];
	param = le32_to_cpu(psmpPayload->param);
	t = ccb->task;
	ts = &t->task_status;
	pm8001_dev = ccb->device;
	if (status)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("smp IO status 0x%x\n", status));
	if (unlikely(!t || !t->lldd_task || !t->dev))
		return;

	switch (status) {
	case IO_SUCCESS:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_SUCCESS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAM_STAT_GOOD;
	if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_ABORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_ABORTED IOMB\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_ABORTED_TASK;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_OVERFLOW:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_UNDERFLOW\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DATA_OVERRUN;
		ts->residual = 0;
		if (pm8001_dev)
			pm8001_dev->running_req--;
		break;
	case IO_NO_DEVICE:
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("IO_NO_DEVICE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_PHY_DOWN;
		break;
	case IO_ERROR_HW_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_ERROR_HW_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAM_STAT_BUSY;
		break;
	case IO_XFER_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAM_STAT_BUSY;
		break;
	case IO_XFER_ERROR_PHY_NOT_READY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_PHY_NOT_READY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAM_STAT_BUSY;
		break;
	case IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
		pm8001_printk("IO_OPEN_CNX_ERROR_PROTOCOL_NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_ZONE_VIOLATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_ZONE_VIOLATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		break;
	case IO_OPEN_CNX_ERROR_BREAK:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BREAK\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_CONT0;
		break;
	case IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_UNKNOWN;
		pm8001_handle_event(pm8001_ha,
				pm8001_dev,
				IO_OPEN_CNX_ERROR_IT_NEXUS_LOSS);
		break;
	case IO_OPEN_CNX_ERROR_BAD_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_BAD_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_BAD_DEST;
		break;
	case IO_OPEN_CNX_ERROR_CONNECTION_RATE_NOT_SUPPORTED:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_CONNECTION_RATE_"
			"NOT_SUPPORTED\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_CONN_RATE;
		break;
	case IO_OPEN_CNX_ERROR_WRONG_DESTINATION:
		PM8001_IO_DBG(pm8001_ha,
		       pm8001_printk("IO_OPEN_CNX_ERROR_WRONG_DESTINATION\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_WRONG_DEST;
		break;
	case IO_XFER_ERROR_RX_FRAME:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_ERROR_RX_FRAME\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	case IO_XFER_OPEN_RETRY_TIMEOUT:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_XFER_OPEN_RETRY_TIMEOUT\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_ERROR_INTERNAL_SMP_RESOURCE:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_ERROR_INTERNAL_SMP_RESOURCE\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_QUEUE_FULL;
		break;
	case IO_PORT_IN_RESET:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_PORT_IN_RESET\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_DS_NON_OPERATIONAL:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_NON_OPERATIONAL\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		break;
	case IO_DS_IN_RECOVERY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_DS_IN_RECOVERY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	case IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("IO_OPEN_CNX_ERROR_HW_RESOURCE_BUSY\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_OPEN_REJECT;
		ts->open_rej_reason = SAS_OREJ_RSVD_RETRY;
		break;
	default:
		PM8001_IO_DBG(pm8001_ha,
			pm8001_printk("Unknown status 0x%x\n", status));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAS_DEV_NO_RESPONSE;
		/* not allowed case. Therefore, return failed status */
		break;
	}
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	if (unlikely((t->task_state_flags & SAS_TASK_STATE_ABORTED))) {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		PM8001_FAIL_DBG(pm8001_ha, pm8001_printk("task 0x%p done with"
			" io_status 0x%x resp 0x%x "
			"stat 0x%x but aborted by upper layer!\n",
			t, status, ts->resp, ts->stat));
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
	} else {
		spin_unlock_irqrestore(&t->task_state_lock, flags);
		pm8001_ccb_task_free(pm8001_ha, t, ccb, tag);
		mb();/* in order to force CPU ordering */
		t->task_done(t);
	}
}

static void
mpi_set_dev_state_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct set_dev_state_resp *pPayload =
		(struct set_dev_state_resp *)(piomb + 4);
	u32 tag = le32_to_cpu(pPayload->tag);
	struct pm8001_ccb_info *ccb = &pm8001_ha->ccb_info[tag];
	struct pm8001_device *pm8001_dev = ccb->device;
	u32 status = le32_to_cpu(pPayload->status);
	u32 device_id = le32_to_cpu(pPayload->device_id);
	u8 pds = le32_to_cpu(pPayload->pds_nds) | PDS_BITS;
	u8 nds = le32_to_cpu(pPayload->pds_nds) | NDS_BITS;
	PM8001_MSG_DBG(pm8001_ha, pm8001_printk("Set device id = 0x%x state "
		"from 0x%x to 0x%x status = 0x%x!\n",
		device_id, pds, nds, status));
	complete(pm8001_dev->setds_completion);
	ccb->task = NULL;
	ccb->ccb_tag = 0xFFFFFFFF;
	pm8001_ccb_free(pm8001_ha, tag);
}

static void
mpi_set_nvmd_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct get_nvm_data_resp *pPayload =
		(struct get_nvm_data_resp *)(piomb + 4);
	u32 tag = le32_to_cpu(pPayload->tag);
	struct pm8001_ccb_info *ccb = &pm8001_ha->ccb_info[tag];
	u32 dlen_status = le32_to_cpu(pPayload->dlen_status);
	complete(pm8001_ha->nvmd_completion);
	PM8001_MSG_DBG(pm8001_ha, pm8001_printk("Set nvm data complete!\n"));
	if ((dlen_status & NVMD_STAT) != 0) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Set nvm data error!\n"));
		return;
	}
	ccb->task = NULL;
	ccb->ccb_tag = 0xFFFFFFFF;
	pm8001_ccb_free(pm8001_ha, tag);
}

static void
mpi_get_nvmd_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct fw_control_ex	*fw_control_context;
	struct get_nvm_data_resp *pPayload =
		(struct get_nvm_data_resp *)(piomb + 4);
	u32 tag = le32_to_cpu(pPayload->tag);
	struct pm8001_ccb_info *ccb = &pm8001_ha->ccb_info[tag];
	u32 dlen_status = le32_to_cpu(pPayload->dlen_status);
	u32 ir_tds_bn_dps_das_nvm =
		le32_to_cpu(pPayload->ir_tda_bn_dps_das_nvm);
	void *virt_addr = pm8001_ha->memoryMap.region[NVMD].virt_ptr;
	fw_control_context = ccb->fw_control_context;

	PM8001_MSG_DBG(pm8001_ha, pm8001_printk("Get nvm data complete!\n"));
	if ((dlen_status & NVMD_STAT) != 0) {
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("Get nvm data error!\n"));
		complete(pm8001_ha->nvmd_completion);
		return;
	}

	if (ir_tds_bn_dps_das_nvm & IPMode) {
		/* indirect mode - IR bit set */
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("Get NVMD success, IR=1\n"));
		if ((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == TWI_DEVICE) {
			if (ir_tds_bn_dps_das_nvm == 0x80a80200) {
				memcpy(pm8001_ha->sas_addr,
				      ((u8 *)virt_addr + 4),
				       SAS_ADDR_SIZE);
				PM8001_MSG_DBG(pm8001_ha,
					pm8001_printk("Get SAS address"
					" from VPD successfully!\n"));
			}
		} else if (((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == C_SEEPROM)
			|| ((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == VPD_FLASH) ||
			((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == EXPAN_ROM)) {
				;
		} else if (((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == AAP1_RDUMP)
			|| ((ir_tds_bn_dps_das_nvm & NVMD_TYPE) == IOP_RDUMP)) {
			;
		} else {
			/* Should not be happened*/
			PM8001_MSG_DBG(pm8001_ha,
				pm8001_printk("(IR=1)Wrong Device type 0x%x\n",
				ir_tds_bn_dps_das_nvm));
		}
	} else /* direct mode */{
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("Get NVMD success, IR=0, dataLen=%d\n",
			(dlen_status & NVMD_LEN) >> 24));
	}
	memcpy(fw_control_context->usrAddr,
		pm8001_ha->memoryMap.region[NVMD].virt_ptr,
		fw_control_context->len);
	complete(pm8001_ha->nvmd_completion);
	ccb->task = NULL;
	ccb->ccb_tag = 0xFFFFFFFF;
	pm8001_ccb_free(pm8001_ha, tag);
}

static int mpi_local_phy_ctl(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct local_phy_ctl_resp *pPayload =
		(struct local_phy_ctl_resp *)(piomb + 4);
	u32 status = le32_to_cpu(pPayload->status);
	u32 phy_id = le32_to_cpu(pPayload->phyop_phyid) & ID_BITS;
	u32 phy_op = le32_to_cpu(pPayload->phyop_phyid) & OP_BITS;
	if (status != 0) {
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("%x phy execute %x phy op failed! \n",
			phy_id, phy_op));
	} else
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("%x phy execute %x phy op success! \n",
			phy_id, phy_op));
	return 0;
}

/**
 * pm8001_bytes_dmaed - one of the interface function communication with libsas
 * @pm8001_ha: our hba card information
 * @i: which phy that received the event.
 *
 * when HBA driver received the identify done event or initiate FIS received
 * event(for SATA), it will invoke this function to notify the sas layer that
 * the sas toplogy has formed, please discover the the whole sas domain,
 * while receive a broadcast(change) primitive just tell the sas
 * layer to discover the changed domain rather than the whole domain.
 */
static void pm8001_bytes_dmaed(struct pm8001_hba_info *pm8001_ha, int i)
{
	struct pm8001_phy *phy = &pm8001_ha->phy[i];
	struct asd_sas_phy *sas_phy = &phy->sas_phy;
	struct sas_ha_struct *sas_ha;
	if (!phy->phy_attached)
		return;

	sas_ha = pm8001_ha->sas;
	if (sas_phy->phy) {
		struct sas_phy *sphy = sas_phy->phy;
		sphy->negotiated_linkrate = sas_phy->linkrate;
		sphy->minimum_linkrate = phy->minimum_linkrate;
		sphy->minimum_linkrate_hw = SAS_LINK_RATE_1_5_GBPS;
		sphy->maximum_linkrate = phy->maximum_linkrate;
		sphy->maximum_linkrate_hw = phy->maximum_linkrate;
	}

	if (phy->phy_type & PORT_TYPE_SAS) {
		struct sas_identify_frame *id;
		id = (struct sas_identify_frame *)phy->frame_rcvd;
		id->dev_type = phy->identify.device_type;
		id->initiator_bits = SAS_PROTOCOL_ALL;
		id->target_bits = phy->identify.target_port_protocols;
	} else if (phy->phy_type & PORT_TYPE_SATA) {
		/*Nothing*/
	}
	PM8001_MSG_DBG(pm8001_ha, pm8001_printk("phy %d byte dmaded.\n", i));

	sas_phy->frame_rcvd_size = phy->frame_rcvd_size;
	pm8001_ha->sas->notify_port_event(sas_phy, PORTE_BYTES_DMAED);
}

/* Get the link rate speed  */
static void get_lrate_mode(struct pm8001_phy *phy, u8 link_rate)
{
	struct sas_phy *sas_phy = phy->sas_phy.phy;

	switch (link_rate) {
	case PHY_SPEED_60:
		phy->sas_phy.linkrate = SAS_LINK_RATE_6_0_GBPS;
		phy->sas_phy.phy->negotiated_linkrate = SAS_LINK_RATE_6_0_GBPS;
		break;
	case PHY_SPEED_30:
		phy->sas_phy.linkrate = SAS_LINK_RATE_3_0_GBPS;
		phy->sas_phy.phy->negotiated_linkrate = SAS_LINK_RATE_3_0_GBPS;
		break;
	case PHY_SPEED_15:
		phy->sas_phy.linkrate = SAS_LINK_RATE_1_5_GBPS;
		phy->sas_phy.phy->negotiated_linkrate = SAS_LINK_RATE_1_5_GBPS;
		break;
	}
	sas_phy->negotiated_linkrate = phy->sas_phy.linkrate;
	sas_phy->maximum_linkrate_hw = SAS_LINK_RATE_6_0_GBPS;
	sas_phy->minimum_linkrate_hw = SAS_LINK_RATE_1_5_GBPS;
	sas_phy->maximum_linkrate = SAS_LINK_RATE_6_0_GBPS;
	sas_phy->minimum_linkrate = SAS_LINK_RATE_1_5_GBPS;
}

/**
 * asd_get_attached_sas_addr -- extract/generate attached SAS address
 * @phy: pointer to asd_phy
 * @sas_addr: pointer to buffer where the SAS address is to be written
 *
 * This function extracts the SAS address from an IDENTIFY frame
 * received.  If OOB is SATA, then a SAS address is generated from the
 * HA tables.
 *
 * LOCKING: the frame_rcvd_lock needs to be held since this parses the frame
 * buffer.
 */
static void pm8001_get_attached_sas_addr(struct pm8001_phy *phy,
	u8 *sas_addr)
{
	if (phy->sas_phy.frame_rcvd[0] == 0x34
		&& phy->sas_phy.oob_mode == SATA_OOB_MODE) {
		struct pm8001_hba_info *pm8001_ha = phy->sas_phy.ha->lldd_ha;
		/* FIS device-to-host */
		u64 addr = be64_to_cpu(*(__be64 *)pm8001_ha->sas_addr);
		addr += phy->sas_phy.id;
		*(__be64 *)sas_addr = cpu_to_be64(addr);
	} else {
		struct sas_identify_frame *idframe =
			(void *) phy->sas_phy.frame_rcvd;
		memcpy(sas_addr, idframe->sas_addr, SAS_ADDR_SIZE);
	}
}

/**
 * pm8001_hw_event_ack_req- For PM8001,some events need to acknowage to FW.
 * @pm8001_ha: our hba card information
 * @Qnum: the outbound queue message number.
 * @SEA: source of event to ack
 * @port_id: port id.
 * @phyId: phy id.
 * @param0: parameter 0.
 * @param1: parameter 1.
 */
static void pm8001_hw_event_ack_req(struct pm8001_hba_info *pm8001_ha,
	u32 Qnum, u32 SEA, u32 port_id, u32 phyId, u32 param0, u32 param1)
{
	struct hw_event_ack_req	 payload;
	u32 opc = OPC_INB_SAS_HW_EVENT_ACK;

	struct inbound_queue_table *circularQ;

	memset((u8 *)&payload, 0, sizeof(payload));
	circularQ = &pm8001_ha->inbnd_q_tbl[Qnum];
	payload.tag = 1;
	payload.sea_phyid_portid = cpu_to_le32(((SEA & 0xFFFF) << 8) |
		((phyId & 0x0F) << 4) | (port_id & 0x0F));
	payload.param0 = cpu_to_le32(param0);
	payload.param1 = cpu_to_le32(param1);
	mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
}

static int pm8001_chip_phy_ctl_req(struct pm8001_hba_info *pm8001_ha,
	u32 phyId, u32 phy_op);

/**
 * hw_event_sas_phy_up -FW tells me a SAS phy up event.
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 */
static void
hw_event_sas_phy_up(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct hw_event_resp *pPayload =
		(struct hw_event_resp *)(piomb + 4);
	u32 lr_evt_status_phyid_portid =
		le32_to_cpu(pPayload->lr_evt_status_phyid_portid);
	u8 link_rate =
		(u8)((lr_evt_status_phyid_portid & 0xF0000000) >> 28);
	u8 port_id = (u8)(lr_evt_status_phyid_portid & 0x0000000F);
	u8 phy_id =
		(u8)((lr_evt_status_phyid_portid & 0x000000F0) >> 4);
	u32 npip_portstate = le32_to_cpu(pPayload->npip_portstate);
	u8 portstate = (u8)(npip_portstate & 0x0000000F);
	struct pm8001_port *port = &pm8001_ha->port[port_id];
	struct sas_ha_struct *sas_ha = pm8001_ha->sas;
	struct pm8001_phy *phy = &pm8001_ha->phy[phy_id];
	unsigned long flags;
	u8 deviceType = pPayload->sas_identify.dev_type;
	port->port_state =  portstate;
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk("HW_EVENT_SAS_PHY_UP port id = %d, phy id = %d\n",
		port_id, phy_id));

	switch (deviceType) {
	case SAS_PHY_UNUSED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("device type no device.\n"));
		break;
	case SAS_END_DEVICE:
		PM8001_MSG_DBG(pm8001_ha, pm8001_printk("end device.\n"));
		pm8001_chip_phy_ctl_req(pm8001_ha, phy_id,
			PHY_NOTIFY_ENABLE_SPINUP);
		port->port_attached = 1;
		get_lrate_mode(phy, link_rate);
		break;
	case SAS_EDGE_EXPANDER_DEVICE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("expander device.\n"));
		port->port_attached = 1;
		get_lrate_mode(phy, link_rate);
		break;
	case SAS_FANOUT_EXPANDER_DEVICE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("fanout expander device.\n"));
		port->port_attached = 1;
		get_lrate_mode(phy, link_rate);
		break;
	default:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("unknown device type(%x)\n", deviceType));
		break;
	}
	phy->phy_type |= PORT_TYPE_SAS;
	phy->identify.device_type = deviceType;
	phy->phy_attached = 1;
	if (phy->identify.device_type == SAS_END_DEV)
		phy->identify.target_port_protocols = SAS_PROTOCOL_SSP;
	else if (phy->identify.device_type != NO_DEVICE)
		phy->identify.target_port_protocols = SAS_PROTOCOL_SMP;
	phy->sas_phy.oob_mode = SAS_OOB_MODE;
	sas_ha->notify_phy_event(&phy->sas_phy, PHYE_OOB_DONE);
	spin_lock_irqsave(&phy->sas_phy.frame_rcvd_lock, flags);
	memcpy(phy->frame_rcvd, &pPayload->sas_identify,
		sizeof(struct sas_identify_frame)-4);
	phy->frame_rcvd_size = sizeof(struct sas_identify_frame) - 4;
	pm8001_get_attached_sas_addr(phy, phy->sas_phy.attached_sas_addr);
	spin_unlock_irqrestore(&phy->sas_phy.frame_rcvd_lock, flags);
	if (pm8001_ha->flags == PM8001F_RUN_TIME)
		mdelay(200);/*delay a moment to wait disk to spinup*/
	pm8001_bytes_dmaed(pm8001_ha, phy_id);
}

/**
 * hw_event_sata_phy_up -FW tells me a SATA phy up event.
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 */
static void
hw_event_sata_phy_up(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct hw_event_resp *pPayload =
		(struct hw_event_resp *)(piomb + 4);
	u32 lr_evt_status_phyid_portid =
		le32_to_cpu(pPayload->lr_evt_status_phyid_portid);
	u8 link_rate =
		(u8)((lr_evt_status_phyid_portid & 0xF0000000) >> 28);
	u8 port_id = (u8)(lr_evt_status_phyid_portid & 0x0000000F);
	u8 phy_id =
		(u8)((lr_evt_status_phyid_portid & 0x000000F0) >> 4);
	u32 npip_portstate = le32_to_cpu(pPayload->npip_portstate);
	u8 portstate = (u8)(npip_portstate & 0x0000000F);
	struct pm8001_port *port = &pm8001_ha->port[port_id];
	struct sas_ha_struct *sas_ha = pm8001_ha->sas;
	struct pm8001_phy *phy = &pm8001_ha->phy[phy_id];
	unsigned long flags;
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk("HW_EVENT_SATA_PHY_UP port id = %d,"
		" phy id = %d\n", port_id, phy_id));
	port->port_state =  portstate;
	port->port_attached = 1;
	get_lrate_mode(phy, link_rate);
	phy->phy_type |= PORT_TYPE_SATA;
	phy->phy_attached = 1;
	phy->sas_phy.oob_mode = SATA_OOB_MODE;
	sas_ha->notify_phy_event(&phy->sas_phy, PHYE_OOB_DONE);
	spin_lock_irqsave(&phy->sas_phy.frame_rcvd_lock, flags);
	memcpy(phy->frame_rcvd, ((u8 *)&pPayload->sata_fis - 4),
		sizeof(struct dev_to_host_fis));
	phy->frame_rcvd_size = sizeof(struct dev_to_host_fis);
	phy->identify.target_port_protocols = SAS_PROTOCOL_SATA;
	phy->identify.device_type = SATA_DEV;
	pm8001_get_attached_sas_addr(phy, phy->sas_phy.attached_sas_addr);
	spin_unlock_irqrestore(&phy->sas_phy.frame_rcvd_lock, flags);
	pm8001_bytes_dmaed(pm8001_ha, phy_id);
}

/**
 * hw_event_phy_down -we should notify the libsas the phy is down.
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 */
static void
hw_event_phy_down(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct hw_event_resp *pPayload =
		(struct hw_event_resp *)(piomb + 4);
	u32 lr_evt_status_phyid_portid =
		le32_to_cpu(pPayload->lr_evt_status_phyid_portid);
	u8 port_id = (u8)(lr_evt_status_phyid_portid & 0x0000000F);
	u8 phy_id =
		(u8)((lr_evt_status_phyid_portid & 0x000000F0) >> 4);
	u32 npip_portstate = le32_to_cpu(pPayload->npip_portstate);
	u8 portstate = (u8)(npip_portstate & 0x0000000F);
	struct pm8001_port *port = &pm8001_ha->port[port_id];
	struct pm8001_phy *phy = &pm8001_ha->phy[phy_id];
	port->port_state =  portstate;
	phy->phy_type = 0;
	phy->identify.device_type = 0;
	phy->phy_attached = 0;
	memset(&phy->dev_sas_addr, 0, SAS_ADDR_SIZE);
	switch (portstate) {
	case PORT_VALID:
		break;
	case PORT_INVALID:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" PortInvalid portID %d \n", port_id));
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" Last phy Down and port invalid\n"));
		port->port_attached = 0;
		pm8001_hw_event_ack_req(pm8001_ha, 0, HW_EVENT_PHY_DOWN,
			port_id, phy_id, 0, 0);
		break;
	case PORT_IN_RESET:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" Port In Reset portID %d \n", port_id));
		break;
	case PORT_NOT_ESTABLISHED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" phy Down and PORT_NOT_ESTABLISHED\n"));
		port->port_attached = 0;
		break;
	case PORT_LOSTCOMM:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" phy Down and PORT_LOSTCOMM\n"));
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" Last phy Down and port invalid\n"));
		port->port_attached = 0;
		pm8001_hw_event_ack_req(pm8001_ha, 0, HW_EVENT_PHY_DOWN,
			port_id, phy_id, 0, 0);
		break;
	default:
		port->port_attached = 0;
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" phy Down and(default) = %x\n",
			portstate));
		break;

	}
}

/**
 * mpi_reg_resp -process register device ID response.
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 *
 * when sas layer find a device it will notify LLDD, then the driver register
 * the domain device to FW, this event is the return device ID which the FW
 * has assigned, from now,inter-communication with FW is no longer using the
 * SAS address, use device ID which FW assigned.
 */
static int mpi_reg_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	u32 status;
	u32 device_id;
	u32 htag;
	struct pm8001_ccb_info *ccb;
	struct pm8001_device *pm8001_dev;
	struct dev_reg_resp *registerRespPayload =
		(struct dev_reg_resp *)(piomb + 4);

	htag = le32_to_cpu(registerRespPayload->tag);
	ccb = &pm8001_ha->ccb_info[registerRespPayload->tag];
	pm8001_dev = ccb->device;
	status = le32_to_cpu(registerRespPayload->status);
	device_id = le32_to_cpu(registerRespPayload->device_id);
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk(" register device is status = %d\n", status));
	switch (status) {
	case DEVREG_SUCCESS:
		PM8001_MSG_DBG(pm8001_ha, pm8001_printk("DEVREG_SUCCESS\n"));
		pm8001_dev->device_id = device_id;
		break;
	case DEVREG_FAILURE_OUT_OF_RESOURCE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("DEVREG_FAILURE_OUT_OF_RESOURCE\n"));
		break;
	case DEVREG_FAILURE_DEVICE_ALREADY_REGISTERED:
		PM8001_MSG_DBG(pm8001_ha,
		   pm8001_printk("DEVREG_FAILURE_DEVICE_ALREADY_REGISTERED\n"));
		break;
	case DEVREG_FAILURE_INVALID_PHY_ID:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("DEVREG_FAILURE_INVALID_PHY_ID\n"));
		break;
	case DEVREG_FAILURE_PHY_ID_ALREADY_REGISTERED:
		PM8001_MSG_DBG(pm8001_ha,
		   pm8001_printk("DEVREG_FAILURE_PHY_ID_ALREADY_REGISTERED\n"));
		break;
	case DEVREG_FAILURE_PORT_ID_OUT_OF_RANGE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("DEVREG_FAILURE_PORT_ID_OUT_OF_RANGE\n"));
		break;
	case DEVREG_FAILURE_PORT_NOT_VALID_STATE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("DEVREG_FAILURE_PORT_NOT_VALID_STATE\n"));
		break;
	case DEVREG_FAILURE_DEVICE_TYPE_NOT_VALID:
		PM8001_MSG_DBG(pm8001_ha,
		       pm8001_printk("DEVREG_FAILURE_DEVICE_TYPE_NOT_VALID\n"));
		break;
	default:
		PM8001_MSG_DBG(pm8001_ha,
		 pm8001_printk("DEVREG_FAILURE_DEVICE_TYPE_NOT_UNSORPORTED\n"));
		break;
	}
	complete(pm8001_dev->dcompletion);
	ccb->task = NULL;
	ccb->ccb_tag = 0xFFFFFFFF;
	pm8001_ccb_free(pm8001_ha, htag);
	return 0;
}

static int mpi_dereg_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	u32 status;
	u32 device_id;
	struct dev_reg_resp *registerRespPayload =
		(struct dev_reg_resp *)(piomb + 4);

	status = le32_to_cpu(registerRespPayload->status);
	device_id = le32_to_cpu(registerRespPayload->device_id);
	if (status != 0)
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(" deregister device failed ,status = %x"
			", device_id = %x\n", status, device_id));
	return 0;
}

static int
mpi_fw_flash_update_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	u32 status;
	struct fw_control_ex	fw_control_context;
	struct fw_flash_Update_resp *ppayload =
		(struct fw_flash_Update_resp *)(piomb + 4);
	u32 tag = le32_to_cpu(ppayload->tag);
	struct pm8001_ccb_info *ccb = &pm8001_ha->ccb_info[tag];
	status = le32_to_cpu(ppayload->status);
	memcpy(&fw_control_context,
		ccb->fw_control_context,
		sizeof(fw_control_context));
	switch (status) {
	case FLASH_UPDATE_COMPLETE_PENDING_REBOOT:
		PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk(": FLASH_UPDATE_COMPLETE_PENDING_REBOOT\n"));
		break;
	case FLASH_UPDATE_IN_PROGRESS:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_IN_PROGRESS\n"));
		break;
	case FLASH_UPDATE_HDR_ERR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_HDR_ERR\n"));
		break;
	case FLASH_UPDATE_OFFSET_ERR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_OFFSET_ERR\n"));
		break;
	case FLASH_UPDATE_CRC_ERR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_CRC_ERR\n"));
		break;
	case FLASH_UPDATE_LENGTH_ERR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_LENGTH_ERR\n"));
		break;
	case FLASH_UPDATE_HW_ERR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_HW_ERR\n"));
		break;
	case FLASH_UPDATE_DNLD_NOT_SUPPORTED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_DNLD_NOT_SUPPORTED\n"));
		break;
	case FLASH_UPDATE_DISABLED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk(": FLASH_UPDATE_DISABLED\n"));
		break;
	default:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("No matched status = %d\n", status));
		break;
	}
	ccb->fw_control_context->fw_control->retcode = status;
	pci_free_consistent(pm8001_ha->pdev,
			fw_control_context.len,
			fw_control_context.virtAddr,
			fw_control_context.phys_addr);
	complete(pm8001_ha->nvmd_completion);
	ccb->task = NULL;
	ccb->ccb_tag = 0xFFFFFFFF;
	pm8001_ccb_free(pm8001_ha, tag);
	return 0;
}

static int
mpi_general_event(struct pm8001_hba_info *pm8001_ha , void *piomb)
{
	u32 status;
	int i;
	struct general_event_resp *pPayload =
		(struct general_event_resp *)(piomb + 4);
	status = le32_to_cpu(pPayload->status);
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk(" status = 0x%x\n", status));
	for (i = 0; i < GENERAL_EVENT_PAYLOAD; i++)
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("inb_IOMB_payload[0x%x] 0x%x, \n", i,
			pPayload->inb_IOMB_payload[i]));
	return 0;
}

static int
mpi_task_abort_resp(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	struct sas_task *t;
	struct pm8001_ccb_info *ccb;
	unsigned long flags;
	u32 status ;
	u32 tag, scp;
	struct task_status_struct *ts;

	struct task_abort_resp *pPayload =
		(struct task_abort_resp *)(piomb + 4);
	ccb = &pm8001_ha->ccb_info[pPayload->tag];
	t = ccb->task;


	status = le32_to_cpu(pPayload->status);
	tag = le32_to_cpu(pPayload->tag);
	scp = le32_to_cpu(pPayload->scp);
	PM8001_IO_DBG(pm8001_ha,
		pm8001_printk(" status = 0x%x\n", status));
	if (t == NULL)
		return -1;
	ts = &t->task_status;
	if (status != 0)
		PM8001_FAIL_DBG(pm8001_ha,
			pm8001_printk("task abort failed status 0x%x ,"
			"tag = 0x%x, scp= 0x%x\n", status, tag, scp));
	switch (status) {
	case IO_SUCCESS:
		PM8001_EH_DBG(pm8001_ha, pm8001_printk("IO_SUCCESS\n"));
		ts->resp = SAS_TASK_COMPLETE;
		ts->stat = SAM_STAT_GOOD;
		break;
	case IO_NOT_VALID:
		PM8001_EH_DBG(pm8001_ha, pm8001_printk("IO_NOT_VALID\n"));
		ts->resp = TMF_RESP_FUNC_FAILED;
		break;
	}
	spin_lock_irqsave(&t->task_state_lock, flags);
	t->task_state_flags &= ~SAS_TASK_STATE_PENDING;
	t->task_state_flags &= ~SAS_TASK_AT_INITIATOR;
	t->task_state_flags |= SAS_TASK_STATE_DONE;
	spin_unlock_irqrestore(&t->task_state_lock, flags);
	pm8001_ccb_task_free(pm8001_ha, t, ccb, pPayload->tag);
	mb();
	t->task_done(t);
	return 0;
}

/**
 * mpi_hw_event -The hw event has come.
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 */
static int mpi_hw_event(struct pm8001_hba_info *pm8001_ha, void* piomb)
{
	unsigned long flags;
	struct hw_event_resp *pPayload =
		(struct hw_event_resp *)(piomb + 4);
	u32 lr_evt_status_phyid_portid =
		le32_to_cpu(pPayload->lr_evt_status_phyid_portid);
	u8 port_id = (u8)(lr_evt_status_phyid_portid & 0x0000000F);
	u8 phy_id =
		(u8)((lr_evt_status_phyid_portid & 0x000000F0) >> 4);
	u16 eventType =
		(u16)((lr_evt_status_phyid_portid & 0x00FFFF00) >> 8);
	u8 status =
		(u8)((lr_evt_status_phyid_portid & 0x0F000000) >> 24);
	struct sas_ha_struct *sas_ha = pm8001_ha->sas;
	struct pm8001_phy *phy = &pm8001_ha->phy[phy_id];
	struct asd_sas_phy *sas_phy = sas_ha->sas_phy[phy_id];
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk("outbound queue HW event & event type : "));
	switch (eventType) {
	case HW_EVENT_PHY_START_STATUS:
		PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk("HW_EVENT_PHY_START_STATUS"
			" status = %x\n", status));
		if (status == 0) {
			phy->phy_state = 1;
			if (pm8001_ha->flags == PM8001F_RUN_TIME)
				complete(phy->enable_completion);
		}
		break;
	case HW_EVENT_SAS_PHY_UP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PHY_START_STATUS \n"));
		hw_event_sas_phy_up(pm8001_ha, piomb);
		break;
	case HW_EVENT_SATA_PHY_UP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_SATA_PHY_UP \n"));
		hw_event_sata_phy_up(pm8001_ha, piomb);
		break;
	case HW_EVENT_PHY_STOP_STATUS:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PHY_STOP_STATUS "
			"status = %x\n", status));
		if (status == 0)
			phy->phy_state = 0;
		break;
	case HW_EVENT_SATA_SPINUP_HOLD:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_SATA_SPINUP_HOLD \n"));
		sas_ha->notify_phy_event(&phy->sas_phy, PHYE_SPINUP_HOLD);
		break;
	case HW_EVENT_PHY_DOWN:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PHY_DOWN \n"));
		sas_ha->notify_phy_event(&phy->sas_phy, PHYE_LOSS_OF_SIGNAL);
		phy->phy_attached = 0;
		phy->phy_state = 0;
		hw_event_phy_down(pm8001_ha, piomb);
		break;
	case HW_EVENT_PORT_INVALID:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PORT_INVALID\n"));
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	/* the broadcast change primitive received, tell the LIBSAS this event
	to revalidate the sas domain*/
	case HW_EVENT_BROADCAST_CHANGE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_BROADCAST_CHANGE\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0, HW_EVENT_BROADCAST_CHANGE,
			port_id, phy_id, 1, 0);
		spin_lock_irqsave(&sas_phy->sas_prim_lock, flags);
		sas_phy->sas_prim = HW_EVENT_BROADCAST_CHANGE;
		spin_unlock_irqrestore(&sas_phy->sas_prim_lock, flags);
		sas_ha->notify_port_event(sas_phy, PORTE_BROADCAST_RCVD);
		break;
	case HW_EVENT_PHY_ERROR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PHY_ERROR\n"));
		sas_phy_disconnected(&phy->sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_phy_event(&phy->sas_phy, PHYE_OOB_ERROR);
		break;
	case HW_EVENT_BROADCAST_EXP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_BROADCAST_EXP\n"));
		spin_lock_irqsave(&sas_phy->sas_prim_lock, flags);
		sas_phy->sas_prim = HW_EVENT_BROADCAST_EXP;
		spin_unlock_irqrestore(&sas_phy->sas_prim_lock, flags);
		sas_ha->notify_port_event(sas_phy, PORTE_BROADCAST_RCVD);
		break;
	case HW_EVENT_LINK_ERR_INVALID_DWORD:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_LINK_ERR_INVALID_DWORD\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_LINK_ERR_INVALID_DWORD, port_id, phy_id, 0, 0);
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_LINK_ERR_DISPARITY_ERROR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_LINK_ERR_DISPARITY_ERROR\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_LINK_ERR_DISPARITY_ERROR,
			port_id, phy_id, 0, 0);
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_LINK_ERR_CODE_VIOLATION:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_LINK_ERR_CODE_VIOLATION\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_LINK_ERR_CODE_VIOLATION,
			port_id, phy_id, 0, 0);
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_LINK_ERR_LOSS_OF_DWORD_SYNCH:
		PM8001_MSG_DBG(pm8001_ha,
		      pm8001_printk("HW_EVENT_LINK_ERR_LOSS_OF_DWORD_SYNCH\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_LINK_ERR_LOSS_OF_DWORD_SYNCH,
			port_id, phy_id, 0, 0);
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_MALFUNCTION:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_MALFUNCTION\n"));
		break;
	case HW_EVENT_BROADCAST_SES:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_BROADCAST_SES\n"));
		spin_lock_irqsave(&sas_phy->sas_prim_lock, flags);
		sas_phy->sas_prim = HW_EVENT_BROADCAST_SES;
		spin_unlock_irqrestore(&sas_phy->sas_prim_lock, flags);
		sas_ha->notify_port_event(sas_phy, PORTE_BROADCAST_RCVD);
		break;
	case HW_EVENT_INBOUND_CRC_ERROR:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_INBOUND_CRC_ERROR\n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_INBOUND_CRC_ERROR,
			port_id, phy_id, 0, 0);
		break;
	case HW_EVENT_HARD_RESET_RECEIVED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_HARD_RESET_RECEIVED\n"));
		sas_ha->notify_port_event(sas_phy, PORTE_HARD_RESET);
		break;
	case HW_EVENT_ID_FRAME_TIMEOUT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_ID_FRAME_TIMEOUT\n"));
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_LINK_ERR_PHY_RESET_FAILED:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_LINK_ERR_PHY_RESET_FAILED \n"));
		pm8001_hw_event_ack_req(pm8001_ha, 0,
			HW_EVENT_LINK_ERR_PHY_RESET_FAILED,
			port_id, phy_id, 0, 0);
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_PORT_RESET_TIMER_TMO:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PORT_RESET_TIMER_TMO \n"));
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_PORT_RECOVERY_TIMER_TMO:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PORT_RECOVERY_TIMER_TMO \n"));
		sas_phy_disconnected(sas_phy);
		phy->phy_attached = 0;
		sas_ha->notify_port_event(sas_phy, PORTE_LINK_RESET_ERR);
		break;
	case HW_EVENT_PORT_RECOVER:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PORT_RECOVER \n"));
		break;
	case HW_EVENT_PORT_RESET_COMPLETE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("HW_EVENT_PORT_RESET_COMPLETE \n"));
		break;
	case EVENT_BROADCAST_ASYNCH_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("EVENT_BROADCAST_ASYNCH_EVENT\n"));
		break;
	default:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("Unknown event type = %x\n", eventType));
		break;
	}
	return 0;
}

/**
 * process_one_iomb - process one outbound Queue memory block
 * @pm8001_ha: our hba card information
 * @piomb: IO message buffer
 */
static void process_one_iomb(struct pm8001_hba_info *pm8001_ha, void *piomb)
{
	u32 pHeader = (u32)*(u32 *)piomb;
	u8 opc = (u8)((le32_to_cpu(pHeader)) & 0xFFF);

	PM8001_MSG_DBG(pm8001_ha, pm8001_printk("process_one_iomb:"));

	switch (opc) {
	case OPC_OUB_ECHO:
		PM8001_MSG_DBG(pm8001_ha, pm8001_printk("OPC_OUB_ECHO \n"));
		break;
	case OPC_OUB_HW_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_HW_EVENT \n"));
		mpi_hw_event(pm8001_ha, piomb);
		break;
	case OPC_OUB_SSP_COMP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SSP_COMP \n"));
		mpi_ssp_completion(pm8001_ha, piomb);
		break;
	case OPC_OUB_SMP_COMP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SMP_COMP \n"));
		mpi_smp_completion(pm8001_ha, piomb);
		break;
	case OPC_OUB_LOCAL_PHY_CNTRL:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_LOCAL_PHY_CNTRL\n"));
		mpi_local_phy_ctl(pm8001_ha, piomb);
		break;
	case OPC_OUB_DEV_REGIST:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_DEV_REGIST \n"));
		mpi_reg_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_DEREG_DEV:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("unresgister the deviece \n"));
		mpi_dereg_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_GET_DEV_HANDLE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GET_DEV_HANDLE \n"));
		break;
	case OPC_OUB_SATA_COMP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SATA_COMP \n"));
		mpi_sata_completion(pm8001_ha, piomb);
		break;
	case OPC_OUB_SATA_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SATA_EVENT \n"));
		mpi_sata_event(pm8001_ha, piomb);
		break;
	case OPC_OUB_SSP_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SSP_EVENT\n"));
		mpi_ssp_event(pm8001_ha, piomb);
		break;
	case OPC_OUB_DEV_HANDLE_ARRIV:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_DEV_HANDLE_ARRIV\n"));
		/*This is for target*/
		break;
	case OPC_OUB_SSP_RECV_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SSP_RECV_EVENT\n"));
		/*This is for target*/
		break;
	case OPC_OUB_DEV_INFO:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_DEV_INFO\n"));
		break;
	case OPC_OUB_FW_FLASH_UPDATE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_FW_FLASH_UPDATE\n"));
		mpi_fw_flash_update_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_GPIO_RESPONSE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GPIO_RESPONSE\n"));
		break;
	case OPC_OUB_GPIO_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GPIO_EVENT\n"));
		break;
	case OPC_OUB_GENERAL_EVENT:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GENERAL_EVENT\n"));
		mpi_general_event(pm8001_ha, piomb);
		break;
	case OPC_OUB_SSP_ABORT_RSP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SSP_ABORT_RSP\n"));
		mpi_task_abort_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_SATA_ABORT_RSP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SATA_ABORT_RSP\n"));
		mpi_task_abort_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_SAS_DIAG_MODE_START_END:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SAS_DIAG_MODE_START_END\n"));
		break;
	case OPC_OUB_SAS_DIAG_EXECUTE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SAS_DIAG_EXECUTE\n"));
		break;
	case OPC_OUB_GET_TIME_STAMP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GET_TIME_STAMP\n"));
		break;
	case OPC_OUB_SAS_HW_EVENT_ACK:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SAS_HW_EVENT_ACK\n"));
		break;
	case OPC_OUB_PORT_CONTROL:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_PORT_CONTROL\n"));
		break;
	case OPC_OUB_SMP_ABORT_RSP:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SMP_ABORT_RSP\n"));
		mpi_task_abort_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_GET_NVMD_DATA:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GET_NVMD_DATA\n"));
		mpi_get_nvmd_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_SET_NVMD_DATA:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SET_NVMD_DATA\n"));
		mpi_set_nvmd_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_DEVICE_HANDLE_REMOVAL:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_DEVICE_HANDLE_REMOVAL\n"));
		break;
	case OPC_OUB_SET_DEVICE_STATE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SET_DEVICE_STATE\n"));
		mpi_set_dev_state_resp(pm8001_ha, piomb);
		break;
	case OPC_OUB_GET_DEVICE_STATE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_GET_DEVICE_STATE\n"));
		break;
	case OPC_OUB_SET_DEV_INFO:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SET_DEV_INFO\n"));
		break;
	case OPC_OUB_SAS_RE_INITIALIZE:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("OPC_OUB_SAS_RE_INITIALIZE\n"));
		break;
	default:
		PM8001_MSG_DBG(pm8001_ha,
			pm8001_printk("Unknown outbound Queue IOMB OPC = %x\n",
			opc));
		break;
	}
}

static int process_oq(struct pm8001_hba_info *pm8001_ha)
{
	struct outbound_queue_table *circularQ;
	void *pMsg1 = NULL;
	u8 bc = 0;
	u32 ret = MPI_IO_STATUS_FAIL;

	circularQ = &pm8001_ha->outbnd_q_tbl[0];
	do {
		ret = mpi_msg_consume(pm8001_ha, circularQ, &pMsg1, &bc);
		if (MPI_IO_STATUS_SUCCESS == ret) {
			/* process the outbound message */
			process_one_iomb(pm8001_ha, (void *)(pMsg1 - 4));
			/* free the message from the outbound circular buffer */
			mpi_msg_free_set(pm8001_ha, pMsg1, circularQ, bc);
		}
		if (MPI_IO_STATUS_BUSY == ret) {
			u32 producer_idx;
			/* Update the producer index from SPC */
			producer_idx = pm8001_read_32(circularQ->pi_virt);
			circularQ->producer_index = cpu_to_le32(producer_idx);
			if (circularQ->producer_index ==
				circularQ->consumer_idx)
				/* OQ is empty */
				break;
		}
	} while (1);
	return ret;
}

/* PCI_DMA_... to our direction translation. */
static const u8 data_dir_flags[] = {
	[PCI_DMA_BIDIRECTIONAL] = DATA_DIR_BYRECIPIENT,/* UNSPECIFIED */
	[PCI_DMA_TODEVICE]	= DATA_DIR_OUT,/* OUTBOUND */
	[PCI_DMA_FROMDEVICE]	= DATA_DIR_IN,/* INBOUND */
	[PCI_DMA_NONE]		= DATA_DIR_NONE,/* NO TRANSFER */
};
static void
pm8001_chip_make_sg(struct scatterlist *scatter, int nr, void *prd)
{
	int i;
	struct scatterlist *sg;
	struct pm8001_prd *buf_prd = prd;

	for_each_sg(scatter, sg, nr, i) {
		buf_prd->addr = cpu_to_le64(sg_dma_address(sg));
		buf_prd->im_len.len = cpu_to_le32(sg_dma_len(sg));
		buf_prd->im_len.e = 0;
		buf_prd++;
	}
}

static void build_smp_cmd(u32 deviceID, u32 hTag, struct smp_req *psmp_cmd)
{
	psmp_cmd->tag = cpu_to_le32(hTag);
	psmp_cmd->device_id = cpu_to_le32(deviceID);
	psmp_cmd->len_ip_ir = cpu_to_le32(1|(1 << 1));
}

/**
 * pm8001_chip_smp_req - send a SMP task to FW
 * @pm8001_ha: our hba card information.
 * @ccb: the ccb information this request used.
 */
static int pm8001_chip_smp_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_ccb_info *ccb)
{
	int elem, rc;
	struct sas_task *task = ccb->task;
	struct domain_device *dev = task->dev;
	struct pm8001_device *pm8001_dev = dev->lldd_dev;
	struct scatterlist *sg_req, *sg_resp;
	u32 req_len, resp_len;
	struct smp_req smp_cmd;
	u32 opc;
	struct inbound_queue_table *circularQ;

	memset(&smp_cmd, 0, sizeof(smp_cmd));
	/*
	 * DMA-map SMP request, response buffers
	 */
	sg_req = &task->smp_task.smp_req;
	elem = dma_map_sg(pm8001_ha->dev, sg_req, 1, PCI_DMA_TODEVICE);
	if (!elem)
		return -ENOMEM;
	req_len = sg_dma_len(sg_req);

	sg_resp = &task->smp_task.smp_resp;
	elem = dma_map_sg(pm8001_ha->dev, sg_resp, 1, PCI_DMA_FROMDEVICE);
	if (!elem) {
		rc = -ENOMEM;
		goto err_out;
	}
	resp_len = sg_dma_len(sg_resp);
	/* must be in dwords */
	if ((req_len & 0x3) || (resp_len & 0x3)) {
		rc = -EINVAL;
		goto err_out_2;
	}

	opc = OPC_INB_SMP_REQUEST;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	smp_cmd.tag = cpu_to_le32(ccb->ccb_tag);
	smp_cmd.long_smp_req.long_req_addr =
		cpu_to_le64((u64)sg_dma_address(&task->smp_task.smp_req));
	smp_cmd.long_smp_req.long_req_size =
		cpu_to_le32((u32)sg_dma_len(&task->smp_task.smp_req)-4);
	smp_cmd.long_smp_req.long_resp_addr =
		cpu_to_le64((u64)sg_dma_address(&task->smp_task.smp_resp));
	smp_cmd.long_smp_req.long_resp_size =
		cpu_to_le32((u32)sg_dma_len(&task->smp_task.smp_resp)-4);
	build_smp_cmd(pm8001_dev->device_id, smp_cmd.tag, &smp_cmd);
	mpi_build_cmd(pm8001_ha, circularQ, opc, (u32 *)&smp_cmd);
	return 0;

err_out_2:
	dma_unmap_sg(pm8001_ha->dev, &ccb->task->smp_task.smp_resp, 1,
			PCI_DMA_FROMDEVICE);
err_out:
	dma_unmap_sg(pm8001_ha->dev, &ccb->task->smp_task.smp_req, 1,
			PCI_DMA_TODEVICE);
	return rc;
}

/**
 * pm8001_chip_ssp_io_req - send a SSP task to FW
 * @pm8001_ha: our hba card information.
 * @ccb: the ccb information this request used.
 */
static int pm8001_chip_ssp_io_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_ccb_info *ccb)
{
	struct sas_task *task = ccb->task;
	struct domain_device *dev = task->dev;
	struct pm8001_device *pm8001_dev = dev->lldd_dev;
	struct ssp_ini_io_start_req ssp_cmd;
	u32 tag = ccb->ccb_tag;
	int ret;
	__le64 phys_addr;
	struct inbound_queue_table *circularQ;
	u32 opc = OPC_INB_SSPINIIOSTART;
	memset(&ssp_cmd, 0, sizeof(ssp_cmd));
	memcpy(ssp_cmd.ssp_iu.lun, task->ssp_task.LUN, 8);
	ssp_cmd.dir_m_tlr =
		cpu_to_le32(data_dir_flags[task->data_dir] << 8 | 0x0);/*0 for
	SAS 1.1 compatible TLR*/
	ssp_cmd.data_len = cpu_to_le32(task->total_xfer_len);
	ssp_cmd.device_id = cpu_to_le32(pm8001_dev->device_id);
	ssp_cmd.tag = cpu_to_le32(tag);
	if (task->ssp_task.enable_first_burst)
		ssp_cmd.ssp_iu.efb_prio_attr |= 0x80;
	ssp_cmd.ssp_iu.efb_prio_attr |= (task->ssp_task.task_prio << 3);
	ssp_cmd.ssp_iu.efb_prio_attr |= (task->ssp_task.task_attr & 7);
	memcpy(ssp_cmd.ssp_iu.cdb, task->ssp_task.cdb, 16);
	circularQ = &pm8001_ha->inbnd_q_tbl[0];

	/* fill in PRD (scatter/gather) table, if any */
	if (task->num_scatter > 1) {
		pm8001_chip_make_sg(task->scatter, ccb->n_elem, ccb->buf_prd);
		phys_addr = cpu_to_le64(ccb->ccb_dma_handle +
				offsetof(struct pm8001_ccb_info, buf_prd[0]));
		ssp_cmd.addr_low = lower_32_bits(phys_addr);
		ssp_cmd.addr_high = upper_32_bits(phys_addr);
		ssp_cmd.esgl = cpu_to_le32(1<<31);
	} else if (task->num_scatter == 1) {
		__le64 dma_addr = cpu_to_le64(sg_dma_address(task->scatter));
		ssp_cmd.addr_low = lower_32_bits(dma_addr);
		ssp_cmd.addr_high = upper_32_bits(dma_addr);
		ssp_cmd.len = cpu_to_le32(task->total_xfer_len);
		ssp_cmd.esgl = 0;
	} else if (task->num_scatter == 0) {
		ssp_cmd.addr_low = 0;
		ssp_cmd.addr_high = 0;
		ssp_cmd.len = cpu_to_le32(task->total_xfer_len);
		ssp_cmd.esgl = 0;
	}
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &ssp_cmd);
	return ret;
}

static int pm8001_chip_sata_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_ccb_info *ccb)
{
	struct sas_task *task = ccb->task;
	struct domain_device *dev = task->dev;
	struct pm8001_device *pm8001_ha_dev = dev->lldd_dev;
	u32 tag = ccb->ccb_tag;
	int ret;
	struct sata_start_req sata_cmd;
	u32 hdr_tag, ncg_tag = 0;
	__le64 phys_addr;
	u32 ATAP = 0x0;
	u32 dir;
	struct inbound_queue_table *circularQ;
	u32  opc = OPC_INB_SATA_HOST_OPSTART;
	memset(&sata_cmd, 0, sizeof(sata_cmd));
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	if (task->data_dir == PCI_DMA_NONE) {
		ATAP = 0x04;  /* no data*/
		PM8001_IO_DBG(pm8001_ha, pm8001_printk("no data \n"));
	} else if (likely(!task->ata_task.device_control_reg_update)) {
		if (task->ata_task.dma_xfer) {
			ATAP = 0x06; /* DMA */
			PM8001_IO_DBG(pm8001_ha, pm8001_printk("DMA \n"));
		} else {
			ATAP = 0x05; /* PIO*/
			PM8001_IO_DBG(pm8001_ha, pm8001_printk("PIO \n"));
		}
		if (task->ata_task.use_ncq &&
			dev->sata_dev.command_set != ATAPI_COMMAND_SET) {
			ATAP = 0x07; /* FPDMA */
			PM8001_IO_DBG(pm8001_ha, pm8001_printk("FPDMA \n"));
		}
	}
	if (task->ata_task.use_ncq && pm8001_get_ncq_tag(task, &hdr_tag))
		ncg_tag = hdr_tag;
	dir = data_dir_flags[task->data_dir] << 8;
	sata_cmd.tag = cpu_to_le32(tag);
	sata_cmd.device_id = cpu_to_le32(pm8001_ha_dev->device_id);
	sata_cmd.data_len = cpu_to_le32(task->total_xfer_len);
	sata_cmd.ncqtag_atap_dir_m =
		cpu_to_le32(((ncg_tag & 0xff)<<16)|((ATAP & 0x3f) << 10) | dir);
	sata_cmd.sata_fis = task->ata_task.fis;
	if (likely(!task->ata_task.device_control_reg_update))
		sata_cmd.sata_fis.flags |= 0x80;/* C=1: update ATA cmd reg */
	sata_cmd.sata_fis.flags &= 0xF0;/* PM_PORT field shall be 0 */
	/* fill in PRD (scatter/gather) table, if any */
	if (task->num_scatter > 1) {
		pm8001_chip_make_sg(task->scatter, ccb->n_elem, ccb->buf_prd);
		phys_addr = cpu_to_le64(ccb->ccb_dma_handle +
				offsetof(struct pm8001_ccb_info, buf_prd[0]));
		sata_cmd.addr_low = lower_32_bits(phys_addr);
		sata_cmd.addr_high = upper_32_bits(phys_addr);
		sata_cmd.esgl = cpu_to_le32(1 << 31);
	} else if (task->num_scatter == 1) {
		__le64 dma_addr = cpu_to_le64(sg_dma_address(task->scatter));
		sata_cmd.addr_low = lower_32_bits(dma_addr);
		sata_cmd.addr_high = upper_32_bits(dma_addr);
		sata_cmd.len = cpu_to_le32(task->total_xfer_len);
		sata_cmd.esgl = 0;
	} else if (task->num_scatter == 0) {
		sata_cmd.addr_low = 0;
		sata_cmd.addr_high = 0;
		sata_cmd.len = cpu_to_le32(task->total_xfer_len);
		sata_cmd.esgl = 0;
	}
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &sata_cmd);
	return ret;
}

/**
 * pm8001_chip_phy_start_req - start phy via PHY_START COMMAND
 * @pm8001_ha: our hba card information.
 * @num: the inbound queue number
 * @phy_id: the phy id which we wanted to start up.
 */
static int
pm8001_chip_phy_start_req(struct pm8001_hba_info *pm8001_ha, u8 phy_id)
{
	struct phy_start_req payload;
	struct inbound_queue_table *circularQ;
	int ret;
	u32 tag = 0x01;
	u32 opcode = OPC_INB_PHYSTART;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memset(&payload, 0, sizeof(payload));
	payload.tag = cpu_to_le32(tag);
	/*
	 ** [0:7]   PHY Identifier
	 ** [8:11]  link rate 1.5G, 3G, 6G
	 ** [12:13] link mode 01b SAS mode; 10b SATA mode; 11b both
	 ** [14]    0b disable spin up hold; 1b enable spin up hold
	 */
	payload.ase_sh_lm_slr_phyid = cpu_to_le32(SPINHOLD_DISABLE |
		LINKMODE_AUTO |	LINKRATE_15 |
		LINKRATE_30 | LINKRATE_60 | phy_id);
	payload.sas_identify.dev_type = SAS_END_DEV;
	payload.sas_identify.initiator_bits = SAS_PROTOCOL_ALL;
	memcpy(payload.sas_identify.sas_addr,
		pm8001_ha->sas_addr, SAS_ADDR_SIZE);
	payload.sas_identify.phy_id = phy_id;
	ret = mpi_build_cmd(pm8001_ha, circularQ, opcode, &payload);
	return ret;
}

/**
 * pm8001_chip_phy_stop_req - start phy via PHY_STOP COMMAND
 * @pm8001_ha: our hba card information.
 * @num: the inbound queue number
 * @phy_id: the phy id which we wanted to start up.
 */
static int pm8001_chip_phy_stop_req(struct pm8001_hba_info *pm8001_ha,
	u8 phy_id)
{
	struct phy_stop_req payload;
	struct inbound_queue_table *circularQ;
	int ret;
	u32 tag = 0x01;
	u32 opcode = OPC_INB_PHYSTOP;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memset(&payload, 0, sizeof(payload));
	payload.tag = cpu_to_le32(tag);
	payload.phy_id = cpu_to_le32(phy_id);
	ret = mpi_build_cmd(pm8001_ha, circularQ, opcode, &payload);
	return ret;
}

/**
 * see comments on mpi_reg_resp.
 */
static int pm8001_chip_reg_dev_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_device *pm8001_dev, u32 flag)
{
	struct reg_dev_req payload;
	u32	opc;
	u32 stp_sspsmp_sata = 0x4;
	struct inbound_queue_table *circularQ;
	u32 linkrate, phy_id;
	int rc, tag = 0xdeadbeef;
	struct pm8001_ccb_info *ccb;
	u8 retryFlag = 0x1;
	u16 firstBurstSize = 0;
	u16 ITNT = 2000;
	struct domain_device *dev = pm8001_dev->sas_device;
	struct domain_device *parent_dev = dev->parent;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];

	memset(&payload, 0, sizeof(payload));
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc)
		return rc;
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->device = pm8001_dev;
	ccb->ccb_tag = tag;
	payload.tag = cpu_to_le32(tag);
	if (flag == 1)
		stp_sspsmp_sata = 0x02; /*direct attached sata */
	else {
		if (pm8001_dev->dev_type == SATA_DEV)
			stp_sspsmp_sata = 0x00; /* stp*/
		else if (pm8001_dev->dev_type == SAS_END_DEV ||
			pm8001_dev->dev_type == EDGE_DEV ||
			pm8001_dev->dev_type == FANOUT_DEV)
			stp_sspsmp_sata = 0x01; /*ssp or smp*/
	}
	if (parent_dev && DEV_IS_EXPANDER(parent_dev->dev_type))
		phy_id = parent_dev->ex_dev.ex_phy->phy_id;
	else
		phy_id = pm8001_dev->attached_phy;
	opc = OPC_INB_REG_DEV;
	linkrate = (pm8001_dev->sas_device->linkrate < dev->port->linkrate) ?
			pm8001_dev->sas_device->linkrate : dev->port->linkrate;
	payload.phyid_portid =
		cpu_to_le32(((pm8001_dev->sas_device->port->id) & 0x0F) |
		((phy_id & 0x0F) << 4));
	payload.dtype_dlr_retry = cpu_to_le32((retryFlag & 0x01) |
		((linkrate & 0x0F) * 0x1000000) |
		((stp_sspsmp_sata & 0x03) * 0x10000000));
	payload.firstburstsize_ITNexustimeout =
		cpu_to_le32(ITNT | (firstBurstSize * 0x10000));
	memcpy(payload.sas_addr, pm8001_dev->sas_device->sas_addr,
		SAS_ADDR_SIZE);
	rc = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return rc;
}

/**
 * see comments on mpi_reg_resp.
 */
static int pm8001_chip_dereg_dev_req(struct pm8001_hba_info *pm8001_ha,
	u32 device_id)
{
	struct dereg_dev_req payload;
	u32 opc = OPC_INB_DEREG_DEV_HANDLE;
	int ret;
	struct inbound_queue_table *circularQ;

	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memset(&payload, 0, sizeof(payload));
	payload.tag = 1;
	payload.device_id = cpu_to_le32(device_id);
	PM8001_MSG_DBG(pm8001_ha,
		pm8001_printk("unregister device device_id = %d\n", device_id));
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return ret;
}

/**
 * pm8001_chip_phy_ctl_req - support the local phy operation
 * @pm8001_ha: our hba card information.
 * @num: the inbound queue number
 * @phy_id: the phy id which we wanted to operate
 * @phy_op:
 */
static int pm8001_chip_phy_ctl_req(struct pm8001_hba_info *pm8001_ha,
	u32 phyId, u32 phy_op)
{
	struct local_phy_ctl_req payload;
	struct inbound_queue_table *circularQ;
	int ret;
	u32 opc = OPC_INB_LOCAL_PHY_CONTROL;
	memset(&payload, 0, sizeof(payload));
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	payload.tag = 1;
	payload.phyop_phyid =
		cpu_to_le32(((phy_op & 0xff) << 8) | (phyId & 0x0F));
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return ret;
}

static u32 pm8001_chip_is_our_interupt(struct pm8001_hba_info *pm8001_ha)
{
	u32 value;
#ifdef PM8001_USE_MSIX
	return 1;
#endif
	value = pm8001_cr32(pm8001_ha, 0, MSGU_ODR);
	if (value)
		return 1;
	return 0;

}

/**
 * pm8001_chip_isr - PM8001 isr handler.
 * @pm8001_ha: our hba card information.
 * @irq: irq number.
 * @stat: stat.
 */
static irqreturn_t
pm8001_chip_isr(struct pm8001_hba_info *pm8001_ha)
{
	unsigned long flags;
	spin_lock_irqsave(&pm8001_ha->lock, flags);
	pm8001_chip_interrupt_disable(pm8001_ha);
	process_oq(pm8001_ha);
	pm8001_chip_interrupt_enable(pm8001_ha);
	spin_unlock_irqrestore(&pm8001_ha->lock, flags);
	return IRQ_HANDLED;
}

static int send_task_abort(struct pm8001_hba_info *pm8001_ha, u32 opc,
	u32 dev_id, u8 flag, u32 task_tag, u32 cmd_tag)
{
	struct task_abort_req task_abort;
	struct inbound_queue_table *circularQ;
	int ret;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memset(&task_abort, 0, sizeof(task_abort));
	if (ABORT_SINGLE == (flag & ABORT_MASK)) {
		task_abort.abort_all = 0;
		task_abort.device_id = cpu_to_le32(dev_id);
		task_abort.tag_to_abort = cpu_to_le32(task_tag);
		task_abort.tag = cpu_to_le32(cmd_tag);
	} else if (ABORT_ALL == (flag & ABORT_MASK)) {
		task_abort.abort_all = cpu_to_le32(1);
		task_abort.device_id = cpu_to_le32(dev_id);
		task_abort.tag = cpu_to_le32(cmd_tag);
	}
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &task_abort);
	return ret;
}

/**
 * pm8001_chip_abort_task - SAS abort task when error or exception happened.
 * @task: the task we wanted to aborted.
 * @flag: the abort flag.
 */
static int pm8001_chip_abort_task(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_device *pm8001_dev, u8 flag, u32 task_tag, u32 cmd_tag)
{
	u32 opc, device_id;
	int rc = TMF_RESP_FUNC_FAILED;
	PM8001_EH_DBG(pm8001_ha, pm8001_printk("cmd_tag = %x, abort task tag"
		" = %x", cmd_tag, task_tag));
	if (pm8001_dev->dev_type == SAS_END_DEV)
		opc = OPC_INB_SSP_ABORT;
	else if (pm8001_dev->dev_type == SATA_DEV)
		opc = OPC_INB_SATA_ABORT;
	else
		opc = OPC_INB_SMP_ABORT;/* SMP */
	device_id = pm8001_dev->device_id;
	rc = send_task_abort(pm8001_ha, opc, device_id, flag,
		task_tag, cmd_tag);
	if (rc != TMF_RESP_FUNC_COMPLETE)
		PM8001_EH_DBG(pm8001_ha, pm8001_printk("rc= %d\n", rc));
	return rc;
}

/**
 * pm8001_chip_ssp_tm_req - built the task management command.
 * @pm8001_ha: our hba card information.
 * @ccb: the ccb information.
 * @tmf: task management function.
 */
static int pm8001_chip_ssp_tm_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_ccb_info *ccb, struct pm8001_tmf_task *tmf)
{
	struct sas_task *task = ccb->task;
	struct domain_device *dev = task->dev;
	struct pm8001_device *pm8001_dev = dev->lldd_dev;
	u32 opc = OPC_INB_SSPINITMSTART;
	struct inbound_queue_table *circularQ;
	struct ssp_ini_tm_start_req sspTMCmd;
	int ret;

	memset(&sspTMCmd, 0, sizeof(sspTMCmd));
	sspTMCmd.device_id = cpu_to_le32(pm8001_dev->device_id);
	sspTMCmd.relate_tag = cpu_to_le32(tmf->tag_of_task_to_be_managed);
	sspTMCmd.tmf = cpu_to_le32(tmf->tmf);
	memcpy(sspTMCmd.lun, task->ssp_task.LUN, 8);
	sspTMCmd.tag = cpu_to_le32(ccb->ccb_tag);
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &sspTMCmd);
	return ret;
}

static int pm8001_chip_get_nvmd_req(struct pm8001_hba_info *pm8001_ha,
	void *payload)
{
	u32 opc = OPC_INB_GET_NVMD_DATA;
	u32 nvmd_type;
	int rc;
	u32 tag;
	struct pm8001_ccb_info *ccb;
	struct inbound_queue_table *circularQ;
	struct get_nvm_data_req nvmd_req;
	struct fw_control_ex *fw_control_context;
	struct pm8001_ioctl_payload *ioctl_payload = payload;

	nvmd_type = ioctl_payload->minor_function;
	fw_control_context = kzalloc(sizeof(struct fw_control_ex), GFP_KERNEL);
	if (!fw_control_context)
		return -ENOMEM;
	fw_control_context->usrAddr = (u8 *)&ioctl_payload->func_specific[0];
	fw_control_context->len = ioctl_payload->length;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memset(&nvmd_req, 0, sizeof(nvmd_req));
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc) {
		kfree(fw_control_context);
		return rc;
	}
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->ccb_tag = tag;
	ccb->fw_control_context = fw_control_context;
	nvmd_req.tag = cpu_to_le32(tag);

	switch (nvmd_type) {
	case TWI_DEVICE: {
		u32 twi_addr, twi_page_size;
		twi_addr = 0xa8;
		twi_page_size = 2;

		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | twi_addr << 16 |
			twi_page_size << 8 | TWI_DEVICE);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	}
	case C_SEEPROM: {
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | C_SEEPROM);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	}
	case VPD_FLASH: {
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | VPD_FLASH);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	}
	case EXPAN_ROM: {
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | EXPAN_ROM);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	}
	default:
		break;
	}
	rc = mpi_build_cmd(pm8001_ha, circularQ, opc, &nvmd_req);
	return rc;
}

static int pm8001_chip_set_nvmd_req(struct pm8001_hba_info *pm8001_ha,
	void *payload)
{
	u32 opc = OPC_INB_SET_NVMD_DATA;
	u32 nvmd_type;
	int rc;
	u32 tag;
	struct pm8001_ccb_info *ccb;
	struct inbound_queue_table *circularQ;
	struct set_nvm_data_req nvmd_req;
	struct fw_control_ex *fw_control_context;
	struct pm8001_ioctl_payload *ioctl_payload = payload;

	nvmd_type = ioctl_payload->minor_function;
	fw_control_context = kzalloc(sizeof(struct fw_control_ex), GFP_KERNEL);
	if (!fw_control_context)
		return -ENOMEM;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	memcpy(pm8001_ha->memoryMap.region[NVMD].virt_ptr,
		ioctl_payload->func_specific,
		ioctl_payload->length);
	memset(&nvmd_req, 0, sizeof(nvmd_req));
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc) {
		kfree(fw_control_context);
		return rc;
	}
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->fw_control_context = fw_control_context;
	ccb->ccb_tag = tag;
	nvmd_req.tag = cpu_to_le32(tag);
	switch (nvmd_type) {
	case TWI_DEVICE: {
		u32 twi_addr, twi_page_size;
		twi_addr = 0xa8;
		twi_page_size = 2;
		nvmd_req.reserved[0] = cpu_to_le32(0xFEDCBA98);
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | twi_addr << 16 |
			twi_page_size << 8 | TWI_DEVICE);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	}
	case C_SEEPROM:
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | C_SEEPROM);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.reserved[0] = cpu_to_le32(0xFEDCBA98);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	case VPD_FLASH:
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | VPD_FLASH);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.reserved[0] = cpu_to_le32(0xFEDCBA98);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	case EXPAN_ROM:
		nvmd_req.len_ir_vpdd = cpu_to_le32(IPMode | EXPAN_ROM);
		nvmd_req.resp_len = cpu_to_le32(ioctl_payload->length);
		nvmd_req.reserved[0] = cpu_to_le32(0xFEDCBA98);
		nvmd_req.resp_addr_hi =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_hi);
		nvmd_req.resp_addr_lo =
		    cpu_to_le32(pm8001_ha->memoryMap.region[NVMD].phys_addr_lo);
		break;
	default:
		break;
	}
	rc = mpi_build_cmd(pm8001_ha, circularQ, opc, &nvmd_req);
	return rc;
}

/**
 * pm8001_chip_fw_flash_update_build - support the firmware update operation
 * @pm8001_ha: our hba card information.
 * @fw_flash_updata_info: firmware flash update param
 */
static int
pm8001_chip_fw_flash_update_build(struct pm8001_hba_info *pm8001_ha,
	void *fw_flash_updata_info, u32 tag)
{
	struct fw_flash_Update_req payload;
	struct fw_flash_updata_info *info;
	struct inbound_queue_table *circularQ;
	int ret;
	u32 opc = OPC_INB_FW_FLASH_UPDATE;

	memset(&payload, 0, sizeof(struct fw_flash_Update_req));
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	info = fw_flash_updata_info;
	payload.tag = cpu_to_le32(tag);
	payload.cur_image_len = cpu_to_le32(info->cur_image_len);
	payload.cur_image_offset = cpu_to_le32(info->cur_image_offset);
	payload.total_image_len = cpu_to_le32(info->total_image_len);
	payload.len = info->sgl.im_len.len ;
	payload.sgl_addr_lo = lower_32_bits(info->sgl.addr);
	payload.sgl_addr_hi = upper_32_bits(info->sgl.addr);
	ret = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return ret;
}

static int
pm8001_chip_fw_flash_update_req(struct pm8001_hba_info *pm8001_ha,
	void *payload)
{
	struct fw_flash_updata_info flash_update_info;
	struct fw_control_info *fw_control;
	struct fw_control_ex *fw_control_context;
	int rc;
	u32 tag;
	struct pm8001_ccb_info *ccb;
	void *buffer = NULL;
	dma_addr_t phys_addr;
	u32 phys_addr_hi;
	u32 phys_addr_lo;
	struct pm8001_ioctl_payload *ioctl_payload = payload;

	fw_control_context = kzalloc(sizeof(struct fw_control_ex), GFP_KERNEL);
	if (!fw_control_context)
		return -ENOMEM;
	fw_control = (struct fw_control_info *)&ioctl_payload->func_specific[0];
	if (fw_control->len != 0) {
		if (pm8001_mem_alloc(pm8001_ha->pdev,
			(void **)&buffer,
			&phys_addr,
			&phys_addr_hi,
			&phys_addr_lo,
			fw_control->len, 0) != 0) {
				PM8001_FAIL_DBG(pm8001_ha,
					pm8001_printk("Mem alloc failure\n"));
				kfree(fw_control_context);
				return -ENOMEM;
		}
	}
	memcpy(buffer, fw_control->buffer, fw_control->len);
	flash_update_info.sgl.addr = cpu_to_le64(phys_addr);
	flash_update_info.sgl.im_len.len = cpu_to_le32(fw_control->len);
	flash_update_info.sgl.im_len.e = 0;
	flash_update_info.cur_image_offset = fw_control->offset;
	flash_update_info.cur_image_len = fw_control->len;
	flash_update_info.total_image_len = fw_control->size;
	fw_control_context->fw_control = fw_control;
	fw_control_context->virtAddr = buffer;
	fw_control_context->len = fw_control->len;
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc) {
		kfree(fw_control_context);
		return rc;
	}
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->fw_control_context = fw_control_context;
	ccb->ccb_tag = tag;
	rc = pm8001_chip_fw_flash_update_build(pm8001_ha, &flash_update_info,
		tag);
	return rc;
}

static int
pm8001_chip_set_dev_state_req(struct pm8001_hba_info *pm8001_ha,
	struct pm8001_device *pm8001_dev, u32 state)
{
	struct set_dev_state_req payload;
	struct inbound_queue_table *circularQ;
	struct pm8001_ccb_info *ccb;
	int rc;
	u32 tag;
	u32 opc = OPC_INB_SET_DEVICE_STATE;
	memset(&payload, 0, sizeof(payload));
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc)
		return -1;
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->ccb_tag = tag;
	ccb->device = pm8001_dev;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	payload.tag = cpu_to_le32(tag);
	payload.device_id = cpu_to_le32(pm8001_dev->device_id);
	payload.nds = cpu_to_le32(state);
	rc = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return rc;

}

static int
pm8001_chip_sas_re_initialization(struct pm8001_hba_info *pm8001_ha)
{
	struct sas_re_initialization_req payload;
	struct inbound_queue_table *circularQ;
	struct pm8001_ccb_info *ccb;
	int rc;
	u32 tag;
	u32 opc = OPC_INB_SAS_RE_INITIALIZE;
	memset(&payload, 0, sizeof(payload));
	rc = pm8001_tag_alloc(pm8001_ha, &tag);
	if (rc)
		return -1;
	ccb = &pm8001_ha->ccb_info[tag];
	ccb->ccb_tag = tag;
	circularQ = &pm8001_ha->inbnd_q_tbl[0];
	payload.tag = cpu_to_le32(tag);
	payload.SSAHOLT = cpu_to_le32(0xd << 25);
	payload.sata_hol_tmo = cpu_to_le32(80);
	payload.open_reject_cmdretries_data_retries = cpu_to_le32(0xff00ff);
	rc = mpi_build_cmd(pm8001_ha, circularQ, opc, &payload);
	return rc;

}

const struct pm8001_dispatch pm8001_8001_dispatch = {
	.name			= "pmc8001",
	.chip_init		= pm8001_chip_init,
	.chip_soft_rst		= pm8001_chip_soft_rst,
	.chip_rst		= pm8001_hw_chip_rst,
	.chip_iounmap		= pm8001_chip_iounmap,
	.isr			= pm8001_chip_isr,
	.is_our_interupt	= pm8001_chip_is_our_interupt,
	.isr_process_oq		= process_oq,
	.interrupt_enable 	= pm8001_chip_interrupt_enable,
	.interrupt_disable	= pm8001_chip_interrupt_disable,
	.make_prd		= pm8001_chip_make_sg,
	.smp_req		= pm8001_chip_smp_req,
	.ssp_io_req		= pm8001_chip_ssp_io_req,
	.sata_req		= pm8001_chip_sata_req,
	.phy_start_req		= pm8001_chip_phy_start_req,
	.phy_stop_req		= pm8001_chip_phy_stop_req,
	.reg_dev_req		= pm8001_chip_reg_dev_req,
	.dereg_dev_req		= pm8001_chip_dereg_dev_req,
	.phy_ctl_req		= pm8001_chip_phy_ctl_req,
	.task_abort		= pm8001_chip_abort_task,
	.ssp_tm_req		= pm8001_chip_ssp_tm_req,
	.get_nvmd_req		= pm8001_chip_get_nvmd_req,
	.set_nvmd_req		= pm8001_chip_set_nvmd_req,
	.fw_flash_update_req	= pm8001_chip_fw_flash_update_req,
	.set_dev_state_req	= pm8001_chip_set_dev_state_req,
	.sas_re_init_req	= pm8001_chip_sas_re_initialization,
};

