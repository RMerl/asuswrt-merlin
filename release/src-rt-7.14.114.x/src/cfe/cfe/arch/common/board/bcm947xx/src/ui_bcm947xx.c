/*
 * Broadcom Common Firmware Environment (CFE)
 * Board device initialization, File: ui_bcm947xx.c
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ui_bcm947xx.c 497188 2014-08-18 09:16:31Z $
 */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "cfe.h"
#include "cfe_iocb.h"
#include "cfe_devfuncs.h"
#include "cfe_ioctl.h"
#include "cfe_error.h"
#include "cfe_fileops.h"
#include "cfe_loader.h"
#include "ui_command.h"
#include "bsp_config.h"

#include <typedefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <hndsoc.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <hndcpu.h>
#include <trxhdr.h>
#include "initdata.h"
#include "bsp_priv.h"

#include <rtk_types.h>

extern uint32_t _frag_merge_func_len;
extern uint32_t _frag_info_sz;

#ifdef RESCUE_MODE
unsigned char DETECT(void);
bool mmode_set(void);
bool rmode_set(void);
extern void LEDON(void);
extern void LEDOFF(void);
extern void GPIO_INIT(void);
extern void FANON(void);

/* define GPIOs*/

#ifdef DSLAC68U
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define WPS_BTN_GPIO	(1 << 7)	// GPIO 7
#endif

#ifdef RTAC87U
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define	TURBO_LED_GPIO	(1 << 4)	// GPIO 4
#define WPS_BTN_GPIO	(1 << 2)	// GPIO 2
#endif

#ifdef RTN18U
#define PWR_LED_GPIO	(1 << 0)	// GPIO 0
#define RST_BTN_GPIO	(1 << 7)	// GPIO 7
#define WPS_BTN_GPIO	(1 << 11)	// GPIO 11
#endif

#ifdef RTAC56U
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define WPS_BTN_GPIO	(1 << 15)	// GPIO 15
#endif

#ifdef RTAC68U
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define	TURBO_LED_GPIO	(1 << 4)	// GPIO 4
#define WPS_BTN_GPIO	(1 << 7)	// GPIO 7
#endif

#ifdef RTAC3200
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define WPS_BTN_GPIO	(1 << 7)	// GPIO 7
#endif
#endif

#if defined(RTAC88U) || defined(RTAC3100)
#ifdef RTL8365MB
#define SMI_SCK_GPIO   	(1 << 6)        // GPIO 6
#define SMI_SDA_GPIO   	(1 << 7)        // GPIO 7
#endif /*~RTL8365MB*/
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define WPS_BTN_GPIO	(1 << 20)	// GPIO 20
#endif

#if defined RTAC5300
#ifdef RTL8365MB
#define SMI_SCK_GPIO   	(1 << 6)        // GPIO 6
#define SMI_SDA_GPIO   	(1 << 7)        // GPIO 7
#endif /*~RTL8365MB*/
#define PWR_LED_GPIO	(1 << 3)	// GPIO 3
#define RST_BTN_GPIO	(1 << 11)	// GPIO 11
#define WPS_BTN_GPIO	(1 << 18)	// GPIO 18
#endif

static int
ui_cmd_reboot(ui_cmdline_t *cmd, int argc, char *argv[])
{
	hnd_cpu_reset(sih);
	return 0;
}

