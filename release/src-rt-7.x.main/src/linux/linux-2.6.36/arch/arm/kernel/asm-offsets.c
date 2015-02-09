/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 * Copyright (C) 1995-2003 Russell King
 *               2001-2002 Keith Owens
 *     
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <asm/mach/arch.h>
#include <asm/thread_info.h>
#include <asm/memory.h>
#include <asm/procinfo.h>
#include <linux/kbuild.h>

/*
 * Make sure that the compiler and target are compatible.
 */
#if defined(__APCS_26__)
#error Sorry, your compiler targets APCS-26 but this kernel requires APCS-32
#endif
#if (__GNUC__ == 3 && __GNUC_MINOR__ < 3)
#error Your compiler is too buggy; it is known to miscompile kernels.
#error    Known good compilers: 3.3
#endif

int main(void)
{
  DEFINE(TSK_ACTIVE_MM,		offsetof(struct task_struct, active_mm));
#ifdef CONFIG_CC_STACKPROTECTOR
  DEFINE(TSK_STACK_CANARY,	offsetof(struct task_struct, stack_canary));
#endif
  BLANK();
  DEFINE(TI_FLAGS,		offsetof(struct thread_info, flags));
  DEFINE(TI_PREEMPT,		offsetof(struct thread_info, preempt_count));
  DEFINE(TI_ADDR_LIMIT,		offsetof(struct thread_info, addr_limit));
  DEFINE(TI_TASK,		offsetof(struct thread_info, task));
  DEFINE(TI_EXEC_DOMAIN,	offsetof(struct thread_info, exec_domain));
  DEFINE(TI_CPU,		offsetof(struct thread_info, cpu));
  DEFINE(TI_CPU_DOMAIN,		offsetof(struct thread_info, cpu_domain));
  DEFINE(TI_CPU_SAVE,		offsetof(struct thread_info, cpu_context));
  DEFINE(TI_USED_CP,		offsetof(struct thread_info, used_cp));
  DEFINE(TI_TP_VALUE,		offsetof(struct thread_info, tp_value));
  DEFINE(TI_FPSTATE,		offsetof(struct thread_info, fpstate));
  DEFINE(TI_VFPSTATE,		offsetof(struct thread_info, vfpstate));
#ifdef CONFIG_ARM_THUMBEE
  DEFINE(TI_THUMBEE_STATE,	offsetof(struct thread_info, thumbee_state));
#endif
#ifdef CONFIG_IWMMXT
  DEFINE(TI_IWMMXT_STATE,	offsetof(struct thread_info, fpstate.iwmmxt));
#endif
#ifdef CONFIG_CRUNCH
  DEFINE(TI_CRUNCH_STATE,	offsetof(struct thread_info, crunchstate));
#endif
  BLANK();
  DEFINE(S_R0,			offsetof(struct pt_regs, ARM_r0));
  DEFINE(S_R1,			offsetof(struct pt_regs, ARM_r1));
  DEFINE(S_R2,			offsetof(struct pt_regs, ARM_r2));
  DEFINE(S_R3,			offsetof(struct pt_regs, ARM_r3));
  DEFINE(S_R4,			offsetof(struct pt_regs, ARM_r4));
  DEFINE(S_R5,			offsetof(struct pt_regs, ARM_r5));
  DEFINE(S_R6,			offsetof(struct pt_regs, ARM_r6));
  DEFINE(S_R7,			offsetof(struct pt_regs, ARM_r7));
  DEFINE(S_R8,			offsetof(struct pt_regs, ARM_r8));
  DEFINE(S_R9,			offsetof(struct pt_regs, ARM_r9));
  DEFINE(S_R10,			offsetof(struct pt_regs, ARM_r10));
  DEFINE(S_FP,			offsetof(struct pt_regs, ARM_fp));
  DEFINE(S_IP,			offsetof(struct pt_regs, ARM_ip));
  DEFINE(S_SP,			offsetof(struct pt_regs, ARM_sp));
  DEFINE(S_LR,			offsetof(struct pt_regs, ARM_lr));
  DEFINE(S_PC,			offsetof(struct pt_regs, ARM_pc));
  DEFINE(S_PSR,			offsetof(struct pt_regs, ARM_cpsr));
  DEFINE(S_OLD_R0,		offsetof(struct pt_regs, ARM_ORIG_r0));
  DEFINE(S_FRAME_SIZE,		sizeof(struct pt_regs));
  BLANK();
#ifdef CONFIG_CPU_HAS_ASID
  DEFINE(MM_CONTEXT_ID,		offsetof(struct mm_struct, context.id));
  BLANK();
#endif
  DEFINE(VMA_VM_MM,		offsetof(struct vm_area_struct, vm_mm));
  DEFINE(VMA_VM_FLAGS,		offsetof(struct vm_area_struct, vm_flags));
  BLANK();
  DEFINE(VM_EXEC,	       	VM_EXEC);
  BLANK();
  DEFINE(PAGE_SZ,	       	PAGE_SIZE);
  BLANK();
  DEFINE(SYS_ERROR0,		0x9f0000);
  BLANK();
  DEFINE(SIZEOF_MACHINE_DESC,	sizeof(struct machine_desc));
  DEFINE(MACHINFO_TYPE,		offsetof(struct machine_desc, nr));
  DEFINE(MACHINFO_NAME,		offsetof(struct machine_desc, name));
  DEFINE(MACHINFO_PHYSIO,	offsetof(struct machine_desc, phys_io));
  DEFINE(MACHINFO_PGOFFIO,	offsetof(struct machine_desc, io_pg_offst));
  BLANK();
  DEFINE(PROC_INFO_SZ,		sizeof(struct proc_info_list));
  DEFINE(PROCINFO_INITFUNC,	offsetof(struct proc_info_list, __cpu_flush));
  DEFINE(PROCINFO_MM_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_mm_mmu_flags));
  DEFINE(PROCINFO_IO_MMUFLAGS,	offsetof(struct proc_info_list, __cpu_io_mmu_flags));
  BLANK();
#ifdef MULTI_DABORT
  DEFINE(PROCESSOR_DABT_FUNC,	offsetof(struct processor, _data_abort));
#endif
#ifdef MULTI_PABORT
  DEFINE(PROCESSOR_PABT_FUNC,	offsetof(struct processor, _prefetch_abort));
#endif
  BLANK();
  DEFINE(DMA_BIDIRECTIONAL,	DMA_BIDIRECTIONAL);
  DEFINE(DMA_TO_DEVICE,		DMA_TO_DEVICE);
  DEFINE(DMA_FROM_DEVICE,	DMA_FROM_DEVICE);
  return 0; 
}
