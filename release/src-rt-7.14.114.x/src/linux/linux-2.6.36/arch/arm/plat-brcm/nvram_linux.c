/*
 * NVRAM variable manipulation (Linux kernel half)
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
 * $Id: nvram_linux.c,v 1.10 2010-09-17 04:51:19 $
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36)
#include <linux/config.h>
#endif

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mtd/mtd.h>

#include <typedefs.h>
#include <bcmendian.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <bcmdefs.h>
#include <hndsoc.h>
#include <siutils.h>
#include <hndmips.h>
#include <hndsflash.h>
#include <bcmdevs.h>
#ifdef CONFIG_MTD_NFLASH
#include <nflash.h>
#endif

int nvram_space = DEF_NVRAM_SPACE;

/* Temp buffer to hold the nvram transfered romboot CFE */
char __initdata ram_nvram_buf[MAX_NVRAM_SPACE] __attribute__((aligned(PAGE_SIZE)));

/* In BSS to minimize text size and page aligned so it can be mmap()-ed */
static char nvram_buf[MAX_NVRAM_SPACE] __attribute__((aligned(PAGE_SIZE)));
static bool nvram_inram = FALSE;

static char *early_nvram_get(const char *name);
#ifdef MODULE

#define early_nvram_get(name) nvram_get(name)

#else /* !MODULE */

/* Global SB handle */
extern si_t *bcm947xx_sih;
extern spinlock_t bcm947xx_sih_lock;

/* Convenience */
#define sih bcm947xx_sih
#define sih_lock bcm947xx_sih_lock
#define KB * 1024
#define MB * 1024 * 1024

#ifndef	MAX_MTD_DEVICES
#define	MAX_MTD_DEVICES	32
#endif

static struct resource norflash_region = {
	.name = "norflash",
	.start = 0x1E000000,
	.end =  0x1FFFFFFF,
        .flags = IORESOURCE_MEM,
};

static unsigned char flash_nvh[MAX_NVRAM_SPACE];
#ifdef CONFIG_MTD_NFLASH

#define NLS_XFR 1              /* added by Jiahao for WL500gP */
#ifdef NLS_XFR

#include <linux/nls.h>

static char *NLS_NVRAM_U2C="asusnlsu2c";
static char *NLS_NVRAM_C2U="asusnlsc2u";
__u16 unibuf[1024];
char codebuf[1024];
char tmpbuf[1024];

void
asusnls_u2c(char *name)
{
#ifdef CONFIG_NLS
        char *codepage;
        char *xfrstr;
        struct nls_table *nls;
        int ret, len;

        strcpy(codebuf, name);
        codepage=codebuf+strlen(NLS_NVRAM_U2C);
        if((xfrstr=strchr(codepage, '_')))
        {
                *xfrstr=NULL;
                xfrstr++;
                nls=load_nls(codepage);
                if(!nls)
                {
                        printk("NLS table is null!!\n");
                }
                else {
                        len = 0;
                        if (ret=utf8s_to_utf16s(xfrstr, strlen(xfrstr), unibuf))
                        {
                                int i;
                                for (i = 0; (i < ret) && unibuf[i]; i++) {
                                        int charlen;
                                        charlen = nls->uni2char(unibuf[i], &name[len], NLS_MAX_CHARSET_SIZE);
                                        if (charlen > 0) {
                                                len += charlen;
                                        }
                                        else {
                                                strcpy(name, "");
                                                unload_nls(nls);
                                                return;
                                        }
                                }
                                name[len] = 0;
                        }
                        unload_nls(nls);
                        if(!len)
                        {
                                printk("can not xfr from utf8 to %s\n", codepage);
                                strcpy(name, "");
                        }
                }
        }
        else
        {
                strcpy(name, "");
        }
#endif
}

