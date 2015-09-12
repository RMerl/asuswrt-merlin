/*
 * Broadcom Common Firmware Environment (CFE)
 * Board device initialization, File: bcm947xx_devs.c
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm947xx_devs.c 428035 2013-10-07 14:23:40Z $
 */

#include "lib_types.h"
#include "lib_printf.h"
#include "lib_physio.h"
#include "cfe.h"
#include "cfe_error.h"
#include "cfe_iocb.h"
#include "cfe_device.h"
#include "cfe_timer.h"
#ifdef CFG_ROMBOOT
#include "cfe_console.h"
#endif
#include "ui_command.h"
#include "bsp_config.h"
#include "dev_newflash.h"
#include "env_subr.h"
#include "pcivar.h"
#include "pcireg.h"
#include "../../../../../dev/ns16550.h"
#include "net_ebuf.h"
#include "net_ether.h"
#include "net_api.h"

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <hndpci.h>
#include "bsp_priv.h"
#include <trxhdr.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <hndcpu.h>
#include <hndchipc.h>
#include <hndpmu.h>
#include <epivers.h>
#if CFG_NFLASH
#include <hndnand.h>
#endif
#if CFG_SFLASH
#include <hndsflash.h>
#endif
#include <cfe_devfuncs.h>
#include <cfe_ioctl.h>

#ifdef RTL8365MB
#include <rtk_switch.h>
#include <smi.h>
#include <vlan.h>
#include <port.h>
#include <rtk_types.h>
#include <rtl8367c_asicdrv_port.h>
#endif

#define MAX_WAIT_TIME 20	/* seconds to wait for boot image */
#define MIN_WAIT_TIME 1 	/* seconds to wait for boot image */

#define RESET_DEBOUNCE_TIME	(500*1000)	/* 500 ms */

/* Defined as sih by bsp_config.h for convenience */
si_t *bcm947xx_sih = NULL;

/* Configured devices */
#if (CFG_FLASH || CFG_SFLASH || CFG_NFLASH) && CFG_XIP
#error "XIP and Flash cannot be defined at the same time"
#endif

extern cfe_driver_t ns16550_uart;
#if CFG_FLASH
extern cfe_driver_t newflashdrv;
#endif
#if CFG_SFLASH
extern cfe_driver_t sflashdrv;
#endif
#if CFG_NFLASH
extern cfe_driver_t nflashdrv;
#endif
#if CFG_ET
extern cfe_driver_t bcmet;
#endif
#if CFG_WL
extern cfe_driver_t bcmwl;
#endif
#if CFG_BCM57XX
extern cfe_driver_t bcm5700drv;
#endif

#ifdef CFG_ROMBOOT
#define MAX_SCRIPT_FSIZE	10240
#endif

/* Reset NVRAM */
static int restore_defaults = 0;
extern char *flashdrv_nvram;

extern void LEDON(void);
extern void LEDOFF(void);

int nospare;

static void
board_console_add(void *regs, uint irq, uint baud_base, uint reg_shift)
{
	physaddr_t base;

	/* The CFE NS16550 driver expects a physical address */
	base = PHYSADDR((physaddr_t) regs);
	cfe_add_device(&ns16550_uart, base, baud_base, &reg_shift);
}

#if CFG_FLASH || CFG_SFLASH || CFG_NFLASH
#if !CFG_SIM
static void
reset_release_wait(void)
{
	int gpio;
	uint32 gpiomask;
	int i=0;

	if ((gpio = nvram_resetgpio_init ((void *)sih)) < 0)
		return;

	/* Reset button is active low */
	gpiomask = (uint32)1 << gpio;
	while (1) {
		if ((i%100000) < 30000) {
			LEDON();
		}
		else {
			LEDOFF();
		}
		i++;

		if (i==0xffffff) {
			i = 0;
		}

		if (si_gpioin(sih) & gpiomask) {
			OSL_DELAY(RESET_DEBOUNCE_TIME);

			if (si_gpioin(sih) & gpiomask)
				break;
		}
	}
}
#endif /* !CFG_SIM */
#endif /* CFG_FLASH || CFG_SFLASH || CFG_NFLASH */

int
BCMINITFN(nvram_wsgpio_init)(void *si)
{
#if defined(RTAC68U) || defined(DSLAC68U)
	int gpio = 5;
#else
	int gpio = 7;
#endif
	si_t *sih;

	sih = (si_t *)si;

	if (gpio > 31)
		return -1;

	/* Setup GPIO input */
	si_gpioouten(sih, ((uint32) 1 << gpio), 0, GPIO_DRV_PRIORITY);

	return gpio;
}

