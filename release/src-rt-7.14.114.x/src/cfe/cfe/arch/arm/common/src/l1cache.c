#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_arena.h"

#include "cfe_error.h"

#include "cfe.h"
#include "cfe_mem.h"

#include "bsp_config.h"
#include "initdata.h"

#define _NOPROTOS_
#include "cfe_boot.h"
#undef _NOPROTOS_

#include "cpu_config.h"

/* Temp soloution, Simon */
static uint8_t cfe_pagetable_array[128*1024+16384];

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

extern void cpu_inv_cache_all(void);
extern void cpu_flush_cache_all(void);

static void
cp_delay (void)
{
	volatile int i;

	/* copro seems to need some delay between reading and writing */
	for (i = 0; i < 1000; i++)
		nop();
	asm volatile("" : : : "memory");
}

/* CFE */
void
l1cache_on(void)
{
	int i;
	uint32_t val, *ptb, ptbaddr;

	/* Inv cache all */
	cpu_inv_cache_all();

	/* Enable I$ */
	asm("mrc p15, 0, %0, c1, c0, 0	@ get CR" : "=r" (val) : : "cc");
	cp_delay();
	val |= CR_I;
	asm volatile("mcr p15, 0, %0, c1, c0, 0	@ set CR" : : "r" (val) : "cc");
	isb();

	/* prepare page table for MMU */
	ptbaddr = (uint32_t)cfe_pagetable_array;
	/* Round down to next 64 kB limit */
	ptbaddr += 0x10000;
	ptbaddr &= ~(0x10000 - 1);
	ptb = (uint32_t *)ptbaddr;

	/* Set up an identity-mapping for all 4GB, rw for everyone */
	for (i = 0; i < 128; i++) { 
		/* DRAM area: TEX = 0x4, Ap = 3, Domain = 0, C =1, B = 0 */
//		ptb[i] = i << 20 | 0x4c0a; //WT
		ptb[i] = i << 20 | 0x4c0e; //WB
	}

	for (i = 128; i < 480; i++) {
		/* TEX = 0x2(device memory), Ap = 3, Domain = 0, C =0, B = 0 */
//		ptb[i] = i << 20 | 0x2c02;
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
