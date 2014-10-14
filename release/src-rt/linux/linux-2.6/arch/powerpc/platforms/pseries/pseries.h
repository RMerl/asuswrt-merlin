/*
 * Copyright 2006 IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _PSERIES_PSERIES_H
#define _PSERIES_PSERIES_H

#include <linux/interrupt.h>

struct device_node;

extern void request_event_sources_irqs(struct device_node *np,
				       irq_handler_t handler, const char *name);

#include <linux/of.h>

extern void __init fw_feature_init(const char *hypertas, unsigned long len);

struct pt_regs;

extern int pSeries_system_reset_exception(struct pt_regs *regs);
extern int pSeries_machine_check_exception(struct pt_regs *regs);

#ifdef CONFIG_SMP
extern void smp_init_pseries_mpic(void);
extern void smp_init_pseries_xics(void);
#else
static inline void smp_init_pseries_mpic(void) { };
static inline void smp_init_pseries_xics(void) { };
#endif

#ifdef CONFIG_KEXEC
extern void setup_kexec_cpu_down_xics(void);
extern void setup_kexec_cpu_down_mpic(void);
#else
static inline void setup_kexec_cpu_down_xics(void) { }
static inline void setup_kexec_cpu_down_mpic(void) { }
#endif

extern void pSeries_final_fixup(void);

/* Poweron flag used for enabling auto ups restart */
extern unsigned long rtas_poweron_auto;

extern void find_udbg_vterm(void);

/* Dynamic logical Partitioning/Mobility */
extern void dlpar_free_cc_nodes(struct device_node *);
extern void dlpar_free_cc_property(struct property *);
extern struct device_node *dlpar_configure_connector(u32);
extern int dlpar_attach_node(struct device_node *);
extern int dlpar_detach_node(struct device_node *);

#endif /* _PSERIES_PSERIES_H */
