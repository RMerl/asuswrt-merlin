/*
 *  linux/mm/oom_kill.c
 * 
 *  Copyright (C)  1998,2000  Rik van Riel
 *	Thanks go out to Claus Fischer for some serious inspiration and
 *	for goading me into coding this file...
 *  Copyright (C)  2010  Google, Inc.
 *	Rewritten by David Rientjes
 *
 *  The routines in this file are used to kill a process when
 *  we're seriously out of memory. This gets called from __alloc_pages()
 *  in mm/page_alloc.c when we really run out of memory.
 *
 *  Since we won't call these routines often (on a well-configured
 *  machine) this file will double as a 'coding guide' and a signpost
 *  for newbie kernel hackers. It features several pointers to major
 *  kernel subsystems and hints as to where to find out what things do.
 */

#include <linux/oom.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/timex.h>
#include <linux/jiffies.h>
#include <linux/cpuset.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/security.h>

int sysctl_panic_on_oom;
int sysctl_oom_kill_allocating_task;
int sysctl_oom_dump_tasks = 1;
static DEFINE_SPINLOCK(zone_scan_lock);

#ifdef CONFIG_NUMA
/**
 * has_intersects_mems_allowed() - check task eligiblity for kill
 * @tsk: task struct of which task to consider
 * @mask: nodemask passed to page allocator for mempolicy ooms
 *
 * Task eligibility is determined by whether or not a candidate task, @tsk,
 * shares the same mempolicy nodes as current if it is bound by such a policy
 * and whether or not it has the same set of allowed cpuset nodes.
 */
static bool has_intersects_mems_allowed(struct task_struct *tsk,
					const nodemask_t *mask)
{
	struct task_struct *start = tsk;

	do {
		if (mask) {
			/*
			 * If this is a mempolicy constrained oom, tsk's
			 * cpuset is irrelevant.  Only return true if its
			 * mempolicy intersects current, otherwise it may be
			 * needlessly killed.
			 */
			if (mempolicy_nodemask_intersects(tsk, mask))
				return true;
		} else {
			/*
			 * This is not a mempolicy constrained oom, so only
			 * check the mems of tsk's cpuset.
			 */
			if (cpuset_mems_allowed_intersects(current, tsk))
				return true;
		}
	} while_each_thread(start, tsk);

	return false;
}
#else
static bool has_intersects_mems_allowed(struct task_struct *tsk,
					const nodemask_t *mask)
{
	return true;
}
#endif /* CONFIG_NUMA */

/*
 * If this is a system OOM (not a memcg OOM) and the task selected to be
 * killed is not already running at high (RT) priorities, speed up the
 * recovery by boosting the dying task to the lowest FIFO priority.
 * That helps with the recovery and avoids interfering with RT tasks.
 */
static void boost_dying_task_prio(struct task_struct *p,
				  struct mem_cgroup *mem)
{
	struct sched_param param = { .sched_priority = 1 };

	if (mem)
		return;

	if (!rt_task(p))
		sched_setscheduler_nocheck(p, SCHED_FIFO, &param);
}

/*
 * The process p may have detached its own ->mm while exiting or through
 * use_mm(), but one or more of its subthreads may still have a valid
 * pointer.  Return p, or any of its subthreads with a valid ->mm, with
 * task_lock() held.
 */
struct task_struct *find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t = p;

	do {
		task_lock(t);
		if (likely(t->mm))
			return t;
		task_unlock(t);
	} while_each_thread(p, t);

	return NULL;
}

/* return true if the task is not adequate as candidate victim task. */
static bool oom_unkillable_task(struct task_struct *p,
		const struct mem_cgroup *mem, const nodemask_t *nodemask)
{
	if (is_global_init(p))
		return true;
	if (p->flags & PF_KTHREAD)
		return true;

	/* When mem_cgroup_out_of_memory() and p is not member of the group */
	if (mem && !task_in_mem_cgroup(p, mem))
		return true;

	/* p may not have freeable memory in nodemask */
	if (!has_intersects_mems_allowed(p, nodemask))
		return true;

	return false;
}