static int
_ui_cmd_nvram(ui_cmdline_t *cmd, int argc, char *argv[])
{
        char *command, *name, *value;
        char *buf;
        size_t size;
        int ret;

        if (!(command = cmd_getarg(cmd, 0)))
                return CFE_ERR_INV_PARAM;

        if (!strcmp(command, "get")) {
                if ((name = cmd_getarg(cmd, 1)))
                        if ((value = nvram_get(name)))
                                printf("%s\n", value);
        } else if (!strcmp(command, "set")) {
                if ((name = cmd_getarg(cmd, 1))) {
                        if ((value = strchr(name, '=')))
                                *value++ = '\0';
                        else if ((value = cmd_getarg(cmd, 2))) {
                                if (*value == '=')
                                        value = cmd_getarg(cmd, 3);
                        }
                        if (value)
                                nvram_set(name, value);
                }
        } else if (!strcmp(command, "unset")) {
                if ((name = cmd_getarg(cmd, 1)))
                        nvram_unset(name);
        } else if (!strcmp(command, "commit")) {
                nvram_commit();
        } else if (!strcmp(command, "corrupt")) {
                /* corrupt nvram */
                nvram_commit_internal(TRUE);
#ifdef BCMNVRAMW
        } else if (!strcmp(command, "otpcommit")) {
                nvram_otpcommit((void *)sih);
#endif
        } else if (!strcmp(command, "erase")) {
                extern char *flashdrv_nvram;
                if ((ret = cfe_open(flashdrv_nvram)) < 0)
                        return ret;
                if (!(buf = KMALLOC(MAX_NVRAM_SPACE, 0))) {
                        cfe_close(ret);
                        return CFE_ERR_NOMEM;
                }
                memset(buf, 0xff, MAX_NVRAM_SPACE);
                cfe_writeblk(ret, 0, (unsigned char *)buf, MAX_NVRAM_SPACE);
                cfe_close(ret);
                KFREE(buf);
        } else if (!strcmp(command, "show") || !strcmp(command, "getall")) {
                if (!(buf = KMALLOC(MAX_NVRAM_SPACE, 0)))
                        return CFE_ERR_NOMEM;
                nvram_getall(buf, MAX_NVRAM_SPACE);
                for (name = buf; *name; name += strlen(name) + 1)
                        printf("%s\n", name);
                size = sizeof(struct nvram_header) + ((uintptr)name - (uintptr)buf);
                printf("size: %d bytes (%d left)\n", size, MAX_NVRAM_SPACE - size);
                KFREE(buf);
        }

        return 0;
}

static int
ui_cmd_nvram(ui_cmdline_t *cmd, int argc, char *argv[])
{
	return _ui_cmd_nvram(cmd, argc, argv);
}

#ifdef BCM_DEVINFO
static int
ui_cmd_devinfo(ui_cmdline_t *cmd, int argc, char *argv[])
{
        int ret;
    char *tmp;

        tmp = flashdrv_nvram;
        flashdrv_nvram = devinfo_flashdrv_nvram;
        _nvram_hash_select(1);  /* 1 is devinfo hash table idx */

        ret = _ui_cmd_nvram(cmd, argc, argv);

        /* revert back to default nvram hash table */
        flashdrv_nvram = tmp;
        _nvram_hash_select(0);  /* 0 is nvram hash table idx */

        return (ret);
}
#endif

#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
#define MAX_BOOT_IMGAGE                 2
#define IMAGE_NAME_SIZE                 16
typedef enum {
        TRX_CRC_NONE,
        TRX_CRC_CHK_FAIL,
        TRX_CRC_CHK_PASS,
        TRX_CRC_CHK_MAX
} TRX_CRC_STATE_T;

typedef struct trx_crc_rec_t_ {
        TRX_CRC_STATE_T state;
        int     code;
        char name[IMAGE_NAME_SIZE];
} trx_crc_rec_t;

trx_crc_rec_t img_crc_rec[MAX_BOOT_IMGAGE] = {
        {TRX_CRC_NONE, -1, {0}},
        {TRX_CRC_NONE, -1, {0}},
};

#ifndef RESCUE_MODE
static void
init_trx_crc_rec(char *trx_name)
{
        int nlen = IMAGE_NAME_SIZE - 1;

        if (trx_name == NULL)
                return;

        strncpy(img_crc_rec[0].name, trx_name, nlen);
        strncpy(img_crc_rec[1].name, trx_name, nlen);

        /* 2nd image suffix, note, cfe does not have a strncat() */
        strcat(img_crc_rec[1].name, "2");
}
#endif

static int
get_crc_rec_idx(char *trx_name)
{
        int idx;

        if (trx_name == NULL)
                return -1;

        for (idx = 0; idx < MAX_BOOT_IMGAGE; idx++) {
                if (strncmp(img_crc_rec[idx].name, trx_name, IMAGE_NAME_SIZE - 1) == 0) {
                        return idx;
                }
        }

        return (-1);
}
#endif /* DUAL_IMAGE || FAILSAFE_UPGRADE */

#if 0
//#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
static int
switch_pwrled(void)
{
	static int i = 0;

	if (i++%2)
		LEDON();
	else
		LEDOFF();

	return 0;
}
#endif

