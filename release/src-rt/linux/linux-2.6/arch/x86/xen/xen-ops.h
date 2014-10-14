#ifndef XEN_OPS_H
#define XEN_OPS_H

#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/irqreturn.h>
#include <xen/xen-ops.h>

/* These are code, but not functions.  Defined in entry.S */
extern const char xen_hypervisor_callback[];
extern const char xen_failsafe_callback[];

extern void *xen_initial_gdt;

struct trap_info;
void xen_copy_trap_info(struct trap_info *traps);

DECLARE_PER_CPU(struct vcpu_info, xen_vcpu_info);
DECLARE_PER_CPU(unsigned long, xen_cr3);
DECLARE_PER_CPU(unsigned long, xen_current_cr3);

extern struct start_info *xen_start_info;
extern struct shared_info xen_dummy_shared_info;
extern struct shared_info *HYPERVISOR_shared_info;

void xen_setup_mfn_list_list(void);
void xen_setup_shared_info(void);
void xen_build_mfn_list_list(void);
void xen_setup_machphys_mapping(void);
pgd_t *xen_setup_kernel_pagetable(pgd_t *pgd, unsigned long max_pfn);
void xen_ident_map_ISA(void);
void xen_reserve_top(void);
extern unsigned long xen_max_p2m_pfn;

void xen_set_pat(u64);

char * __init xen_memory_setup(void);
void __init xen_arch_setup(void);
void __init xen_init_IRQ(void);
void xen_enable_sysenter(void);
void xen_enable_syscall(void);
void xen_vcpu_restore(void);

void xen_callback_vector(void);
void xen_hvm_init_shared_info(void);
void xen_unplug_emulated_devices(void);

void __init xen_build_dynamic_phys_to_machine(void);

void xen_init_irq_ops(void);
void xen_setup_timer(int cpu);
void xen_setup_runstate_info(int cpu);
void xen_teardown_timer(int cpu);
cycle_t xen_clocksource_read(void);
void xen_setup_cpu_clockevents(void);
void __init xen_init_time_ops(void);
void __init xen_hvm_init_time_ops(void);

irqreturn_t xen_debug_interrupt(int irq, void *dev_id);

bool xen_vcpu_stolen(int vcpu);

void xen_setup_vcpu_info_placement(void);

#ifdef CONFIG_SMP
void xen_smp_init(void);
void __init xen_hvm_smp_init(void);

extern cpumask_var_t xen_cpu_initialized_map;
#else
static inline void xen_smp_init(void) {}
static inline void xen_hvm_smp_init(void) {}
#endif

#ifdef CONFIG_PARAVIRT_SPINLOCKS
void __init xen_init_spinlocks(void);
__cpuinit void xen_init_lock_cpu(int cpu);
void xen_uninit_lock_cpu(int cpu);
#else
static inline void xen_init_spinlocks(void)
{
}
static inline void xen_init_lock_cpu(int cpu)
{
}
static inline void xen_uninit_lock_cpu(int cpu)
{
}
#endif

/* Declare an asm function, along with symbols needed to make it
   inlineable */
#define DECL_ASM(ret, name, ...)		\
	ret name(__VA_ARGS__);			\
	extern char name##_end[];		\
	extern char name##_reloc[]		\

DECL_ASM(void, xen_irq_enable_direct, void);
DECL_ASM(void, xen_irq_disable_direct, void);
DECL_ASM(unsigned long, xen_save_fl_direct, void);
DECL_ASM(void, xen_restore_fl_direct, unsigned long);

/* These are not functions, and cannot be called normally */
void xen_iret(void);
void xen_sysexit(void);
void xen_sysret32(void);
void xen_sysret64(void);
void xen_adjust_exception_frame(void);

extern int xen_panic_handler_init(void);

#endif /* XEN_OPS_H */
