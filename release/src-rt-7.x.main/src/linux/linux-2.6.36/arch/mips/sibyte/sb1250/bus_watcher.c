/*
 * Copyright (C) 2002,2003 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/*
 * The Bus Watcher monitors internal bus transactions and maintains
 * counts of transactions with error status, logging details and
 * causing one of several interrupts.  This driver provides a handler
 * for those interrupts which aggregates the counts (to avoid
 * saturating the 8-bit counters) and provides a presence in
 * /proc/bus_watcher if PROC_FS is on.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <asm/system.h>
#include <asm/io.h>

#include <asm/sibyte/sb1250.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>
#include <asm/sibyte/sb1250_scd.h>


struct bw_stats_struct {
	uint64_t status;
	uint32_t l2_err;
	uint32_t memio_err;
	int status_printed;
	unsigned long l2_cor_d;
	unsigned long l2_bad_d;
	unsigned long l2_cor_t;
	unsigned long l2_bad_t;
	unsigned long mem_cor_d;
	unsigned long mem_bad_d;
	unsigned long bus_error;
} bw_stats;


static void print_summary(uint32_t status, uint32_t l2_err,
			  uint32_t memio_err)
{
	printk("Bus watcher error counters: %08x %08x\n", l2_err, memio_err);
	printk("\nLast recorded signature:\n");
	printk("Request %02x from %d, answered by %d with Dcode %d\n",
	       (unsigned int)(G_SCD_BERR_TID(status) & 0x3f),
	       (int)(G_SCD_BERR_TID(status) >> 6),
	       (int)G_SCD_BERR_RID(status),
	       (int)G_SCD_BERR_DCODE(status));
}

/*
 * check_bus_watcher is exported for use in situations where we want
 * to see the most recent status of the bus watcher, which might have
 * already been destructively read out of the registers.
 *
 * notes: this is currently used by the cache error handler
 *        should provide locking against the interrupt handler
 */
void check_bus_watcher(void)
{
	u32 status, l2_err, memio_err;

#ifdef CONFIG_SB1_PASS_1_WORKAROUNDS
	/* Destructive read, clears register and interrupt */
	status = csr_in32(IOADDR(A_SCD_BUS_ERR_STATUS));
#else
	/* Use non-destructive register */
	status = csr_in32(IOADDR(A_SCD_BUS_ERR_STATUS_DEBUG));
#endif
	if (!(status & 0x7fffffff)) {
		printk("Using last values reaped by bus watcher driver\n");
		status = bw_stats.status;
		l2_err = bw_stats.l2_err;
		memio_err = bw_stats.memio_err;
	} else {
		l2_err = csr_in32(IOADDR(A_BUS_L2_ERRORS));
		memio_err = csr_in32(IOADDR(A_BUS_MEM_IO_ERRORS));
	}
	if (status & ~(1UL << 31))
		print_summary(status, l2_err, memio_err);
	else
		printk("Bus watcher indicates no error\n");
}

static int bw_print_buffer(char *page, struct bw_stats_struct *stats)
{
	int len;

	len = sprintf(page, "SiByte Bus Watcher statistics\n");
	len += sprintf(page+len, "-----------------------------\n");
	len += sprintf(page+len, "L2-d-cor %8ld\nL2-d-bad %8ld\n",
		       stats->l2_cor_d, stats->l2_bad_d);
	len += sprintf(page+len, "L2-t-cor %8ld\nL2-t-bad %8ld\n",
		       stats->l2_cor_t, stats->l2_bad_t);
	len += sprintf(page+len, "MC-d-cor %8ld\nMC-d-bad %8ld\n",
		       stats->mem_cor_d, stats->mem_bad_d);
	len += sprintf(page+len, "IO-err   %8ld\n", stats->bus_error);
	len += sprintf(page+len, "\nLast recorded signature:\n");
	len += sprintf(page+len, "Request %02x from %d, answered by %d with Dcode %d\n",
		       (unsigned int)(G_SCD_BERR_TID(stats->status) & 0x3f),
		       (int)(G_SCD_BERR_TID(stats->status) >> 6),
		       (int)G_SCD_BERR_RID(stats->status),
		       (int)G_SCD_BERR_DCODE(stats->status));
	/* XXXKW indicate multiple errors between printings, or stats
           collection (or both)? */
	if (stats->status & M_SCD_BERR_MULTERRS)
		len += sprintf(page+len, "Multiple errors observed since last check.\n");
	if (stats->status_printed) {
		len += sprintf(page+len, "(no change since last printing)\n");
	} else {
		stats->status_printed = 1;
	}

	return len;
}

#ifdef CONFIG_PROC_FS

/* For simplicity, I want to assume a single read is required each
   time */
static int bw_read_proc(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	int len;

	if (off == 0) {
		len = bw_print_buffer(page, data);
		*start = page;
	} else {
		len = 0;
		*eof = 1;
	}
	return len;
}

