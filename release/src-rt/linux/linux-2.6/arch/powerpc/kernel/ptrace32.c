/*
 * ptrace for 32-bit processes running on a 64-bit kernel.
 *
 *  PowerPC version
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/m68k/kernel/ptrace.c"
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * Modified by Cort Dougan (cort@hq.fsmlabs.com)
 * and Paul Mackerras (paulus@samba.org).
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/regset.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/signal.h>
#include <linux/compat.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/*
 * Here are the old "legacy" powerpc specific getregs/setregs ptrace calls,
 * we mark them as obsolete now, they will be removed in a future version
 */
static long compat_ptrace_old(struct task_struct *child, long request,
			      long addr, long data)
{
	switch (request) {
	case PPC_PTRACE_GETREGS:	/* Get GPRs 0 - 31. */
		return copy_regset_to_user(child,
					   task_user_regset_view(current), 0,
					   0, 32 * sizeof(compat_long_t),
					   compat_ptr(data));

	case PPC_PTRACE_SETREGS:	/* Set GPRs 0 - 31. */
		return copy_regset_from_user(child,
					     task_user_regset_view(current), 0,
					     0, 32 * sizeof(compat_long_t),
					     compat_ptr(data));
	}

	return -EPERM;
}

/* Macros to workout the correct index for the FPR in the thread struct */
#define FPRNUMBER(i) (((i) - PT_FPR0) >> 1)
#define FPRHALF(i) (((i) - PT_FPR0) & 1)
#define FPRINDEX(i) TS_FPRWIDTH * FPRNUMBER(i) * 2 + FPRHALF(i)
#define FPRINDEX_3264(i) (TS_FPRWIDTH * ((i) - PT_FPR0))

