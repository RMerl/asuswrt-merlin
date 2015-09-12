#include "lib_types.h"
#include "lib_string.h"
#include "lib_queue.h"
#include "lib_malloc.h"
#include "lib_printf.h"
#include "lib_arena.h"

#include "cfe_error.h"

#include "cfe.h"
#include "cfe_mem.h"

#include "initdata.h"

#define _NOPROTOS_
#include "cfe_boot.h"
#undef _NOPROTOS_

#include "cpu_config.h"

typedef volatile struct scu_reg_struct_t {
	uint32_t control;
	uint32_t config;
	uint32_t cpupwrstatus;
	uint32_t invalidate;
	uint32_t rsvd1[4];
	uint32_t rsvd2[4];
	uint32_t rsvd3[4];
	uint32_t filtstart;
	uint32_t filtend;
	uint32_t rsvd4[2];
	uint32_t sac;
	uint32_t snsac;
} scu_reg_struct;

typedef volatile struct l2cc_reg_struct_t {	
	uint32_t cache_id;	
	uint32_t cache_type;
	uint32_t rsvd1[62];
	uint32_t control;	/* 0x100 */
	uint32_t aux_control;
	uint32_t tag_ram_control;
	uint32_t data_ram_control;
	uint32_t rsvd2[60];
	uint32_t ev_counter_ctrl;	/* 0x200 */
	uint32_t ev_counter1_cfg;
	uint32_t ev_counter0_cfg;
	uint32_t ev_counter1;
	uint32_t ev_counter0;
	uint32_t int_mask;
	uint32_t int_mask_status;
	uint32_t int_raw_status;
	uint32_t int_clear;
	uint32_t rsvd3[55];
	uint32_t rsvd4[64]; /* 0x300 */
	uint32_t rsvd5[64]; /* 0x400 */
	uint32_t rsvd6[64]; /* 0x500 */
	uint32_t rsvd7[64]; /* 0x600 */
	uint32_t rsvd8[12]; /* 0x700 - 0x72F */
	uint32_t cache_sync; /* 0x730 */
	uint32_t rsvd9[15];
	uint32_t inv_pa; /* 0x770 */
	uint32_t rsvd10[2];
	uint32_t inv_way; /* 0x77C */
	uint32_t rsvd11[12]; 
	uint32_t clean_pa; /* 0x7B0 */
	uint32_t rsvd12[1];
	uint32_t clean_index; /* 0x7B8 */
	uint32_t clean_way;
	uint32_t rsvd13[12]; 
	uint32_t clean_inv_pa; /* 0x7F0 */
	uint32_t rsvd14[1];
	uint32_t clean_inv_index;
	uint32_t clean_inv_way;
	uint32_t rsvd15[64]; /* 0x800 - 0x8FF*/
	uint32_t d_lockdown0; /* 0x900 */
	uint32_t i_lockdown0;
	uint32_t d_lockdown1;
	uint32_t i_lockdown1;
	uint32_t d_lockdown2;
	uint32_t i_lockdown2;
	uint32_t d_lockdown3;
	uint32_t i_lockdown3;
	uint32_t d_lockdown4;
	uint32_t i_lockdown4;
	uint32_t d_lockdown5;
	uint32_t i_lockdown5;
	uint32_t d_lockdown6;
	uint32_t i_lockdown6;
	uint32_t d_lockdown7;
	uint32_t i_lockdown7;
	uint32_t rsvd16[4]; /* 0x940 */
	uint32_t lock_line_en; /* 0x950 */
	uint32_t unlock_way;
	uint32_t rsvd17[42];
	uint32_t rsvd18[64]; /* 0xA00 */
	uint32_t rsvd19[64]; /* 0xB00 */
	uint32_t addr_filtering_start; /* 0xC00 */
	uint32_t addr_filtering_end;
	uint32_t rsvd20[62];
	uint32_t rsvd21[64]; /* 0xD00 */
	uint32_t rsvd22[64]; /* 0xE00 */
	uint32_t rsvd23[16]; /* 0xF00 - 0xF3F */
	uint32_t debug_ctrl; /* 0xF40 */
	uint32_t rsvd24[7];
	uint32_t prefetch_ctrl; /* 0xF60 */
	uint32_t rsvd25[7];
	uint32_t power_ctrl; /* 0xF80 */
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

void
scu_disable(void) {
	scu_reg_struct *scu = (scu_reg_struct *)IPROC_PERIPH_SCU_REG_BASE;

	scu->control &= ~IPROC_SCU_CTRL_SCU_EN;
}

void
armv7_scu_on(void) {
	scu_reg_struct *scu = (scu_reg_struct *)IPROC_PERIPH_SCU_REG_BASE;

	scu->invalidate = 0xFF;
	
	/* Turn on the SCU */
	scu->control |= IPROC_SCU_CTRL_SCU_EN;

}

static void
l2cache_init(void) 
{
	uint32_t regval;
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;
	regval = l2cc->aux_control;
	regval &= ~(0x000F0000); // Clear the Way-size and associativity (8 way)
	regval |= 0x0A130000; /* Non-secure interrupt access, Way-size 16KB, 16 way
						    and event monitoring */
	l2cc->aux_control = regval;
	l2cc->tag_ram_control = 0; /* Tag ram latency */
	l2cc->data_ram_control = 0; /* Data ram latency */
}

static void
l2cache_invalidate(void) 
{
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;
	
	/* Invalidate the entire L2 cache */
	l2cc->inv_way = 0x0000FFFF;
}

int
l2cache_on(void)
{
	int i;
	l2cc_reg_struct *l2cc = (l2cc_reg_struct *)IPROC_L2CC_REG_BASE;

	l2cache_init();
	l2cache_invalidate();

	i = 1000;
	while(l2cc->inv_way && i)
	{
		--i;
	};

	if(i == 0)
		return (-1);

	/* Clear any pending interrupts from this controller */
	l2cc->int_clear = 0x1FF;

	/* Enable the L2 */
	l2cc->control = 0x01;

	/* mem barrier to sync up things */
	i = 0;
	asm("mcr p15, 0, %0, c7, c10, 4": :"r"(i));
/* These instructions work with ARMV7 arch only
	//asm("isb sy"); 
	//asm("dsb sy");
*/
	return 0;
}
