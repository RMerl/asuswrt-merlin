/*
 * arch/m68k/atari/ataints.c -- Atari Linux interrupt handling code
 *
 * 5/2/94 Roman Hodek:
 *  Added support for TT interrupts; setup for TT SCU (may someone has
 *  twiddled there and we won't get the right interrupts :-()
 *
 *  Major change: The device-independent code in m68k/ints.c didn't know
 *  about non-autovec ints yet. It hardcoded the number of possible ints to
 *  7 (IRQ1...IRQ7). But the Atari has lots of non-autovec ints! I made the
 *  number of possible ints a constant defined in interrupt.h, which is
 *  47 for the Atari. So we can call request_irq() for all Atari interrupts
 *  just the normal way. Additionally, all vectors >= 48 are initialized to
 *  call trap() instead of inthandler(). This must be changed here, too.
 *
 * 1995-07-16 Lars Brinkhoff <f93labr@dd.chalmers.se>:
 *  Corrected a bug in atari_add_isr() which rejected all SCC
 *  interrupt sources if there were no TT MFP!
 *
 * 12/13/95: New interface functions atari_level_triggered_int() and
 *  atari_register_vme_int() as support for level triggered VME interrupts.
 *
 * 02/12/96: (Roman)
 *  Total rewrite of Atari interrupt handling, for new scheme see comments
 *  below.
 *
 * 1996-09-03 lars brinkhoff <f93labr@dd.chalmers.se>:
 *  Added new function atari_unregister_vme_int(), and
 *  modified atari_register_vme_int() as well as IS_VALID_INTNO()
 *  to work with it.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include <asm/system.h>
#include <asm/traps.h>

#include <asm/atarihw.h>
#include <asm/atariints.h>
#include <asm/atari_stdma.h>
#include <asm/irq.h>
#include <asm/entry.h>


/*
 * Atari interrupt handling scheme:
 * --------------------------------
 *
 * All interrupt source have an internal number (defined in
 * <asm/atariints.h>): Autovector interrupts are 1..7, then follow ST-MFP,
 * TT-MFP, SCC, and finally VME interrupts. Vector numbers for the latter can
 * be allocated by atari_register_vme_int().
 *
 * Each interrupt can be of three types:
 *
 *  - SLOW: The handler runs with all interrupts enabled, except the one it
 *    was called by (to avoid reentering). This should be the usual method.
 *    But it is currently possible only for MFP ints, since only the MFP
 *    offers an easy way to mask interrupts.
 *
 *  - FAST: The handler runs with all interrupts disabled. This should be used
 *    only for really fast handlers, that just do actions immediately
 *    necessary, and let the rest do a bottom half or task queue.
 *
 *  - PRIORITIZED: The handler can be interrupted by higher-level ints
 *    (greater IPL, no MFP priorities!). This is the method of choice for ints
 *    which should be slow, but are not from a MFP.
 *
 * The feature of more than one handler for one int source is still there, but
 * only applicable if all handers are of the same type. To not slow down
 * processing of ints with only one handler by the chaining feature, the list
 * calling function atari_call_irq_list() is only plugged in at the time the
 * second handler is registered.
 *
 * Implementation notes: For fast-as-possible int handling, there are separate
 * entry points for each type (slow/fast/prio). The assembler handler calls
 * the irq directly in the usual case, no C wrapper is involved. In case of
 * multiple handlers, atari_call_irq_list() is registered as handler and calls
 * in turn the real irq's. To ease access from assembler level to the irq
 * function pointer and accompanying data, these two are stored in a separate
 * array, irq_handler[]. The rest of data (type, name) are put into a second
 * array, irq_param, that is accessed from C only. For each slow interrupt (32
 * in all) there are separate handler functions, which makes it possible to
 * hard-code the MFP register address and value, are necessary to mask the
 * int. If there'd be only one generic function, lots of calculations would be
 * needed to determine MFP register and int mask from the vector number :-(
 *
 * Furthermore, slow ints may not lower the IPL below its previous value
 * (before the int happened). This is needed so that an int of class PRIO, on
 * that this int may be stacked, cannot be reentered. This feature is
 * implemented as follows: If the stack frame format is 1 (throwaway), the int
 * is not stacked, and the IPL is anded with 0xfbff, resulting in a new level
 * 2, which still blocks the HSYNC, but no interrupts of interest. If the
 * frame format is 0, the int is nested, and the old IPL value can be found in
 * the sr copy in the frame.
 */

