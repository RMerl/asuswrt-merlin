/*
 * Architecture specific (PPC64) functions for kexec based crash dumps.
 *
 * Copyright (C) 2005, IBM Corp.
 *
 * Created by: Haren Myneni
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 *
 */

#undef DEBUG

#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/reboot.h>
#include <linux/kexec.h>
#include <linux/bootmem.h>
#include <linux/crash_dump.h>
#include <linux/delay.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/memblock.h>

#include <asm/processor.h>
#include <asm/machdep.h>
#include <asm/kexec.h>
#include <asm/kdump.h>
#include <asm/prom.h>
#include <asm/firmware.h>
#include <asm/smp.h>
#include <asm/system.h>
#include <asm/setjmp.h>

#ifdef DEBUG
#include <asm/udbg.h>
#define DBG(fmt...) udbg_printf(fmt)
#else
#define DBG(fmt...)
#endif

/* This keeps a track of which one is crashing cpu. */
int crashing_cpu = -1;
static cpumask_t cpus_in_crash = CPU_MASK_NONE;
cpumask_t cpus_in_sr = CPU_MASK_NONE;

#define CRASH_HANDLER_MAX 2
/* NULL terminated list of shutdown handles */
static crash_shutdown_t crash_shutdown_handles[CRASH_HANDLER_MAX+1];
static DEFINE_SPINLOCK(crash_handlers_lock);

#ifdef CONFIG_SMP
static atomic_t enter_on_soft_reset = ATOMIC_INIT(0);

void crash_ipi_callback(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	if (!cpu_online(cpu))
		return;

	hard_irq_disable();
	if (!cpu_isset(cpu, cpus_in_crash))
		crash_save_cpu(regs, cpu);
	cpu_set(cpu, cpus_in_crash);

	/*
	 * Entered via soft-reset - could be the kdump
	 * process is invoked using soft-reset or user activated
	 * it if some CPU did not respond to an IPI.
	 * For soft-reset, the secondary CPU can enter this func
	 * twice. 1 - using IPI, and 2. soft-reset.
	 * Tell the kexec CPU that entered via soft-reset and ready
	 * to go down.
	 */
	if (cpu_isset(cpu, cpus_in_sr)) {
		cpu_clear(cpu, cpus_in_sr);
		atomic_inc(&enter_on_soft_reset);
	}

	/*
	 * Starting the kdump boot.
	 * This barrier is needed to make sure that all CPUs are stopped.
	 * If not, soft-reset will be invoked to bring other CPUs.
	 */
	while (!cpu_isset(crashing_cpu, cpus_in_crash))
		cpu_relax();

	if (ppc_md.kexec_cpu_down)
		ppc_md.kexec_cpu_down(1, 1);

#ifdef CONFIG_PPC64
	kexec_smp_wait();
#else
	for (;;);
#endif

	/* NOTREACHED */
}

/*
 * Wait until all CPUs are entered via soft-reset.
 */
static void crash_soft_reset_check(int cpu)
{
	unsigned int ncpus = num_online_cpus() - 1;/* Excluding the panic cpu */

	cpu_clear(cpu, cpus_in_sr);
	while (atomic_read(&enter_on_soft_reset) != ncpus)
		cpu_relax();
}


static void crash_kexec_prepare_cpus(int cpu)
{
	unsigned int msecs;

	unsigned int ncpus = num_online_cpus() - 1;/* Excluding the panic cpu */

	crash_send_ipi(crash_ipi_callback);
	smp_wmb();

	printk(KERN_EMERG "Sending IPI to other cpus...\n");
	msecs = 10000;
	while ((cpus_weight(cpus_in_crash) < ncpus) && (--msecs > 0)) {
		cpu_relax();
		mdelay(1);
	}

	/* Would it be better to replace the trap vector here? */

	if (cpus_weight(cpus_in_crash) < ncpus) {
		printk(KERN_EMERG "done waiting: %d cpu(s) not responding\n",
			ncpus - cpus_weight(cpus_in_crash));
		printk(KERN_EMERG "Activate soft-reset to stop other cpu(s)\n");
		cpus_in_sr = CPU_MASK_NONE;
		atomic_set(&enter_on_soft_reset, 0);
		while (cpus_weight(cpus_in_crash) < ncpus)
			cpu_relax();
	}
	/*
	 * Make sure all CPUs are entered via soft-reset if the kdump is
	 * invoked using soft-reset.
	 */
	if (cpu_isset(cpu, cpus_in_sr))
		crash_soft_reset_check(cpu);
	/* Leave the IPI callback set */
}