void
asusnls_c2u(char *name)
{
#ifdef CONFIG_NLS
        char *codepage;
        char *xfrstr;
        struct nls_table *nls;
        int ret;

        strcpy(codebuf, name);
        codepage=codebuf+strlen(NLS_NVRAM_C2U);
        if((xfrstr=strchr(codepage, '_')))
        {
                *xfrstr=NULL;
                xfrstr++;

                strcpy(name, "");
                nls=load_nls(codepage);
                if(!nls)
                {
                        printk("NLS table is null!!\n");
                }
                else
                {
                        int charlen;
                        int i;
                        int len = strlen(xfrstr);
                        for (i = 0; len && *xfrstr; i++, xfrstr += charlen, len -= charlen) {   /* string to unicode */
                                charlen = nls->char2uni(xfrstr, len, &unibuf[i]);
                                if (charlen < 1) {
                                        strcpy(name ,"");
                                        unload_nls(nls);
                                        return;
                                }
                        }
                        unibuf[i] = 0;
                        ret=utf16s_to_utf8s(unibuf, i, UTF16_HOST_ENDIAN, name, 1024);  /* unicode to utf-8, 1024 is size of array unibuf */
                        name[ret]=0;
                        unload_nls(nls);
                        if(!ret)
                        {
                                printk("can not xfr from %s to utf8\n", codepage);
                                strcpy(name, "");
                        }
                }
        }
        else
        {
                strcpy(name, "");
      }
#endif
}

char *
nvram_xfr(const char *buf)
{
        char *name = tmpbuf;
        ssize_t ret=0;

        if (copy_from_user(name, buf, strlen(buf)+1)) {
                ret = -EFAULT;
                goto done;
        }

        if (strncmp(tmpbuf, NLS_NVRAM_U2C, strlen(NLS_NVRAM_U2C))==0)
        {
                asusnls_u2c(tmpbuf);
        }
        else if (strncmp(buf, NLS_NVRAM_C2U, strlen(NLS_NVRAM_C2U))==0)
        {
                asusnls_c2u(tmpbuf);
        }
        else
        {
                strcpy(tmpbuf, "");
        }

        if (copy_to_user(buf, tmpbuf, strlen(tmpbuf)+1))
        {
                ret = -EFAULT;
                goto done;
        }
done:
        if(ret==0) return tmpbuf;
        else return NULL;
}

#endif  // NLS_XFR

static struct nvram_header *
BCMINITFN(nand_find_nvram)(hndnand_t *nfl, uint32 off)
{
	int blocksize = nfl->blocksize;
	unsigned char *buf = flash_nvh;
	int rlen = sizeof(flash_nvh);
	int len;

	for (; off < nfl_boot_size(nfl); off += blocksize) {
		if (hndnand_checkbadb(nfl, off) != 0)
			continue;

		len = blocksize;
		if (len >= rlen)
			len = rlen;

		if (hndnand_read(nfl, off, len, buf) == 0)
			break;

		buf += len;
		rlen -= len;
		if (rlen == 0)
			return (struct nvram_header *)flash_nvh;
	}

	return NULL;
}
#endif /* CONFIG_MTD_NFLASH */

static struct nvram_header *
BCMINITFN(sflash_find_nvram)(hndsflash_t *sfl, uint32 off)
{
	struct nvram_header header;
	unsigned char *buf = flash_nvh;
	uint32 rlen = sizeof(flash_nvh), hdrlen = sizeof(struct nvram_header);

	/* direct access to offset if within XIP region(16M) or NorthStar platform */
	if (off < SI_FLASH_WINDOW || sih->ccrev == 42) {
		return (struct nvram_header *)(sfl->base + off);
	}

	/* check read size boundary */
	if (off + rlen > sfl->size) {
		printk("Out of flash boundary off %x sfl->size %x\n", off, sfl->size);
		return NULL;
	}

	/* retrieve flash content if buffer have NVRAM_MAGIC header */
	if (sfl->read(sfl, off, hdrlen, (uint8 *)&header) == hdrlen &&
	    header.magic == NVRAM_MAGIC) {
		if (sfl->read(sfl, off, rlen, buf) == rlen)
			return (struct nvram_header *)buf;
	}

	return NULL;
}

