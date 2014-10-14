/*
 * GCC stack protector support.
 *
 * Stack protector works by putting predefined pattern at the start of
 * the stack frame and verifying that it hasn't been overwritten when
 * returning from the function.  The pattern is called stack canary
 * and unfortunately gcc requires it to be at a fixed offset from %gs.
 * On x86_64, the offset is 40 bytes and on x86_32 20 bytes.  x86_64
 * and x86_32 use segment registers differently and thus handles this
 * requirement differently.
 *
 * On x86_64, %gs is shared by percpu area and stack canary.  All
 * percpu symbols are zero based and %gs points to the base of percpu
 * area.  The first occupant of the percpu area is always
 * irq_stack_union which contains stack_canary at offset 40.  Userland
 * %gs is always saved and restored on kernel entry and exit using
 * swapgs, so stack protector doesn't add any complexity there.
 *
 * On x86_32, it's slightly more complicated.  As in x86_64, %gs is
 * used for userland TLS.  Unfortunately, some processors are much
 * slower at loading segment registers with different value when
 * entering and leaving the kernel, so the kernel uses %fs for percpu
 * area and manages %gs lazily so that %gs is switched only when
 * necessary, usually during task switch.
 *
 * As gcc requires the stack canary at %gs:20, %gs can't be managed
 * lazily if stack protector is enabled, so the kernel saves and
 * restores userland %gs on kernel entry and exit.  This behavior is
 * controlled by CONFIG_X86_32_LAZY_GS and accessors are defined in
 * system.h to hide the details.
 */

#ifndef _ASM_STACKPROTECTOR_H
#define _ASM_STACKPROTECTOR_H 1

#ifdef CONFIG_CC_STACKPROTECTOR

#include <asm/tsc.h>
#include <asm/processor.h>
#include <asm/percpu.h>
#include <asm/system.h>
#include <asm/desc.h>
#include <linux/random.h>

/*
 * 24 byte read-only segment initializer for stack canary.  Linker
 * can't handle the address bit shifting.  Address will be set in
 * head_32 for boot CPU and setup_per_cpu_areas() for others.
 */
#define GDT_STACK_CANARY_INIT						\
	[GDT_ENTRY_STACK_CANARY] = GDT_ENTRY_INIT(0x4090, 0, 0x18),

/*
 * Initialize the stackprotector canary value.
 *
 * NOTE: this must only be called from functions that never return,
 * and it must always be inlined.
 */
static __always_inline void boot_init_stack_canary(void)
{
	u64 canary;
	u64 tsc;

#ifdef CONFIG_X86_64
	BUILD_BUG_ON(offsetof(union irq_stack_union, stack_canary) != 40);
#endif
	/*
	 * We both use the random pool and the current TSC as a source
	 * of randomness. The TSC only matters for very early init,
	 * there it already has some randomness on most systems. Later
	 * on during the bootup the random pool has true entropy too.
	 */
	get_random_bytes(&canary, sizeof(canary));
	tsc = __native_read_tsc();
	canary += tsc + (tsc << 32UL);

	current->stack_canary = canary;
#ifdef CONFIG_X86_64
	percpu_write(irq_stack_union.stack_canary, canary);
#else
	percpu_write(stack_canary.canary, canary);
#endif
}

static inline void setup_stack_canary_segment(int cpu)
{
#ifdef CONFIG_X86_32
	unsigned long canary = (unsigned long)&per_cpu(stack_canary, cpu);
	struct desc_struct *gdt_table = get_cpu_gdt_table(cpu);
	struct desc_struct desc;

	desc = gdt_table[GDT_ENTRY_STACK_CANARY];
	set_desc_base(&desc, canary);
	write_gdt_entry(gdt_table, GDT_ENTRY_STACK_CANARY, &desc, DESCTYPE_S);
#endif
}

static inline void load_stack_canary_segment(void)
{
#ifdef CONFIG_X86_32
	asm("mov %0, %%gs" : : "r" (__KERNEL_STACK_CANARY) : "memory");
#endif
}

#else	/* CC_STACKPROTECTOR */

#define GDT_STACK_CANARY_INIT

/* dummy boot_init_stack_canary() is defined in linux/stackprotector.h */

static inline void setup_stack_canary_segment(int cpu)
{ }

static inline void load_stack_canary_segment(void)
{
#ifdef CONFIG_X86_32
	asm volatile ("mov %0, %%gs" : : "r" (0));
#endif
}

#endif	/* CC_STACKPROTECTOR */
#endif	/* _ASM_STACKPROTECTOR_H */
