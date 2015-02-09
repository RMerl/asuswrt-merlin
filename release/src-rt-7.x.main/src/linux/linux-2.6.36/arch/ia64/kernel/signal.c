/*
 * Architecture-specific signal handling support.
 *
 * Copyright (C) 1999-2004 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Derived from i386 and Alpha versions.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/tracehook.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/smp.h>
#include <linux/stddef.h>
#include <linux/tty.h>
#include <linux/binfmts.h>
#include <linux/unistd.h>
#include <linux/wait.h>

#include <asm/intrinsics.h>
#include <asm/uaccess.h>
#include <asm/rse.h>
#include <asm/sigcontext.h>

#include "sigframe.h"

#define DEBUG_SIG	0
#define STACK_ALIGN	16		/* minimal alignment for stack pointer */
#define _BLOCKABLE	(~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

#if _NSIG_WORDS > 1
# define PUT_SIGSET(k,u)	__copy_to_user((u)->sig, (k)->sig, sizeof(sigset_t))
# define GET_SIGSET(k,u)	__copy_from_user((k)->sig, (u)->sig, sizeof(sigset_t))
#else
# define PUT_SIGSET(k,u)	__put_user((k)->sig[0], &(u)->sig[0])
# define GET_SIGSET(k,u)	__get_user((k)->sig[0], &(u)->sig[0])
#endif

asmlinkage long
sys_sigaltstack (const stack_t __user *uss, stack_t __user *uoss, long arg2,
		 long arg3, long arg4, long arg5, long arg6, long arg7,
		 struct pt_regs regs)
{
	return do_sigaltstack(uss, uoss, regs.r12);
}

static long
restore_sigcontext (struct sigcontext __user *sc, struct sigscratch *scr)
{
	unsigned long ip, flags, nat, um, cfm, rsc;
	long err;

	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;

	/* restore scratch that always needs gets updated during signal delivery: */
	err  = __get_user(flags, &sc->sc_flags);
	err |= __get_user(nat, &sc->sc_nat);
	err |= __get_user(ip, &sc->sc_ip);			/* instruction pointer */
	err |= __get_user(cfm, &sc->sc_cfm);
	err |= __get_user(um, &sc->sc_um);			/* user mask */
	err |= __get_user(rsc, &sc->sc_ar_rsc);
	err |= __get_user(scr->pt.ar_unat, &sc->sc_ar_unat);
	err |= __get_user(scr->pt.ar_fpsr, &sc->sc_ar_fpsr);
	err |= __get_user(scr->pt.ar_pfs, &sc->sc_ar_pfs);
	err |= __get_user(scr->pt.pr, &sc->sc_pr);		/* predicates */
	err |= __get_user(scr->pt.b0, &sc->sc_br[0]);		/* b0 (rp) */
	err |= __get_user(scr->pt.b6, &sc->sc_br[6]);		/* b6 */
	err |= __copy_from_user(&scr->pt.r1, &sc->sc_gr[1], 8);	/* r1 */
	err |= __copy_from_user(&scr->pt.r8, &sc->sc_gr[8], 4*8);	/* r8-r11 */
	err |= __copy_from_user(&scr->pt.r12, &sc->sc_gr[12], 2*8);	/* r12-r13 */
	err |= __copy_from_user(&scr->pt.r15, &sc->sc_gr[15], 8);	/* r15 */

	scr->pt.cr_ifs = cfm | (1UL << 63);
	scr->pt.ar_rsc = rsc | (3 << 2); /* force PL3 */

	/* establish new instruction pointer: */
	scr->pt.cr_iip = ip & ~0x3UL;
	ia64_psr(&scr->pt)->ri = ip & 0x3;
	scr->pt.cr_ipsr = (scr->pt.cr_ipsr & ~IA64_PSR_UM) | (um & IA64_PSR_UM);

	scr->scratch_unat = ia64_put_scratch_nat_bits(&scr->pt, nat);

	if (!(flags & IA64_SC_FLAG_IN_SYSCALL)) {
		/* Restore most scratch-state only when not in syscall. */
		err |= __get_user(scr->pt.ar_ccv, &sc->sc_ar_ccv);		/* ar.ccv */
		err |= __get_user(scr->pt.b7, &sc->sc_br[7]);			/* b7 */
		err |= __get_user(scr->pt.r14, &sc->sc_gr[14]);			/* r14 */
		err |= __copy_from_user(&scr->pt.ar_csd, &sc->sc_ar25, 2*8); /* ar.csd & ar.ssd */
		err |= __copy_from_user(&scr->pt.r2, &sc->sc_gr[2], 2*8);	/* r2-r3 */
		err |= __copy_from_user(&scr->pt.r16, &sc->sc_gr[16], 16*8);	/* r16-r31 */
	}

	if ((flags & IA64_SC_FLAG_FPH_VALID) != 0) {
		struct ia64_psr *psr = ia64_psr(&scr->pt);

		err |= __copy_from_user(current->thread.fph, &sc->sc_fr[32], 96*16);
		psr->mfh = 0;	/* drop signal handler's fph contents... */
		preempt_disable();
		if (psr->dfh)
			ia64_drop_fpu(current);
		else {
			/* We already own the local fph, otherwise psr->dfh wouldn't be 0.  */
			__ia64_load_fpu(current->thread.fph);
			ia64_set_local_fpu_owner(current);
		}
		preempt_enable();
	}
	return err;
}

