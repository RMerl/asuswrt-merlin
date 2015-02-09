/*
 * Copyright 2010 Tilera Corporation. All Rights Reserved.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 *   NON INFRINGEMENT.  See the GNU General Public License for
 *   more details.
 */

#include <linux/sched.h>
#include <linux/preempt.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/elfcore.h>
#include <linux/tick.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/compat.h>
#include <linux/hardirq.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/stack.h>
#include <asm/homecache.h>
#include <asm/syscalls.h>
#ifdef CONFIG_HARDWALL
#include <asm/hardwall.h>
#endif
#include <arch/chip.h>
#include <arch/abi.h>


/*
 * Use the (x86) "idle=poll" option to prefer low latency when leaving the
 * idle loop over low power while in the idle loop, e.g. if we have
 * one thread per core and we want to get threads out of futex waits fast.
 */
static int no_idle_nap;
static int __init idle_setup(char *str)
{
	if (!str)
		return -EINVAL;

	if (!strcmp(str, "poll")) {
		pr_info("using polling idle threads.\n");
		no_idle_nap = 1;
	} else if (!strcmp(str, "halt"))
		no_idle_nap = 0;
	else
		return -1;

	return 0;
}
early_param("idle", idle_setup);

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	int cpu = smp_processor_id();


	current_thread_info()->status |= TS_POLLING;

	if (no_idle_nap) {
		while (1) {
			while (!need_resched())
				cpu_relax();
			schedule();
		}
	}

	/* endless idle loop with no priority at all */
	while (1) {
		tick_nohz_stop_sched_tick(1);
		while (!need_resched()) {
			if (cpu_is_offline(cpu))
				BUG();  /* no HOTPLUG_CPU */

			local_irq_disable();
			__get_cpu_var(irq_stat).idle_timestamp = jiffies;
			current_thread_info()->status &= ~TS_POLLING;
			/*
			 * TS_POLLING-cleared state must be visible before we
			 * test NEED_RESCHED:
			 */
			smp_mb();

			if (!need_resched())
				_cpu_idle();
			else
				local_irq_enable();
			current_thread_info()->status |= TS_POLLING;
		}
		tick_nohz_restart_sched_tick();
		preempt_enable_no_resched();
		schedule();
		preempt_disable();
	}
}

struct thread_info *alloc_thread_info(struct task_struct *task)
{
	struct page *page;
	gfp_t flags = GFP_KERNEL;

#ifdef CONFIG_DEBUG_STACK_USAGE
	flags |= __GFP_ZERO;
#endif

	page = alloc_pages(flags, THREAD_SIZE_ORDER);
	if (!page)
		return NULL;

	return (struct thread_info *)page_address(page);
}

/*
 * Free a thread_info node, and all of its derivative
 * data structures.
 */
void free_thread_info(struct thread_info *info)
{
	struct single_step_state *step_state = info->step_state;

#ifdef CONFIG_HARDWALL
	/*
	 * We free a thread_info from the context of the task that has
	 * been scheduled next, so the original task is already dead.
	 * Calling deactivate here just frees up the data structures.
	 * If the task we're freeing held the last reference to a
	 * hardwall fd, it would have been released prior to this point
	 * anyway via exit_files(), and "hardwall" would be NULL by now.
	 */
	if (info->task->thread.hardwall)
		hardwall_deactivate(info->task);
#endif

	if (step_state) {

		kfree(step_state);
	}

	free_page((unsigned long)info);
}

static void save_arch_state(struct thread_struct *t);

int copy_thread(unsigned long clone_flags, unsigned long sp,
		unsigned long stack_size,
		struct task_struct *p, struct pt_regs *regs)
{
	struct pt_regs *childregs;
	unsigned long ksp;

	/*
	 * When creating a new kernel thread we pass sp as zero.
	 * Assign it to a reasonable value now that we have the stack.
	 */
	if (sp == 0 && regs->ex1 == PL_ICS_EX1(KERNEL_PL, 0))
		sp = KSTK_TOP(p);

