/*
 * Machine specific setup for xen
 *
 * Jeremy Fitzhardinge <jeremy@xensource.com>, XenSource Inc, 2007
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/pm.h>

#include <asm/elf.h>
#include <asm/vdso.h>
#include <asm/e820.h>
#include <asm/setup.h>
#include <asm/acpi.h>
#include <asm/xen/hypervisor.h>
#include <asm/xen/hypercall.h>

#include <xen/page.h>
#include <xen/interface/callback.h>
#include <xen/interface/physdev.h>
#include <xen/interface/memory.h>
#include <xen/features.h>

#include "xen-ops.h"
#include "vdso.h"

/* These are code, but not functions.  Defined in entry.S */
extern const char xen_hypervisor_callback[];
extern const char xen_failsafe_callback[];
extern void xen_sysenter_target(void);
extern void xen_syscall_target(void);
extern void xen_syscall32_target(void);

static unsigned long __init xen_release_chunk(phys_addr_t start_addr,
					      phys_addr_t end_addr)
{
	struct xen_memory_reservation reservation = {
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};
	unsigned long start, end;
	unsigned long len = 0;
	unsigned long pfn;
	int ret;

	start = PFN_UP(start_addr);
	end = PFN_DOWN(end_addr);

	if (end <= start)
		return 0;

	printk(KERN_INFO "xen_release_chunk: looking at area pfn %lx-%lx: ",
	       start, end);
	for(pfn = start; pfn < end; pfn++) {
		unsigned long mfn = pfn_to_mfn(pfn);

		/* Make sure pfn exists to start with */
		if (mfn == INVALID_P2M_ENTRY || mfn_to_pfn(mfn) != pfn)
			continue;

		set_xen_guest_handle(reservation.extent_start, &mfn);
		reservation.nr_extents = 1;

		ret = HYPERVISOR_memory_op(XENMEM_decrease_reservation,
					   &reservation);
		WARN(ret != 1, "Failed to release memory %lx-%lx err=%d\n",
		     start, end, ret);
		if (ret == 1) {
			set_phys_to_machine(pfn, INVALID_P2M_ENTRY);
			len++;
		}
	}
	printk(KERN_CONT "%ld pages freed\n", len);

	return len;
}

static unsigned long __init xen_return_unused_memory(unsigned long max_pfn,
						     const struct e820map *e820)
{
	phys_addr_t max_addr = PFN_PHYS(max_pfn);
	phys_addr_t last_end = 0;
	unsigned long released = 0;
	int i;

	for (i = 0; i < e820->nr_map && last_end < max_addr; i++) {
		phys_addr_t end = e820->map[i].addr;
		end = min(max_addr, end);

		released += xen_release_chunk(last_end, end);
		last_end = e820->map[i].addr + e820->map[i].size;
	}

	if (last_end < max_addr)
		released += xen_release_chunk(last_end, max_addr);

	printk(KERN_INFO "released %ld pages of unused memory\n", released);
	return released;
}

/**
 * machine_specific_memory_setup - Hook for machine specific memory setup.
 **/

char * __init xen_memory_setup(void)
{
	unsigned long max_pfn = xen_start_info->nr_pages;

	max_pfn = min(MAX_DOMAIN_PAGES, max_pfn);

	e820.nr_map = 0;

	e820_add_region(0, PFN_PHYS((u64)max_pfn), E820_RAM);

	/*
	 * Even though this is normal, usable memory under Xen, reserve
	 * ISA memory anyway because too many things think they can poke
	 * about in there.
	 */
	e820_add_region(ISA_START_ADDRESS, ISA_END_ADDRESS - ISA_START_ADDRESS,
			E820_RESERVED);

	/*
	 * Reserve Xen bits:
	 *  - mfn_list
	 *  - xen_start_info
	 * See comment above "struct start_info" in <xen/interface/xen.h>
	 */
	reserve_early(__pa(xen_start_info->mfn_list),
		      __pa(xen_start_info->pt_base),
			"XEN START INFO");

	sanitize_e820_map(e820.map, ARRAY_SIZE(e820.map), &e820.nr_map);

	xen_return_unused_memory(xen_start_info->nr_pages, &e820);

	return "Xen";
}