int
copy_siginfo_to_user (siginfo_t __user *to, siginfo_t *from)
{
	if (!access_ok(VERIFY_WRITE, to, sizeof(siginfo_t)))
		return -EFAULT;
	if (from->si_code < 0) {
		if (__copy_to_user(to, from, sizeof(siginfo_t)))
			return -EFAULT;
		return 0;
	} else {
		int err;

		/*
		 * If you change siginfo_t structure, please be sure this code is fixed
		 * accordingly.  It should never copy any pad contained in the structure
		 * to avoid security leaks, but must copy the generic 3 ints plus the
		 * relevant union member.
		 */
		err = __put_user(from->si_signo, &to->si_signo);
		err |= __put_user(from->si_errno, &to->si_errno);
		err |= __put_user((short)from->si_code, &to->si_code);
		switch (from->si_code >> 16) {
		      case __SI_FAULT >> 16:
			err |= __put_user(from->si_flags, &to->si_flags);
			err |= __put_user(from->si_isr, &to->si_isr);
		      case __SI_POLL >> 16:
			err |= __put_user(from->si_addr, &to->si_addr);
			err |= __put_user(from->si_imm, &to->si_imm);
			break;
		      case __SI_TIMER >> 16:
			err |= __put_user(from->si_tid, &to->si_tid);
			err |= __put_user(from->si_overrun, &to->si_overrun);
			err |= __put_user(from->si_ptr, &to->si_ptr);
			break;
		      case __SI_RT >> 16:	/* Not generated by the kernel as of now.  */
		      case __SI_MESGQ >> 16:
			err |= __put_user(from->si_uid, &to->si_uid);
			err |= __put_user(from->si_pid, &to->si_pid);
			err |= __put_user(from->si_ptr, &to->si_ptr);
			break;
		      case __SI_CHLD >> 16:
			err |= __put_user(from->si_utime, &to->si_utime);
			err |= __put_user(from->si_stime, &to->si_stime);
			err |= __put_user(from->si_status, &to->si_status);
		      default:
			err |= __put_user(from->si_uid, &to->si_uid);
			err |= __put_user(from->si_pid, &to->si_pid);
			break;
		}
		return err;
	}
}

