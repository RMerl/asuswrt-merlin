/*
 * This file is part of wl1271
 *
 * Copyright (C) 2008-2010 Nokia Corporation
 *
 * Contact: Luciano Coelho <luciano.coelho@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/gpio.h>
#include <linux/slab.h>

#include "wl1271_acx.h"
#include "wl1271_reg.h"
#include "wl1271_boot.h"
#include "wl1271_io.h"
#include "wl1271_event.h"

static struct wl1271_partition_set part_table[PART_TABLE_LEN] = {
	[PART_DOWN] = {
		.mem = {
			.start = 0x00000000,
			.size  = 0x000177c0
		},
		.reg = {
			.start = REGISTERS_BASE,
			.size  = 0x00008800
		},
		.mem2 = {
			.start = 0x00000000,
			.size  = 0x00000000
		},
		.mem3 = {
			.start = 0x00000000,
			.size  = 0x00000000
		},
	},

	[PART_WORK] = {
		.mem = {
			.start = 0x00040000,
			.size  = 0x00014fc0
		},
		.reg = {
			.start = REGISTERS_BASE,
			.size  = 0x0000a000
		},
		.mem2 = {
			.start = 0x003004f8,
			.size  = 0x00000004
		},
		.mem3 = {
			.start = 0x00040404,
			.size  = 0x00000000
		},
	},

	[PART_DRPW] = {
		.mem = {
			.start = 0x00040000,
			.size  = 0x00014fc0
		},
		.reg = {
			.start = DRPW_BASE,
			.size  = 0x00006000
		},
		.mem2 = {
			.start = 0x00000000,
			.size  = 0x00000000
		},
		.mem3 = {
			.start = 0x00000000,
			.size  = 0x00000000
		}
	}
};

static void wl1271_boot_set_ecpu_ctrl(struct wl1271 *wl, u32 flag)
{
	u32 cpu_ctrl;

	/* 10.5.0 run the firmware (I) */
	cpu_ctrl = wl1271_read32(wl, ACX_REG_ECPU_CONTROL);

	/* 10.5.1 run the firmware (II) */
	cpu_ctrl |= flag;
	wl1271_write32(wl, ACX_REG_ECPU_CONTROL, cpu_ctrl);
}

static void wl1271_boot_fw_version(struct wl1271 *wl)
{
	struct wl1271_static_data static_data;

	wl1271_read(wl, wl->cmd_box_addr, &static_data, sizeof(static_data),
		    false);

	strncpy(wl->chip.fw_ver, static_data.fw_version,
		sizeof(wl->chip.fw_ver));

	/* make sure the string is NULL-terminated */
	wl->chip.fw_ver[sizeof(wl->chip.fw_ver) - 1] = '\0';
}

static int wl1271_boot_upload_firmware_chunk(struct wl1271 *wl, void *buf,
					     size_t fw_data_len, u32 dest)
{
	struct wl1271_partition_set partition;
	int addr, chunk_num, partition_limit;
	u8 *p, *chunk;

	/* whal_FwCtrl_LoadFwImageSm() */

	wl1271_debug(DEBUG_BOOT, "starting firmware upload");

	wl1271_debug(DEBUG_BOOT, "fw_data_len %zd chunk_size %d",
		     fw_data_len, CHUNK_SIZE);

	if ((fw_data_len % 4) != 0) {
		wl1271_error("firmware length not multiple of four");
		return -EIO;
	}

	chunk = kmalloc(CHUNK_SIZE, GFP_KERNEL);
	if (!chunk) {
		wl1271_error("allocation for firmware upload chunk failed");
		return -ENOMEM;
	}

	memcpy(&partition, &part_table[PART_DOWN], sizeof(partition));
	partition.mem.start = dest;
	wl1271_set_partition(wl, &partition);

	/* 10.1 set partition limit and chunk num */
	chunk_num = 0;
	partition_limit = part_table[PART_DOWN].mem.size;

	while (chunk_num < fw_data_len / CHUNK_SIZE) {
		/* 10.2 update partition, if needed */
		addr = dest + (chunk_num + 2) * CHUNK_SIZE;
		if (addr > partition_limit) {
			addr = dest + chunk_num * CHUNK_SIZE;
			partition_limit = chunk_num * CHUNK_SIZE +
				part_table[PART_DOWN].mem.size;
			partition.mem.start = addr;
			wl1271_set_partition(wl, &partition);
		}

		/* 10.3 upload the chunk */
		addr = dest + chunk_num * CHUNK_SIZE;
		p = buf + chunk_num * CHUNK_SIZE;
		memcpy(chunk, p, CHUNK_SIZE);
		wl1271_debug(DEBUG_BOOT, "uploading fw chunk 0x%p to 0x%x",
			     p, addr);
		wl1271_write(wl, addr, chunk, CHUNK_SIZE, false);

		chunk_num++;
	}

	/* 10.4 upload the last chunk */
	addr = dest + chunk_num * CHUNK_SIZE;
	p = buf + chunk_num * CHUNK_SIZE;
	memcpy(chunk, p, fw_data_len % CHUNK_SIZE);
	wl1271_debug(DEBUG_BOOT, "uploading fw last chunk (%zd B) 0x%p to 0x%x",
		     fw_data_len % CHUNK_SIZE, p, addr);
	wl1271_write(wl, addr, chunk, fw_data_len % CHUNK_SIZE, false);

	kfree(chunk);
	return 0;
}

