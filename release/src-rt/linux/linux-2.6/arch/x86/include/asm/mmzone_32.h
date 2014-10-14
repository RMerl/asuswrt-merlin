/*
 * Written by Pat Gaughen (gone@us.ibm.com) Mar 2002
 *
 */

#ifndef _ASM_X86_MMZONE_32_H
#define _ASM_X86_MMZONE_32_H

#include <asm/smp.h>

#ifdef CONFIG_NUMA
extern struct pglist_data *node_data[];
#define NODE_DATA(nid)	(node_data[nid])

#include <asm/numaq.h>
/* summit or generic arch */
#include <asm/srat.h>

extern int get_memcfg_numa_flat(void);
/*
 * This allows any one NUMA architecture to be compiled
 * for, and still fall back to the flat function if it
 * fails.
 */
static inline void get_memcfg_numa(void)
{

	if (get_memcfg_numaq())
		return;
	if (get_memcfg_from_srat())
		return;
	get_memcfg_numa_flat();
}

extern void resume_map_numa_kva(pgd_t *pgd);

#else /* !CONFIG_NUMA */

#define get_memcfg_numa get_memcfg_numa_flat

static inline void resume_map_numa_kva(pgd_t *pgd) {}

#endif /* CONFIG_NUMA */

#ifdef CONFIG_DISCONTIGMEM

/*
 * generic node memory support, the following assumptions apply:
 *
 * 1) memory comes in 64Mb contiguous chunks which are either present or not
 * 2) we will not have more than 64Gb in total
 *
 * for now assume that 64Gb is max amount of RAM for whole system
 *    64Gb / 4096bytes/page = 16777216 pages
 */
#define MAX_NR_PAGES 16777216
#define MAX_ELEMENTS 1024
#define PAGES_PER_ELEMENT (MAX_NR_PAGES/MAX_ELEMENTS)

extern s8 physnode_map[];

static inline int pfn_to_nid(unsigned long pfn)
{
#ifdef CONFIG_NUMA
	return((int) physnode_map[(pfn) / PAGES_PER_ELEMENT]);
#else
	return 0;
#endif
}

/*
 * Following are macros that each numa implmentation must define.
 */

#define node_start_pfn(nid)	(NODE_DATA(nid)->node_start_pfn)
#define node_end_pfn(nid)						\
({									\
	pg_data_t *__pgdat = NODE_DATA(nid);				\
	__pgdat->node_start_pfn + __pgdat->node_spanned_pages;		\
})

static inline int pfn_valid(int pfn)
{
	int nid = pfn_to_nid(pfn);

	if (nid >= 0)
		return (pfn < node_end_pfn(nid));
	return 0;
}

#endif /* CONFIG_DISCONTIGMEM */

#ifdef CONFIG_NEED_MULTIPLE_NODES
/* always use node 0 for bootmem on this numa platform */
#define bootmem_arch_preferred_node(__bdata, size, align, goal, limit)	\
	(NODE_DATA(0)->bdata)
#endif /* CONFIG_NEED_MULTIPLE_NODES */

#endif /* _ASM_X86_MMZONE_32_H */
