/*
 * BCM47XX support code for some chipcommon facilities (uart, jtagm)
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
 * $Id: hndchipc.c 310902 2012-01-26 19:45:33Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmnvram.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <hndchipc.h>
#include <hndcpu.h>

/* debug/trace */
#ifdef BCMDBG_ERR
#define	CC_ERROR(args)	printf args
#else
#define	CC_ERROR(args)
#endif	/* BCMDBG_ERR */

#ifdef BCMDBG
#define	CC_MSG(args)	printf args
#else
#define	CC_MSG(args)
#endif	/* BCMDBG */

/* interested chipcommon interrupt source
 *  - GPIO
 *  - EXTIF
 *  - ECI
 *  - PMU
 *  - UART
 */
#define	MAX_CC_INT_SOURCE 5

/* chipc secondary isr info */
typedef struct {
	uint intmask;		/* int mask */
	cc_isr_fn isr;		/* secondary isr handler */
	void *cbdata;		/* pointer to private data */
} cc_isr_info_t;

static cc_isr_info_t cc_isr_desc[MAX_CC_INT_SOURCE];

/* chip common intmask */
static uint32 cc_intmask = 0;

/*
 * ROM accessor to avoid struct in shdat
 */
static cc_isr_info_t *
get_cc_isr_desc(void)
{
	return cc_isr_desc;
}

/*
 * Initializes UART access. The callback function will be called once
 * per found UART.
 */
void
BCMATTACHFN(si_serial_init)(si_t *sih, si_serial_init_fn add)
{
	osl_t *osh;
	void *regs;
	chipcregs_t *cc;
	uint32 rev, cap, pll, baud_base, div;
	uint irq;
	int i, n;

	osh = si_osh(sih);

	cc = (chipcregs_t *)si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(cc);

	/* Determine core revision and capabilities */
	rev = sih->ccrev;
	cap = sih->cccaps;
	pll = cap & CC_CAP_PLL_MASK;

	/* Determine IRQ */
	irq = si_irq(sih);

	if (CCPLL_ENAB(sih) && pll == PLL_TYPE1) {
		/* PLL clock */
		baud_base = si_clock_rate(pll,
		                          R_REG(osh, &cc->clockcontrol_n),
		                          R_REG(osh, &cc->clockcontrol_m2));
		div = 1;
	} else {
		/* Fixed ALP clock */
		if (rev >= 11 && rev != 15) {
			baud_base = si_alp_clock(sih);
			div = 1;
			/* Turn off UART clock before switching clock source */
			if (rev >= 21)
				AND_REG(osh, &cc->corecontrol, ~CC_UARTCLKEN);
			/* Set the override bit so we don't divide it */
			OR_REG(osh, &cc->corecontrol, CC_UARTCLKO);
			if (rev >= 21)
				OR_REG(osh, &cc->corecontrol, CC_UARTCLKEN);
		} else if (rev >= 3) {
			/* Internal backplane clock */
			baud_base = si_clock(sih);
			div = 2;	/* Minimum divisor */
			W_REG(osh, &cc->clkdiv,
			      ((R_REG(osh, &cc->clkdiv) & ~CLKD_UART) | div));
		} else {
			/* Fixed internal backplane clock */
			baud_base = 88000000;
			div = 48;
		}

		/* Clock source depends on strapping if UartClkOverride is unset */
		if ((R_REG(osh, &cc->corecontrol) & CC_UARTCLKO) == 0) {
			if ((cap & CC_CAP_UCLKSEL) == CC_CAP_UINTCLK) {
				/* Internal divided backplane clock */
				baud_base /= div;
			} else {
				/* Assume external clock of 1.8432 MHz */
				baud_base = 1843200;
			}
		}
	}

	/* Add internal UARTs */
	n = cap & CC_CAP_UARTS_MASK;
	for (i = 0; i < n; i++) {
		regs = (void *)((ulong) &cc->uart0data + (i * 256));
		if (add)
			add(regs, irq, baud_base, 0);
	}
}

#define JTAG_RETRIES	10000

/*
 * Initialize jtag master and return handle for
 * jtag_rwreg. Returns NULL on failure.
 */
void *
hnd_jtagm_init(si_t *sih, uint clkd, bool exttap)
{
	void *regs;

	if ((regs = si_setcoreidx(sih, SI_CC_IDX)) != NULL) {
		chipcregs_t *cc = (chipcregs_t *) regs;
		uint32 tmp;

		/*
		 * Determine jtagm availability from
		 * core revision and capabilities.
		 */

		/*
		 * Corerev 10 has jtagm, but the only chip
		 * with it does not have a mips, and
		 * the layout of the jtagcmd register is
		 * different. We'll only accept >= 11.
		 */
		if (sih->ccrev < 11)
			return (NULL);

		if ((sih->cccaps & CC_CAP_JTAGP) == 0)
			return (NULL);

		/* Set clock divider if requested */
		if (clkd != 0) {
			tmp = R_REG(NULL, &cc->clkdiv);
			tmp = (tmp & ~CLKD_JTAG) |
				((clkd << CLKD_JTAG_SHIFT) & CLKD_JTAG);
			W_REG(NULL, &cc->clkdiv, tmp);
		}

		/* Enable jtagm */
		tmp = JCTRL_EN | (exttap ? JCTRL_EXT_EN : 0);
		W_REG(NULL, &cc->jtagctrl, tmp);
	}

	return (regs);
}

void
hnd_jtagm_disable(si_t *sih, void *h)
{
	chipcregs_t *cc = (chipcregs_t *)h;

	W_REG(NULL, &cc->jtagctrl, R_REG(NULL, &cc->jtagctrl) & ~JCTRL_EN);
}


