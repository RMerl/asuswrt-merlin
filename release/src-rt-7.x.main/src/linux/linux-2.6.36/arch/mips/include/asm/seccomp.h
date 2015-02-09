#ifndef __ASM_SECCOMP_H

#include <linux/unistd.h>

#define __NR_seccomp_read __NR_read
#define __NR_seccomp_write __NR_write
#define __NR_seccomp_exit __NR_exit
#define __NR_seccomp_sigreturn __NR_rt_sigreturn

/*
 * Kludge alert:
 *
 * The generic seccomp code currently allows only a single compat ABI.  Until
 * this is fixed we priorize O32 as the compat ABI over N32.
 */
#ifdef CONFIG_MIPS32_O32

#define __NR_seccomp_read_32		4003
#define __NR_seccomp_write_32		4004
#define __NR_seccomp_exit_32		4001
#define __NR_seccomp_sigreturn_32	4193	/* rt_sigreturn */

#elif defined(CONFIG_MIPS32_N32)

#define __NR_seccomp_read_32		6000
#define __NR_seccomp_write_32		6001
#define __NR_seccomp_exit_32		6058
#define __NR_seccomp_sigreturn_32	6211	/* rt_sigreturn */

#endif /* CONFIG_MIPS32_O32 */

#endif /* __ASM_SECCOMP_H */