/**
 * oom_badness - heuristic function to determine which candidate task to kill
 * @p: task struct of which task we should calculate
 * @totalpages: total present RAM allowed for page allocation
 *
 * The heuristic for determining which task to kill is made to be as simple and
 * predictable as possible.  The goal is to return the highest value for the
 * task consuming the most memory to avoid subsequent oom failures.
 */
unsigned int oom_badness(struct task_struct *p, struct mem_cgroup *mem,
		      const nodemask_t *nodemask, unsigned long totalpages)
{
	int points;

	if (oom_unkillable_task(p, mem, nodemask))
		return 0;

	p = find_lock_task_mm(p);
	if (!p)
		return 0;

	/*
	 * Shortcut check for OOM_SCORE_ADJ_MIN so the entire heuristic doesn't
	 * need to be executed for something that cannot be killed.
	 */
	if (p->signal->oom_score_adj == OOM_SCORE_ADJ_MIN) {
		task_unlock(p);
		return 0;
	}

	/*
	 * When the PF_OOM_ORIGIN bit is set, it indicates the task should have
	 * priority for oom killing.
	 */
	if (p->flags & PF_OOM_ORIGIN) {
		task_unlock(p);
		return 1000;
	}

	/*
	 * The memory controller may have a limit of 0 bytes, so avoid a divide
	 * by zero, if necessary.
	 */
	if (!totalpages)
		totalpages = 1;

	/*
	 * The baseline for the badness score is the proportion of RAM that each
	 * task's rss and swap space use.
	 */
	points = (get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS)) * 1000 /
			totalpages;
	task_unlock(p);

	/*
	 * Root processes get 3% bonus, just like the __vm_enough_memory()
	 * implementation used by LSMs.
	 */
	if (has_capability_noaudit(p, CAP_SYS_ADMIN))
		points -= 30;

	/*
	 * /proc/pid/oom_score_adj ranges from -1000 to +1000 such that it may
	 * either completely disable oom killing or always prefer a certain
	 * task.
	 */
	points += p->signal->oom_score_adj;

	/*
	 * Never return 0 for an eligible task that may be killed since it's
	 * possible that no single user task uses more than 0.1% of memory and
	 * no single admin tasks uses more than 3.0%.
	 */
	if (points <= 0)
		return 1;
	return (points < 1000) ? points : 1000;
}

/*
 * Determine the type of allocation constraint.
 */
#ifdef CONFIG_NUMA
static enum oom_constraint constrained_alloc(struct zonelist *zonelist,
				gfp_t gfp_mask, nodemask_t *nodemask,
				unsigned long *totalpages)
{
	struct zone *zone;
	struct zoneref *z;
	enum zone_type high_zoneidx = gfp_zone(gfp_mask);
	bool cpuset_limited = false;
	int nid;

	/* Default to all available memory */
	*totalpages = totalram_pages + total_swap_pages;

	if (!zonelist)
		return CONSTRAINT_NONE;
	/*
	 * Reach here only when __GFP_NOFAIL is used. So, we should avoid
	 * to kill current.We have to random task kill in this case.
	 * Hopefully, CONSTRAINT_THISNODE...but no way to handle it, now.
	 */
	if (gfp_mask & __GFP_THISNODE)
		return CONSTRAINT_NONE;

	/*
	 * This is not a __GFP_THISNODE allocation, so a truncated nodemask in
	 * the page allocator means a mempolicy is in effect.  Cpuset policy
	 * is enforced in get_page_from_freelist().
	 */
	if (nodemask && !nodes_subset(node_states[N_HIGH_MEMORY], *nodemask)) {
		*totalpages = total_swap_pages;
		for_each_node_mask(nid, *nodemask)
			*totalpages += node_spanned_pages(nid);
		return CONSTRAINT_MEMORY_POLICY;
	}

	/* Check this allocation failure is caused by cpuset's wall function */
	for_each_zone_zonelist_nodemask(zone, z, zonelist,
			high_zoneidx, nodemask)
		if (!cpuset_zone_allowed_softwall(zone, gfp_mask))
			cpuset_limited = true;

	if (cpuset_limited) {
		*totalpages = total_swap_pages;
		for_each_node_mask(nid, cpuset_current_mems_allowed)
			*totalpages += node_spanned_pages(nid);
		return CONSTRAINT_CPUSET;
	}
	return CONSTRAINT_NONE;
}
#else
static enum oom_constraint constrained_alloc(struct zonelist *zonelist,
				gfp_t gfp_mask, nodemask_t *nodemask,
				unsigned long *totalpages)
{
	*totalpages = totalram_pages + total_swap_pages;
	return CONSTRAINT_NONE;
}
#endif