#if 0

#define	NUM_INT_SOURCES	(8 + NUM_ATARI_SOURCES)

typedef void (*asm_irq_handler)(void);

struct irqhandler {
	irqreturn_t (*handler)(int, void *, struct pt_regs *);
	void	*dev_id;
};

struct irqparam {
	unsigned long	flags;
	const char	*devname;
};

/*
 * Array with irq's and their parameter data. This array is accessed from low
 * level assembler code, so an element size of 8 allows usage of index scaling
 * addressing mode.
 */
static struct irqhandler irq_handler[NUM_INT_SOURCES];

/*
 * This array hold the rest of parameters of int handlers: type
 * (slow,fast,prio) and the name of the handler. These values are only
 * accessed from C
 */
static struct irqparam irq_param[NUM_INT_SOURCES];

/* check for valid int number (complex, sigh...) */
#define	IS_VALID_INTNO(n)											\
	((n) > 0 &&														\
	 /* autovec and ST-MFP ok anyway */								\
	 (((n) < TTMFP_SOURCE_BASE) ||									\
	  /* TT-MFP ok if present */									\
	  ((n) >= TTMFP_SOURCE_BASE && (n) < SCC_SOURCE_BASE &&			\
	   ATARIHW_PRESENT(TT_MFP)) ||									\
	  /* SCC ok if present and number even */						\
	  ((n) >= SCC_SOURCE_BASE && (n) < VME_SOURCE_BASE &&			\
	   !((n) & 1) && ATARIHW_PRESENT(SCC)) ||						\
	  /* greater numbers ok if they are registered VME vectors */		\
	  ((n) >= VME_SOURCE_BASE && (n) < VME_SOURCE_BASE + VME_MAX_SOURCES && \
		  free_vme_vec_bitmap & (1 << ((n) - VME_SOURCE_BASE)))))


/*
 * Here start the assembler entry points for interrupts
 */

#define IRQ_NAME(nr) atari_slow_irq_##nr##_handler(void)

#define	BUILD_SLOW_IRQ(n)						   \
asmlinkage void IRQ_NAME(n);						   \
/* Dummy function to allow asm with operands.  */			   \
void atari_slow_irq_##n##_dummy (void) {				   \
__asm__ (__ALIGN_STR "\n"						   \
"atari_slow_irq_" #n "_handler:\t"					   \
"	addl	%6,%5\n"	/* preempt_count() += HARDIRQ_OFFSET */	   \
	SAVE_ALL_INT "\n"						   \
	GET_CURRENT(%%d0) "\n"						   \
"	andb	#~(1<<(%c3&7)),%a4:w\n"	/* mask this interrupt */	   \
	/* get old IPL from stack frame */				   \
"	bfextu	%%sp@(%c2){#5,#3},%%d0\n"				   \
"	movew	%%sr,%%d1\n"						   \
"	bfins	%%d0,%%d1{#21,#3}\n"					   \
"	movew	%%d1,%%sr\n"		/* set IPL = previous value */	   \
"	addql	#1,%a0\n"						   \
"	lea	%a1,%%a0\n"						   \
"	pea	%%sp@\n"		/* push addr of frame */	   \
"	movel	%%a0@(4),%%sp@-\n"	/* push handler data */		   \
"	pea	(%c3+8)\n"		/* push int number */		   \
"	movel	%%a0@,%%a0\n"						   \
"	jbsr	%%a0@\n"		/* call the handler */		   \
"	addql	#8,%%sp\n"						   \
"	addql	#4,%%sp\n"						   \
"	orw	#0x0600,%%sr\n"						   \
"	andw	#0xfeff,%%sr\n"		/* set IPL = 6 again */		   \
"	orb	#(1<<(%c3&7)),%a4:w\n"	/* now unmask the int again */	   \
"	jbra	ret_from_interrupt\n"					   \
	 : : "i" (&kstat_cpu(0).irqs[n+8]), "i" (&irq_handler[n+8]),	   \
	     "n" (PT_OFF_SR), "n" (n),					   \
	     "i" (n & 8 ? (n & 16 ? &tt_mfp.int_mk_a : &st_mfp.int_mk_a)   \
		        : (n & 16 ? &tt_mfp.int_mk_b : &st_mfp.int_mk_b)), \
	     "m" (preempt_count()), "di" (HARDIRQ_OFFSET)		   \
);									   \
	for (;;);			/* fake noreturn */		   \
}

