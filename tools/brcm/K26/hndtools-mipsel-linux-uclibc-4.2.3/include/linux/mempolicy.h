#ifndef _LINUX_MEMPOLICY_H
#define _LINUX_MEMPOLICY_H 1

#include <linux/errno.h>

/*
 * NUMA memory policies for Linux.
 * Copyright 2003,2004 Andi Kleen SuSE Labs
 */

/* Policies */
#define MPOL_DEFAULT	0
#define MPOL_PREFERRED	1
#define MPOL_BIND	2
#define MPOL_INTERLEAVE	3

#define MPOL_MAX MPOL_INTERLEAVE

/* Flags for get_mem_policy */
#define MPOL_F_NODE	(1<<0)	/* return next IL mode instead of node mask */
#define MPOL_F_ADDR	(1<<1)	/* look up vma using address */

/* Flags for mbind */
#define MPOL_MF_STRICT	(1<<0)	/* Verify existing pages in the mapping */
#define MPOL_MF_MOVE	(1<<1)	/* Move pages owned by this process to conform to mapping */
#define MPOL_MF_MOVE_ALL (1<<2)	/* Move every page to conform to mapping */
#define MPOL_MF_INTERNAL (1<<3)	/* Internal flags start here */


#endif