/*
 * Simple selection loop. We chose the process with the highest
 * number of 'points'. We expect the caller will lock the tasklist.
 *
 * (not docbooked, we don't want this one cluttering up the manual)
 */
static struct task_struct *select_bad_process(unsigned int *ppoints,
		unsigned long totalpages, struct mem_cgroup *mem,
		const nodemask_t *nodemask)
{
	struct task_struct *p;
	struct task_struct *chosen = NULL;
	*ppoints = 0;

	for_each_process(p) {
		unsigned int points;

		if (oom_unkillable_task(p, mem, nodemask))
			continue;

		/*
		 * This task already has access to memory reserves and is
		 * being killed. Don't allow any other task access to the
		 * memory reserve.
		 *
		 * Note: this may have a chance of deadlock if it gets
		 * blocked waiting for another task which itself is waiting
		 * for memory. Is there a better alternative?
		 */
		if (test_tsk_thread_flag(p, TIF_MEMDIE))
			return ERR_PTR(-1UL);

		/*
		 * This is in the process of releasing memory so wait for it
		 * to finish before killing some other task by mistake.
		 *
		 * However, if p is the current task, we allow the 'kill' to
		 * go ahead if it is exiting: this will simply set TIF_MEMDIE,
		 * which will allow it to gain access to memory reserves in
		 * the process of exiting and releasing its resources.
		 * Otherwise we could get an easy OOM deadlock.
		 */
		if (thread_group_empty(p) && (p->flags & PF_EXITING) && p->mm) {
			if (p != current)
				return ERR_PTR(-1UL);

			chosen = p;
			*ppoints = 1000;
		}

		points = oom_badness(p, mem, nodemask, totalpages);
		if (points > *ppoints) {
			chosen = p;
			*ppoints = points;
		}
	}

	return chosen;
}

/**
 * dump_tasks - dump current memory state of all system tasks
 * @mem: current's memory controller, if constrained
 * @nodemask: nodemask passed to page allocator for mempolicy ooms
 *
 * Dumps the current memory state of all eligible tasks.  Tasks not in the same
 * memcg, not in the same cpuset, or bound to a disjoint set of mempolicy nodes
 * are not shown.
 * State information includes task's pid, uid, tgid, vm size, rss, cpu, oom_adj
 * value, oom_score_adj value, and name.
 *
 * Call with tasklist_lock read-locked.
 */
static void dump_tasks(const struct mem_cgroup *mem, const nodemask_t *nodemask)
{
	struct task_struct *p;
	struct task_struct *task;

	pr_info("[ pid ]   uid  tgid total_vm      rss cpu oom_adj oom_score_adj name\n");
	for_each_process(p) {
		if (oom_unkillable_task(p, mem, nodemask))
			continue;

		task = find_lock_task_mm(p);
		if (!task) {
			/*
			 * This is a kthread or all of p's threads have already
			 * detached their mm's.  There's no need to report
			 * them; they can't be oom killed anyway.
			 */
			continue;
		}

		pr_info("[%5d] %5d %5d %8lu %8lu %3u     %3d         %5d %s\n",
			task->pid, task_uid(task), task->tgid,
			task->mm->total_vm, get_mm_rss(task->mm),
			task_cpu(task), task->signal->oom_adj,
			task->signal->oom_score_adj, task->comm);
		task_unlock(task);
	}
}

