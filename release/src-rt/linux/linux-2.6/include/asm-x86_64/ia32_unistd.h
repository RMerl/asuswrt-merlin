#ifndef _ASM_X86_64_IA32_UNISTD_H_
#define _ASM_X86_64_IA32_UNISTD_H_

/*
 * This file contains the system call numbers of the ia32 port,
 * this is for the kernel only.
 * Only add syscalls here where some part of the kernel needs to know
 * the number. This should be otherwise in sync with asm-i386/unistd.h. -AK
 */

#define __NR_ia32_restart_syscall 0
#define __NR_ia32_exit		  1
#define __NR_ia32_read		  3
#define __NR_ia32_write		  4
#define __NR_ia32_sigreturn	119
#define __NR_ia32_rt_sigreturn	173

#endif /* _ASM_X86_64_IA32_UNISTD_H_ */