#ifdef RTAC68U
int cpu_turbo_mode = 0;

static void
detect_turbo_button(void)
{
	int gpio;
	uint32 gpiomask;

	if ((gpio = nvram_wsgpio_init ((void *)sih)) < 0)
		return;

	/* active high */
	gpiomask = (uint32)1 << gpio;
	if ((si_gpioin(sih) & gpiomask))
		cpu_turbo_mode = 1;
}
#endif

/*
 *  board_console_init()
 *
 *  Add the console device and set it to be the primary
 *  console.
 *
 *  Input parameters:
 *     nothing
 *
 *  Return value:
 *     nothing
 */
void
board_console_init(void)
{
#if !CFG_MINIMAL_SIZE
	cfe_set_console(CFE_BUFFER_CONSOLE);
#endif

	/* Initialize SB access */
	sih = si_kattach(SI_OSH);
	ASSERT(sih);

	/* Set this to a default value, since nvram_reset needs to use it in OSL_DELAY */
	board_cfe_cpu_speed_upd(sih);

#if !CFG_SIM
	board_pinmux_init(sih);
	/* Check whether NVRAM reset needs be done */
	if (nvram_reset((void *)sih) > 0)
		restore_defaults = 1;
#endif

	/* Initialize NVRAM access accordingly. In case of invalid NVRAM, load defaults */
	if (nvram_init((void *)sih) > 0)
		restore_defaults = 1;

#if CFG_SIM
	restore_defaults = 0;
#else /* !CFG_SIM */

	if (!restore_defaults)
		board_clock_init(sih);

	board_power_init(sih);
#endif /* !CFG_SIM */

	board_cpu_init(sih);

	/* Initialize UARTs */
	si_serial_init(sih, board_console_add);

	if (cfe_finddev("uart0"))
		cfe_set_console("uart0");
#ifdef RTAC68U
	printf("Detect CPU turbo button... ");
	detect_turbo_button();
	if (cpu_turbo_mode && atoi(nvram_safe_get("btn_led_mode")))
		board_clock_init(sih);
#endif
}


#if (CFG_FLASH || CFG_SFLASH || CFG_NFLASH)
#if (CFG_NFLASH || defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE))
static fl_size_t
get_flash_size(char *device_name)
{
	int fd;
	flash_info_t flashinfo;
	int res = -1;

	fd = cfe_open(device_name);
	if ((fd > 0) &&
	    (cfe_ioctl(fd, IOCTL_FLASH_GETINFO,
		(unsigned char *) &flashinfo,
		sizeof(flash_info_t), &res, 0) == 0)) {

		cfe_close(fd);
		return flashinfo.flash_size;
	}

	return -1;
}
#endif /* CFG_NFLASH */

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
static fl_size_t
calculate_max_image_size(
	char *device_name,
	int reserved_space_begin,
	int reserved_space_end,
	int *need_commit,
	int nand_os_size)
{
	fl_size_t image_size = 0, flash_size = 0, available_size = 0;
	char *nvram_setting;
	char buf[64];

	*need_commit = 0;

#ifdef DUAL_IMAGE
	if (!nvram_get(IMAGE_BOOT))
#else
	if (!nvram_get(BOOTPARTITION))
#endif
		return 0;

	nvram_setting =  nvram_get(IMAGE_SIZE);

	if (nvram_setting) {
		image_size = atoi(nvram_setting)*1024;
#ifdef CFG_NFLASH
	} else if (device_name[0] == 'n') {
		/* use 32 meg for nand flash */
		image_size = (nand_os_size - reserved_space_begin)/2;
		image_size = image_size - image_size%(64*1024);
#endif
	} else {

		flash_size = get_flash_size(device_name);
		if (flash_size > 0) {
			available_size = flash_size - (reserved_space_begin + reserved_space_end);

			/* Calculate the 2nd offset with divide the
			 * availabe space by 2
			 * Make sure it is aligned to 64Kb to set
			 * the rootfs search algorithm
			 */
			image_size = available_size/2;
			image_size = image_size - image_size%(128*1024);
		}
	}
	/* 1st image start from bootsz end */
	sprintf(buf, "%d", reserved_space_begin);
	if (!nvram_match(IMAGE_FIRST_OFFSET, buf)) {
		printf("The 1st image start addr  changed, set to %s[%x] (was %s)\n",
			buf, reserved_space_begin, nvram_get(IMAGE_FIRST_OFFSET));
		nvram_set(IMAGE_FIRST_OFFSET, buf);
		*need_commit = 1;
	}

	sprintf(buf, "%"FL_FMT"d", reserved_space_begin + image_size);

	if (!nvram_match(IMAGE_SECOND_OFFSET, buf)) {
		printf("The 2nd image start addr  changed, set to %s[%"FL_FMT"x] (was %s)\n",
			buf, reserved_space_begin + image_size, nvram_get(IMAGE_SECOND_OFFSET));

		nvram_set(IMAGE_SECOND_OFFSET, buf);
		*need_commit = 1;
	}

	return image_size;
}
#endif /* FAILSAFE_UPGRADE|| DUAL_IMAGE */

