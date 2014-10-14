/*
 *  linux/arch/arm26/kernel/ptrace.c
 *
 *  By Ross Biro 1/23/92
 * edited by Linus Torvalds
 * ARM modifications Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/signal.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/system.h>
//#include <asm/processor.h>

#include "ptrace.h"

#define REG_PC	15
#define REG_PSR 15
/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/*
 * Breakpoint SWI instruction: SWI &9F0001
 */
#define BREAKINST_ARM	0xef9f0001

/*
 * this routine will get a word off of the processes privileged stack.
 * the offset is how far from the base addr as stored in the THREAD.
 * this routine assumes that all the privileged stacks are in our
 * data space.
 */
static inline long get_user_reg(struct task_struct *task, int offset)
{
	return task_pt_regs(task)->uregs[offset];
}

/*
 * this routine will put a word on the processes privileged stack.
 * the offset is how far from the base addr as stored in the THREAD.
 * this routine assumes that all the privileged stacks are in our
 * data space.
 */
static inline int
put_user_reg(struct task_struct *task, int offset, long data)
{
	struct pt_regs newregs, *regs = task_pt_regs(task);
	int ret = -EINVAL;

	newregs = *regs;
	newregs.uregs[offset] = data;

	if (valid_user_regs(&newregs)) {
		regs->uregs[offset] = data;
		ret = 0;
	}

	return ret;
}

static inline int
read_u32(struct task_struct *task, unsigned long addr, u32 *res)
{
	int ret;

	ret = access_process_vm(task, addr, res, sizeof(*res), 0);

	return ret == sizeof(*res) ? 0 : -EIO;
}

static inline int
read_instr(struct task_struct *task, unsigned long addr, u32 *res)
{
	int ret;
	u32 val;
	ret = access_process_vm(task, addr & ~3, &val, sizeof(val), 0);
	ret = ret == sizeof(val) ? 0 : -EIO;
	*res = val;
	return ret;
}

/*
 * Get value of register `rn' (in the instruction)
 */
static unsigned long
ptrace_getrn(struct task_struct *child, unsigned long insn)
{
	unsigned int reg = (insn >> 16) & 15;
	unsigned long val;

	val = get_user_reg(child, reg);
	if (reg == 15)
		val = pc_pointer(val + 8); //FIXME - correct for arm26?

	return val;
}

/*
 * Get value of operand 2 (in an ALU instruction)
 */
static unsigned long
ptrace_getaluop2(struct task_struct *child, unsigned long insn)
{
	unsigned long val;
	int shift;
	int type;

	if (insn & 1 << 25) {
		val = insn & 255;
		shift = (insn >> 8) & 15;
		type = 3;
	} else {
		val = get_user_reg (child, insn & 15);

		if (insn & (1 << 4))
			shift = (int)get_user_reg (child, (insn >> 8) & 15);
		else
			shift = (insn >> 7) & 31;

		type = (insn >> 5) & 3;
	}

	switch (type) {
	case 0:	val <<= shift;	break;
	case 1:	val >>= shift;	break;
	case 2:
		val = (((signed long)val) >> shift);
		break;
	case 3:
 		val = (val >> shift) | (val << (32 - shift));
		break;
	}
	return val;
}

/*
 * Get value of operand 2 (in a LDR instruction)
 */
static unsigned long
ptrace_getldrop2(struct task_struct *child, unsigned long insn)
{
	unsigned long val;
	int shift;
	int type;

	val = get_user_reg(child, insn & 15);
	shift = (insn >> 7) & 31;
	type = (insn >> 5) & 3;

	switch (type) {
	case 0:	val <<= shift;	break;
	case 1:	val >>= shift;	break;
	case 2:
		val = (((signed long)val) >> shift);
		break;
	case 3:
 		val = (val >> shift) | (val << (32 - shift));
		break;
	}
	return val;
}

#define OP_MASK	0x01e00000
#define OP_AND	0x00000000
#define OP_EOR	0x00200000
#define OP_SUB	0x00400000
#define OP_RSB	0x00600000
#define OP_ADD	0x00800000
#define OP_ADC	0x00a00000
#define OP_SBC	0x00c00000
#define OP_RSC	0x00e00000
#define OP_ORR	0x01800000
#define OP_MOV	0x01a00000
#define OP_BIC	0x01c00000
#define OP_MVN	0x01e00000

