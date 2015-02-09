/*
 * arch/score/kernel/signal.c
 *
 * Score Processor version.
 *
 * Copyright (C) 2009 Sunplus Core Technology Co., Ltd.
 *  Chen Liqin <liqin.chen@sunplusct.com>
 *  Lennox Wu <lennox.wu@sunplusct.com>
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
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>

#include <asm/cacheflush.h>
#include <asm/syscalls.h>
#include <asm/ucontext.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

struct rt_sigframe {
	u32 rs_ass[4];		/* argument save space */
	u32 rs_code[2];		/* signal trampoline */
	struct siginfo rs_info;
	struct ucontext rs_uc;
};

static int setup_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc)
{
	int err = 0;
	unsigned long reg;

	reg = regs->cp0_epc; err |= __put_user(reg, &sc->sc_pc);
	err |= __put_user(regs->cp0_psr, &sc->sc_psr);
	err |= __put_user(regs->cp0_condition, &sc->sc_condition);


#define save_gp_reg(i) {				\
	reg = regs->regs[i];				\
	err |= __put_user(reg, &sc->sc_regs[i]);	\
} while (0)
	save_gp_reg(0); save_gp_reg(1); save_gp_reg(2);
	save_gp_reg(3); save_gp_reg(4); save_gp_reg(5);
	save_gp_reg(6);	save_gp_reg(7); save_gp_reg(8);
	save_gp_reg(9); save_gp_reg(10); save_gp_reg(11);
	save_gp_reg(12); save_gp_reg(13); save_gp_reg(14);
	save_gp_reg(15); save_gp_reg(16); save_gp_reg(17);
	save_gp_reg(18); save_gp_reg(19); save_gp_reg(20);
	save_gp_reg(21); save_gp_reg(22); save_gp_reg(23);
	save_gp_reg(24); save_gp_reg(25); save_gp_reg(26);
	save_gp_reg(27); save_gp_reg(28); save_gp_reg(29);
#undef save_gp_reg

	reg = regs->ceh; err |= __put_user(reg, &sc->sc_mdceh);
	reg = regs->cel; err |= __put_user(reg, &sc->sc_mdcel);
	err |= __put_user(regs->cp0_ecr, &sc->sc_ecr);
	err |= __put_user(regs->cp0_ema, &sc->sc_ema);

	return err;
}

static int restore_sigcontext(struct pt_regs *regs, struct sigcontext __user *sc)
{
	int err = 0;
	u32 reg;

	err |= __get_user(regs->cp0_epc, &sc->sc_pc);
	err |= __get_user(regs->cp0_condition, &sc->sc_condition);

	err |= __get_user(reg, &sc->sc_mdceh);
	regs->ceh = (int) reg;
	err |= __get_user(reg, &sc->sc_mdcel);
	regs->cel = (int) reg;

	err |= __get_user(reg, &sc->sc_psr);
	regs->cp0_psr = (int) reg;
	err |= __get_user(reg, &sc->sc_ecr);
	regs->cp0_ecr = (int) reg;
	err |= __get_user(reg, &sc->sc_ema);
	regs->cp0_ema = (int) reg;

#define restore_gp_reg(i) do {				\
	err |= __get_user(reg, &sc->sc_regs[i]);	\
	regs->regs[i] = reg;				\
} while (0)
	restore_gp_reg(0); restore_gp_reg(1); restore_gp_reg(2);
	restore_gp_reg(3); restore_gp_reg(4); restore_gp_reg(5);
	restore_gp_reg(6); restore_gp_reg(7); restore_gp_reg(8);
	restore_gp_reg(9); restore_gp_reg(10); restore_gp_reg(11);
	restore_gp_reg(12); restore_gp_reg(13); restore_gp_reg(14);
	restore_gp_reg(15); restore_gp_reg(16); restore_gp_reg(17);
	restore_gp_reg(18); restore_gp_reg(19);	restore_gp_reg(20);
	restore_gp_reg(21); restore_gp_reg(22); restore_gp_reg(23);
	restore_gp_reg(24); restore_gp_reg(25); restore_gp_reg(26);
	restore_gp_reg(27); restore_gp_reg(28); restore_gp_reg(29);
#undef restore_gp_reg

	return err;
}

/*
 * Determine which stack to use..
 */
static void __user *get_sigframe(struct k_sigaction *ka,
			struct pt_regs *regs, size_t frame_size)
{
	unsigned long sp;

	/* Default to using normal stack */
	sp = regs->regs[0];
	sp -= 32;

	/* This is the X/Open sanctioned signal stack switching.  */
	if ((ka->sa.sa_flags & SA_ONSTACK) && (!on_sig_stack(sp)))
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void __user*)((sp - frame_size) & ~7);
}

asmlinkage long
score_sigaltstack(struct pt_regs *regs)
{
	const stack_t __user *uss = (const stack_t __user *) regs->regs[4];
	stack_t __user *uoss = (stack_t __user *) regs->regs[5];
	unsigned long usp = regs->regs[0];

	return do_sigaltstack(uss, uoss, usp);
}

asmlinkage long
score_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	sigset_t set;
	stack_t st;
	int sig;

	frame = (struct rt_sigframe __user *) regs->regs[0];
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->rs_uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	sig = restore_sigcontext(regs, &frame->rs_uc.uc_mcontext);
	if (sig < 0)
		goto badframe;
	else if (sig)
		force_sig(sig, current);

	if (__copy_from_user(&st, &frame->rs_uc.uc_stack, sizeof(st)))
		goto badframe;

	/* It is more difficult to avoid calling this function than to
	   call it and ignore errors.  */
	do_sigaltstack((stack_t __user *)&st, NULL, regs->regs[0]);

	__asm__ __volatile__(
		"mv\tr0, %0\n\t"
		"la\tr8, syscall_exit\n\t"
		"br\tr8\n\t"
		: : "r" (regs) : "r8");

