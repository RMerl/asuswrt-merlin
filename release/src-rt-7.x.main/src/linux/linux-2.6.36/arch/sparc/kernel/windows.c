/* windows.c: Routines to deal with register window management
 *            at the C-code level.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

/* Do save's until all user register windows are out of the cpu. */
void flush_user_windows(void)
{
	register int ctr asm("g5");

	ctr = 0;
	__asm__ __volatile__(
		"\n1:\n\t"
		"ld	[%%g6 + %2], %%g4\n\t"
		"orcc	%%g0, %%g4, %%g0\n\t"
		"add	%0, 1, %0\n\t"
		"bne	1b\n\t"
		" save	%%sp, -64, %%sp\n"
		"2:\n\t"
		"subcc	%0, 1, %0\n\t"
		"bne	2b\n\t"
		" restore %%g0, %%g0, %%g0\n"
	: "=&r" (ctr)
	: "0" (ctr),
	  "i" ((const unsigned long)TI_UWINMASK)
	: "g4", "cc");
}

static inline void shift_window_buffer(int first_win, int last_win, struct thread_info *tp)
{
	int i;

	for(i = first_win; i < last_win; i++) {
		tp->rwbuf_stkptrs[i] = tp->rwbuf_stkptrs[i+1];
		memcpy(&tp->reg_window[i], &tp->reg_window[i+1], sizeof(struct reg_window32));
	}
}

void synchronize_user_stack(void)
{
	struct thread_info *tp = current_thread_info();
	int window;

	flush_user_windows();
	if(!tp->w_saved)
		return;

	/* Ok, there is some dirty work to do. */
	for(window = tp->w_saved - 1; window >= 0; window--) {
		unsigned long sp = tp->rwbuf_stkptrs[window];

		/* Ok, let it rip. */
		if (copy_to_user((char __user *) sp, &tp->reg_window[window],
				 sizeof(struct reg_window32)))
			continue;

		shift_window_buffer(window, tp->w_saved - 1, tp);
		tp->w_saved--;
	}
}


/* Try to push the windows in a threads window buffer to the
 * user stack.  Unaligned %sp's are not allowed here.
 */

void try_to_clear_window_buffer(struct pt_regs *regs, int who)
{
	struct thread_info *tp = current_thread_info();
	int window;

	flush_user_windows();
	for(window = 0; window < tp->w_saved; window++) {
		unsigned long sp = tp->rwbuf_stkptrs[window];

		if ((sp & 7) ||
		    copy_to_user((char __user *) sp, &tp->reg_window[window],
				 sizeof(struct reg_window32)))
			do_exit(SIGILL);
	}
	tp->w_saved = 0;
}