static unsigned long
get_branch_address(struct task_struct *child, unsigned long pc, unsigned long insn)
{
	u32 alt = 0;

	switch (insn & 0x0e000000) {
	case 0x00000000:
	case 0x02000000: {
		/*
		 * data processing
		 */
		long aluop1, aluop2, ccbit;

		if ((insn & 0xf000) != 0xf000)
			break;

		aluop1 = ptrace_getrn(child, insn);
		aluop2 = ptrace_getaluop2(child, insn);
		ccbit  = get_user_reg(child, REG_PSR) & PSR_C_BIT ? 1 : 0;

		switch (insn & OP_MASK) {
		case OP_AND: alt = aluop1 & aluop2;		break;
		case OP_EOR: alt = aluop1 ^ aluop2;		break;
		case OP_SUB: alt = aluop1 - aluop2;		break;
		case OP_RSB: alt = aluop2 - aluop1;		break;
		case OP_ADD: alt = aluop1 + aluop2;		break;
		case OP_ADC: alt = aluop1 + aluop2 + ccbit;	break;
		case OP_SBC: alt = aluop1 - aluop2 + ccbit;	break;
		case OP_RSC: alt = aluop2 - aluop1 + ccbit;	break;
		case OP_ORR: alt = aluop1 | aluop2;		break;
		case OP_MOV: alt = aluop2;			break;
		case OP_BIC: alt = aluop1 & ~aluop2;		break;
		case OP_MVN: alt = ~aluop2;			break;
		}
		break;
	}

	case 0x04000000:
	case 0x06000000:
		/*
		 * ldr
		 */
		if ((insn & 0x0010f000) == 0x0010f000) {
			unsigned long base;

			base = ptrace_getrn(child, insn);
			if (insn & 1 << 24) {
				long aluop2;

				if (insn & 0x02000000)
					aluop2 = ptrace_getldrop2(child, insn);
				else
					aluop2 = insn & 0xfff;

				if (insn & 1 << 23)
					base += aluop2;
				else
					base -= aluop2;
			}
			if (read_u32(child, base, &alt) == 0)
				alt = pc_pointer(alt);
		}
		break;

	case 0x08000000:
		/*
		 * ldm
		 */
		if ((insn & 0x00108000) == 0x00108000) {
			unsigned long base;
			unsigned int nr_regs;

			if (insn & (1 << 23)) {
				nr_regs = hweight16(insn & 65535) << 2;

				if (!(insn & (1 << 24)))
					nr_regs -= 4;
			} else {
				if (insn & (1 << 24))
					nr_regs = -4;
				else
					nr_regs = 0;
			}

			base = ptrace_getrn(child, insn);

			if (read_u32(child, base + nr_regs, &alt) == 0)
				alt = pc_pointer(alt);
			break;
		}
		break;

	case 0x0a000000: {
		/*
		 * bl or b
		 */
		signed long displ;
		/* It's a branch/branch link: instead of trying to
		 * figure out whether the branch will be taken or not,
		 * we'll put a breakpoint at both locations.  This is
		 * simpler, more reliable, and probably not a whole lot
		 * slower than the alternative approach of emulating the
		 * branch.
		 */
		displ = (insn & 0x00ffffff) << 8;
		displ = (displ >> 6) + 8;
		if (displ != 0 && displ != 4)
			alt = pc + displ;
	    }
	    break;
	}

	return alt;
}

static int
swap_insn(struct task_struct *task, unsigned long addr,
	  void *old_insn, void *new_insn, int size)
{
	int ret;

	ret = access_process_vm(task, addr, old_insn, size, 0);
	if (ret == size)
		ret = access_process_vm(task, addr, new_insn, size, 1);
	return ret;
}

static void
add_breakpoint(struct task_struct *task, struct debug_info *dbg, unsigned long addr)
{
	int nr = dbg->nsaved;

	if (nr < 2) {
		u32 new_insn = BREAKINST_ARM;
		int res;

		res = swap_insn(task, addr, &dbg->bp[nr].insn, &new_insn, 4);

		if (res == 4) {
			dbg->bp[nr].address = addr;
			dbg->nsaved += 1;
		}
	} else
		printk(KERN_ERR "ptrace: too many breakpoints\n");
}

/*
 * Clear one breakpoint in the user program.  We copy what the hardware
 * does and use bit 0 of the address to indicate whether this is a Thumb
 * breakpoint or an ARM breakpoint.
 */
