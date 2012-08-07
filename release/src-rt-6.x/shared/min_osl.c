/*
 * Initialization and support routines for self-booting compressed
 * image.
 *
 * Copyright (C) 2011, Broadcom Corporation. All Rights Reserved.
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
 * $Id: min_osl.c 300516 2011-12-04 17:39:44Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmdevs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <hndcpu.h>
#include <sbchipc.h>
#include <hndchipc.h>

/* Global ASSERT type */
uint32 g_assert_type = 0;

#ifdef	mips
/* Cache support */

/* Cache and line sizes */
uint __icache_size, __ic_lsize, __dcache_size, __dc_lsize;

static void
_change_cachability(uint32 cm)
{
	uint32 prid, c0reg;

	c0reg = MFC0(C0_CONFIG, 0);
	c0reg &= ~CONF_CM_CMASK;
	c0reg |= (cm & CONF_CM_CMASK);
	MTC0(C0_CONFIG, 0, c0reg);
	prid = MFC0(C0_PRID, 0);
	if (BCM330X(prid)) {
		c0reg = MFC0(C0_BROADCOM, 0);
		/* Enable icache & dcache */
		c0reg |= BRCM_IC_ENABLE | BRCM_DC_ENABLE;
		MTC0(C0_BROADCOM, 0, c0reg);
	}
}
static void (*change_cachability)(uint32);

void
caches_on(void)
{
	uint32 config, config1, r2, tmp;
	uint start, end, size, lsize;

	config = MFC0(C0_CONFIG, 0);
	r2 = config & CONF_AR;
	config1 = MFC0(C0_CONFIG, 1);

	icache_probe(config1, &size, &lsize);
	__icache_size = size;
	__ic_lsize = lsize;

	dcache_probe(config1, &size, &lsize);
	__dcache_size = size;
	__dc_lsize = lsize;

	/* If caches are not in the default state then
	 * presume that caches are already init'd
	 */
	if ((config & CONF_CM_CMASK) != CONF_CM_UNCACHED) {
		blast_dcache();
		blast_icache();
		return;
	}

	tmp = R_REG(NULL, (uint32 *)(OSL_UNCACHED(SI_ENUM_BASE + CC_CHIPID)));
	if (((tmp & CID_PKG_MASK) >> CID_PKG_SHIFT) != HDLSIM_PKG_ID) {
		/* init icache */
		start = KSEG0ADDR(caches_on) & 0xff800000;
		end = (start + __icache_size);
		MTC0(C0_TAGLO, 0, 0);
		MTC0(C0_TAGHI, 0, 0);
		while (start < end) {
			cache_op(start, Index_Store_Tag_I);
			start += __ic_lsize;
		}

		/* init dcache */
		start = KSEG0ADDR(caches_on) & 0xff800000;
		end = (start + __dcache_size);
		if (r2) {
			/* mips32r2 has the data tags in select 2 */
			MTC0(C0_TAGLO, 2, 0);
			MTC0(C0_TAGHI, 2, 0);
		} else {
			MTC0(C0_TAGLO, 0, 0);
			MTC0(C0_TAGHI, 0, 0);
		}
		while (start < end) {
			cache_op(start, Index_Store_Tag_D);
			start += __dc_lsize;
		}
	}

	/* Must be in KSEG1 to change cachability */
	change_cachability = (void (*)(uint32))KSEG1ADDR(_change_cachability);
	change_cachability(CONF_CM_CACHABLE_NONCOHERENT);
}


void
blast_dcache(void)
{
	uint32 start, end;

	start = KSEG0ADDR(blast_dcache) & 0xff800000;
	end = start + __dcache_size;

	while (start < end) {
		cache_op(start, Index_Writeback_Inv_D);
		start += __dc_lsize;
	}
}

void
blast_icache(void)
{
	uint32 start, end;

	start = KSEG0ADDR(blast_icache) & 0xff800000;
	end = start + __icache_size;

	while (start < end) {
		cache_op(start, Index_Invalidate_I);
		start += __ic_lsize;
	}
}
#endif	/* mips */

/* uart output */

struct serial_struct {
	unsigned char	*reg_base;
	unsigned short	reg_shift;
	int	irq;
	int	baud_base;
};

static struct serial_struct min_uart;

#ifdef	BCMDBG
#define LOG_BUF_LEN	(16 * 1024)
#else
#define LOG_BUF_LEN	(1024)
#endif
#define LOG_BUF_MASK	(LOG_BUF_LEN-1)
static unsigned long log_idx;
static char log_buf[LOG_BUF_LEN];


static inline int
serial_in(struct serial_struct *info, int offset)
{
	return ((int)R_REG(NULL, (uint8 *)(info->reg_base + (offset << info->reg_shift))));
}

static inline void
serial_out(struct serial_struct *info, int offset, int value)
{
	W_REG(NULL, (uint8 *)(info->reg_base + (offset << info->reg_shift)), value);
}

