/*
 * Initialization and support routines for self-booting compressed
 * image.
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
 * $Id: min_osl.c 367505 2012-11-08 08:33:26Z $
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

#elif defined(__ARM_ARCH_7A__)

static uint8 loader_pagetable_array[128*1024+16384];

typedef volatile struct scu_reg_struct_t {
	uint32 control;
	uint32 config;
	uint32 cpupwrstatus;
	uint32 invalidate;
	uint32 rsvd1[4];
	uint32 rsvd2[4];
	uint32 rsvd3[4];
	uint32 filtstart;
	uint32 filtend;
	uint32 rsvd4[2];
	uint32 sac;
	uint32 snsac;
} scu_reg_struct;

typedef volatile struct l2cc_reg_struct_t {
	uint32 cache_id;
	uint32 cache_type;
	uint32 rsvd1[62];
	uint32 control;	/* 0x100 */
	uint32 aux_control;
	uint32 tag_ram_control;
	uint32 data_ram_control;
	uint32 rsvd2[60];
	uint32 ev_counter_ctrl;	/* 0x200 */
	uint32 ev_counter1_cfg;
	uint32 ev_counter0_cfg;
	uint32 ev_counter1;
	uint32 ev_counter0;
	uint32 int_mask;
	uint32 int_mask_status;
	uint32 int_raw_status;
	uint32 int_clear;
	uint32 rsvd3[55];
	uint32 rsvd4[64]; /* 0x300 */
	uint32 rsvd5[64]; /* 0x400 */
	uint32 rsvd6[64]; /* 0x500 */
	uint32 rsvd7[64]; /* 0x600 */
	uint32 rsvd8[12]; /* 0x700 - 0x72F */
	uint32 cache_sync; /* 0x730 */
	uint32 rsvd9[15];
	uint32 inv_pa; /* 0x770 */
	uint32 rsvd10[2];
	uint32 inv_way; /* 0x77C */
	uint32 rsvd11[12];
	uint32 clean_pa; /* 0x7B0 */
	uint32 rsvd12[1];
	uint32 clean_index; /* 0x7B8 */
	uint32 clean_way;
	uint32 rsvd13[12];
	uint32 clean_inv_pa; /* 0x7F0 */
	uint32 rsvd14[1];
	uint32 clean_inv_index;
	uint32 clean_inv_way;
	uint32 rsvd15[64]; /* 0x800 - 0x8FF */
	uint32 d_lockdown0; /* 0x900 */
	uint32 i_lockdown0;
	uint32 d_lockdown1;
	uint32 i_lockdown1;
	uint32 d_lockdown2;
	uint32 i_lockdown2;
	uint32 d_lockdown3;
	uint32 i_lockdown3;
	uint32 d_lockdown4;
	uint32 i_lockdown4;
	uint32 d_lockdown5;
	uint32 i_lockdown5;
	uint32 d_lockdown6;
	uint32 i_lockdown6;
	uint32 d_lockdown7;
	uint32 i_lockdown7;
	uint32 rsvd16[4]; /* 0x940 */
	uint32 lock_line_en; /* 0x950 */
	uint32 unlock_way;
	uint32 rsvd17[42];
	uint32 rsvd18[64]; /* 0xA00 */
	uint32 rsvd19[64]; /* 0xB00 */
	uint32 addr_filtering_start; /* 0xC00 */
	uint32 addr_filtering_end;
	uint32 rsvd20[62];
	uint32 rsvd21[64]; /* 0xD00 */
	uint32 rsvd22[64]; /* 0xE00 */
	uint32 rsvd23[16]; /* 0xF00 - 0xF3F */
	uint32 debug_ctrl; /* 0xF40 */
	uint32 rsvd24[7];
	uint32 prefetch_ctrl; /* 0xF60 */
	uint32 rsvd25[7];
	uint32 power_ctrl; /* 0xF80 */
} l2cc_reg_struct;