static void clear_breakpoint(struct task_struct *task, struct debug_entry *bp)
{
	unsigned long addr = bp->address;
	u32 old_insn;
	int ret;

	ret = swap_insn(task, addr & ~3, &old_insn,
			&bp->insn, 4);

	if (ret != 4 || old_insn != BREAKINST_ARM)
		printk(KERN_ERR "%s:%d: corrupted ARM breakpoint at "
			"0x%08lx (0x%08x)\n", task->comm, task->pid,
			addr, old_insn);
}

void ptrace_set_bpt(struct task_struct *child)
{
	struct pt_regs *regs;
	unsigned long pc;
	u32 insn;
	int res;

	regs = task_pt_regs(child);
	pc = instruction_pointer(regs);

	res = read_instr(child, pc, &insn);
	if (!res) {
		struct debug_info *dbg = &child->thread.debug;
		unsigned long alt;

		dbg->nsaved = 0;

		alt = get_branch_address(child, pc, insn);
		if (alt)
			add_breakpoint(child, dbg, alt);

		/*
		 * Note that we ignore the result of setting the above
		 * breakpoint since it may fail.  When it does, this is
		 * not so much an error, but a forewarning that we may
		 * be receiving a prefetch abort shortly.
		 *
		 * If we don't set this breakpoint here, then we can
		 * lose control of the thread during single stepping.
		 */
		if (!alt || predicate(insn) != PREDICATE_ALWAYS)
			add_breakpoint(child, dbg, pc + 4);
	}
}

/*
 * Ensure no single-step breakpoint is pending.  Returns non-zero
 * value if child was being single-stepped.
 */
void ptrace_cancel_bpt(struct task_struct *child)
{
	int i, nsaved = child->thread.debug.nsaved;

	child->thread.debug.nsaved = 0;

	if (nsaved > 2) {
		printk("ptrace_cancel_bpt: bogus nsaved: %d!\n", nsaved);
		nsaved = 2;
	}

	for (i = 0; i < nsaved; i++)
		clear_breakpoint(child, &child->thread.debug.bp[i]);
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child)
{
	child->ptrace &= ~PT_SINGLESTEP;
	ptrace_cancel_bpt(child);
}

/*
 * Handle hitting a breakpoint.
 */
void ptrace_break(struct task_struct *tsk, struct pt_regs *regs)
{
	siginfo_t info;

	/*
	 * The PC is always left pointing at the next instruction.  Fix this.
	 */
	regs->ARM_pc -= 4;

	if (tsk->thread.debug.nsaved == 0)
		printk(KERN_ERR "ptrace: bogus breakpoint trap\n");

	ptrace_cancel_bpt(tsk);

	info.si_signo = SIGTRAP;
	info.si_errno = 0;
	info.si_code  = TRAP_BRKPT;
	info.si_addr  = (void *)instruction_pointer(regs) - 4;

	force_sig_info(SIGTRAP, &info, tsk);
}

/*
 * Read the word at offset "off" into the "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_read_user(struct task_struct *tsk, unsigned long off,
			    unsigned long *ret)
{
	unsigned long tmp;

	if (off & 3 || off >= sizeof(struct user))
		return -EIO;

	tmp = 0;
	if (off < sizeof(struct pt_regs))
		tmp = get_user_reg(tsk, off >> 2);

	return put_user(tmp, ret);
}

/*
 * Write the word at offset "off" into "struct user".  We
 * actually access the pt_regs stored on the kernel stack.
 */
static int ptrace_write_user(struct task_struct *tsk, unsigned long off,
			     unsigned long val)
{
	if (off & 3 || off >= sizeof(struct user))
		return -EIO;

	if (off >= sizeof(struct pt_regs))
		return 0;

	return put_user_reg(tsk, off >> 2, val);
}

/*
 * Get all user integer registers.
 */
static int ptrace_getregs(struct task_struct *tsk, void *uregs)
{
	struct pt_regs *regs = task_pt_regs(tsk);

	return copy_to_user(uregs, regs, sizeof(struct pt_regs)) ? -EFAULT : 0;
}

/*
 * Set all user integer registers.
 */
static int ptrace_setregs(struct task_struct *tsk, void *uregs)
{
	struct pt_regs newregs;
	int ret;

	ret = -EFAULT;
	if (copy_from_user(&newregs, uregs, sizeof(struct pt_regs)) == 0) {
		struct pt_regs *regs = task_pt_regs(tsk);

		ret = -EINVAL;
		if (valid_user_regs(&newregs)) {
			*regs = newregs;
			ret = 0;
		}
	}

	return ret;
}