static int wl1271_boot_upload_firmware(struct wl1271 *wl)
{
	u32 chunks, addr, len;
	int ret = 0;
	u8 *fw;

	fw = wl->fw;
	chunks = be32_to_cpup((__be32 *) fw);
	fw += sizeof(u32);

	wl1271_debug(DEBUG_BOOT, "firmware chunks to be uploaded: %u", chunks);

	while (chunks--) {
		addr = be32_to_cpup((__be32 *) fw);
		fw += sizeof(u32);
		len = be32_to_cpup((__be32 *) fw);
		fw += sizeof(u32);

		if (len > 300000) {
			wl1271_info("firmware chunk too long: %u", len);
			return -EINVAL;
		}
		wl1271_debug(DEBUG_BOOT, "chunk %d addr 0x%x len %u",
			     chunks, addr, len);
		ret = wl1271_boot_upload_firmware_chunk(wl, fw, len, addr);
		if (ret != 0)
			break;
		fw += len;
	}

	return ret;
}

static int wl1271_boot_upload_nvs(struct wl1271 *wl)
{
	size_t nvs_len, burst_len;
	int i;
	u32 dest_addr, val;
	u8 *nvs_ptr, *nvs_aligned;

	if (wl->nvs == NULL)
		return -ENODEV;

	/* only the first part of the NVS needs to be uploaded */
	nvs_len = sizeof(wl->nvs->nvs);
	nvs_ptr = (u8 *)wl->nvs->nvs;

	/* update current MAC address to NVS */
	nvs_ptr[11] = wl->mac_addr[0];
	nvs_ptr[10] = wl->mac_addr[1];
	nvs_ptr[6] = wl->mac_addr[2];
	nvs_ptr[5] = wl->mac_addr[3];
	nvs_ptr[4] = wl->mac_addr[4];
	nvs_ptr[3] = wl->mac_addr[5];

	/*
	 * Layout before the actual NVS tables:
	 * 1 byte : burst length.
	 * 2 bytes: destination address.
	 * n bytes: data to burst copy.
	 *
	 * This is ended by a 0 length, then the NVS tables.
	 */

	while (nvs_ptr[0]) {
		burst_len = nvs_ptr[0];
		dest_addr = (nvs_ptr[1] & 0xfe) | ((u32)(nvs_ptr[2] << 8));

		dest_addr += REGISTERS_BASE;

		/* We move our pointer to the data */
		nvs_ptr += 3;

		for (i = 0; i < burst_len; i++) {
			val = (nvs_ptr[0] | (nvs_ptr[1] << 8)
			       | (nvs_ptr[2] << 16) | (nvs_ptr[3] << 24));

			wl1271_debug(DEBUG_BOOT,
				     "nvs burst write 0x%x: 0x%x",
				     dest_addr, val);
			wl1271_write32(wl, dest_addr, val);

			nvs_ptr += 4;
			dest_addr += 4;
		}
	}

	/*
	 * We've reached the first zero length, the first NVS table
	 * is 7 bytes further.
	 */
	nvs_ptr += 7;
	nvs_len -= nvs_ptr - (u8 *)wl->nvs->nvs;
	nvs_len = ALIGN(nvs_len, 4);

	/* Now we must set the partition correctly */
	wl1271_set_partition(wl, &part_table[PART_WORK]);

	/* Copy the NVS tables to a new block to ensure alignment */
	nvs_ptr += 3;

	nvs_aligned = kmemdup(nvs_ptr, nvs_len, GFP_KERNEL); if
	(!nvs_aligned) return -ENOMEM;

	/* And finally we upload the NVS tables */
	wl1271_write(wl, CMD_MBOX_ADDRESS, nvs_aligned, nvs_len, false);

	kfree(nvs_aligned);
	return 0;
}

static void wl1271_boot_enable_interrupts(struct wl1271 *wl)
{
	wl1271_enable_interrupts(wl);
	wl1271_write32(wl, ACX_REG_INTERRUPT_MASK,
		       WL1271_ACX_INTR_ALL & ~(WL1271_INTR_MASK));
	wl1271_write32(wl, HI_CFG, HI_CFG_DEF_VAL);
}

