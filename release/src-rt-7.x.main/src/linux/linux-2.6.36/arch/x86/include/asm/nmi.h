#ifndef _ASM_X86_NMI_H
#define _ASM_X86_NMI_H

#include <linux/pm.h>
#include <asm/irq.h>
#include <asm/io.h>

#ifdef ARCH_HAS_NMI_WATCHDOG

/**
 * do_nmi_callback
 *
 * Check to see if a callback exists and execute it.  Return 1
 * if the handler exists and was handled successfully.
 */
int do_nmi_callback(struct pt_regs *regs, int cpu);

extern void die_nmi(char *str, struct pt_regs *regs, int do_panic);
extern int check_nmi_watchdog(void);
#if !defined(CONFIG_LOCKUP_DETECTOR)
extern int nmi_watchdog_enabled;
#endif
extern int avail_to_resrv_perfctr_nmi_bit(unsigned int);
extern int reserve_perfctr_nmi(unsigned int);
extern void release_perfctr_nmi(unsigned int);
extern int reserve_evntsel_nmi(unsigned int);
extern void release_evntsel_nmi(unsigned int);

extern void setup_apic_nmi_watchdog(void *);
extern void stop_apic_nmi_watchdog(void *);
extern void disable_timer_nmi_watchdog(void);
extern void enable_timer_nmi_watchdog(void);
extern int nmi_watchdog_tick(struct pt_regs *regs, unsigned reason);
extern void cpu_nmi_set_wd_enabled(void);

extern atomic_t nmi_active;
extern unsigned int nmi_watchdog;
#define NMI_NONE	0
#define NMI_IO_APIC	1
#define NMI_LOCAL_APIC	2
#define NMI_INVALID	3

struct ctl_table;
extern int proc_nmi_enabled(struct ctl_table *, int ,
			void __user *, size_t *, loff_t *);
extern int unknown_nmi_panic;

void arch_trigger_all_cpu_backtrace(void);
#define arch_trigger_all_cpu_backtrace arch_trigger_all_cpu_backtrace

static inline void localise_nmi_watchdog(void)
{
	if (nmi_watchdog == NMI_IO_APIC)
		nmi_watchdog = NMI_LOCAL_APIC;
}

/* check if nmi_watchdog is active (ie was specified at boot) */
static inline int nmi_watchdog_active(void)
{
	/*
	 * actually it should be:
	 * 	return (nmi_watchdog == NMI_LOCAL_APIC ||
	 * 		nmi_watchdog == NMI_IO_APIC)
	 * but since they are power of two we could use a
	 * cheaper way --cvg
	 */
	return nmi_watchdog & (NMI_LOCAL_APIC | NMI_IO_APIC);
}
#endif

void lapic_watchdog_stop(void);
int lapic_watchdog_init(unsigned nmi_hz);
int lapic_wd_event(unsigned nmi_hz);
unsigned lapic_adjust_nmi_hz(unsigned hz);
void disable_lapic_nmi_watchdog(void);
void enable_lapic_nmi_watchdog(void);
void stop_nmi(void);
void restart_nmi(void);

#endif /* _ASM_X86_NMI_H */
