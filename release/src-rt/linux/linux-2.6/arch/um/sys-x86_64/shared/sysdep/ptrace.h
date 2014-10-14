/*
 * Copyright 2003 PathScale, Inc.
 * Copyright (C) 2003 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 *
 * Licensed under the GPL
 */

#ifndef __SYSDEP_X86_64_PTRACE_H
#define __SYSDEP_X86_64_PTRACE_H

#include "user_constants.h"
#include "sysdep/faultinfo.h"

#define MAX_REG_OFFSET (UM_FRAME_SIZE)
#define MAX_REG_NR ((MAX_REG_OFFSET) / sizeof(unsigned long))

#include "skas_ptregs.h"

#define REGS_IP(r) ((r)[HOST_IP])
#define REGS_SP(r) ((r)[HOST_SP])

#define REGS_RBX(r) ((r)[HOST_RBX])
#define REGS_RCX(r) ((r)[HOST_RCX])
#define REGS_RDX(r) ((r)[HOST_RDX])
#define REGS_RSI(r) ((r)[HOST_RSI])
#define REGS_RDI(r) ((r)[HOST_RDI])
#define REGS_RBP(r) ((r)[HOST_RBP])
#define REGS_RAX(r) ((r)[HOST_RAX])
#define REGS_R8(r) ((r)[HOST_R8])
#define REGS_R9(r) ((r)[HOST_R9])
#define REGS_R10(r) ((r)[HOST_R10])
#define REGS_R11(r) ((r)[HOST_R11])
#define REGS_R12(r) ((r)[HOST_R12])
#define REGS_R13(r) ((r)[HOST_R13])
#define REGS_R14(r) ((r)[HOST_R14])
#define REGS_R15(r) ((r)[HOST_R15])
#define REGS_CS(r) ((r)[HOST_CS])
#define REGS_EFLAGS(r) ((r)[HOST_EFLAGS])
#define REGS_SS(r) ((r)[HOST_SS])

#define HOST_FS_BASE 21
#define HOST_GS_BASE 22
#define HOST_DS 23
#define HOST_ES 24
#define HOST_FS 25
#define HOST_GS 26

/* Also defined in asm/ptrace-x86_64.h, but not in libc headers.  So, these
 * are already defined for kernel code, but not for userspace code.
 */
#ifndef FS_BASE
/* These aren't defined in ptrace.h, but exist in struct user_regs_struct,
 * which is what x86_64 ptrace actually uses.
 */
#define FS_BASE (HOST_FS_BASE * sizeof(long))
#define GS_BASE (HOST_GS_BASE * sizeof(long))
#define DS (HOST_DS * sizeof(long))
#define ES (HOST_ES * sizeof(long))
#define FS (HOST_FS * sizeof(long))
#define GS (HOST_GS * sizeof(long))
#endif

#define REGS_FS_BASE(r) ((r)[HOST_FS_BASE])
#define REGS_GS_BASE(r) ((r)[HOST_GS_BASE])
#define REGS_DS(r) ((r)[HOST_DS])
#define REGS_ES(r) ((r)[HOST_ES])
#define REGS_FS(r) ((r)[HOST_FS])
#define REGS_GS(r) ((r)[HOST_GS])

#define REGS_ORIG_RAX(r) ((r)[HOST_ORIG_RAX])

#define REGS_SET_SYSCALL_RETURN(r, res) REGS_RAX(r) = (res)

#define REGS_RESTART_SYSCALL(r) IP_RESTART_SYSCALL(REGS_IP(r))

#define REGS_SEGV_IS_FIXABLE(r) SEGV_IS_FIXABLE((r)->trap_type)

#define REGS_FAULT_ADDR(r) ((r)->fault_addr)

#define REGS_FAULT_WRITE(r) FAULT_WRITE((r)->fault_type)

#define REGS_TRAP(r) ((r)->trap_type)

#define REGS_ERR(r) ((r)->fault_type)