	/*
	 * Do not clone step state from the parent; each thread
	 * must make its own lazily.
	 */
	task_thread_info(p)->step_state = NULL;

	/*
	 * Start new thread in ret_from_fork so it schedules properly
	 * and then return from interrupt like the parent.
	 */
	p->thread.pc = (unsigned long) ret_from_fork;

	/* Save user stack top pointer so we can ID the stack vm area later. */
	p->thread.usp0 = sp;

	/* Record the pid of the process that created this one. */
	p->thread.creator_pid = current->pid;

	/*
	 * Copy the registers onto the kernel stack so the
	 * return-from-interrupt code will reload it into registers.
	 */
	childregs = task_pt_regs(p);
	*childregs = *regs;
	childregs->regs[0] = 0;         /* return value is zero */
	childregs->sp = sp;  /* override with new user stack pointer */

	/*
	 * If CLONE_SETTLS is set, set "tp" in the new task to "r4",
	 * which is passed in as arg #5 to sys_clone().
	 */
	if (clone_flags & CLONE_SETTLS)
		childregs->tp = regs->regs[4];

	/*
	 * Copy the callee-saved registers from the passed pt_regs struct
	 * into the context-switch callee-saved registers area.
	 * We have to restore the callee-saved registers since we may
	 * be cloning a userspace task with userspace register state,
	 * and we won't be unwinding the same kernel frames to restore them.
	 * Zero out the C ABI save area to mark the top of the stack.
	 */
	ksp = (unsigned long) childregs;
	ksp -= C_ABI_SAVE_AREA_SIZE;   /* interrupt-entry save area */
	((long *)ksp)[0] = ((long *)ksp)[1] = 0;
	ksp -= CALLEE_SAVED_REGS_COUNT * sizeof(unsigned long);
	memcpy((void *)ksp, &regs->regs[CALLEE_SAVED_FIRST_REG],
	       CALLEE_SAVED_REGS_COUNT * sizeof(unsigned long));
	ksp -= C_ABI_SAVE_AREA_SIZE;   /* __switch_to() save area */
	((long *)ksp)[0] = ((long *)ksp)[1] = 0;
	p->thread.ksp = ksp;

#if CHIP_HAS_TILE_DMA()
	/*
	 * No DMA in the new thread.  We model this on the fact that
	 * fork() clears the pending signals, alarms, and aio for the child.
	 */
	memset(&p->thread.tile_dma_state, 0, sizeof(struct tile_dma_state));
	memset(&p->thread.dma_async_tlb, 0, sizeof(struct async_tlb));
#endif

#if CHIP_HAS_SN_PROC()
	/* Likewise, the new thread is not running static processor code. */
	p->thread.sn_proc_running = 0;
	memset(&p->thread.sn_async_tlb, 0, sizeof(struct async_tlb));
#endif

#if CHIP_HAS_PROC_STATUS_SPR()
	/* New thread has its miscellaneous processor state bits clear. */
	p->thread.proc_status = 0;
#endif

#ifdef CONFIG_HARDWALL
	/* New thread does not own any networks. */
	p->thread.hardwall = NULL;
#endif


	/*
	 * Start the new thread with the current architecture state
	 * (user interrupt masks, etc.).
	 */
	save_arch_state(&p->thread);

	return 0;
}

/*
 * Return "current" if it looks plausible, or else a pointer to a dummy.
 * This can be helpful if we are just trying to emit a clean panic.
 */
struct task_struct *validate_current(void)
{
	static struct task_struct corrupt = { .comm = "<corrupt>" };
	struct task_struct *tsk = current;
	if (unlikely((unsigned long)tsk < PAGE_OFFSET ||
		     (void *)tsk > high_memory ||
		     ((unsigned long)tsk & (__alignof__(*tsk) - 1)) != 0)) {
		pr_err("Corrupt 'current' %p (sp %#lx)\n", tsk, stack_pointer);
		tsk = &corrupt;
	}
	return tsk;
}