#ifdef CFG_NFLASH

void
dump_nflash(int block_no)
{
	hndnand_t *nfl_info;
	uchar buf[2048];
	uchar oob_buf[64];
	int page_num = 1;
	uint64 i, addr = block_no?131072*(block_no-1):0;
	int block_num = block_no?block_no:1;
	uint64 img_size = block_no?131072:34700000, dump_size;

	dump_size = ROUNDUP(img_size, 2048);

	printf("ndump:%d\n", (int)dump_size);

	nfl_info = hndnand_init(sih);
	if (nfl_info == 0) {
		printf("Can't find nandflash! ccrev = %d, chipst= %d \n", sih->ccrev, sih->chipst);
		return;
	}

	while(dump_size){
		printf("\nBlock[%d] page[%d] data:\n", block_num, page_num);

		/* data */
		nfl_info->read(nfl_info, addr, sizeof(buf), buf);
		for(i=0; i<sizeof(buf); ++i){
			if(i%16==0)
				printf("\n%8x: ", (unsigned int)(addr+i+((block_num-1)<<12)+((page_num-1)<<6)));
			printf("%02x ", buf[i]);
		}	
	
		printf("\n");

		/* spare data */
		nfl_info->read_oob(nfl_info, addr, oob_buf);
		for(i=0; i<sizeof(oob_buf); ++i){
			if(i%16==0)
				printf("\n%8x: ", (unsigned int)(addr+2048+i+((block_num-1)<<12)+((page_num-1)<<6)));
			printf("%02x ", oob_buf[i]);
		}
		
		addr += sizeof(buf);
		dump_size -= sizeof(buf);
		++page_num;
		if(page_num>64){
			++block_num;
			page_num&=63;
		}
	}

	printf("\ndone\n");
}


static void
flash_nflash_init(void)
{
	newflash_probe_t fprobe;
	cfe_driver_t *drv;
	hndnand_t *nfl_info;
	int j;
	fl_size_t max_image_size = 0;
#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
	int need_commit = 0;
#endif

printf("*** flash_nflash_init ***\n");
	memset(&fprobe, 0, sizeof(fprobe));

	nfl_info = hndnand_init(sih);
	if (nfl_info == 0) {
		printf("Can't find nandflash! ccrev = %d, chipst= %d \n", sih->ccrev, sih->chipst);
		return;
	}

	drv = &nflashdrv;
	fprobe.flash_phys = nfl_info->phybase;

	j = 0;

	/* kernel in nand flash */
	if (soc_knl_dev((void *)sih) == SOC_KNLDEV_NANDFLASH) {
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		max_image_size = calculate_max_image_size("nflash0", 0, 0, &need_commit,
			nfl_boot_os_size(nfl_info));
#endif
		/* Because CFE can only boot from the beginning of a partition */
		fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
		fprobe.flash_parts[j++].fp_name = "trx";
		fprobe.flash_parts[j].fp_size = max_image_size ?
		        max_image_size - sizeof(struct trx_header) : 0;
		fprobe.flash_parts[j++].fp_name = "os";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
			fprobe.flash_parts[j++].fp_name = "trx2";
			fprobe.flash_parts[j].fp_size = max_image_size;
			fprobe.flash_parts[j++].fp_name = "os2";
		}
#endif
		fprobe.flash_nparts = j;

		cfe_add_device(drv, 0, 0, &fprobe);

		/* Because CFE can only boot from the beginning of a partition */
		j = 0;
		fprobe.flash_parts[j].fp_size = max_image_size ?
		max_image_size : nfl_boot_os_size(nfl_info);
		fprobe.flash_parts[j++].fp_name = "trx";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size = nfl_boot_os_size(nfl_info)
				- max_image_size;
			fprobe.flash_parts[j++].fp_name = "trx2";
		}