static void dump_header(struct task_struct *p, gfp_t gfp_mask, int order,
			struct mem_cgroup *mem, const nodemask_t *nodemask)
{
	task_lock(current);
	pr_warning("%s invoked oom-killer: gfp_mask=0x%x, order=%d, "
		"oom_adj=%d, oom_score_adj=%d\n",
		current->comm, gfp_mask, order, current->signal->oom_adj,
		current->signal->oom_score_adj);
	cpuset_print_task_mems_allowed(current);
	task_unlock(current);
	dump_stack();
	mem_cgroup_print_oom_info(mem, p);
	show_mem();
	if (sysctl_oom_dump_tasks)
		dump_tasks(mem, nodemask);
}

#define K(x) ((x) << (PAGE_SHIFT-10))
static int oom_kill_task(struct task_struct *p, struct mem_cgroup *mem)
{
	p = find_lock_task_mm(p);
	if (!p)
		return 1;

	pr_err("Killed process %d (%s) total-vm:%lukB, anon-rss:%lukB, file-rss:%lukB\n",
		task_pid_nr(p), p->comm, K(p->mm->total_vm),
		K(get_mm_counter(p->mm, MM_ANONPAGES)),
		K(get_mm_counter(p->mm, MM_FILEPAGES)));
	task_unlock(p);


	set_tsk_thread_flag(p, TIF_MEMDIE);
	force_sig(SIGKILL, p);

	/*
	 * We give our sacrificial lamb high priority and access to
	 * all the memory it needs. That way it should be able to
	 * exit() and clear out its resources quickly...
	 */
	boost_dying_task_prio(p, mem);

	return 0;
}
#undef K

static int oom_kill_process(struct task_struct *p, gfp_t gfp_mask, int order,
			    unsigned int points, unsigned long totalpages,
			    struct mem_cgroup *mem, nodemask_t *nodemask,
			    const char *message)
{
	struct task_struct *victim = p;
	struct task_struct *child;
	struct task_struct *t = p;
	unsigned int victim_points = 0;

	if (printk_ratelimit())
		dump_header(p, gfp_mask, order, mem, nodemask);

	/*
	 * If the task is already exiting, don't alarm the sysadmin or kill
	 * its children or threads, just set TIF_MEMDIE so it can die quickly
	 */
	if (p->flags & PF_EXITING) {
		set_tsk_thread_flag(p, TIF_MEMDIE);
		boost_dying_task_prio(p, mem);
		return 0;
	}

	task_lock(p);
	pr_err("%s: Kill process %d (%s) score %d or sacrifice child\n",
		message, task_pid_nr(p), p->comm, points);
	task_unlock(p);

	/*
	 * If any of p's children has a different mm and is eligible for kill,
	 * the one with the highest badness() score is sacrificed for its
	 * parent.  This attempts to lose the minimal amount of work done while
	 * still freeing memory.
	 */
	do {
		list_for_each_entry(child, &t->children, sibling) {
			unsigned int child_points;

			/*
			 * oom_badness() returns 0 if the thread is unkillable
			 */
			child_points = oom_badness(child, mem, nodemask,
								totalpages);
			if (child_points > victim_points) {
				victim = child;
				victim_points = child_points;
			}
		}
	} while_each_thread(p, t);

	return oom_kill_task(victim, mem);
}

/*
 * Determines whether the kernel must panic because of the panic_on_oom sysctl.
 */
