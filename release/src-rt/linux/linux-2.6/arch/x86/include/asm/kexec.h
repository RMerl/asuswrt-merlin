#ifndef _ASM_X86_KEXEC_H
#define _ASM_X86_KEXEC_H

#ifdef CONFIG_X86_32
# define PA_CONTROL_PAGE	0
# define VA_CONTROL_PAGE	1
# define PA_PGD			2
# define PA_SWAP_PAGE		3
# define PAGES_NR		4
#else
# define PA_CONTROL_PAGE	0
# define VA_CONTROL_PAGE	1
# define PA_TABLE_PAGE		2
# define PA_SWAP_PAGE		3
# define PAGES_NR		4
#endif

# define KEXEC_CONTROL_CODE_MAX_SIZE	2048

#ifndef __ASSEMBLY__

#include <linux/string.h>

#include <asm/page.h>
#include <asm/ptrace.h>

/*
 * KEXEC_SOURCE_MEMORY_LIMIT maximum page get_free_page can return.
 * I.e. Maximum page that is mapped directly into kernel memory,
 * and kmap is not required.
 *
 * So far x86_64 is limited to 40 physical address bits.
 */
#ifdef CONFIG_X86_32
/* Maximum physical address we can use pages from */
# define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)
/* Maximum address we can reach in physical address mode */
# define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)
/* Maximum address we can use for the control code buffer */
# define KEXEC_CONTROL_MEMORY_LIMIT TASK_SIZE

# define KEXEC_CONTROL_PAGE_SIZE	4096

/* The native architecture */
# define KEXEC_ARCH KEXEC_ARCH_386

/* We can also handle crash dumps from 64 bit kernel. */
# define vmcore_elf_check_arch_cross(x) ((x)->e_machine == EM_X86_64)
#else
/* Maximum physical address we can use pages from */
# define KEXEC_SOURCE_MEMORY_LIMIT      (0xFFFFFFFFFFUL)
/* Maximum address we can reach in physical address mode */
# define KEXEC_DESTINATION_MEMORY_LIMIT (0xFFFFFFFFFFUL)
/* Maximum address we can use for the control pages */
# define KEXEC_CONTROL_MEMORY_LIMIT     (0xFFFFFFFFFFUL)

/* Allocate one page for the pdp and the second for the code */
# define KEXEC_CONTROL_PAGE_SIZE  (4096UL + 4096UL)

/* The native architecture */
# define KEXEC_ARCH KEXEC_ARCH_X86_64
#endif

/*
 * CPU does not save ss and sp on stack if execution is already
 * running in kernel mode at the time of NMI occurrence. This code
 * fixes it.
 */
static inline void crash_fixup_ss_esp(struct pt_regs *newregs,
				      struct pt_regs *oldregs)
{
#ifdef CONFIG_X86_32
	newregs->sp = (unsigned long)&(oldregs->sp);
	asm volatile("xorl %%eax, %%eax\n\t"
		     "movw %%ss, %%ax\n\t"
		     :"=a"(newregs->ss));
#endif
}

/*
 * This function is responsible for capturing register states if coming
 * via panic otherwise just fix up the ss and sp if coming via kernel
 * mode exception.
 */
static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	if (oldregs) {
		memcpy(newregs, oldregs, sizeof(*newregs));
		crash_fixup_ss_esp(newregs, oldregs);
	} else {
#ifdef CONFIG_X86_32
		asm volatile("movl %%ebx,%0" : "=m"(newregs->bx));
		asm volatile("movl %%ecx,%0" : "=m"(newregs->cx));
		asm volatile("movl %%edx,%0" : "=m"(newregs->dx));
		asm volatile("movl %%esi,%0" : "=m"(newregs->si));
		asm volatile("movl %%edi,%0" : "=m"(newregs->di));
		asm volatile("movl %%ebp,%0" : "=m"(newregs->bp));
		asm volatile("movl %%eax,%0" : "=m"(newregs->ax));
		asm volatile("movl %%esp,%0" : "=m"(newregs->sp));
		asm volatile("movl %%ss, %%eax;" :"=a"(newregs->ss));
		asm volatile("movl %%cs, %%eax;" :"=a"(newregs->cs));
		asm volatile("movl %%ds, %%eax;" :"=a"(newregs->ds));
		asm volatile("movl %%es, %%eax;" :"=a"(newregs->es));
		asm volatile("pushfl; popl %0" :"=m"(newregs->flags));
#else
		asm volatile("movq %%rbx,%0" : "=m"(newregs->bx));
		asm volatile("movq %%rcx,%0" : "=m"(newregs->cx));
		asm volatile("movq %%rdx,%0" : "=m"(newregs->dx));
		asm volatile("movq %%rsi,%0" : "=m"(newregs->si));
		asm volatile("movq %%rdi,%0" : "=m"(newregs->di));
		asm volatile("movq %%rbp,%0" : "=m"(newregs->bp));
		asm volatile("movq %%rax,%0" : "=m"(newregs->ax));
		asm volatile("movq %%rsp,%0" : "=m"(newregs->sp));
		asm volatile("movq %%r8,%0" : "=m"(newregs->r8));
		asm volatile("movq %%r9,%0" : "=m"(newregs->r9));
		asm volatile("movq %%r10,%0" : "=m"(newregs->r10));
		asm volatile("movq %%r11,%0" : "=m"(newregs->r11));
		asm volatile("movq %%r12,%0" : "=m"(newregs->r12));
		asm volatile("movq %%r13,%0" : "=m"(newregs->r13));
		asm volatile("movq %%r14,%0" : "=m"(newregs->r14));
		asm volatile("movq %%r15,%0" : "=m"(newregs->r15));
		asm volatile("movl %%ss, %%eax;" :"=a"(newregs->ss));
		asm volatile("movl %%cs, %%eax;" :"=a"(newregs->cs));
		asm volatile("pushfq; popq %0" :"=m"(newregs->flags));
#endif
		newregs->ip = (unsigned long)current_text_addr();
	}
}

#ifdef CONFIG_X86_32
asmlinkage unsigned long
relocate_kernel(unsigned long indirection_page,
		unsigned long control_page,
		unsigned long start_address,
		unsigned int has_pae,
		unsigned int preserve_context);
#else
unsigned long
relocate_kernel(unsigned long indirection_page,
		unsigned long page_list,
		unsigned long start_address,
		unsigned int preserve_context);
#endif

#define ARCH_HAS_KIMAGE_ARCH

#ifdef CONFIG_X86_32
struct kimage_arch {
	pgd_t *pgd;
#ifdef CONFIG_X86_PAE
	pmd_t *pmd0;
	pmd_t *pmd1;
#endif
	pte_t *pte0;
	pte_t *pte1;
};
#else
struct kimage_arch {
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
};
#endif

#endif /* __ASSEMBLY__ */

#endif /* _ASM_X86_KEXEC_H */
