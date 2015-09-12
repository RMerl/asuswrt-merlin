/*
 * HND MIPS boards setup routines
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: setup.c,v 1.23 2010-10-20 08:26:12 $
 */

#include <linux/types.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_core.h>
#include <linux/serial_8250.h>	/* for early_serial_setup */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
#include <linux/blkdev.h>
#include <linux/ide.h>
#endif
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/time.h>
#include <asm/reboot.h>

#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/romfs_fs.h>
#include <linux/cramfs_fs.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/squashfs_fs.h>
#else
/* #include <magic.h> */
#endif
#endif
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/initrd.h>
#endif
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <siutils.h>
#include <hndsoc.h>
#include <hndcpu.h>
#include <mips33_core.h>
#include <mips74k_core.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <hndpci.h>
#include <trxhdr.h>
#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif /* HNDCTF */
#include "bcm947xx.h"
#ifdef CONFIG_MTD_NFLASH
#include "nflash.h"
#endif
#include "bcmdevs.h"

extern void bcm947xx_time_init(void);
extern void bcm947xx_timer_setup(struct irqaction *irq);

#ifdef CONFIG_KGDB
extern void set_debug_traps(void);
extern void rs_kgdb_hook(struct uart_port *);
extern void breakpoint(void);
#endif

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
extern struct ide_ops std_ide_ops;
#endif

