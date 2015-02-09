/*
 *  arch/m68k/bvme6000/config.c
 *
 *  Copyright (C) 1997 Richard Hirst [richard@sleepie.demon.co.uk]
 *
 * Based on:
 *
 *  linux/amiga/config.c
 *
 *  Copyright (C) 1993 Hamish Macdonald
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file README.legal in the main directory of this archive
 * for more details.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/console.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/major.h>
#include <linux/genhd.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/bcd.h>

#include <asm/bootinfo.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/rtc.h>
#include <asm/machdep.h>
#include <asm/bvme6000hw.h>

static void bvme6000_get_model(char *model);
extern void bvme6000_sched_init(irq_handler_t handler);
extern unsigned long bvme6000_gettimeoffset (void);
extern int bvme6000_hwclk (int, struct rtc_time *);
extern int bvme6000_set_clock_mmss (unsigned long);
extern void bvme6000_reset (void);
void bvme6000_set_vectors (void);

/* Save tick handler routine pointer, will point to do_timer() in
 * kernel/sched.c, called via bvme6000_process_int() */

static irq_handler_t tick_handler;


int bvme6000_parse_bootinfo(const struct bi_record *bi)
{
	if (bi->tag == BI_VME_TYPE)
		return 0;
	else
		return 1;
}

void bvme6000_reset(void)
{
	volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;

	printk ("\r\n\nCalled bvme6000_reset\r\n"
			"\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r\r");
	/* The string of returns is to delay the reset until the whole
	 * message is output. */
	/* Enable the watchdog, via PIT port C bit 4 */

	pit->pcddr	|= 0x10;	/* WDOG enable */

	while(1)
		;
}

static void bvme6000_get_model(char *model)
{
    sprintf(model, "BVME%d000", m68k_cputype == CPU_68060 ? 6 : 4);
}

/*
 * This function is called during kernel startup to initialize
 * the bvme6000 IRQ handling routines.
 */
static void __init bvme6000_init_IRQ(void)
{
	m68k_setup_user_interrupt(VEC_USER, 192, NULL);
}

void __init config_bvme6000(void)
{
    volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;

    /* Board type is only set by newer versions of vmelilo/tftplilo */
    if (!vme_brdtype) {
	if (m68k_cputype == CPU_68060)
	    vme_brdtype = VME_TYPE_BVME6000;
	else
	    vme_brdtype = VME_TYPE_BVME4000;
    }

    mach_max_dma_address = 0xffffffff;
    mach_sched_init      = bvme6000_sched_init;
    mach_init_IRQ        = bvme6000_init_IRQ;
    mach_gettimeoffset   = bvme6000_gettimeoffset;
    mach_hwclk           = bvme6000_hwclk;
    mach_set_clock_mmss	 = bvme6000_set_clock_mmss;
    mach_reset		 = bvme6000_reset;
    mach_get_model       = bvme6000_get_model;

    printk ("Board is %sconfigured as a System Controller\n",
		*config_reg_ptr & BVME_CONFIG_SW1 ? "" : "not ");

    /* Now do the PIT configuration */

    pit->pgcr	= 0x00;	/* Unidirectional 8 bit, no handshake for now */
    pit->psrr	= 0x18;	/* PIACK and PIRQ functions enabled */
    pit->pacr	= 0x00;	/* Sub Mode 00, H2 i/p, no DMA */
    pit->padr	= 0x00;	/* Just to be tidy! */
    pit->paddr	= 0x00;	/* All inputs for now (safest) */
    pit->pbcr	= 0x80;	/* Sub Mode 1x, H4 i/p, no DMA */
    pit->pbdr	= 0xbc | (*config_reg_ptr & BVME_CONFIG_SW1 ? 0 : 0x40);
			/* PRI, SYSCON?, Level3, SCC clks from xtal */
    pit->pbddr	= 0xf3;	/* Mostly outputs */
    pit->pcdr	= 0x01;	/* PA transceiver disabled */
    pit->pcddr	= 0x03;	/* WDOG disable */

    /* Disable snooping for Ethernet and VME accesses */

    bvme_acr_addrctl = 0;
}