/* Take and return the pointer to the previous task, for schedule_tail(). */
struct task_struct *sim_notify_fork(struct task_struct *prev)
{
	struct task_struct *tsk = current;
	__insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_OS_FORK_PARENT |
		     (tsk->thread.creator_pid << _SIM_CONTROL_OPERATOR_BITS));
	__insn_mtspr(SPR_SIM_CONTROL, SIM_CONTROL_OS_FORK |
		     (tsk->pid << _SIM_CONTROL_OPERATOR_BITS));
	return prev;
}

int dump_task_regs(struct task_struct *tsk, elf_gregset_t *regs)
{
	struct pt_regs *ptregs = task_pt_regs(tsk);
	elf_core_copy_regs(regs, ptregs);
	return 1;
}

#if CHIP_HAS_TILE_DMA()

/* Allow user processes to access the DMA SPRs */
void grant_dma_mpls(void)
{
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_0, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_0, 1);
}

/* Forbid user processes from accessing the DMA SPRs */
void restrict_dma_mpls(void)
{
	__insn_mtspr(SPR_MPL_DMA_CPL_SET_1, 1);
	__insn_mtspr(SPR_MPL_DMA_NOTIFY_SET_1, 1);
}

/* Pause the DMA engine, then save off its state registers. */
static void save_tile_dma_state(struct tile_dma_state *dma)
{
	unsigned long state = __insn_mfspr(SPR_DMA_USER_STATUS);
	unsigned long post_suspend_state;

	/* If we're running, suspend the engine. */
	if ((state & DMA_STATUS_MASK) == SPR_DMA_STATUS__RUNNING_MASK)
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__SUSPEND_MASK);

	/*
	 * Wait for the engine to idle, then save regs.  Note that we
	 * want to record the "running" bit from before suspension,
	 * and the "done" bit from after, so that we can properly
	 * distinguish a case where the user suspended the engine from
	 * the case where the kernel suspended as part of the context
	 * swap.
	 */
	do {
		post_suspend_state = __insn_mfspr(SPR_DMA_USER_STATUS);
	} while (post_suspend_state & SPR_DMA_STATUS__BUSY_MASK);

	dma->src = __insn_mfspr(SPR_DMA_SRC_ADDR);
	dma->src_chunk = __insn_mfspr(SPR_DMA_SRC_CHUNK_ADDR);
	dma->dest = __insn_mfspr(SPR_DMA_DST_ADDR);
	dma->dest_chunk = __insn_mfspr(SPR_DMA_DST_CHUNK_ADDR);
	dma->strides = __insn_mfspr(SPR_DMA_STRIDE);
	dma->chunk_size = __insn_mfspr(SPR_DMA_CHUNK_SIZE);
	dma->byte = __insn_mfspr(SPR_DMA_BYTE);
	dma->status = (state & SPR_DMA_STATUS__RUNNING_MASK) |
		(post_suspend_state & SPR_DMA_STATUS__DONE_MASK);
}