static void xen_idle(void)
{
	local_irq_disable();

	if (need_resched())
		local_irq_enable();
	else {
		current_thread_info()->status &= ~TS_POLLING;
		smp_mb__after_clear_bit();
		safe_halt();
		current_thread_info()->status |= TS_POLLING;
	}
}

/*
 * Set the bit indicating "nosegneg" library variants should be used.
 * We only need to bother in pure 32-bit mode; compat 32-bit processes
 * can have un-truncated segments, so wrapping around is allowed.
 */
static void __init fiddle_vdso(void)
{
#ifdef CONFIG_X86_32
	u32 *mask;
	mask = VDSO32_SYMBOL(&vdso32_int80_start, NOTE_MASK);
	*mask |= 1 << VDSO_NOTE_NONEGSEG_BIT;
	mask = VDSO32_SYMBOL(&vdso32_sysenter_start, NOTE_MASK);
	*mask |= 1 << VDSO_NOTE_NONEGSEG_BIT;
#endif
}

static __cpuinit int register_callback(unsigned type, const void *func)
{
	struct callback_register callback = {
		.type = type,
		.address = XEN_CALLBACK(__KERNEL_CS, func),
		.flags = CALLBACKF_mask_events,
	};

	return HYPERVISOR_callback_op(CALLBACKOP_register, &callback);
}

void __cpuinit xen_enable_sysenter(void)
{
	int ret;
	unsigned sysenter_feature;

#ifdef CONFIG_X86_32
	sysenter_feature = X86_FEATURE_SEP;
#else
	sysenter_feature = X86_FEATURE_SYSENTER32;
#endif

	if (!boot_cpu_has(sysenter_feature))
		return;

	ret = register_callback(CALLBACKTYPE_sysenter, xen_sysenter_target);
	if(ret != 0)
		setup_clear_cpu_cap(sysenter_feature);
}

void __cpuinit xen_enable_syscall(void)
{
#ifdef CONFIG_X86_64
	int ret;

	ret = register_callback(CALLBACKTYPE_syscall, xen_syscall_target);
	if (ret != 0) {
		printk(KERN_ERR "Failed to set syscall callback: %d\n", ret);
		/* Pretty fatal; 64-bit userspace has no other
		   mechanism for syscalls. */
	}

	if (boot_cpu_has(X86_FEATURE_SYSCALL32)) {
		ret = register_callback(CALLBACKTYPE_syscall32,
					xen_syscall32_target);
		if (ret != 0)
			setup_clear_cpu_cap(X86_FEATURE_SYSCALL32);
	}
#endif /* CONFIG_X86_64 */
}

void __init xen_arch_setup(void)
{
	struct physdev_set_iopl set_iopl;
	int rc;

	xen_panic_handler_init();

	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_4gb_segments);
	HYPERVISOR_vm_assist(VMASST_CMD_enable, VMASST_TYPE_writable_pagetables);

	if (!xen_feature(XENFEAT_auto_translated_physmap))
		HYPERVISOR_vm_assist(VMASST_CMD_enable,
				     VMASST_TYPE_pae_extended_cr3);

	if (register_callback(CALLBACKTYPE_event, xen_hypervisor_callback) ||
	    register_callback(CALLBACKTYPE_failsafe, xen_failsafe_callback))
		BUG();

	xen_enable_sysenter();
	xen_enable_syscall();

	set_iopl.iopl = 1;
	rc = HYPERVISOR_physdev_op(PHYSDEVOP_set_iopl, &set_iopl);
	if (rc != 0)
		printk(KERN_INFO "physdev_op failed %d\n", rc);

#ifdef CONFIG_ACPI
	if (!(xen_start_info->flags & SIF_INITDOMAIN)) {
		printk(KERN_INFO "ACPI in unprivileged domain disabled\n");
		disable_acpi();
	}
#endif

	memcpy(boot_command_line, xen_start_info->cmd_line,
	       MAX_GUEST_CMDLINE > COMMAND_LINE_SIZE ?
	       COMMAND_LINE_SIZE : MAX_GUEST_CMDLINE);

	pm_idle = xen_idle;

	paravirt_disable_iospace();

	fiddle_vdso();
}
