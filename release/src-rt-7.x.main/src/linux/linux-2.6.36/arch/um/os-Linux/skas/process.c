/*
 * Copyright (C) 2002- 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <asm/unistd.h>
#include "as-layout.h"
#include "chan_user.h"
#include "kern_constants.h"
#include "kern_util.h"
#include "mem.h"
#include "os.h"
#include "process.h"
#include "proc_mm.h"
#include "ptrace_user.h"
#include "registers.h"
#include "skas.h"
#include "skas_ptrace.h"
#include "user.h"
#include "sysdep/stub.h"

int is_skas_winch(int pid, int fd, void *data)
{
	if (pid != getpgrp())
		return 0;

	register_winch_irq(-1, fd, -1, data, 0);
	return 1;
}

static int ptrace_dump_regs(int pid)
{
	unsigned long regs[MAX_REG_NR];
	int i;

	if (ptrace(PTRACE_GETREGS, pid, 0, regs) < 0)
		return -errno;

	printk(UM_KERN_ERR "Stub registers -\n");
	for (i = 0; i < ARRAY_SIZE(regs); i++)
		printk(UM_KERN_ERR "\t%d - %lx\n", i, regs[i]);

	return 0;
}

/*
 * Signals that are OK to receive in the stub - we'll just continue it.
 * SIGWINCH will happen when UML is inside a detached screen.
 */
#define STUB_SIG_MASK ((1 << SIGVTALRM) | (1 << SIGWINCH))

/* Signals that the stub will finish with - anything else is an error */
#define STUB_DONE_MASK (1 << SIGTRAP)

void wait_stub_done(int pid)
{
	int n, status, err;

	while (1) {
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED | __WALL));
		if ((n < 0) || !WIFSTOPPED(status))
			goto bad_wait;

		if (((1 << WSTOPSIG(status)) & STUB_SIG_MASK) == 0)
			break;

		err = ptrace(PTRACE_CONT, pid, 0, 0);
		if (err) {
			printk(UM_KERN_ERR "wait_stub_done : continue failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}
	}

	if (((1 << WSTOPSIG(status)) & STUB_DONE_MASK) != 0)
		return;

bad_wait:
	err = ptrace_dump_regs(pid);
	if (err)
		printk(UM_KERN_ERR "Failed to get registers from stub, "
		       "errno = %d\n", -err);
	printk(UM_KERN_ERR "wait_stub_done : failed to wait for SIGTRAP, "
	       "pid = %d, n = %d, errno = %d, status = 0x%x\n", pid, n, errno,
	       status);
	fatal_sigsegv();
}

extern unsigned long current_stub_stack(void);

static void get_skas_faultinfo(int pid, struct faultinfo *fi)
{
	int err;

	if (ptrace_faultinfo) {
		err = ptrace(PTRACE_FAULTINFO, pid, 0, fi);
		if (err) {
			printk(UM_KERN_ERR "get_skas_faultinfo - "
			       "PTRACE_FAULTINFO failed, errno = %d\n", errno);
			fatal_sigsegv();
		}

		/* Special handling for i386, which has different structs */
		if (sizeof(struct ptrace_faultinfo) < sizeof(struct faultinfo))
			memset((char *)fi + sizeof(struct ptrace_faultinfo), 0,
			       sizeof(struct faultinfo) -
			       sizeof(struct ptrace_faultinfo));
	}
	else {
		unsigned long fpregs[FP_SIZE];

		err = get_fp_registers(pid, fpregs);
		if (err < 0) {
			printk(UM_KERN_ERR "save_fp_registers returned %d\n",
			       err);
			fatal_sigsegv();
		}
		err = ptrace(PTRACE_CONT, pid, 0, SIGSEGV);
		if (err) {
			printk(UM_KERN_ERR "Failed to continue stub, pid = %d, "
			       "errno = %d\n", pid, errno);
			fatal_sigsegv();
		}
		wait_stub_done(pid);

		/*
		 * faultinfo is prepared by the stub-segv-handler at start of
		 * the stub stack page. We just have to copy it.
		 */
		memcpy(fi, (void *)current_stub_stack(), sizeof(*fi));

		err = put_fp_registers(pid, fpregs);
		if (err < 0) {
			printk(UM_KERN_ERR "put_fp_registers returned %d\n",
			       err);
			fatal_sigsegv();
		}
	}
}

