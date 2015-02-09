/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "drmP.h"
#include "radeon.h"
#include "radeon_asic.h"
#include "radeon_drm.h"
#include "rv770d.h"
#include "atom.h"
#include "avivod.h"

#define R700_PFP_UCODE_SIZE 848
#define R700_PM4_UCODE_SIZE 1360

static void rv770_gpu_init(struct radeon_device *rdev);
void rv770_fini(struct radeon_device *rdev);

/* get temperature in millidegrees */
u32 rv770_get_temp(struct radeon_device *rdev)
{
	u32 temp = (RREG32(CG_MULT_THERMAL_STATUS) & ASIC_T_MASK) >>
		ASIC_T_SHIFT;
	u32 actual_temp = 0;

	if ((temp >> 9) & 1)
		actual_temp = 0;
	else
		actual_temp = (temp >> 1) & 0xff;

	return actual_temp * 1000;
}

void rv770_pm_misc(struct radeon_device *rdev)
{
	int req_ps_idx = rdev->pm.requested_power_state_index;
	int req_cm_idx = rdev->pm.requested_clock_mode_index;
	struct radeon_power_state *ps = &rdev->pm.power_state[req_ps_idx];
	struct radeon_voltage *voltage = &ps->clock_info[req_cm_idx].voltage;

	if ((voltage->type == VOLTAGE_SW) && voltage->voltage) {
		if (voltage->voltage != rdev->pm.current_vddc) {
			radeon_atom_set_voltage(rdev, voltage->voltage);
			rdev->pm.current_vddc = voltage->voltage;
			DRM_DEBUG("Setting: v: %d\n", voltage->voltage);
		}
	}
}

/*
 * GART
 */
int rv770_pcie_gart_enable(struct radeon_device *rdev)
{
	u32 tmp;
	int r, i;

	if (rdev->gart.table.vram.robj == NULL) {
		dev_err(rdev->dev, "No VRAM object for PCIE GART.\n");
		return -EINVAL;
	}
	r = radeon_gart_table_vram_pin(rdev);
	if (r)
		return r;
	radeon_gart_restore(rdev);
	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	WREG32(VM_CONTEXT0_PAGE_TABLE_START_ADDR, rdev->mc.gtt_start >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_END_ADDR, rdev->mc.gtt_end >> 12);
	WREG32(VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, rdev->gart.table_addr >> 12);
	WREG32(VM_CONTEXT0_CNTL, ENABLE_CONTEXT | PAGE_TABLE_DEPTH(0) |
				RANGE_PROTECTION_FAULT_ENABLE_DEFAULT);
	WREG32(VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR,
			(u32)(rdev->dummy_page.addr >> 12));
	for (i = 1; i < 7; i++)
		WREG32(VM_CONTEXT0_CNTL + (i * 4), 0);

	r600_pcie_gart_tlb_flush(rdev);
	rdev->gart.ready = true;
	return 0;
}

void rv770_pcie_gart_disable(struct radeon_device *rdev)
{
	u32 tmp;
	int i, r;

	/* Disable all tables */
	for (i = 0; i < 7; i++)
		WREG32(VM_CONTEXT0_CNTL + (i * 4), 0);

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_FRAGMENT_PROCESSING |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	if (rdev->gart.table.vram.robj) {
		r = radeon_bo_reserve(rdev->gart.table.vram.robj, false);
		if (likely(r == 0)) {
			radeon_bo_kunmap(rdev->gart.table.vram.robj);
			radeon_bo_unpin(rdev->gart.table.vram.robj);
			radeon_bo_unreserve(rdev->gart.table.vram.robj);
		}
	}
}

void rv770_pcie_gart_fini(struct radeon_device *rdev)
{
	radeon_gart_fini(rdev);
	rv770_pcie_gart_disable(rdev);
	radeon_gart_table_vram_free(rdev);
}


void rv770_agp_enable(struct radeon_device *rdev)
{
	u32 tmp;
	int i;

	/* Setup L2 cache */
	WREG32(VM_L2_CNTL, ENABLE_L2_CACHE | ENABLE_L2_FRAGMENT_PROCESSING |
				ENABLE_L2_PTE_CACHE_LRU_UPDATE_BY_WRITE |
				EFFECTIVE_L2_QUEUE_SIZE(7));
	WREG32(VM_L2_CNTL2, 0);
	WREG32(VM_L2_CNTL3, BANK_SELECT(0) | CACHE_UPDATE_MODE(2));
	/* Setup TLB control */
	tmp = ENABLE_L1_TLB | ENABLE_L1_FRAGMENT_PROCESSING |
		SYSTEM_ACCESS_MODE_NOT_IN_SYS |
		SYSTEM_APERTURE_UNMAPPED_ACCESS_PASS_THRU |
		EFFECTIVE_L1_TLB_SIZE(5) | EFFECTIVE_L1_QUEUE_SIZE(5);
	WREG32(MC_VM_MD_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MD_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB0_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB1_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB2_CNTL, tmp);
	WREG32(MC_VM_MB_L1_TLB3_CNTL, tmp);
	for (i = 0; i < 7; i++)
		WREG32(VM_CONTEXT0_CNTL + (i * 4), 0);
}