BUILD_SLOW_IRQ(0);
BUILD_SLOW_IRQ(1);
BUILD_SLOW_IRQ(2);
BUILD_SLOW_IRQ(3);
BUILD_SLOW_IRQ(4);
BUILD_SLOW_IRQ(5);
BUILD_SLOW_IRQ(6);
BUILD_SLOW_IRQ(7);
BUILD_SLOW_IRQ(8);
BUILD_SLOW_IRQ(9);
BUILD_SLOW_IRQ(10);
BUILD_SLOW_IRQ(11);
BUILD_SLOW_IRQ(12);
BUILD_SLOW_IRQ(13);
BUILD_SLOW_IRQ(14);
BUILD_SLOW_IRQ(15);
BUILD_SLOW_IRQ(16);
BUILD_SLOW_IRQ(17);
BUILD_SLOW_IRQ(18);
BUILD_SLOW_IRQ(19);
BUILD_SLOW_IRQ(20);
BUILD_SLOW_IRQ(21);
BUILD_SLOW_IRQ(22);
BUILD_SLOW_IRQ(23);
BUILD_SLOW_IRQ(24);
BUILD_SLOW_IRQ(25);
BUILD_SLOW_IRQ(26);
BUILD_SLOW_IRQ(27);
BUILD_SLOW_IRQ(28);
BUILD_SLOW_IRQ(29);
BUILD_SLOW_IRQ(30);
BUILD_SLOW_IRQ(31);

asm_irq_handler slow_handlers[32] = {
	[0]	= atari_slow_irq_0_handler,
	[1]	= atari_slow_irq_1_handler,
	[2]	= atari_slow_irq_2_handler,
	[3]	= atari_slow_irq_3_handler,
	[4]	= atari_slow_irq_4_handler,
	[5]	= atari_slow_irq_5_handler,
	[6]	= atari_slow_irq_6_handler,
	[7]	= atari_slow_irq_7_handler,
	[8]	= atari_slow_irq_8_handler,
	[9]	= atari_slow_irq_9_handler,
	[10]	= atari_slow_irq_10_handler,
	[11]	= atari_slow_irq_11_handler,
	[12]	= atari_slow_irq_12_handler,
	[13]	= atari_slow_irq_13_handler,
	[14]	= atari_slow_irq_14_handler,
	[15]	= atari_slow_irq_15_handler,
	[16]	= atari_slow_irq_16_handler,
	[17]	= atari_slow_irq_17_handler,
	[18]	= atari_slow_irq_18_handler,
	[19]	= atari_slow_irq_19_handler,
	[20]	= atari_slow_irq_20_handler,
	[21]	= atari_slow_irq_21_handler,
	[22]	= atari_slow_irq_22_handler,
	[23]	= atari_slow_irq_23_handler,
	[24]	= atari_slow_irq_24_handler,
	[25]	= atari_slow_irq_25_handler,
	[26]	= atari_slow_irq_26_handler,
	[27]	= atari_slow_irq_27_handler,
	[28]	= atari_slow_irq_28_handler,
	[29]	= atari_slow_irq_29_handler,
	[30]	= atari_slow_irq_30_handler,
	[31]	= atari_slow_irq_31_handler
};