static void check_panic_on_oom(enum oom_constraint constraint, gfp_t gfp_mask,
				int order, const nodemask_t *nodemask)
{
	if (likely(!sysctl_panic_on_oom))
		return;
	if (sysctl_panic_on_oom != 2) {
		/*
		 * panic_on_oom == 1 only affects CONSTRAINT_NONE, the kernel
		 * does not panic for cpuset, mempolicy, or memcg allocation
		 * failures.
		 */
		if (constraint != CONSTRAINT_NONE)
			return;
	}
	read_lock(&tasklist_lock);
	dump_header(NULL, gfp_mask, order, NULL, nodemask);
	read_unlock(&tasklist_lock);
	panic("Out of memory: %s panic_on_oom is enabled\n",
		sysctl_panic_on_oom == 2 ? "compulsory" : "system-wide");
}

#ifdef CONFIG_CGROUP_MEM_RES_CTLR
void mem_cgroup_out_of_memory(struct mem_cgroup *mem, gfp_t gfp_mask)
{
	unsigned long limit;
	unsigned int points = 0;
	struct task_struct *p;

	check_panic_on_oom(CONSTRAINT_MEMCG, gfp_mask, 0, NULL);
	limit = mem_cgroup_get_limit(mem) >> PAGE_SHIFT;
	read_lock(&tasklist_lock);
retry:
	p = select_bad_process(&points, limit, mem, NULL);
	if (!p || PTR_ERR(p) == -1UL)
		goto out;

	if (oom_kill_process(p, gfp_mask, 0, points, limit, mem, NULL,
				"Memory cgroup out of memory"))
		goto retry;
out:
	read_unlock(&tasklist_lock);
}
#endif

static BLOCKING_NOTIFIER_HEAD(oom_notify_list);

int register_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_oom_notifier);

int unregister_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_oom_notifier);

/*
 * Try to acquire the OOM killer lock for the zones in zonelist.  Returns zero
 * if a parallel OOM killing is already taking place that includes a zone in
 * the zonelist.  Otherwise, locks all zones in the zonelist and returns 1.
 */
int try_set_zonelist_oom(struct zonelist *zonelist, gfp_t gfp_mask)
{
	struct zoneref *z;
	struct zone *zone;
	int ret = 1;

	spin_lock(&zone_scan_lock);
	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		if (zone_is_oom_locked(zone)) {
			ret = 0;
			goto out;
		}
	}

	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		/*
		 * Lock each zone in the zonelist under zone_scan_lock so a
		 * parallel invocation of try_set_zonelist_oom() doesn't succeed
		 * when it shouldn't.
		 */
		zone_set_flag(zone, ZONE_OOM_LOCKED);
	}

out:
	spin_unlock(&zone_scan_lock);
	return ret;
}

/*
 * Clears the ZONE_OOM_LOCKED flag for all zones in the zonelist so that failed
 * allocation attempts with zonelists containing them may now recall the OOM
 * killer, if necessary.
 */
void clear_zonelist_oom(struct zonelist *zonelist, gfp_t gfp_mask)
{
	struct zoneref *z;
	struct zone *zone;

	spin_lock(&zone_scan_lock);
	for_each_zone_zonelist(zone, z, zonelist, gfp_zone(gfp_mask)) {
		zone_clear_flag(zone, ZONE_OOM_LOCKED);
	}
	spin_unlock(&zone_scan_lock);
}

/*
 * Try to acquire the oom killer lock for all system zones.  Returns zero if a
 * parallel oom killing is taking place, otherwise locks all zones and returns
 * non-zero.
 */
static int try_set_system_oom(void)
{
	struct zone *zone;
	int ret = 1;

	spin_lock(&zone_scan_lock);
	for_each_populated_zone(zone)
		if (zone_is_oom_locked(zone)) {
			ret = 0;
			goto out;
		}
	for_each_populated_zone(zone)
		zone_set_flag(zone, ZONE_OOM_LOCKED);
out:
	spin_unlock(&zone_scan_lock);
	return ret;
}

/*
 * Clears ZONE_OOM_LOCKED for all system zones so that failed allocation
 * attempts or page faults may now recall the oom killer, if necessary.
 */
static void clear_system_oom(void)
{
	struct zone *zone;

	spin_lock(&zone_scan_lock);
	for_each_populated_zone(zone)
		zone_clear_flag(zone, ZONE_OOM_LOCKED);
	spin_unlock(&zone_scan_lock);
}