/* Probe for NVRAM header */
static int
early_nvram_init(void)
{
	struct nvram_header *header;
	int i;
	u32 *src, *dst;
#ifdef CONFIG_MTD_NFLASH
	hndnand_t *nfl_info = NULL;
	uint32 blocksize;
#endif
	char *nvram_space_str;
	int bootdev;
	uint32 flash_base;
	uint32 lim = SI_FLASH_WINDOW;
	uint32 off;
	hndsflash_t *sfl_info;

	header = (struct nvram_header *)ram_nvram_buf;
	if (header->magic == NVRAM_MAGIC) {
		if (nvram_calc_crc(header) == (uint8)header->crc_ver_init) {
			nvram_inram = TRUE;
			goto found;
		}
	}

	bootdev = soc_boot_dev((void *)sih);
#ifdef CONFIG_MTD_NFLASH
	if (bootdev == SOC_BOOTDEV_NANDFLASH) {
		if ((nfl_info = hndnand_init(sih)) == NULL)
			return -1;
		flash_base = nfl_info->base;
		blocksize = nfl_info->blocksize;
		off = blocksize;
		for (; off < nfl_boot_size(nfl_info); off += blocksize) {
			if (hndnand_checkbadb(nfl_info, off) != 0)
				continue;
			header = (struct nvram_header *)(flash_base + off);
			if (header->magic != NVRAM_MAGIC)
				continue;

			/* Read into the nand_nvram */
			if ((header = nand_find_nvram(nfl_info, off)) == NULL)
				continue;
			if (nvram_calc_crc(header) == (uint8)header->crc_ver_init)
				goto found;
		}
	}
	else
#endif /* CONFIG_MTD_NFLASH */
	if (bootdev == SOC_BOOTDEV_SFLASH ||
	    bootdev == SOC_BOOTDEV_ROM) {
		/* Boot from SFLASH or ROM */
		if ((sfl_info = hndsflash_init(sih)) == NULL)
			return -1;

		lim = sfl_info->size;
		if (BCM53573_CHIP(sih->chip)) {
			norflash_region.start = sfl_info->phybase;
			norflash_region.end = norflash_region.start + sfl_info->size - 1;
		}
		BUG_ON(request_resource(&iomem_resource, &norflash_region));

		flash_base = sfl_info->base;

		BUG_ON(IS_ERR_OR_NULL((void *)flash_base));

		off = FLASH_MIN;
		while (off <= lim) {
			/* Windowed flash access */
			header = sflash_find_nvram(sfl_info, off - nvram_space);

			if (header && header->magic == NVRAM_MAGIC) {
				if (nvram_calc_crc(header) == (uint8)header->crc_ver_init) {
					goto found;
				}
			}
			off += DEF_NVRAM_SPACE;
		}
	}
	else {
		/* This is the case bootdev == SOC_BOOTDEV_PFLASH, not applied on NorthStar */
		ASSERT(0);
	}

	/* Try embedded NVRAM at 4 KB and 1 KB as last resorts */
	header = (struct nvram_header *)(flash_base + 4 KB);
	if (header->magic == NVRAM_MAGIC)
		if (nvram_calc_crc(header) == (uint8)header->crc_ver_init) {
			goto found;
		}

	header = (struct nvram_header *)(flash_base + 1 KB);
	if (header->magic == NVRAM_MAGIC)
		if (nvram_calc_crc(header) == (uint8)header->crc_ver_init) {
			goto found;
		}

	return -1;

found:
	src = (u32 *)header;
	dst = (u32 *)nvram_buf;
	for (i = 0; i < sizeof(struct nvram_header); i += 4)
		*dst++ = *src++;
	for (; i < header->len && i < MAX_NVRAM_SPACE; i += 4)
		*dst++ = ltoh32(*src++);

	nvram_space_str = early_nvram_get("nvram_space");
	if (nvram_space_str)
		nvram_space = bcm_strtoul(nvram_space_str, NULL, 0);

	return 0;
}

/* Early (before mm or mtd) read-only access to NVRAM */
static char *
early_nvram_get(const char *name)
{
	char *var, *value, *end, *eq;

	if (!name)
		return NULL;

	/* Too early? */
	if (sih == NULL)
		return NULL;

	if (!nvram_buf[0])
		if (early_nvram_init() != 0) {
			printk("early_nvram_get: Failed reading nvram var %s\n", name);
			return NULL;
		}

	/* Look for name=value and return value */
	var = &nvram_buf[sizeof(struct nvram_header)];
	end = nvram_buf + sizeof(nvram_buf) - 2;
	end[0] = end[1] = '\0';
	for (; *var; var = value + strlen(value) + 1) {
		if (!(eq = strchr(var, '=')))
			break;
		value = eq + 1;
		if ((eq - var) == strlen(name) && strncmp(var, name, (eq - var)) == 0)
			return value;
	}

	return NULL;
}