struct uml_pt_regs {
	unsigned long gp[MAX_REG_NR];
	struct faultinfo faultinfo;
	long syscall;
	int is_user;
};

#define EMPTY_UML_PT_REGS { }

#define UPT_RBX(r) REGS_RBX((r)->gp)
#define UPT_RCX(r) REGS_RCX((r)->gp)
#define UPT_RDX(r) REGS_RDX((r)->gp)
#define UPT_RSI(r) REGS_RSI((r)->gp)
#define UPT_RDI(r) REGS_RDI((r)->gp)
#define UPT_RBP(r) REGS_RBP((r)->gp)
#define UPT_RAX(r) REGS_RAX((r)->gp)
#define UPT_R8(r) REGS_R8((r)->gp)
#define UPT_R9(r) REGS_R9((r)->gp)
#define UPT_R10(r) REGS_R10((r)->gp)
#define UPT_R11(r) REGS_R11((r)->gp)
#define UPT_R12(r) REGS_R12((r)->gp)
#define UPT_R13(r) REGS_R13((r)->gp)
#define UPT_R14(r) REGS_R14((r)->gp)
#define UPT_R15(r) REGS_R15((r)->gp)
#define UPT_CS(r) REGS_CS((r)->gp)
#define UPT_FS_BASE(r) REGS_FS_BASE((r)->gp)
#define UPT_FS(r) REGS_FS((r)->gp)
#define UPT_GS_BASE(r) REGS_GS_BASE((r)->gp)
#define UPT_GS(r) REGS_GS((r)->gp)
#define UPT_DS(r) REGS_DS((r)->gp)
#define UPT_ES(r) REGS_ES((r)->gp)
#define UPT_CS(r) REGS_CS((r)->gp)
#define UPT_SS(r) REGS_SS((r)->gp)
#define UPT_ORIG_RAX(r) REGS_ORIG_RAX((r)->gp)

#define UPT_IP(r) REGS_IP((r)->gp)
#define UPT_SP(r) REGS_SP((r)->gp)

#define UPT_EFLAGS(r) REGS_EFLAGS((r)->gp)
#define UPT_SYSCALL_NR(r) ((r)->syscall)
#define UPT_SYSCALL_RET(r) UPT_RAX(r)

extern int user_context(unsigned long sp);

#define UPT_IS_USER(r) ((r)->is_user)

#define UPT_SYSCALL_ARG1(r) UPT_RDI(r)
#define UPT_SYSCALL_ARG2(r) UPT_RSI(r)
#define UPT_SYSCALL_ARG3(r) UPT_RDX(r)
#define UPT_SYSCALL_ARG4(r) UPT_R10(r)
#define UPT_SYSCALL_ARG5(r) UPT_R8(r)
#define UPT_SYSCALL_ARG6(r) UPT_R9(r)

struct syscall_args {
	unsigned long args[6];
};

#define SYSCALL_ARGS(r) ((struct syscall_args) \
			 { .args = { UPT_SYSCALL_ARG1(r),	 \
				     UPT_SYSCALL_ARG2(r),	 \
				     UPT_SYSCALL_ARG3(r),	 \
				     UPT_SYSCALL_ARG4(r),	 \
				     UPT_SYSCALL_ARG5(r),	 \
				     UPT_SYSCALL_ARG6(r) } } )