/*
 * Get the child FPU state.
 */
static int ptrace_getfpregs(struct task_struct *tsk, void *ufp)
{
	return copy_to_user(ufp, &task_thread_info(tsk)->fpstate,
			    sizeof(struct user_fp)) ? -EFAULT : 0;
}

/*
 * Set the child FPU state.
 */
static int ptrace_setfpregs(struct task_struct *tsk, void *ufp)
{
	set_stopped_child_used_math(tsk);
	return copy_from_user(&task_thread_info(tsk)->fpstate, ufp,
			      sizeof(struct user_fp)) ? -EFAULT : 0;
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
	unsigned long tmp;
	int ret;

	switch (request) {
		/*
		 * read word at location "addr" in the child process.
		 */
		case PTRACE_PEEKTEXT:
		case PTRACE_PEEKDATA:
			ret = access_process_vm(child, addr, &tmp,
						sizeof(unsigned long), 0);
			if (ret == sizeof(unsigned long))
				ret = put_user(tmp, (unsigned long *) data);
			else
				ret = -EIO;
			break;

		case PTRACE_PEEKUSR:
			ret = ptrace_read_user(child, addr, (unsigned long *)data);
			break;

		/*
		 * write the word at location addr.
		 */
		case PTRACE_POKETEXT:
		case PTRACE_POKEDATA:
			ret = access_process_vm(child, addr, &data,
						sizeof(unsigned long), 1);
			if (ret == sizeof(unsigned long))
				ret = 0;
			else
				ret = -EIO;
			break;

		case PTRACE_POKEUSR:
			ret = ptrace_write_user(child, addr, data);
			break;

		/*
		 * continue/restart and stop at next (return from) syscall
		 */
		case PTRACE_SYSCALL:
		case PTRACE_CONT:
			ret = -EIO;
			if (!valid_signal(data))
				break;
			if (request == PTRACE_SYSCALL)
				set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
			else
				clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
			child->exit_code = data;
			/* make sure single-step breakpoint is gone. */
			child->ptrace &= ~PT_SINGLESTEP;
			ptrace_cancel_bpt(child);
			wake_up_process(child);
			ret = 0;
			break;

		/*
		 * make the child exit.  Best I can do is send it a sigkill.
		 * perhaps it should be put in the status that it wants to
		 * exit.
		 */
		case PTRACE_KILL:
			/* make sure single-step breakpoint is gone. */
			child->ptrace &= ~PT_SINGLESTEP;
			ptrace_cancel_bpt(child);
			if (child->exit_state != EXIT_ZOMBIE) {
				child->exit_code = SIGKILL;
				wake_up_process(child);
			}
			ret = 0;
			break;

		/*
		 * execute single instruction.
		 */
		case PTRACE_SINGLESTEP:
			ret = -EIO;
			if (!valid_signal(data))
				break;
			child->ptrace |= PT_SINGLESTEP;
			clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
			child->exit_code = data;
			/* give it a chance to run. */
			wake_up_process(child);
			ret = 0;
			break;

		case PTRACE_DETACH:
			ret = ptrace_detach(child, data);
			break;

		case PTRACE_GETREGS:
			ret = ptrace_getregs(child, (void *)data);
			break;

		case PTRACE_SETREGS:
			ret = ptrace_setregs(child, (void *)data);
			break;

		case PTRACE_GETFPREGS:
			ret = ptrace_getfpregs(child, (void *)data);
			break;
		
		case PTRACE_SETFPREGS:
			ret = ptrace_setfpregs(child, (void *)data);
			break;

		default:
			ret = ptrace_request(child, request, addr, data);
			break;
	}

	return ret;
}

asmlinkage void syscall_trace(int why, struct pt_regs *regs)
{
	unsigned long ip;

	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;
	if (!(current->ptrace & PT_PTRACED))
		return;

	/*
	 * Save IP.  IP is used to denote syscall entry/exit:
	 *  IP = 0 -> entry, = 1 -> exit
	 */
	ip = regs->ARM_ip;
	regs->ARM_ip = why;

	/* the 0x80 provides a way for the tracing parent to distinguish
	   between a syscall stop and SIGTRAP delivery */
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
	regs->ARM_ip = ip;
}
