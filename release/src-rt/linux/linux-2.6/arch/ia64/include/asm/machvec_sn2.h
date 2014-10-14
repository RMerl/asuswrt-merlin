/*
 * Copyright (c) 2002-2003,2006 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of version 2 of the GNU General Public License 
 * as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * 
 * Further, this software is distributed without any warranty that it is 
 * free of the rightful claim of any third person regarding infringement 
 * or the like.  Any license provided herein, whether implied or 
 * otherwise, applies only to this software file.  Patent licenses, if 
 * any, provided herein do not apply to combinations of this program with 
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write the Free Software 
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/NoticeExplan
 */

#ifndef _ASM_IA64_MACHVEC_SN2_H
#define _ASM_IA64_MACHVEC_SN2_H

extern ia64_mv_setup_t sn_setup;
extern ia64_mv_cpu_init_t sn_cpu_init;
extern ia64_mv_irq_init_t sn_irq_init;
extern ia64_mv_send_ipi_t sn2_send_IPI;
extern ia64_mv_timer_interrupt_t sn_timer_interrupt;
extern ia64_mv_global_tlb_purge_t sn2_global_tlb_purge;
extern ia64_mv_tlb_migrate_finish_t	sn_tlb_migrate_finish;
extern ia64_mv_irq_to_vector sn_irq_to_vector;
extern ia64_mv_local_vector_to_irq sn_local_vector_to_irq;
extern ia64_mv_pci_get_legacy_mem_t sn_pci_get_legacy_mem;
extern ia64_mv_pci_legacy_read_t sn_pci_legacy_read;
extern ia64_mv_pci_legacy_write_t sn_pci_legacy_write;
extern ia64_mv_inb_t __sn_inb;
extern ia64_mv_inw_t __sn_inw;
extern ia64_mv_inl_t __sn_inl;
extern ia64_mv_outb_t __sn_outb;
extern ia64_mv_outw_t __sn_outw;
extern ia64_mv_outl_t __sn_outl;
extern ia64_mv_mmiowb_t __sn_mmiowb;
extern ia64_mv_readb_t __sn_readb;
extern ia64_mv_readw_t __sn_readw;
extern ia64_mv_readl_t __sn_readl;
extern ia64_mv_readq_t __sn_readq;
extern ia64_mv_readb_t __sn_readb_relaxed;
extern ia64_mv_readw_t __sn_readw_relaxed;
extern ia64_mv_readl_t __sn_readl_relaxed;
extern ia64_mv_readq_t __sn_readq_relaxed;
extern ia64_mv_dma_get_required_mask	sn_dma_get_required_mask;
extern ia64_mv_dma_init			sn_dma_init;
extern ia64_mv_migrate_t		sn_migrate;
extern ia64_mv_kernel_launch_event_t	sn_kernel_launch_event;
extern ia64_mv_setup_msi_irq_t		sn_setup_msi_irq;
extern ia64_mv_teardown_msi_irq_t	sn_teardown_msi_irq;
extern ia64_mv_pci_fixup_bus_t		sn_pci_fixup_bus;


/*
 * This stuff has dual use!
 *
 * For a generic kernel, the macros are used to initialize the
 * platform's machvec structure.  When compiling a non-generic kernel,
 * the macros are used directly.
 */
#define platform_name			"sn2"
#define platform_setup			sn_setup
#define platform_cpu_init		sn_cpu_init
#define platform_irq_init		sn_irq_init
#define platform_send_ipi		sn2_send_IPI
#define platform_timer_interrupt	sn_timer_interrupt
#define platform_global_tlb_purge       sn2_global_tlb_purge
#define platform_tlb_migrate_finish	sn_tlb_migrate_finish
#define platform_pci_fixup		sn_pci_fixup
#define platform_inb			__sn_inb
#define platform_inw			__sn_inw
#define platform_inl			__sn_inl
#define platform_outb			__sn_outb
#define platform_outw			__sn_outw
#define platform_outl			__sn_outl
#define platform_mmiowb			__sn_mmiowb
#define platform_readb			__sn_readb
#define platform_readw			__sn_readw
#define platform_readl			__sn_readl
#define platform_readq			__sn_readq
#define platform_readb_relaxed		__sn_readb_relaxed
#define platform_readw_relaxed		__sn_readw_relaxed
#define platform_readl_relaxed		__sn_readl_relaxed
#define platform_readq_relaxed		__sn_readq_relaxed
#define platform_irq_to_vector		sn_irq_to_vector
#define platform_local_vector_to_irq	sn_local_vector_to_irq
#define platform_pci_get_legacy_mem	sn_pci_get_legacy_mem
#define platform_pci_legacy_read	sn_pci_legacy_read
#define platform_pci_legacy_write	sn_pci_legacy_write
#define platform_dma_get_required_mask	sn_dma_get_required_mask
#define platform_dma_init		sn_dma_init
#define platform_migrate		sn_migrate
#define platform_kernel_launch_event    sn_kernel_launch_event
#ifdef CONFIG_PCI_MSI
#define platform_setup_msi_irq		sn_setup_msi_irq
#define platform_teardown_msi_irq	sn_teardown_msi_irq
#else
#define platform_setup_msi_irq		((ia64_mv_setup_msi_irq_t*)NULL)
#define platform_teardown_msi_irq	((ia64_mv_teardown_msi_irq_t*)NULL)
#endif
#define platform_pci_fixup_bus		sn_pci_fixup_bus

#include <asm/sn/io.h>

#endif /* _ASM_IA64_MACHVEC_SN2_H */