long
ia64_rt_sigreturn (struct sigscratch *scr)
{
	extern char ia64_strace_leave_kernel, ia64_leave_kernel;
	struct sigcontext __user *sc;
	struct siginfo si;
	sigset_t set;
	long retval;

	sc = &((struct sigframe __user *) (scr->pt.r12 + 16))->sc;

	/*
	 * When we return to the previously executing context, r8 and r10 have already
	 * been setup the way we want them.  Indeed, if the signal wasn't delivered while
	 * in a system call, we must not touch r8 or r10 as otherwise user-level state
	 * could be corrupted.
	 */
	retval = (long) &ia64_leave_kernel;
	if (test_thread_flag(TIF_SYSCALL_TRACE)
	    || test_thread_flag(TIF_SYSCALL_AUDIT))
		/*
		 * strace expects to be notified after sigreturn returns even though the
		 * context to which we return may not be in the middle of a syscall.
		 * Thus, the return-value that strace displays for sigreturn is
		 * meaningless.
		 */
		retval = (long) &ia64_strace_leave_kernel;

	if (!access_ok(VERIFY_READ, sc, sizeof(*sc)))
		goto give_sigsegv;

	if (GET_SIGSET(&set, &sc->sc_mask))
		goto give_sigsegv;

	sigdelsetmask(&set, ~_BLOCKABLE);

	spin_lock_irq(&current->sighand->siglock);
	{
		current->blocked = set;
		recalc_sigpending();
	}
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(sc, scr))
		goto give_sigsegv;

#if DEBUG_SIG
	printk("SIG return (%s:%d): sp=%lx ip=%lx\n",
	       current->comm, current->pid, scr->pt.r12, scr->pt.cr_iip);
#endif
	/*
	 * It is more difficult to avoid calling this function than to
	 * call it and ignore errors.
	 */
	do_sigaltstack(&sc->sc_stack, NULL, scr->pt.r12);
	return retval;

  give_sigsegv:
	si.si_signo = SIGSEGV;
	si.si_errno = 0;
	si.si_code = SI_KERNEL;
	si.si_pid = task_pid_vnr(current);
	si.si_uid = current_uid();
	si.si_addr = sc;
	force_sig_info(SIGSEGV, &si, current);
	return retval;
}

/*
 * This does just the minimum required setup of sigcontext.
 * Specifically, it only installs data that is either not knowable at
 * the user-level or that gets modified before execution in the
 * trampoline starts.  Everything else is done at the user-level.
 */