irqreturn_t bvme6000_abort_int (int irq, void *dev_id)
{
        unsigned long *new = (unsigned long *)vectors;
        unsigned long *old = (unsigned long *)0xf8000000;

        /* Wait for button release */
        while (*(volatile unsigned char *)BVME_LOCAL_IRQ_STAT & BVME_ABORT_STATUS)
                ;

        *(new+4) = *(old+4);            /* Illegal instruction */
        *(new+9) = *(old+9);            /* Trace */
        *(new+47) = *(old+47);          /* Trap #15 */
        *(new+0x1f) = *(old+0x1f);      /* ABORT switch */
	return IRQ_HANDLED;
}


static irqreturn_t bvme6000_timer_int (int irq, void *dev_id)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    unsigned char msr = rtc->msr & 0xc0;

    rtc->msr = msr | 0x20;		/* Ack the interrupt */

    return tick_handler(irq, dev_id);
}

/*
 * Set up the RTC timer 1 to mode 2, so T1 output toggles every 5ms
 * (40000 x 125ns).  It will interrupt every 10ms, when T1 goes low.
 * So, when reading the elapsed time, you should read timer1,
 * subtract it from 39999, and then add 40000 if T1 is high.
 * That gives you the number of 125ns ticks in to the 10ms period,
 * so divide by 8 to get the microsecond result.
 */

void bvme6000_sched_init (irq_handler_t timer_routine)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    unsigned char msr = rtc->msr & 0xc0;

    rtc->msr = 0;	/* Ensure timer registers accessible */

    tick_handler = timer_routine;
    if (request_irq(BVME_IRQ_RTC, bvme6000_timer_int, 0,
				"timer", bvme6000_timer_int))
	panic ("Couldn't register timer int");

    rtc->t1cr_omr = 0x04;	/* Mode 2, ext clk */
    rtc->t1msb = 39999 >> 8;
    rtc->t1lsb = 39999 & 0xff;
    rtc->irr_icr1 &= 0xef;	/* Route timer 1 to INTR pin */
    rtc->msr = 0x40;		/* Access int.cntrl, etc */
    rtc->pfr_icr0 = 0x80;	/* Just timer 1 ints enabled */
    rtc->irr_icr1 = 0;
    rtc->t1cr_omr = 0x0a;	/* INTR+T1 active lo, push-pull */
    rtc->t0cr_rtmr &= 0xdf;	/* Stop timers in standby */
    rtc->msr = 0;		/* Access timer 1 control */
    rtc->t1cr_omr = 0x05;	/* Mode 2, ext clk, GO */

    rtc->msr = msr;

    if (request_irq(BVME_IRQ_ABORT, bvme6000_abort_int, 0,
				"abort", bvme6000_abort_int))
	panic ("Couldn't register abort int");
}


/* This is always executed with interrupts disabled.  */

/*
 * NOTE:  Don't accept any readings within 5us of rollover, as
 * the T1INT bit may be a little slow getting set.  There is also
 * a fault in the chip, meaning that reads may produce invalid
 * results...
 */

unsigned long bvme6000_gettimeoffset (void)
{
    volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
    volatile PitRegsPtr pit = (PitRegsPtr)BVME_PIT_BASE;
    unsigned char msr = rtc->msr & 0xc0;
    unsigned char t1int, t1op;
    unsigned long v = 800000, ov;

    rtc->msr = 0;	/* Ensure timer registers accessible */

    do {
	ov = v;
	t1int = rtc->msr & 0x20;
	t1op  = pit->pcdr & 0x04;
	rtc->t1cr_omr |= 0x40;		/* Latch timer1 */
	v = rtc->t1msb << 8;		/* Read timer1 */
	v |= rtc->t1lsb;		/* Read timer1 */
    } while (t1int != (rtc->msr & 0x20) ||
		t1op != (pit->pcdr & 0x04) ||
			abs(ov-v) > 80 ||
				v > 39960);

    v = 39999 - v;
    if (!t1op)				/* If in second half cycle.. */
	v += 40000;
    v /= 8;				/* Convert ticks to microseconds */
    if (t1int)
	v += 10000;			/* Int pending, + 10ms */
    rtc->msr = msr;

    return v;
}

