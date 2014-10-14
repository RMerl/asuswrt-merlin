/*
 * HW_breakpoint: a unified kernel/user-space hardware breakpoint facility,
 * using the CPU's debug registers. Derived from
 * "arch/x86/kernel/hw_breakpoint.c"
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright 2010 IBM Corporation
 * Author: K.Prasad <prasad@linux.vnet.ibm.com>
 *
 */

#include <linux/hw_breakpoint.h>
#include <linux/notifier.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/hw_breakpoint.h>
#include <asm/processor.h>
#include <asm/sstep.h>
#include <asm/uaccess.h>

/*
 * Stores the breakpoints currently in use on each breakpoint address
 * register for every cpu
 */
static DEFINE_PER_CPU(struct perf_event *, bp_per_reg);

/*
 * Returns total number of data or instruction breakpoints available.
 */
int hw_breakpoint_slots(int type)
{
	if (type == TYPE_DATA)
		return HBP_NUM;
	return 0;		/* no instruction breakpoints available */
}

/*
 * Install a perf counter breakpoint.
 *
 * We seek a free debug address register and use it for this
 * breakpoint.
 *
 * Atomic: we hold the counter->ctx->lock and we only handle variables
 * and registers local to this cpu.
 */
int arch_install_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);
	struct perf_event **slot = &__get_cpu_var(bp_per_reg);

	*slot = bp;

	/*
	 * Do not install DABR values if the instruction must be single-stepped.
	 * If so, DABR will be populated in single_step_dabr_instruction().
	 */
	if (current->thread.last_hit_ubp != bp)
		set_dabr(info->address | info->type | DABR_TRANSLATION);

	return 0;
}

/*
 * Uninstall the breakpoint contained in the given counter.
 *
 * First we search the debug address register it uses and then we disable
 * it.
 *
 * Atomic: we hold the counter->ctx->lock and we only handle variables
 * and registers local to this cpu.
 */
void arch_uninstall_hw_breakpoint(struct perf_event *bp)
{
	struct perf_event **slot = &__get_cpu_var(bp_per_reg);

	if (*slot != bp) {
		WARN_ONCE(1, "Can't find the breakpoint");
		return;
	}

	*slot = NULL;
	set_dabr(0);
}

/*
 * Perform cleanup of arch-specific counters during unregistration
 * of the perf-event
 */
void arch_unregister_hw_breakpoint(struct perf_event *bp)
{
	/*
	 * If the breakpoint is unregistered between a hw_breakpoint_handler()
	 * and the single_step_dabr_instruction(), then cleanup the breakpoint
	 * restoration variables to prevent dangling pointers.
	 */
	if (bp->ctx->task)
		bp->ctx->task->thread.last_hit_ubp = NULL;
}

/*
 * Check for virtual address in kernel space.
 */
int arch_check_bp_in_kernelspace(struct perf_event *bp)
{
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	return is_kernel_addr(info->address);
}