badframe:
	force_sig(SIGSEGV, current);

	return 0;
}

static int setup_rt_frame(struct k_sigaction *ka, struct pt_regs *regs,
		int signr, sigset_t *set, siginfo_t *info)
{
	struct rt_sigframe __user *frame;
	int err = 0;

	frame = get_sigframe(ka, regs, sizeof(*frame));
	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	/*
	 * Set up the return code ...
	 *
	 *         li      v0, __NR_rt_sigreturn
	 *         syscall
	 */
	err |= __put_user(0x87788000 + __NR_rt_sigreturn*2,
			frame->rs_code + 0);
	err |= __put_user(0x80008002, frame->rs_code + 1);
	flush_cache_sigtramp((unsigned long) frame->rs_code);

	err |= copy_siginfo_to_user(&frame->rs_info, info);
	err |= __put_user(0, &frame->rs_uc.uc_flags);
	err |= __put_user(NULL, &frame->rs_uc.uc_link);
	err |= __put_user((void __user *)current->sas_ss_sp,
				&frame->rs_uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->regs[0]),
				&frame->rs_uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size,
				&frame->rs_uc.uc_stack.ss_size);
	err |= setup_sigcontext(regs, &frame->rs_uc.uc_mcontext);
	err |= __copy_to_user(&frame->rs_uc.uc_sigmask, set, sizeof(*set));

	if (err)
		goto give_sigsegv;

	regs->regs[0] = (unsigned long) frame;
	regs->regs[3] = (unsigned long) frame->rs_code;
	regs->regs[4] = signr;
	regs->regs[5] = (unsigned long) &frame->rs_info;
	regs->regs[6] = (unsigned long) &frame->rs_uc;
	regs->regs[29] = (unsigned long) ka->sa.sa_handler;
	regs->cp0_epc = (unsigned long) ka->sa.sa_handler;

	return 0;

give_sigsegv:
	if (signr == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
	return -EFAULT;
}

static int handle_signal(unsigned long sig, siginfo_t *info,
	struct k_sigaction *ka, sigset_t *oldset, struct pt_regs *regs)
{
	int ret;

	if (regs->is_syscall) {
		switch (regs->regs[4]) {
		case ERESTART_RESTARTBLOCK:
		case ERESTARTNOHAND:
			regs->regs[4] = EINTR;
			break;
		case ERESTARTSYS:
			if (!(ka->sa.sa_flags & SA_RESTART)) {
				regs->regs[4] = EINTR;
				break;
			}
		case ERESTARTNOINTR:
			regs->regs[4] = regs->orig_r4;
			regs->regs[7] = regs->orig_r7;
			regs->cp0_epc -= 8;
		}

		regs->is_syscall = 0;
	}

	/*
	 * Set up the stack frame
	 */
	ret = setup_rt_frame(ka, regs, sig, oldset, info);

	spin_lock_irq(&current->sighand->siglock);
	sigorsets(&current->blocked, &current->blocked, &ka->sa.sa_mask);
	if (!(ka->sa.sa_flags & SA_NODEFER))
		sigaddset(&current->blocked, sig);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	return ret;
}

static void do_signal(struct pt_regs *regs)
{
	struct k_sigaction ka;
	sigset_t *oldset;
	siginfo_t info;
	int signr;

	/*
	 * We want the common case to go fast, which is why we may in certain
	 * cases get here from kernel mode. Just return without doing anything
	 * if so.
	 */
	if (!user_mode(regs))
		return;

	if (test_thread_flag(TIF_RESTORE_SIGMASK))
		oldset = &current->saved_sigmask;
	else
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Actually deliver the signal.  */
		if (handle_signal(signr, &info, &ka, oldset, regs) == 0) {
			/*
			 * A signal was successfully delivered; the saved
			 * sigmask will have been stored in the signal frame,
			 * and will be restored by sigreturn, so we can simply
			 * clear the TIF_RESTORE_SIGMASK flag.
			 */
			if (test_thread_flag(TIF_RESTORE_SIGMASK))
				clear_thread_flag(TIF_RESTORE_SIGMASK);
		}

		return;
	}

	if (regs->is_syscall) {
		if (regs->regs[4] == ERESTARTNOHAND ||
		    regs->regs[4] == ERESTARTSYS ||
		    regs->regs[4] == ERESTARTNOINTR) {
			regs->regs[4] = regs->orig_r4;
			regs->regs[7] = regs->orig_r7;
			regs->cp0_epc -= 8;
		}

		if (regs->regs[4] == ERESTART_RESTARTBLOCK) {
			regs->regs[27] = __NR_restart_syscall;
			regs->regs[4] = regs->orig_r4;
			regs->regs[7] = regs->orig_r7;
			regs->cp0_epc -= 8;
		}

		regs->is_syscall = 0;	/* Don't deal with this again.  */
	}

	/*
	 * If there's no signal to deliver, we just put the saved sigmask
	 * back
	 */
	if (test_thread_flag(TIF_RESTORE_SIGMASK)) {
		clear_thread_flag(TIF_RESTORE_SIGMASK);
		sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
	}
}

/*
 * notification of userspace execution resumption
 * - triggered by the TIF_WORK_MASK flags
 */
asmlinkage void do_notify_resume(struct pt_regs *regs, void *unused,
				__u32 thread_info_flags)
{
	/* deal with pending signal delivery */
	if (thread_info_flags & (_TIF_SIGPENDING | _TIF_RESTORE_SIGMASK))
		do_signal(regs);
}