/* ARM9 Private memory region */
#define IPROC_PERIPH_BASE		(0x19020000)	/* (IHOST_A9MP_scu_CONTROL) */
#define IPROC_PERIPH_SCU_REG_BASE	(IPROC_PERIPH_BASE)
#define IPROC_L2CC_REG_BASE		(IPROC_PERIPH_BASE + 0x2000) /* L2 Cache controller */

/* Structures and bit definitions */
/* SCU Control register */
#define IPROC_SCU_CTRL_SCU_EN		(0x00000001)
#define IPROC_SCU_CTRL_ADRFLT_EN	(0x00000002)
#define IPROC_SCU_CTRL_PARITY_EN	(0x00000004)
#define IPROC_SCU_CTRL_SPEC_LNFL_EN	(0x00000008)
#define IPROC_SCU_CTRL_FRC2P0_EN	(0x00000010)
#define IPROC_SCU_CTRL_SCU_STNDBY_EN	(0x00000020)
#define IPROC_SCU_CTRL_IC_STNDBY_EN	(0x00000040)

/*
 * CR1 bits (CP#15 CR1)
 */
#define CR_M	(1 << 0)	/* MMU enable				*/
#define CR_A	(1 << 1)	/* Alignment abort enable		*/
#define CR_C	(1 << 2)	/* Dcache enable			*/
#define CR_W	(1 << 3)	/* Write buffer enable			*/
#define CR_P	(1 << 4)	/* 32-bit exception handler		*/
#define CR_D	(1 << 5)	/* 32-bit data address range		*/
#define CR_L	(1 << 6)	/* Implementation defined		*/
#define CR_B	(1 << 7)	/* Big endian				*/
#define CR_S	(1 << 8)	/* System MMU protection		*/
#define CR_R	(1 << 9)	/* ROM MMU protection			*/
#define CR_F	(1 << 10)	/* Implementation defined		*/
#define CR_Z	(1 << 11)	/* Implementation defined		*/
#define CR_I	(1 << 12)	/* Icache enable			*/
#define CR_V	(1 << 13)	/* Vectors relocated to 0xffff0000	*/
#define CR_RR	(1 << 14)	/* Round Robin cache replacement	*/
#define CR_L4	(1 << 15)	/* LDR pc can set T bit			*/
#define CR_DT	(1 << 16)
#define CR_IT	(1 << 18)
#define CR_ST	(1 << 19)
#define CR_FI	(1 << 21)	/* Fast interrupt (lower latency mode)	*/
#define CR_U	(1 << 22)	/* Unaligned access operation		*/
#define CR_XP	(1 << 23)	/* Extended page tables			*/
#define CR_VE	(1 << 24)	/* Vectored interrupts			*/
#define CR_EE	(1 << 25)	/* Exception (Big) Endian		*/
#define CR_TRE	(1 << 28)	/* TEX remap enable			*/
#define CR_AFE	(1 << 29)	/* Access flag enable			*/
#define CR_TE	(1 << 30)	/* Thumb exception enable		*/

#define isb() __asm__ __volatile__ ("" : : : "memory")
#define nop() __asm__ __volatile__("mov\tr0,r0\t@ nop\n\t")

extern void cpu_flush_cache_all(void);
extern void cpu_inv_cache_all(void);

void flush_cache(unsigned long dummy1, unsigned long dummy2)
{
	cpu_flush_cache_all();
	return;
}

static void l2cc_init(void)
{
	uint32 regval;
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;

	regval = l2cc->aux_control;
	regval &= ~(0x000F0000); /* Clear the Way-size and associativity (8 way) */
	regval |= 0x0A130000;    /* Non-secure interrupt access, Way-size 16KB,
				    16 way and event monitoring
				  */
	l2cc->aux_control = regval;
	l2cc->tag_ram_control = 0; /* Tag ram latency */
	l2cc->data_ram_control = 0; /* Data ram latency */
}

static void l2cc_invalidate(void)
{
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;

	/* Invalidate the entire L2 cache */
	l2cc->inv_way = 0x0000FFFF;
}