static int wl1271_boot_soft_reset(struct wl1271 *wl)
{
	unsigned long timeout;
	u32 boot_data;

	/* perform soft reset */
	wl1271_write32(wl, ACX_REG_SLV_SOFT_RESET, ACX_SLV_SOFT_RESET_BIT);

	/* SOFT_RESET is self clearing */
	timeout = jiffies + usecs_to_jiffies(SOFT_RESET_MAX_TIME);
	while (1) {
		boot_data = wl1271_read32(wl, ACX_REG_SLV_SOFT_RESET);
		wl1271_debug(DEBUG_BOOT, "soft reset bootdata 0x%x", boot_data);
		if ((boot_data & ACX_SLV_SOFT_RESET_BIT) == 0)
			break;

		if (time_after(jiffies, timeout)) {
			/* 1.2 check pWhalBus->uSelfClearTime if the
			 * timeout was reached */
			wl1271_error("soft reset timeout");
			return -1;
		}

		udelay(SOFT_RESET_STALL_TIME);
	}

	/* disable Rx/Tx */
	wl1271_write32(wl, ENABLE, 0x0);

	/* disable auto calibration on start*/
	wl1271_write32(wl, SPARE_A2, 0xffff);

	return 0;
}

static int wl1271_boot_run_firmware(struct wl1271 *wl)
{
	int loop, ret;
	u32 chip_id, intr;

	wl1271_boot_set_ecpu_ctrl(wl, ECPU_CONTROL_HALT);

	chip_id = wl1271_read32(wl, CHIP_ID_B);

	wl1271_debug(DEBUG_BOOT, "chip id after firmware boot: 0x%x", chip_id);

	if (chip_id != wl->chip.id) {
		wl1271_error("chip id doesn't match after firmware boot");
		return -EIO;
	}

	/* wait for init to complete */
	loop = 0;
	while (loop++ < INIT_LOOP) {
		udelay(INIT_LOOP_DELAY);
		intr = wl1271_read32(wl, ACX_REG_INTERRUPT_NO_CLEAR);

		if (intr == 0xffffffff) {
			wl1271_error("error reading hardware complete "
				     "init indication");
			return -EIO;
		}
		/* check that ACX_INTR_INIT_COMPLETE is enabled */
		else if (intr & WL1271_ACX_INTR_INIT_COMPLETE) {
			wl1271_write32(wl, ACX_REG_INTERRUPT_ACK,
				       WL1271_ACX_INTR_INIT_COMPLETE);
			break;
		}
	}

	if (loop > INIT_LOOP) {
		wl1271_error("timeout waiting for the hardware to "
			     "complete initialization");
		return -EIO;
	}

	/* get hardware config command mail box */
	wl->cmd_box_addr = wl1271_read32(wl, REG_COMMAND_MAILBOX_PTR);

	/* get hardware config event mail box */
	wl->event_box_addr = wl1271_read32(wl, REG_EVENT_MAILBOX_PTR);

	/* set the working partition to its "running" mode offset */
	wl1271_set_partition(wl, &part_table[PART_WORK]);

	wl1271_debug(DEBUG_MAILBOX, "cmd_box_addr 0x%x event_box_addr 0x%x",
		     wl->cmd_box_addr, wl->event_box_addr);

	wl1271_boot_fw_version(wl);

	/*
	 * in case of full asynchronous mode the firmware event must be
	 * ready to receive event from the command mailbox
	 */

	/* unmask required mbox events  */
	wl->event_mask = BSS_LOSE_EVENT_ID |
		SCAN_COMPLETE_EVENT_ID |
		PS_REPORT_EVENT_ID |
		JOIN_EVENT_COMPLETE_ID |
		DISCONNECT_EVENT_COMPLETE_ID |
		RSSI_SNR_TRIGGER_0_EVENT_ID |
		PSPOLL_DELIVERY_FAILURE_EVENT_ID |
		SOFT_GEMINI_SENSE_EVENT_ID;

	ret = wl1271_event_unmask(wl);
	if (ret < 0) {
		wl1271_error("EVENT mask setting failed");
		return ret;
	}

	wl1271_event_mbox_config(wl);

	/* firmware startup completed */
	return 0;
}

static int wl1271_boot_write_irq_polarity(struct wl1271 *wl)
{
	u32 polarity;

	polarity = wl1271_top_reg_read(wl, OCP_REG_POLARITY);

	/* We use HIGH polarity, so unset the LOW bit */
	polarity &= ~POLARITY_LOW;
	wl1271_top_reg_write(wl, OCP_REG_POLARITY, polarity);

	return 0;
}

static void wl1271_boot_hw_version(struct wl1271 *wl)
{
	u32 fuse;

	fuse = wl1271_top_reg_read(wl, REG_FUSE_DATA_2_1);
	fuse = (fuse & PG_VER_MASK) >> PG_VER_OFFSET;

	wl->hw_pg_ver = (s8)fuse;
}