/* wait for all the CPUs to hit real mode but timeout if they don't come in */
#ifdef CONFIG_PPC_STD_MMU_64
static void crash_kexec_wait_realmode(int cpu)
{
	unsigned int msecs;
	int i;

	msecs = 10000;
	for (i=0; i < NR_CPUS && msecs > 0; i++) {
		if (i == cpu)
			continue;

		while (paca[i].kexec_state < KEXEC_STATE_REAL_MODE) {
			barrier();
			if (!cpu_possible(i)) {
				break;
			}
			if (!cpu_online(i)) {
				break;
			}
			msecs--;
			mdelay(1);
		}
	}
	mb();
}
#endif

/*
 * This function will be called by secondary cpus or by kexec cpu
 * if soft-reset is activated to stop some CPUs.
 */
void crash_kexec_secondary(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	unsigned long flags;
	int msecs = 5;

	local_irq_save(flags);
	/* Wait 5ms if the kexec CPU is not entered yet. */
	while (crashing_cpu < 0) {
		if (--msecs < 0) {
			/*
			 * Either kdump image is not loaded or
			 * kdump process is not started - Probably xmon
			 * exited using 'x'(exit and recover) or
			 * kexec_should_crash() failed for all running tasks.
			 */
			cpu_clear(cpu, cpus_in_sr);
			local_irq_restore(flags);
			return;
		}
		mdelay(1);
		cpu_relax();
	}
	if (cpu == crashing_cpu) {
		/*
		 * Panic CPU will enter this func only via soft-reset.
		 * Wait until all secondary CPUs entered and
		 * then start kexec boot.
		 */
		crash_soft_reset_check(cpu);
		cpu_set(crashing_cpu, cpus_in_crash);
		if (ppc_md.kexec_cpu_down)
			ppc_md.kexec_cpu_down(1, 0);
		machine_kexec(kexec_crash_image);
		/* NOTREACHED */
	}
	crash_ipi_callback(regs);
}

#else
static void crash_kexec_prepare_cpus(int cpu)
{
	/*
	 * move the secondarys to us so that we can copy
	 * the new kernel 0-0x100 safely
	 *
	 * do this if kexec in setup.c ?
	 */
#ifdef CONFIG_PPC64
	smp_release_cpus();
#else
#endif
}

void crash_kexec_secondary(struct pt_regs *regs)
{
	cpus_in_sr = CPU_MASK_NONE;
}
#endif
#ifdef CONFIG_SPU_BASE

#include <asm/spu.h>
#include <asm/spu_priv1.h>

struct crash_spu_info {
	struct spu *spu;
	u32 saved_spu_runcntl_RW;
	u32 saved_spu_status_R;
	u32 saved_spu_npc_RW;
	u64 saved_mfc_sr1_RW;
	u64 saved_mfc_dar;
	u64 saved_mfc_dsisr;
};

#define CRASH_NUM_SPUS	16	/* Enough for current hardware */
static struct crash_spu_info crash_spu_info[CRASH_NUM_SPUS];

static void crash_kexec_stop_spus(void)
{
	struct spu *spu;
	int i;
	u64 tmp;

	for (i = 0; i < CRASH_NUM_SPUS; i++) {
		if (!crash_spu_info[i].spu)
			continue;

		spu = crash_spu_info[i].spu;

		crash_spu_info[i].saved_spu_runcntl_RW =
			in_be32(&spu->problem->spu_runcntl_RW);
		crash_spu_info[i].saved_spu_status_R =
			in_be32(&spu->problem->spu_status_R);
		crash_spu_info[i].saved_spu_npc_RW =
			in_be32(&spu->problem->spu_npc_RW);

		crash_spu_info[i].saved_mfc_dar    = spu_mfc_dar_get(spu);
		crash_spu_info[i].saved_mfc_dsisr  = spu_mfc_dsisr_get(spu);
		tmp = spu_mfc_sr1_get(spu);
		crash_spu_info[i].saved_mfc_sr1_RW = tmp;

		tmp &= ~MFC_STATE1_MASTER_RUN_CONTROL_MASK;
		spu_mfc_sr1_set(spu, tmp);

		__delay(200);
	}
}

void crash_register_spus(struct list_head *list)
{
	struct spu *spu;

	list_for_each_entry(spu, list, full_list) {
		if (WARN_ON(spu->number >= CRASH_NUM_SPUS))
			continue;

		crash_spu_info[spu->number].spu = spu;
	}
}

