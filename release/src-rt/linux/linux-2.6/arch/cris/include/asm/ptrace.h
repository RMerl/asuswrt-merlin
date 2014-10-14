#ifndef _CRIS_PTRACE_H
#define _CRIS_PTRACE_H

#include <arch/ptrace.h>

#ifdef __KERNEL__

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
#define PTRACE_GETREGS            12
#define PTRACE_SETREGS            13

#define profile_pc(regs) instruction_pointer(regs)

#endif /* __KERNEL__ */

#endif /* _CRIS_PTRACE_H */