#endif
	}

	fprobe.flash_parts[j].fp_size = 0;
	fprobe.flash_parts[j++].fp_name = "brcmnand";
	fprobe.flash_nparts = j;

	cfe_add_device(drv, 0, 0, &fprobe);

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
	if (need_commit)
		nvram_commit();
#endif
}
#endif /* CFG_NFLASH */


static void
flash_init(void)
{
	newflash_probe_t fprobe;
	int bootdev;
	uint32 bootsz, *bisz;
	cfe_driver_t *drv = NULL;
	int j = 0;
	fl_size_t max_image_size = 0;
	int rom_envram_size;
#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
	int need_commit = 0;
#endif
#ifdef CFG_NFLASH
	hndnand_t *nfl_info = NULL;
#endif
#if CFG_SFLASH
	hndsflash_t *sfl_info = NULL;
#endif

	memset(&fprobe, 0, sizeof(fprobe));

	bootdev = soc_boot_dev((void *)sih);

        if(nvram_match("nospare", "1"))
                nospare = 1;
        else
                nospare = 0;

#ifdef CFG_NFLASH
	if (bootdev == SOC_BOOTDEV_NANDFLASH) {
		nfl_info = hndnand_init(sih);
		if (!nfl_info)
			return;

		fprobe.flash_phys = nfl_info->phybase;
		drv = &nflashdrv;
	} else
#endif	/* CFG_NFLASH */
#if CFG_SFLASH
	if (bootdev == SOC_BOOTDEV_SFLASH) {
		sfl_info = hndsflash_init(sih);
		if (!sfl_info)
			return;

		fprobe.flash_phys = sfl_info->phybase;
		drv = &sflashdrv;
	}
	else
#endif
#if CFG_FLASH
	{
		/* This might be wrong, but set pflash
		 * as default if nothing configured
		 */
		chipcregs_t *cc;

		if ((cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX)) == NULL)
			return;

		fprobe.flash_phys = SI_FLASH2;
		fprobe.flash_flags = FLASH_FLG_BUS16 | FLASH_FLG_DEV16;
		if (!(R_REG(NULL, &cc->flash_config) & CC_CFG_DS))
			fprobe.flash_flags = FLASH_FLG_BUS8 | FLASH_FLG_DEV16;
		drv = &newflashdrv;
	}
#endif /* CFG_FLASH */

	ASSERT(drv);

	/* Default is 256K boot partition */
	bootsz = 256 * 1024;

	/* Do we have a self-describing binary image? */
	bisz = (uint32 *)UNCADDR(fprobe.flash_phys + BISZ_OFFSET);
	if (bisz[BISZ_MAGIC_IDX] == BISZ_MAGIC) {
		int isz = bisz[BISZ_DATAEND_IDX] - bisz[BISZ_TXTST_IDX];

		if (isz > (1024 * 1024))
			bootsz = 2048 * 1024;
		else if (isz > (512 * 1024))
			bootsz = 1024 * 1024;
		else if (isz > (256 * 1024))
			bootsz = 512 * 1024;
		else if (isz <= (128 * 1024))
			bootsz = 128 * 1024;
	}
	printf("Boot partition size = %d(0x%x)\n", bootsz, bootsz);
#if CFG_NFLASH
	if (nfl_info) {
		fl_size_t flash_size = 0;

		if (bootsz > nfl_info->blocksize) {
			/* Prepare double space in case of bad blocks */
			bootsz = (bootsz << 1);
		} else {
			/* CFE occupies at least one block */
			bootsz = nfl_info->blocksize;
		}

		/* Because sometimes we want to program the entire device */
		fprobe.flash_nparts = 0;
		cfe_add_device(drv, 0, 0, &fprobe);

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		max_image_size = calculate_max_image_size("nflash0", nfl_boot_size(nfl_info), 0,
			&need_commit, nfl_boot_os_size(nfl_info));
#endif
		/* Because sometimes we want to program the entire device */
		/* Because CFE can only boot from the beginning of a partition */
		j = 0;
		fprobe.flash_parts[j].fp_size = bootsz;
		fprobe.flash_parts[j++].fp_name = "boot";
		fprobe.flash_parts[j].fp_size = (nfl_boot_size(nfl_info) - bootsz);
		fprobe.flash_parts[j++].fp_name = "nvram";

		fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
		fprobe.flash_parts[j++].fp_name = "trx";
		fprobe.flash_parts[j].fp_size = max_image_size ?
		        max_image_size - sizeof(struct trx_header) : 0;
		fprobe.flash_parts[j++].fp_name = "os";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
			fprobe.flash_parts[j++].fp_name = "trx2";
			fprobe.flash_parts[j].fp_size = max_image_size;
			fprobe.flash_parts[j++].fp_name = "os2";
		}