static long
setup_sigcontext (struct sigcontext __user *sc, sigset_t *mask, struct sigscratch *scr)
{
	unsigned long flags = 0, ifs, cfm, nat;
	long err = 0;

	ifs = scr->pt.cr_ifs;

	if (on_sig_stack((unsigned long) sc))
		flags |= IA64_SC_FLAG_ONSTACK;
	if ((ifs & (1UL << 63)) == 0)
		/* if cr_ifs doesn't have the valid bit set, we got here through a syscall */
		flags |= IA64_SC_FLAG_IN_SYSCALL;
	cfm = ifs & ((1UL << 38) - 1);
	ia64_flush_fph(current);
	if ((current->thread.flags & IA64_THREAD_FPH_VALID)) {
		flags |= IA64_SC_FLAG_FPH_VALID;
		err = __copy_to_user(&sc->sc_fr[32], current->thread.fph, 96*16);
	}

	nat = ia64_get_scratch_nat_bits(&scr->pt, scr->scratch_unat);

	err |= __put_user(flags, &sc->sc_flags);
	err |= __put_user(nat, &sc->sc_nat);
	err |= PUT_SIGSET(mask, &sc->sc_mask);
	err |= __put_user(cfm, &sc->sc_cfm);
	err |= __put_user(scr->pt.cr_ipsr & IA64_PSR_UM, &sc->sc_um);
	err |= __put_user(scr->pt.ar_rsc, &sc->sc_ar_rsc);
	err |= __put_user(scr->pt.ar_unat, &sc->sc_ar_unat);		/* ar.unat */
	err |= __put_user(scr->pt.ar_fpsr, &sc->sc_ar_fpsr);		/* ar.fpsr */
	err |= __put_user(scr->pt.ar_pfs, &sc->sc_ar_pfs);
	err |= __put_user(scr->pt.pr, &sc->sc_pr);			/* predicates */
	err |= __put_user(scr->pt.b0, &sc->sc_br[0]);			/* b0 (rp) */
	err |= __put_user(scr->pt.b6, &sc->sc_br[6]);			/* b6 */
	err |= __copy_to_user(&sc->sc_gr[1], &scr->pt.r1, 8);		/* r1 */
	err |= __copy_to_user(&sc->sc_gr[8], &scr->pt.r8, 4*8);		/* r8-r11 */
	err |= __copy_to_user(&sc->sc_gr[12], &scr->pt.r12, 2*8);	/* r12-r13 */
	err |= __copy_to_user(&sc->sc_gr[15], &scr->pt.r15, 8);		/* r15 */
	err |= __put_user(scr->pt.cr_iip + ia64_psr(&scr->pt)->ri, &sc->sc_ip);

	if (!(flags & IA64_SC_FLAG_IN_SYSCALL)) {
		/* Copy scratch regs to sigcontext if the signal didn't interrupt a syscall. */
		err |= __put_user(scr->pt.ar_ccv, &sc->sc_ar_ccv);		/* ar.ccv */
		err |= __put_user(scr->pt.b7, &sc->sc_br[7]);			/* b7 */
		err |= __put_user(scr->pt.r14, &sc->sc_gr[14]);			/* r14 */
		err |= __copy_to_user(&sc->sc_ar25, &scr->pt.ar_csd, 2*8); /* ar.csd & ar.ssd */
		err |= __copy_to_user(&sc->sc_gr[2], &scr->pt.r2, 2*8);		/* r2-r3 */
		err |= __copy_to_user(&sc->sc_gr[16], &scr->pt.r16, 16*8);	/* r16-r31 */
	}
	return err;
}

/*
 * Check whether the register-backing store is already on the signal stack.
 */
static inline int
rbs_on_sig_stack (unsigned long bsp)
{
	return (bsp - current->sas_ss_sp < current->sas_ss_size);
}

static long
force_sigsegv_info (int sig, void __user *addr)
{
	unsigned long flags;
	struct siginfo si;

	if (sig == SIGSEGV) {
		/*
		 * Acquiring siglock around the sa_handler-update is almost
		 * certainly overkill, but this isn't a
		 * performance-critical path and I'd rather play it safe
		 * here than having to debug a nasty race if and when
		 * something changes in kernel/signal.c that would make it
		 * no longer safe to modify sa_handler without holding the
		 * lock.
		 */
		spin_lock_irqsave(&current->sighand->siglock, flags);
		current->sighand->action[sig - 1].sa.sa_handler = SIG_DFL;
		spin_unlock_irqrestore(&current->sighand->siglock, flags);
	}
	si.si_signo = SIGSEGV;
	si.si_errno = 0;
	si.si_code = SI_KERNEL;
	si.si_pid = task_pid_vnr(current);
	si.si_uid = current_uid();
	si.si_addr = addr;
	force_sig_info(SIGSEGV, &si, current);
	return 0;
}

static long
setup_frame (int sig, struct k_sigaction *ka, siginfo_t *info, sigset_t *set,
	     struct sigscratch *scr)
{
	extern char __kernel_sigtramp[];
	unsigned long tramp_addr, new_rbs = 0, new_sp;
	struct sigframe __user *frame;
	long err;