static void create_proc_decoder(struct bw_stats_struct *stats)
{
	struct proc_dir_entry *ent;

	ent = create_proc_read_entry("bus_watcher", S_IWUSR | S_IRUGO, NULL,
				     bw_read_proc, stats);
	if (!ent) {
		printk(KERN_INFO "Unable to initialize bus_watcher /proc entry\n");
		return;
	}
}

#endif /* CONFIG_PROC_FS */

/*
 * sibyte_bw_int - handle bus watcher interrupts and accumulate counts
 *
 * notes: possible re-entry due to multiple sources
 *        should check/indicate saturation
 */
static irqreturn_t sibyte_bw_int(int irq, void *data)
{
	struct bw_stats_struct *stats = data;
	unsigned long cntr;
#ifdef CONFIG_SIBYTE_BW_TRACE
	int i;
#endif
#ifndef CONFIG_PROC_FS
	char bw_buf[1024];
#endif

#ifdef CONFIG_SIBYTE_BW_TRACE
	csr_out32(M_SCD_TRACE_CFG_FREEZE, IOADDR(A_SCD_TRACE_CFG));
	csr_out32(M_SCD_TRACE_CFG_START_READ, IOADDR(A_SCD_TRACE_CFG));

	for (i=0; i<256*6; i++)
		printk("%016llx\n",
		       (long long)__raw_readq(IOADDR(A_SCD_TRACE_READ)));

	csr_out32(M_SCD_TRACE_CFG_RESET, IOADDR(A_SCD_TRACE_CFG));
	csr_out32(M_SCD_TRACE_CFG_START, IOADDR(A_SCD_TRACE_CFG));
#endif

	/* Destructive read, clears register and interrupt */
	stats->status = csr_in32(IOADDR(A_SCD_BUS_ERR_STATUS));
	stats->status_printed = 0;

	stats->l2_err = cntr = csr_in32(IOADDR(A_BUS_L2_ERRORS));
	stats->l2_cor_d += G_SCD_L2ECC_CORR_D(cntr);
	stats->l2_bad_d += G_SCD_L2ECC_BAD_D(cntr);
	stats->l2_cor_t += G_SCD_L2ECC_CORR_T(cntr);
	stats->l2_bad_t += G_SCD_L2ECC_BAD_T(cntr);
	csr_out32(0, IOADDR(A_BUS_L2_ERRORS));

	stats->memio_err = cntr = csr_in32(IOADDR(A_BUS_MEM_IO_ERRORS));
	stats->mem_cor_d += G_SCD_MEM_ECC_CORR(cntr);
	stats->mem_bad_d += G_SCD_MEM_ECC_BAD(cntr);
	stats->bus_error += G_SCD_MEM_BUSERR(cntr);
	csr_out32(0, IOADDR(A_BUS_MEM_IO_ERRORS));

#ifndef CONFIG_PROC_FS
	bw_print_buffer(bw_buf, stats);
	printk(bw_buf);
#endif

	return IRQ_HANDLED;
}

int __init sibyte_bus_watcher(void)
{
	memset(&bw_stats, 0, sizeof(struct bw_stats_struct));
	bw_stats.status_printed = 1;

	if (request_irq(K_INT_BAD_ECC, sibyte_bw_int, 0, "Bus watcher", &bw_stats)) {
		printk("Failed to register bus watcher BAD_ECC irq\n");
		return -1;
	}
	if (request_irq(K_INT_COR_ECC, sibyte_bw_int, 0, "Bus watcher", &bw_stats)) {
		free_irq(K_INT_BAD_ECC, &bw_stats);
		printk("Failed to register bus watcher COR_ECC irq\n");
		return -1;
	}
	if (request_irq(K_INT_IO_BUS, sibyte_bw_int, 0, "Bus watcher", &bw_stats)) {
		free_irq(K_INT_BAD_ECC, &bw_stats);
		free_irq(K_INT_COR_ECC, &bw_stats);
		printk("Failed to register bus watcher IO_BUS irq\n");
		return -1;
	}

#ifdef CONFIG_PROC_FS
	create_proc_decoder(&bw_stats);
#endif

#ifdef CONFIG_SIBYTE_BW_TRACE
	csr_out32((M_SCD_TRSEQ_ASAMPLE | M_SCD_TRSEQ_DSAMPLE |
		   K_SCD_TRSEQ_TRIGGER_ALL),
		  IOADDR(A_SCD_TRACE_SEQUENCE_0));
	csr_out32(M_SCD_TRACE_CFG_RESET, IOADDR(A_SCD_TRACE_CFG));
	csr_out32(M_SCD_TRACE_CFG_START, IOADDR(A_SCD_TRACE_CFG));
#endif

	return 0;
}

__initcall(sibyte_bus_watcher);
