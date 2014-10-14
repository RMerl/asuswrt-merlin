/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Copyright 2003 PathScale, Inc.
 * Licensed under the GPL
 */

#include <linux/stddef.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/personality.h>
#include <linux/proc_fs.h>
#include <linux/ptrace.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/tick.h>
#include <linux/threads.h>
#include <asm/current.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include "as-layout.h"
#include "kern_util.h"
#include "os.h"
#include "skas.h"
#include "tlb.h"

/*
 * This is a per-cpu array.  A processor only modifies its entry and it only
 * cares about its entry, so it's OK if another processor is modifying its
 * entry.
 */
struct cpu_task cpu_tasks[NR_CPUS] = { [0 ... NR_CPUS - 1] = { -1, NULL } };

static inline int external_pid(void)
{
	/* FIXME: Need to look up userspace_pid by cpu */
	return userspace_pid[0];
}

int pid_to_processor_id(int pid)
{
	int i;

	for (i = 0; i < ncpus; i++) {
		if (cpu_tasks[i].pid == pid)
			return i;
	}
	return -1;
}

void free_stack(unsigned long stack, int order)
{
	free_pages(stack, order);
}

unsigned long alloc_stack(int order, int atomic)
{
	unsigned long page;
	gfp_t flags = GFP_KERNEL;

	if (atomic)
		flags = GFP_ATOMIC;
	page = __get_free_pages(flags, order);

	return page;
}

int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	int pid;

	current->thread.request.u.thread.proc = fn;
	current->thread.request.u.thread.arg = arg;
	pid = do_fork(CLONE_VM | CLONE_UNTRACED | flags, 0,
		      &current->thread.regs, 0, NULL, NULL);
	return pid;
}

static inline void set_current(struct task_struct *task)
{
	cpu_tasks[task_thread_info(task)->cpu] = ((struct cpu_task)
		{ external_pid(), task });
}

extern void arch_switch_to(struct task_struct *to);

void *_switch_to(void *prev, void *next, void *last)
{
	struct task_struct *from = prev;
	struct task_struct *to = next;

	to->thread.prev_sched = from;
	set_current(to);

	do {
		current->thread.saved_task = NULL;

		switch_threads(&from->thread.switch_buf,
			       &to->thread.switch_buf);

		arch_switch_to(current);

		if (current->thread.saved_task)
			show_regs(&(current->thread.regs));
		to = current->thread.saved_task;
		from = current;
	} while (current->thread.saved_task);

	return current->thread.prev_sched;

}

void interrupt_end(void)
{
	if (need_resched())
		schedule();
	if (test_tsk_thread_flag(current, TIF_SIGPENDING))
		do_signal();
}

void exit_thread(void)
{
}

void *get_current(void)
{
	return current;
}

/*
 * This is called magically, by its address being stuffed in a jmp_buf
 * and being longjmp-d to.
 */
void new_thread_handler(void)
{
	int (*fn)(void *), n;
	void *arg;

	if (current->thread.prev_sched != NULL)
		schedule_tail(current->thread.prev_sched);
	current->thread.prev_sched = NULL;

	fn = current->thread.request.u.thread.proc;
	arg = current->thread.request.u.thread.arg;

	/*
	 * The return value is 1 if the kernel thread execs a process,
	 * 0 if it just exits
	 */
	n = run_kernel_thread(fn, arg, &current->thread.exec_buf);
	if (n == 1) {
		/* Handle any immediate reschedules or signals */
		interrupt_end();
		userspace(&current->thread.regs.regs);
	}
	else do_exit(0);
}

/* Called magically, see new_thread_handler above */
void fork_handler(void)
{
	force_flush_all();

	schedule_tail(current->thread.prev_sched);

	/*
	 * XXX: if interrupt_end() calls schedule, this call to
	 * arch_switch_to isn't needed. We could want to apply this to
	 * improve performance. -bb
	 */
	arch_switch_to(current);

	current->thread.prev_sched = NULL;

	/* Handle any immediate reschedules or signals */
	interrupt_end();

	userspace(&current->thread.regs.regs);
}

int copy_thread(unsigned long clone_flags, unsigned long sp,
		unsigned long stack_top, struct task_struct * p,
		struct pt_regs *regs)
{
	void (*handler)(void);
	int ret = 0;

	p->thread = (struct thread_struct) INIT_THREAD;

	if (current->thread.forking) {
	  	memcpy(&p->thread.regs.regs, &regs->regs,
		       sizeof(p->thread.regs.regs));
		REGS_SET_SYSCALL_RETURN(p->thread.regs.regs.gp, 0);
		if (sp != 0)
			REGS_SP(p->thread.regs.regs.gp) = sp;

		handler = fork_handler;

		arch_copy_thread(&current->thread.arch, &p->thread.arch);
	}
	else {
		get_safe_registers(p->thread.regs.regs.gp);
		p->thread.request.u.thread = current->thread.request.u.thread;
		handler = new_thread_handler;
	}

	new_thread(task_stack_page(p), &p->thread.switch_buf, handler);

	if (current->thread.forking) {
		clear_flushed_tls(p);

		/*
		 * Set a new TLS for the child thread?
		 */
		if (clone_flags & CLONE_SETTLS)
			ret = arch_copy_tls(p);
	}

	return ret;
}

