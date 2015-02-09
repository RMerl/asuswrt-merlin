#ifndef _LINUX_U64_STATS_SYNC_H
#define _LINUX_U64_STATS_SYNC_H

/*
 * To properly implement 64bits network statistics on 32bit and 64bit hosts,
 * we provide a synchronization point, that is a noop on 64bit or UP kernels.
 *
 * Key points :
 * 1) Use a seqcount on SMP 32bits, with low overhead.
 * 2) Whole thing is a noop on 64bit arches or UP kernels.
 * 3) Write side must ensure mutual exclusion or one seqcount update could
 *    be lost, thus blocking readers forever.
 *    If this synchronization point is not a mutex, but a spinlock or
 *    spinlock_bh() or disable_bh() :
 * 3.1) Write side should not sleep.
 * 3.2) Write side should not allow preemption.
 * 3.3) If applicable, interrupts should be disabled.
 *
 * 4) If reader fetches several counters, there is no guarantee the whole values
 *    are consistent (remember point 1) : this is a noop on 64bit arches anyway)
 *
 * 5) readers are allowed to sleep or be preempted/interrupted : They perform
 *    pure reads. But if they have to fetch many values, it's better to not allow
 *    preemptions/interruptions to avoid many retries.
 *
 * 6) If counter might be written by an interrupt, readers should block interrupts.
 *    (On UP, there is no seqcount_t protection, a reader allowing interrupts could
 *     read partial values)
 *
 * 7) For softirq uses, readers can use u64_stats_fetch_begin_bh() and
 *    u64_stats_fetch_retry_bh() helpers
 *
 * Usage :
 *
 * Stats producer (writer) should use following template granted it already got
 * an exclusive access to counters (a lock is already taken, or per cpu
 * data is used [in a non preemptable context])
 *
 *   spin_lock_bh(...) or other synchronization to get exclusive access
 *   ...
 *   u64_stats_update_begin(&stats->syncp);
 *   stats->bytes64 += len; // non atomic operation
 *   stats->packets64++;    // non atomic operation
 *   u64_stats_update_end(&stats->syncp);
 *
 * While a consumer (reader) should use following template to get consistent
 * snapshot for each variable (but no guarantee on several ones)
 *
 * u64 tbytes, tpackets;
 * unsigned int start;
 *
 * do {
 *         start = u64_stats_fetch_begin(&stats->syncp);
 *         tbytes = stats->bytes64; // non atomic operation
 *         tpackets = stats->packets64; // non atomic operation
 * } while (u64_stats_fetch_retry(&stats->syncp, start));
 *
 *
 * Example of use in drivers/net/loopback.c, using per_cpu containers,
 * in BH disabled context.
 */
#include <linux/seqlock.h>

struct u64_stats_sync {
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	seqcount_t	seq;
#endif
};

static void inline u64_stats_update_begin(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_begin(&syncp->seq);
#endif
}

static void inline u64_stats_update_end(struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	write_seqcount_end(&syncp->seq);
#endif
}

static unsigned int inline u64_stats_fetch_begin(const struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if BITS_PER_LONG==32
	preempt_disable();
#endif
	return 0;
#endif
}

static bool inline u64_stats_fetch_retry(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if BITS_PER_LONG==32
	preempt_enable();
#endif
	return false;
#endif
}

/*
 * In case softirq handlers can update u64 counters, readers can use following helpers
 * - SMP 32bit arches use seqcount protection, irq safe.
 * - UP 32bit must disable BH.
 * - 64bit have no problem atomically reading u64 values, irq safe.
 */
static unsigned int inline u64_stats_fetch_begin_bh(const struct u64_stats_sync *syncp)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_begin(&syncp->seq);
#else
#if BITS_PER_LONG==32
	local_bh_disable();
#endif
	return 0;
#endif
}

static bool inline u64_stats_fetch_retry_bh(const struct u64_stats_sync *syncp,
					 unsigned int start)
{
#if BITS_PER_LONG==32 && defined(CONFIG_SMP)
	return read_seqcount_retry(&syncp->seq, start);
#else
#if BITS_PER_LONG==32
	local_bh_enable();
#endif
	return false;
#endif
}

#endif /* _LINUX_U64_STATS_SYNC_H */