int wl1271_boot(struct wl1271 *wl)
{
	int ret = 0;
	u32 tmp, clk, pause;

	wl1271_boot_hw_version(wl);

	if (REF_CLOCK == 0 || REF_CLOCK == 2 || REF_CLOCK == 4)
		/* ref clk: 19.2/38.4/38.4-XTAL */
		clk = 0x3;
	else if (REF_CLOCK == 1 || REF_CLOCK == 3)
		/* ref clk: 26/52 */
		clk = 0x5;

	if (REF_CLOCK != 0) {
		u16 val;
		/* Set clock type (open drain) */
		val = wl1271_top_reg_read(wl, OCP_REG_CLK_TYPE);
		val &= FREF_CLK_TYPE_BITS;
		wl1271_top_reg_write(wl, OCP_REG_CLK_TYPE, val);

		/* Set clock pull mode (no pull) */
		val = wl1271_top_reg_read(wl, OCP_REG_CLK_PULL);
		val |= NO_PULL;
		wl1271_top_reg_write(wl, OCP_REG_CLK_PULL, val);
	} else {
		u16 val;
		/* Set clock polarity */
		val = wl1271_top_reg_read(wl, OCP_REG_CLK_POLARITY);
		val &= FREF_CLK_POLARITY_BITS;
		val |= CLK_REQ_OUTN_SEL;
		wl1271_top_reg_write(wl, OCP_REG_CLK_POLARITY, val);
	}

	wl1271_write32(wl, PLL_PARAMETERS, clk);

	pause = wl1271_read32(wl, PLL_PARAMETERS);

	wl1271_debug(DEBUG_BOOT, "pause1 0x%x", pause);

	pause &= ~(WU_COUNTER_PAUSE_VAL);
	pause |= WU_COUNTER_PAUSE_VAL;
	wl1271_write32(wl, WU_COUNTER_PAUSE, pause);

	/* Continue the ELP wake up sequence */
	wl1271_write32(wl, WELP_ARM_COMMAND, WELP_ARM_COMMAND_VAL);
	udelay(500);

	wl1271_set_partition(wl, &part_table[PART_DRPW]);

	/* Read-modify-write DRPW_SCRATCH_START register (see next state)
	   to be used by DRPw FW. The RTRIM value will be added by the FW
	   before taking DRPw out of reset */

	wl1271_debug(DEBUG_BOOT, "DRPW_SCRATCH_START %08x", DRPW_SCRATCH_START);
	clk = wl1271_read32(wl, DRPW_SCRATCH_START);

	wl1271_debug(DEBUG_BOOT, "clk2 0x%x", clk);

	/* 2 */
	clk |= (REF_CLOCK << 1) << 4;
	wl1271_write32(wl, DRPW_SCRATCH_START, clk);

	wl1271_set_partition(wl, &part_table[PART_WORK]);

	/* Disable interrupts */
	wl1271_write32(wl, ACX_REG_INTERRUPT_MASK, WL1271_ACX_INTR_ALL);

	ret = wl1271_boot_soft_reset(wl);
	if (ret < 0)
		goto out;

	/* 2. start processing NVS file */
	ret = wl1271_boot_upload_nvs(wl);
	if (ret < 0)
		goto out;

	/* write firmware's last address (ie. it's length) to
	 * ACX_EEPROMLESS_IND_REG */
	wl1271_debug(DEBUG_BOOT, "ACX_EEPROMLESS_IND_REG");

	wl1271_write32(wl, ACX_EEPROMLESS_IND_REG, ACX_EEPROMLESS_IND_REG);

	tmp = wl1271_read32(wl, CHIP_ID_B);

	wl1271_debug(DEBUG_BOOT, "chip id 0x%x", tmp);

	/* 6. read the EEPROM parameters */
	tmp = wl1271_read32(wl, SCR_PAD2);

	ret = wl1271_boot_write_irq_polarity(wl);
	if (ret < 0)
		goto out;

	wl1271_write32(wl, ACX_REG_INTERRUPT_MASK,
		       WL1271_ACX_ALL_EVENTS_VECTOR);

	/* WL1271: The reference driver skips steps 7 to 10 (jumps directly
	 * to upload_fw) */

	ret = wl1271_boot_upload_firmware(wl);
	if (ret < 0)
		goto out;

	/* 10.5 start firmware */
	ret = wl1271_boot_run_firmware(wl);
	if (ret < 0)
		goto out;

	/* Enable firmware interrupts now */
	wl1271_boot_enable_interrupts(wl);

	/* set the wl1271 default filters */
	wl->rx_config = WL1271_DEFAULT_RX_CONFIG;
	wl->rx_filter = WL1271_DEFAULT_RX_FILTER;

	wl1271_event_mbox_config(wl);

out:
	return ret;
}