#endif

		fprobe.flash_nparts = j;
		cfe_add_device(drv, 0, 0, &fprobe);

		/* Because CFE can only flash an entire partition */
		j = 0;
		fprobe.flash_parts[j].fp_size = bootsz;
		fprobe.flash_parts[j++].fp_name = "boot";
		fprobe.flash_parts[j].fp_size = (nfl_boot_size(nfl_info) - bootsz);
		fprobe.flash_parts[j++].fp_name = "nvram";
		fprobe.flash_parts[j].fp_size = max_image_size;
		fprobe.flash_parts[j++].fp_name = "trx";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size =
				nfl_boot_os_size(nfl_info) - nfl_boot_size(nfl_info)
				- max_image_size;
			fprobe.flash_parts[j++].fp_name = "trx2";
		}
#endif
		flash_size = get_flash_size("nflash0") - nfl_boot_os_size(nfl_info);
#ifdef DUAL_TRX
                fprobe.flash_parts[j].fp_size = NFL_BOOT_OS_SIZE;
                fprobe.flash_parts[j++].fp_name = "trx2";
		flash_size -= NFL_BOOT_OS_SIZE;
#endif

		if (flash_size > 0) {
			fprobe.flash_parts[j].fp_size = flash_size;
			fprobe.flash_parts[j++].fp_name = "brcmnand";
		}

		fprobe.flash_nparts = j;
		cfe_add_device(drv, 0, 0, &fprobe);

		/* Change nvram device name for NAND boot */
		flashdrv_nvram = "nflash0.nvram";
	} else
#endif /* CFG_NFLASH */
	{
		/* Because sometimes we want to program the entire device */
		fprobe.flash_nparts = 0;
		cfe_add_device(drv, 0, 0, &fprobe);

#ifdef CFG_ROMBOOT
		if (board_bootdev_rom(sih)) {
			rom_envram_size = ROM_ENVRAM_SPACE;
		}
		else
#endif
		{
			rom_envram_size = 0;
		}

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		/* If the kernel is not in nand flash, split up the sflash */
		if (soc_knl_dev((void *)sih) != SOC_KNLDEV_NANDFLASH) {
			max_image_size = calculate_max_image_size("flash0",
				bootsz, MAX_NVRAM_SPACE+rom_envram_size, &need_commit, 0);
		}
#endif

		/* Because CFE can only boot from the beginning of a partition */
		j = 0;
		fprobe.flash_parts[j].fp_size = bootsz;
		fprobe.flash_parts[j++].fp_name = "boot";
		fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
		fprobe.flash_parts[j++].fp_name = "trx";
		fprobe.flash_parts[j].fp_size = max_image_size ?
		        max_image_size - sizeof(struct trx_header) : 0;
		fprobe.flash_parts[j++].fp_name = "os";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size = sizeof(struct trx_header);
			fprobe.flash_parts[j++].fp_name = "trx2";
			fprobe.flash_parts[j].fp_size = 0;
			fprobe.flash_parts[j++].fp_name = "os2";
		}
#endif
#ifdef CFG_ROMBOOT
		if (rom_envram_size) {
			fprobe.flash_parts[j].fp_size = rom_envram_size;
			fprobe.flash_parts[j++].fp_name = "envram";
		}
#endif

#ifdef BCM_DEVINFO
                fprobe.flash_parts[j].fp_size = 0x80000;
                fprobe.flash_parts[j++].fp_name = "fs_rw";

                fprobe.flash_parts[j].fp_size = 0x10000;
                fprobe.flash_parts[j++].fp_name = "devinfo";
#endif  /* BCM_DEVINFO */

		fprobe.flash_parts[j].fp_size = MAX_NVRAM_SPACE;
		fprobe.flash_parts[j++].fp_name = "nvram";
		fprobe.flash_nparts = j;
		cfe_add_device(drv, 0, 0, &fprobe);

		/* Because CFE can only flash an entire partition */
		j = 0;
		fprobe.flash_parts[j].fp_size = bootsz;
		fprobe.flash_parts[j++].fp_name = "boot";
		fprobe.flash_parts[j].fp_size = max_image_size;
		fprobe.flash_parts[j++].fp_name = "trx";
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
		if (max_image_size) {
			fprobe.flash_parts[j].fp_size = 0;
			fprobe.flash_parts[j++].fp_name = "trx2";
		}
