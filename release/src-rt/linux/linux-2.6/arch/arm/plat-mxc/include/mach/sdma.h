#ifndef __MACH_MXC_SDMA_H__
#define __MACH_MXC_SDMA_H__

/**
 * struct sdma_script_start_addrs - SDMA script start pointers
 *
 * start addresses of the different functions in the physical
 * address space of the SDMA engine.
 */
struct sdma_script_start_addrs {
	s32 ap_2_ap_addr;
	s32 ap_2_bp_addr;
	s32 ap_2_ap_fixed_addr;
	s32 bp_2_ap_addr;
	s32 loopback_on_dsp_side_addr;
	s32 mcu_interrupt_only_addr;
	s32 firi_2_per_addr;
	s32 firi_2_mcu_addr;
	s32 per_2_firi_addr;
	s32 mcu_2_firi_addr;
	s32 uart_2_per_addr;
	s32 uart_2_mcu_addr;
	s32 per_2_app_addr;
	s32 mcu_2_app_addr;
	s32 per_2_per_addr;
	s32 uartsh_2_per_addr;
	s32 uartsh_2_mcu_addr;
	s32 per_2_shp_addr;
	s32 mcu_2_shp_addr;
	s32 ata_2_mcu_addr;
	s32 mcu_2_ata_addr;
	s32 app_2_per_addr;
	s32 app_2_mcu_addr;
	s32 shp_2_per_addr;
	s32 shp_2_mcu_addr;
	s32 mshc_2_mcu_addr;
	s32 mcu_2_mshc_addr;
	s32 spdif_2_mcu_addr;
	s32 mcu_2_spdif_addr;
	s32 asrc_2_mcu_addr;
	s32 ext_mem_2_ipu_addr;
	s32 descrambler_addr;
	s32 dptc_dvfs_addr;
	s32 utra_addr;
	s32 ram_code_start_addr;
};

/**
 * struct sdma_platform_data - platform specific data for SDMA engine
 *
 * @sdma_version	The version of this SDMA engine
 * @cpu_name		used to generate the firmware name
 * @to_version		CPU Tape out version
 * @script_addrs	SDMA scripts addresses in SDMA ROM
 */
struct sdma_platform_data {
	int sdma_version;
	char *cpu_name;
	int to_version;
	struct sdma_script_start_addrs *script_addrs;
};

#endif /* __MACH_MXC_SDMA_H__ */