int l2cc_enable(void)
{
	int i;
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;

	l2cc_init();
	l2cc_invalidate();

	i = 1000;
	while (l2cc->inv_way && i)
	{
		--i;
	};

	if (i == 0)
		return (-1);

	/* Clear any pending interrupts from this controller */
	l2cc->int_clear = 0x1FF;

	/* Enable the L2 */
	l2cc->control = 0x01;

	/* mem barrier to sync up things */
	i = 0;
	asm("mcr p15, 0, %0, c7, c10, 4": :"r"(i));

	return 0;
}

static void cp_delay(void)
{
	volatile int i;

	/* copro seems to need some delay between reading and writing */
	for (i = 0; i < 1000; i++)
		nop();
	asm volatile("" : : : "memory");
}

void
caches_on(void)
{
	int i;
	uint32 val, *ptb, ptbaddr;

	cpu_inv_cache_all();

	/* Enable I$ */
	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	cp_delay();
	val |= CR_I;
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();

	/* prepare page table for MMU */
	ptbaddr = (uint32)loader_pagetable_array;
	/* Round down to next 64 kB limit */
	ptbaddr += 0x10000;
	ptbaddr &= ~(0x10000 - 1);
	ptb = (uint32 *)ptbaddr;

	/* Set up an identity-mapping for all 4GB, rw for everyone */
	for (i = 0; i < 128; i++) {
		/* DRAM area: TEX = 0x4, Ap = 3, Domain = 0, C =1, B = 0 */
		ptb[i] = i << 20 | 0x4c0e;
	}

	for (i = 128; i < 480; i++) {
		/* TEX = 0x2(device memory), Ap = 3, Domain = 0, C =0, B = 0 */
		ptb[i] = i << 20 | 0x0c02;
	}

	for (i = 480; i < 512; i++) {
		/* SPI region: TEX = 0x4, Ap = 3, Domain = 0, C =1, B = 0 */
		ptb[i] = i << 20 | 0x4c0a;
	}

	for (i = 512; i < 4096; i++) {
		/* TEX = 0x2(device memory), Ap = 3, Domain = 0, C =0, B = 0 */
		ptb[i] = i << 20 | 0x2c02;
	}

	/* Apply page table address to CP15 */
	asm volatile("mcr p15, 0, %0, c2, c0, 0" : : "r" (ptb) : "memory");
	/* Set the access control to all-supervisor */
	asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r" (~0));

	/* Enable I$ and MMU */
	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	cp_delay();
	val |= (CR_C | CR_M);
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();
}

void
blast_dcache(void)
{
#ifndef CFG_UNCACHED
	uint32 val;

	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	cp_delay();

	if ((val & CR_C) != CR_C)
		return; /* D$ not enabled */

	flush_cache(0, ~0);

#ifdef CFG_SHMOO
	val &= ~CR_C;
#else
	val &= ~(CR_C | CR_M);
#endif
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();
#endif /* !CFG_UNCACHED */
}

void
blast_icache(void)
{
#ifndef CFG_UNCACHED
	uint32 val;

	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	cp_delay();

	if ((val & CR_I) != CR_I)
		return; /* I$ not enabled */

	val &= ~CR_I;
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();

	/* invalidate I-cache */
	asm("mcr p15, 0, %0, c7, c5, 0": :"r" (0));
#endif
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
#ifdef __ARM_ARCH_7A__
extern long _getticks(void);
#define get_arm_cyclecount	(uint32)_getticks
#endif
#endif /* mips */

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
#if defined(CFG_SIM) && defined(__ARM_ARCH_7A__)
	quot = (min_uart.baud_base + 300) / 600;
#else
	quot = (min_uart.baud_base + 57600) / 115200;
#endif
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
#if defined(CFG_SHMOO)
	{
	extern int _memsize;
	if (_memsize) {
		free_mem_ptr = _memsize >> 1;
		free_mem_ptr_end = _memsize - (_memsize >> 2);
	}
	}
#else
	free_mem_ptr = (ulong) bss_end;
	free_mem_ptr_end = ((ulong)&sih) - 8192;	/* Enough stack? */
#endif /* CFG_SHMOO */
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