#endif
#ifdef CFG_ROMBOOT
		if (rom_envram_size) {
			fprobe.flash_parts[j].fp_size = rom_envram_size;
			fprobe.flash_parts[j++].fp_name = "envram";
		}
#endif

#ifdef BCM_DEVINFO
                fprobe.flash_parts[j].fp_size = 0x80000;
                fprobe.flash_parts[j++].fp_name = "fs_rw";

                fprobe.flash_parts[j].fp_size = 0x10000;
                fprobe.flash_parts[j++].fp_name = "devinfo";
#endif  /* BCM_DEVINFO */

		fprobe.flash_parts[j].fp_size = MAX_NVRAM_SPACE;
		fprobe.flash_parts[j++].fp_name = "nvram";
		fprobe.flash_nparts = j;
		cfe_add_device(drv, 0, 0, &fprobe);
	}

#if (CFG_FLASH || CFG_SFLASH)
	flash_memory_size_config(sih, (void *)&fprobe);
#endif /* (CFG_FLASH || CFG_SFLASH) */

#ifdef CFG_NFLASH
	/* If boot from sflash, try to init partition for JFFS2 anyway */
	if (nfl_info == NULL)
		flash_nflash_init();
#endif /* CFG_NFLASH */

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
	if (need_commit)
		nvram_commit();
#endif
}
#endif	/* CFG_FLASH || CFG_SFLASH || CFG_NFLASH */

/*
 *  board_device_init()
 *
 *  Initialize and add other devices.  Add everything you need
 *  for bootstrap here, like disk drives, flash memory, UARTs,
 *  network controllers, etc.
 *
 *  Input parameters:
 *     nothing
 *
 *  Return value:
 *     nothing
 */
void
board_device_init(void)
{
	unsigned int unit;

#if CFG_ET || CFG_WL || CFG_BCM57XX
	void *regs;
#endif

	/* Set by board_console_init() */
	ASSERT(sih);


#if CFG_FLASH || CFG_SFLASH || CFG_NFLASH
	flash_init();
#endif

	mach_device_init(sih);

	for (unit = 0; unit < SI_MAXCORES; unit++) {
#if CFG_ET
		if ((regs = si_setcore(sih, ENET_CORE_ID, unit)))
			cfe_add_device(&bcmet, BCM47XX_ENET_ID, unit, regs);

#if CFG_GMAC
		if ((regs = si_setcore(sih, GMAC_CORE_ID, unit)))
			cfe_add_device(&bcmet, BCM47XX_GMAC_ID, unit, regs);
#endif
#endif /* CFG_ET */
#if CFG_WL
		if ((regs = si_setcore(sih, D11_CORE_ID, unit)))
			cfe_add_device(&bcmwl, BCM4306_D11G_ID, unit, regs);
#endif
#if CFG_BCM57XX
		if ((regs = si_setcore(sih, GIGETH_CORE_ID, unit)))
			cfe_add_device(&bcm5700drv, BCM47XX_GIGETH_ID, unit, regs);
#endif
	}
}

/*
 *  board_device_reset()
 *
 *  Reset devices.  This call is done when the firmware is restarted,
 *  as might happen when an operating system exits, just before the
 *  "reset" command is applied to the installed devices.   You can
 *  do whatever board-specific things are here to keep the system
 *  stable, like stopping DMA sources, interrupts, etc.
 *
 *  Input parameters:
 *     nothing
 *
 *  Return value:
 *     nothing
 */
void
board_device_reset(void)
{
}

extern void GPIO_INIT(void);

/*
 *  board_final_init()
 *
 *  Do any final initialization, such as adding commands to the
 *  user interface.
 *
 *  If you don't want a user interface, put the startup code here.
 *  This routine is called just before CFE starts its user interface.
 *
 *  Input parameters:
 *     nothing
 *
 *  Return value:
 *     nothing
 */