void initial_thread_cb(void (*proc)(void *), void *arg)
{
	int save_kmalloc_ok = kmalloc_ok;

	kmalloc_ok = 0;
	initial_thread_cb_skas(proc, arg);
	kmalloc_ok = save_kmalloc_ok;
}

void default_idle(void)
{
	unsigned long long nsecs;

	while (1) {
		/* endless idle loop with no priority at all */

		/*
		 * although we are an idle CPU, we do not want to
		 * get into the scheduler unnecessarily.
		 */
		if (need_resched())
			schedule();

		tick_nohz_stop_sched_tick(1);
		nsecs = disable_timer();
		idle_sleep(nsecs);
		tick_nohz_restart_sched_tick();
	}
}

void cpu_idle(void)
{
	cpu_tasks[current_thread_info()->cpu].pid = os_getpid();
	default_idle();
}

int __cant_sleep(void) {
	return in_atomic() || irqs_disabled() || in_interrupt();
	/* Is in_interrupt() really needed? */
}

int user_context(unsigned long sp)
{
	unsigned long stack;

	stack = sp & (PAGE_MASK << CONFIG_KERNEL_STACK_ORDER);
	return stack != (unsigned long) current_thread_info();
}

extern exitcall_t __uml_exitcall_begin, __uml_exitcall_end;

void do_uml_exitcalls(void)
{
	exitcall_t *call;

	call = &__uml_exitcall_end;
	while (--call >= &__uml_exitcall_begin)
		(*call)();
}

char *uml_strdup(const char *string)
{
	return kstrdup(string, GFP_KERNEL);
}

int copy_to_user_proc(void __user *to, void *from, int size)
{
	return copy_to_user(to, from, size);
}

int copy_from_user_proc(void *to, void __user *from, int size)
{
	return copy_from_user(to, from, size);
}

int clear_user_proc(void __user *buf, int size)
{
	return clear_user(buf, size);
}

int strlen_user_proc(char __user *str)
{
	return strlen_user(str);
}

int smp_sigio_handler(void)
{
#ifdef CONFIG_SMP
	int cpu = current_thread_info()->cpu;
	IPI_handler(cpu);
	if (cpu != 0)
		return 1;
#endif
	return 0;
}

int cpu(void)
{
	return current_thread_info()->cpu;
}

static atomic_t using_sysemu = ATOMIC_INIT(0);
int sysemu_supported;

void set_using_sysemu(int value)
{
	if (value > sysemu_supported)
		return;
	atomic_set(&using_sysemu, value);
}

int get_using_sysemu(void)
{
	return atomic_read(&using_sysemu);
}

static int sysemu_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", get_using_sysemu());
	return 0;
}

static int sysemu_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sysemu_proc_show, NULL);
}

static ssize_t sysemu_proc_write(struct file *file, const char __user *buf,
				 size_t count, loff_t *pos)
{
	char tmp[2];

	if (copy_from_user(tmp, buf, 1))
		return -EFAULT;

	if (tmp[0] >= '0' && tmp[0] <= '2')
		set_using_sysemu(tmp[0] - '0');
	/* We use the first char, but pretend to write everything */
	return count;
}

static const struct file_operations sysemu_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= sysemu_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
	.write		= sysemu_proc_write,
};

int __init make_proc_sysemu(void)
{
	struct proc_dir_entry *ent;
	if (!sysemu_supported)
		return 0;

	ent = proc_create("sysemu", 0600, NULL, &sysemu_proc_fops);

	if (ent == NULL)
	{
		printk(KERN_WARNING "Failed to register /proc/sysemu\n");
		return 0;
	}

	return 0;
}

late_initcall(make_proc_sysemu);

int singlestepping(void * t)
{
	struct task_struct *task = t ? t : current;

	if (!(task->ptrace & PT_DTRACE))
		return 0;

	if (task->thread.singlestep_syscall)
		return 1;

	return 2;
}

/*
 * Only x86 and x86_64 have an arch_align_stack().
 * All other arches have "#define arch_align_stack(x) (x)"
 * in their asm/system.h
 * As this is included in UML from asm-um/system-generic.h,
 * we can use it to behave as the subarch does.
 */
#ifndef arch_align_stack
unsigned long arch_align_stack(unsigned long sp)
{
	if (!(current->personality & ADDR_NO_RANDOMIZE) && randomize_va_space)
		sp -= get_random_int() % 8192;
	return sp & ~0xf;
}
#endif

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long stack_page, sp, ip;
	bool seen_sched = 0;

	if ((p == NULL) || (p == current) || (p->state == TASK_RUNNING))
		return 0;

	stack_page = (unsigned long) task_stack_page(p);
	/* Bail if the process has no kernel stack for some reason */
	if (stack_page == 0)
		return 0;

	sp = p->thread.switch_buf->JB_SP;
	/*
	 * Bail if the stack pointer is below the bottom of the kernel
	 * stack for some reason
	 */
	if (sp < stack_page)
		return 0;

	while (sp < stack_page + THREAD_SIZE) {
		ip = *((unsigned long *) sp);
		if (in_sched_functions(ip))
			/* Ignore everything until we're above the scheduler */
			seen_sched = 1;
		else if (kernel_text_address(ip) && seen_sched)
			return ip;

		sp += sizeof(unsigned long);
	}

	return 0;
}

int elf_core_copy_fpregs(struct task_struct *t, elf_fpregset_t *fpu)
{
	int cpu = current_thread_info()->cpu;

	return save_fp_registers(userspace_pid[cpu], (unsigned long *) fpu);
}

