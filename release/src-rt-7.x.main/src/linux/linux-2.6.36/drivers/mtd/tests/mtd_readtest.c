/*
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING. If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Check MTD device read.
 *
 * Author: Adrian Hunter <ext-adrian.hunter@nokia.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>
#include <linux/sched.h>

#define PRINT_PREF KERN_INFO "mtd_readtest: "

static int dev;
module_param(dev, int, S_IRUGO);
MODULE_PARM_DESC(dev, "MTD device number to use");

static struct mtd_info *mtd;
static unsigned char *iobuf;
static unsigned char *iobuf1;
static unsigned char *bbt;

static int pgsize;
static int ebcnt;
static int pgcnt;

static int read_eraseblock_by_page(int ebnum)
{
	size_t read = 0;
	int i, ret, err = 0;
	loff_t addr = ebnum * mtd->erasesize;
	void *buf = iobuf;
	void *oobbuf = iobuf1;

	for (i = 0; i < pgcnt; i++) {
		memset(buf, 0 , pgcnt);
		ret = mtd->read(mtd, addr, pgsize, &read, buf);
		if (ret == -EUCLEAN)
			ret = 0;
		if (ret || read != pgsize) {
			printk(PRINT_PREF "error: read failed at %#llx\n",
			       (long long)addr);
			if (!err)
				err = ret;
			if (!err)
				err = -EINVAL;
		}
		if (mtd->oobsize) {
			struct mtd_oob_ops ops;

			ops.mode      = MTD_OOB_PLACE;
			ops.len       = 0;
			ops.retlen    = 0;
			ops.ooblen    = mtd->oobsize;
			ops.oobretlen = 0;
			ops.ooboffs   = 0;
			ops.datbuf    = NULL;
			ops.oobbuf    = oobbuf;
			ret = mtd->read_oob(mtd, addr, &ops);
			if (ret || ops.oobretlen != mtd->oobsize) {
				printk(PRINT_PREF "error: read oob failed at "
						  "%#llx\n", (long long)addr);
				if (!err)
					err = ret;
				if (!err)
					err = -EINVAL;
			}
			oobbuf += mtd->oobsize;
		}
		addr += pgsize;
		buf += pgsize;
	}

	return err;
}

static void dump_eraseblock(int ebnum)
{
	int i, j, n;
	char line[128];
	int pg, oob;

	printk(PRINT_PREF "dumping eraseblock %d\n", ebnum);
	n = mtd->erasesize;
	for (i = 0; i < n;) {
		char *p = line;

		p += sprintf(p, "%05x: ", i);
		for (j = 0; j < 32 && i < n; j++, i++)
			p += sprintf(p, "%02x", (unsigned int)iobuf[i]);
		printk(KERN_CRIT "%s\n", line);
		cond_resched();
	}
	if (!mtd->oobsize)
		return;
	printk(PRINT_PREF "dumping oob from eraseblock %d\n", ebnum);
	n = mtd->oobsize;
	for (pg = 0, i = 0; pg < pgcnt; pg++)
		for (oob = 0; oob < n;) {
			char *p = line;

			p += sprintf(p, "%05x: ", i);
			for (j = 0; j < 32 && oob < n; j++, oob++, i++)
				p += sprintf(p, "%02x",
					     (unsigned int)iobuf1[i]);
			printk(KERN_CRIT "%s\n", line);
			cond_resched();
		}
}

static int is_block_bad(int ebnum)
{
	loff_t addr = ebnum * mtd->erasesize;
	int ret;

	ret = mtd->block_isbad(mtd, addr);
	if (ret)
		printk(PRINT_PREF "block %d is bad\n", ebnum);
	return ret;
}

static int scan_for_bad_eraseblocks(void)
{
	int i, bad = 0;

	bbt = kzalloc(ebcnt, GFP_KERNEL);
	if (!bbt) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		return -ENOMEM;
	}

	/* NOR flash does not implement block_isbad */
	if (mtd->block_isbad == NULL)
		return 0;

	printk(PRINT_PREF "scanning for bad eraseblocks\n");
	for (i = 0; i < ebcnt; ++i) {
		bbt[i] = is_block_bad(i) ? 1 : 0;
		if (bbt[i])
			bad += 1;
		cond_resched();
	}
	printk(PRINT_PREF "scanned %d eraseblocks, %d are bad\n", i, bad);
	return 0;
}

static int __init mtd_readtest_init(void)
{
	uint64_t tmp;
	int err, i;

	printk(KERN_INFO "\n");
	printk(KERN_INFO "=================================================\n");
	printk(PRINT_PREF "MTD device: %d\n", dev);

	mtd = get_mtd_device(NULL, dev);
	if (IS_ERR(mtd)) {
		err = PTR_ERR(mtd);
		printk(PRINT_PREF "error: Cannot get MTD device\n");
		return err;
	}

	if (mtd->writesize == 1) {
		printk(PRINT_PREF "not NAND flash, assume page size is 512 "
		       "bytes.\n");
		pgsize = 512;
	} else
		pgsize = mtd->writesize;

	tmp = mtd->size;
	do_div(tmp, mtd->erasesize);
	ebcnt = tmp;
	pgcnt = mtd->erasesize / pgsize;

	printk(PRINT_PREF "MTD device size %llu, eraseblock size %u, "
	       "page size %u, count of eraseblocks %u, pages per "
	       "eraseblock %u, OOB size %u\n",
	       (unsigned long long)mtd->size, mtd->erasesize,
	       pgsize, ebcnt, pgcnt, mtd->oobsize);

	err = -ENOMEM;
	iobuf = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!iobuf) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		goto out;
	}
	iobuf1 = kmalloc(mtd->erasesize, GFP_KERNEL);
	if (!iobuf1) {
		printk(PRINT_PREF "error: cannot allocate memory\n");
		goto out;
	}

	err = scan_for_bad_eraseblocks();
	if (err)
		goto out;

	/* Read all eraseblocks 1 page at a time */
	printk(PRINT_PREF "testing page read\n");
	for (i = 0; i < ebcnt; ++i) {
		int ret;

		if (bbt[i])
			continue;
		ret = read_eraseblock_by_page(i);
		if (ret) {
			dump_eraseblock(i);
			if (!err)
				err = ret;
		}
		cond_resched();
	}

	if (err)
		printk(PRINT_PREF "finished with errors\n");
	else
		printk(PRINT_PREF "finished\n");

out:

	kfree(iobuf);
	kfree(iobuf1);
	kfree(bbt);
	put_mtd_device(mtd);
	if (err)
		printk(PRINT_PREF "error %d occurred\n", err);
	printk(KERN_INFO "=================================================\n");
	return err;
}
module_init(mtd_readtest_init);

static void __exit mtd_readtest_exit(void)
{
	return;
}
module_exit(mtd_readtest_exit);

MODULE_DESCRIPTION("Read test module");
MODULE_AUTHOR("Adrian Hunter");
MODULE_LICENSE("GPL");