/* Restart a DMA that was running before we were context-switched out. */
static void restore_tile_dma_state(struct thread_struct *t)
{
	const struct tile_dma_state *dma = &t->tile_dma_state;

	/*
	 * The only way to restore the done bit is to run a zero
	 * length transaction.
	 */
	if ((dma->status & SPR_DMA_STATUS__DONE_MASK) &&
	    !(__insn_mfspr(SPR_DMA_USER_STATUS) & SPR_DMA_STATUS__DONE_MASK)) {
		__insn_mtspr(SPR_DMA_BYTE, 0);
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__REQUEST_MASK);
		while (__insn_mfspr(SPR_DMA_USER_STATUS) &
		       SPR_DMA_STATUS__BUSY_MASK)
			;
	}

	__insn_mtspr(SPR_DMA_SRC_ADDR, dma->src);
	__insn_mtspr(SPR_DMA_SRC_CHUNK_ADDR, dma->src_chunk);
	__insn_mtspr(SPR_DMA_DST_ADDR, dma->dest);
	__insn_mtspr(SPR_DMA_DST_CHUNK_ADDR, dma->dest_chunk);
	__insn_mtspr(SPR_DMA_STRIDE, dma->strides);
	__insn_mtspr(SPR_DMA_CHUNK_SIZE, dma->chunk_size);
	__insn_mtspr(SPR_DMA_BYTE, dma->byte);

	/*
	 * Restart the engine if we were running and not done.
	 * Clear a pending async DMA fault that we were waiting on return
	 * to user space to execute, since we expect the DMA engine
	 * to regenerate those faults for us now.  Note that we don't
	 * try to clear the TIF_ASYNC_TLB flag, since it's relatively
	 * harmless if set, and it covers both DMA and the SN processor.
	 */
	if ((dma->status & DMA_STATUS_MASK) == SPR_DMA_STATUS__RUNNING_MASK) {
		t->dma_async_tlb.fault_num = 0;
		__insn_mtspr(SPR_DMA_CTR, SPR_DMA_CTR__REQUEST_MASK);
	}
}

#endif

static void save_arch_state(struct thread_struct *t)
{
#if CHIP_HAS_SPLIT_INTR_MASK()
	t->interrupt_mask = __insn_mfspr(SPR_INTERRUPT_MASK_0_0) |
		((u64)__insn_mfspr(SPR_INTERRUPT_MASK_0_1) << 32);
#else
	t->interrupt_mask = __insn_mfspr(SPR_INTERRUPT_MASK_0);
#endif
	t->ex_context[0] = __insn_mfspr(SPR_EX_CONTEXT_0_0);
	t->ex_context[1] = __insn_mfspr(SPR_EX_CONTEXT_0_1);
	t->system_save[0] = __insn_mfspr(SPR_SYSTEM_SAVE_0_0);
	t->system_save[1] = __insn_mfspr(SPR_SYSTEM_SAVE_0_1);
	t->system_save[2] = __insn_mfspr(SPR_SYSTEM_SAVE_0_2);
	t->system_save[3] = __insn_mfspr(SPR_SYSTEM_SAVE_0_3);
	t->intctrl_0 = __insn_mfspr(SPR_INTCTRL_0_STATUS);
#if CHIP_HAS_PROC_STATUS_SPR()
	t->proc_status = __insn_mfspr(SPR_PROC_STATUS);
#endif
#if !CHIP_HAS_FIXED_INTVEC_BASE()
	t->interrupt_vector_base = __insn_mfspr(SPR_INTERRUPT_VECTOR_BASE_0);
#endif
#if CHIP_HAS_TILE_RTF_HWM()
	t->tile_rtf_hwm = __insn_mfspr(SPR_TILE_RTF_HWM);
#endif
#if CHIP_HAS_DSTREAM_PF()
	t->dstream_pf = __insn_mfspr(SPR_DSTREAM_PF);
#endif
}

static void restore_arch_state(const struct thread_struct *t)
{
#if CHIP_HAS_SPLIT_INTR_MASK()
	__insn_mtspr(SPR_INTERRUPT_MASK_0_0, (u32) t->interrupt_mask);
	__insn_mtspr(SPR_INTERRUPT_MASK_0_1, t->interrupt_mask >> 32);
#else
	__insn_mtspr(SPR_INTERRUPT_MASK_0, t->interrupt_mask);
#endif
	__insn_mtspr(SPR_EX_CONTEXT_0_0, t->ex_context[0]);
	__insn_mtspr(SPR_EX_CONTEXT_0_1, t->ex_context[1]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_0, t->system_save[0]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_1, t->system_save[1]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_2, t->system_save[2]);
	__insn_mtspr(SPR_SYSTEM_SAVE_0_3, t->system_save[3]);
	__insn_mtspr(SPR_INTCTRL_0_STATUS, t->intctrl_0);
#if CHIP_HAS_PROC_STATUS_SPR()
	__insn_mtspr(SPR_PROC_STATUS, t->proc_status);
#endif
#if !CHIP_HAS_FIXED_INTVEC_BASE()
	__insn_mtspr(SPR_INTERRUPT_VECTOR_BASE_0, t->interrupt_vector_base);
#endif
#if CHIP_HAS_TILE_RTF_HWM()
	__insn_mtspr(SPR_TILE_RTF_HWM, t->tile_rtf_hwm);
#endif
#if CHIP_HAS_DSTREAM_PF()
	__insn_mtspr(SPR_DSTREAM_PF, t->dstream_pf);
#endif
}