static void rv770_mc_program(struct radeon_device *rdev)
{
	struct rv515_mc_save save;
	u32 tmp;
	int i, j;

	/* Initialize HDP */
	for (i = 0, j = 0; i < 32; i++, j += 0x18) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
	}
	/* r7xx hw bug.  Read from HDP_DEBUG1 rather
	 * than writing to HDP_REG_COHERENCY_FLUSH_CNTL
	 */
	tmp = RREG32(HDP_DEBUG1);

	rv515_mc_stop(rdev, &save);
	if (r600_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	/* Lockout access through VGA aperture*/
	WREG32(VGA_HDP_CONTROL, VGA_MEMORY_DISABLE);
	/* Update configuration */
	if (rdev->flags & RADEON_IS_AGP) {
		if (rdev->mc.vram_start < rdev->mc.gtt_start) {
			/* VRAM before AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.vram_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.gtt_end >> 12);
		} else {
			/* VRAM after AGP */
			WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
				rdev->mc.gtt_start >> 12);
			WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
				rdev->mc.vram_end >> 12);
		}
	} else {
		WREG32(MC_VM_SYSTEM_APERTURE_LOW_ADDR,
			rdev->mc.vram_start >> 12);
		WREG32(MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
			rdev->mc.vram_end >> 12);
	}
	WREG32(MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, 0);
	tmp = ((rdev->mc.vram_end >> 24) & 0xFFFF) << 16;
	tmp |= ((rdev->mc.vram_start >> 24) & 0xFFFF);
	WREG32(MC_VM_FB_LOCATION, tmp);
	WREG32(HDP_NONSURFACE_BASE, (rdev->mc.vram_start >> 8));
	WREG32(HDP_NONSURFACE_INFO, (2 << 7));
	WREG32(HDP_NONSURFACE_SIZE, 0x3FFFFFFF);
	if (rdev->flags & RADEON_IS_AGP) {
		WREG32(MC_VM_AGP_TOP, rdev->mc.gtt_end >> 16);
		WREG32(MC_VM_AGP_BOT, rdev->mc.gtt_start >> 16);
		WREG32(MC_VM_AGP_BASE, rdev->mc.agp_base >> 22);
	} else {
		WREG32(MC_VM_AGP_BASE, 0);
		WREG32(MC_VM_AGP_TOP, 0x0FFFFFFF);
		WREG32(MC_VM_AGP_BOT, 0x0FFFFFFF);
	}
	if (r600_mc_wait_for_idle(rdev)) {
		dev_warn(rdev->dev, "Wait for MC idle timedout !\n");
	}
	rv515_mc_resume(rdev, &save);
	/* we need to own VRAM, so turn off the VGA renderer here
	 * to stop it overwriting our objects */
	rv515_vga_render_disable(rdev);
}


/*
 * CP.
 */
void r700_cp_stop(struct radeon_device *rdev)
{
	rdev->mc.active_vram_size = rdev->mc.visible_vram_size;
	WREG32(CP_ME_CNTL, (CP_ME_HALT | CP_PFP_HALT));
}

static int rv770_cp_load_microcode(struct radeon_device *rdev)
{
	const __be32 *fw_data;
	int i;

	if (!rdev->me_fw || !rdev->pfp_fw)
		return -EINVAL;

	r700_cp_stop(rdev);
	WREG32(CP_RB_CNTL, RB_NO_UPDATE | (15 << 8) | (3 << 0));

	/* Reset cp */
	WREG32(GRBM_SOFT_RESET, SOFT_RESET_CP);
	RREG32(GRBM_SOFT_RESET);
	mdelay(15);
	WREG32(GRBM_SOFT_RESET, 0);

	fw_data = (const __be32 *)rdev->pfp_fw->data;
	WREG32(CP_PFP_UCODE_ADDR, 0);
	for (i = 0; i < R700_PFP_UCODE_SIZE; i++)
		WREG32(CP_PFP_UCODE_DATA, be32_to_cpup(fw_data++));
	WREG32(CP_PFP_UCODE_ADDR, 0);

	fw_data = (const __be32 *)rdev->me_fw->data;
	WREG32(CP_ME_RAM_WADDR, 0);
	for (i = 0; i < R700_PM4_UCODE_SIZE; i++)
		WREG32(CP_ME_RAM_DATA, be32_to_cpup(fw_data++));

	WREG32(CP_PFP_UCODE_ADDR, 0);
	WREG32(CP_ME_RAM_WADDR, 0);
	WREG32(CP_ME_RAM_RADDR, 0);
	return 0;
}

void r700_cp_fini(struct radeon_device *rdev)
{
	r700_cp_stop(rdev);
	radeon_ring_fini(rdev);
}

/*
 * Core functions
 */