/**
 * out_of_memory - kill the "best" process when we run out of memory
 * @zonelist: zonelist pointer
 * @gfp_mask: memory allocation flags
 * @order: amount of memory being requested as a power of 2
 * @nodemask: nodemask passed to page allocator
 *
 * If we run out of memory, we have the choice between either
 * killing a random task (bad), letting the system crash (worse)
 * OR try to be smart about which process to kill. Note that we
 * don't have to be perfect here, we just have to be good.
 */
void out_of_memory(struct zonelist *zonelist, gfp_t gfp_mask,
		int order, nodemask_t *nodemask)
{
	const nodemask_t *mpol_mask;
	struct task_struct *p;
	unsigned long totalpages;
	unsigned long freed = 0;
	unsigned int points;
	enum oom_constraint constraint = CONSTRAINT_NONE;
	int killed = 0;

	blocking_notifier_call_chain(&oom_notify_list, 0, &freed);
	if (freed > 0)
		/* Got some memory back in the last second. */
		return;

	/*
	 * If current has a pending SIGKILL, then automatically select it.  The
	 * goal is to allow it to allocate so that it may quickly exit and free
	 * its memory.
	 */
	if (fatal_signal_pending(current)) {
		set_thread_flag(TIF_MEMDIE);
		boost_dying_task_prio(current, NULL);
		return;
	}

	/*
	 * Check if there were limitations on the allocation (only relevant for
	 * NUMA) that may require different handling.
	 */
	constraint = constrained_alloc(zonelist, gfp_mask, nodemask,
						&totalpages);
	mpol_mask = (constraint == CONSTRAINT_MEMORY_POLICY) ? nodemask : NULL;
	check_panic_on_oom(constraint, gfp_mask, order, mpol_mask);

	read_lock(&tasklist_lock);
	if (sysctl_oom_kill_allocating_task &&
	    !oom_unkillable_task(current, NULL, nodemask) &&
	    (current->signal->oom_adj != OOM_DISABLE)) {
		/*
		 * oom_kill_process() needs tasklist_lock held.  If it returns
		 * non-zero, current could not be killed so we must fallback to
		 * the tasklist scan.
		 */
		if (!oom_kill_process(current, gfp_mask, order, 0, totalpages,
				NULL, nodemask,
				"Out of memory (oom_kill_allocating_task)"))
			goto out;
	}

retry:
	p = select_bad_process(&points, totalpages, NULL, mpol_mask);
	if (PTR_ERR(p) == -1UL)
		goto out;

	/* Found nothing?!?! Either we hang forever, or we panic. */
	if (!p) {
		dump_header(NULL, gfp_mask, order, NULL, mpol_mask);
		read_unlock(&tasklist_lock);
		panic("Out of memory and no killable processes...\n");
	}

	if (oom_kill_process(p, gfp_mask, order, points, totalpages, NULL,
				nodemask, "Out of memory"))
		goto retry;
	killed = 1;
out:
	read_unlock(&tasklist_lock);

	/*
	 * Give "p" a good chance of killing itself before we
	 * retry to allocate memory unless "p" is current
	 */
	if (killed && !test_thread_flag(TIF_MEMDIE))
		schedule_timeout_uninterruptible(1);
}

/*
 * The pagefault handler calls here because it is out of memory, so kill a
 * memory-hogging task.  If a populated zone has ZONE_OOM_LOCKED set, a parallel
 * oom killing is already in progress so do nothing.  If a task is found with
 * TIF_MEMDIE set, it has been killed so do nothing and allow it to exit.
 */
void pagefault_out_of_memory(void)
{
	if (try_set_system_oom()) {
		out_of_memory(NULL, 0, 0, NULL);
		clear_system_oom();
	}
	if (!test_thread_flag(TIF_MEMDIE))
		schedule_timeout_uninterruptible(1);
}