static void handle_segv(int pid, struct uml_pt_regs * regs)
{
	get_skas_faultinfo(pid, &regs->faultinfo);
	segv(regs->faultinfo, 0, 1, NULL);
}

/*
 * To use the same value of using_sysemu as the caller, ask it that value
 * (in local_using_sysemu
 */
static void handle_trap(int pid, struct uml_pt_regs *regs,
			int local_using_sysemu)
{
	int err, status;

	if ((UPT_IP(regs) >= STUB_START) && (UPT_IP(regs) < STUB_END))
		fatal_sigsegv();

	/* Mark this as a syscall */
	UPT_SYSCALL_NR(regs) = PT_SYSCALL_NR(regs->gp);

	if (!local_using_sysemu)
	{
		err = ptrace(PTRACE_POKEUSR, pid, PT_SYSCALL_NR_OFFSET,
			     __NR_getpid);
		if (err < 0) {
			printk(UM_KERN_ERR "handle_trap - nullifying syscall "
			       "failed, errno = %d\n", errno);
			fatal_sigsegv();
		}

		err = ptrace(PTRACE_SYSCALL, pid, 0, 0);
		if (err < 0) {
			printk(UM_KERN_ERR "handle_trap - continuing to end of "
			       "syscall failed, errno = %d\n", errno);
			fatal_sigsegv();
		}

		CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | __WALL));
		if ((err < 0) || !WIFSTOPPED(status) ||
		    (WSTOPSIG(status) != SIGTRAP + 0x80)) {
			err = ptrace_dump_regs(pid);
			if (err)
				printk(UM_KERN_ERR "Failed to get registers "
				       "from process, errno = %d\n", -err);
			printk(UM_KERN_ERR "handle_trap - failed to wait at "
			       "end of syscall, errno = %d, status = %d\n",
			       errno, status);
			fatal_sigsegv();
		}
	}

	handle_syscall(regs);
}

extern int __syscall_stub_start;

static int userspace_tramp(void *stack)
{
	void *addr;
	int err;

	ptrace(PTRACE_TRACEME, 0, 0, 0);

	signal(SIGTERM, SIG_DFL);
	signal(SIGWINCH, SIG_IGN);
	err = set_interval();
	if (err) {
		printk(UM_KERN_ERR "userspace_tramp - setting timer failed, "
		       "errno = %d\n", err);
		exit(1);
	}

	if (!proc_mm) {
		/*
		 * This has a pte, but it can't be mapped in with the usual
		 * tlb_flush mechanism because this is part of that mechanism
		 */
		int fd;
		unsigned long long offset;
		fd = phys_mapping(to_phys(&__syscall_stub_start), &offset);
		addr = mmap64((void *) STUB_CODE, UM_KERN_PAGE_SIZE,
			      PROT_EXEC, MAP_FIXED | MAP_PRIVATE, fd, offset);
		if (addr == MAP_FAILED) {
			printk(UM_KERN_ERR "mapping mmap stub at 0x%lx failed, "
			       "errno = %d\n", STUB_CODE, errno);
			exit(1);
		}

		if (stack != NULL) {
			fd = phys_mapping(to_phys(stack), &offset);
			addr = mmap((void *) STUB_DATA,
				    UM_KERN_PAGE_SIZE, PROT_READ | PROT_WRITE,
				    MAP_FIXED | MAP_SHARED, fd, offset);
			if (addr == MAP_FAILED) {
				printk(UM_KERN_ERR "mapping segfault stack "
				       "at 0x%lx failed, errno = %d\n",
				       STUB_DATA, errno);
				exit(1);
			}
		}
	}
	if (!ptrace_faultinfo && (stack != NULL)) {
		struct sigaction sa;

		unsigned long v = STUB_CODE +
				  (unsigned long) stub_segv_handler -
				  (unsigned long) &__syscall_stub_start;

		set_sigstack((void *) STUB_DATA, UM_KERN_PAGE_SIZE);
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_ONSTACK | SA_NODEFER;
		sa.sa_handler = (void *) v;
		sa.sa_restorer = NULL;
		if (sigaction(SIGSEGV, &sa, NULL) < 0) {
			printk(UM_KERN_ERR "userspace_tramp - setting SIGSEGV "
			       "handler failed - errno = %d\n", errno);
			exit(1);
		}
	}

	kill(os_getpid(), SIGSTOP);
	return 0;
}