void _prepare_arch_switch(struct task_struct *next)
{
#if CHIP_HAS_SN_PROC()
	int snctl;
#endif
#if CHIP_HAS_TILE_DMA()
	struct tile_dma_state *dma = &current->thread.tile_dma_state;
	if (dma->enabled)
		save_tile_dma_state(dma);
#endif
#if CHIP_HAS_SN_PROC()
	/*
	 * Suspend the static network processor if it was running.
	 * We do not suspend the fabric itself, just like we don't
	 * try to suspend the UDN.
	 */
	snctl = __insn_mfspr(SPR_SNCTL);
	current->thread.sn_proc_running =
		(snctl & SPR_SNCTL__FRZPROC_MASK) == 0;
	if (current->thread.sn_proc_running)
		__insn_mtspr(SPR_SNCTL, snctl | SPR_SNCTL__FRZPROC_MASK);
#endif
}


struct task_struct *__sched _switch_to(struct task_struct *prev,
				       struct task_struct *next)
{
	/* DMA state is already saved; save off other arch state. */
	save_arch_state(&prev->thread);

#if CHIP_HAS_TILE_DMA()
	/*
	 * Restore DMA in new task if desired.
	 * Note that it is only safe to restart here since interrupts
	 * are disabled, so we can't take any DMATLB miss or access
	 * interrupts before we have finished switching stacks.
	 */
	if (next->thread.tile_dma_state.enabled) {
		restore_tile_dma_state(&next->thread);
		grant_dma_mpls();
	} else {
		restrict_dma_mpls();
	}
#endif

	/* Restore other arch state. */
	restore_arch_state(&next->thread);

#if CHIP_HAS_SN_PROC()
	/*
	 * Restart static network processor in the new process
	 * if it was running before.
	 */
	if (next->thread.sn_proc_running) {
		int snctl = __insn_mfspr(SPR_SNCTL);
		__insn_mtspr(SPR_SNCTL, snctl & ~SPR_SNCTL__FRZPROC_MASK);
	}
#endif

#ifdef CONFIG_HARDWALL
	/* Enable or disable access to the network registers appropriately. */
	if (prev->thread.hardwall != NULL) {
		if (next->thread.hardwall == NULL)
			restrict_network_mpls();
	} else if (next->thread.hardwall != NULL) {
		grant_network_mpls();
	}
#endif

	/*
	 * Switch kernel SP, PC, and callee-saved registers.
	 * In the context of the new task, return the old task pointer
	 * (i.e. the task that actually called __switch_to).
	 * Pass the value to use for SYSTEM_SAVE_1_0 when we reset our sp.
	 */
	return __switch_to(prev, next, next_current_ksp0(next));
}

long _sys_fork(struct pt_regs *regs)
{
	return do_fork(SIGCHLD, regs->sp, regs, 0, NULL, NULL);
}

long _sys_clone(unsigned long clone_flags, unsigned long newsp,
		void __user *parent_tidptr, void __user *child_tidptr,
		struct pt_regs *regs)
{
	if (!newsp)
		newsp = regs->sp;
	return do_fork(clone_flags, newsp, regs, 0,
		       parent_tidptr, child_tidptr);
}

long _sys_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->sp,
		       regs, 0, NULL, NULL);
}

/*
 * sys_execve() executes a new program.
 */
