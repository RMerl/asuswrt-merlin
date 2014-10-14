/*
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 */

#include <linux/crypto.h>
#include <linux/sched.h> 
#include <linux/stddef.h>
#include <linux/errno.h> 
#include <linux/hardirq.h>
#include <linux/suspend.h>
#include <asm/pda.h>
#include <asm/processor.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#include <asm/ia32.h>

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

#define __NO_STUBS 1
#undef __SYSCALL
#undef _ASM_X86_64_UNISTD_H_
#define __SYSCALL(nr, sym) [nr] = 1,
static char syscalls[] = {
#include <asm/unistd.h>
};

int main(void)
{
#define ENTRY(entry) DEFINE(tsk_ ## entry, offsetof(struct task_struct, entry))
	ENTRY(state);
	ENTRY(flags); 
	ENTRY(thread); 
	ENTRY(pid);
	BLANK();
#undef ENTRY
#define ENTRY(entry) DEFINE(threadinfo_ ## entry, offsetof(struct thread_info, entry))
	ENTRY(flags);
	ENTRY(addr_limit);
	ENTRY(preempt_count);
	ENTRY(status);
	BLANK();
#undef ENTRY
#define ENTRY(entry) DEFINE(pda_ ## entry, offsetof(struct x8664_pda, entry))
	ENTRY(kernelstack); 
	ENTRY(oldrsp); 
	ENTRY(pcurrent); 
	ENTRY(irqcount);
	ENTRY(cpunumber);
	ENTRY(irqstackptr);
	ENTRY(data_offset);
	BLANK();
#undef ENTRY
#ifdef CONFIG_IA32_EMULATION
#define ENTRY(entry) DEFINE(IA32_SIGCONTEXT_ ## entry, offsetof(struct sigcontext_ia32, entry))
	ENTRY(eax);
	ENTRY(ebx);
	ENTRY(ecx);
	ENTRY(edx);
	ENTRY(esi);
	ENTRY(edi);
	ENTRY(ebp);
	ENTRY(esp);
	ENTRY(eip);
	BLANK();
#undef ENTRY
	DEFINE(IA32_RT_SIGFRAME_sigcontext,
	       offsetof (struct rt_sigframe32, uc.uc_mcontext));
	BLANK();
#endif
	DEFINE(pbe_address, offsetof(struct pbe, address));
	DEFINE(pbe_orig_address, offsetof(struct pbe, orig_address));
	DEFINE(pbe_next, offsetof(struct pbe, next));
	BLANK();
	DEFINE(TSS_ist, offsetof(struct tss_struct, ist));
	BLANK();
	DEFINE(crypto_tfm_ctx_offset, offsetof(struct crypto_tfm, __crt_ctx));
	BLANK();
	DEFINE(__NR_syscall_max, sizeof(syscalls) - 1);
	return 0;
}
