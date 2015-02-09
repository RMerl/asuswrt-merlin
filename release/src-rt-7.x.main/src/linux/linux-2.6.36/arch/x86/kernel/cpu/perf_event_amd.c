#ifdef CONFIG_CPU_SUP_AMD

static DEFINE_RAW_SPINLOCK(amd_nb_lock);

static __initconst const u64 amd_hw_cache_event_ids
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] =
{
 [ C(L1D) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0040, /* Data Cache Accesses        */
		[ C(RESULT_MISS)   ] = 0x0041, /* Data Cache Misses          */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x0142, /* Data Cache Refills :system */
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x0267, /* Data Prefetcher :attempts  */
		[ C(RESULT_MISS)   ] = 0x0167, /* Data Prefetcher :cancelled */
	},
 },
 [ C(L1I ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0080, /* Instruction cache fetches  */
		[ C(RESULT_MISS)   ] = 0x0081, /* Instruction cache misses   */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0x014B, /* Prefetch Instructions :Load */
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(LL  ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x037D, /* Requests to L2 Cache :IC+DC */
		[ C(RESULT_MISS)   ] = 0x037E, /* L2 Cache Misses : IC+DC     */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0x017F, /* L2 Fill/Writeback           */
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(DTLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0040, /* Data Cache Accesses        */
		[ C(RESULT_MISS)   ] = 0x0746, /* L1_DTLB_AND_L2_DLTB_MISS.ALL */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = 0,
		[ C(RESULT_MISS)   ] = 0,
	},
 },
 [ C(ITLB) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x0080, /* Instruction fecthes        */
		[ C(RESULT_MISS)   ] = 0x0385, /* L1_ITLB_AND_L2_ITLB_MISS.ALL */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
 [ C(BPU ) ] = {
	[ C(OP_READ) ] = {
		[ C(RESULT_ACCESS) ] = 0x00c2, /* Retired Branch Instr.      */
		[ C(RESULT_MISS)   ] = 0x00c3, /* Retired Mispredicted BI    */
	},
	[ C(OP_WRITE) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
	[ C(OP_PREFETCH) ] = {
		[ C(RESULT_ACCESS) ] = -1,
		[ C(RESULT_MISS)   ] = -1,
	},
 },
};

/*
 * AMD Performance Monitor K7 and later.
 */
static const u64 amd_perfmon_event_map[] =
{
  [PERF_COUNT_HW_CPU_CYCLES]		= 0x0076,
  [PERF_COUNT_HW_INSTRUCTIONS]		= 0x00c0,
  [PERF_COUNT_HW_CACHE_REFERENCES]	= 0x0080,
  [PERF_COUNT_HW_CACHE_MISSES]		= 0x0081,
  [PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= 0x00c2,
  [PERF_COUNT_HW_BRANCH_MISSES]		= 0x00c3,
};

static u64 amd_pmu_event_map(int hw_event)
{
	return amd_perfmon_event_map[hw_event];
}

static int amd_pmu_hw_config(struct perf_event *event)
{
	int ret = x86_pmu_hw_config(event);

	if (ret)
		return ret;

	if (event->attr.type != PERF_TYPE_RAW)
		return 0;

	event->hw.config |= event->attr.config & AMD64_RAW_EVENT_MASK;

	return 0;
}

/*
 * AMD64 events are detected based on their event codes.
 */
static inline int amd_is_nb_event(struct hw_perf_event *hwc)
{
	return (hwc->config & 0xe0) == 0xe0;
}

static inline int amd_has_nb(struct cpu_hw_events *cpuc)
{
	struct amd_nb *nb = cpuc->amd_nb;

	return nb && nb->nb_id != -1;
}

static void amd_put_event_constraints(struct cpu_hw_events *cpuc,
				      struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct amd_nb *nb = cpuc->amd_nb;
	int i;

	/*
	 * only care about NB events
	 */
	if (!(amd_has_nb(cpuc) && amd_is_nb_event(hwc)))
		return;

	/*
	 * need to scan whole list because event may not have
	 * been assigned during scheduling
	 *
	 * no race condition possible because event can only
	 * be removed on one CPU at a time AND PMU is disabled
	 * when we come here
	 */
	for (i = 0; i < x86_pmu.num_counters; i++) {
		if (nb->owners[i] == event) {
			cmpxchg(nb->owners+i, event, NULL);
			break;
		}
	}
}

 /*
  * AMD64 NorthBridge events need special treatment because
  * counter access needs to be synchronized across all cores
  * of a package. Refer to BKDG section 3.12
  *
  * NB events are events measuring L3 cache, Hypertransport
  * traffic. They are identified by an event code >= 0xe00.
  * They measure events on the NorthBride which is shared
  * by all cores on a package. NB events are counted on a
  * shared set of counters. When a NB event is programmed
  * in a counter, the data actually comes from a shared
  * counter. Thus, access to those counters needs to be
  * synchronized.
  *
  * We implement the synchronization such that no two cores
  * can be measuring NB events using the same counters. Thus,
  * we maintain a per-NB allocation table. The available slot
  * is propagated using the event_constraint structure.
  *
  * We provide only one choice for each NB event based on
  * the fact that only NB events have restrictions. Consequently,
  * if a counter is available, there is a guarantee the NB event
  * will be assigned to it. If no slot is available, an empty
  * constraint is returned and scheduling will eventually fail
  * for this event.
  *
  * Note that all cores attached the same NB compete for the same
  * counters to host NB events, this is why we use atomic ops. Some
  * multi-chip CPUs may have more than one NB.
  *
  * Given that resources are allocated (cmpxchg), they must be
  * eventually freed for others to use. This is accomplished by
  * calling amd_put_event_constraints().
  *
  * Non NB events are not impacted by this restriction.
  */
static struct event_constraint *
amd_get_event_constraints(struct cpu_hw_events *cpuc, struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	struct amd_nb *nb = cpuc->amd_nb;
	struct perf_event *old = NULL;
	int max = x86_pmu.num_counters;
	int i, j, k = -1;

	/*
	 * if not NB event or no NB, then no constraints
	 */
	if (!(amd_has_nb(cpuc) && amd_is_nb_event(hwc)))
		return &unconstrained;

	/*
	 * detect if already present, if so reuse
	 *
	 * cannot merge with actual allocation
	 * because of possible holes
	 *
	 * event can already be present yet not assigned (in hwc->idx)
	 * because of successive calls to x86_schedule_events() from
	 * hw_perf_group_sched_in() without hw_perf_enable()
	 */
	for (i = 0; i < max; i++) {
		/*
		 * keep track of first free slot
		 */
		if (k == -1 && !nb->owners[i])
			k = i;

		/* already present, reuse */
		if (nb->owners[i] == event)
			goto done;
	}
	/*
	 * not present, so grab a new slot
	 * starting either at:
	 */
	if (hwc->idx != -1) {
		/* previous assignment */
		i = hwc->idx;
	} else if (k != -1) {
		/* start from free slot found */
		i = k;
	} else {
		/*
		 * event not found, no slot found in
		 * first pass, try again from the
		 * beginning
		 */
		i = 0;
	}
	j = i;
	do {
		old = cmpxchg(nb->owners+i, NULL, event);
		if (!old)
			break;
		if (++i == max)
			i = 0;
	} while (i != j);
done:
	if (!old)
		return &nb->event_constraints[i];

	return &emptyconstraint;
}

static struct amd_nb *amd_alloc_nb(int cpu, int nb_id)
{
	struct amd_nb *nb;
	int i;

	nb = kmalloc(sizeof(struct amd_nb), GFP_KERNEL);
	if (!nb)
		return NULL;

	memset(nb, 0, sizeof(*nb));
	nb->nb_id = nb_id;

	/*
	 * initialize all possible NB constraints
	 */
	for (i = 0; i < x86_pmu.num_counters; i++) {
		__set_bit(i, nb->event_constraints[i].idxmsk);
		nb->event_constraints[i].weight = 1;
	}
	return nb;
}

static int amd_pmu_cpu_prepare(int cpu)
{
	struct cpu_hw_events *cpuc = &per_cpu(cpu_hw_events, cpu);

	WARN_ON_ONCE(cpuc->amd_nb);

	if (boot_cpu_data.x86_max_cores < 2)
		return NOTIFY_OK;

	cpuc->amd_nb = amd_alloc_nb(cpu, -1);
	if (!cpuc->amd_nb)
		return NOTIFY_BAD;

	return NOTIFY_OK;
}

static void amd_pmu_cpu_starting(int cpu)
{
	struct cpu_hw_events *cpuc = &per_cpu(cpu_hw_events, cpu);
	struct amd_nb *nb;
	int i, nb_id;

	if (boot_cpu_data.x86_max_cores < 2)
		return;

	nb_id = amd_get_nb_id(cpu);
	WARN_ON_ONCE(nb_id == BAD_APICID);

	raw_spin_lock(&amd_nb_lock);

	for_each_online_cpu(i) {
		nb = per_cpu(cpu_hw_events, i).amd_nb;
		if (WARN_ON_ONCE(!nb))
			continue;

		if (nb->nb_id == nb_id) {
			kfree(cpuc->amd_nb);
			cpuc->amd_nb = nb;
			break;
		}
	}

	cpuc->amd_nb->nb_id = nb_id;
	cpuc->amd_nb->refcnt++;

	raw_spin_unlock(&amd_nb_lock);
}

static void amd_pmu_cpu_dead(int cpu)
{
	struct cpu_hw_events *cpuhw;

	if (boot_cpu_data.x86_max_cores < 2)
		return;

	cpuhw = &per_cpu(cpu_hw_events, cpu);

	raw_spin_lock(&amd_nb_lock);

	if (cpuhw->amd_nb) {
		struct amd_nb *nb = cpuhw->amd_nb;

		if (nb->nb_id == -1 || --nb->refcnt == 0)
			kfree(nb);

		cpuhw->amd_nb = NULL;
	}

	raw_spin_unlock(&amd_nb_lock);
}

static __initconst const struct x86_pmu amd_pmu = {
	.name			= "AMD",
	.handle_irq		= x86_pmu_handle_irq,
	.disable_all		= x86_pmu_disable_all,
	.enable_all		= x86_pmu_enable_all,
	.enable			= x86_pmu_enable_event,
	.disable		= x86_pmu_disable_event,
	.hw_config		= amd_pmu_hw_config,
	.schedule_events	= x86_schedule_events,
	.eventsel		= MSR_K7_EVNTSEL0,
	.perfctr		= MSR_K7_PERFCTR0,
	.event_map		= amd_pmu_event_map,
	.max_events		= ARRAY_SIZE(amd_perfmon_event_map),
	.num_counters		= 4,
	.cntval_bits		= 48,
	.cntval_mask		= (1ULL << 48) - 1,
	.apic			= 1,
	/* use highest bit to detect overflow */
	.max_period		= (1ULL << 47) - 1,
	.get_event_constraints	= amd_get_event_constraints,
	.put_event_constraints	= amd_put_event_constraints,

	.cpu_prepare		= amd_pmu_cpu_prepare,
	.cpu_starting		= amd_pmu_cpu_starting,
	.cpu_dead		= amd_pmu_cpu_dead,
};

static __init int amd_pmu_init(void)
{
	/* Performance-monitoring supported from K7 and later: */
	if (boot_cpu_data.x86 < 6)
		return -ENODEV;

	x86_pmu = amd_pmu;

	/* Events are common for all AMDs */
	memcpy(hw_cache_event_ids, amd_hw_cache_event_ids,
	       sizeof(hw_cache_event_ids));

	return 0;
}

#else /* CONFIG_CPU_SUP_AMD */

static int amd_pmu_init(void)
{
	return 0;
}

#endif
