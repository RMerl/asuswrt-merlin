#ifndef _XT_HASHLIMIT_H
#define _XT_HASHLIMIT_H

#include <linux/types.h>

/* timings are in milliseconds. */
#define XT_HASHLIMIT_SCALE 10000
/* 1/10,000 sec period => max of 10,000/sec.  Min rate is then 429490
   seconds, or one every 59 hours. */

/* details of this structure hidden by the implementation */
struct xt_hashlimit_htable;

enum {
	XT_HASHLIMIT_HASH_DIP = 1 << 0,
	XT_HASHLIMIT_HASH_DPT = 1 << 1,
	XT_HASHLIMIT_HASH_SIP = 1 << 2,
	XT_HASHLIMIT_HASH_SPT = 1 << 3,
	XT_HASHLIMIT_INVERT   = 1 << 4,
};

struct hashlimit_cfg {
	__u32 mode;	  /* bitmask of XT_HASHLIMIT_HASH_* */
	__u32 avg;    /* Average secs between packets * scale */
	__u32 burst;  /* Period multiplier for upper limit. */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;	/* when do entries expire? */
};

struct xt_hashlimit_info {
	char name [IFNAMSIZ];		/* name */
	struct hashlimit_cfg cfg;

	/* Used internally by the kernel */
	struct xt_hashlimit_htable *hinfo;
	union {
		void *ptr;
		struct xt_hashlimit_info *master;
	} u;
};

struct hashlimit_cfg1 {
	__u32 mode;	  /* bitmask of XT_HASHLIMIT_HASH_* */
	__u32 avg;    /* Average secs between packets * scale */
	__u32 burst;  /* Period multiplier for upper limit. */

	/* user specified */
	__u32 size;		/* how many buckets */
	__u32 max;		/* max number of entries */
	__u32 gc_interval;	/* gc interval */
	__u32 expire;	/* when do entries expire? */

	__u8 srcmask, dstmask;
};

struct xt_hashlimit_mtinfo1 {
	char name[IFNAMSIZ];
	struct hashlimit_cfg1 cfg;

	/* Used internally by the kernel */
	struct xt_hashlimit_htable *hinfo __attribute__((aligned(8)));
};

#endif /*_XT_HASHLIMIT_H*/
