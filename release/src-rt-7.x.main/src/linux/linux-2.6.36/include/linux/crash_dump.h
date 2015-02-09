#ifndef LINUX_CRASH_DUMP_H
#define LINUX_CRASH_DUMP_H

#ifdef CONFIG_CRASH_DUMP
#include <linux/kexec.h>
#include <linux/device.h>
#include <linux/proc_fs.h>

#define ELFCORE_ADDR_MAX	(-1ULL)
#define ELFCORE_ADDR_ERR	(-2ULL)

extern unsigned long long elfcorehdr_addr;

extern ssize_t copy_oldmem_page(unsigned long, char *, size_t,
						unsigned long, int);

/* Architecture code defines this if there are other possible ELF
 * machine types, e.g. on bi-arch capable hardware. */
#ifndef vmcore_elf_check_arch_cross
#define vmcore_elf_check_arch_cross(x) 0
#endif

#define vmcore_elf_check_arch(x) (elf_check_arch(x) || vmcore_elf_check_arch_cross(x))

/*
 * is_kdump_kernel() checks whether this kernel is booting after a panic of
 * previous kernel or not. This is determined by checking if previous kernel
 * has passed the elf core header address on command line.
 *
 * This is not just a test if CONFIG_CRASH_DUMP is enabled or not. It will
 * return 1 if CONFIG_CRASH_DUMP=y and if kernel is booting after a panic of
 * previous kernel.
 */

static inline int is_kdump_kernel(void)
{
	return (elfcorehdr_addr != ELFCORE_ADDR_MAX) ? 1 : 0;
}

/* is_vmcore_usable() checks if the kernel is booting after a panic and
 * the vmcore region is usable.
 *
 * This makes use of the fact that due to alignment -2ULL is not
 * a valid pointer, much in the vain of IS_ERR(), except
 * dealing directly with an unsigned long long rather than a pointer.
 */

static inline int is_vmcore_usable(void)
{
	return is_kdump_kernel() && elfcorehdr_addr != ELFCORE_ADDR_ERR ? 1 : 0;
}

/* vmcore_unusable() marks the vmcore as unusable,
 * without disturbing the logic of is_kdump_kernel()
 */

static inline void vmcore_unusable(void)
{
	if (is_kdump_kernel())
		elfcorehdr_addr = ELFCORE_ADDR_ERR;
}
#else /* !CONFIG_CRASH_DUMP */
static inline int is_kdump_kernel(void) { return 0; }
#endif /* CONFIG_CRASH_DUMP */

extern unsigned long saved_max_pfn;
#endif /* LINUX_CRASHDUMP_H */
