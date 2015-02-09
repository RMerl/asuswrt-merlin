/*
 * spu aware cpufreq governor for the cell processor
 *
 * © Copyright IBM Corporation 2006-2008
 *
 * Author: Christian Krafft <krafft@de.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>
#include <asm/machdep.h>
#include <asm/spu.h>

#define POLL_TIME	100000		/* in µs */
#define EXP		753		/* exp(-1) in fixed-point */

struct spu_gov_info_struct {
	unsigned long busy_spus;	/* fixed-point */
	struct cpufreq_policy *policy;
	struct delayed_work work;
	unsigned int poll_int;		/* µs */
};
static DEFINE_PER_CPU(struct spu_gov_info_struct, spu_gov_info);

static struct workqueue_struct *kspugov_wq;

static int calc_freq(struct spu_gov_info_struct *info)
{
	int cpu;
	int busy_spus;

	cpu = info->policy->cpu;
	busy_spus = atomic_read(&cbe_spu_info[cpu_to_node(cpu)].busy_spus);

	CALC_LOAD(info->busy_spus, EXP, busy_spus * FIXED_1);
	pr_debug("cpu %d: busy_spus=%d, info->busy_spus=%ld\n",
			cpu, busy_spus, info->busy_spus);

	return info->policy->max * info->busy_spus / FIXED_1;
}

static void spu_gov_work(struct work_struct *work)
{
	struct spu_gov_info_struct *info;
	int delay;
	unsigned long target_freq;

	info = container_of(work, struct spu_gov_info_struct, work.work);

	/* after cancel_delayed_work_sync we unset info->policy */
	BUG_ON(info->policy == NULL);

	target_freq = calc_freq(info);
	__cpufreq_driver_target(info->policy, target_freq, CPUFREQ_RELATION_H);

	delay = usecs_to_jiffies(info->poll_int);
	queue_delayed_work_on(info->policy->cpu, kspugov_wq, &info->work, delay);
}

static void spu_gov_init_work(struct spu_gov_info_struct *info)
{
	int delay = usecs_to_jiffies(info->poll_int);
	INIT_DELAYED_WORK_DEFERRABLE(&info->work, spu_gov_work);
	queue_delayed_work_on(info->policy->cpu, kspugov_wq, &info->work, delay);
}

static void spu_gov_cancel_work(struct spu_gov_info_struct *info)
{
	cancel_delayed_work_sync(&info->work);
}

static int spu_gov_govern(struct cpufreq_policy *policy, unsigned int event)
{
	unsigned int cpu = policy->cpu;
	struct spu_gov_info_struct *info, *affected_info;
	int i;
	int ret = 0;

	info = &per_cpu(spu_gov_info, cpu);

	switch (event) {
	case CPUFREQ_GOV_START:
		if (!cpu_online(cpu)) {
			printk(KERN_ERR "cpu %d is not online\n", cpu);
			ret = -EINVAL;
			break;
		}

		if (!policy->cur) {
			printk(KERN_ERR "no cpu specified in policy\n");
			ret = -EINVAL;
			break;
		}

		/* initialize spu_gov_info for all affected cpus */
		for_each_cpu(i, policy->cpus) {
			affected_info = &per_cpu(spu_gov_info, i);
			affected_info->policy = policy;
		}

		info->poll_int = POLL_TIME;

		/* setup timer */
		spu_gov_init_work(info);

		break;

	case CPUFREQ_GOV_STOP:
		/* cancel timer */
		spu_gov_cancel_work(info);

		/* clean spu_gov_info for all affected cpus */
		for_each_cpu (i, policy->cpus) {
			info = &per_cpu(spu_gov_info, i);
			info->policy = NULL;
		}

		break;
	}

	return ret;
}

static struct cpufreq_governor spu_governor = {
	.name = "spudemand",
	.governor = spu_gov_govern,
	.owner = THIS_MODULE,
};

/*
 * module init and destoy
 */

static int __init spu_gov_init(void)
{
	int ret;

	kspugov_wq = create_workqueue("kspugov");
	if (!kspugov_wq) {
		printk(KERN_ERR "creation of kspugov failed\n");
		ret = -EFAULT;
		goto out;
	}

	ret = cpufreq_register_governor(&spu_governor);
	if (ret) {
		printk(KERN_ERR "registration of governor failed\n");
		destroy_workqueue(kspugov_wq);
		goto out;
	}
out:
	return ret;
}

static void __exit spu_gov_exit(void)
{
	cpufreq_unregister_governor(&spu_governor);
	destroy_workqueue(kspugov_wq);
}


module_init(spu_gov_init);
module_exit(spu_gov_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Krafft <krafft@de.ibm.com>");