asmlinkage void atari_fast_irq_handler( void );
asmlinkage void atari_prio_irq_handler( void );

/* Dummy function to allow asm with operands.  */
void atari_fast_prio_irq_dummy (void) {
__asm__ (__ALIGN_STR "\n"
"atari_fast_irq_handler:\n\t"
	"orw	#0x700,%%sr\n"		/* disable all interrupts */
"atari_prio_irq_handler:\n\t"
	"addl	%3,%2\n\t"		/* preempt_count() += HARDIRQ_OFFSET */
	SAVE_ALL_INT "\n\t"
	GET_CURRENT(%%d0) "\n\t"
	/* get vector number from stack frame and convert to source */
	"bfextu	%%sp@(%c1){#4,#10},%%d0\n\t"
	"subw	#(0x40-8),%%d0\n\t"
	"jpl	1f\n\t"
	"addw	#(0x40-8-0x18),%%d0\n"
    "1:\tlea	%a0,%%a0\n\t"
	"addql	#1,%%a0@(%%d0:l:4)\n\t"
	"lea	irq_handler,%%a0\n\t"
	"lea	%%a0@(%%d0:l:8),%%a0\n\t"
	"pea	%%sp@\n\t"		/* push frame address */
	"movel	%%a0@(4),%%sp@-\n\t"	/* push handler data */
	"movel	%%d0,%%sp@-\n\t"	/* push int number */
	"movel	%%a0@,%%a0\n\t"
	"jsr	%%a0@\n\t"		/* and call the handler */
	"addql	#8,%%sp\n\t"
	"addql	#4,%%sp\n\t"
	"jbra	ret_from_interrupt"
	 : : "i" (&kstat_cpu(0).irqs), "n" (PT_OFF_FORMATVEC),
	     "m" (preempt_count()), "di" (HARDIRQ_OFFSET)
);
	for (;;);
}
#endif

/*
 * Bitmap for free interrupt vector numbers
 * (new vectors starting from 0x70 can be allocated by
 * atari_register_vme_int())
 */
static int free_vme_vec_bitmap;

/* GK:
 * HBL IRQ handler for Falcon. Nobody needs it :-)
 * ++andreas: raise ipl to disable further HBLANK interrupts.
 */
asmlinkage void falcon_hblhandler(void);
asm(".text\n"
__ALIGN_STR "\n\t"
"falcon_hblhandler:\n\t"
	"orw	#0x200,%sp@\n\t"	/* set saved ipl to 2 */
	"rte");

extern void atari_microwire_cmd(int cmd);

extern int atari_SCC_reset_done;

static int atari_startup_irq(unsigned int irq)
{
	m68k_irq_startup(irq);
	atari_turnon_irq(irq);
	atari_enable_irq(irq);
	return 0;
}

static void atari_shutdown_irq(unsigned int irq)
{
	atari_disable_irq(irq);
	atari_turnoff_irq(irq);
	m68k_irq_shutdown(irq);

	if (irq == IRQ_AUTO_4)
	    vectors[VEC_INT4] = falcon_hblhandler;
}

static struct irq_controller atari_irq_controller = {
	.name		= "atari",
	.lock		= __SPIN_LOCK_UNLOCKED(atari_irq_controller.lock),
	.startup	= atari_startup_irq,
	.shutdown	= atari_shutdown_irq,
	.enable		= atari_enable_irq,
	.disable	= atari_disable_irq,
};

/*
 * void atari_init_IRQ (void)
 *
 * Parameters:	None
 *
 * Returns:	Nothing
 *
 * This function should be called during kernel startup to initialize
 * the atari IRQ handling routines.
 */

