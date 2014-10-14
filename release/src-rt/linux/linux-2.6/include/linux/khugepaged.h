#ifndef _LINUX_KHUGEPAGED_H
#define _LINUX_KHUGEPAGED_H

#include <linux/sched.h> /* MMF_VM_HUGEPAGE */

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
extern int __khugepaged_enter(struct mm_struct *mm);
extern void __khugepaged_exit(struct mm_struct *mm);
extern int khugepaged_enter_vma_merge(struct vm_area_struct *vma);

#define khugepaged_enabled()					       \
	(transparent_hugepage_flags &				       \
	 ((1<<TRANSPARENT_HUGEPAGE_FLAG) |		       \
	  (1<<TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG)))
#define khugepaged_always()				\
	(transparent_hugepage_flags &			\
	 (1<<TRANSPARENT_HUGEPAGE_FLAG))
#define khugepaged_req_madv()					\
	(transparent_hugepage_flags &				\
	 (1<<TRANSPARENT_HUGEPAGE_REQ_MADV_FLAG))
#define khugepaged_defrag()					\
	(transparent_hugepage_flags &				\
	 (1<<TRANSPARENT_HUGEPAGE_DEFRAG_KHUGEPAGED_FLAG))

static inline int khugepaged_fork(struct mm_struct *mm, struct mm_struct *oldmm)
{
	if (test_bit(MMF_VM_HUGEPAGE, &oldmm->flags))
		return __khugepaged_enter(mm);
	return 0;
}

static inline void khugepaged_exit(struct mm_struct *mm)
{
	if (test_bit(MMF_VM_HUGEPAGE, &mm->flags))
		__khugepaged_exit(mm);
}

static inline int khugepaged_enter(struct vm_area_struct *vma)
{
	if (!test_bit(MMF_VM_HUGEPAGE, &vma->vm_mm->flags))
		if ((khugepaged_always() ||
		     (khugepaged_req_madv() &&
		      vma->vm_flags & VM_HUGEPAGE)) &&
		    !(vma->vm_flags & VM_NOHUGEPAGE))
			if (__khugepaged_enter(vma->vm_mm))
				return -ENOMEM;
	return 0;
}
#else /* CONFIG_TRANSPARENT_HUGEPAGE */
static inline int khugepaged_fork(struct mm_struct *mm, struct mm_struct *oldmm)
{
	return 0;
}
static inline void khugepaged_exit(struct mm_struct *mm)
{
}
static inline int khugepaged_enter(struct vm_area_struct *vma)
{
	return 0;
}
static inline int khugepaged_enter_vma_merge(struct vm_area_struct *vma)
{
	return 0;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#endif /* _LINUX_KHUGEPAGED_H */
