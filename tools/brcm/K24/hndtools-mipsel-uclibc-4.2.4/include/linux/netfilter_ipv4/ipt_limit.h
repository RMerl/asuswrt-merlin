#ifndef _IPT_RATE_H
#define _IPT_RATE_H

/* timings are in milliseconds. */
#define IPT_LIMIT_SCALE 10000

/* 1/10,000 sec period => max of 10,000/sec.  Min rate is then 429490
   seconds, or one every 59 hours. */
struct ipt_rateinfo {
	u_int32_t avg;    /* Average secs between packets * scale */
	u_int32_t burst;  /* Period multiplier for upper limit. */

#ifdef KERNEL_64_USERSPACE_32
	u_int64_t prev;
	u_int64_t placeholder;
#else
	/* Used internally by the kernel */
	unsigned long prev;
	/* Ugly, ugly fucker. */
	struct ipt_rateinfo *master;
#endif

	u_int32_t credit;
	u_int32_t credit_cap, cost;
};
#endif /*_IPT_RATE_H*/