int arch_bp_generic_fields(int type, int *gen_bp_type)
{
	switch (type) {
	case DABR_DATA_READ:
		*gen_bp_type = HW_BREAKPOINT_R;
		break;
	case DABR_DATA_WRITE:
		*gen_bp_type = HW_BREAKPOINT_W;
		break;
	case (DABR_DATA_WRITE | DABR_DATA_READ):
		*gen_bp_type = (HW_BREAKPOINT_W | HW_BREAKPOINT_R);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Validate the arch-specific HW Breakpoint register settings
 */
int arch_validate_hwbkpt_settings(struct perf_event *bp)
{
	int ret = -EINVAL;
	struct arch_hw_breakpoint *info = counter_arch_bp(bp);

	if (!bp)
		return ret;

	switch (bp->attr.bp_type) {
	case HW_BREAKPOINT_R:
		info->type = DABR_DATA_READ;
		break;
	case HW_BREAKPOINT_W:
		info->type = DABR_DATA_WRITE;
		break;
	case HW_BREAKPOINT_R | HW_BREAKPOINT_W:
		info->type = (DABR_DATA_READ | DABR_DATA_WRITE);
		break;
	default:
		return ret;
	}

	info->address = bp->attr.bp_addr;
	info->len = bp->attr.bp_len;

	/*
	 * Since breakpoint length can be a maximum of HW_BREAKPOINT_LEN(8)
	 * and breakpoint addresses are aligned to nearest double-word
	 * HW_BREAKPOINT_ALIGN by rounding off to the lower address, the
	 * 'symbolsize' should satisfy the check below.
	 */
	if (info->len >
	    (HW_BREAKPOINT_LEN - (info->address & HW_BREAKPOINT_ALIGN)))
		return -EINVAL;
	return 0;
}

/*
 * Restores the breakpoint on the debug registers.
 * Invoke this function if it is known that the execution context is
 * about to change to cause loss of MSR_SE settings.
 */
void thread_change_pc(struct task_struct *tsk, struct pt_regs *regs)
{
	struct arch_hw_breakpoint *info;

	if (likely(!tsk->thread.last_hit_ubp))
		return;

	info = counter_arch_bp(tsk->thread.last_hit_ubp);
	regs->msr &= ~MSR_SE;
	set_dabr(info->address | info->type | DABR_TRANSLATION);
	tsk->thread.last_hit_ubp = NULL;
}

/*
 * Handle debug exception notifications.
 */
int __kprobes hw_breakpoint_handler(struct die_args *args)
{
	int rc = NOTIFY_STOP;
	struct perf_event *bp;
	struct pt_regs *regs = args->regs;
	int stepped = 1;
	struct arch_hw_breakpoint *info;
	unsigned int instr;
	unsigned long dar = regs->dar;

	/* Disable breakpoints during exception handling */
	set_dabr(0);

	/*
	 * The counter may be concurrently released but that can only
	 * occur from a call_rcu() path. We can then safely fetch
	 * the breakpoint, use its callback, touch its counter
	 * while we are in an rcu_read_lock() path.
	 */
	rcu_read_lock();

	bp = __get_cpu_var(bp_per_reg);
	if (!bp)
		goto out;
	info = counter_arch_bp(bp);

	/*
	 * Return early after invoking user-callback function without restoring
	 * DABR if the breakpoint is from ptrace which always operates in
	 * one-shot mode. The ptrace-ed process will receive the SIGTRAP signal
	 * generated in do_dabr().
	 */
	if (bp->overflow_handler == ptrace_triggered) {
		perf_bp_event(bp, regs);
		rc = NOTIFY_DONE;
		goto out;
	}

	/*
	 * Verify if dar lies within the address range occupied by the symbol
	 * being watched to filter extraneous exceptions.  If it doesn't,
	 * we still need to single-step the instruction, but we don't
	 * generate an event.
	 */
	info->extraneous_interrupt = !((bp->attr.bp_addr <= dar) &&
			(dar - bp->attr.bp_addr < bp->attr.bp_len));

	/* Do not emulate user-space instructions, instead single-step them */
	if (user_mode(regs)) {
		bp->ctx->task->thread.last_hit_ubp = bp;
		regs->msr |= MSR_SE;
		goto out;
	}

	stepped = 0;
	instr = 0;
	if (!__get_user_inatomic(instr, (unsigned int *) regs->nip))
		stepped = emulate_step(regs, instr);

	/*
	 * emulate_step() could not execute it. We've failed in reliably
	 * handling the hw-breakpoint. Unregister it and throw a warning
	 * message to let the user know about it.
	 */
	if (!stepped) {
		WARN(1, "Unable to handle hardware breakpoint. Breakpoint at "
			"0x%lx will be disabled.", info->address);
		perf_event_disable(bp);
		goto out;
	}
	/*
	 * As a policy, the callback is invoked in a 'trigger-after-execute'
	 * fashion
	 */
	if (!info->extraneous_interrupt)
		perf_bp_event(bp, regs);

	set_dabr(info->address | info->type | DABR_TRANSLATION);
out:
	rcu_read_unlock();
	return rc;
}

/*
 * Handle single-step exceptions following a DABR hit.
 */
int __kprobes single_step_dabr_instruction(struct die_args *args)
{
	struct pt_regs *regs = args->regs;
	struct perf_event *bp = NULL;
	struct arch_hw_breakpoint *bp_info;

	bp = current->thread.last_hit_ubp;
	/*
	 * Check if we are single-stepping as a result of a
	 * previous HW Breakpoint exception
	 */
	if (!bp)
		return NOTIFY_DONE;

	bp_info = counter_arch_bp(bp);

	/*
	 * We shall invoke the user-defined callback function in the single
	 * stepping handler to confirm to 'trigger-after-execute' semantics
	 */
	if (!bp_info->extraneous_interrupt)
		perf_bp_event(bp, regs);

	set_dabr(bp_info->address | bp_info->type | DABR_TRANSLATION);
	current->thread.last_hit_ubp = NULL;

	/*
	 * If the process was being single-stepped by ptrace, let the
	 * other single-step actions occur (e.g. generate SIGTRAP).
	 */
	if (test_thread_flag(TIF_SINGLESTEP))
		return NOTIFY_DONE;

	return NOTIFY_STOP;
}

/*
 * Handle debug exception notifications.
 */
int __kprobes hw_breakpoint_exceptions_notify(
		struct notifier_block *unused, unsigned long val, void *data)
{
	int ret = NOTIFY_DONE;

	switch (val) {
	case DIE_DABR_MATCH:
		ret = hw_breakpoint_handler(data);
		break;
	case DIE_SSTEP:
		ret = single_step_dabr_instruction(data);
		break;
	}

	return ret;
}

/*
 * Release the user breakpoints used by ptrace
 */
void flush_ptrace_hw_breakpoint(struct task_struct *tsk)
{
	struct thread_struct *t = &tsk->thread;

	unregister_hw_breakpoint(t->ptrace_bps[0]);
	t->ptrace_bps[0] = NULL;
}

void hw_breakpoint_pmu_read(struct perf_event *bp)
{
	/* TODO */
}
