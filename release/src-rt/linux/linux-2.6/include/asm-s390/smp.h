/*
 *  include/asm-s390/smp.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *               Heiko Carstens (heiko.carstens@de.ibm.com)
 */
#ifndef __ASM_SMP_H
#define __ASM_SMP_H

#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/bitops.h>

#if defined(__KERNEL__) && defined(CONFIG_SMP) && !defined(__ASSEMBLY__)

#include <asm/lowcore.h>
#include <asm/sigp.h>
#include <asm/ptrace.h>

/*
  s390 specific smp.c headers
 */
typedef struct
{
	int        intresting;
	sigp_ccode ccode; 
	__u32      status;
	__u16      cpu;
} sigp_info;

extern void machine_restart_smp(char *);
extern void machine_halt_smp(void);
extern void machine_power_off_smp(void);

extern void smp_setup_cpu_possible_map(void);
extern int smp_call_function_on(void (*func) (void *info), void *info,
				int nonatomic, int wait, int cpu);
#define NO_PROC_ID		0xFF		/* No processor magic marker */

/*
 *	This magic constant controls our willingness to transfer
 *	a process across CPUs. Such a transfer incurs misses on the L1
 *	cache, and on a P6 or P5 with multiple L2 caches L2 hits. My
 *	gut feeling is this will vary by board in value. For a board
 *	with separate L2 cache it probably depends also on the RSS, and
 *	for a board with shared L2 cache it ought to decay fast as other
 *	processes are run.
 */
 
#define PROC_CHANGE_PENALTY	20		/* Schedule penalty */

#define raw_smp_processor_id()	(S390_lowcore.cpu_data.cpu_nr)

static inline __u16 hard_smp_processor_id(void)
{
        __u16 cpu_address;
 
	asm volatile("stap %0" : "=m" (cpu_address));
        return cpu_address;
}

/*
 * returns 1 if cpu is in stopped/check stopped state or not operational
 * returns 0 otherwise
 */
static inline int
smp_cpu_not_running(int cpu)
{
	__u32 status;

	switch (signal_processor_ps(&status, 0, cpu, sigp_sense)) {
	case sigp_order_code_accepted:
	case sigp_status_stored:
		/* Check for stopped and check stop state */
		if (status & 0x50)
			return 1;
		break;
	case sigp_not_operational:
		return 1;
	default:
		break;
	}
	return 0;
}

#define cpu_logical_map(cpu) (cpu)

extern int __cpu_disable (void);
extern void __cpu_die (unsigned int cpu);
extern void cpu_die (void) __attribute__ ((noreturn));
extern int __cpu_up (unsigned int cpu);

#endif

#ifndef CONFIG_SMP
static inline int
smp_call_function_on(void (*func) (void *info), void *info,
		     int nonatomic, int wait, int cpu)
{
	func(info);
	return 0;
}

static inline void smp_send_stop(void)
{
	/* Disable all interrupts/machine checks */
	__load_psw_mask(psw_kernel_bits & ~PSW_MASK_MCHECK);
}

#define hard_smp_processor_id()		0
#define smp_cpu_not_running(cpu)	1
#define smp_setup_cpu_possible_map()	do { } while (0)
#endif

extern union save_area *zfcpdump_save_areas[NR_CPUS + 1];
#endif
