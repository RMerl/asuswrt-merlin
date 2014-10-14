/*
 * Copyright 2004-2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later
 */

#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/ptrace.h>
#include <linux/tty.h>
#include <linux/personality.h>
#include <linux/binfmts.h>
#include <linux/freezer.h>
#include <linux/uaccess.h>
#include <linux/tracehook.h>

#include <asm/cacheflush.h>
#include <asm/ucontext.h>
#include <asm/fixed_code.h>
#include <asm/syscall.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

/* Location of the trace bit in SYSCFG. */
#define TRACE_BITS 0x0001

struct fdpic_func_descriptor {
	unsigned long	text;
	unsigned long	GOT;
};

struct rt_sigframe {
	int sig;
	struct siginfo *pinfo;
	void *puc;
	/* This is no longer needed by the kernel, but unfortunately userspace
	 * code expects it to be there.  */
	char retcode[8];
	struct siginfo info;
	struct ucontext uc;
};

asmlinkage int sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss)
{
	return do_sigaltstack(uss, uoss, rdusp());
}

static inline int
rt_restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc, int *pr0)
{
	unsigned long usp = 0;
	int err = 0;

	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

#define RESTORE(x) err |= __get_user(regs->x, &sc->sc_##x)

	/* restore passed registers */
	RESTORE(r0); RESTORE(r1); RESTORE(r2); RESTORE(r3);
	RESTORE(r4); RESTORE(r5); RESTORE(r6); RESTORE(r7);
	RESTORE(p0); RESTORE(p1); RESTORE(p2); RESTORE(p3);
	RESTORE(p4); RESTORE(p5);
	err |= __get_user(usp, &sc->sc_usp);
	wrusp(usp);
	RESTORE(a0w); RESTORE(a1w);
	RESTORE(a0x); RESTORE(a1x);
	RESTORE(astat);
	RESTORE(rets);
	RESTORE(pc);
	RESTORE(retx);
	RESTORE(fp);
	RESTORE(i0); RESTORE(i1); RESTORE(i2); RESTORE(i3);
	RESTORE(m0); RESTORE(m1); RESTORE(m2); RESTORE(m3);
	RESTORE(l0); RESTORE(l1); RESTORE(l2); RESTORE(l3);
	RESTORE(b0); RESTORE(b1); RESTORE(b2); RESTORE(b3);
	RESTORE(lc0); RESTORE(lc1);
	RESTORE(lt0); RESTORE(lt1);
	RESTORE(lb0); RESTORE(lb1);
	RESTORE(seqstat);

	regs->orig_p0 = -1;	/* disable syscall checks */

	*pr0 = regs->r0;
	return err;
}

asmlinkage int do_rt_sigreturn(unsigned long __unused)
{
	struct pt_regs *regs = (struct pt_regs *)__unused;
	unsigned long usp = rdusp();
	struct rt_sigframe *frame = (struct rt_sigframe *)(usp);
	sigset_t set;
	int r0;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (rt_restore_sigcontext(regs, &frame->uc.uc_mcontext, &r0))
		goto badframe;

	if (do_sigaltstack(&frame->uc.uc_stack, NULL, regs->usp) == -EFAULT)
		goto badframe;

	return r0;

 badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

static inline int rt_setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs)
{
	int err = 0;

#define SETUP(x) err |= __put_user(regs->x, &sc->sc_##x)

	SETUP(r0); SETUP(r1); SETUP(r2); SETUP(r3);
	SETUP(r4); SETUP(r5); SETUP(r6); SETUP(r7);
	SETUP(p0); SETUP(p1); SETUP(p2); SETUP(p3);
	SETUP(p4); SETUP(p5);
	err |= __put_user(rdusp(), &sc->sc_usp);
	SETUP(a0w); SETUP(a1w);
	SETUP(a0x); SETUP(a1x);
	SETUP(astat);
	SETUP(rets);
	SETUP(pc);
	SETUP(retx);
	SETUP(fp);
	SETUP(i0); SETUP(i1); SETUP(i2); SETUP(i3);
	SETUP(m0); SETUP(m1); SETUP(m2); SETUP(m3);
	SETUP(l0); SETUP(l1); SETUP(l2); SETUP(l3);
	SETUP(b0); SETUP(b1); SETUP(b2); SETUP(b3);
	SETUP(lc0); SETUP(lc1);
	SETUP(lt0); SETUP(lt1);
	SETUP(lb0); SETUP(lb1);
	SETUP(seqstat);

	return err;
}

static inline void *get_sigframe(struct k_sigaction *ka, struct pt_regs *regs,
				 size_t frame_size)
{
	unsigned long usp;

	/* Default to using normal stack.  */
	usp = rdusp();

	/* This is the X/Open sanctioned signal stack switching.  */
	if (ka->sa.sa_flags & SA_ONSTACK) {
		if (!on_sig_stack(usp))
			usp = current->sas_ss_sp + current->sas_ss_size;
	}
	return (void *)((usp - frame_size) & -8UL);
}