static int
early_nvram_getall(char *buf, int count)
{
	char *var, *end;
	int len = 0;

	/* Too early? */
	if (sih == NULL)
		return -1;

	if (!nvram_buf[0])
		if (early_nvram_init() != 0) {
			printk("early_nvram_getall: Failed reading nvram var\n");
			return -1;
		}

	bzero(buf, count);

	/* Write name=value\0 ... \0\0 */
	var = &nvram_buf[sizeof(struct nvram_header)];
	end = nvram_buf + sizeof(nvram_buf) - 2;
	end[0] = end[1] = '\0';
	for (; *var; var += strlen(var) + 1) {
		if ((count - len) <= (strlen(var) + 1))
			break;
		len += sprintf(buf + len, "%s", var) + 1;
	}

	return 0;
}
#endif /* !MODULE */

extern char * _nvram_get(const char *name);
extern int _nvram_set(const char *name, const char *value);
extern int _nvram_unset(const char *name);
extern int _nvram_getall(char *buf, int count);
extern int _nvram_commit(struct nvram_header *header);
extern int _nvram_init(si_t *sih);
extern void _nvram_exit(void);

/* Globals */
static spinlock_t nvram_lock = SPIN_LOCK_UNLOCKED;
static struct semaphore nvram_sem;
static unsigned long nvram_offset = 0;
static int nvram_major = -1;
static struct class *nvram_class = NULL;
static struct mtd_info *nvram_mtd = NULL;

int
_nvram_read(char *buf)
{
	struct nvram_header *header = (struct nvram_header *) buf;
	size_t len;
	int offset = 0;

	if (nvram_mtd) {
#ifdef CONFIG_MTD_NFLASH
		if (nvram_mtd->type == MTD_NANDFLASH)
			offset = 0;
		else
#endif
			offset = nvram_mtd->size - nvram_space;
	}
	if (nvram_inram || !nvram_mtd ||
	    nvram_mtd->read(nvram_mtd, offset, nvram_space, &len, buf) ||
	    len != nvram_space ||
	    header->magic != NVRAM_MAGIC) {
		/* Maybe we can recover some data from early initialization */
		if (nvram_inram)
			printk("Sourcing NVRAM from ram\n");
		memcpy(buf, nvram_buf, nvram_space);
	}

	return 0;
}

struct nvram_tuple *
_nvram_realloc(struct nvram_tuple *t, const char *name, const char *value)
{
	if ((nvram_offset + strlen(value) + 1) > nvram_space)
		return NULL;

	if (!t) {
		if (!(t = kmalloc(sizeof(struct nvram_tuple) + strlen(name) + 1, GFP_ATOMIC)))
			return NULL;

		/* Copy name */
		t->name = (char *) &t[1];
		strcpy(t->name, name);

		t->value = NULL;
	}

	/* Copy value */
	if (t->value == NULL || strlen(t->value) < strlen(value)) {
		/* Alloc value space */
		t->value = &nvram_buf[nvram_offset];
		strcpy(t->value, value);
		nvram_offset += strlen(value) + 1;
	} else if( 0 != strcmp(t->value, value)) {
		/* In place */
		strcpy(t->value, value);
	} 

	return t;
}

void
_nvram_free(struct nvram_tuple *t)
{
	if (!t) {
		nvram_offset = 0;
		memset( nvram_buf, 0, sizeof(nvram_buf) );
	} else {
		kfree(t);
	}
}

int
nvram_init(void *sih)
{
	return 0;
}