void
putc(int c)
{
	uint32 idx;

	/* CR before LF */
	if (c == '\n')
		putc('\r');

	/* Store in log buffer */
	idx = *((uint32 *)OSL_UNCACHED((uintptr)&log_idx));
	*((char *)OSL_UNCACHED(&log_buf[idx])) = (char)c;
	*((uint32 *)OSL_UNCACHED((uintptr)&log_idx)) = (idx + 1) & LOG_BUF_MASK;

	/* No UART */
	if (!min_uart.reg_base)
		return;

	while (!(serial_in(&min_uart, UART_LSR) & UART_LSR_THRE));
	serial_out(&min_uart, UART_TX, c);
}

/* assert & debugging */


/* general purpose memory allocation */

extern char text_start[], text_end[];
extern char data_start[], data_end[];
extern char bss_start[], bss_end[];

static ulong free_mem_ptr = 0;
static ulong free_mem_ptr_end = 0;

#define	MIN_ALIGN	4	/* Alignment at 4 bytes */
#define	MAX_ALIGN	4096	/* Max alignment at 4k */

void *
malloc(uint size)
{
	return malloc_align(size, MIN_ALIGN);
}

void *
malloc_align(uint size, uint align_bits)
{
	void *p;
	uint align_mask;

	/* Sanity check */
	if (size < 0)
		printf("Malloc error\n");
	if (free_mem_ptr == 0)
		printf("Memory error\n");

	/* Align */
	align_mask = 1 << align_bits;
	if (align_mask < MIN_ALIGN)
		align_mask = MIN_ALIGN;
	if (align_mask > MAX_ALIGN)
		align_mask = MAX_ALIGN;
	align_mask--;
	free_mem_ptr = (free_mem_ptr + align_mask) & ~align_mask;

	p = (void *) free_mem_ptr;
	free_mem_ptr += size;

	if (free_mem_ptr >= free_mem_ptr_end)
		printf("Out of memory\n");

	return p;
}

int
free(void *where)
{
	return 0;
}

/* get processor cycle count */

#if defined(mips)
#define	get_cycle_count	get_c0_count
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#define	get_cycle_count	get_arm_cyclecount
#endif

uint32
osl_getcycles(void)
{
	return get_cycle_count();
}

/* microsecond delay */

/* Default to 125 MHz */
static uint32 cpu_clock = 125000000;
static uint32 c0counts_per_us = 125000000 / 2000000;
static uint32 c0counts_per_ms = 125000000 / 2000;

void
udelay(uint32 us)
{
	uint32 curr, lim;

	curr = get_cycle_count();
	lim = curr + (us * c0counts_per_us);

	if (lim < curr)
		while (get_cycle_count() > curr)
			;

	while (get_cycle_count() < lim)
		;
}

#ifndef	MIN_DO_TRAP

/* No trap handling in self-decompressing boots */
extern void trap_init(void);

void
trap_init(void)
{
}

#endif	/* !MIN_DO_TRAP */

static void
serial_add(void *regs, uint irq, uint baud_base, uint reg_shift)
{
	int quot;

	if (min_uart.reg_base)
		return;

	min_uart.reg_base = regs;
	min_uart.irq = irq;
	min_uart.baud_base = baud_base / 16;
	min_uart.reg_shift = reg_shift;

	/* Set baud and 8N1 */
	quot = (min_uart.baud_base + 57600) / 115200;
	serial_out(&min_uart, UART_LCR, UART_LCR_DLAB);
	serial_out(&min_uart, UART_DLL, quot & 0xff);
	serial_out(&min_uart, UART_DLM, quot >> 8);
	serial_out(&min_uart, UART_LCR, UART_LCR_WLEN8);

	/* According to the Synopsys website: "the serial clock
	 * modules must have time to see new register values
	 * and reset their respective state machines. This
	 * total time is guaranteed to be no more than
	 * (2 * baud divisor * 16) clock cycles of the slower
	 * of the two system clocks. No data should be transmitted
	 * or received before this maximum time expires."
	 */
	udelay(1000);
}


void *
osl_init()
{
	uint32 c0counts_per_cycle;
	si_t *sih;

	/* Scan backplane */
	sih = si_kattach(SI_OSH);

	if (sih == NULL)
		return NULL;

#if defined(mips)
	si_mips_init(sih, 0);
	c0counts_per_cycle = 2;
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
	si_arm_init(sih);
	c0counts_per_cycle = 1;
#else
#error "Unknow CPU"
#endif
	cpu_clock = si_cpu_clock(sih);
	c0counts_per_us = cpu_clock / (1000000 * c0counts_per_cycle);
	c0counts_per_ms = si_cpu_clock(sih) / (1000 * c0counts_per_cycle);

	/* Don't really need to talk to the uart in simulation */
	if ((sih->chippkg != HDLSIM_PKG_ID) && (sih->chippkg != HWSIM_PKG_ID))
		si_serial_init(sih, serial_add);

	/* Init malloc */
	free_mem_ptr = (ulong) bss_end;
	free_mem_ptr_end = ((ulong)&sih) - 8192;	/* Enough stack? */

	return ((void *)sih);
}

/* translate bcmerros */
int
osl_error(int bcmerror)
{
	if (bcmerror)
		return -1;
	else
		return 0;
}