long compat_arch_ptrace(struct task_struct *child, compat_long_t request,
			compat_ulong_t caddr, compat_ulong_t cdata)
{
	unsigned long addr = caddr;
	unsigned long data = cdata;
	int ret;

	switch (request) {
	/*
	 * Read 4 bytes of the other process' storage
	 *  data is a pointer specifying where the user wants the
	 *	4 bytes copied into
	 *  addr is a pointer in the user's storage that contains an 8 byte
	 *	address in the other process of the 4 bytes that is to be read
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 * when I and D space are separate, these will need to be fixed.
	 */
	case PPC_PTRACE_PEEKTEXT_3264:
	case PPC_PTRACE_PEEKDATA_3264: {
		u32 tmp;
		int copied;
		u32 __user * addrOthers;

		ret = -EIO;

		/* Get the addr in the other process that we want to read */
		if (get_user(addrOthers, (u32 __user * __user *)addr) != 0)
			break;

		copied = access_process_vm(child, (u64)addrOthers, &tmp,
				sizeof(tmp), 0);
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp, (u32 __user *)data);
		break;
	}

	/* Read a register (specified by ADDR) out of the "user area" */
	case PTRACE_PEEKUSR: {
		int index;
		unsigned long tmp;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR32))
			break;

		CHECK_FULL_REGS(child->thread.regs);
		if (index < PT_FPR0) {
			tmp = ptrace_get_reg(child, index);
		} else {
			flush_fp_to_thread(child);
			/*
			 * the user space code considers the floating point
			 * to be an array of unsigned int (32 bits) - the
			 * index passed in is based on this assumption.
			 */
			tmp = ((unsigned int *)child->thread.fpr)
				[FPRINDEX(index)];
		}
		ret = put_user((unsigned int)tmp, (u32 __user *)data);
		break;
	}
  
	/*
	 * Read 4 bytes out of the other process' pt_regs area
	 *  data is a pointer specifying where the user wants the
	 *	4 bytes copied into
	 *  addr is the offset into the other process' pt_regs structure
	 *	that is to be read
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 */
	case PPC_PTRACE_PEEKUSR_3264: {
		u32 index;
		u32 reg32bits;
		u64 tmp;
		u32 numReg;
		u32 part;

		ret = -EIO;
		/* Determine which register the user wants */
		index = (u64)addr >> 2;
		numReg = index / 2;
		/* Determine which part of the register the user wants */
		if (index % 2)
			part = 1;  /* want the 2nd half of the register (right-most). */
		else
			part = 0;  /* want the 1st half of the register (left-most). */

		/* Validate the input - check to see if address is on the wrong boundary
		 * or beyond the end of the user area
		 */
		if ((addr & 3) || numReg > PT_FPSCR)
			break;

		CHECK_FULL_REGS(child->thread.regs);
		if (numReg >= PT_FPR0) {
			flush_fp_to_thread(child);
			/* get 64 bit FPR */
			tmp = ((u64 *)child->thread.fpr)
				[FPRINDEX_3264(numReg)];
		} else { /* register within PT_REGS struct */
			tmp = ptrace_get_reg(child, numReg);
		} 
		reg32bits = ((u32*)&tmp)[part];
		ret = put_user(reg32bits, (u32 __user *)data);
		break;
	}

	/*
	 * Write 4 bytes into the other process' storage
	 *  data is the 4 bytes that the user wants written
	 *  addr is a pointer in the user's storage that contains an
	 *	8 byte address in the other process where the 4 bytes
	 *	that is to be written
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 * when I and D space are separate, these will need to be fixed.
	 */
	case PPC_PTRACE_POKETEXT_3264:
	case PPC_PTRACE_POKEDATA_3264: {
		u32 tmp = data;
		u32 __user * addrOthers;

		/* Get the addr in the other process that we want to write into */
		ret = -EIO;
		if (get_user(addrOthers, (u32 __user * __user *)addr) != 0)
			break;
		ret = 0;
		if (access_process_vm(child, (u64)addrOthers, &tmp,
					sizeof(tmp), 1) == sizeof(tmp))
			break;
		ret = -EIO;
		break;
	}

	/* write the word at location addr in the USER area */
	case PTRACE_POKEUSR: {
		unsigned long index;

		ret = -EIO;
		/* convert to index and check */
		index = (unsigned long) addr >> 2;
		if ((addr & 3) || (index > PT_FPSCR32))
			break;

		CHECK_FULL_REGS(child->thread.regs);
		if (index < PT_FPR0) {
			ret = ptrace_put_reg(child, index, data);
		} else {
			flush_fp_to_thread(child);
			/*
			 * the user space code considers the floating point
			 * to be an array of unsigned int (32 bits) - the
			 * index passed in is based on this assumption.
			 */
			((unsigned int *)child->thread.fpr)
				[FPRINDEX(index)] = data;
			ret = 0;
		}
		break;
	}

	/*
	 * Write 4 bytes into the other process' pt_regs area
	 *  data is the 4 bytes that the user wants written
	 *  addr is the offset into the other process' pt_regs structure
	 *	that is to be written into
	 * (this is run in a 32-bit process looking at a 64-bit process)
	 */
	case PPC_PTRACE_POKEUSR_3264: {
		u32 index;
		u32 numReg;

		ret = -EIO;
		/* Determine which register the user wants */
		index = (u64)addr >> 2;
		numReg = index / 2;

		/*
		 * Validate the input - check to see if address is on the
		 * wrong boundary or beyond the end of the user area
		 */
		if ((addr & 3) || (numReg > PT_FPSCR))
			break;
		CHECK_FULL_REGS(child->thread.regs);
		if (numReg < PT_FPR0) {
			unsigned long freg = ptrace_get_reg(child, numReg);
			if (index % 2)
				freg = (freg & ~0xfffffffful) | (data & 0xfffffffful);
			else
				freg = (freg & 0xfffffffful) | (data << 32);
			ret = ptrace_put_reg(child, numReg, freg);
		} else {
			u64 *tmp;
			flush_fp_to_thread(child);
			/* get 64 bit FPR ... */
			tmp = &(((u64 *)child->thread.fpr)
				[FPRINDEX_3264(numReg)]);
			/* ... write the 32 bit part we want */
			((u32 *)tmp)[index % 2] = data;
			ret = 0;
		}
		break;
	}

	case PTRACE_GET_DEBUGREG: {
		ret = -EINVAL;
		/* We only support one DABR and no IABRS at the moment */
		if (addr > 0)
			break;
#ifdef CONFIG_PPC_ADV_DEBUG_REGS
		ret = put_user(child->thread.dac1, (u32 __user *)data);
#else
		ret = put_user(child->thread.dabr, (u32 __user *)data);
#endif
		break;
	}

	case PTRACE_GETREGS:	/* Get all pt_regs from the child. */
		return copy_regset_to_user(
			child, task_user_regset_view(current), 0,
			0, PT_REGS_COUNT * sizeof(compat_long_t),
			compat_ptr(data));

	case PTRACE_SETREGS:	/* Set all gp regs in the child. */
		return copy_regset_from_user(
			child, task_user_regset_view(current), 0,
			0, PT_REGS_COUNT * sizeof(compat_long_t),
			compat_ptr(data));

	case PTRACE_GETFPREGS:
	case PTRACE_SETFPREGS:
	case PTRACE_GETVRREGS:
	case PTRACE_SETVRREGS:
	case PTRACE_GETVSRREGS:
	case PTRACE_SETVSRREGS:
	case PTRACE_GETREGS64:
	case PTRACE_SETREGS64:
	case PPC_PTRACE_GETFPREGS:
	case PPC_PTRACE_SETFPREGS:
	case PTRACE_KILL:
	case PTRACE_SINGLESTEP:
	case PTRACE_DETACH:
	case PTRACE_SET_DEBUGREG:
	case PTRACE_SYSCALL:
	case PTRACE_CONT:
	case PPC_PTRACE_GETHWDBGINFO:
	case PPC_PTRACE_SETHWDEBUG:
	case PPC_PTRACE_DELHWDEBUG:
		ret = arch_ptrace(child, request, addr, data);
		break;

	/* Old reverse args ptrace callss */
	case PPC_PTRACE_GETREGS: /* Get GPRs 0 - 31. */
	case PPC_PTRACE_SETREGS: /* Set GPRs 0 - 31. */
		ret = compat_ptrace_old(child, request, addr, data);
		break;

	default:
		ret = compat_ptrace_request(child, request, addr, data);
		break;
	}

	return ret;
}