void __init atari_init_IRQ(void)
{
	m68k_setup_user_interrupt(VEC_USER, NUM_ATARI_SOURCES - IRQ_USER, NULL);
	m68k_setup_irq_controller(&atari_irq_controller, 1, NUM_ATARI_SOURCES - 1);

	/* Initialize the MFP(s) */

#ifdef ATARI_USE_SOFTWARE_EOI
	st_mfp.vec_adr  = 0x48;	/* Software EOI-Mode */
#else
	st_mfp.vec_adr  = 0x40;	/* Automatic EOI-Mode */
#endif
	st_mfp.int_en_a = 0x00;	/* turn off MFP-Ints */
	st_mfp.int_en_b = 0x00;
	st_mfp.int_mk_a = 0xff;	/* no Masking */
	st_mfp.int_mk_b = 0xff;

	if (ATARIHW_PRESENT(TT_MFP)) {
#ifdef ATARI_USE_SOFTWARE_EOI
		tt_mfp.vec_adr  = 0x58;		/* Software EOI-Mode */
#else
		tt_mfp.vec_adr  = 0x50;		/* Automatic EOI-Mode */
#endif
		tt_mfp.int_en_a = 0x00;		/* turn off MFP-Ints */
		tt_mfp.int_en_b = 0x00;
		tt_mfp.int_mk_a = 0xff;		/* no Masking */
		tt_mfp.int_mk_b = 0xff;
	}

	if (ATARIHW_PRESENT(SCC) && !atari_SCC_reset_done) {
		atari_scc.cha_a_ctrl = 9;
		MFPDELAY();
		atari_scc.cha_a_ctrl = (char) 0xc0; /* hardware reset */
	}

	if (ATARIHW_PRESENT(SCU)) {
		/* init the SCU if present */
		tt_scu.sys_mask = 0x10;		/* enable VBL (for the cursor) and
									 * disable HSYNC interrupts (who
									 * needs them?)  MFP and SCC are
									 * enabled in VME mask
									 */
		tt_scu.vme_mask = 0x60;		/* enable MFP and SCC ints */
	} else {
		/* If no SCU and no Hades, the HSYNC interrupt needs to be
		 * disabled this way. (Else _inthandler in kernel/sys_call.S
		 * gets overruns)
		 */

		vectors[VEC_INT2] = falcon_hblhandler;
		vectors[VEC_INT4] = falcon_hblhandler;
	}

	if (ATARIHW_PRESENT(PCM_8BIT) && ATARIHW_PRESENT(MICROWIRE)) {
		/* Initialize the LM1992 Sound Controller to enable
		   the PSG sound.  This is misplaced here, it should
		   be in an atasound_init(), that doesn't exist yet. */
		atari_microwire_cmd(MW_LM1992_PSG_HIGH);
	}

	stdma_init();

	/* Initialize the PSG: all sounds off, both ports output */
	sound_ym.rd_data_reg_sel = 7;
	sound_ym.wd_data = 0xff;
}


/*
 * atari_register_vme_int() returns the number of a free interrupt vector for
 * hardware with a programmable int vector (probably a VME board).
 */

unsigned long atari_register_vme_int(void)
{
	int i;

	for (i = 0; i < 32; i++)
		if ((free_vme_vec_bitmap & (1 << i)) == 0)
			break;

	if (i == 16)
		return 0;

	free_vme_vec_bitmap |= 1 << i;
	return VME_SOURCE_BASE + i;
}
EXPORT_SYMBOL(atari_register_vme_int);


void atari_unregister_vme_int(unsigned long irq)
{
	if (irq >= VME_SOURCE_BASE && irq < VME_SOURCE_BASE + VME_MAX_SOURCES) {
		irq -= VME_SOURCE_BASE;
		free_vme_vec_bitmap &= ~(1 << irq);
	}
}
EXPORT_SYMBOL(atari_unregister_vme_int);