static int
setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t * info,
	       sigset_t * set, struct pt_regs *regs)
{
	struct rt_sigframe *frame;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	err |= __put_user((current_thread_info()->exec_domain
			   && current_thread_info()->exec_domain->signal_invmap
			   && sig < 32
			   ? current_thread_info()->exec_domain->
			   signal_invmap[sig] : sig), &frame->sig);

	err |= __put_user(&frame->info, &frame->pinfo);
	err |= __put_user(&frame->uc, &frame->puc);
	err |= copy_siginfo_to_user(&frame->info, info);

	/* Create the ucontext.  */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |=
	    __put_user((void *)current->sas_ss_sp, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(rdusp()), &frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= rt_setup_sigcontext(&frame->uc.uc_mcontext, regs);
	err |= copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	wrusp((unsigned long)frame);
	if (current->personality & FDPIC_FUNCPTRS) {
		struct fdpic_func_descriptor __user *funcptr =
			(struct fdpic_func_descriptor *) ka->sa.sa_handler;
		__get_user(regs->pc, &funcptr->text);
		__get_user(regs->p3, &funcptr->GOT);
	} else
		regs->pc = (unsigned long)ka->sa.sa_handler;
	regs->rets = SIGRETURN_STUB;

	regs->r0 = frame->sig;
	regs->r1 = (unsigned long)(&frame->info);
	regs->r2 = (unsigned long)(&frame->uc);

	return 0;

 give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
	return -EFAULT;
}

static inline void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka, int has_handler)
{
	switch (regs->r0) {
	case -ERESTARTNOHAND:
		if (!has_handler)
			goto do_restart;
		regs->r0 = -EINTR;
		break;

	case -ERESTARTSYS:
		if (has_handler && !(ka->sa.sa_flags & SA_RESTART)) {
			regs->r0 = -EINTR;
			break;
		}
		/* fallthrough */
	case -ERESTARTNOINTR:
 do_restart:
		regs->p0 = regs->orig_p0;
		regs->r0 = regs->orig_r0;
		regs->pc -= 2;
		break;

	case -ERESTART_RESTARTBLOCK:
		regs->p0 = __NR_restart_syscall;
		regs->pc -= 2;
		break;
	}
}

/*
 * OK, we're invoking a handler
 */
static int
handle_signal(int sig, siginfo_t *info, struct k_sigaction *ka,
	      sigset_t *oldset, struct pt_regs *regs)
{
	int ret;

	/* are we from a system call? to see pt_regs->orig_p0 */
	if (regs->orig_p0 >= 0)
		/* If so, check system call restarting.. */
		handle_restart(regs, ka, 1);

	/* set up the stack frame */
	ret = setup_rt_frame(sig, ka, info, oldset, regs);

	if (ret == 0) {
		spin_lock_irq(&current->sighand->siglock);
		sigorsets(&current->blocked, &current->blocked,
			  &ka->sa.sa_mask);
		if (!(ka->sa.sa_flags & SA_NODEFER))
			sigaddset(&current->blocked, sig);
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}
	return ret;
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals
 * that the kernel can handle, and then we build all the user-level signal
 * handling stack-frames in one go after that.
 */
asmlinkage void do_signal(struct pt_regs *regs)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;
	sigset_t *oldset;

	current->thread.esp0 = (unsigned long)regs;

	if (try_to_freeze())
		goto no_signal;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Whee!  Actually deliver the signal.  */
		if (handle_signal(signr, &info, &ka, oldset, regs) == 0) {
			/* a signal was successfully delivered; the saved
			 * sigmask will have been stored in the signal frame,
			 * and will be restored by sigreturn, so we can simply
			 * clear the TIF_RESTORE_SIGMASK flag */
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);

			tracehook_signal_handler(signr, &info, &ka, regs,
				test_thread_flag(TIF_SINGLESTEP));
		}

		return;
	}

 no_signal:
	/* Did we come from a system call? */
	if (regs->orig_p0 >= 0)
		/* Restart the system call - no handlers present */
		handle_restart(regs, NULL, 0);

	/* if there's no signal to deliver, we just put the saved sigmask
	 * back */
	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

/*
 * notification of userspace execution resumption
 */
asmlinkage void do_notify_resume(struct pt_regs *regs)
{
	if (test_thread_flag(TIF_SIGPENDING) || test_thread_flag(TIF_RESTORE_SIGMASK))
		do_signal(regs);

	if (test_thread_flag(TIF_NOTIFY_RESUME)) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
		tracehook_notify_resume(regs);
		if (current->replacement_session_keyring)
			key_replace_session_keyring();
	}
}