#define UPT_REG(regs, reg) \
	({      unsigned long val;		\
		switch(reg){						\
		case R8: val = UPT_R8(regs); break;			\
		case R9: val = UPT_R9(regs); break;			\
		case R10: val = UPT_R10(regs); break;			\
		case R11: val = UPT_R11(regs); break;			\
		case R12: val = UPT_R12(regs); break;			\
		case R13: val = UPT_R13(regs); break;			\
		case R14: val = UPT_R14(regs); break;			\
		case R15: val = UPT_R15(regs); break;			\
		case RIP: val = UPT_IP(regs); break;			\
		case RSP: val = UPT_SP(regs); break;			\
		case RAX: val = UPT_RAX(regs); break;			\
		case RBX: val = UPT_RBX(regs); break;			\
		case RCX: val = UPT_RCX(regs); break;			\
		case RDX: val = UPT_RDX(regs); break;			\
		case RSI: val = UPT_RSI(regs); break;			\
		case RDI: val = UPT_RDI(regs); break;			\
		case RBP: val = UPT_RBP(regs); break;			\
		case ORIG_RAX: val = UPT_ORIG_RAX(regs); break;		\
		case CS: val = UPT_CS(regs); break;			\
		case SS: val = UPT_SS(regs); break;			\
		case FS_BASE: val = UPT_FS_BASE(regs); break;		\
		case GS_BASE: val = UPT_GS_BASE(regs); break;		\
		case DS: val = UPT_DS(regs); break;			\
		case ES: val = UPT_ES(regs); break;			\
		case FS : val = UPT_FS (regs); break;			\
		case GS: val = UPT_GS(regs); break;			\
		case EFLAGS: val = UPT_EFLAGS(regs); break;		\
		default :						\
			panic("Bad register in UPT_REG : %d\n", reg);	\
			val = -1;					\
		}							\
		val;							\
	})


#define UPT_SET(regs, reg, val) \
	({      unsigned long __upt_val = val;	\
		switch(reg){						\
		case R8: UPT_R8(regs) = __upt_val; break;		\
		case R9: UPT_R9(regs) = __upt_val; break;		\
		case R10: UPT_R10(regs) = __upt_val; break;		\
		case R11: UPT_R11(regs) = __upt_val; break;		\
		case R12: UPT_R12(regs) = __upt_val; break;		\
		case R13: UPT_R13(regs) = __upt_val; break;		\
		case R14: UPT_R14(regs) = __upt_val; break;		\
		case R15: UPT_R15(regs) = __upt_val; break;		\
		case RIP: UPT_IP(regs) = __upt_val; break;		\
		case RSP: UPT_SP(regs) = __upt_val; break;		\
		case RAX: UPT_RAX(regs) = __upt_val; break;		\
		case RBX: UPT_RBX(regs) = __upt_val; break;		\
		case RCX: UPT_RCX(regs) = __upt_val; break;		\
		case RDX: UPT_RDX(regs) = __upt_val; break;		\
		case RSI: UPT_RSI(regs) = __upt_val; break;		\
		case RDI: UPT_RDI(regs) = __upt_val; break;		\
		case RBP: UPT_RBP(regs) = __upt_val; break;		\
		case ORIG_RAX: UPT_ORIG_RAX(regs) = __upt_val; break;	\
		case CS: UPT_CS(regs) = __upt_val; break;		\
		case SS: UPT_SS(regs) = __upt_val; break;		\
		case FS_BASE: UPT_FS_BASE(regs) = __upt_val; break;	\
		case GS_BASE: UPT_GS_BASE(regs) = __upt_val; break;	\
		case DS: UPT_DS(regs) = __upt_val; break;		\
		case ES: UPT_ES(regs) = __upt_val; break;		\
		case FS: UPT_FS(regs) = __upt_val; break;		\
		case GS: UPT_GS(regs) = __upt_val; break;		\
		case EFLAGS: UPT_EFLAGS(regs) = __upt_val; break;	\
		default :						\
			panic("Bad register in UPT_SET : %d\n", reg);	\
			break;						\
		}							\
		__upt_val;						\
	})

#define UPT_SET_SYSCALL_RETURN(r, res) \
	REGS_SET_SYSCALL_RETURN((r)->regs, (res))

#define UPT_RESTART_SYSCALL(r) REGS_RESTART_SYSCALL((r)->gp)

#define UPT_SEGV_IS_FIXABLE(r) REGS_SEGV_IS_FIXABLE(&r->skas)

#define UPT_FAULTINFO(r) (&(r)->faultinfo)

static inline void arch_init_registers(int pid)
{
}

#endif