void
board_final_init(void)
{
	char *addr, *mask, *wait_time;
	char buf[512], *cur = buf;
#ifdef CFG_ROMBOOT
	char *laddr = NULL;
#endif
#if !CFG_SIM
	char *boot_cfg = NULL;
	char *go_cmd = "go;";
#endif
	int commit = 0;
	uint32 ncdl;
#if CFG_WL && CFG_WLU && CFG_SIM
	char *ssid;
#endif

	ui_init_bcm947xxcmds();

	/* Force commit of embedded NVRAM */
	commit = restore_defaults;

	/* Set the SDRAM NCDL value into NVRAM if not already done */
	if ((getintvar(NULL, "sdram_ncdl") == 0) &&
	    ((ncdl = si_memc_get_ncdl(sih)) != 0)) {
		sprintf(buf, "0x%x", ncdl);
		nvram_set("sdram_ncdl", buf);
		commit = 1;
	}

	/* Set the bootloader version string if not already done */
	sprintf(buf, "CFE %s", EPI_VERSION_STR);
	if (strcmp(nvram_safe_get("pmon_ver"), buf) != 0) {
		nvram_set("pmon_ver", buf);
		commit = 1;
	}

	/* Set the size of the nvram area if not already done */
	sprintf(buf, "%d", MAX_NVRAM_SPACE);
	if (strcmp(nvram_safe_get("nvram_space"), buf) != 0) {
		nvram_set("nvram_space", buf);
		commit = 1;
	}

#ifdef RTL8365MB
	int ret = -1, retry = 0;
	rtk_port_mac_ability_t pa;

	GPIO_INIT();
	for(retry = 0; ret && retry < 10; ++retry)
	{
		ret = rtk_switch_init();
		if(ret) cfe_usleep(10000);
		else break;
	}
	printf("rtl8354mb initialized(%d)(retry %d) %s\n", ret, retry, ret?"failed":"");

	ret = rtk_port_phyEnableAll_set(1);
	if(ret)        printf("rtk port_phyEnableAll Failed!(%d)\n", ret);
	else printf("rtk port_phyEnableAll ok\n");

	/* configure & set GMAC ports */
	pa.forcemode       = MAC_FORCE;
	pa.speed           = SPD_1000M;
	pa.duplex          = FULL_DUPLEX;
	pa.link            = 1;
	pa.nway            = 0;
	pa.rxpause         = 0;
	pa.txpause         = 0;

	ret = rtk_port_macForceLinkExt_set(EXT_PORT0, MODE_EXT_RGMII, &pa);
	if(ret)        printf("rtk port_macForceLink_set ext_Port1 Failed!(%d)\n", ret);
	else printf("rtk port_macForceLink_set ext_Port1 ok\n");        

	/* asic chk */
	rtk_uint32 retVal, retVal2;
	rtk_uint32 data, data2;
	if((retVal = rtl8367c_getAsicReg(0x1311, &data)) != RT_ERR_OK || (retVal2 = rtl8367c_getAsicReg(0x1305, &data2)) != RT_ERR_OK) {
		printf("get failed(%d)(%d). (%x)(supposed to be 0x1016)\n", retVal, retVal2, data);
	}
	else {      /* get ok, then chk reg*/
		printf("get ok, chk 0x1311:%x(0x1016), 0x1305:%x(b4~b7:0x1)\n", data, data2);
		if(data != 0x1016){
			printf("\n!! rtl reg 0x1311 error: (%d)(get %x)\n", retVal, data);
		}
		if((data2 & 0xf0) != 0x10) {
			printf("\n!! rtl reg 0x1305 error: (%d)(get %x)\n", retVal, data2);
		}
	}

	/* reset tx/rx delay */
	retVal = rtk_port_rgmiiDelayExt_set(EXT_PORT0, 1, 4);
	if (retVal != RT_ERR_OK)
		printf("\n!! rtl set delay failed (%d)\n", retVal);
#endif

#if CFG_FLASH || CFG_SFLASH || CFG_NFLASH
#if !CFG_SIM
	/* Commit NVRAM only if in FLASH */
	if (
#ifdef BCMNVRAMW
		!nvram_inotp() &&
#endif
		commit) {
		printf("Committing NVRAM...");
		nvram_commit();
		printf("done\n");
		if (restore_defaults) {
#ifdef BCM_DEVINFO
                        /* devinfo nvram hash table sync */
                        devinfo_nvram_sync();
#endif
			printf("Waiting for wps button release.....");
			reset_release_wait();
			printf("done\n");
		}
	}

	/* Reboot after restoring defaults */
	if (restore_defaults)
		si_watchdog(sih, 1);
#endif	/* !CFG_SIM */
#else
	if (commit)
		printf("Flash not configured, not commiting NVRAM...\n");
#endif /* CFG_FLASH || CFG_SFLASH || CFG_NFLASH */

	/*
	 * Read the wait_time NVRAM variable and set the tftp max retries.
	 * Assumption: tftp_rrq_timeout & tftp_recv_timeout are set to 1sec.
	 */
	if ((wait_time = nvram_get("wait_time")) != NULL) {
		tftp_max_retries = atoi(wait_time);
		if (tftp_max_retries > MAX_WAIT_TIME)
			tftp_max_retries = MAX_WAIT_TIME;
		else if (tftp_max_retries < MIN_WAIT_TIME)
			tftp_max_retries = MIN_WAIT_TIME;
	}
#ifdef CFG_ROMBOOT
	else if (board_bootdev_rom(sih)) {
		tftp_max_retries = 10;
	}
#endif

	/* Configure network */
	if (cfe_finddev("eth0")) {
		int res;

		if ((addr = nvram_get("lan_ipaddr")) &&
		    (mask = nvram_get("lan_netmask")))
			sprintf(buf, "ifconfig eth0 -addr=%s -mask=%s",
			        addr, mask);
		else
			sprintf(buf, "ifconfig eth0 -auto");

		res = ui_docommand(buf);

#ifdef CFG_ROMBOOT
		/* Try indefinite netboot only while booting from ROM
		 * and we are sure that we dont have valid nvram in FLASH
		 */
		while (board_bootdev_rom(sih) && !addr) {
			char ch;

			cur = buf;
			/* Check if something typed at console */
			if (console_status()) {
				console_read(&ch, 1);
				/* Check for Ctrl-C */
				if (ch == 3) {
					if (laddr)
						MFREE(osh, laddr, MAX_SCRIPT_FSIZE);
					xprintf("Stopped auto netboot!!!\n");
					return;
				}
			}

			if (!res) {
				char *bserver, *bfile, *load_ptr;

				if (!laddr)
					laddr = MALLOC(osh, MAX_SCRIPT_FSIZE);

				if (!laddr) {
					load_ptr = (char *) 0x00008000;
					xprintf("Failed malloc for boot_script, Using :0x%x\n",
						(unsigned int)laddr);
				}
				else {
					load_ptr = laddr;
				}
				bserver = (bserver = env_getenv("BOOT_SERVER"))
					? bserver:"192.168.1.1";

				if ((bfile = env_getenv("BOOT_FILE"))) {
					int len;

					if (((len = strlen(bfile)) > 5) &&
					    !strncmp((bfile + len - 5), "cfesh", 5)) {
						cur += sprintf(cur,
						"batch -raw -tftp -addr=0x%x -max=0x%x %s:%s;",
							(unsigned int)load_ptr,
							MAX_SCRIPT_FSIZE, bserver, bfile);
					}
					if (((len = strlen(bfile)) > 3)) {
						if (!strncmp((bfile + len - 3), "elf", 3)) {
							cur += sprintf(cur,
							"boot -elf -tftp -max=0x5000000 %s:%s;",
							bserver, bfile);
						}
						if (!strncmp((bfile + len - 3), "raw", 3)) {
							cur += sprintf(cur,
							"boot -raw -z -tftp -addr=0x00008000"
							" -max=0x5000000 %s:%s;",
							bserver, bfile);
						}
					}
				}
				else {  /* Make last effort */
					cur += sprintf(cur,
						"batch -raw -tftp -addr=0x%x -max=0x%x %s:%s;",
						(unsigned int)load_ptr, MAX_SCRIPT_FSIZE,
						bserver, "cfe_script.cfesh");
					cur += sprintf(cur,
						"boot -elf -tftp -max=0x5000000 %s:%s;",
						bserver, "boot_file.elf");
					cur += sprintf(cur,
						"boot -raw -z -tftp -addr=0x00008000"
						" -max=0x5000000 %s:%s;",
						bserver, "boot_file.raw");
				}

				ui_docommand(buf);
				cfe_sleep(3*CFE_HZ);
			}

			sprintf(buf, "ifconfig eth0 -auto");
			res = ui_docommand(buf);
		}
#endif /* CFG_ROMBOOT */
	}
#if CFG_WL && CFG_WLU && CFG_SIM
	if ((ssid = nvram_get("wl0_ssid"))) {
		sprintf(buf, "wl join %s", ssid);
		ui_docommand(buf);
	}
#endif

#if !CFG_SIM
	/* Try to run boot_config command if configured.
	 * make sure to leave space for "go" command.
	 */
	if ((boot_cfg = nvram_get("boot_config"))) {
		if (strlen(boot_cfg) < (sizeof(buf) - sizeof(go_cmd)))
			cur += sprintf(cur, "%s;", boot_cfg);
		else
			printf("boot_config too long, skipping to autoboot\n");
	}

	/* Boot image */
	cur += sprintf(cur, go_cmd);
#endif	/* !CFG_SIM */

	/* Startup */
	if (cur > buf)
		env_setenv("STARTUP", buf, ENV_FLG_NORMAL);
}