struct dummy_super_block {
	u32	s_magic ;
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
/* Enable CPU wait or not */
extern int cpu_wait_enable;
#else
int coherentio;   /* init to 0 => no DMA cache coherency (may be set by user) */
int hw_coherentio;/* init to 0 => no HW DMA cache coherency (reflects real HW) */
#endif

/* Global SB handle */
si_t *bcm947xx_sih = NULL;
spinlock_t bcm947xx_sih_lock = SPIN_LOCK_UNLOCKED;
EXPORT_SYMBOL(bcm947xx_sih);
EXPORT_SYMBOL(bcm947xx_sih_lock);

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock

#ifdef HNDCTF
ctf_t *kcih = NULL;
EXPORT_SYMBOL(kcih);
ctf_attach_t ctf_attach_fn = NULL;
EXPORT_SYMBOL(ctf_attach_fn);
#endif /* HNDCTF */

/* Kernel command line */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
extern char arcs_cmdline[CL_SIZE];
#endif
static int lanports_enable = 0;


EXPORT_SYMBOL( si_router_coma );	/* for loadable modules */
EXPORT_SYMBOL( hnd_jtagm_init );
EXPORT_SYMBOL( hnd_jtagm_disable );
EXPORT_SYMBOL( jtag_scan );

static void
bcm947xx_reboot_handler(void)
{
#ifndef BCMDBG
	int wombo_reset;
#endif /* BCMDBG */

	/* Reset the PCI(e) interfaces */
	if (CHIPID(sih->chip) == BCM4706_CHIP_ID)
		hndpci_deinit(sih);

	if (lanports_enable) {
		uint lp = 1 << lanports_enable;

		si_gpioout(sih, lp, 0, GPIO_DRV_PRIORITY);
		si_gpioouten(sih, lp, lp, GPIO_DRV_PRIORITY);
		bcm_mdelay(1);
	}

#ifndef BCMDBG
	/* gpio 0 is also valid wombo_reset */
	if ((wombo_reset = getgpiopin(NULL, "wombo_reset", GPIO_PIN_NOTDEFINED)) !=
	    GPIO_PIN_NOTDEFINED) {
		int reset = 1 << wombo_reset;

		si_gpioout(sih, reset, 0, GPIO_DRV_PRIORITY);
		si_gpioouten(sih, reset, reset, GPIO_DRV_PRIORITY);
		bcm_mdelay(10);
	}
#endif /* BCMDBG */
}

void
bcm947xx_machine_restart(char *command)
{
	printk("Please stand by while rebooting the system...\n");

	/* Set the watchdog timer to reset immediately */
	local_irq_disable();
	bcm947xx_reboot_handler();
	hnd_cpu_reset(sih);
}

void
bcm947xx_machine_halt(void)
{
	printk("System halted\n");

	/* Disable interrupts and watchdog and spin forever */
	local_irq_disable();
	si_watchdog(sih, 0);
	bcm947xx_reboot_handler();
	while (1);
}

#ifdef CONFIG_SERIAL_CORE

static struct uart_port rs = {
	line: 0,
	flags: ASYNC_BOOT_AUTOCONF,
	iotype: SERIAL_IO_MEM,
};

static void __init
serial_add(void *regs, uint irq, uint baud_base, uint reg_shift)
{
	rs.membase = regs;
	rs.irq = irq + 2;
	rs.uartclk = baud_base;
	rs.regshift = reg_shift;

	early_serial_setup(&rs);

	rs.line++;
}

static void __init
serial_setup(si_t *sih)
{
	si_serial_init(sih, serial_add);

#ifdef CONFIG_KGDB
	/* Use the last port for kernel debugging */
	if (rs.membase)
		rs_kgdb_hook(&rs);
#endif
}

#endif /* CONFIG_SERIAL_CORE */

static int boot_flags(void)
{
	int bootflags = 0;
	char *val;

	/* Only support chipcommon revision == 38 or BCM4706 for now */
	if ((CHIPID(sih->chip) == BCM4706_CHIP_ID) || sih->ccrev == 38) {
		if (sih->ccrev == 38 && (sih->chipst & (1 << 4)) != 0) {
			/* This is NANDBOOT */
			bootflags = FLASH_BOOT_NFLASH | FLASH_KERNEL_NFLASH;
		}
		else if ((val = nvram_get("bootflags"))) {
			bootflags = simple_strtol(val, NULL, 0);
			bootflags &= FLASH_KERNEL_NFLASH;
		}
	}

	return bootflags;
}

static int rootfs_mtdblock(void)
{
	int bootflags;
	int block = 0;

	bootflags = boot_flags();

	/* NANDBOOT */
	if ((bootflags & (FLASH_BOOT_NFLASH | FLASH_KERNEL_NFLASH)) ==
		(FLASH_BOOT_NFLASH | FLASH_KERNEL_NFLASH))
		return 3;

	/* SFLASH/PFLASH only */
	if ((bootflags & (FLASH_BOOT_NFLASH | FLASH_KERNEL_NFLASH)) == 0)
		return 2;

#ifdef BCMCONFMTD
	block++;
#endif
	/* Boot from norflash and kernel in nandflash */
	return block+3;
}

void __init
brcm_setup(void)
{
	char *value;

	/* Get global SB handle */
	sih = si_kattach(SI_OSH);

	/* Initialize clocks and interrupts */
	si_mips_init(sih, SBMIPS_VIRTIRQ_BASE);

	if (BCM330X(current_cpu_data.processor_id) &&
		(read_c0_diag() & BRCM_PFC_AVAIL)) {
		/* 
		 * Now that the sih is inited set the  proper PFC value 
		 */	
		printk("Setting the PFC to its default value\n");
		enable_pfc(PFC_AUTO);
	}


#ifdef CONFIG_SERIAL_CORE
	/* Initialize UARTs */
	serial_setup(sih);
#endif /* CONFIG_SERIAL_CORE */

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
	ide_ops = &std_ide_ops;
#endif

	sprintf(arcs_cmdline, "root=/dev/mtdblock%d console=ttyS0,115200 init=/sbin/preinit", rootfs_mtdblock());

	/* Override default command line arguments */
	value = nvram_get("kernel_args");
	if (value && strlen(value) && strncmp(value, "empty", 5))
		strncpy(arcs_cmdline, value, sizeof(arcs_cmdline));


	if ((lanports_enable = getgpiopin(NULL, "lanports_enable", GPIO_PIN_NOTDEFINED)) ==
		GPIO_PIN_NOTDEFINED)
		lanports_enable = 0;


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	/* Check if we want to enable cpu wait */
	if (nvram_match("wait", "1"))
		cpu_wait_enable = 1;
#endif

	/* Generic setup */
	_machine_restart = bcm947xx_machine_restart;
	_machine_halt = bcm947xx_machine_halt;
	pm_power_off = bcm947xx_machine_halt;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	board_time_init = bcm947xx_time_init;
#endif
}

const char *
get_system_type(void)
{
	static char s[32];

	if (bcm947xx_sih) {
		sprintf(s, "Broadcom BCM%X chip rev %d", bcm947xx_sih->chip,
			bcm947xx_sih->chiprev);
		return s;
	}
	else
		return "Broadcom BCM947XX";
}

void __init
bus_error_init(void)
{
}

void __init
plat_mem_setup(void)
{
	brcm_setup();
	return;
}

#ifdef CONFIG_MTD_PARTITIONS

static struct mutex *mtd_mutex = NULL;

struct mutex *partitions_mutex_init(void)
{
	if (!mtd_mutex) {
		mtd_mutex = (struct mutex *)kzalloc(sizeof(struct mutex), GFP_KERNEL);
		if (!mtd_mutex)
			return NULL;
		mutex_init(mtd_mutex);
	}
	return mtd_mutex;
}
EXPORT_SYMBOL(partitions_mutex_init);

/* Find out prom size */
static uint32 boot_partition_size(uint32 flash_phys) {
	uint32 bootsz, *bisz;

	/* Default is 256K boot partition */
	bootsz = 256 * 1024;

	/* Do we have a self-describing binary image? */
	bisz = (uint32 *)KSEG1ADDR(flash_phys + BISZ_OFFSET);
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
	return bootsz;
}

#if defined(BCMCONFMTD)
#define FLASH_PARTS_NUM	6
#else
#define FLASH_PARTS_NUM	5 /* boot;nvram;kernel;rootfs;empty */
#endif

static struct mtd_partition bcm947xx_flash_parts[FLASH_PARTS_NUM] = {{0}};

static uint lookup_flash_rootfs_offset(struct mtd_info *mtd, int *trx_off, size_t size)
{
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	struct squashfs_super_block *squashfsb;
#else
	struct dummy_super_block *squashfsb;
#endif
	struct trx_header *trx;
	unsigned char buf[512];
	int off;
	size_t len;

	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	squashfsb = (void *) buf;
	trx = (struct trx_header *) buf;

	/* Look at every 64 KB boundary */
	for (off = 0; off < size; off += (64 * 1024)) {
		memset(buf, 0xe5, sizeof(buf));

		/*
		 * Read block 0 to test for romfs and cramfs superblock
		 */
		if (mtd->read(mtd, off, sizeof(buf), &len, buf) ||
		    len != sizeof(buf))
			continue;

		/* Try looking at TRX header for rootfs offset */
		if (le32_to_cpu(trx->magic) == TRX_MAGIC) {
			*trx_off = off;
			if (trx->offsets[1] == 0)
				continue;
			/*
			 * Read to test for romfs and cramfs superblock
			 */
			off += le32_to_cpu(trx->offsets[1]);
			memset(buf, 0xe5, sizeof(buf));
			if (mtd->read(mtd, off, sizeof(buf), &len, buf) || len != sizeof(buf))
				continue;
		}

		/* romfs is at block zero too */
		if (romfsb->word0 == ROMSB_WORD0 &&
		    romfsb->word1 == ROMSB_WORD1) {
			printk(KERN_NOTICE
			       "%s: romfs filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}

		/* so is cramfs */
		if (cramfsb->magic == CRAMFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: cramfs filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}
		if (squashfsb->s_magic == SQUASHFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: squash filesystem found at block %d\n",
			       mtd->name, off / mtd->erasesize);
			break;
		}
	}

	return off;
}
struct mtd_partition *
init_mtd_partitions(struct mtd_info *mtd, size_t size)
{
	int bootflags;
	int nparts = 0;
	uint32 offset = 0;
	uint rfs_off = 0;
	uint vmlz_off, knl_size;
	uint32 top = 0;
	uint32 bootsz;

	bootflags = boot_flags();

	if ((bootflags & FLASH_KERNEL_NFLASH) != FLASH_KERNEL_NFLASH) {
		rfs_off = lookup_flash_rootfs_offset(mtd, &vmlz_off, size);

		/* Size pmon */
		bcm947xx_flash_parts[nparts].name = "boot";
		bcm947xx_flash_parts[nparts].size = vmlz_off;
		bcm947xx_flash_parts[nparts].offset = top;
		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		nparts++;

		/* Setup kernel MTD partition */
		bcm947xx_flash_parts[nparts].name = "linux";
		bcm947xx_flash_parts[nparts].size = mtd->size - vmlz_off;
		
		/* Reserve for NVRAM */
		bcm947xx_flash_parts[nparts].size -= ROUNDUP(NVRAM_SPACE, mtd->erasesize);

#ifdef BCMCONFMTD
		bcm947xx_flash_parts[nparts].size -= (mtd->erasesize *4);
#endif
		bcm947xx_flash_parts[nparts].offset = vmlz_off;
		knl_size = bcm947xx_flash_parts[nparts].size;
		offset = bcm947xx_flash_parts[nparts].offset + knl_size;
		nparts++; 
		
		/* Setup rootfs MTD partition */
		bcm947xx_flash_parts[nparts].name = "rootfs";
		bcm947xx_flash_parts[nparts].size = knl_size - (rfs_off - vmlz_off);
		bcm947xx_flash_parts[nparts].offset = rfs_off;
		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		nparts++;
	} else {
		bootsz = boot_partition_size(SI_FLASH2);
		printk("Boot partition size = %d(0x%x)\n", bootsz, bootsz);
		/* Size pmon */
		bcm947xx_flash_parts[nparts].name = "boot";
		bcm947xx_flash_parts[nparts].size = bootsz;
		bcm947xx_flash_parts[nparts].offset = top;
		bcm947xx_flash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		offset = bcm947xx_flash_parts[nparts].size;
		nparts++;
	}

#ifdef BCMCONFMTD
	/* Setup CONF MTD partition */
	bcm947xx_flash_parts[nparts].name = "confmtd";
	bcm947xx_flash_parts[nparts].size = mtd->erasesize * 4;
	bcm947xx_flash_parts[nparts].offset = offset;
	offset = bcm947xx_flash_parts[nparts].offset + bcm947xx_flash_parts[nparts].size;
	nparts++;
#endif	/* BCMCONFMTD */

	/* Setup nvram MTD partition */
	bcm947xx_flash_parts[nparts].name = "nvram";
	bcm947xx_flash_parts[nparts].size = ROUNDUP(NVRAM_SPACE, mtd->erasesize);
	bcm947xx_flash_parts[nparts].offset = size - bcm947xx_flash_parts[nparts].size;
	nparts++;

	return bcm947xx_flash_parts;
}

EXPORT_SYMBOL(init_mtd_partitions);

#ifdef CONFIG_MTD_NFLASH
#define NFLASH_PARTS_NUM	6
static struct mtd_partition bcm947xx_nflash_parts[NFLASH_PARTS_NUM] = {{0}};

static uint lookup_nflash_rootfs_offset(struct mtd_info *mtd, int offset, size_t size) 
{
	struct romfs_super_block *romfsb;
	struct cramfs_super *cramfsb;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
	struct squashfs_super_block *squashfsb;
#else
	struct dummy_super_block *squashfsb;
#endif
	struct trx_header *trx;
	unsigned char buf[NFL_SECTOR_SIZE];
	uint blocksize, mask, blk_offset, off, shift = 0;
	chipcregs_t *cc;
	int ret;
	
	romfsb = (struct romfs_super_block *) buf;
	cramfsb = (struct cramfs_super *) buf;
	squashfsb = (void *) buf;
	trx = (struct trx_header *) buf;

	if ((cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX)) == NULL)
		return 0;

	/* Look at every block boundary till 16MB; higher space is reserved for application data. */
	blocksize = mtd->erasesize;
	printk("lookup_nflash_rootfs_offset: offset = 0x%x\n", offset);
	for (off = offset; off < NFL_BOOT_OS_SIZE; off += blocksize) {
		mask = blocksize - 1;
		blk_offset = off & ~mask;
		if (nflash_checkbadb(sih, cc, blk_offset) != 0)
			continue;
		memset(buf, 0xe5, sizeof(buf));
		if ((ret = nflash_read(sih, cc, off, sizeof(buf), buf)) != sizeof(buf)) {
			printk(KERN_NOTICE
			       "%s: nflash_read return %d\n", mtd->name, ret);
			continue;
		}
		
		/* Try looking at TRX header for rootfs offset */
		if (le32_to_cpu(trx->magic) == TRX_MAGIC) {
			mask = NFL_SECTOR_SIZE - 1;
			off = offset + (le32_to_cpu(trx->offsets[1]) & ~mask) - blocksize;
			shift = (le32_to_cpu(trx->offsets[1]) & mask);
			romfsb = (struct romfs_super_block *)((unsigned char *)romfsb + shift);
			cramfsb = (struct cramfs_super *)((unsigned char *)cramfsb + shift);
			squashfsb = (void *)((unsigned char *)squashfsb + shift);
			continue;
		}

		/* romfs is at block zero too */
		if (romfsb->word0 == ROMSB_WORD0 &&
		    romfsb->word1 == ROMSB_WORD1) {
			printk(KERN_NOTICE
			       "%s: romfs filesystem found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}

		/* so is cramfs */
		if (cramfsb->magic == CRAMFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: cramfs filesystem found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}

		if (squashfsb->s_magic == SQUASHFS_MAGIC) {
			printk(KERN_NOTICE
			       "%s: squash filesystem found at block %d\n",
			       mtd->name, off / blocksize);
			break;
		}
	} 
	return shift + off;
}

struct mtd_partition * init_nflash_mtd_partitions(struct mtd_info *mtd, size_t size)
{
	int bootflags;
	int nparts = 0;
	uint32 offset = 0;
	uint shift = 0;
	uint32 top = 0;
	uint32 bootsz;
	
	bootflags = boot_flags();
	if ((bootflags & FLASH_BOOT_NFLASH) == FLASH_BOOT_NFLASH) {
		bootsz = boot_partition_size(SI_FLASH1);
		if (bootsz > mtd->erasesize) {
			/* Prepare double space in case of bad blocks */
			bootsz = (bootsz << 1);
		} else {
			/* CFE occupies at least one block */
			bootsz = mtd->erasesize;
		}
		printk("Boot partition size = %d(0x%x)\n", bootsz, bootsz);

		/* Size pmon */
		bcm947xx_nflash_parts[nparts].name = "boot";
		bcm947xx_nflash_parts[nparts].size = bootsz;
		bcm947xx_nflash_parts[nparts].offset = top;
		bcm947xx_nflash_parts[nparts].mask_flags = MTD_WRITEABLE; /* forces on read only */
		offset = bcm947xx_nflash_parts[nparts].size;
		nparts++;

		/* Setup NVRAM MTD partition */
		bcm947xx_nflash_parts[nparts].name = "nvram";
		bcm947xx_nflash_parts[nparts].size = NFL_BOOT_SIZE - offset;
		bcm947xx_nflash_parts[nparts].offset = offset;
			
		offset = NFL_BOOT_SIZE;
		nparts++;
	}
		
	if ((bootflags & FLASH_KERNEL_NFLASH) == FLASH_KERNEL_NFLASH) {
		/* Setup kernel MTD partition */
		bcm947xx_nflash_parts[nparts].name = "linux";
		bcm947xx_nflash_parts[nparts].size = nparts ? (NFL_BOOT_OS_SIZE - NFL_BOOT_SIZE) : NFL_BOOT_OS_SIZE;
		bcm947xx_nflash_parts[nparts].offset = offset;
			
		shift = lookup_nflash_rootfs_offset(mtd, offset, size);
		
		offset = NFL_BOOT_OS_SIZE;
		nparts++;
		
		/* Setup rootfs MTD partition */
		bcm947xx_nflash_parts[nparts].name = "rootfs";
		bcm947xx_nflash_parts[nparts].size = NFL_BOOT_OS_SIZE - shift;
		bcm947xx_nflash_parts[nparts].offset = shift;
		bcm947xx_nflash_parts[nparts].mask_flags = MTD_WRITEABLE;
		
		nparts++;
	}

	return bcm947xx_nflash_parts;
}

EXPORT_SYMBOL(init_nflash_mtd_partitions);
#endif /* CONFIG_MTD_NFLASH */

#ifdef CONFIG_BLK_DEV_INITRD
extern char _end;

/* The check_ramdisk_trx has more exact qualification to look at TRX header from end of linux */
static __init int
check_ramdisk_trx(unsigned long offset, unsigned long ram_size)
{
	struct trx_header *trx;
	uint32 crc;
	unsigned int len;
	uint8 *ptr = (uint8 *)offset;

	trx = (struct trx_header *)ptr;

	/* Not a TRX_MAGIC */
	if (le32_to_cpu(trx->magic) != TRX_MAGIC) {
		printk("check_ramdisk_trx: not a valid TRX magic\n");
		return -1;
	}

	/* TRX len invalid */
	len = le32_to_cpu(trx->len);
	if (offset + len > ram_size) {
		printk("check_ramdisk_trx: not a valid TRX length\n");
		return -1;
	}

	/* Checksum over header */
	crc = hndcrc32((uint8 *) &trx->flag_version,
		    sizeof(struct trx_header) - OFFSETOF(struct trx_header, flag_version),
		    CRC32_INIT_VALUE);

	/* Move ptr to data */
	ptr += sizeof(struct trx_header);
	len -= sizeof(struct trx_header);

	/* Checksum over data */
	crc = hndcrc32(ptr, len, crc);

	/* Verify checksum */
	if (le32_to_cpu(trx->crc32) != crc) {
		printk("check_ramdisk_trx: checksum invalid\n");
		return -1;
	}

	return 0;
}

void __init init_ramdisk(unsigned long mem_end)
{
	struct trx_header *trx = NULL;
	char *from_rootfs, *to_rootfs;
	unsigned long rootfs_size = 0;
	unsigned long ram_size = mem_end + 0x80000000;
	unsigned long offset;
	char *root_cmd = "root=/dev/ram0 console=ttyS0,115200 rdinit=/sbin/preinit";

	to_rootfs = (char *)(((unsigned long)&_end + PAGE_SIZE-1) & PAGE_MASK);
	offset = ((unsigned long)&_end +0xffff) & ~0xffff;

	/* Look at TRX header from end of linux */
	for (; offset < ram_size; offset += 0x10000) {
		trx = (struct trx_header *)offset;
		if (le32_to_cpu(trx->magic) == TRX_MAGIC &&
			check_ramdisk_trx(offset, ram_size) == 0) {
			printk(KERN_NOTICE
				   "Found TRX image  at %08lx\n", offset);
			from_rootfs = (char *)((unsigned long)trx + le32_to_cpu(trx->offsets[1]));
			rootfs_size = le32_to_cpu(trx->len) - le32_to_cpu(trx->offsets[1]);
			rootfs_size = (rootfs_size + 0xffff) & ~0xffff;
			printk("rootfs size is %ld bytes at 0x%p, copying to 0x%p\n", rootfs_size, from_rootfs, to_rootfs);
			memmove(to_rootfs, from_rootfs, rootfs_size);

			initrd_start = (int)to_rootfs;
			initrd_end = initrd_start + rootfs_size;
			strncpy(arcs_cmdline, root_cmd, sizeof(arcs_cmdline));
			/* 
			 * In case the system warm boot, the memory won't be zeroed.
			 * So we have to erase trx magic.
			 */
			if (initrd_end < (unsigned long)trx)
				trx->magic = 0;
			break;
		}
	}
}
#endif
#endif