	new_sp = scr->pt.r12;
	tramp_addr = (unsigned long) __kernel_sigtramp;
	if (ka->sa.sa_flags & SA_ONSTACK) {
		int onstack = sas_ss_flags(new_sp);

		if (onstack == 0) {
			new_sp = current->sas_ss_sp + current->sas_ss_size;
			/*
			 * We need to check for the register stack being on the
			 * signal stack separately, because it's switched
			 * separately (memory stack is switched in the kernel,
			 * register stack is switched in the signal trampoline).
			 */
			if (!rbs_on_sig_stack(scr->pt.ar_bspstore))
				new_rbs = ALIGN(current->sas_ss_sp,
						sizeof(long));
		} else if (onstack == SS_ONSTACK) {
			unsigned long check_sp;

			/*
			 * If we are on the alternate signal stack and would
			 * overflow it, don't. Return an always-bogus address
			 * instead so we will die with SIGSEGV.
			 */
			check_sp = (new_sp - sizeof(*frame)) & -STACK_ALIGN;
			if (!likely(on_sig_stack(check_sp)))
				return force_sigsegv_info(sig, (void __user *)
							  check_sp);
		}
	}
	frame = (void __user *) ((new_sp - sizeof(*frame)) & -STACK_ALIGN);

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		return force_sigsegv_info(sig, frame);

	err  = __put_user(sig, &frame->arg0);
	err |= __put_user(&frame->info, &frame->arg1);
	err |= __put_user(&frame->sc, &frame->arg2);
	err |= __put_user(new_rbs, &frame->sc.sc_rbs_base);
	err |= __put_user(0, &frame->sc.sc_loadrs);	/* initialize to zero */
	err |= __put_user(ka->sa.sa_handler, &frame->handler);

	err |= copy_siginfo_to_user(&frame->info, info);

	err |= __put_user(current->sas_ss_sp, &frame->sc.sc_stack.ss_sp);
	err |= __put_user(current->sas_ss_size, &frame->sc.sc_stack.ss_size);
	err |= __put_user(sas_ss_flags(scr->pt.r12), &frame->sc.sc_stack.ss_flags);
	err |= setup_sigcontext(&frame->sc, set, scr);

	if (unlikely(err))
		return force_sigsegv_info(sig, frame);

	scr->pt.r12 = (unsigned long) frame - 16;	/* new stack pointer */
	scr->pt.ar_fpsr = FPSR_DEFAULT;			/* reset fpsr for signal handler */
	scr->pt.cr_iip = tramp_addr;
	ia64_psr(&scr->pt)->ri = 0;			/* start executing in first slot */
	ia64_psr(&scr->pt)->be = 0;			/* force little-endian byte-order */
	/*
	 * Force the interruption function mask to zero.  This has no effect when a
	 * system-call got interrupted by a signal (since, in that case, scr->pt_cr_ifs is
	 * ignored), but it has the desirable effect of making it possible to deliver a
	 * signal with an incomplete register frame (which happens when a mandatory RSE
	 * load faults).  Furthermore, it has no negative effect on the getting the user's
	 * dirty partition preserved, because that's governed by scr->pt.loadrs.
	 */
	scr->pt.cr_ifs = (1UL << 63);

	/*
	 * Note: this affects only the NaT bits of the scratch regs (the ones saved in
	 * pt_regs), which is exactly what we want.
	 */
	scr->scratch_unat = 0; /* ensure NaT bits of r12 is clear */

#if DEBUG_SIG
	printk("SIG deliver (%s:%d): sig=%d sp=%lx ip=%lx handler=%p\n",
	       current->comm, current->pid, sig, scr->pt.r12, frame->sc.sc_ip, frame->handler);
#endif
	return 1;
}

static long
handle_signal (unsigned long sig, struct k_sigaction *ka, siginfo_t *info, sigset_t *oldset,
	       struct sigscratch *scr)
{
	if (!setup_frame(sig, ka, info, oldset, scr))
		return 0;

	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked, &current->blocked, &ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked, sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	/*
	 * Let tracing know that we've done the handler setup.
	 */
	tracehook_signal_handler(sig, info, ka, &scr->pt,
				 test_thread_flag(TIF_SINGLESTEP));