int
nvram_set(const char *name, const char *value)
{
	unsigned long flags;
	int ret;
	struct nvram_header *header;

	spin_lock_irqsave(&nvram_lock, flags);
	if ((ret = _nvram_set(name, value))) {
		printk( KERN_INFO "nvram: consolidating space!\n");
		/* Consolidate space and try again */
		if ((header = kmalloc(nvram_space, GFP_ATOMIC))) {
			if (_nvram_commit(header) == 0)
				ret = _nvram_set(name, value);
			kfree(header);
		}
	}
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

char *
real_nvram_get(const char *name)
{
	unsigned long flags;
	char *value;

	spin_lock_irqsave(&nvram_lock, flags);
	value = _nvram_get(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return value;
}

char *
nvram_get(const char *name)
{
	if (nvram_major >= 0)
		return real_nvram_get(name);
	else
		return early_nvram_get(name);
}

int
nvram_unset(const char *name)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_unset(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

static void
erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *) done->priv;
	wake_up(wait_q);
}

#ifdef CONFIG_MTD_NFLASH
int
nvram_nflash_commit(void)
{
	char *buf;
	size_t len;
	unsigned int i;
	int ret;
	struct nvram_header *header;
	unsigned long flags;
	u_int32_t offset;

	if (!(buf = kmalloc(nvram_space, GFP_KERNEL))) {
		printk(KERN_WARNING "nvram_commit: out of memory\n");
		return -ENOMEM;
	}

	down(&nvram_sem);

	offset = 0;
	header = (struct nvram_header *)buf;
	header->magic = NVRAM_MAGIC;
	/* reset MAGIC before we regenerate the NVRAM,
	 * otherwise we'll have an incorrect CRC
	 */
	/* Regenerate NVRAM */
	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_commit(header);
	spin_unlock_irqrestore(&nvram_lock, flags);
	if (ret)
		goto done;

	/* Write partition up to end of data area */
	i = header->len;
	ret = nvram_mtd->write(nvram_mtd, offset, i, &len, buf);
	if (ret || len != i) {
		printk(KERN_WARNING "nvram_commit: write error\n");
		ret = -EIO;
		goto done;
	}

done:
	up(&nvram_sem);
	kfree(buf);
	return ret;
}
#endif

int
nvram_commit(void)
{
	char *buf;
	size_t erasesize, len, magic_len;
	unsigned int i;
	int ret;
	struct nvram_header *header;
	unsigned long flags;
	u_int32_t offset;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	struct erase_info erase;
	u_int32_t magic_offset = 0; /* Offset for writing MAGIC # */

	if (!nvram_mtd) {
		printk(KERN_ERR "nvram_commit: NVRAM not found\n");
		return -ENODEV;
	}

	if (in_interrupt()) {
		printk(KERN_WARNING "nvram_commit: not committing in interrupt\n");
		return -EINVAL;
	}

#ifdef CONFIG_MTD_NFLASH
	if (nvram_mtd->type == MTD_NANDFLASH)
		return nvram_nflash_commit();
#endif
	/* Backup sector blocks to be erased */
	erasesize = ROUNDUP(nvram_space, nvram_mtd->erasesize);
	if (!(buf = kmalloc(erasesize, GFP_KERNEL))) {
		printk(KERN_WARNING "nvram_commit: out of memory\n");
		return -ENOMEM;
	}

	down(&nvram_sem);

	if ((i = erasesize - nvram_space) > 0) {
		offset = nvram_mtd->size - erasesize;
		len = 0;
		ret = nvram_mtd->read(nvram_mtd, offset, i, &len, buf);
		if (ret || len != i) {
			printk(KERN_ERR "nvram_commit: read error ret = %d, len = %d/%d\n", ret, len, i);
			ret = -EIO;
			goto done;
		}
		header = (struct nvram_header *)(buf + i);
		magic_offset = i + ((void *)&header->magic - (void *)header);
	} else {
		offset = nvram_mtd->size - nvram_space;
		header = (struct nvram_header *)buf;
		magic_offset = ((void *)&header->magic - (void *)header);
	}

	/* clear the existing magic # to mark the NVRAM as unusable 
	 * we can pull MAGIC bits low without erase
	 */
	header->magic = NVRAM_CLEAR_MAGIC; /* All zeros magic */
	/* Unlock sector blocks */
	if (nvram_mtd->unlock)
		nvram_mtd->unlock(nvram_mtd, offset, nvram_mtd->erasesize);
	ret = nvram_mtd->write(nvram_mtd, offset + magic_offset, sizeof(header->magic),
		&magic_len, (char *)&header->magic);
	if (ret || magic_len != sizeof(header->magic)) {
		printk(KERN_ERR "nvram_commit: clear MAGIC error\n");
		ret = -EIO;
		goto done;
	}

	header->magic = NVRAM_MAGIC;
	/* reset MAGIC before we regenerate the NVRAM,
	 * otherwise we'll have an incorrect CRC
	 */
	/* Regenerate NVRAM */
	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_commit(header);
	spin_unlock_irqrestore(&nvram_lock, flags);
	if (ret)
		goto done;

	/* Erase sector blocks */
	init_waitqueue_head(&wait_q);
	for (; offset < nvram_mtd->size - nvram_space + header->len;
		offset += nvram_mtd->erasesize) {

		erase.mtd = nvram_mtd;
		erase.addr = offset;
		erase.len = nvram_mtd->erasesize;
		erase.callback = erase_callback;
		erase.priv = (u_long) &wait_q;

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wait_q, &wait);

		/* Unlock sector blocks */
		if (nvram_mtd->unlock)
			nvram_mtd->unlock(nvram_mtd, offset, nvram_mtd->erasesize);

		if ((ret = nvram_mtd->erase(nvram_mtd, &erase))) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wait_q, &wait);
			printk(KERN_ERR "nvram_commit: erase error\n");
			goto done;
		}

		/* Wait for erase to finish */
		schedule();
		remove_wait_queue(&wait_q, &wait);
	}

	/* Write partition up to end of data area */
	header->magic = NVRAM_INVALID_MAGIC; /* All ones magic */
	offset = nvram_mtd->size - erasesize;
	i = erasesize - nvram_space + header->len;
	ret = nvram_mtd->write(nvram_mtd, offset, i, &len, buf);
	if (ret || len != i) {
		printk(KERN_ERR "nvram_commit: write error\n");
		ret = -EIO;
		goto done;
	}

	/* Now mark the NVRAM in flash as "valid" by setting the correct
	 * MAGIC #
	 */
	header->magic = NVRAM_MAGIC;
	ret = nvram_mtd->write(nvram_mtd, offset + magic_offset, sizeof(header->magic),
		&magic_len, (char *)&header->magic);
	if (ret || magic_len != sizeof(header->magic)) {
		printk(KERN_ERR "nvram_commit: write MAGIC error\n");
		ret = -EIO;
		goto done;
	}

	offset = nvram_mtd->size - erasesize;
	ret = nvram_mtd->read(nvram_mtd, offset, 4, &len, buf);

