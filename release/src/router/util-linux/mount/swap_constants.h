/*
 * It is too painful to get these out of <linux/swap.h>
 * (which again requires <asm/page.h> etc).
 * These exist since Linux 1.3.2.
 */

#ifndef SWAP_FLAG_PREFER
#define SWAP_FLAG_PREFER	0x8000	/* set if swap priority specified */
#endif
#ifndef SWAP_FLAG_PRIO_MASK
#define SWAP_FLAG_PRIO_MASK	0x7fff
#endif
#ifndef SWAP_FLAG_PRIO_SHIFT
#define SWAP_FLAG_PRIO_SHIFT	0
#endif