#else
static inline void crash_kexec_stop_spus(void)
{
}
#endif /* CONFIG_SPU_BASE */

/*
 * Register a function to be called on shutdown.  Only use this if you
 * can't reset your device in the second kernel.
 */
int crash_shutdown_register(crash_shutdown_t handler)
{
	unsigned int i, rc;

	spin_lock(&crash_handlers_lock);
	for (i = 0 ; i < CRASH_HANDLER_MAX; i++)
		if (!crash_shutdown_handles[i]) {
			/* Insert handle at first empty entry */
			crash_shutdown_handles[i] = handler;
			rc = 0;
			break;
		}

	if (i == CRASH_HANDLER_MAX) {
		printk(KERN_ERR "Crash shutdown handles full, "
		       "not registered.\n");
		rc = 1;
	}

	spin_unlock(&crash_handlers_lock);
	return rc;
}
EXPORT_SYMBOL(crash_shutdown_register);

int crash_shutdown_unregister(crash_shutdown_t handler)
{
	unsigned int i, rc;

	spin_lock(&crash_handlers_lock);
	for (i = 0 ; i < CRASH_HANDLER_MAX; i++)
		if (crash_shutdown_handles[i] == handler)
			break;

	if (i == CRASH_HANDLER_MAX) {
		printk(KERN_ERR "Crash shutdown handle not found\n");
		rc = 1;
	} else {
		/* Shift handles down */
		for (; crash_shutdown_handles[i]; i++)
			crash_shutdown_handles[i] =
				crash_shutdown_handles[i+1];
		rc = 0;
	}

	spin_unlock(&crash_handlers_lock);
	return rc;
}
EXPORT_SYMBOL(crash_shutdown_unregister);

static unsigned long crash_shutdown_buf[JMP_BUF_LEN];
static int crash_shutdown_cpu = -1;

static int handle_fault(struct pt_regs *regs)
{
	if (crash_shutdown_cpu == smp_processor_id())
		longjmp(crash_shutdown_buf, 1);
	return 0;
}

void default_machine_crash_shutdown(struct pt_regs *regs)
{
	unsigned int i;
	int (*old_handler)(struct pt_regs *regs);


	/*
	 * This function is only called after the system
	 * has panicked or is otherwise in a critical state.
	 * The minimum amount of code to allow a kexec'd kernel
	 * to run successfully needs to happen here.
	 *
	 * In practice this means stopping other cpus in
	 * an SMP system.
	 * The kernel is broken so disable interrupts.
	 */
	hard_irq_disable();

	/*
	 * Make a note of crashing cpu. Will be used in machine_kexec
	 * such that another IPI will not be sent.
	 */
	crashing_cpu = smp_processor_id();
	crash_save_cpu(regs, crashing_cpu);
	crash_kexec_prepare_cpus(crashing_cpu);
	cpu_set(crashing_cpu, cpus_in_crash);
#if defined(CONFIG_PPC_STD_MMU_64) && defined(CONFIG_SMP)
	crash_kexec_wait_realmode(crashing_cpu);
#endif

	for_each_irq(i) {
		struct irq_desc *desc = irq_to_desc(i);

		if (!desc || !desc->chip || !desc->chip->eoi)
			continue;

		if (desc->status & IRQ_INPROGRESS)
			desc->chip->eoi(i);

		if (!(desc->status & IRQ_DISABLED))
			desc->chip->shutdown(i);
	}

	/*
	 * Call registered shutdown routines savely.  Swap out
	 * __debugger_fault_handler, and replace on exit.
	 */
	old_handler = __debugger_fault_handler;
	__debugger_fault_handler = handle_fault;
	crash_shutdown_cpu = smp_processor_id();
	for (i = 0; crash_shutdown_handles[i]; i++) {
		if (setjmp(crash_shutdown_buf) == 0) {
			/*
			 * Insert syncs and delay to ensure
			 * instructions in the dangerous region don't
			 * leak away from this protected region.
			 */
			asm volatile("sync; isync");
			/* dangerous region */
			crash_shutdown_handles[i]();
			asm volatile("sync; isync");
		}
	}
	crash_shutdown_cpu = -1;
	__debugger_fault_handler = old_handler;

	crash_kexec_stop_spus();

	if (ppc_md.kexec_cpu_down)
		ppc_md.kexec_cpu_down(1, 0);
}
