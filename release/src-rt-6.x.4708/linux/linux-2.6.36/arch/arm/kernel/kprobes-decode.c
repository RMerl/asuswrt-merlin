/*
 * arch/arm/kernel/kprobes-decode.c
 *
 * Copyright (C) 2006, 2007 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

/*
 * We do not have hardware single-stepping on ARM, This
 * effort is further complicated by the ARM not having a
 * "next PC" register.  Instructions that change the PC
 * can't be safely single-stepped in a MP environment, so
 * we have a lot of work to do:
 *
 * In the prepare phase:
 *   *) If it is an instruction that does anything
 *      with the CPU mode, we reject it for a kprobe.
 *      (This is out of laziness rather than need.  The
 *      instructions could be simulated.)
 *
 *   *) Otherwise, decode the instruction rewriting its
 *      registers to take fixed, ordered registers and
 *      setting a handler for it to run the instruction.
 *
 * In the execution phase by an instruction's handler:
 *
 *   *) If the PC is written to by the instruction, the
 *      instruction must be fully simulated in software.
 *      If it is a conditional instruction, the handler
 *      will use insn[0] to copy its condition code to
 *	set r0 to 1 and insn[1] to "mov pc, lr" to return.
 *
 *   *) Otherwise, a modified form of the instruction is
 *      directly executed.  Its handler calls the
 *      instruction in insn[0].  In insn[1] is a
 *      "mov pc, lr" to return.
 *
 *      Before calling, load up the reordered registers
 *      from the original instruction's registers.  If one
 *      of the original input registers is the PC, compute
 *      and adjust the appropriate input register.
 *
 *	After call completes, copy the output registers to
 *      the original instruction's original registers.
 *
 * We don't use a real breakpoint instruction since that
 * would have us in the kernel go from SVC mode to SVC
 * mode losing the link register.  Instead we use an
 * undefined instruction.  To simplify processing, the
 * undefined instruction used for kprobes must be reserved
 * exclusively for kprobes use.
 *
 * TODO: ifdef out some instruction decoding based on architecture.
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>

#define sign_extend(x, signbit) ((x) | (0 - ((x) & (1 << (signbit)))))

#define branch_displacement(insn) sign_extend(((insn) & 0xffffff) << 2, 25)

#define PSR_fs	(PSR_f|PSR_s)

#define KPROBE_RETURN_INSTRUCTION	0xe1a0f00e	/* mov pc, lr */
#define SET_R0_TRUE_INSTRUCTION		0xe3a00001	/* mov	r0, #1 */

#define	truecc_insn(insn)	(((insn) & 0xf0000000) | \
				 (SET_R0_TRUE_INSTRUCTION & 0x0fffffff))

typedef long (insn_0arg_fn_t)(void);
typedef long (insn_1arg_fn_t)(long);
typedef long (insn_2arg_fn_t)(long, long);
typedef long (insn_3arg_fn_t)(long, long, long);
typedef long (insn_4arg_fn_t)(long, long, long, long);
typedef long long (insn_llret_0arg_fn_t)(void);
typedef long long (insn_llret_3arg_fn_t)(long, long, long);
typedef long long (insn_llret_4arg_fn_t)(long, long, long, long);

union reg_pair {
	long long	dr;
#ifdef __LITTLE_ENDIAN
	struct { long	r0, r1; };
#else
	struct { long	r1, r0; };
#endif
};

/*
 * For STR and STM instructions, an ARM core may choose to use either
 * a +8 or a +12 displacement from the current instruction's address.
 * Whichever value is chosen for a given core, it must be the same for
 * both instructions and may not change.  This function measures it.
 */

static int str_pc_offset;

static void __init find_str_pc_offset(void)
{
	int addr, scratch, ret;

	__asm__ (
		"sub	%[ret], pc, #4		\n\t"
		"str	pc, %[addr]		\n\t"
		"ldr	%[scr], %[addr]		\n\t"
		"sub	%[ret], %[scr], %[ret]	\n\t"
		: [ret] "=r" (ret), [scr] "=r" (scratch), [addr] "+m" (addr));

	str_pc_offset = ret;
}

/*
 * The insnslot_?arg_r[w]flags() functions below are to keep the
 * msr -> *fn -> mrs instruction sequences indivisible so that
 * the state of the CPSR flags aren't inadvertently modified
 * just before or just after the call.
 */

static inline long __kprobes
insnslot_0arg_rflags(long cpsr, insn_0arg_fn_t *fn)
{
	register long ret asm("r0");

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret)
		: [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	return ret;
}

static inline long long __kprobes
insnslot_llret_0arg_rflags(long cpsr, insn_llret_0arg_fn_t *fn)
{
	register long ret0 asm("r0");
	register long ret1 asm("r1");
	union reg_pair fnr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret0), "=r" (ret1)
		: [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	fnr.r0 = ret0;
	fnr.r1 = ret1;
	return fnr.dr;
}

static inline long __kprobes
insnslot_1arg_rflags(long r0, long cpsr, insn_1arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long ret asm("r0");

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret)
		: "0" (rr0), [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	return ret;
}

static inline long __kprobes
insnslot_2arg_rflags(long r0, long r1, long cpsr, insn_2arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long ret asm("r0");

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret)
		: "0" (rr0), "r" (rr1),
		  [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	return ret;
}