done:
	up(&nvram_sem);
	kfree(buf);
	return ret;
}

int
nvram_getall(char *buf, int count)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nvram_lock, flags);
	if (nvram_major >= 0)
		ret = _nvram_getall(buf, count);
	else
		ret = early_nvram_getall(buf, count);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

EXPORT_SYMBOL(nvram_init);
EXPORT_SYMBOL(nvram_get);
EXPORT_SYMBOL(nvram_getall);
EXPORT_SYMBOL(nvram_set);
EXPORT_SYMBOL(nvram_unset);
EXPORT_SYMBOL(nvram_commit);

/* User mode interface below */

static ssize_t
dev_nvram_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	char tmp[100], *name = tmp, *value;
	ssize_t ret;
	unsigned long off;

	if ((count+1) > sizeof(tmp)) {
		if (!(name = kmalloc(count+1, GFP_KERNEL)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}
	name[count] = '\0';

	if (*name == '\0') {
		/* Get all variables */
		ret = nvram_getall(name, count);
		if (ret == 0) {
			if (copy_to_user(buf, name, count)) {
				ret = -EFAULT;
				goto done;
			}
			ret = count;
		}
	} else {
		if (!(value = nvram_get(name))) {
			ret = 0;
			goto done;
		}

		/* Provide the offset into mmap() space */
		off = (unsigned long) value - (unsigned long) nvram_buf;

		if (copy_to_user(buf, &off, ret = sizeof(off))) {
			ret = -EFAULT;
			goto done;
		}
	}
#ifdef	_DEPRECATED
	flush_cache_all();
#endif
done:
	if (name != tmp)
		kfree(name);

	return ret;
}

static ssize_t
dev_nvram_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	char tmp[512], *name = tmp, *value;
	ssize_t ret;

	if ((count+1) > sizeof(tmp)) {
		if (!(name = kmalloc(count+1, GFP_KERNEL)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}
	name[count] = '\0';
	value = name;
	name = strsep(&value, "=");
	if (value)
		ret = nvram_set(name, value) ;
	else
		ret = nvram_unset(name) ;

	if( 0 == ret )
		ret = count;
done:
	if (name != tmp)
		kfree(name);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
static int
#else
static long
#endif
dev_nvram_ioctl(
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	struct inode *inode, 
#endif
	struct file *file, 
	unsigned int cmd, 
	unsigned long arg)
{
	if (cmd != NVRAM_MAGIC)
		return -EINVAL;
	return nvram_commit();
}

static int
dev_nvram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = __pa(nvram_buf) >> PAGE_SHIFT;

	if (remap_pfn_range(vma, vma->vm_start, offset,
	                    vma->vm_end - vma->vm_start,
	                    vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int
dev_nvram_open(struct inode *inode, struct file * file)
{
	return 0;
}

static int
dev_nvram_release(struct inode *inode, struct file * file)
{
	return 0;
}

static struct file_operations dev_nvram_fops = {
	owner:		THIS_MODULE,
	open:		dev_nvram_open,
	release:	dev_nvram_release,
	read:		dev_nvram_read,
	write:		dev_nvram_write,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	ioctl:		dev_nvram_ioctl,
#else
	unlocked_ioctl:	dev_nvram_ioctl,
#endif
	mmap:		dev_nvram_mmap
};

static void
dev_nvram_exit(void)
{
	int order = 0;
	struct page *page, *end;

	if (nvram_class) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
		class_device_destroy(nvram_class, MKDEV(nvram_major, 0));
#else /* 2.6.36 and up */
		device_destroy(nvram_class, MKDEV(nvram_major, 0));
#endif
		class_destroy(nvram_class);
	}

	if (nvram_major >= 0)
		unregister_chrdev(nvram_major, "nvram");

	if (nvram_mtd)
		put_mtd_device(nvram_mtd);

	while ((PAGE_SIZE << order) < MAX_NVRAM_SPACE)
		order++;
	end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
	for (page = virt_to_page(nvram_buf); page <= end; page++)
		ClearPageReserved(page);

	_nvram_exit();
}

static int
dev_nvram_init(void)
{
	int order = 0, ret = 0;
	struct page *page, *end;
	osl_t *osh;
#if defined(CONFIG_MTD) || defined(CONFIG_MTD_MODULE)
	unsigned int i;
#endif

	/* Allocate and reserve memory to mmap() */
	while ((PAGE_SIZE << order) < nvram_space)
		order++;
	end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
	for (page = virt_to_page(nvram_buf); page <= end; page++) {
		SetPageReserved(page);
	}

#if defined(CONFIG_MTD) || defined(CONFIG_MTD_MODULE)
	/* Find associated MTD device */
	for (i = 0; i < MAX_MTD_DEVICES; i++) {
		nvram_mtd = get_mtd_device(NULL, i);
		if (!IS_ERR(nvram_mtd)) {
			if (!strcmp(nvram_mtd->name, "nvram") &&
			    nvram_mtd->size >= nvram_space) {
				break;
			}
			put_mtd_device(nvram_mtd);
		}
	}
	if (i >= MAX_MTD_DEVICES)
		nvram_mtd = NULL;
#endif

	/* Initialize hash table lock */
	spin_lock_init(&nvram_lock);

	/* Initialize commit semaphore */
	init_MUTEX(&nvram_sem);

	/* Register char device */
	if ((nvram_major = register_chrdev(0, "nvram", &dev_nvram_fops)) < 0) {
		ret = nvram_major;
		goto err;
	}

	if (si_osh(sih) == NULL) {
		osh = osl_attach(NULL, SI_BUS, FALSE);
		if (osh == NULL) {
			printk("Error allocating osh\n");
			unregister_chrdev(nvram_major, "nvram");
			goto err;
		}
		si_setosh(sih, osh);
	}

	/* Initialize hash table */
	_nvram_init(sih);

	/* Create /dev/nvram handle */
	nvram_class = class_create(THIS_MODULE, "nvram");
	if (IS_ERR(nvram_class)) {
		printk("Error creating nvram class\n");
		goto err;
	}

	/* Add the device nvram0 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	class_device_create(nvram_class, NULL, MKDEV(nvram_major, 0), NULL, "nvram");
#else /* Linux 2.6.36 and above */
	device_create(nvram_class, NULL, MKDEV(nvram_major, 0), NULL, "nvram");
#endif	/* Linux 2.6.36 */

	return 0;

err:
	dev_nvram_exit();
	return ret;
}

/*
* This is not a module, and is not unloadable.
* Also, this module must not be initialized before
* the Flash MTD partitions have been set up, in case
* the contents are stored in Flash.
* Thus, late_initcall() macro is used to insert this
* device driver initialization later.
* An alternative solution would be to initialize
* inside the xx_open() call.
* -LR
*/
late_initcall(dev_nvram_init);