/* Each element set once, and only accessed by a single processor anyway */
#undef NR_CPUS
#define NR_CPUS 1
int userspace_pid[NR_CPUS];

int start_userspace(unsigned long stub_stack)
{
	void *stack;
	unsigned long sp;
	int pid, status, n, flags, err;

	stack = mmap(NULL, UM_KERN_PAGE_SIZE,
		     PROT_READ | PROT_WRITE | PROT_EXEC,
		     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (stack == MAP_FAILED) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : mmap failed, "
		       "errno = %d\n", errno);
		return err;
	}

	sp = (unsigned long) stack + UM_KERN_PAGE_SIZE - sizeof(void *);

	flags = CLONE_FILES;
	if (proc_mm)
		flags |= CLONE_VM;
	else
		flags |= SIGCHLD;

	pid = clone(userspace_tramp, (void *) sp, flags, (void *) stub_stack);
	if (pid < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : clone failed, "
		       "errno = %d\n", errno);
		return err;
	}

	do {
		CATCH_EINTR(n = waitpid(pid, &status, WUNTRACED | __WALL));
		if (n < 0) {
			err = -errno;
			printk(UM_KERN_ERR "start_userspace : wait failed, "
			       "errno = %d\n", errno);
			goto out_kill;
		}
	} while (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGVTALRM));

	if (!WIFSTOPPED(status) || (WSTOPSIG(status) != SIGSTOP)) {
		err = -EINVAL;
		printk(UM_KERN_ERR "start_userspace : expected SIGSTOP, got "
		       "status = %d\n", status);
		goto out_kill;
	}

	if (ptrace(PTRACE_OLDSETOPTIONS, pid, NULL,
		   (void *) PTRACE_O_TRACESYSGOOD) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : PTRACE_OLDSETOPTIONS "
		       "failed, errno = %d\n", errno);
		goto out_kill;
	}

	if (munmap(stack, UM_KERN_PAGE_SIZE) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "start_userspace : munmap failed, "
		       "errno = %d\n", errno);
		goto out_kill;
	}

	return pid;

 out_kill:
	os_kill_ptraced_process(pid, 1);
	return err;
}