static uint32
jtm_wait(chipcregs_t *cc, bool readdr)
{
	uint i;

	i = 0;
	while (((R_REG(NULL, &cc->jtagcmd) & JCMD_BUSY) == JCMD_BUSY) &&
	       (i < JTAG_RETRIES)) {
		i++;
	}

	if (i >= JTAG_RETRIES)
		return 0xbadbad03;

	if (readdr)
		return R_REG(NULL, &cc->jtagdr);
	else
		return 0xffffffff;
}

/* Read/write a jtag register. Assumes both ir and dr <= 64bits. */

uint32
jtag_scan(si_t *sih, void *h, uint irsz, uint32 ir0, uint32 ir1,
          uint drsz, uint32 dr0, uint32 *dr1, bool rti)
{
	chipcregs_t *cc = (chipcregs_t *) h;
	uint32 acc_dr, acc_irdr;
	uint32 tmp;

	if ((irsz > 64) || (drsz > 64)) {
		return 0xbadbad00;
	}
	if (rti) {
		if (sih->ccrev < 28)
			return 0xbadbad01;
		acc_irdr = JCMD_ACC_IRDR_I;
		acc_dr = JCMD_ACC_DR_I;
	} else {
		acc_irdr = JCMD_ACC_IRDR;
		acc_dr = JCMD_ACC_DR;
	}
	if (irsz == 0) {
		/* scan in the first (or only) DR word with a dr-only command */
		W_REG(NULL, &cc->jtagdr, dr0);
		if (drsz > 32) {
			W_REG(NULL, &cc->jtagcmd, JCMD_START | JCMD_ACC_PDR | 31);
			drsz -= 32;
		} else
			W_REG(NULL, &cc->jtagcmd, JCMD_START | acc_dr | (drsz - 1));
	} else {
		W_REG(NULL, &cc->jtagir, ir0);
		if (irsz > 32) {
			/* Use Partial IR for first IR word */
			W_REG(NULL, &cc->jtagcmd, JCMD_START | JCMD_ACC_PIR |
			      (31 << JCMD_IRW_SHIFT));
			jtm_wait(cc, FALSE);
			W_REG(NULL, &cc->jtagir, ir1);
			irsz -= 32;
		}
		if (drsz == 0) {
			/* If drsz is 0, do an IR-only scan and that's it */
			W_REG(NULL, &cc->jtagcmd, JCMD_START | JCMD_ACC_IR |
			      ((irsz - 1) << JCMD_IRW_SHIFT));
			return jtm_wait(cc, FALSE);
		}
		/* Now scan in the IR word and the first (or only) DR word */
		W_REG(NULL, &cc->jtagdr, dr0);
		if (drsz <= 32)
			W_REG(NULL, &cc->jtagcmd, JCMD_START | acc_irdr |
			      ((irsz - 1) << JCMD_IRW_SHIFT) | (drsz - 1));
		else
			W_REG(NULL, &cc->jtagcmd, JCMD_START | JCMD_ACC_IRPDR |
			      ((irsz - 1) << JCMD_IRW_SHIFT) | 31);
	}
	/* Now scan out the DR and scan in & out the second DR word if needed */
	tmp = jtm_wait(cc, TRUE);
	if (drsz > 32) {
		if (dr1 == NULL)
			return 0xbadbad04;
		W_REG(NULL, &cc->jtagdr, *dr1);
		W_REG(NULL, &cc->jtagcmd, JCMD_START | acc_dr | (drsz - 33));
		*dr1 = jtm_wait(cc, TRUE);
	}
	return (tmp);
}


/*
 * Interface to register chipc secondary isr
 */

bool
BCMATTACHFN(si_cc_register_isr)(si_t *sih, cc_isr_fn isr, uint32 ccintmask, void *cbdata)
{
	bool done = FALSE;
	chipcregs_t *regs;
	uint origidx;
	uint i;

	/* Save the current core index */
	origidx = si_coreidx(sih);
	regs = si_setcoreidx(sih, SI_CC_IDX);
	ASSERT(regs);

	for (i = 0; i < MAX_CC_INT_SOURCE; i++) {
		if (cc_isr_desc[i].isr == NULL) {
			cc_isr_desc[i].isr = isr;
			cc_isr_desc[i].cbdata = cbdata;
			cc_isr_desc[i].intmask = ccintmask;
			done = TRUE;
			break;
		}
	}

	if (done) {
		cc_intmask = R_REG(si_osh(sih), &regs->intmask);
		cc_intmask |= ccintmask;
		W_REG(si_osh(sih), &regs->intmask, cc_intmask);
	}

	/* restore original coreidx */
	si_setcoreidx(sih, origidx);
	return done;
}

/*
 * chipc primary interrupt handler
 *
 */

void
si_cc_isr(si_t *sih, chipcregs_t *regs)
{
	uint32 ccintstatus;
	uint32 intstatus;
	uint32 i;
	cc_isr_info_t *desc;

	/* prior to rev 21 chipc interrupt means uart and gpio */
	if (sih->ccrev >= 21)
		ccintstatus = R_REG(si_osh(sih), &regs->intstatus) & cc_intmask;
	else
		ccintstatus = (CI_UART | CI_GPIO);

	desc = get_cc_isr_desc();
	ASSERT(desc);
	for (i = 0; i < MAX_CC_INT_SOURCE; i++, desc++) {
		if ((desc->isr != NULL) &&
		    (intstatus = (desc->intmask & ccintstatus))) {
			(desc->isr)(desc->cbdata, intstatus);
		}
	}
}