static int
check_trx(char *trx_name)
{
	int ret;
	fileio_ctx_t *fsctx;
	void *ref;
	struct trx_header trx;
#ifdef CFG_NFLASH
	uint32 crc;
	static uint32 buf[16*1024];
#else
	uint32 crc, buf[512];
#endif /* CFG_NFLASH */
	int first_read = 1;
	unsigned int len, count;
	//int i = 0;

#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
        int idx;

        idx = get_crc_rec_idx(trx_name);
        if ((idx != -1) && (img_crc_rec[idx].state != TRX_CRC_NONE)) {
                return (img_crc_rec[idx].code);
        }
#endif /* DUAL_IMAGE || FAILSAFE_UPGRADE */

	/* Open header */
	ret = fs_init("raw", &fsctx, trx_name);
	if (ret)
		goto error;

	ret = fs_open(fsctx, &ref, "", FILE_MODE_READ);
	if (ret) {
		fs_uninit(fsctx);
		goto error;
	}

	/* Read header */
	ret = fs_read(fsctx, ref, (unsigned char *) &trx, sizeof(struct trx_header));
	if (ret != sizeof(struct trx_header)) {
		ret = CFE_ERR_IOERR;
		goto done;
	}

	/* Verify magic number */
	if (ltoh32(trx.magic) != TRX_MAGIC) {
		ret = CFE_ERR_INVBOOTBLOCK;
		goto done;
	}

	/* Checksum over header */
	crc = hndcrc32((uint8 *) &trx.flag_version,
	               sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
	               CRC32_INIT_VALUE);

	for (len = ltoh32(trx.len) - sizeof(struct trx_header); len; len -= count) {
//#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
//		if(i++ % 50 == 0)	
//			switch_pwrled();
//#endif

		if (first_read) {
			count = MIN(len, sizeof(buf) - sizeof(struct trx_header));
			first_read = 0;
		} else
			count = MIN(len, sizeof(buf));

		/* Read data */
		ret = fs_read(fsctx, ref, (unsigned char *) buf, count);
		if (ret != count) {
			ret = CFE_ERR_IOERR;
			goto done;
		}

		/* Checksum over data */
		crc = hndcrc32((uint8 *) buf, count, crc);
	}

	/* Verify checksum */
	if (ltoh32(trx.crc32) != crc) {
		ret = CFE_ERR_BOOTPROGCHKSUM;
		goto done;
	}

//#if defined(RTAC88U) || defined(RTAC3100) || defined(RTAC5300)
//	LEDOFF();
//#endif
	ret = 0;

done:
	fs_close(fsctx, ref);
	fs_uninit(fsctx);
error:
	if (ret)
		xprintf("%s\n", cfe_errortext(ret));
#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
        /* cache update crc check result */
        if (idx != -1) {
                img_crc_rec[idx].state = (ret == 0) ? TRX_CRC_CHK_PASS : TRX_CRC_CHK_FAIL;
                img_crc_rec[idx].code = ret;
        }
#endif /* DUAL_IMAGE || FAILSAFE_UPGRADE */

	return ret;
}

#ifdef RTL8365MB
void set_gpio_dir(rtk_uint32 gpioid, rtk_uint32 dir)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	si_gpioouten(sih, 1<<gpioid, !dir?1<<gpioid:0, GPIO_DRV_PRIORITY);
}

void set_gpio_data(rtk_uint32 gpioid, rtk_uint32 data)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	si_gpioout(sih, 1<<gpioid, data?1<<gpioid:0, GPIO_DRV_PRIORITY);
}

void get_gpio_data(rtk_uint32 gpioid, rtk_uint32 *pdata)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	*pdata = (si_gpioin(sih) & 1<<gpioid) == 0 ? 0 : 1;
}
#endif

#ifdef RESCUE_MODE
bool mmode_set(void)
{
        unsigned long gpioin;

        sih = si_kattach(SI_OSH);
        ASSERT(sih);
        gpioin = si_gpioin(sih);

        si_detach(sih);
        return gpioin & RST_BTN_GPIO ? FALSE : TRUE;
}

bool rmode_set(void) /* reset mode */
{
        unsigned long gpioin;

        sih = si_kattach(SI_OSH);
        ASSERT(sih);
        gpioin = si_gpioin(sih);

        si_detach(sih);
        return gpioin & WPS_BTN_GPIO ? FALSE : TRUE;
}