void userspace(struct uml_pt_regs *regs)
{
	struct itimerval timer;
	unsigned long long nsecs, now;
	int err, status, op, pid = userspace_pid[0];
	/* To prevent races if using_sysemu changes under us.*/
	int local_using_sysemu;

	if (getitimer(ITIMER_VIRTUAL, &timer))
		printk(UM_KERN_ERR "Failed to get itimer, errno = %d\n", errno);
	nsecs = timer.it_value.tv_sec * UM_NSEC_PER_SEC +
		timer.it_value.tv_usec * UM_NSEC_PER_USEC;
	nsecs += os_nsecs();

	while (1) {
		/*
		 * This can legitimately fail if the process loads a
		 * bogus value into a segment register.  It will
		 * segfault and PTRACE_GETREGS will read that value
		 * out of the process.  However, PTRACE_SETREGS will
		 * fail.  In this case, there is nothing to do but
		 * just kill the process.
		 */
		if (ptrace(PTRACE_SETREGS, pid, 0, regs->gp))
			fatal_sigsegv();

		/* Now we set local_using_sysemu to be used for one loop */
		local_using_sysemu = get_using_sysemu();

		op = SELECT_PTRACE_OPERATION(local_using_sysemu,
					     singlestepping(NULL));

		if (ptrace(op, pid, 0, 0)) {
			printk(UM_KERN_ERR "userspace - ptrace continue "
			       "failed, op = %d, errno = %d\n", op, errno);
			fatal_sigsegv();
		}

		CATCH_EINTR(err = waitpid(pid, &status, WUNTRACED | __WALL));
		if (err < 0) {
			printk(UM_KERN_ERR "userspace - wait failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}

		regs->is_user = 1;
		if (ptrace(PTRACE_GETREGS, pid, 0, regs->gp)) {
			printk(UM_KERN_ERR "userspace - PTRACE_GETREGS failed, "
			       "errno = %d\n", errno);
			fatal_sigsegv();
		}

		UPT_SYSCALL_NR(regs) = -1; /* Assume: It's not a syscall */

		if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
			switch (sig) {
			case SIGSEGV:
				if (PTRACE_FULL_FAULTINFO ||
				    !ptrace_faultinfo) {
					get_skas_faultinfo(pid,
							   &regs->faultinfo);
					(*sig_info[SIGSEGV])(SIGSEGV, regs);
				}
				else handle_segv(pid, regs);
				break;
			case SIGTRAP + 0x80:
			        handle_trap(pid, regs, local_using_sysemu);
				break;
			case SIGTRAP:
				relay_signal(SIGTRAP, regs);
				break;
			case SIGVTALRM:
				now = os_nsecs();
				if (now < nsecs)
					break;
				block_signals();
				(*sig_info[sig])(sig, regs);
				unblock_signals();
				nsecs = timer.it_value.tv_sec *
					UM_NSEC_PER_SEC +
					timer.it_value.tv_usec *
					UM_NSEC_PER_USEC;
				nsecs += os_nsecs();
				break;
			case SIGIO:
			case SIGILL:
			case SIGBUS:
			case SIGFPE:
			case SIGWINCH:
				block_signals();
				(*sig_info[sig])(sig, regs);
				unblock_signals();
				break;
			default:
				printk(UM_KERN_ERR "userspace - child stopped "
				       "with signal %d\n", sig);
				fatal_sigsegv();
			}
			pid = userspace_pid[0];
			interrupt_end();

			/* Avoid -ERESTARTSYS handling in host */
			if (PT_SYSCALL_NR_OFFSET != PT_SYSCALL_RET_OFFSET)
				PT_SYSCALL_NR(regs->gp) = -1;
		}
	}
}

static unsigned long thread_regs[MAX_REG_NR];

static int __init init_thread_regs(void)
{
	get_safe_registers(thread_regs);
	/* Set parent's instruction pointer to start of clone-stub */
	thread_regs[REGS_IP_INDEX] = STUB_CODE +
				(unsigned long) stub_clone_handler -
				(unsigned long) &__syscall_stub_start;
	thread_regs[REGS_SP_INDEX] = STUB_DATA + UM_KERN_PAGE_SIZE -
		sizeof(void *);
#ifdef __SIGNAL_FRAMESIZE
	thread_regs[REGS_SP_INDEX] -= __SIGNAL_FRAMESIZE;
#endif
	return 0;
}

__initcall(init_thread_regs);

int copy_context_skas0(unsigned long new_stack, int pid)
{
	struct timeval tv = { .tv_sec = 0, .tv_usec = UM_USEC_PER_SEC / UM_HZ };
	int err;
	unsigned long current_stack = current_stub_stack();
	struct stub_data *data = (struct stub_data *) current_stack;
	struct stub_data *child_data = (struct stub_data *) new_stack;
	unsigned long long new_offset;
	int new_fd = phys_mapping(to_phys((void *)new_stack), &new_offset);

	/*
	 * prepare offset and fd of child's stack as argument for parent's
	 * and child's mmap2 calls
	 */
	*data = ((struct stub_data) { .offset	= MMAP_OFFSET(new_offset),
				      .fd	= new_fd,
				      .timer    = ((struct itimerval)
					           { .it_value = tv,
						     .it_interval = tv }) });

	err = ptrace_setregs(pid, thread_regs);
	if (err < 0) {
		err = -errno;
		printk(UM_KERN_ERR "copy_context_skas0 : PTRACE_SETREGS "
		       "failed, pid = %d, errno = %d\n", pid, -err);
		return err;
	}

	/* set a well known return code for detection of child write failure */
	child_data->err = 12345678;

	/*
	 * Wait, until parent has finished its work: read child's pid from
	 * parent's stack, and check, if bad result.
	 */
	err = ptrace(PTRACE_CONT, pid, 0, 0);
	if (err) {
		err = -errno;
		printk(UM_KERN_ERR "Failed to continue new process, pid = %d, "
		       "errno = %d\n", pid, errno);
		return err;
	}

	wait_stub_done(pid);

	pid = data->err;
	if (pid < 0) {
		printk(UM_KERN_ERR "copy_context_skas0 - stub-parent reports "
		       "error %d\n", -pid);
		return pid;
	}

	/*
	 * Wait, until child has finished too: read child's result from
	 * child's stack and check it.
	 */
	wait_stub_done(pid);
	if (child_data->err != STUB_DATA) {
		printk(UM_KERN_ERR "copy_context_skas0 - stub-child reports "
		       "error %ld\n", child_data->err);
		err = child_data->err;
		goto out_kill;
	}

	if (ptrace(PTRACE_OLDSETOPTIONS, pid, NULL,
		   (void *)PTRACE_O_TRACESYSGOOD) < 0) {
		err = -errno;
		printk(UM_KERN_ERR "copy_context_skas0 : PTRACE_OLDSETOPTIONS "
		       "failed, errno = %d\n", errno);
		goto out_kill;
	}

	return pid;

 out_kill:
	os_kill_ptraced_process(pid, 1);
	return err;
}

/*
 * This is used only, if stub pages are needed, while proc_mm is
 * available. Opening /proc/mm creates a new mm_context, which lacks
 * the stub-pages. Thus, we map them using /proc/mm-fd
 */
int map_stub_pages(int fd, unsigned long code, unsigned long data,
		   unsigned long stack)
{
	struct proc_mm_op mmop;
	int n;
	unsigned long long code_offset;
	int code_fd = phys_mapping(to_phys((void *) &__syscall_stub_start),
				   &code_offset);

	mmop = ((struct proc_mm_op) { .op        = MM_MMAP,
				      .u         =
				      { .mmap    =
					{ .addr    = code,
					  .len     = UM_KERN_PAGE_SIZE,
					  .prot    = PROT_EXEC,
					  .flags   = MAP_FIXED | MAP_PRIVATE,
					  .fd      = code_fd,
					  .offset  = code_offset
	} } });
	CATCH_EINTR(n = write(fd, &mmop, sizeof(mmop)));
	if (n != sizeof(mmop)) {
		n = errno;
		printk(UM_KERN_ERR "mmap args - addr = 0x%lx, fd = %d, "
		       "offset = %llx\n", code, code_fd,
		       (unsigned long long) code_offset);
		printk(UM_KERN_ERR "map_stub_pages : /proc/mm map for code "
		       "failed, err = %d\n", n);
		return -n;
	}

	if (stack) {
		unsigned long long map_offset;
		int map_fd = phys_mapping(to_phys((void *)stack), &map_offset);
		mmop = ((struct proc_mm_op)
				{ .op        = MM_MMAP,
				  .u         =
				  { .mmap    =
				    { .addr    = data,
				      .len     = UM_KERN_PAGE_SIZE,
				      .prot    = PROT_READ | PROT_WRITE,
				      .flags   = MAP_FIXED | MAP_SHARED,
				      .fd      = map_fd,
				      .offset  = map_offset
		} } });
		CATCH_EINTR(n = write(fd, &mmop, sizeof(mmop)));
		if (n != sizeof(mmop)) {
			n = errno;
			printk(UM_KERN_ERR "map_stub_pages : /proc/mm map for "
			       "data failed, err = %d\n", n);
			return -n;
		}
	}

	return 0;
}

void new_thread(void *stack, jmp_buf *buf, void (*handler)(void))
{
	(*buf)[0].JB_IP = (unsigned long) handler;
	(*buf)[0].JB_SP = (unsigned long) stack + UM_THREAD_SIZE -
		sizeof(void *);
}

#define INIT_JMP_NEW_THREAD 0
#define INIT_JMP_CALLBACK 1
#define INIT_JMP_HALT 2
#define INIT_JMP_REBOOT 3

void switch_threads(jmp_buf *me, jmp_buf *you)
{
	if (UML_SETJMP(me) == 0)
		UML_LONGJMP(you, 1);
}

static jmp_buf initial_jmpbuf;

static void (*cb_proc)(void *arg);
static void *cb_arg;
static jmp_buf *cb_back;

int start_idle_thread(void *stack, jmp_buf *switch_buf)
{
	int n;

	set_handler(SIGWINCH, (__sighandler_t) sig_handler,
		    SA_ONSTACK | SA_RESTART, SIGUSR1, SIGIO, SIGVTALRM, -1);

	/*
	 * Can't use UML_SETJMP or UML_LONGJMP here because they save
	 * and restore signals, with the possible side-effect of
	 * trying to handle any signals which came when they were
	 * blocked, which can't be done on this stack.
	 * Signals must be blocked when jumping back here and restored
	 * after returning to the jumper.
	 */
	n = setjmp(initial_jmpbuf);
	switch (n) {
	case INIT_JMP_NEW_THREAD:
		(*switch_buf)[0].JB_IP = (unsigned long) new_thread_handler;
		(*switch_buf)[0].JB_SP = (unsigned long) stack +
			UM_THREAD_SIZE - sizeof(void *);
		break;
	case INIT_JMP_CALLBACK:
		(*cb_proc)(cb_arg);
		longjmp(*cb_back, 1);
		break;
	case INIT_JMP_HALT:
		kmalloc_ok = 0;
		return 0;
	case INIT_JMP_REBOOT:
		kmalloc_ok = 0;
		return 1;
	default:
		printk(UM_KERN_ERR "Bad sigsetjmp return in "
		       "start_idle_thread - %d\n", n);
		fatal_sigsegv();
	}
	longjmp(*switch_buf, 1);
}

void initial_thread_cb_skas(void (*proc)(void *), void *arg)
{
	jmp_buf here;

	cb_proc = proc;
	cb_arg = arg;
	cb_back = &here;

	block_signals();
	if (UML_SETJMP(&here) == 0)
		UML_LONGJMP(&initial_jmpbuf, INIT_JMP_CALLBACK);
	unblock_signals();

	cb_proc = NULL;
	cb_arg = NULL;
	cb_back = NULL;
}

void halt_skas(void)
{
	block_signals();
	UML_LONGJMP(&initial_jmpbuf, INIT_JMP_HALT);
}

void reboot_skas(void)
{
	block_signals();
	UML_LONGJMP(&initial_jmpbuf, INIT_JMP_REBOOT);
}

void __switch_mm(struct mm_id *mm_idp)
{
	int err;

	if (proc_mm) {
		err = ptrace(PTRACE_SWITCH_MM, userspace_pid[0], 0,
			     mm_idp->u.mm_fd);
		if (err) {
			printk(UM_KERN_ERR "__switch_mm - PTRACE_SWITCH_MM "
			       "failed, errno = %d\n", errno);
			fatal_sigsegv();
		}
	}
	else userspace_pid[0] = mm_idp->u.pid;
}
