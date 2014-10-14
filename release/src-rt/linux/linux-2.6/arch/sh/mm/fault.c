/*
 * Page fault handler for SH with an MMU.
 *
 *  Copyright (C) 1999  Niibe Yutaka
 *  Copyright (C) 2003 - 2007  Paul Mundt
 *
 *  Based on linux/arch/i386/mm/fault.c:
 *   Copyright (C) 1995  Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/hardirq.h>
#include <linux/kprobes.h>
#include <asm/system.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/kgdb.h>

/*
 * This routine handles page faults.  It determines the address,
 * and the problem, and then passes it off to one of the appropriate
 * routines.
 */
asmlinkage void __kprobes do_page_fault(struct pt_regs *regs,
					unsigned long writeaccess,
					unsigned long address)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct * vma;
	unsigned long page;
	int si_code;
	int fault;
	siginfo_t info;

	trace_hardirqs_on();
	local_irq_enable();

#ifdef CONFIG_SH_KGDB
	if (kgdb_nofault && kgdb_bus_err_hook)
		kgdb_bus_err_hook();
#endif

	tsk = current;
	mm = tsk->mm;
	si_code = SEGV_MAPERR;

	if (unlikely(address >= TASK_SIZE)) {
		/*
		 * Synchronize this task's top level page-table
		 * with the 'reference' page table.
		 *
		 * Do _not_ use "tsk" here. We might be inside
		 * an interrupt in the middle of a task switch..
		 */
		int offset = pgd_index(address);
		pgd_t *pgd, *pgd_k;
		pud_t *pud, *pud_k;
		pmd_t *pmd, *pmd_k;

		pgd = get_TTB() + offset;
		pgd_k = swapper_pg_dir + offset;

		/* This will never happen with the folded page table. */
		if (!pgd_present(*pgd)) {
			if (!pgd_present(*pgd_k))
				goto bad_area_nosemaphore;
			set_pgd(pgd, *pgd_k);
			return;
		}

		pud = pud_offset(pgd, address);
		pud_k = pud_offset(pgd_k, address);
		if (pud_present(*pud) || !pud_present(*pud_k))
			goto bad_area_nosemaphore;
		set_pud(pud, *pud_k);

		pmd = pmd_offset(pud, address);
		pmd_k = pmd_offset(pud_k, address);
		if (pmd_present(*pmd) || !pmd_present(*pmd_k))
			goto bad_area_nosemaphore;
		set_pmd(pmd, *pmd_k);

		return;
	}

	/*
	 * If we're in an interrupt or have no user
	 * context, we must not take the fault..
	 */
	if (in_atomic() || !mm)
		goto no_context;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= address)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (expand_stack(vma, address))
		goto bad_area;
/*
 * Ok, we have a good vm_area for this memory access, so
 * we can handle it..
 */
good_area:
	si_code = SEGV_ACCERR;
	if (writeaccess) {
		if (!(vma->vm_flags & VM_WRITE))
			goto bad_area;
	} else {
		if (!(vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)))
			goto bad_area;
	}

	/*
	 * If for any reason at all we couldn't handle the fault,
	 * make sure we exit gracefully rather than endlessly redo
	 * the fault.
	 */
survive:
	fault = handle_mm_fault(mm, vma, address, writeaccess);
	if (unlikely(fault & VM_FAULT_ERROR)) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}
	if (fault & VM_FAULT_MAJOR)
		tsk->maj_flt++;
	else
		tsk->min_flt++;

	up_read(&mm->mmap_sem);
	return;

/*
 * Something tried to access memory that isn't in our memory map..
 * Fix it, but check if it's kernel or user first..
 */
bad_area:
	up_read(&mm->mmap_sem);

bad_area_nosemaphore:
	if (user_mode(regs)) {
		info.si_signo = SIGSEGV;
		info.si_errno = 0;
		info.si_code = si_code;
		info.si_addr = (void *) address;
		force_sig_info(SIGSEGV, &info, tsk);
		return;
	}

no_context:
	/* Are we prepared to handle this kernel fault?  */
	if (fixup_exception(regs))
		return;