extern void LEDON(void)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
#ifdef RTL8365MB
	si_gpiocontrol(sih, SMI_SCK_GPIO, 0, GPIO_DRV_PRIORITY);
	si_gpiocontrol(sih, SMI_SDA_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpioouten(sih, TURBO_LED_GPIO, TURBO_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
	/* led on */
	/* negative logic and hence val==0 */
	si_gpioout(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#ifdef RTL8365MB
	si_gpioouten(sih, SMI_SCK_GPIO, 0, GPIO_DRV_PRIORITY);
	si_gpioouten(sih, SMI_SDA_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpioout(sih, TURBO_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
}
extern void GPIO_INIT(void)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	si_gpiocontrol(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpiocontrol(sih, TURBO_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
	si_gpioouten(sih, PWR_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpioouten(sih, TURBO_LED_GPIO, 0, GPIO_DRV_PRIORITY);
#endif
}

extern void LEDOFF(void)
{
	sih = si_kattach(SI_OSH);
	ASSERT(sih);
	si_gpioouten(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpioouten(sih, TURBO_LED_GPIO, TURBO_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
	si_gpioout(sih, PWR_LED_GPIO, PWR_LED_GPIO, GPIO_DRV_PRIORITY);
#if defined(RTAC68U) || defined(RTAC87U)
	si_gpioout(sih, TURBO_LED_GPIO, TURBO_LED_GPIO, GPIO_DRV_PRIORITY);
#endif
}

unsigned char DETECT(void)
{
        unsigned char d = 0;
        char *rescueflag;

        if ((rescueflag = nvram_get("rescueflag")) != NULL) {
                if (!nvram_invmatch("rescueflag", "enable")) {
                        xprintf("Rescue Flag enable.\n");
                        d = 1;
                }
                else {
                        xprintf("Rescue Flag disable.\n");
                        if (mmode_set())
                                d = 1;
                        else
                                d = 0;
                }
                nvram_set("rescueflag", "disable");
                nvram_commit();
        }
        else {
                xprintf("Null Rescue Flag.\n");
                if (mmode_set())
                        d = 1;
                else
                        d = 0;
        }

        /* Set 1 to be high active and 0 to be low active */
        if (d==1)
                return 1;
        else
                return 0;
}
#endif // RESCUE_MODE

/*
 *  ui_get_loadbuf(bufptr, bufsize)
 *
 *  Figure out the location and size of the staging buffer.
 *
 *  Input parameters:
 *       bufptr - address to return buffer location
 *       bufsize - address to return buffer size
 */
static void ui_get_loadbuf(uint8_t **bufptr, int *bufsize)
{
	int size = CFG_FLASH_STAGING_BUFFER_SIZE;

	/*
	 * Get the address of the staging buffer.  We can't
	 * allocate the space from the heap to store the
	 * new flash image, because the heap may not be big
	 * enough.  So, if FLASH_STAGING_BUFFER_SIZE is non-zero
	 * then just use it and FLASH_STAGING_BUFFER; else
	 * use the larger of (mem_bottomofmem - FLASH_STAGING_BUFFER)
	 * and (mem_totalsize - mem_topofmem).
	 */

	if (size > 0) {
		*bufptr = (uint8_t *) KERNADDR(CFG_FLASH_STAGING_BUFFER_ADDR);
		*bufsize = size;
	} else {
		int below, above;
		int reserved = CFG_FLASH_STAGING_BUFFER_ADDR;

		/* For small memory size (8MB), we tend to use the below region.
		 * The buffer address may conflict with the os running address,
		 * so we reserve 3MB for the os.
		 */
		if ((mem_totalsize == (8*1024)) && (PHYSADDR(mem_bottomofmem) > 0x300000))
			reserved = 0x300000;

		below = PHYSADDR(mem_bottomofmem) - reserved;
		above = (mem_totalsize << 10) - PHYSADDR(mem_topofmem);

		if (below > above) {
			*bufptr = (uint8_t *) KERNADDR(reserved);
			*bufsize = below;
		} else {
			*bufptr = (uint8_t *) KERNADDR((mem_topofmem +
				_frag_merge_func_len + _frag_info_sz));
			*bufsize = above;
		}
	}
}

#if defined(DUAL_IMAGE) || defined(FAILSAFE_UPGRADE)
static
int check_image_prepare_cmd(int the_image, char *buf, uint32 osaddr, int bufsize)
{
	int ret = -1;
	char trx_name[16], os_name[16];
	char trx2_name[16], os2_name[16];

#ifdef CFG_NFLASH
	ui_get_trx_flashdev(trx_name);
	ui_get_os_flashdev(os_name);
#else
	strcpy(trx_name, "flash1.trx");
	strcpy(os_name, "flash0.os");
#endif

	strncpy(trx2_name, trx_name, sizeof(trx2_name));
	strncpy(os2_name, os_name, sizeof(os2_name));
	strcat(trx2_name, "2");
	strcat(os2_name, "2");

	if (the_image == 0) {
		if ((ret = check_trx(trx_name)) == 0) {
			sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize,
			        os_name);
		} else {
			printf("%s CRC check failed!\n", trx_name);
		}
	} else if (the_image == 1) {
		if ((ret = check_trx(trx2_name)) == 0) {
			sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize,
			        os2_name);
		} else {
			printf("%s CRC check failed!\n", trx2_name);
		}
	} else {
		printf("Image partition %d does not exist\n", the_image);
	}
	return ret;
}
#endif /* DUAL_IMAGE || FAILSAFE_UPGRADE */

#ifdef CFG_NFLASH
void
ui_get_boot_flashdev(char *flashdev)
{
	if (!flashdev)
		return;

	if (soc_boot_dev((void *)sih) == SOC_BOOTDEV_NANDFLASH)
		strcpy(flashdev, "nflash1.boot");
	else
		strcpy(flashdev, "flash1.boot");

	return;
}

void
ui_get_os_flashdev(char *flashdev)
{
	if (!flashdev)
		return;

	if (soc_knl_dev((void *)sih) == SOC_KNLDEV_NANDFLASH)
		strcpy(flashdev, "nflash0.os");
	else
		strcpy(flashdev, "flash0.os");

	return;
}

void
ui_get_trx_flashdev(char *flashdev)
{
	if (!flashdev)
		return;

	if (soc_knl_dev((void *)sih) == SOC_KNLDEV_NANDFLASH)
		strcpy(flashdev, "nflash1.trx");
	else
		strcpy(flashdev, "flash1.trx");

	return;
}
#endif /* CFG_NFLASH */

static int
ui_cmd_go(ui_cmdline_t *cmd, int argc, char *argv[])
{
	int ret = 0;
	char buf[512];
	struct trx_header *file_buf;
	uint8_t *ptr;
	char *val;
	uint32 osaddr;
	int bufsize = 0;
#ifndef RESCUE_MODE
        int retry = 0;
        int trx_failed;
#else
        int  i = 0;
        GPIO_INIT();
        LEDON();
#endif
#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
#ifndef RESCUE_MODE
        char *trx_name_1st = NULL;
        char *trx_name_2nd = NULL;
        int boot_img_idx = 0;
#endif
	char *bootpartition = nvram_get(BOOTPARTITION);
	char *partialboots = nvram_get(PARTIALBOOTS);
	char *maxpartialboots = nvram_get(MAXPARTIALBOOTS);
#endif
#ifdef CFG_NFLASH
	char trx_name[16], os_name[16];
#else
	char *trx_name = "flash1.trx";
	char *os_name = "flash0.os";
#endif	/* CFG_NFLASH */
	int FW_err_count;
	char FW_err[4];

#ifdef DUAL_TRX
        char *trx2_name = "nflash1.trx2";
        int trx1_ret, trx2_ret;
#endif

	val = nvram_get("os_ram_addr");
	if (val)
		osaddr = bcm_strtoul(val, NULL, 16);
	else {
#ifdef	__mips__
		osaddr = 0x80001000;
#else
		osaddr = 0x00008000;
#endif
	}

#ifdef RESCUE_MODE
        strcpy(trx_name, "nflash1.trx");
        strcpy(os_name, "nflash0.os");

        if (DETECT()) {
                xprintf("Hello!! Enter Rescue Mode: (by Force)\n\n");
                /* Wait forever for an image */
                while ((ret = ui_docommand("flash -noheader : nflash1.trx")) == CFE_ERR_TIMEOUT) {
                        if (i%2 == 0) {
                                LEDOFF();
                        } else {
                                LEDON();
			}
                        i++;
                        if (i==0xffffff)
                                i = 0;
                }
        }
#if 0
        else if (rmode_set()) {
                xprintf("Wait until reset button released...\n");
                while(rmode_set() == 1) {
                        if ((i%100000) < 50000) {
                                LEDON();
                        }
                        else {
                                LEDOFF();
                        }
                        i++;

                        if (i==0xffffff) {
                                i = 0;
                        }
                }
                ui_docommand ("nvram erase");
                ui_docommand ("reboot");
        }
#endif
        else {
                xprintf("boot the image...\n"); // tmp test
#ifdef DUAL_TRX
                trx1_ret = check_trx(trx_name);
                trx2_ret = check_trx(trx2_name);
                xprintf("Check 2 trx result: %d, %d\n", trx1_ret, trx2_ret);

                if( trx1_ret == 0 && trx2_ret != 0 ) {
                        //copy trx1 -> trx2
                        ret = ui_docommand("flash -size=48234496 nflash1.trx nflash1.trx2 -cfe");
                }
                else if( trx1_ret != 0 && trx2_ret == 0 ) {
                        //copy trx2 -> trx1
                        ret = ui_docommand("flash -size=50331648 nflash1.trx2 nflash1.trx -cfe");
                        //check trx1 again
                        trx1_ret = check_trx(trx_name);
                        xprintf("Check trx1 result= %d\n", trx1_ret);
                }
                if(trx1_ret) { //trx1 failed
#else
                if (check_trx(trx_name) || nvram_match("asus_trx_test", "1")) {
#endif
                        xprintf("Hello!! Enter Rescue Mode: (Check error)\n\n");
			FW_err_count = atoi(nvram_get("Ate_FW_err"));
			FW_err_count++;
			sprintf(FW_err, "%d", FW_err_count);
			nvram_set("Ate_FW_err", FW_err);
			nvram_commit();
			if(nvram_match("asus_mfg", "1")){	// goto cmd mode if during ATE mode
				LEDOFF();
				cfe_command_loop();
			} else{					// 3 steps
				LEDOFF();
				/* wait for cmd input */
				xprintf("\n1. Wait 10 secs to enter tftp mode\n   or push RESCUE-BTN to enter cmd mode\n");
				for(i=0; i<3; ++i){
					if (DETECT())
						cfe_command_loop();
					else
						cfe_sleep(2*CFE_HZ);
				}
                        	/* Wait awhile for an image */
				xprintf("\n2. enter tftp mode:\n");
				i=0;
                        	while ((ret = ui_docommand("flash -noheader : nflash1.trx")) == CFE_ERR_TIMEOUT) {
                                	if (i%2 == 0)
                                        	LEDOFF();
                                	else
                                        	LEDON();
                                	i++;
					/*
                                	if (i==0xffffff)
                                        	i = 0;
					*/
                                	if (i > 20)
						break;
                        	}
				/* try reboot if no actions at all */
                        	xprintf("\n\n3. rebooting...\n");
                		ui_docommand ("reboot");
			}
                } else if (!nvram_invmatch("boot_wait", "on")) {
                //} else if (0) {
                        xprintf("go load\n");   // tmp test
                        ui_get_loadbuf(&ptr, &bufsize);
                        /* Load the image */
                        sprintf(buf, "load -raw -addr=0x%x -max=0x%x :", (unsigned int)ptr, bufsize);
                        ret = ui_docommand(buf);

                	/* Load was successful. Check for the TRX magic.
                 	* If it's a TRX image, then proceed to flash it, else try to boot
                 	* Note: To boot a TRX image directly from the memory, address will need to be
                 	* load address + trx header length.
                 	*/
                        if (ret == 0) {
                                file_buf = (struct trx_header *)ptr;
                                /* If it's a TRX, then proceed to writing to flash else,
                                 * try to boot from memory
                                 */
                                if (file_buf->magic != TRX_MAGIC) {
                                        sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x -fs=memory :0x%x",
                                                osaddr, (unsigned int)bufsize, (unsigned int)ptr);
                                        return ui_docommand(buf);
                                }
                                /* Flash the image from memory directly */
                                sprintf(buf, "flash -noheader -mem -size=0x%x 0x%x nflash1.trx",
                                        (unsigned int)file_buf->len, (unsigned int)ptr);
                                ret = ui_docommand(buf);
                        }
                }
        }

#else // RESCUE_MODE

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
	if (bootpartition != NULL) {
#ifdef CFG_NFLASH
		ui_get_trx_flashdev(trx_name);
#endif
                /* init trx crc check records */
                init_trx_crc_rec(trx_name);

                /* optimize boot image check */
                boot_img_idx = atoi(bootpartition) % MAX_BOOT_IMGAGE;
                trx_name_1st = img_crc_rec[boot_img_idx].name;
                trx_name_2nd = img_crc_rec[boot_img_idx ^ 0x1].name; /* flip the idx */
 
                trx_failed = check_trx(trx_name_1st);
                if (trx_failed != 0) {
                        printf("partition 0 trx image check error. %d", trx_failed);
                        trx_failed &= check_trx(trx_name_2nd);
                }
	} else
#endif	/* FAILSAFE_UPGRADE || DUAL_IMAGE */
	{
#ifdef CFG_NFLASH
		ui_get_trx_flashdev(trx_name);
		ui_get_os_flashdev(os_name);
#endif
		trx_failed = check_trx(trx_name);
	}

	if (trx_failed) {
		/* Wait for CFE_ERR_TIMEOUT_LIMIT for an image */
		while (1) {
			sprintf(buf, "flash -noheader :%s", trx_name);
			if ((ret = ui_docommand(buf)) != CFE_ERR_TIMEOUT)
				break;
			if (++retry == CFE_ERR_TIMEOUT_LIMIT) {
				ret = CFE_ERR_INTR;
				break;
			}
		}
	//} else if (!nvram_invmatch("boot_wait", "on")) {
	} else if (0) {	// disable for tmp
		ui_get_loadbuf(&ptr, &bufsize);
		/* Load the image */
		sprintf(buf, "load -raw -addr=0x%p -max=0x%x :", ptr, bufsize);
		ret = ui_docommand(buf);

		/* Load was successful. Check for the TRX magic.
		 * If it's a TRX image, then proceed to flash it, else try to boot
		 * Note: To boot a TRX image directly from the memory, address will need to be
		 * load address + trx header length.
		 */
		if (ret == 0) {
			file_buf = (struct trx_header *)ptr;
			/* If it's a TRX, then proceed to writing to flash else,
			 * try to boot from memory
			 */
			if (file_buf->magic != TRX_MAGIC) {
				sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x -fs=memory :0x%p",
				        osaddr, bufsize, ptr);
				return ui_docommand(buf);
			}
			/* Flash the image from memory directly */
			sprintf(buf, "flash -noheader -mem -size=0x%x 0x%p %s",
			        file_buf->len, ptr, trx_name);
			ret = ui_docommand(buf);
		}
	}
#endif  // RESCUE_MODE

	if (ret == CFE_ERR_INTR)
		return ret;
	/* Boot the image */
#ifdef	__ARM_ARCH_7A__
	/* Support for loading to ACP base */
	bufsize = (PHYSADDR(mem_bottomofmem) - (PHYSADDR(osaddr) & ~0x80000000));
#else
	bufsize = (PHYSADDR(mem_bottomofmem) - PHYSADDR(osaddr));
#endif

#if defined(FAILSAFE_UPGRADE) || defined(DUAL_IMAGE)
	/* Get linux_boot variable to see what is current image */
	if (bootpartition != NULL) {
		char temp[20];
		int i = atoi(bootpartition);
#ifdef FAILSAFE_UPGRADE
		if (maxpartialboots && (atoi(maxpartialboots) > 0)) {
			if (partialboots && (atoi(partialboots) > atoi(maxpartialboots))) {
				i = 1-i;
				printf("Changed to the other image %d"
				       " (maxpartialboots exceeded)\n", i);
				/* reset to new image */
				sprintf(temp, "%d", i);
				nvram_set(PARTIALBOOTS, "1");
				nvram_set(BOOTPARTITION, temp);
				nvram_commit();
			} else {
				/* Increment the counter */
				sprintf(temp, "%d", partialboots ? atoi(partialboots)+1 : 1);
				nvram_set(PARTIALBOOTS, temp);
				nvram_commit();
			}
		}
#endif	/* FAILSAFE_UPGRADE */
		if (i > 1) {
			/* If we hav eexceeded the max partition # reset it to 0 */
			i = 0;
			nvram_set(BOOTPARTITION, "0");
			nvram_commit();
		}
		/* We try the specified one, if it is failed, we try the other one */
		if (check_image_prepare_cmd(i, buf, osaddr, bufsize) == 0) {
			printf("Booting(%d): %s\n", i, buf);
		} else if (check_image_prepare_cmd(1-i, buf, osaddr, bufsize) == 0) {
			printf("Changed to the other image %d\n", 1 - i);
			sprintf(temp, "%d", 1-i);
			nvram_set(BOOTPARTITION, temp);
#ifdef FAILSAFE_UPGRADE
			nvram_set(PARTIALBOOTS, "1");
#endif
			nvram_commit();
		} else {
			printf("Both images bad!!!\n");
			buf[0] = 0;
		}
	}
	else
#endif /* FAILSAFE_UPGRADE || DUAL_IMAGE */
	sprintf(buf, "boot -raw -z -addr=0x%x -max=0x%x %s:", osaddr, bufsize, os_name);

	return ui_docommand(buf);
}

static int
ui_cmd_clocks(ui_cmdline_t *cmd, int argc, char *argv[])
{
	chipcregs_t *cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	uint32 ccc = R_REG(NULL, &cc->capabilities);
	uint32 pll = ccc & CC_CAP_PLL_MASK;

	if (pll != PLL_NONE) {
		uint32 n = R_REG(NULL, &cc->clockcontrol_n);
		printf("Current clocks for pll=0x%x:\n", pll);
		printf("    mips: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_m3)));
		printf("    sb: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_sb)));
		printf("    pci: %d\n", si_clock_rate(pll, n, R_REG(NULL, &cc->clockcontrol_pci)));
		printf("    mipsref: %d\n", si_clock_rate(pll, n,
		       R_REG(NULL, &cc->clockcontrol_m2)));
	} else {
		printf("Current clocks: %d/%d/%d/%d Mhz.\n",
		       si_cpu_clock(sih) / 1000000,
		       si_mem_clock(sih) / 1000000,
		       si_clock(sih) / 1000000,
		       si_alp_clock(sih) / 1000000);
	}
	return 0;
}

int
ui_init_bcm947xxcmds(void)
{
	cmd_addcmd("reboot",
	           ui_cmd_reboot,
	           NULL,
	           "Reboot.",
	           "reboot\n\n"
	           "Reboots.",
	           "");
	cmd_addcmd("nvram",
	           ui_cmd_nvram,
	           NULL,
	           "NVRAM utility.",
	           "nvram [command] [args..]\n\n"
	           "Access NVRAM.",
	           "get [name];Gets the value of the specified variable|"
	           "set [name=value];Sets the value of the specified variable|"
	           "unset [name];Deletes the specified variable|"
	           "commit;Commit variables to flash|"
		   "corrupt;Corrupt NVRAM -- For testing purposes|"
#ifdef BCMNVRAMW
		       "otpcommit ;Commit otp nvram header to flash"
#endif
	           "erase;Erase all nvram|"
	           "show;Shows all variables|");
#ifdef BCM_DEVINFO
        cmd_addcmd("devinfo",
                   ui_cmd_devinfo,
                   NULL,
                   "Device Info NVRAM utility.",
                   "devinfo [command] [args..]\n\n"
                   "Access devinfo NVRAM.",
                   "get [name];Gets the value of the specified variable|"
                   "set [name=value];Sets the value of the specified variable|"
                   "unset [name];Deletes the specified variable|"
                   "commit;Commit variables to flash|"
#ifdef BCMNVRAMW
                       "otpcommit ;Commit otp devinfo nvram header to flash"
#endif
                   "erase;Erase all devinfo nvram|"
                   "show;Shows all devinfo nvram variables|");
#endif
	cmd_addcmd("go",
	           ui_cmd_go,
	           NULL,
	           "Verify and boot OS image.",
	           "go\n\n"
	           "Boots OS image if valid. Waits for a new OS image if image is invalid\n"
	           "or boot_wait is unset or not on.",
	           "");
	cmd_addcmd("show clocks",
	           ui_cmd_clocks,
	           NULL,
	           "Show current values of the clocks.",
	           "show clocks\n\n"
	           "Shows the current values of the clocks.",
	           "");

#if CFG_WLU
	wl_addcmd();
#endif

	return 0;
}