static inline long __kprobes
insnslot_3arg_rflags(long r0, long r1, long r2, long cpsr, insn_3arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long ret asm("r0");

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret)
		: "0" (rr0), "r" (rr1), "r" (rr2),
		  [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	return ret;
}

static inline long long __kprobes
insnslot_llret_3arg_rflags(long r0, long r1, long r2, long cpsr,
			   insn_llret_3arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long ret0 asm("r0");
	register long ret1 asm("r1");
	union reg_pair fnr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret0), "=r" (ret1)
		: "0" (rr0), "r" (rr1), "r" (rr2),
		  [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	fnr.r0 = ret0;
	fnr.r1 = ret1;
	return fnr.dr;
}

static inline long __kprobes
insnslot_4arg_rflags(long r0, long r1, long r2, long r3, long cpsr,
		     insn_4arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long rr3 asm("r3") = r3;
	register long ret asm("r0");

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[cpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		: "=r" (ret)
		: "0" (rr0), "r" (rr1), "r" (rr2), "r" (rr3),
		  [cpsr] "r" (cpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	return ret;
}

static inline long __kprobes
insnslot_1arg_rwflags(long r0, long *cpsr, insn_1arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long ret asm("r0");
	long oldcpsr = *cpsr;
	long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: "=r" (ret), [newcpsr] "=r" (newcpsr)
		: "0" (rr0), [oldcpsr] "r" (oldcpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	*cpsr = (oldcpsr & ~PSR_fs) | (newcpsr & PSR_fs);
	return ret;
}

static inline long __kprobes
insnslot_2arg_rwflags(long r0, long r1, long *cpsr, insn_2arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long ret asm("r0");
	long oldcpsr = *cpsr;
	long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: "=r" (ret), [newcpsr] "=r" (newcpsr)
		: "0" (rr0), "r" (rr1), [oldcpsr] "r" (oldcpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	*cpsr = (oldcpsr & ~PSR_fs) | (newcpsr & PSR_fs);
	return ret;
}

static inline long __kprobes
insnslot_3arg_rwflags(long r0, long r1, long r2, long *cpsr,
		      insn_3arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long ret asm("r0");
	long oldcpsr = *cpsr;
	long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: "=r" (ret), [newcpsr] "=r" (newcpsr)
		: "0" (rr0), "r" (rr1), "r" (rr2),
		  [oldcpsr] "r" (oldcpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	*cpsr = (oldcpsr & ~PSR_fs) | (newcpsr & PSR_fs);
	return ret;
}

static inline long __kprobes
insnslot_4arg_rwflags(long r0, long r1, long r2, long r3, long *cpsr,
		      insn_4arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long rr3 asm("r3") = r3;
	register long ret asm("r0");
	long oldcpsr = *cpsr;
	long newcpsr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: "=r" (ret), [newcpsr] "=r" (newcpsr)
		: "0" (rr0), "r" (rr1), "r" (rr2), "r" (rr3),
		  [oldcpsr] "r" (oldcpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	*cpsr = (oldcpsr & ~PSR_fs) | (newcpsr & PSR_fs);
	return ret;
}

static inline long long __kprobes
insnslot_llret_4arg_rwflags(long r0, long r1, long r2, long r3, long *cpsr,
			    insn_llret_4arg_fn_t *fn)
{
	register long rr0 asm("r0") = r0;
	register long rr1 asm("r1") = r1;
	register long rr2 asm("r2") = r2;
	register long rr3 asm("r3") = r3;
	register long ret0 asm("r0");
	register long ret1 asm("r1");
	long oldcpsr = *cpsr;
	long newcpsr;
	union reg_pair fnr;

	__asm__ __volatile__ (
		"msr	cpsr_fs, %[oldcpsr]	\n\t"
		"mov	lr, pc			\n\t"
		"mov	pc, %[fn]		\n\t"
		"mrs	%[newcpsr], cpsr	\n\t"
		: "=r" (ret0), "=r" (ret1), [newcpsr] "=r" (newcpsr)
		: "0" (rr0), "r" (rr1), "r" (rr2), "r" (rr3),
		  [oldcpsr] "r" (oldcpsr), [fn] "r" (fn)
		: "lr", "cc"
	);
	*cpsr = (oldcpsr & ~PSR_fs) | (newcpsr & PSR_fs);
	fnr.r0 = ret0;
	fnr.r1 = ret1;
	return fnr.dr;
}

/*
 * To avoid the complications of mimicing single-stepping on a
 * processor without a Next-PC or a single-step mode, and to
 * avoid having to deal with the side-effects of boosting, we
 * simulate or emulate (almost) all ARM instructions.
 *
 * "Simulation" is where the instruction's behavior is duplicated in
 * C code.  "Emulation" is where the original instruction is rewritten
 * and executed, often by altering its registers.
 *
 * By having all behavior of the kprobe'd instruction completed before
 * returning from the kprobe_handler(), all locks (scheduler and
 * interrupt) can safely be released.  There is no need for secondary
 * breakpoints, no race with MP or preemptable kernels, nor having to
 * clean up resources counts at a later time impacting overall system
 * performance.  By rewriting the instruction, only the minimum registers
 * need to be loaded and saved back optimizing performance.
 *
 * Calling the insnslot_*_rwflags version of a function doesn't hurt
 * anything even when the CPSR flags aren't updated by the
 * instruction.  It's just a little slower in return for saving
 * a little space by not having a duplicate function that doesn't
 * update the flags.  (The same optimization can be said for
 * instructions that do or don't perform register writeback)
 * Also, instructions can either read the flags, only write the
 * flags, or read and write the flags.  To save combinations
 * rather than for sheer performance, flag functions just assume
 * read and write of flags.
 */

static void __kprobes simulate_bbl(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	long iaddr = (long)p->addr;
	int disp  = branch_displacement(insn);

	if (!insnslot_1arg_rflags(0, regs->ARM_cpsr, i_fn))
		return;

	if (insn & (1 << 24))
		regs->ARM_lr = iaddr + 4;

	regs->ARM_pc = iaddr + 8 + disp;
}

static void __kprobes simulate_blx1(struct kprobe *p, struct pt_regs *regs)
{
	kprobe_opcode_t insn = p->opcode;
	long iaddr = (long)p->addr;
	int disp = branch_displacement(insn);

	regs->ARM_lr = iaddr + 4;
	regs->ARM_pc = iaddr + 8 + disp + ((insn >> 23) & 0x2);
	regs->ARM_cpsr |= PSR_T_BIT;
}

static void __kprobes simulate_blx2bx(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rm = insn & 0xf;
	long rmv = regs->uregs[rm];

	if (!insnslot_1arg_rflags(0, regs->ARM_cpsr, i_fn))
		return;

	if (insn & (1 << 5))
		regs->ARM_lr = (long)p->addr + 4;

	regs->ARM_pc = rmv & ~0x1;
	regs->ARM_cpsr &= ~PSR_T_BIT;
	if (rmv & 0x1)
		regs->ARM_cpsr |= PSR_T_BIT;
}

static void __kprobes simulate_ldm1stm1(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rn = (insn >> 16) & 0xf;
	int lbit = insn & (1 << 20);
	int wbit = insn & (1 << 21);
	int ubit = insn & (1 << 23);
	int pbit = insn & (1 << 24);
	long *addr = (long *)regs->uregs[rn];
	int reg_bit_vector;
	int reg_count;

	if (!insnslot_1arg_rflags(0, regs->ARM_cpsr, i_fn))
		return;

	reg_count = 0;
	reg_bit_vector = insn & 0xffff;
	while (reg_bit_vector) {
		reg_bit_vector &= (reg_bit_vector - 1);
		++reg_count;
	}

	if (!ubit)
		addr -= reg_count;
	addr += (!pbit == !ubit);

	reg_bit_vector = insn & 0xffff;
	while (reg_bit_vector) {
		int reg = __ffs(reg_bit_vector);
		reg_bit_vector &= (reg_bit_vector - 1);
		if (lbit)
			regs->uregs[reg] = *addr++;
		else
			*addr++ = regs->uregs[reg];
	}

	if (wbit) {
		if (!ubit)
			addr -= reg_count;
		addr -= (!pbit == !ubit);
		regs->uregs[rn] = (long)addr;
	}
}

static void __kprobes simulate_stm1_pc(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];

	if (!insnslot_1arg_rflags(0, regs->ARM_cpsr, i_fn))
		return;

	regs->ARM_pc = (long)p->addr + str_pc_offset;
	simulate_ldm1stm1(p, regs);
	regs->ARM_pc = (long)p->addr + 4;
}

static void __kprobes simulate_mov_ipsp(struct kprobe *p, struct pt_regs *regs)
{
	regs->uregs[12] = regs->uregs[13];
}

static void __kprobes emulate_ldcstc(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rn = (insn >> 16) & 0xf;
	long rnv = regs->uregs[rn];

	/* Save Rn in case of writeback. */
	regs->uregs[rn] = insnslot_1arg_rflags(rnv, regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_ldrd(struct kprobe *p, struct pt_regs *regs)
{
	insn_2arg_fn_t *i_fn = (insn_2arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;  /* rm may be invalid, don't care. */

	/* Not following the C calling convention here, so need asm(). */
	__asm__ __volatile__ (
		"ldr	r0, %[rn]	\n\t"
		"ldr	r1, %[rm]	\n\t"
		"msr	cpsr_fs, %[cpsr]\n\t"
		"mov	lr, pc		\n\t"
		"mov	pc, %[i_fn]	\n\t"
		"str	r0, %[rn]	\n\t"	/* in case of writeback */
		"str	r2, %[rd0]	\n\t"
		"str	r3, %[rd1]	\n\t"
		: [rn]  "+m" (regs->uregs[rn]),
		  [rd0] "=m" (regs->uregs[rd]),
		  [rd1] "=m" (regs->uregs[rd+1])
		: [rm]   "m" (regs->uregs[rm]),
		  [cpsr] "r" (regs->ARM_cpsr),
		  [i_fn] "r" (i_fn)
		: "r0", "r1", "r2", "r3", "lr", "cc"
	);
}

static void __kprobes emulate_strd(struct kprobe *p, struct pt_regs *regs)
{
	insn_4arg_fn_t *i_fn = (insn_4arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm  = insn & 0xf;
	long rnv = regs->uregs[rn];
	long rmv = regs->uregs[rm];  /* rm/rmv may be invalid, don't care. */

	regs->uregs[rn] = insnslot_4arg_rflags(rnv, rmv, regs->uregs[rd],
					       regs->uregs[rd+1],
					       regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_ldr(struct kprobe *p, struct pt_regs *regs)
{
	insn_llret_3arg_fn_t *i_fn = (insn_llret_3arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	long ppc = (long)p->addr + 8;
	union reg_pair fnr;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;
	long rdv;
	long rnv = (rn == 15) ? ppc : regs->uregs[rn];
	long rmv = (rm == 15) ? ppc : regs->uregs[rm];
	long cpsr = regs->ARM_cpsr;

	fnr.dr = insnslot_llret_3arg_rflags(rnv, 0, rmv, cpsr, i_fn);
	regs->uregs[rn] = fnr.r0;  /* Save Rn in case of writeback. */
	rdv = fnr.r1;

	if (rd == 15) {
#if __LINUX_ARM_ARCH__ >= 5
		cpsr &= ~PSR_T_BIT;
		if (rdv & 0x1)
			cpsr |= PSR_T_BIT;
		regs->ARM_cpsr = cpsr;
		rdv &= ~0x1;
#else
		rdv &= ~0x2;
#endif
	}
	regs->uregs[rd] = rdv;
}

static void __kprobes emulate_str(struct kprobe *p, struct pt_regs *regs)
{
	insn_3arg_fn_t *i_fn = (insn_3arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	long iaddr = (long)p->addr;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;
	long rdv = (rd == 15) ? iaddr + str_pc_offset : regs->uregs[rd];
	long rnv = (rn == 15) ? iaddr +  8 : regs->uregs[rn];
	long rmv = regs->uregs[rm];  /* rm/rmv may be invalid, don't care. */

	/* Save Rn in case of writeback. */
	regs->uregs[rn] =
		insnslot_3arg_rflags(rnv, rdv, rmv, regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_mrrc(struct kprobe *p, struct pt_regs *regs)
{
	insn_llret_0arg_fn_t *i_fn = (insn_llret_0arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	union reg_pair fnr;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;

	fnr.dr = insnslot_llret_0arg_rflags(regs->ARM_cpsr, i_fn);
	regs->uregs[rn] = fnr.r0;
	regs->uregs[rd] = fnr.r1;
}

static void __kprobes emulate_mcrr(struct kprobe *p, struct pt_regs *regs)
{
	insn_2arg_fn_t *i_fn = (insn_2arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	long rnv = regs->uregs[rn];
	long rdv = regs->uregs[rd];

	insnslot_2arg_rflags(rnv, rdv, regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_sat(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rm = insn & 0xf;
	long rmv = regs->uregs[rm];

	/* Writes Q flag */
	regs->uregs[rd] = insnslot_1arg_rwflags(rmv, &regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_sel(struct kprobe *p, struct pt_regs *regs)
{
	insn_2arg_fn_t *i_fn = (insn_2arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;
	long rnv = regs->uregs[rn];
	long rmv = regs->uregs[rm];

	/* Reads GE bits */
	regs->uregs[rd] = insnslot_2arg_rflags(rnv, rmv, regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_none(struct kprobe *p, struct pt_regs *regs)
{
	insn_0arg_fn_t *i_fn = (insn_0arg_fn_t *)&p->ainsn.insn[0];

	insnslot_0arg_rflags(regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_rd12(struct kprobe *p, struct pt_regs *regs)
{
	insn_0arg_fn_t *i_fn = (insn_0arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;

	regs->uregs[rd] = insnslot_0arg_rflags(regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_ird12(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int ird = (insn >> 12) & 0xf;

	insnslot_1arg_rflags(regs->uregs[ird], regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_rn16(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rn = (insn >> 16) & 0xf;
	long rnv = regs->uregs[rn];

	insnslot_1arg_rflags(rnv, regs->ARM_cpsr, i_fn);
}

static void __kprobes emulate_rd12rm0(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rm = insn & 0xf;
	long rmv = regs->uregs[rm];

	regs->uregs[rd] = insnslot_1arg_rflags(rmv, regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_rd12rn16rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_2arg_fn_t *i_fn = (insn_2arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	int rm = insn & 0xf;
	long rnv = regs->uregs[rn];
	long rmv = regs->uregs[rm];

	regs->uregs[rd] =
		insnslot_2arg_rwflags(rnv, rmv, &regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_rd16rn12rs8rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_3arg_fn_t *i_fn = (insn_3arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 16) & 0xf;
	int rn = (insn >> 12) & 0xf;
	int rs = (insn >> 8) & 0xf;
	int rm = insn & 0xf;
	long rnv = regs->uregs[rn];
	long rsv = regs->uregs[rs];
	long rmv = regs->uregs[rm];

	regs->uregs[rd] =
		insnslot_3arg_rwflags(rnv, rsv, rmv, &regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_rd16rs8rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_2arg_fn_t *i_fn = (insn_2arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 16) & 0xf;
	int rs = (insn >> 8) & 0xf;
	int rm = insn & 0xf;
	long rsv = regs->uregs[rs];
	long rmv = regs->uregs[rm];

	regs->uregs[rd] =
		insnslot_2arg_rwflags(rsv, rmv, &regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_rdhi16rdlo12rs8rm0_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_llret_4arg_fn_t *i_fn = (insn_llret_4arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	union reg_pair fnr;
	int rdhi = (insn >> 16) & 0xf;
	int rdlo = (insn >> 12) & 0xf;
	int rs   = (insn >> 8) & 0xf;
	int rm   = insn & 0xf;
	long rsv = regs->uregs[rs];
	long rmv = regs->uregs[rm];

	fnr.dr = insnslot_llret_4arg_rwflags(regs->uregs[rdhi],
					     regs->uregs[rdlo], rsv, rmv,
					     &regs->ARM_cpsr, i_fn);
	regs->uregs[rdhi] = fnr.r0;
	regs->uregs[rdlo] = fnr.r1;
}

static void __kprobes
emulate_alu_imm_rflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	long rnv = (rn == 15) ? (long)p->addr + 8 : regs->uregs[rn];

	regs->uregs[rd] = insnslot_1arg_rflags(rnv, regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_alu_imm_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_1arg_fn_t *i_fn = (insn_1arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;
	long rnv = (rn == 15) ? (long)p->addr + 8 : regs->uregs[rn];

	regs->uregs[rd] = insnslot_1arg_rwflags(rnv, &regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_alu_rflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_3arg_fn_t *i_fn = (insn_3arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	long ppc = (long)p->addr + 8;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;	/* rn/rnv/rs/rsv may be */
	int rs = (insn >> 8) & 0xf;	/* invalid, don't care. */
	int rm = insn & 0xf;
	long rnv = (rn == 15) ? ppc : regs->uregs[rn];
	long rmv = (rm == 15) ? ppc : regs->uregs[rm];
	long rsv = regs->uregs[rs];

	regs->uregs[rd] =
		insnslot_3arg_rflags(rnv, rmv, rsv, regs->ARM_cpsr, i_fn);
}

static void __kprobes
emulate_alu_rwflags(struct kprobe *p, struct pt_regs *regs)
{
	insn_3arg_fn_t *i_fn = (insn_3arg_fn_t *)&p->ainsn.insn[0];
	kprobe_opcode_t insn = p->opcode;
	long ppc = (long)p->addr + 8;
	int rd = (insn >> 12) & 0xf;
	int rn = (insn >> 16) & 0xf;	/* rn/rnv/rs/rsv may be */
	int rs = (insn >> 8) & 0xf;	/* invalid, don't care. */
	int rm = insn & 0xf;
	long rnv = (rn == 15) ? ppc : regs->uregs[rn];
	long rmv = (rm == 15) ? ppc : regs->uregs[rm];
	long rsv = regs->uregs[rs];

	regs->uregs[rd] =
		insnslot_3arg_rwflags(rnv, rmv, rsv, &regs->ARM_cpsr, i_fn);
}

static enum kprobe_insn __kprobes
prep_emulate_ldr_str(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	int ibit = (insn & (1 << 26)) ? 25 : 22;

	insn &= 0xfff00fff;
	insn |= 0x00001000;	/* Rn = r0, Rd = r1 */
	if (insn & (1 << ibit)) {
		insn &= ~0xf;
		insn |= 2;	/* Rm = r2 */
	}
	asi->insn[0] = insn;
	asi->insn_handler = (insn & (1 << 20)) ? emulate_ldr : emulate_str;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rd12rm0(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= 0xffff0ff0;	/* Rd = r0, Rm = r0 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rd12rm0;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rd12(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= 0xffff0fff;	/* Rd = r0 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rd12;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rd12rn16rm0_wflags(kprobe_opcode_t insn,
				struct arch_specific_insn *asi)
{
	insn &= 0xfff00ff0;	/* Rd = r0, Rn = r0 */
	insn |= 0x00000001;	/* Rm = r1 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rd12rn16rm0_rwflags;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rd16rs8rm0_wflags(kprobe_opcode_t insn,
			       struct arch_specific_insn *asi)
{
	insn &= 0xfff0f0f0;	/* Rd = r0, Rs = r0 */
	insn |= 0x00000001;	/* Rm = r1          */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rd16rs8rm0_rwflags;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rd16rn12rs8rm0_wflags(kprobe_opcode_t insn,
				   struct arch_specific_insn *asi)
{
	insn &= 0xfff000f0;	/* Rd = r0, Rn = r0 */
	insn |= 0x00000102;	/* Rs = r1, Rm = r2 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rd16rn12rs8rm0_rwflags;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
prep_emulate_rdhi16rdlo12rs8rm0_wflags(kprobe_opcode_t insn,
				       struct arch_specific_insn *asi)
{
	insn &= 0xfff000f0;	/* RdHi = r0, RdLo = r1 */
	insn |= 0x00001203;	/* Rs = r2, Rm = r3 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_rdhi16rdlo12rs8rm0_rwflags;
	return INSN_GOOD;
}

/*
 * For the instruction masking and comparisons in all the "space_*"
 * functions below, Do _not_ rearrange the order of tests unless
 * you're very, very sure of what you are doing.  For the sake of
 * efficiency, the masks for some tests sometimes assume other test
 * have been done prior to them so the number of patterns to test
 * for an instruction set can be as broad as possible to reduce the
 * number of tests needed.
 */

static enum kprobe_insn __kprobes
space_1111(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0xfff30020) == 0xf1020000 ||
	    (insn & 0xfe500f00) == 0xf8100a00 ||
	    (insn & 0xfe5f0f00) == 0xf84d0500)
		return INSN_REJECTED;

	if ((insn & 0xfd700000) == 0xf4500000) {
		insn &= 0xfff0ffff;	/* Rn = r0 */
		asi->insn[0] = insn;
		asi->insn_handler = emulate_rn16;
		return INSN_GOOD;
	}

	if ((insn & 0xfe000000) == 0xfa000000) {
		asi->insn_handler = simulate_blx1;
		return INSN_GOOD_NO_SLOT;
	}

	if ((insn & 0xffff00f0) == 0xf1010000 ||
	    (insn & 0xff000010) == 0xfe000000) {
		asi->insn[0] = insn;
		asi->insn_handler = emulate_none;
		return INSN_GOOD;
	}

	if ((insn & 0xffe00000) == 0xfc400000) {
		insn &= 0xfff00fff;	/* Rn = r0 */
		insn |= 0x00001000;	/* Rd = r1 */
		asi->insn[0] = insn;
		asi->insn_handler =
			(insn & (1 << 20)) ? emulate_mrrc : emulate_mcrr;
		return INSN_GOOD;
	}

	if ((insn & 0xfe000000) == 0xfc000000) {
		insn &= 0xfff0ffff;      /* Rn = r0 */
		asi->insn[0] = insn;
		asi->insn_handler = emulate_ldcstc;
		return INSN_GOOD;
	}

	insn &= 0xffff0fff;	/* Rd = r0 */
	asi->insn[0]      = insn;
	asi->insn_handler = (insn & (1 << 20)) ? emulate_rd12 : emulate_ird12;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_000x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0x0f900010) == 0x01000000) {

		if ((insn & 0x0ff000f0) == 0x01200020 ||
		    (insn & 0x0fb000f0) == 0x01200000)
			return INSN_REJECTED;

		if ((insn & 0x0fb00010) == 0x01000000)
			return prep_emulate_rd12(insn, asi);

		if ((insn & 0x0ff00090) == 0x01400080)
			return prep_emulate_rdhi16rdlo12rs8rm0_wflags(insn, asi);

		if ((insn & 0x0ff000b0) == 0x012000a0 ||
		    (insn & 0x0ff00090) == 0x01600080)
			return prep_emulate_rd16rs8rm0_wflags(insn, asi);

		return prep_emulate_rd16rn12rs8rm0_wflags(insn, asi);

	}

	else if ((insn & 0x0f900090) == 0x01000010) {

		if ((insn & 0xfff000f0) == 0xe1200070)
			return INSN_REJECTED;

		if ((insn & 0x0ff000d0) == 0x01200010) {
			asi->insn[0] = truecc_insn(insn);
			asi->insn_handler = simulate_blx2bx;
			return INSN_GOOD;
		}

		if ((insn & 0x0ff000f0) == 0x01600010)
			return prep_emulate_rd12rm0(insn, asi);

		return prep_emulate_rd12rn16rm0_wflags(insn, asi);
	}

	else if ((insn & 0x0f000090) == 0x00000090) {

		if ((insn & 0x0fe000f0) == 0x00000090) {
		       return prep_emulate_rd16rs8rm0_wflags(insn, asi);
		} else if  ((insn & 0x0fe000f0) == 0x00200090) {
		       return prep_emulate_rd16rn12rs8rm0_wflags(insn, asi);
		} else {
		       return prep_emulate_rdhi16rdlo12rs8rm0_wflags(insn, asi);
		}
	}

	else if ((insn & 0x0e000090) == 0x00000090) {

		if ((insn & 0x0fb000f0) == 0x01000090) {
			/* SWP/SWPB */
			return prep_emulate_rd12rn16rm0_wflags(insn, asi);
		} else if ((insn & 0x0e1000d0) == 0x00000d0) {
			/* STRD/LDRD */
			insn &= 0xfff00fff;
			insn |= 0x00002000;	/* Rn = r0, Rd = r2 */
			if (insn & (1 << 22)) {
				/* I bit */
				insn &= ~0xf;
				insn |= 1;	/* Rm = r1 */
			}
			asi->insn[0] = insn;
			asi->insn_handler =
				(insn & (1 << 5)) ? emulate_strd : emulate_ldrd;
			return INSN_GOOD;
		}

		return prep_emulate_ldr_str(insn, asi);
	}


	if ((insn & 0x0e10f000) == 0x0010f000)
		return INSN_REJECTED;

	/*
	 * "mov ip, sp" is the most common kprobe'd instruction by far.
	 * Check and optimize for it explicitly.
	 */
	if (insn == 0xe1a0c00d) {
		asi->insn_handler = simulate_mov_ipsp;
		return INSN_GOOD_NO_SLOT;
	}

	insn &= 0xfff00ff0;	/* Rn = r0, Rd = r0 */
	insn |= 0x00000001;	/* Rm = r1 */
	if (insn & 0x010) {
		insn &= 0xfffff0ff;     /* register shift */
		insn |= 0x00000200;     /* Rs = r2 */
	}
	asi->insn[0] = insn;
	asi->insn_handler = (insn & (1 << 20)) ?  /* S-bit */
				emulate_alu_rwflags : emulate_alu_rflags;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_001x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0x0fb00000) == 0x03200000 ||	/* MSR */
	    (insn & 0x0ff00000) == 0x03400000 ||	/* Undef */
	    (insn & 0x0e10f000) == 0x0210f000)		/* ALU s-bit, R15  */
		return INSN_REJECTED;

	insn &= 0xffff0fff;	/* Rd = r0 */
	asi->insn[0] = insn;
	asi->insn_handler = (insn & (1 << 20)) ?  /* S-bit */
			emulate_alu_imm_rwflags : emulate_alu_imm_rflags;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_0110__1(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0x0ff000f0) == 0x068000b0) {
		insn &= 0xfff00ff0;	/* Rd = r0, Rn = r0 */
		insn |= 0x00000001;	/* Rm = r1 */
		asi->insn[0] = insn;
		asi->insn_handler = emulate_sel;
		return INSN_GOOD;
	}

	if ((insn & 0x0fa00030) == 0x06a00010 ||
	    (insn & 0x0fb000f0) == 0x06a00030) {
		insn &= 0xffff0ff0;	/* Rd = r0, Rm = r0 */
		asi->insn[0] = insn;
		asi->insn_handler = emulate_sat;
		return INSN_GOOD;
	}

	if ((insn & 0x0ff00070) == 0x06b00030 ||
	    (insn & 0x0ff000f0) == 0x06f000b0)
		return prep_emulate_rd12rm0(insn, asi);

	return prep_emulate_rd12rn16rm0_wflags(insn, asi);
}

static enum kprobe_insn __kprobes
space_cccc_0111__1(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0x0ff000f0) == 0x03f000f0)
		return INSN_REJECTED;

	if ((insn & 0x0ff000f0) == 0x07800010)
		 return prep_emulate_rd16rn12rs8rm0_wflags(insn, asi);

	if ((insn & 0x0ff00090) == 0x07400010)
		return prep_emulate_rdhi16rdlo12rs8rm0_wflags(insn, asi);

	if ((insn & 0x0ff00090) == 0x07000010 ||
	    (insn & 0x0ff000d0) == 0x07500010 ||
	    (insn & 0x0ff000d0) == 0x075000d0)
		return prep_emulate_rd16rn12rs8rm0_wflags(insn, asi);

	return prep_emulate_rd16rs8rm0_wflags(insn, asi);
}

static enum kprobe_insn __kprobes
space_cccc_01xx(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	return prep_emulate_ldr_str(insn, asi);
}

static enum kprobe_insn __kprobes
space_cccc_100x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0x0e708000) == 0x85000000 ||
	    (insn & 0x0e508000) == 0x85010000)
		return INSN_REJECTED;

	asi->insn[0] = truecc_insn(insn);
	asi->insn_handler = ((insn & 0x108000) == 0x008000) ? /* STM & R15 */
				simulate_stm1_pc : simulate_ldm1stm1;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_101x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn[0] = truecc_insn(insn);
	asi->insn_handler = simulate_bbl;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_1100_010x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= 0xfff00fff;
	insn |= 0x00001000;	/* Rn = r0, Rd = r1 */
	asi->insn[0] = insn;
	asi->insn_handler = (insn & (1 << 20)) ? emulate_mrrc : emulate_mcrr;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_110x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	insn &= 0xfff0ffff;	/* Rn = r0 */
	asi->insn[0] = insn;
	asi->insn_handler = emulate_ldcstc;
	return INSN_GOOD;
}

static enum kprobe_insn __kprobes
space_cccc_111x(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	if ((insn & 0xfff000f0) == 0xe1200070 ||
	    (insn & 0x0f000000) == 0x0f000000)
		return INSN_REJECTED;

	if ((insn & 0x0f000010) == 0x0e000000) {
		asi->insn[0] = insn;
		asi->insn_handler = emulate_none;
		return INSN_GOOD;
	}

	insn &= 0xffff0fff;	/* Rd = r0 */
	asi->insn[0] = insn;
	asi->insn_handler = (insn & (1 << 20)) ? emulate_rd12 : emulate_ird12;
	return INSN_GOOD;
}

/* Return:
 *   INSN_REJECTED     If instruction is one not allowed to kprobe,
 *   INSN_GOOD         If instruction is supported and uses instruction slot,
 *   INSN_GOOD_NO_SLOT If instruction is supported but doesn't use its slot.
 *
 * For instructions we don't want to kprobe (INSN_REJECTED return result):
 *   These are generally ones that modify the processor state making
 *   them "hard" to simulate such as switches processor modes or
 *   make accesses in alternate modes.  Any of these could be simulated
 *   if the work was put into it, but low return considering they
 *   should also be very rare.
 */
enum kprobe_insn __kprobes
arm_kprobe_decode_insn(kprobe_opcode_t insn, struct arch_specific_insn *asi)
{
	asi->insn[1] = KPROBE_RETURN_INSTRUCTION;

	if ((insn & 0xf0000000) == 0xf0000000) {

		return space_1111(insn, asi);

	} else if ((insn & 0x0e000000) == 0x00000000) {

		return space_cccc_000x(insn, asi);

	} else if ((insn & 0x0e000000) == 0x02000000) {

		return space_cccc_001x(insn, asi);

	} else if ((insn & 0x0f000010) == 0x06000010) {

		return space_cccc_0110__1(insn, asi);

	} else if ((insn & 0x0f000010) == 0x07000010) {

		return space_cccc_0111__1(insn, asi);

	} else if ((insn & 0x0c000000) == 0x04000000) {

		return space_cccc_01xx(insn, asi);

	} else if ((insn & 0x0e000000) == 0x08000000) {

		return space_cccc_100x(insn, asi);

	} else if ((insn & 0x0e000000) == 0x0a000000) {

		return space_cccc_101x(insn, asi);

	} else if ((insn & 0x0fe00000) == 0x0c400000) {

		return space_cccc_1100_010x(insn, asi);

	} else if ((insn & 0x0e000000) == 0x0c400000) {

		return space_cccc_110x(insn, asi);

	}

	return space_cccc_111x(insn, asi);
}

void __init arm_kprobe_decode_init(void)
{
	find_str_pc_offset();
}


/*
 * All ARM instructions listed below.
 *
 * Instructions and their general purpose registers are given.
 * If a particular register may not use R15, it is prefixed with a "!".
 * If marked with a "*" means the value returned by reading R15
 * is implementation defined.
 *
 * ADC/ADD/AND/BIC/CMN/CMP/EOR/MOV/MVN/ORR/RSB/RSC/SBC/SUB/TEQ
 *     TST: Rd, Rn, Rm, !Rs
 * BX: Rm
 * BLX(2): !Rm
 * BX: Rm (R15 legal, but discouraged)
 * BXJ: !Rm,
 * CLZ: !Rd, !Rm
 * CPY: Rd, Rm
 * LDC/2,STC/2 immediate offset & unindex: Rn
 * LDC/2,STC/2 immediate pre/post-indexed: !Rn
 * LDM(1/3): !Rn, register_list
 * LDM(2): !Rn, !register_list
 * LDR,STR,PLD immediate offset: Rd, Rn
 * LDR,STR,PLD register offset: Rd, Rn, !Rm
 * LDR,STR,PLD scaled register offset: Rd, !Rn, !Rm
 * LDR,STR immediate pre/post-indexed: Rd, !Rn
 * LDR,STR register pre/post-indexed: Rd, !Rn, !Rm
 * LDR,STR scaled register pre/post-indexed: Rd, !Rn, !Rm
 * LDRB,STRB immediate offset: !Rd, Rn
 * LDRB,STRB register offset: !Rd, Rn, !Rm
 * LDRB,STRB scaled register offset: !Rd, !Rn, !Rm
 * LDRB,STRB immediate pre/post-indexed: !Rd, !Rn
 * LDRB,STRB register pre/post-indexed: !Rd, !Rn, !Rm
 * LDRB,STRB scaled register pre/post-indexed: !Rd, !Rn, !Rm
 * LDRT,LDRBT,STRBT immediate pre/post-indexed: !Rd, !Rn
 * LDRT,LDRBT,STRBT register pre/post-indexed: !Rd, !Rn, !Rm
 * LDRT,LDRBT,STRBT scaled register pre/post-indexed: !Rd, !Rn, !Rm
 * LDRH/SH/SB/D,STRH/SH/SB/D immediate offset: !Rd, Rn
 * LDRH/SH/SB/D,STRH/SH/SB/D register offset: !Rd, Rn, !Rm
 * LDRH/SH/SB/D,STRH/SH/SB/D immediate pre/post-indexed: !Rd, !Rn
 * LDRH/SH/SB/D,STRH/SH/SB/D register pre/post-indexed: !Rd, !Rn, !Rm
 * LDREX: !Rd, !Rn
 * MCR/2: !Rd
 * MCRR/2,MRRC/2: !Rd, !Rn
 * MLA: !Rd, !Rn, !Rm, !Rs
 * MOV: Rd
 * MRC/2: !Rd (if Rd==15, only changes cond codes, not the register)
 * MRS,MSR: !Rd
 * MUL: !Rd, !Rm, !Rs
 * PKH{BT,TB}: !Rd, !Rn, !Rm
 * QDADD,[U]QADD/16/8/SUBX: !Rd, !Rm, !Rn
 * QDSUB,[U]QSUB/16/8/ADDX: !Rd, !Rm, !Rn
 * REV/16/SH: !Rd, !Rm
 * RFE: !Rn
 * {S,U}[H]ADD{16,8,SUBX},{S,U}[H]SUB{16,8,ADDX}: !Rd, !Rn, !Rm
 * SEL: !Rd, !Rn, !Rm
 * SMLA<x><y>,SMLA{D,W<y>},SMLSD,SMML{A,S}: !Rd, !Rn, !Rm, !Rs
 * SMLAL<x><y>,SMLA{D,LD},SMLSLD,SMMULL,SMULW<y>: !RdHi, !RdLo, !Rm, !Rs
 * SMMUL,SMUAD,SMUL<x><y>,SMUSD: !Rd, !Rm, !Rs
 * SSAT/16: !Rd, !Rm
 * STM(1/2): !Rn, register_list* (R15 in reg list not recommended)
 * STRT immediate pre/post-indexed: Rd*, !Rn
 * STRT register pre/post-indexed: Rd*, !Rn, !Rm
 * STRT scaled register pre/post-indexed: Rd*, !Rn, !Rm
 * STREX: !Rd, !Rn, !Rm
 * SWP/B: !Rd, !Rn, !Rm
 * {S,U}XTA{B,B16,H}: !Rd, !Rn, !Rm
 * {S,U}XT{B,B16,H}: !Rd, !Rm
 * UM{AA,LA,UL}L: !RdHi, !RdLo, !Rm, !Rs
 * USA{D8,A8,T,T16}: !Rd, !Rm, !Rs
 *
 * May transfer control by writing R15 (possible mode changes or alternate
 * mode accesses marked by "*"):
 * ALU op (* with s-bit), B, BL, BKPT, BLX(1/2), BX, BXJ, CPS*, CPY,
 * LDM(1), LDM(2/3)*, LDR, MOV, RFE*, SWI*
 *
 * Instructions that do not take general registers, nor transfer control:
 * CDP/2, SETEND, SRS*
 */