long _sys_execve(const char __user *path,
		 const char __user *const __user *argv,
		 const char __user *const __user *envp, struct pt_regs *regs)
{
	long error;
	char *filename;

	filename = getname(path);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	return error;
}

#ifdef CONFIG_COMPAT
long _compat_sys_execve(const char __user *path,
			const compat_uptr_t __user *argv,
			const compat_uptr_t __user *envp, struct pt_regs *regs)
{
	long error;
	char *filename;

	filename = getname(path);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = compat_do_execve(filename, argv, envp, regs);
	putname(filename);
out:
	return error;
}
#endif

unsigned long get_wchan(struct task_struct *p)
{
	struct KBacktraceIterator kbt;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	for (KBacktraceIterator_init(&kbt, p, NULL);
	     !KBacktraceIterator_end(&kbt);
	     KBacktraceIterator_next(&kbt)) {
		if (!in_sched_functions(kbt.it.pc))
			return kbt.it.pc;
	}

	return 0;
}

/*
 * We pass in lr as zero (cleared in kernel_thread) and the caller
 * part of the backtrace ABI on the stack also zeroed (in copy_thread)
 * so that backtraces will stop with this function.
 * Note that we don't use r0, since copy_thread() clears it.
 */
static void start_kernel_thread(int dummy, int (*fn)(int), int arg)
{
	do_exit(fn(arg));
}

/*
 * Create a kernel thread
 */
int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	struct pt_regs regs;

	memset(&regs, 0, sizeof(regs));
	regs.ex1 = PL_ICS_EX1(KERNEL_PL, 0);  /* run at kernel PL, no ICS */
	regs.pc = (long) start_kernel_thread;
	regs.flags = PT_FLAGS_CALLER_SAVES;   /* need to restore r1 and r2 */
	regs.regs[1] = (long) fn;             /* function pointer */
	regs.regs[2] = (long) arg;            /* parameter register */

	/* Ok, create the new process.. */
	return do_fork(flags | CLONE_VM | CLONE_UNTRACED, 0, &regs,
		       0, NULL, NULL);
}
EXPORT_SYMBOL(kernel_thread);

/* Flush thread state. */
void flush_thread(void)
{
	/* Nothing */
}

/*
 * Free current thread data structures etc..
 */
void exit_thread(void)
{
	/* Nothing */
}

void show_regs(struct pt_regs *regs)
{
	struct task_struct *tsk = validate_current();
	int i;

	pr_err("\n");
	pr_err(" Pid: %d, comm: %20s, CPU: %d\n",
	       tsk->pid, tsk->comm, smp_processor_id());
#ifdef __tilegx__
	for (i = 0; i < 51; i += 3)
		pr_err(" r%-2d: "REGFMT" r%-2d: "REGFMT" r%-2d: "REGFMT"\n",
		       i, regs->regs[i], i+1, regs->regs[i+1],
		       i+2, regs->regs[i+2]);
	pr_err(" r51: "REGFMT" r52: "REGFMT" tp : "REGFMT"\n",
	       regs->regs[51], regs->regs[52], regs->tp);
	pr_err(" sp : "REGFMT" lr : "REGFMT"\n", regs->sp, regs->lr);
#else
	for (i = 0; i < 52; i += 4)
		pr_err(" r%-2d: "REGFMT" r%-2d: "REGFMT
		       " r%-2d: "REGFMT" r%-2d: "REGFMT"\n",
		       i, regs->regs[i], i+1, regs->regs[i+1],
		       i+2, regs->regs[i+2], i+3, regs->regs[i+3]);
	pr_err(" r52: "REGFMT" tp : "REGFMT" sp : "REGFMT" lr : "REGFMT"\n",
	       regs->regs[52], regs->tp, regs->sp, regs->lr);
#endif
	pr_err(" pc : "REGFMT" ex1: %ld     faultnum: %ld\n",
	       regs->pc, regs->ex1, regs->faultnum);

	dump_stack_regs(regs);
}