/*
 * Looks like op is non-zero for setting the clock, and zero for
 * reading the clock.
 *
 *  struct hwclk_time {
 *         unsigned        sec;       0..59
 *         unsigned        min;       0..59
 *         unsigned        hour;      0..23
 *         unsigned        day;       1..31
 *         unsigned        mon;       0..11
 *         unsigned        year;      00...
 *         int             wday;      0..6, 0 is Sunday, -1 means unknown/don't set
 * };
 */

int bvme6000_hwclk(int op, struct rtc_time *t)
{
	volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
	unsigned char msr = rtc->msr & 0xc0;

	rtc->msr = 0x40;	/* Ensure clock and real-time-mode-register
				 * are accessible */
	if (op)
	{	/* Write.... */
		rtc->t0cr_rtmr = t->tm_year%4;
		rtc->bcd_tenms = 0;
		rtc->bcd_sec = bin2bcd(t->tm_sec);
		rtc->bcd_min = bin2bcd(t->tm_min);
		rtc->bcd_hr  = bin2bcd(t->tm_hour);
		rtc->bcd_dom = bin2bcd(t->tm_mday);
		rtc->bcd_mth = bin2bcd(t->tm_mon + 1);
		rtc->bcd_year = bin2bcd(t->tm_year%100);
		if (t->tm_wday >= 0)
			rtc->bcd_dow = bin2bcd(t->tm_wday+1);
		rtc->t0cr_rtmr = t->tm_year%4 | 0x08;
	}
	else
	{	/* Read....  */
		do {
			t->tm_sec  = bcd2bin(rtc->bcd_sec);
			t->tm_min  = bcd2bin(rtc->bcd_min);
			t->tm_hour = bcd2bin(rtc->bcd_hr);
			t->tm_mday = bcd2bin(rtc->bcd_dom);
			t->tm_mon  = bcd2bin(rtc->bcd_mth)-1;
			t->tm_year = bcd2bin(rtc->bcd_year);
			if (t->tm_year < 70)
				t->tm_year += 100;
			t->tm_wday = bcd2bin(rtc->bcd_dow)-1;
		} while (t->tm_sec != bcd2bin(rtc->bcd_sec));
	}

	rtc->msr = msr;

	return 0;
}

/*
 * Set the minutes and seconds from seconds value 'nowtime'.  Fail if
 * clock is out by > 30 minutes.  Logic lifted from atari code.
 * Algorithm is to wait for the 10ms register to change, and then to
 * wait a short while, and then set it.
 */

int bvme6000_set_clock_mmss (unsigned long nowtime)
{
	int retval = 0;
	short real_seconds = nowtime % 60, real_minutes = (nowtime / 60) % 60;
	unsigned char rtc_minutes, rtc_tenms;
	volatile RtcPtr_t rtc = (RtcPtr_t)BVME_RTC_BASE;
	unsigned char msr = rtc->msr & 0xc0;
	unsigned long flags;
	volatile int i;

	rtc->msr = 0;		/* Ensure clock accessible */
	rtc_minutes = bcd2bin (rtc->bcd_min);

	if ((rtc_minutes < real_minutes
		? real_minutes - rtc_minutes
			: rtc_minutes - real_minutes) < 30)
	{
		local_irq_save(flags);
		rtc_tenms = rtc->bcd_tenms;
		while (rtc_tenms == rtc->bcd_tenms)
			;
		for (i = 0; i < 1000; i++)
			;
		rtc->bcd_min = bin2bcd(real_minutes);
		rtc->bcd_sec = bin2bcd(real_seconds);
		local_irq_restore(flags);
	}
	else
		retval = -1;

	rtc->msr = msr;

	return retval;
}
