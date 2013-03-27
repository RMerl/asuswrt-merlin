/* e500 registers, for PSIM, the PowerPC simulator.

   Copyright 2003, 2007 Free Software Foundation, Inc.

   Contributed by Red Hat Inc; developed under contract from Motorola.
   Written by matthew green <mrg@redhat.com>.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* e500 accumulator.  */

typedef unsigned64 accreg;

enum {
  msr_e500_spu_enable = BIT(38)
};

/* E500 regsiters.  */

enum
  {
  spefscr_sovh = BIT(32),	/* summary integer overlow (high) */
  spefscr_ovh = BIT(33),	/* int overflow (high) */
  spefscr_fgh = BIT(34),	/* FP guard (high) */
  spefscr_fxh = BIT(35),	/* FP sticky (high) */
  spefscr_finvh = BIT(36),	/* FP invalid operand (high) */
  spefscr_fdbzh = BIT(37),	/* FP divide by zero (high) */
  spefscr_funfh = BIT(38),	/* FP underflow (high) */
  spefscr_fovfh = BIT(39),	/* FP overflow (high) */
  spefscr_finxs = BIT(42),	/* FP inexact sticky */
  spefscr_finvs = BIT(43),	/* FP invalid operand sticky */
  spefscr_fdbzs = BIT(44),	/* FP divide by zero sticky */
  spefscr_funfs = BIT(45),	/* FP underflow sticky */
  spefscr_fovfs = BIT(46),	/* FP overflow sticky */
  spefscr_mode = BIT(47),	/* SPU MODE (read only) */
  spefscr_sov = BIT(48),	/* Summary integer overlow (low) */
  spefscr_ov = BIT(49),		/* int overflow (low) */
  spefscr_fg = BIT(50),		/* FP guard (low) */
  spefscr_fx = BIT(51),		/* FP sticky (low) */
  spefscr_finv = BIT(52),	/* FP invalid operand (low) */
  spefscr_fdbz = BIT(53),	/* FP divide by zero (low) */
  spefscr_funf = BIT(54),	/* FP underflow (low) */
  spefscr_fovf = BIT(55),	/* FP overflow (low) */
  spefscr_finxe = BIT(57),	/* FP inexact enable */
  spefscr_finve = BIT(58),	/* FP invalid operand enable */
  spefscr_fdbze = BIT(59),	/* FP divide by zero enable */
  spefscr_funfe = BIT(60),	/* FP underflow enable */
  spefscr_fovfe = BIT(61),	/* FP overflow enable */
  spefscr_frmc0 = BIT(62),	/* FP round mode control */
  spefscr_frmc1 = BIT(63),
  spefscr_frmc = (spefscr_frmc0 | spefscr_frmc1),
};

struct e500_regs {
  /* e500 high bits.  */
  signed_word gprh[32];
  /* Accumulator */
  accreg acc;
};

/* SPE partially visible acculator */
#define ACC		cpu_registers(processor)->e500.acc

/* e500 register high bits */
#define GPRH(N)		cpu_registers(processor)->e500.gprh[N]

/* e500 unified vector register
   We need to cast the gpr value to an unsigned type so that it
   doesn't get sign-extended when it's or-ed with a 64-bit value; that
   would wipe out the upper 32 bits of the register's value.  */
#define EVR(N)		((((unsigned64)GPRH(N)) << 32) | (unsigned32) GPR(N))