/*
 * Oops. The kernel tried to access some bad page. We'll have to
 * terminate things with extreme prejudice.
 *
 */
	if (address < PAGE_SIZE)
		printk(KERN_ALERT "Unable to handle kernel NULL pointer dereference");
	else
		printk(KERN_ALERT "Unable to handle kernel paging request");
	printk(" at virtual address %08lx\n", address);
	printk(KERN_ALERT "pc = %08lx\n", regs->pc);
	page = (unsigned long)get_TTB();
	if (page) {
		page = ((unsigned long *) page)[address >> PGDIR_SHIFT];
		printk(KERN_ALERT "*pde = %08lx\n", page);
		if (page & _PAGE_PRESENT) {
			page &= PAGE_MASK;
			address &= 0x003ff000;
			page = ((unsigned long *) __va(page))[address >> PAGE_SHIFT];
			printk(KERN_ALERT "*pte = %08lx\n", page);
		}
	}
	die("Oops", regs, writeaccess);
	do_exit(SIGKILL);

/*
 * We ran out of memory, or some other thing happened to us that made
 * us unable to handle the page fault gracefully.
 */
out_of_memory:
	up_read(&mm->mmap_sem);
	if (is_init(current)) {
		yield();
		down_read(&mm->mmap_sem);
		goto survive;
	}
	printk("VM: killing process %s\n", tsk->comm);
	if (user_mode(regs))
		do_exit(SIGKILL);
	goto no_context;

do_sigbus:
	up_read(&mm->mmap_sem);

	/*
	 * Send a sigbus, regardless of whether we were in kernel
	 * or user mode.
	 */
	info.si_signo = SIGBUS;
	info.si_errno = 0;
	info.si_code = BUS_ADRERR;
	info.si_addr = (void *)address;
	force_sig_info(SIGBUS, &info, tsk);

	/* Kernel mode? Handle exceptions or die */
	if (!user_mode(regs))
		goto no_context;
}

#ifdef CONFIG_SH_STORE_QUEUES
/*
 * This is a special case for the SH-4 store queues, as pages for this
 * space still need to be faulted in before it's possible to flush the
 * store queue cache for writeout to the remapped region.
 */
#define P3_ADDR_MAX		(P4SEG_STORE_QUE + 0x04000000)
#else
#define P3_ADDR_MAX		P4SEG
#endif

/*
 * Called with interrupts disabled.
 */
asmlinkage int __kprobes __do_page_fault(struct pt_regs *regs,
					 unsigned long writeaccess,
					 unsigned long address)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	struct mm_struct *mm = current->mm;
	spinlock_t *ptl = NULL;
	int ret = 1;

#ifdef CONFIG_SH_KGDB
	if (kgdb_nofault && kgdb_bus_err_hook)
		kgdb_bus_err_hook();
#endif

	/*
	 * We don't take page faults for P1, P2, and parts of P4, these
	 * are always mapped, whether it be due to legacy behaviour in
	 * 29-bit mode, or due to PMB configuration in 32-bit mode.
	 */
	if (address >= P3SEG && address < P3_ADDR_MAX) {
		pgd = pgd_offset_k(address);
		mm = NULL;
	} else {
		if (unlikely(address >= TASK_SIZE || !mm))
			return 1;

		pgd = pgd_offset(mm, address);
	}

	pud = pud_offset(pgd, address);
	if (pud_none_or_clear_bad(pud))
		return 1;
	pmd = pmd_offset(pud, address);
	if (pmd_none_or_clear_bad(pmd))
		return 1;

	if (mm)
		pte = pte_offset_map_lock(mm, pmd, address, &ptl);
	else
		pte = pte_offset_kernel(pmd, address);

	entry = *pte;
	if (unlikely(pte_none(entry) || pte_not_present(entry)))
		goto unlock;
	if (unlikely(writeaccess && !pte_write(entry)))
		goto unlock;

	if (writeaccess)
		entry = pte_mkdirty(entry);
	entry = pte_mkyoung(entry);

#ifdef CONFIG_CPU_SH4
	/*
	 * ITLB is not affected by "ldtlb" instruction.
	 * So, we need to flush the entry by ourselves.
	 */
	local_flush_tlb_one(get_asid(), address & PAGE_MASK);
#endif

	set_pte(pte, entry);
	update_mmu_cache(NULL, address, entry);
	ret = 0;
unlock:
	if (mm)
		pte_unmap_unlock(pte, ptl);
	return ret;
}
