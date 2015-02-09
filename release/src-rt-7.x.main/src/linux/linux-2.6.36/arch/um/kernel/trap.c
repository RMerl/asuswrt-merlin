/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/hardirq.h>
#include <asm/current.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include "arch.h"
#include "as-layout.h"
#include "kern_util.h"
#include "os.h"
#include "skas.h"
#include "sysdep/sigcontext.h"

/*
 * Note this is constrained to return 0, -EFAULT, -EACCESS, -ENOMEM by
 * segv().
 */
int handle_page_fault(unsigned long address, unsigned long ip,
		      int is_write, int is_user, int *code_out)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	int err = -EFAULT;

	*code_out = SEGV_MAPERR;

	/*
	 * If the fault was during atomic operation, don't take the fault, just
	 * fail.
	 */
	if (in_atomic())
		goto out_nosemaphore;

	down_read(&mm->mmap_sem);
	vma = find_vma(mm, address);
	if (!vma)
		goto out;
	else if (vma->vm_start <= address)
		goto good_area;
	else if (!(vma->vm_flags & VM_GROWSDOWN))
		goto out;
	else if (is_user && !ARCH_IS_STACKGROW(address))
		goto out;
	else if (expand_stack(vma, address))
		goto out;

good_area:
	*code_out = SEGV_ACCERR;
	if (is_write && !(vma->vm_flags & VM_WRITE))
		goto out;

	/* Don't require VM_READ|VM_EXEC for write faults! */
	if (!is_write && !(vma->vm_flags & (VM_READ | VM_EXEC)))
		goto out;

	do {
		int fault;

		fault = handle_mm_fault(mm, vma, address, is_write ? FAULT_FLAG_WRITE : 0);
		if (unlikely(fault & VM_FAULT_ERROR)) {
			if (fault & VM_FAULT_OOM) {
				goto out_of_memory;
			} else if (fault & VM_FAULT_SIGBUS) {
				err = -EACCES;
				goto out;
			}
			BUG();
		}
		if (fault & VM_FAULT_MAJOR)
			current->maj_flt++;
		else
			current->min_flt++;

		pgd = pgd_offset(mm, address);
		pud = pud_offset(pgd, address);
		pmd = pmd_offset(pud, address);
		pte = pte_offset_kernel(pmd, address);
	} while (!pte_present(*pte));
	err = 0;
	/*
	 * The below warning was added in place of
	 *	pte_mkyoung(); if (is_write) pte_mkdirty();
	 * If it's triggered, we'd see normally a hang here (a clean pte is
	 * marked read-only to emulate the dirty bit).
	 * However, the generic code can mark a PTE writable but clean on a
	 * concurrent read fault, triggering this harmlessly. So comment it out.
	 */
	flush_tlb_page(vma, address);
out:
	up_read(&mm->mmap_sem);
out_nosemaphore:
	return err;

out_of_memory:
	/*
	 * We ran out of memory, call the OOM killer, and return the userspace
	 * (which will retry the fault, or kill us if we got oom-killed).
	 */
	up_read(&mm->mmap_sem);
	pagefault_out_of_memory();
	return 0;
}

static void bad_segv(struct faultinfo fi, unsigned long ip)
{
	struct siginfo si;

	si.si_signo = SIGSEGV;
	si.si_code = SEGV_ACCERR;
	si.si_addr = (void __user *) FAULT_ADDRESS(fi);
	current->thread.arch.faultinfo = fi;
	force_sig_info(SIGSEGV, &si, current);
}

void fatal_sigsegv(void)
{
	force_sigsegv(SIGSEGV, current);
	do_signal();
	/*
	 * This is to tell gcc that we're not returning - do_signal
	 * can, in general, return, but in this case, it's not, since
	 * we just got a fatal SIGSEGV queued.
	 */
	os_dump_core();
}

void segv_handler(int sig, struct uml_pt_regs *regs)
{
	struct faultinfo * fi = UPT_FAULTINFO(regs);

	if (UPT_IS_USER(regs) && !SEGV_IS_FIXABLE(fi)) {
		bad_segv(*fi, UPT_IP(regs));
		return;
	}
	segv(*fi, UPT_IP(regs), UPT_IS_USER(regs), regs);
}

/*
 * We give a *copy* of the faultinfo in the regs to segv.
 * This must be done, since nesting SEGVs could overwrite
 * the info in the regs. A pointer to the info then would
 * give us bad data!
 */
unsigned long segv(struct faultinfo fi, unsigned long ip, int is_user,
		   struct uml_pt_regs *regs)
{
	struct siginfo si;
	jmp_buf *catcher;
	int err;
	int is_write = FAULT_WRITE(fi);
	unsigned long address = FAULT_ADDRESS(fi);

	if (!is_user && (address >= start_vm) && (address < end_vm)) {
		flush_tlb_kernel_vm();
		return 0;
	}
	else if (current->mm == NULL) {
		show_regs(container_of(regs, struct pt_regs, regs));
		panic("Segfault with no mm");
	}

	if (SEGV_IS_FIXABLE(&fi) || SEGV_MAYBE_FIXABLE(&fi))
		err = handle_page_fault(address, ip, is_write, is_user,
					&si.si_code);
	else {
		err = -EFAULT;
		address = 0;
	}

	catcher = current->thread.fault_catcher;
	if (!err)
		return 0;
	else if (catcher != NULL) {
		current->thread.fault_addr = (void *) address;
		UML_LONGJMP(catcher, 1);
	}
	else if (current->thread.fault_addr != NULL)
		panic("fault_addr set but no fault catcher");
	else if (!is_user && arch_fixup(ip, regs))
		return 0;

	if (!is_user) {
		show_regs(container_of(regs, struct pt_regs, regs));
		panic("Kernel mode fault at addr 0x%lx, ip 0x%lx",
		      address, ip);
	}

	if (err == -EACCES) {
		si.si_signo = SIGBUS;
		si.si_errno = 0;
		si.si_code = BUS_ADRERR;
		si.si_addr = (void __user *)address;
		current->thread.arch.faultinfo = fi;
		force_sig_info(SIGBUS, &si, current);
	} else {
		BUG_ON(err != -EFAULT);
		si.si_signo = SIGSEGV;
		si.si_addr = (void __user *) address;
		current->thread.arch.faultinfo = fi;
		force_sig_info(SIGSEGV, &si, current);
	}
	return 0;
}

void relay_signal(int sig, struct uml_pt_regs *regs)
{
	if (!UPT_IS_USER(regs)) {
		if (sig == SIGBUS)
			printk(KERN_ERR "Bus error - the host /dev/shm or /tmp "
			       "mount likely just ran out of space\n");
		panic("Kernel mode signal %d", sig);
	}

	arch_examine_signal(sig, regs);

	current->thread.arch.faultinfo = *UPT_FAULTINFO(regs);
	force_sig(sig, current);
}

void bus_handler(int sig, struct uml_pt_regs *regs)
{
	if (current->thread.fault_catcher != NULL)
		UML_LONGJMP(current->thread.fault_catcher, 1);
	else relay_signal(sig, regs);
}

void winch(int sig, struct uml_pt_regs *regs)
{
	do_IRQ(WINCH_IRQ, regs);
}

void trap_init(void)
{
}