static u32 r700_get_tile_pipe_to_backend_map(struct radeon_device *rdev,
					     u32 num_tile_pipes,
					     u32 num_backends,
					     u32 backend_disable_mask)
{
	u32 backend_map = 0;
	u32 enabled_backends_mask;
	u32 enabled_backends_count;
	u32 cur_pipe;
	u32 swizzle_pipe[R7XX_MAX_PIPES];
	u32 cur_backend;
	u32 i;
	bool force_no_swizzle;

	if (num_tile_pipes > R7XX_MAX_PIPES)
		num_tile_pipes = R7XX_MAX_PIPES;
	if (num_tile_pipes < 1)
		num_tile_pipes = 1;
	if (num_backends > R7XX_MAX_BACKENDS)
		num_backends = R7XX_MAX_BACKENDS;
	if (num_backends < 1)
		num_backends = 1;

	enabled_backends_mask = 0;
	enabled_backends_count = 0;
	for (i = 0; i < R7XX_MAX_BACKENDS; ++i) {
		if (((backend_disable_mask >> i) & 1) == 0) {
			enabled_backends_mask |= (1 << i);
			++enabled_backends_count;
		}
		if (enabled_backends_count == num_backends)
			break;
	}

	if (enabled_backends_count == 0) {
		enabled_backends_mask = 1;
		enabled_backends_count = 1;
	}

	if (enabled_backends_count != num_backends)
		num_backends = enabled_backends_count;

	switch (rdev->family) {
	case CHIP_RV770:
	case CHIP_RV730:
		force_no_swizzle = false;
		break;
	case CHIP_RV710:
	case CHIP_RV740:
	default:
		force_no_swizzle = true;
		break;
	}

	memset((uint8_t *)&swizzle_pipe[0], 0, sizeof(u32) * R7XX_MAX_PIPES);
	switch (num_tile_pipes) {
	case 1:
		swizzle_pipe[0] = 0;
		break;
	case 2:
		swizzle_pipe[0] = 0;
		swizzle_pipe[1] = 1;
		break;
	case 3:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 1;
		}
		break;
	case 4:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 3;
			swizzle_pipe[3] = 1;
		}
		break;
	case 5:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 1;
			swizzle_pipe[4] = 3;
		}
		break;
	case 6:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
			swizzle_pipe[5] = 5;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 5;
			swizzle_pipe[4] = 3;
			swizzle_pipe[5] = 1;
		}
		break;
	case 7:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
			swizzle_pipe[5] = 5;
			swizzle_pipe[6] = 6;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 6;
			swizzle_pipe[4] = 3;
			swizzle_pipe[5] = 1;
			swizzle_pipe[6] = 5;
		}
		break;
	case 8:
		if (force_no_swizzle) {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 1;
			swizzle_pipe[2] = 2;
			swizzle_pipe[3] = 3;
			swizzle_pipe[4] = 4;
			swizzle_pipe[5] = 5;
			swizzle_pipe[6] = 6;
			swizzle_pipe[7] = 7;
		} else {
			swizzle_pipe[0] = 0;
			swizzle_pipe[1] = 2;
			swizzle_pipe[2] = 4;
			swizzle_pipe[3] = 6;
			swizzle_pipe[4] = 3;
			swizzle_pipe[5] = 1;
			swizzle_pipe[6] = 7;
			swizzle_pipe[7] = 5;
		}
		break;
	}

	cur_backend = 0;
	for (cur_pipe = 0; cur_pipe < num_tile_pipes; ++cur_pipe) {
		while (((1 << cur_backend) & enabled_backends_mask) == 0)
			cur_backend = (cur_backend + 1) % R7XX_MAX_BACKENDS;

		backend_map |= (u32)(((cur_backend & 3) << (swizzle_pipe[cur_pipe] * 2)));

		cur_backend = (cur_backend + 1) % R7XX_MAX_BACKENDS;
	}

	return backend_map;
}