	return 1;
}

/*
 * Note that `init' is a special process: it doesn't get signals it doesn't want to
 * handle.  Thus you cannot kill init even with a SIGKILL even by mistake.
 */
void
ia64_do_signal (struct sigscratch *scr, long in_syscall)
{
	struct k_sigaction ka;
	sigset_t *oldset;
	siginfo_t info;
	long restart = in_syscall;
	long errno = scr->pt.r8;

	/*
	 * In the ia64_leave_kernel code path, we want the common case to go fast, which
	 * is why we may in certain cases get here from kernel mode. Just return without
	 * doing anything if so.
	 */
	if (!user_mode(&scr->pt))
		return;

	if (current_thread_info()->status & TS_RESTORE_SIGMASK)
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	/*
	 * This only loops in the rare cases of handle_signal() failing, in which case we
	 * need to push through a forced SIGSEGV.
	 */
	while (1) {
		int signr = get_signal_to_deliver(&info, &ka, &scr->pt, NULL);

		/*
		 * get_signal_to_deliver() may have run a debugger (via notify_parent())
		 * and the debugger may have modified the state (e.g., to arrange for an
		 * inferior call), thus it's important to check for restarting _after_
		 * get_signal_to_deliver().
		 */
		if ((long) scr->pt.r10 != -1)
			/*
			 * A system calls has to be restarted only if one of the error codes
			 * ERESTARTNOHAND, ERESTARTSYS, or ERESTARTNOINTR is returned.  If r10
			 * isn't -1 then r8 doesn't hold an error code and we don't need to
			 * restart the syscall, so we can clear the "restart" flag here.
			 */
			restart = 0;

		if (signr <= 0)
			break;

		if (unlikely(restart)) {
			switch (errno) {
			      case ERESTART_RESTARTBLOCK:
			      case ERESTARTNOHAND:
				scr->pt.r8 = EINTR;
				/* note: scr->pt.r10 is already -1 */
				break;

			      case ERESTARTSYS:
				if ((ka.sa.sa_flags & SA_RESTART) == 0) {
					scr->pt.r8 = EINTR;
					/* note: scr->pt.r10 is already -1 */
					break;
				}
			      case ERESTARTNOINTR:
				ia64_decrement_ip(&scr->pt);
				restart = 0; /* don't restart twice if handle_signal() fails... */
			}
		}

		/*
		 * Whee!  Actually deliver the signal.  If the delivery failed, we need to
		 * continue to iterate in this loop so we can deliver the SIGSEGV...
		 */
		if (handle_signal(signr, &ka, &info, oldset, scr)) {
			/*
			 * A signal was successfully delivered; the saved
			 * sigmask will have been stored in the signal frame,
			 * and will be restored by sigreturn, so we can simply
			 * clear the TS_RESTORE_SIGMASK flag.
			 */
			current_thread_info()->status &= ~TS_RESTORE_SIGMASK;
			return;
		}
	}

	/* Did we come from a system call? */
	if (restart) {
		/* Restart the system call - no handlers present */
		if (errno == ERESTARTNOHAND || errno == ERESTARTSYS || errno == ERESTARTNOINTR
		    || errno == ERESTART_RESTARTBLOCK)
		{
			/*
			 * Note: the syscall number is in r15 which is saved in
			 * pt_regs so all we need to do here is adjust ip so that
			 * the "break" instruction gets re-executed.
			 */
			ia64_decrement_ip(&scr->pt);
			if (errno == ERESTART_RESTARTBLOCK)
				scr->pt.r15 = __NR_restart_syscall;
		}
	}

	/* if there's no signal to deliver, we just put the saved sigmask
	 * back */
	if (current_thread_info()->status & TS_RESTORE_SIGMASK) {
		current_thread_info()->status &= ~TS_RESTORE_SIGMASK;
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}