static void rv770_gpu_init(struct radeon_device *rdev)
{
	int i, j, num_qd_pipes;
	u32 ta_aux_cntl;
	u32 sx_debug_1;
	u32 smx_dc_ctl0;
	u32 db_debug3;
	u32 num_gs_verts_per_thread;
	u32 vgt_gs_per_es;
	u32 gs_prim_buffer_depth = 0;
	u32 sq_ms_fifo_sizes;
	u32 sq_config;
	u32 sq_thread_resource_mgmt;
	u32 hdp_host_path_cntl;
	u32 sq_dyn_gpr_size_simd_ab_0;
	u32 backend_map;
	u32 gb_tiling_config = 0;
	u32 cc_rb_backend_disable = 0;
	u32 cc_gc_shader_pipe_config = 0;
	u32 mc_arb_ramcfg;
	u32 db_debug4;

	/* setup chip specs */
	switch (rdev->family) {
	case CHIP_RV770:
		rdev->config.rv770.max_pipes = 4;
		rdev->config.rv770.max_tile_pipes = 8;
		rdev->config.rv770.max_simds = 10;
		rdev->config.rv770.max_backends = 4;
		rdev->config.rv770.max_gprs = 256;
		rdev->config.rv770.max_threads = 248;
		rdev->config.rv770.max_stack_entries = 512;
		rdev->config.rv770.max_hw_contexts = 8;
		rdev->config.rv770.max_gs_threads = 16 * 2;
		rdev->config.rv770.sx_max_export_size = 128;
		rdev->config.rv770.sx_max_export_pos_size = 16;
		rdev->config.rv770.sx_max_export_smx_size = 112;
		rdev->config.rv770.sq_num_cf_insts = 2;

		rdev->config.rv770.sx_num_of_sets = 7;
		rdev->config.rv770.sc_prim_fifo_size = 0xF9;
		rdev->config.rv770.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.rv770.sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV730:
		rdev->config.rv770.max_pipes = 2;
		rdev->config.rv770.max_tile_pipes = 4;
		rdev->config.rv770.max_simds = 8;
		rdev->config.rv770.max_backends = 2;
		rdev->config.rv770.max_gprs = 128;
		rdev->config.rv770.max_threads = 248;
		rdev->config.rv770.max_stack_entries = 256;
		rdev->config.rv770.max_hw_contexts = 8;
		rdev->config.rv770.max_gs_threads = 16 * 2;
		rdev->config.rv770.sx_max_export_size = 256;
		rdev->config.rv770.sx_max_export_pos_size = 32;
		rdev->config.rv770.sx_max_export_smx_size = 224;
		rdev->config.rv770.sq_num_cf_insts = 2;

		rdev->config.rv770.sx_num_of_sets = 7;
		rdev->config.rv770.sc_prim_fifo_size = 0xf9;
		rdev->config.rv770.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.rv770.sc_earlyz_tile_fifo_fize = 0x130;
		if (rdev->config.rv770.sx_max_export_pos_size > 16) {
			rdev->config.rv770.sx_max_export_pos_size -= 16;
			rdev->config.rv770.sx_max_export_smx_size += 16;
		}
		break;
	case CHIP_RV710:
		rdev->config.rv770.max_pipes = 2;
		rdev->config.rv770.max_tile_pipes = 2;
		rdev->config.rv770.max_simds = 2;
		rdev->config.rv770.max_backends = 1;
		rdev->config.rv770.max_gprs = 256;
		rdev->config.rv770.max_threads = 192;
		rdev->config.rv770.max_stack_entries = 256;
		rdev->config.rv770.max_hw_contexts = 4;
		rdev->config.rv770.max_gs_threads = 8 * 2;
		rdev->config.rv770.sx_max_export_size = 128;
		rdev->config.rv770.sx_max_export_pos_size = 16;
		rdev->config.rv770.sx_max_export_smx_size = 112;
		rdev->config.rv770.sq_num_cf_insts = 1;

		rdev->config.rv770.sx_num_of_sets = 7;
		rdev->config.rv770.sc_prim_fifo_size = 0x40;
		rdev->config.rv770.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.rv770.sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV740:
		rdev->config.rv770.max_pipes = 4;
		rdev->config.rv770.max_tile_pipes = 4;
		rdev->config.rv770.max_simds = 8;
		rdev->config.rv770.max_backends = 4;
		rdev->config.rv770.max_gprs = 256;
		rdev->config.rv770.max_threads = 248;
		rdev->config.rv770.max_stack_entries = 512;
		rdev->config.rv770.max_hw_contexts = 8;
		rdev->config.rv770.max_gs_threads = 16 * 2;
		rdev->config.rv770.sx_max_export_size = 256;
		rdev->config.rv770.sx_max_export_pos_size = 32;
		rdev->config.rv770.sx_max_export_smx_size = 224;
		rdev->config.rv770.sq_num_cf_insts = 2;

		rdev->config.rv770.sx_num_of_sets = 7;
		rdev->config.rv770.sc_prim_fifo_size = 0x100;
		rdev->config.rv770.sc_hiz_tile_fifo_size = 0x30;
		rdev->config.rv770.sc_earlyz_tile_fifo_fize = 0x130;

		if (rdev->config.rv770.sx_max_export_pos_size > 16) {
			rdev->config.rv770.sx_max_export_pos_size -= 16;
			rdev->config.rv770.sx_max_export_smx_size += 16;
		}
		break;
	default:
		break;
	}

	/* Initialize HDP */
	j = 0;
	for (i = 0; i < 32; i++) {
		WREG32((0x2c14 + j), 0x00000000);
		WREG32((0x2c18 + j), 0x00000000);
		WREG32((0x2c1c + j), 0x00000000);
		WREG32((0x2c20 + j), 0x00000000);
		WREG32((0x2c24 + j), 0x00000000);
		j += 0x18;
	}

	WREG32(GRBM_CNTL, GRBM_READ_TIMEOUT(0xff));

	/* setup tiling, simd, pipe config */
	mc_arb_ramcfg = RREG32(MC_ARB_RAMCFG);

	switch (rdev->config.rv770.max_tile_pipes) {
	case 1:
	default:
		gb_tiling_config |= PIPE_TILING(0);
		break;
	case 2:
		gb_tiling_config |= PIPE_TILING(1);
		break;
	case 4:
		gb_tiling_config |= PIPE_TILING(2);
		break;
	case 8:
		gb_tiling_config |= PIPE_TILING(3);
		break;
	}
	rdev->config.rv770.tiling_npipes = rdev->config.rv770.max_tile_pipes;

	if (rdev->family == CHIP_RV770)
		gb_tiling_config |= BANK_TILING(1);
	else
		gb_tiling_config |= BANK_TILING((mc_arb_ramcfg & NOOFBANK_MASK) >> NOOFBANK_SHIFT);
	rdev->config.rv770.tiling_nbanks = 4 << ((gb_tiling_config >> 4) & 0x3);
	gb_tiling_config |= GROUP_SIZE((mc_arb_ramcfg & BURSTLENGTH_MASK) >> BURSTLENGTH_SHIFT);
	if ((mc_arb_ramcfg & BURSTLENGTH_MASK) >> BURSTLENGTH_SHIFT)
		rdev->config.rv770.tiling_group_size = 512;
	else
		rdev->config.rv770.tiling_group_size = 256;
	if (((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT) > 3) {
		gb_tiling_config |= ROW_TILING(3);
		gb_tiling_config |= SAMPLE_SPLIT(3);
	} else {
		gb_tiling_config |=
			ROW_TILING(((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT));
		gb_tiling_config |=
			SAMPLE_SPLIT(((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT));
	}

	gb_tiling_config |= BANK_SWAPS(1);

	cc_rb_backend_disable = RREG32(CC_RB_BACKEND_DISABLE) & 0x00ff0000;
	cc_rb_backend_disable |=
		BACKEND_DISABLE((R7XX_MAX_BACKENDS_MASK << rdev->config.rv770.max_backends) & R7XX_MAX_BACKENDS_MASK);

	cc_gc_shader_pipe_config = RREG32(CC_GC_SHADER_PIPE_CONFIG) & 0xffffff00;
	cc_gc_shader_pipe_config |=
		INACTIVE_QD_PIPES((R7XX_MAX_PIPES_MASK << rdev->config.rv770.max_pipes) & R7XX_MAX_PIPES_MASK);
	cc_gc_shader_pipe_config |=
		INACTIVE_SIMDS((R7XX_MAX_SIMDS_MASK << rdev->config.rv770.max_simds) & R7XX_MAX_SIMDS_MASK);

	if (rdev->family == CHIP_RV740)
		backend_map = 0x28;
	else
		backend_map = r700_get_tile_pipe_to_backend_map(rdev,
								rdev->config.rv770.max_tile_pipes,
								(R7XX_MAX_BACKENDS -
								 r600_count_pipe_bits((cc_rb_backend_disable &
										       R7XX_MAX_BACKENDS_MASK) >> 16)),
								(cc_rb_backend_disable >> 16));

	rdev->config.rv770.tile_config = gb_tiling_config;
	gb_tiling_config |= BACKEND_MAP(backend_map);

	WREG32(GB_TILING_CONFIG, gb_tiling_config);
	WREG32(DCP_TILING_CONFIG, (gb_tiling_config & 0xffff));
	WREG32(HDP_TILING_CONFIG, (gb_tiling_config & 0xffff));

	WREG32(CC_RB_BACKEND_DISABLE,      cc_rb_backend_disable);
	WREG32(CC_GC_SHADER_PIPE_CONFIG,   cc_gc_shader_pipe_config);
	WREG32(GC_USER_SHADER_PIPE_CONFIG, cc_gc_shader_pipe_config);
	WREG32(CC_SYS_RB_BACKEND_DISABLE,  cc_rb_backend_disable);

	WREG32(CGTS_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_SYS_TCC_DISABLE, 0);
	WREG32(CGTS_USER_TCC_DISABLE, 0);

	num_qd_pipes =
		R7XX_MAX_PIPES - r600_count_pipe_bits((cc_gc_shader_pipe_config & INACTIVE_QD_PIPES_MASK) >> 8);
	WREG32(VGT_OUT_DEALLOC_CNTL, (num_qd_pipes * 4) & DEALLOC_DIST_MASK);
	WREG32(VGT_VERTEX_REUSE_BLOCK_CNTL, ((num_qd_pipes * 4) - 2) & VTX_REUSE_DEPTH_MASK);

	/* set HW defaults for 3D engine */
	WREG32(CP_QUEUE_THRESHOLDS, (ROQ_IB1_START(0x16) |
				     ROQ_IB2_START(0x2b)));

	WREG32(CP_MEQ_THRESHOLDS, STQ_SPLIT(0x30));

	ta_aux_cntl = RREG32(TA_CNTL_AUX);
	WREG32(TA_CNTL_AUX, ta_aux_cntl | DISABLE_CUBE_ANISO);

	sx_debug_1 = RREG32(SX_DEBUG_1);
	sx_debug_1 |= ENABLE_NEW_SMX_ADDRESS;
	WREG32(SX_DEBUG_1, sx_debug_1);

	smx_dc_ctl0 = RREG32(SMX_DC_CTL0);
	smx_dc_ctl0 &= ~CACHE_DEPTH(0x1ff);
	smx_dc_ctl0 |= CACHE_DEPTH((rdev->config.rv770.sx_num_of_sets * 64) - 1);
	WREG32(SMX_DC_CTL0, smx_dc_ctl0);

	if (rdev->family != CHIP_RV740)
		WREG32(SMX_EVENT_CTL, (ES_FLUSH_CTL(4) |
				       GS_FLUSH_CTL(4) |
				       ACK_FLUSH_CTL(3) |
				       SYNC_FLUSH_CTL));

	db_debug3 = RREG32(DB_DEBUG3);
	db_debug3 &= ~DB_CLK_OFF_DELAY(0x1f);
	switch (rdev->family) {
	case CHIP_RV770:
	case CHIP_RV740:
		db_debug3 |= DB_CLK_OFF_DELAY(0x1f);
		break;
	case CHIP_RV710:
	case CHIP_RV730:
	default:
		db_debug3 |= DB_CLK_OFF_DELAY(2);
		break;
	}
	WREG32(DB_DEBUG3, db_debug3);

	if (rdev->family != CHIP_RV770) {
		db_debug4 = RREG32(DB_DEBUG4);
		db_debug4 |= DISABLE_TILE_COVERED_FOR_PS_ITER;
		WREG32(DB_DEBUG4, db_debug4);
	}

	WREG32(SX_EXPORT_BUFFER_SIZES, (COLOR_BUFFER_SIZE((rdev->config.rv770.sx_max_export_size / 4) - 1) |
					POSITION_BUFFER_SIZE((rdev->config.rv770.sx_max_export_pos_size / 4) - 1) |
					SMX_BUFFER_SIZE((rdev->config.rv770.sx_max_export_smx_size / 4) - 1)));

	WREG32(PA_SC_FIFO_SIZE, (SC_PRIM_FIFO_SIZE(rdev->config.rv770.sc_prim_fifo_size) |
				 SC_HIZ_TILE_FIFO_SIZE(rdev->config.rv770.sc_hiz_tile_fifo_size) |
				 SC_EARLYZ_TILE_FIFO_SIZE(rdev->config.rv770.sc_earlyz_tile_fifo_fize)));

	WREG32(PA_SC_MULTI_CHIP_CNTL, 0);

	WREG32(VGT_NUM_INSTANCES, 1);

	WREG32(SPI_CONFIG_CNTL, GPR_WRITE_PRIORITY(0));

	WREG32(SPI_CONFIG_CNTL_1, VTX_DONE_DELAY(4));

	WREG32(CP_PERFMON_CNTL, 0);

	sq_ms_fifo_sizes = (CACHE_FIFO_SIZE(16 * rdev->config.rv770.sq_num_cf_insts) |
			    DONE_FIFO_HIWATER(0xe0) |
			    ALU_UPDATE_FIFO_HIWATER(0x8));
	switch (rdev->family) {
	case CHIP_RV770:
	case CHIP_RV730:
	case CHIP_RV710:
		sq_ms_fifo_sizes |= FETCH_FIFO_HIWATER(0x1);
		break;
	case CHIP_RV740:
	default:
		sq_ms_fifo_sizes |= FETCH_FIFO_HIWATER(0x4);
		break;
	}
	WREG32(SQ_MS_FIFO_SIZES, sq_ms_fifo_sizes);

	/* SQ_CONFIG, SQ_GPR_RESOURCE_MGMT, SQ_THREAD_RESOURCE_MGMT, SQ_STACK_RESOURCE_MGMT
	 * should be adjusted as needed by the 2D/3D drivers.  This just sets default values
	 */
	sq_config = RREG32(SQ_CONFIG);
	sq_config &= ~(PS_PRIO(3) |
		       VS_PRIO(3) |
		       GS_PRIO(3) |
		       ES_PRIO(3));
	sq_config |= (DX9_CONSTS |
		      VC_ENABLE |
		      EXPORT_SRC_C |
		      PS_PRIO(0) |
		      VS_PRIO(1) |
		      GS_PRIO(2) |
		      ES_PRIO(3));
	if (rdev->family == CHIP_RV710)
		/* no vertex cache */
		sq_config &= ~VC_ENABLE;

	WREG32(SQ_CONFIG, sq_config);

	WREG32(SQ_GPR_RESOURCE_MGMT_1,  (NUM_PS_GPRS((rdev->config.rv770.max_gprs * 24)/64) |
					 NUM_VS_GPRS((rdev->config.rv770.max_gprs * 24)/64) |
					 NUM_CLAUSE_TEMP_GPRS(((rdev->config.rv770.max_gprs * 24)/64)/2)));

	WREG32(SQ_GPR_RESOURCE_MGMT_2,  (NUM_GS_GPRS((rdev->config.rv770.max_gprs * 7)/64) |
					 NUM_ES_GPRS((rdev->config.rv770.max_gprs * 7)/64)));

	sq_thread_resource_mgmt = (NUM_PS_THREADS((rdev->config.rv770.max_threads * 4)/8) |
				   NUM_VS_THREADS((rdev->config.rv770.max_threads * 2)/8) |
				   NUM_ES_THREADS((rdev->config.rv770.max_threads * 1)/8));
	if (((rdev->config.rv770.max_threads * 1) / 8) > rdev->config.rv770.max_gs_threads)
		sq_thread_resource_mgmt |= NUM_GS_THREADS(rdev->config.rv770.max_gs_threads);
	else
		sq_thread_resource_mgmt |= NUM_GS_THREADS((rdev->config.rv770.max_gs_threads * 1)/8);
	WREG32(SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);

	WREG32(SQ_STACK_RESOURCE_MGMT_1, (NUM_PS_STACK_ENTRIES((rdev->config.rv770.max_stack_entries * 1)/4) |
						     NUM_VS_STACK_ENTRIES((rdev->config.rv770.max_stack_entries * 1)/4)));

	WREG32(SQ_STACK_RESOURCE_MGMT_2, (NUM_GS_STACK_ENTRIES((rdev->config.rv770.max_stack_entries * 1)/4) |
						     NUM_ES_STACK_ENTRIES((rdev->config.rv770.max_stack_entries * 1)/4)));

	sq_dyn_gpr_size_simd_ab_0 = (SIMDA_RING0((rdev->config.rv770.max_gprs * 38)/64) |
				     SIMDA_RING1((rdev->config.rv770.max_gprs * 38)/64) |
				     SIMDB_RING0((rdev->config.rv770.max_gprs * 38)/64) |
				     SIMDB_RING1((rdev->config.rv770.max_gprs * 38)/64));

	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_0, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_1, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_2, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_3, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_4, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_5, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_6, sq_dyn_gpr_size_simd_ab_0);
	WREG32(SQ_DYN_GPR_SIZE_SIMD_AB_7, sq_dyn_gpr_size_simd_ab_0);

	WREG32(PA_SC_FORCE_EOV_MAX_CNTS, (FORCE_EOV_MAX_CLK_CNT(4095) |
					  FORCE_EOV_MAX_REZ_CNT(255)));

	if (rdev->family == CHIP_RV710)
		WREG32(VGT_CACHE_INVALIDATION, (CACHE_INVALIDATION(TC_ONLY) |
						AUTO_INVLD_EN(ES_AND_GS_AUTO)));
	else
		WREG32(VGT_CACHE_INVALIDATION, (CACHE_INVALIDATION(VC_AND_TC) |
						AUTO_INVLD_EN(ES_AND_GS_AUTO)));

	switch (rdev->family) {
	case CHIP_RV770:
	case CHIP_RV730:
	case CHIP_RV740:
		gs_prim_buffer_depth = 384;
		break;
	case CHIP_RV710:
		gs_prim_buffer_depth = 128;
		break;
	default:
		break;
	}

	num_gs_verts_per_thread = rdev->config.rv770.max_pipes * 16;
	vgt_gs_per_es = gs_prim_buffer_depth + num_gs_verts_per_thread;
	/* Max value for this is 256 */
	if (vgt_gs_per_es > 256)
		vgt_gs_per_es = 256;

	WREG32(VGT_ES_PER_GS, 128);
	WREG32(VGT_GS_PER_ES, vgt_gs_per_es);
	WREG32(VGT_GS_PER_VS, 2);

	/* more default values. 2D/3D driver should adjust as needed */
	WREG32(VGT_GS_VERTEX_REUSE, 16);
	WREG32(PA_SC_LINE_STIPPLE_STATE, 0);
	WREG32(VGT_STRMOUT_EN, 0);
	WREG32(SX_MISC, 0);
	WREG32(PA_SC_MODE_CNTL, 0);
	WREG32(PA_SC_EDGERULE, 0xaaaaaaaa);
	WREG32(PA_SC_AA_CONFIG, 0);
	WREG32(PA_SC_CLIPRECT_RULE, 0xffff);
	WREG32(PA_SC_LINE_STIPPLE, 0);
	WREG32(SPI_INPUT_Z, 0);
	WREG32(SPI_PS_IN_CONTROL_0, NUM_INTERP(2));
	WREG32(CB_COLOR7_FRAG, 0);

	/* clear render buffer base addresses */
	WREG32(CB_COLOR0_BASE, 0);
	WREG32(CB_COLOR1_BASE, 0);
	WREG32(CB_COLOR2_BASE, 0);
	WREG32(CB_COLOR3_BASE, 0);
	WREG32(CB_COLOR4_BASE, 0);
	WREG32(CB_COLOR5_BASE, 0);
	WREG32(CB_COLOR6_BASE, 0);
	WREG32(CB_COLOR7_BASE, 0);

	WREG32(TCP_CNTL, 0);

	hdp_host_path_cntl = RREG32(HDP_HOST_PATH_CNTL);
	WREG32(HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	WREG32(PA_SC_MULTI_CHIP_CNTL, 0);

	WREG32(PA_CL_ENHANCE, (CLIP_VTX_REORDER_ENA |
					  NUM_CLIP_SEQ(3)));

}

static int rv770_vram_scratch_init(struct radeon_device *rdev)
{
	int r;
	u64 gpu_addr;

	if (rdev->vram_scratch.robj == NULL) {
		r = radeon_bo_create(rdev, NULL, RADEON_GPU_PAGE_SIZE,
					true, RADEON_GEM_DOMAIN_VRAM,
					&rdev->vram_scratch.robj);
		if (r) {
			return r;
		}
	}

	r = radeon_bo_reserve(rdev->vram_scratch.robj, false);
	if (unlikely(r != 0))
		return r;
	r = radeon_bo_pin(rdev->vram_scratch.robj,
			  RADEON_GEM_DOMAIN_VRAM, &gpu_addr);
	if (r) {
		radeon_bo_unreserve(rdev->vram_scratch.robj);
		return r;
	}
	r = radeon_bo_kmap(rdev->vram_scratch.robj,
				(void **)&rdev->vram_scratch.ptr);
	if (r)
		radeon_bo_unpin(rdev->vram_scratch.robj);
	radeon_bo_unreserve(rdev->vram_scratch.robj);

	return r;
}

static void rv770_vram_scratch_fini(struct radeon_device *rdev)
{
	int r;

	if (rdev->vram_scratch.robj == NULL) {
		return;
	}
	r = radeon_bo_reserve(rdev->vram_scratch.robj, false);
	if (likely(r == 0)) {
		radeon_bo_kunmap(rdev->vram_scratch.robj);
		radeon_bo_unpin(rdev->vram_scratch.robj);
		radeon_bo_unreserve(rdev->vram_scratch.robj);
	}
	radeon_bo_unref(&rdev->vram_scratch.robj);
}

int rv770_mc_init(struct radeon_device *rdev)
{
	u32 tmp;
	int chansize, numchan;

	/* Get VRAM informations */
	rdev->mc.vram_is_ddr = true;
	tmp = RREG32(MC_ARB_RAMCFG);
	if (tmp & CHANSIZE_OVERRIDE) {
		chansize = 16;
	} else if (tmp & CHANSIZE_MASK) {
		chansize = 64;
	} else {
		chansize = 32;
	}
	tmp = RREG32(MC_SHARED_CHMAP);
	switch ((tmp & NOOFCHAN_MASK) >> NOOFCHAN_SHIFT) {
	case 0:
	default:
		numchan = 1;
		break;
	case 1:
		numchan = 2;
		break;
	case 2:
		numchan = 4;
		break;
	case 3:
		numchan = 8;
		break;
	}
	rdev->mc.vram_width = numchan * chansize;
	/* Could aper size report 0 ? */
	rdev->mc.aper_base = pci_resource_start(rdev->pdev, 0);
	rdev->mc.aper_size = pci_resource_len(rdev->pdev, 0);
	/* Setup GPU memory space */
	rdev->mc.mc_vram_size = RREG32(CONFIG_MEMSIZE);
	rdev->mc.real_vram_size = RREG32(CONFIG_MEMSIZE);
	rdev->mc.visible_vram_size = rdev->mc.aper_size;
	rdev->mc.active_vram_size = rdev->mc.visible_vram_size;
	r600_vram_gtt_location(rdev, &rdev->mc);
	radeon_update_bandwidth_info(rdev);

	return 0;
}

static int rv770_startup(struct radeon_device *rdev)
{
	int r;

	if (!rdev->me_fw || !rdev->pfp_fw || !rdev->rlc_fw) {
		r = r600_init_microcode(rdev);
		if (r) {
			DRM_ERROR("Failed to load firmware!\n");
			return r;
		}
	}

	rv770_mc_program(rdev);
	if (rdev->flags & RADEON_IS_AGP) {
		rv770_agp_enable(rdev);
	} else {
		r = rv770_pcie_gart_enable(rdev);
		if (r)
			return r;
	}
	r = rv770_vram_scratch_init(rdev);
	if (r)
		return r;
	rv770_gpu_init(rdev);
	r = r600_blit_init(rdev);
	if (r) {
		r600_blit_fini(rdev);
		rdev->asic->copy = NULL;
		dev_warn(rdev->dev, "failed blitter (%d) falling back to memcpy\n", r);
	}
	/* pin copy shader into vram */
	if (rdev->r600_blit.shader_obj) {
		r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
		if (unlikely(r != 0))
			return r;
		r = radeon_bo_pin(rdev->r600_blit.shader_obj, RADEON_GEM_DOMAIN_VRAM,
				&rdev->r600_blit.shader_gpu_addr);
		radeon_bo_unreserve(rdev->r600_blit.shader_obj);
		if (r) {
			DRM_ERROR("failed to pin blit object %d\n", r);
			return r;
		}
	}
	/* Enable IRQ */
	r = r600_irq_init(rdev);
	if (r) {
		DRM_ERROR("radeon: IH init failed (%d).\n", r);
		radeon_irq_kms_fini(rdev);
		return r;
	}
	r600_irq_set(rdev);

	r = radeon_ring_init(rdev, rdev->cp.ring_size);
	if (r)
		return r;
	r = rv770_cp_load_microcode(rdev);
	if (r)
		return r;
	r = r600_cp_resume(rdev);
	if (r)
		return r;
	/* write back buffer are not vital so don't worry about failure */
	r600_wb_enable(rdev);
	return 0;
}

int rv770_resume(struct radeon_device *rdev)
{
	int r;

	/* Do not reset GPU before posting, on rv770 hw unlike on r500 hw,
	 * posting will perform necessary task to bring back GPU into good
	 * shape.
	 */
	/* post card */
	atom_asic_init(rdev->mode_info.atom_context);

	r = rv770_startup(rdev);
	if (r) {
		DRM_ERROR("r600 startup failed on resume\n");
		return r;
	}

	r = r600_ib_test(rdev);
	if (r) {
		DRM_ERROR("radeon: failled testing IB (%d).\n", r);
		return r;
	}

	r = r600_audio_init(rdev);
	if (r) {
		dev_err(rdev->dev, "radeon: audio init failed\n");
		return r;
	}

	return r;

}

int rv770_suspend(struct radeon_device *rdev)
{
	int r;

	r600_audio_fini(rdev);
	r700_cp_stop(rdev);
	rdev->cp.ready = false;
	r600_irq_suspend(rdev);
	r600_wb_disable(rdev);
	rv770_pcie_gart_disable(rdev);
	/* unpin shaders bo */
	if (rdev->r600_blit.shader_obj) {
		r = radeon_bo_reserve(rdev->r600_blit.shader_obj, false);
		if (likely(r == 0)) {
			radeon_bo_unpin(rdev->r600_blit.shader_obj);
			radeon_bo_unreserve(rdev->r600_blit.shader_obj);
		}
	}
	return 0;
}

/* Plan is to move initialization in that function and use
 * helper function so that radeon_device_init pretty much
 * do nothing more than calling asic specific function. This
 * should also allow to remove a bunch of callback function
 * like vram_info.
 */
int rv770_init(struct radeon_device *rdev)
{
	int r;

	r = radeon_dummy_page_init(rdev);
	if (r)
		return r;
	/* This don't do much */
	r = radeon_gem_init(rdev);
	if (r)
		return r;
	/* Read BIOS */
	if (!radeon_get_bios(rdev)) {
		if (ASIC_IS_AVIVO(rdev))
			return -EINVAL;
	}
	/* Must be an ATOMBIOS */
	if (!rdev->is_atom_bios) {
		dev_err(rdev->dev, "Expecting atombios for R600 GPU\n");
		return -EINVAL;
	}
	r = radeon_atombios_init(rdev);
	if (r)
		return r;
	/* Post card if necessary */
	if (!r600_card_posted(rdev)) {
		if (!rdev->bios) {
			dev_err(rdev->dev, "Card not posted and no BIOS - ignoring\n");
			return -EINVAL;
		}
		DRM_INFO("GPU not posted. posting now...\n");
		atom_asic_init(rdev->mode_info.atom_context);
	}
	/* Initialize scratch registers */
	r600_scratch_init(rdev);
	/* Initialize surface registers */
	radeon_surface_init(rdev);
	/* Initialize clocks */
	radeon_get_clock_info(rdev->ddev);
	/* Fence driver */
	r = radeon_fence_driver_init(rdev);
	if (r)
		return r;
	/* initialize AGP */
	if (rdev->flags & RADEON_IS_AGP) {
		r = radeon_agp_init(rdev);
		if (r)
			radeon_agp_disable(rdev);
	}
	r = rv770_mc_init(rdev);
	if (r)
		return r;
	/* Memory manager */
	r = radeon_bo_init(rdev);
	if (r)
		return r;

	r = radeon_irq_kms_init(rdev);
	if (r)
		return r;

	rdev->cp.ring_obj = NULL;
	r600_ring_init(rdev, 1024 * 1024);

	rdev->ih.ring_obj = NULL;
	r600_ih_ring_init(rdev, 64 * 1024);

	r = r600_pcie_gart_init(rdev);
	if (r)
		return r;

	rdev->accel_working = true;
	r = rv770_startup(rdev);
	if (r) {
		dev_err(rdev->dev, "disabling GPU acceleration\n");
		r700_cp_fini(rdev);
		r600_wb_fini(rdev);
		r600_irq_fini(rdev);
		radeon_irq_kms_fini(rdev);
		rv770_pcie_gart_fini(rdev);
		rdev->accel_working = false;
	}
	if (rdev->accel_working) {
		r = radeon_ib_pool_init(rdev);
		if (r) {
			dev_err(rdev->dev, "IB initialization failed (%d).\n", r);
			rdev->accel_working = false;
		} else {
			r = r600_ib_test(rdev);
			if (r) {
				dev_err(rdev->dev, "IB test failed (%d).\n", r);
				rdev->accel_working = false;
			}
		}
	}

	r = r600_audio_init(rdev);
	if (r) {
		dev_err(rdev->dev, "radeon: audio init failed\n");
		return r;
	}

	return 0;
}

void rv770_fini(struct radeon_device *rdev)
{
	r600_blit_fini(rdev);
	r700_cp_fini(rdev);
	r600_wb_fini(rdev);
	r600_irq_fini(rdev);
	radeon_irq_kms_fini(rdev);
	rv770_pcie_gart_fini(rdev);
	rv770_vram_scratch_fini(rdev);
	radeon_gem_fini(rdev);
	radeon_fence_driver_fini(rdev);
	radeon_agp_fini(rdev);
	radeon_bo_fini(rdev);
	radeon_atombios_fini(rdev);
	kfree(rdev->bios);
	rdev->bios = NULL;
	radeon_dummy_page_fini(rdev);
}
