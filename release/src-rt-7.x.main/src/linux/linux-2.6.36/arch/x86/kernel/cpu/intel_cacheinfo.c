/*
 *	Routines to indentify caches on Intel CPU.
 *
 *	Changes:
 *	Venkatesh Pallipadi	: Adding cache identification through cpuid(4)
 *	Ashok Raj <ashok.raj@intel.com>: Work with CPU hotplug infrastructure.
 *	Andi Kleen / Andreas Herrmann	: CPUID4 emulation on AMD.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/compiler.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/pci.h>

#include <asm/processor.h>
#include <linux/smp.h>
#include <asm/k8.h>
#include <asm/smp.h>

#define LVL_1_INST	1
#define LVL_1_DATA	2
#define LVL_2		3
#define LVL_3		4
#define LVL_TRACE	5

struct _cache_table {
	unsigned char descriptor;
	char cache_type;
	short size;
};

#define MB(x)	((x) * 1024)

/* All the cache descriptor types we care about (no TLB or
   trace cache entries) */

static const struct _cache_table __cpuinitconst cache_table[] =
{
	{ 0x06, LVL_1_INST, 8 },	/* 4-way set assoc, 32 byte line size */
	{ 0x08, LVL_1_INST, 16 },	/* 4-way set assoc, 32 byte line size */
	{ 0x09, LVL_1_INST, 32 },	/* 4-way set assoc, 64 byte line size */
	{ 0x0a, LVL_1_DATA, 8 },	/* 2 way set assoc, 32 byte line size */
	{ 0x0c, LVL_1_DATA, 16 },	/* 4-way set assoc, 32 byte line size */
	{ 0x0d, LVL_1_DATA, 16 },	/* 4-way set assoc, 64 byte line size */
	{ 0x21, LVL_2,      256 },	/* 8-way set assoc, 64 byte line size */
	{ 0x22, LVL_3,      512 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x23, LVL_3,      MB(1) },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x25, LVL_3,      MB(2) },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x29, LVL_3,      MB(4) },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x2c, LVL_1_DATA, 32 },	/* 8-way set assoc, 64 byte line size */
	{ 0x30, LVL_1_INST, 32 },	/* 8-way set assoc, 64 byte line size */
	{ 0x39, LVL_2,      128 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x3a, LVL_2,      192 },	/* 6-way set assoc, sectored cache, 64 byte line size */
	{ 0x3b, LVL_2,      128 },	/* 2-way set assoc, sectored cache, 64 byte line size */
	{ 0x3c, LVL_2,      256 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x3d, LVL_2,      384 },	/* 6-way set assoc, sectored cache, 64 byte line size */
	{ 0x3e, LVL_2,      512 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x3f, LVL_2,      256 },	/* 2-way set assoc, 64 byte line size */
	{ 0x41, LVL_2,      128 },	/* 4-way set assoc, 32 byte line size */
	{ 0x42, LVL_2,      256 },	/* 4-way set assoc, 32 byte line size */
	{ 0x43, LVL_2,      512 },	/* 4-way set assoc, 32 byte line size */
	{ 0x44, LVL_2,      MB(1) },	/* 4-way set assoc, 32 byte line size */
	{ 0x45, LVL_2,      MB(2) },	/* 4-way set assoc, 32 byte line size */
	{ 0x46, LVL_3,      MB(4) },	/* 4-way set assoc, 64 byte line size */
	{ 0x47, LVL_3,      MB(8) },	/* 8-way set assoc, 64 byte line size */
	{ 0x49, LVL_3,      MB(4) },	/* 16-way set assoc, 64 byte line size */
	{ 0x4a, LVL_3,      MB(6) },	/* 12-way set assoc, 64 byte line size */
	{ 0x4b, LVL_3,      MB(8) },	/* 16-way set assoc, 64 byte line size */
	{ 0x4c, LVL_3,      MB(12) },	/* 12-way set assoc, 64 byte line size */
	{ 0x4d, LVL_3,      MB(16) },	/* 16-way set assoc, 64 byte line size */
	{ 0x4e, LVL_2,      MB(6) },	/* 24-way set assoc, 64 byte line size */
	{ 0x60, LVL_1_DATA, 16 },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x66, LVL_1_DATA, 8 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x67, LVL_1_DATA, 16 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x68, LVL_1_DATA, 32 },	/* 4-way set assoc, sectored cache, 64 byte line size */
	{ 0x70, LVL_TRACE,  12 },	/* 8-way set assoc */
	{ 0x71, LVL_TRACE,  16 },	/* 8-way set assoc */
	{ 0x72, LVL_TRACE,  32 },	/* 8-way set assoc */
	{ 0x73, LVL_TRACE,  64 },	/* 8-way set assoc */
	{ 0x78, LVL_2,      MB(1) },	/* 4-way set assoc, 64 byte line size */
	{ 0x79, LVL_2,      128 },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7a, LVL_2,      256 },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7b, LVL_2,      512 },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7c, LVL_2,      MB(1) },	/* 8-way set assoc, sectored cache, 64 byte line size */
	{ 0x7d, LVL_2,      MB(2) },	/* 8-way set assoc, 64 byte line size */
	{ 0x7f, LVL_2,      512 },	/* 2-way set assoc, 64 byte line size */
	{ 0x82, LVL_2,      256 },	/* 8-way set assoc, 32 byte line size */
	{ 0x83, LVL_2,      512 },	/* 8-way set assoc, 32 byte line size */
	{ 0x84, LVL_2,      MB(1) },	/* 8-way set assoc, 32 byte line size */
	{ 0x85, LVL_2,      MB(2) },	/* 8-way set assoc, 32 byte line size */
	{ 0x86, LVL_2,      512 },	/* 4-way set assoc, 64 byte line size */
	{ 0x87, LVL_2,      MB(1) },	/* 8-way set assoc, 64 byte line size */
	{ 0xd0, LVL_3,      512 },	/* 4-way set assoc, 64 byte line size */
	{ 0xd1, LVL_3,      MB(1) },	/* 4-way set assoc, 64 byte line size */
	{ 0xd2, LVL_3,      MB(2) },	/* 4-way set assoc, 64 byte line size */
	{ 0xd6, LVL_3,      MB(1) },	/* 8-way set assoc, 64 byte line size */
	{ 0xd7, LVL_3,      MB(2) },	/* 8-way set assoc, 64 byte line size */
	{ 0xd8, LVL_3,      MB(4) },	/* 12-way set assoc, 64 byte line size */
	{ 0xdc, LVL_3,      MB(2) },	/* 12-way set assoc, 64 byte line size */
	{ 0xdd, LVL_3,      MB(4) },	/* 12-way set assoc, 64 byte line size */
	{ 0xde, LVL_3,      MB(8) },	/* 12-way set assoc, 64 byte line size */
	{ 0xe2, LVL_3,      MB(2) },	/* 16-way set assoc, 64 byte line size */
	{ 0xe3, LVL_3,      MB(4) },	/* 16-way set assoc, 64 byte line size */
	{ 0xe4, LVL_3,      MB(8) },	/* 16-way set assoc, 64 byte line size */
	{ 0xea, LVL_3,      MB(12) },	/* 24-way set assoc, 64 byte line size */
	{ 0xeb, LVL_3,      MB(18) },	/* 24-way set assoc, 64 byte line size */
	{ 0xec, LVL_3,      MB(24) },	/* 24-way set assoc, 64 byte line size */
	{ 0x00, 0, 0}
};


enum _cache_type {
	CACHE_TYPE_NULL	= 0,
	CACHE_TYPE_DATA = 1,
	CACHE_TYPE_INST = 2,
	CACHE_TYPE_UNIFIED = 3
};

union _cpuid4_leaf_eax {
	struct {
		enum _cache_type	type:5;
		unsigned int		level:3;
		unsigned int		is_self_initializing:1;
		unsigned int		is_fully_associative:1;
		unsigned int		reserved:4;
		unsigned int		num_threads_sharing:12;
		unsigned int		num_cores_on_die:6;
	} split;
	u32 full;
};

union _cpuid4_leaf_ebx {
	struct {
		unsigned int		coherency_line_size:12;
		unsigned int		physical_line_partition:10;
		unsigned int		ways_of_associativity:10;
	} split;
	u32 full;
};

union _cpuid4_leaf_ecx {
	struct {
		unsigned int		number_of_sets:32;
	} split;
	u32 full;
};

struct amd_l3_cache {
	struct	 pci_dev *dev;
	bool	 can_disable;
	unsigned indices;
	u8	 subcaches[4];
};

struct _cpuid4_info {
	union _cpuid4_leaf_eax eax;
	union _cpuid4_leaf_ebx ebx;
	union _cpuid4_leaf_ecx ecx;
	unsigned long size;
	struct amd_l3_cache *l3;
	DECLARE_BITMAP(shared_cpu_map, NR_CPUS);
};

/* subset of above _cpuid4_info w/o shared_cpu_map */
struct _cpuid4_info_regs {
	union _cpuid4_leaf_eax eax;
	union _cpuid4_leaf_ebx ebx;
	union _cpuid4_leaf_ecx ecx;
	unsigned long size;
	struct amd_l3_cache *l3;
};

unsigned short			num_cache_leaves;

/* AMD doesn't have CPUID4. Emulate it here to report the same
   information to the user.  This makes some assumptions about the machine:
   L2 not shared, no SMT etc. that is currently true on AMD CPUs.

   In theory the TLBs could be reported as fake type (they are in "dummy").
   Maybe later */
union l1_cache {
	struct {
		unsigned line_size:8;
		unsigned lines_per_tag:8;
		unsigned assoc:8;
		unsigned size_in_kb:8;
	};
	unsigned val;
};

union l2_cache {
	struct {
		unsigned line_size:8;
		unsigned lines_per_tag:4;
		unsigned assoc:4;
		unsigned size_in_kb:16;
	};
	unsigned val;
};

union l3_cache {
	struct {
		unsigned line_size:8;
		unsigned lines_per_tag:4;
		unsigned assoc:4;
		unsigned res:2;
		unsigned size_encoded:14;
	};
	unsigned val;
};

static const unsigned short __cpuinitconst assocs[] = {
	[1] = 1,
	[2] = 2,
	[4] = 4,
	[6] = 8,
	[8] = 16,
	[0xa] = 32,
	[0xb] = 48,
	[0xc] = 64,
	[0xd] = 96,
	[0xe] = 128,
	[0xf] = 0xffff /* fully associative - no way to show this currently */
};

static const unsigned char __cpuinitconst levels[] = { 1, 1, 2, 3 };
static const unsigned char __cpuinitconst types[] = { 1, 2, 3, 3 };

static void __cpuinit
amd_cpuid4(int leaf, union _cpuid4_leaf_eax *eax,
		     union _cpuid4_leaf_ebx *ebx,
		     union _cpuid4_leaf_ecx *ecx)
{
	unsigned dummy;
	unsigned line_size, lines_per_tag, assoc, size_in_kb;
	union l1_cache l1i, l1d;
	union l2_cache l2;
	union l3_cache l3;
	union l1_cache *l1 = &l1d;

	eax->full = 0;
	ebx->full = 0;
	ecx->full = 0;

	cpuid(0x80000005, &dummy, &dummy, &l1d.val, &l1i.val);
	cpuid(0x80000006, &dummy, &dummy, &l2.val, &l3.val);

	switch (leaf) {
	case 1:
		l1 = &l1i;
	case 0:
		if (!l1->val)
			return;
		assoc = assocs[l1->assoc];
		line_size = l1->line_size;
		lines_per_tag = l1->lines_per_tag;
		size_in_kb = l1->size_in_kb;
		break;
	case 2:
		if (!l2.val)
			return;
		assoc = assocs[l2.assoc];
		line_size = l2.line_size;
		lines_per_tag = l2.lines_per_tag;
		/* cpu_data has errata corrections for K7 applied */
		size_in_kb = current_cpu_data.x86_cache_size;
		break;
	case 3:
		if (!l3.val)
			return;
		assoc = assocs[l3.assoc];
		line_size = l3.line_size;
		lines_per_tag = l3.lines_per_tag;
		size_in_kb = l3.size_encoded * 512;
		if (boot_cpu_has(X86_FEATURE_AMD_DCM)) {
			size_in_kb = size_in_kb >> 1;
			assoc = assoc >> 1;
		}
		break;
	default:
		return;
	}

	eax->split.is_self_initializing = 1;
	eax->split.type = types[leaf];
	eax->split.level = levels[leaf];
	eax->split.num_threads_sharing = 0;
	eax->split.num_cores_on_die = current_cpu_data.x86_max_cores - 1;


	if (assoc == 0xffff)
		eax->split.is_fully_associative = 1;
	ebx->split.coherency_line_size = line_size - 1;
	ebx->split.ways_of_associativity = assoc - 1;
	ebx->split.physical_line_partition = lines_per_tag - 1;
	ecx->split.number_of_sets = (size_in_kb * 1024) / line_size /
		(ebx->split.ways_of_associativity + 1) - 1;
}

struct _cache_attr {
	struct attribute attr;
	ssize_t (*show)(struct _cpuid4_info *, char *);
	ssize_t (*store)(struct _cpuid4_info *, const char *, size_t count);
};

#ifdef CONFIG_CPU_SUP_AMD

/*
 * L3 cache descriptors
 */
static struct amd_l3_cache **__cpuinitdata l3_caches;

static void __cpuinit amd_calc_l3_indices(struct amd_l3_cache *l3)
{
	unsigned int sc0, sc1, sc2, sc3;
	u32 val = 0;

	pci_read_config_dword(l3->dev, 0x1C4, &val);

	/* calculate subcache sizes */
	l3->subcaches[0] = sc0 = !(val & BIT(0));
	l3->subcaches[1] = sc1 = !(val & BIT(4));
	l3->subcaches[2] = sc2 = !(val & BIT(8))  + !(val & BIT(9));
	l3->subcaches[3] = sc3 = !(val & BIT(12)) + !(val & BIT(13));

	l3->indices = (max(max(max(sc0, sc1), sc2), sc3) << 10) - 1;
}

static struct amd_l3_cache * __cpuinit amd_init_l3_cache(int node)
{
	struct amd_l3_cache *l3;
	struct pci_dev *dev = node_to_k8_nb_misc(node);

	l3 = kzalloc(sizeof(struct amd_l3_cache), GFP_ATOMIC);
	if (!l3) {
		printk(KERN_WARNING "Error allocating L3 struct\n");
		return NULL;
	}

	l3->dev = dev;

	amd_calc_l3_indices(l3);

	return l3;
}

static void __cpuinit amd_check_l3_disable(struct _cpuid4_info_regs *this_leaf,
					   int index)
{
	int node;

	if (boot_cpu_data.x86 != 0x10)
		return;

	if (index < 3)
		return;

	/* see errata #382 and #388 */
	if (boot_cpu_data.x86_model < 0x8)
		return;

	if ((boot_cpu_data.x86_model == 0x8 ||
	     boot_cpu_data.x86_model == 0x9)
		&&
	     boot_cpu_data.x86_mask < 0x1)
			return;

	/* not in virtualized environments */
	if (num_k8_northbridges == 0)
		return;

	/*
	 * Strictly speaking, the amount in @size below is leaked since it is
	 * never freed but this is done only on shutdown so it doesn't matter.
	 */
	if (!l3_caches) {
		int size = num_k8_northbridges * sizeof(struct amd_l3_cache *);

		l3_caches = kzalloc(size, GFP_ATOMIC);
		if (!l3_caches)
			return;
	}

	node = amd_get_nb_id(smp_processor_id());

	if (!l3_caches[node]) {
		l3_caches[node] = amd_init_l3_cache(node);
		l3_caches[node]->can_disable = true;
	}

	WARN_ON(!l3_caches[node]);

	this_leaf->l3 = l3_caches[node];
}

/*
 * check whether a slot used for disabling an L3 index is occupied.
 * @l3: L3 cache descriptor
 * @slot: slot number (0..1)
 *
 * @returns: the disabled index if used or negative value if slot free.
 */
int amd_get_l3_disable_slot(struct amd_l3_cache *l3, unsigned slot)
{
	unsigned int reg = 0;

	pci_read_config_dword(l3->dev, 0x1BC + slot * 4, &reg);

	/* check whether this slot is activated already */
	if (reg & (3UL << 30))
		return reg & 0xfff;

	return -1;
}

static ssize_t show_cache_disable(struct _cpuid4_info *this_leaf, char *buf,
				  unsigned int slot)
{
	int index;

	if (!this_leaf->l3 || !this_leaf->l3->can_disable)
		return -EINVAL;

	index = amd_get_l3_disable_slot(this_leaf->l3, slot);
	if (index >= 0)
		return sprintf(buf, "%d\n", index);

	return sprintf(buf, "FREE\n");
}

#define SHOW_CACHE_DISABLE(slot)					\
static ssize_t								\
show_cache_disable_##slot(struct _cpuid4_info *this_leaf, char *buf)	\
{									\
	return show_cache_disable(this_leaf, buf, slot);		\
}
SHOW_CACHE_DISABLE(0)
SHOW_CACHE_DISABLE(1)

static void amd_l3_disable_index(struct amd_l3_cache *l3, int cpu,
				 unsigned slot, unsigned long idx)
{
	int i;

	idx |= BIT(30);

	/*
	 *  disable index in all 4 subcaches
	 */
	for (i = 0; i < 4; i++) {
		u32 reg = idx | (i << 20);

		if (!l3->subcaches[i])
			continue;

		pci_write_config_dword(l3->dev, 0x1BC + slot * 4, reg);

		/*
		 * We need to WBINVD on a core on the node containing the L3
		 * cache which indices we disable therefore a simple wbinvd()
		 * is not sufficient.
		 */
		wbinvd_on_cpu(cpu);

		reg |= BIT(31);
		pci_write_config_dword(l3->dev, 0x1BC + slot * 4, reg);
	}
}

/*
 * disable a L3 cache index by using a disable-slot
 *
 * @l3:    L3 cache descriptor
 * @cpu:   A CPU on the node containing the L3 cache
 * @slot:  slot number (0..1)
 * @index: index to disable
 *
 * @return: 0 on success, error status on failure
 */
int amd_set_l3_disable_slot(struct amd_l3_cache *l3, int cpu, unsigned slot,
			    unsigned long index)
{
	int ret = 0;

#define SUBCACHE_MASK	(3UL << 20)
#define SUBCACHE_INDEX	0xfff

	/*
	 * check whether this slot is already used or
	 * the index is already disabled
	 */
	ret = amd_get_l3_disable_slot(l3, slot);
	if (ret >= 0)
		return -EINVAL;

	/*
	 * check whether the other slot has disabled the
	 * same index already
	 */
	if (index == amd_get_l3_disable_slot(l3, !slot))
		return -EINVAL;

	/* do not allow writes outside of allowed bits */
	if ((index & ~(SUBCACHE_MASK | SUBCACHE_INDEX)) ||
	    ((index & SUBCACHE_INDEX) > l3->indices))
		return -EINVAL;

	amd_l3_disable_index(l3, cpu, slot, index);

	return 0;
}

static ssize_t store_cache_disable(struct _cpuid4_info *this_leaf,
				  const char *buf, size_t count,
				  unsigned int slot)
{
	unsigned long val = 0;
	int cpu, err = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!this_leaf->l3 || !this_leaf->l3->can_disable)
		return -EINVAL;

	cpu = cpumask_first(to_cpumask(this_leaf->shared_cpu_map));

	if (strict_strtoul(buf, 10, &val) < 0)
		return -EINVAL;

	err = amd_set_l3_disable_slot(this_leaf->l3, cpu, slot, val);
	if (err) {
		if (err == -EEXIST)
			printk(KERN_WARNING "L3 disable slot %d in use!\n",
					    slot);
		return err;
	}
	return count;
}

#define STORE_CACHE_DISABLE(slot)					\
static ssize_t								\
store_cache_disable_##slot(struct _cpuid4_info *this_leaf,		\
			    const char *buf, size_t count)		\
{									\
	return store_cache_disable(this_leaf, buf, count, slot);	\
}
STORE_CACHE_DISABLE(0)
STORE_CACHE_DISABLE(1)

static struct _cache_attr cache_disable_0 = __ATTR(cache_disable_0, 0644,
		show_cache_disable_0, store_cache_disable_0);
static struct _cache_attr cache_disable_1 = __ATTR(cache_disable_1, 0644,
		show_cache_disable_1, store_cache_disable_1);

#else	/* CONFIG_CPU_SUP_AMD */
static void __cpuinit
amd_check_l3_disable(struct _cpuid4_info_regs *this_leaf, int index)
{
};
#endif /* CONFIG_CPU_SUP_AMD */

static int
__cpuinit cpuid4_cache_lookup_regs(int index,
				   struct _cpuid4_info_regs *this_leaf)
{
	union _cpuid4_leaf_eax 	eax;
	union _cpuid4_leaf_ebx 	ebx;
	union _cpuid4_leaf_ecx 	ecx;
	unsigned		edx;

	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD) {
		amd_cpuid4(index, &eax, &ebx, &ecx);
		amd_check_l3_disable(this_leaf, index);
	} else {
		cpuid_count(4, index, &eax.full, &ebx.full, &ecx.full, &edx);
	}

	if (eax.split.type == CACHE_TYPE_NULL)
		return -EIO; /* better error ? */

	this_leaf->eax = eax;
	this_leaf->ebx = ebx;
	this_leaf->ecx = ecx;
	this_leaf->size = (ecx.split.number_of_sets          + 1) *
			  (ebx.split.coherency_line_size     + 1) *
			  (ebx.split.physical_line_partition + 1) *
			  (ebx.split.ways_of_associativity   + 1);
	return 0;
}

static int __cpuinit find_num_cache_leaves(void)
{
	unsigned int		eax, ebx, ecx, edx;
	union _cpuid4_leaf_eax	cache_eax;
	int 			i = -1;

	do {
		++i;
		/* Do cpuid(4) loop to find out num_cache_leaves */
		cpuid_count(4, i, &eax, &ebx, &ecx, &edx);
		cache_eax.full = eax;
	} while (cache_eax.split.type != CACHE_TYPE_NULL);
	return i;
}

unsigned int __cpuinit init_intel_cacheinfo(struct cpuinfo_x86 *c)
{
	/* Cache sizes */
	unsigned int trace = 0, l1i = 0, l1d = 0, l2 = 0, l3 = 0;
	unsigned int new_l1d = 0, new_l1i = 0; /* Cache sizes from cpuid(4) */
	unsigned int new_l2 = 0, new_l3 = 0, i; /* Cache sizes from cpuid(4) */
	unsigned int l2_id = 0, l3_id = 0, num_threads_sharing, index_msb;
#ifdef CONFIG_X86_HT
	unsigned int cpu = c->cpu_index;
#endif

	if (c->cpuid_level > 3) {
		static int is_initialized;

		if (is_initialized == 0) {
			/* Init num_cache_leaves from boot CPU */
			num_cache_leaves = find_num_cache_leaves();
			is_initialized++;
		}

		/*
		 * Whenever possible use cpuid(4), deterministic cache
		 * parameters cpuid leaf to find the cache details
		 */
		for (i = 0; i < num_cache_leaves; i++) {
			struct _cpuid4_info_regs this_leaf;
			int retval;

			retval = cpuid4_cache_lookup_regs(i, &this_leaf);
			if (retval >= 0) {
				switch (this_leaf.eax.split.level) {
				case 1:
					if (this_leaf.eax.split.type ==
							CACHE_TYPE_DATA)
						new_l1d = this_leaf.size/1024;
					else if (this_leaf.eax.split.type ==
							CACHE_TYPE_INST)
						new_l1i = this_leaf.size/1024;
					break;
				case 2:
					new_l2 = this_leaf.size/1024;
					num_threads_sharing = 1 + this_leaf.eax.split.num_threads_sharing;
					index_msb = get_count_order(num_threads_sharing);
					l2_id = c->apicid >> index_msb;
					break;
				case 3:
					new_l3 = this_leaf.size/1024;
					num_threads_sharing = 1 + this_leaf.eax.split.num_threads_sharing;
					index_msb = get_count_order(
							num_threads_sharing);
					l3_id = c->apicid >> index_msb;
					break;
				default:
					break;
				}
			}
		}
	}
	/*
	 * Don't use cpuid2 if cpuid4 is supported. For P4, we use cpuid2 for
	 * trace cache
	 */
	if ((num_cache_leaves == 0 || c->x86 == 15) && c->cpuid_level > 1) {
		/* supports eax=2  call */
		int j, n;
		unsigned int regs[4];
		unsigned char *dp = (unsigned char *)regs;
		int only_trace = 0;

		if (num_cache_leaves != 0 && c->x86 == 15)
			only_trace = 1;

		/* Number of times to iterate */
		n = cpuid_eax(2) & 0xFF;

		for (i = 0 ; i < n ; i++) {
			cpuid(2, &regs[0], &regs[1], &regs[2], &regs[3]);

			/* If bit 31 is set, this is an unknown format */
			for (j = 0 ; j < 3 ; j++)
				if (regs[j] & (1 << 31))
					regs[j] = 0;

			/* Byte 0 is level count, not a descriptor */
			for (j = 1 ; j < 16 ; j++) {
				unsigned char des = dp[j];
				unsigned char k = 0;

				/* look up this descriptor in the table */
				while (cache_table[k].descriptor != 0) {
					if (cache_table[k].descriptor == des) {
						if (only_trace && cache_table[k].cache_type != LVL_TRACE)
							break;
						switch (cache_table[k].cache_type) {
						case LVL_1_INST:
							l1i += cache_table[k].size;
							break;
						case LVL_1_DATA:
							l1d += cache_table[k].size;
							break;
						case LVL_2:
							l2 += cache_table[k].size;
							break;
						case LVL_3:
							l3 += cache_table[k].size;
							break;
						case LVL_TRACE:
							trace += cache_table[k].size;
							break;
						}

						break;
					}

					k++;
				}
			}
		}
	}

	if (new_l1d)
		l1d = new_l1d;

	if (new_l1i)
		l1i = new_l1i;

	if (new_l2) {
		l2 = new_l2;
#ifdef CONFIG_X86_HT
		per_cpu(cpu_llc_id, cpu) = l2_id;
#endif
	}

	if (new_l3) {
		l3 = new_l3;
#ifdef CONFIG_X86_HT
		per_cpu(cpu_llc_id, cpu) = l3_id;
#endif
	}

	c->x86_cache_size = l3 ? l3 : (l2 ? l2 : (l1i+l1d));

	return l2;
}

#ifdef CONFIG_SYSFS

/* pointer to _cpuid4_info array (for each cache leaf) */
static DEFINE_PER_CPU(struct _cpuid4_info *, ici_cpuid4_info);
#define CPUID4_INFO_IDX(x, y)	(&((per_cpu(ici_cpuid4_info, x))[y]))

#ifdef CONFIG_SMP
static void __cpuinit cache_shared_cpu_map_setup(unsigned int cpu, int index)
{
	struct _cpuid4_info	*this_leaf, *sibling_leaf;
	unsigned long num_threads_sharing;
	int index_msb, i, sibling;
	struct cpuinfo_x86 *c = &cpu_data(cpu);

	if ((index == 3) && (c->x86_vendor == X86_VENDOR_AMD)) {
		for_each_cpu(i, c->llc_shared_map) {
			if (!per_cpu(ici_cpuid4_info, i))
				continue;
			this_leaf = CPUID4_INFO_IDX(i, index);
			for_each_cpu(sibling, c->llc_shared_map) {
				if (!cpu_online(sibling))
					continue;
				set_bit(sibling, this_leaf->shared_cpu_map);
			}
		}
		return;
	}
	this_leaf = CPUID4_INFO_IDX(cpu, index);
	num_threads_sharing = 1 + this_leaf->eax.split.num_threads_sharing;

	if (num_threads_sharing == 1)
		cpumask_set_cpu(cpu, to_cpumask(this_leaf->shared_cpu_map));
	else {
		index_msb = get_count_order(num_threads_sharing);

		for_each_online_cpu(i) {
			if (cpu_data(i).apicid >> index_msb ==
			    c->apicid >> index_msb) {
				cpumask_set_cpu(i,
					to_cpumask(this_leaf->shared_cpu_map));
				if (i != cpu && per_cpu(ici_cpuid4_info, i))  {
					sibling_leaf =
						CPUID4_INFO_IDX(i, index);
					cpumask_set_cpu(cpu, to_cpumask(
						sibling_leaf->shared_cpu_map));
				}
			}
		}
	}
}
static void __cpuinit cache_remove_shared_cpu_map(unsigned int cpu, int index)
{
	struct _cpuid4_info	*this_leaf, *sibling_leaf;
	int sibling;

	this_leaf = CPUID4_INFO_IDX(cpu, index);
	for_each_cpu(sibling, to_cpumask(this_leaf->shared_cpu_map)) {
		sibling_leaf = CPUID4_INFO_IDX(sibling, index);
		cpumask_clear_cpu(cpu,
				  to_cpumask(sibling_leaf->shared_cpu_map));
	}
}
#else
static void __cpuinit cache_shared_cpu_map_setup(unsigned int cpu, int index)
{
}

static void __cpuinit cache_remove_shared_cpu_map(unsigned int cpu, int index)
{
}
#endif

static void __cpuinit free_cache_attributes(unsigned int cpu)
{
	int i;

	for (i = 0; i < num_cache_leaves; i++)
		cache_remove_shared_cpu_map(cpu, i);

	kfree(per_cpu(ici_cpuid4_info, cpu)->l3);
	kfree(per_cpu(ici_cpuid4_info, cpu));
	per_cpu(ici_cpuid4_info, cpu) = NULL;
}

static int
__cpuinit cpuid4_cache_lookup(int index, struct _cpuid4_info *this_leaf)
{
	struct _cpuid4_info_regs *leaf_regs =
		(struct _cpuid4_info_regs *)this_leaf;

	return cpuid4_cache_lookup_regs(index, leaf_regs);
}

static void __cpuinit get_cpu_leaves(void *_retval)
{
	int j, *retval = _retval, cpu = smp_processor_id();

	/* Do cpuid and store the results */
	for (j = 0; j < num_cache_leaves; j++) {
		struct _cpuid4_info *this_leaf;
		this_leaf = CPUID4_INFO_IDX(cpu, j);
		*retval = cpuid4_cache_lookup(j, this_leaf);
		if (unlikely(*retval < 0)) {
			int i;

			for (i = 0; i < j; i++)
				cache_remove_shared_cpu_map(cpu, i);
			break;
		}
		cache_shared_cpu_map_setup(cpu, j);
	}
}

static int __cpuinit detect_cache_attributes(unsigned int cpu)
{
	int			retval;

	if (num_cache_leaves == 0)
		return -ENOENT;

	per_cpu(ici_cpuid4_info, cpu) = kzalloc(
	    sizeof(struct _cpuid4_info) * num_cache_leaves, GFP_KERNEL);
	if (per_cpu(ici_cpuid4_info, cpu) == NULL)
		return -ENOMEM;

	smp_call_function_single(cpu, get_cpu_leaves, &retval, true);
	if (retval) {
		kfree(per_cpu(ici_cpuid4_info, cpu));
		per_cpu(ici_cpuid4_info, cpu) = NULL;
	}

	return retval;
}

#include <linux/kobject.h>
#include <linux/sysfs.h>

extern struct sysdev_class cpu_sysdev_class; /* from drivers/base/cpu.c */

/* pointer to kobject for cpuX/cache */
static DEFINE_PER_CPU(struct kobject *, ici_cache_kobject);

struct _index_kobject {
	struct kobject kobj;
	unsigned int cpu;
	unsigned short index;
};

/* pointer to array of kobjects for cpuX/cache/indexY */
static DEFINE_PER_CPU(struct _index_kobject *, ici_index_kobject);
#define INDEX_KOBJECT_PTR(x, y)		(&((per_cpu(ici_index_kobject, x))[y]))

#define show_one_plus(file_name, object, val)				\
static ssize_t show_##file_name						\
			(struct _cpuid4_info *this_leaf, char *buf)	\
{									\
	return sprintf(buf, "%lu\n", (unsigned long)this_leaf->object + val); \
}

show_one_plus(level, eax.split.level, 0);
show_one_plus(coherency_line_size, ebx.split.coherency_line_size, 1);
show_one_plus(physical_line_partition, ebx.split.physical_line_partition, 1);
show_one_plus(ways_of_associativity, ebx.split.ways_of_associativity, 1);
show_one_plus(number_of_sets, ecx.split.number_of_sets, 1);

static ssize_t show_size(struct _cpuid4_info *this_leaf, char *buf)
{
	return sprintf(buf, "%luK\n", this_leaf->size / 1024);
}

static ssize_t show_shared_cpu_map_func(struct _cpuid4_info *this_leaf,
					int type, char *buf)
{
	ptrdiff_t len = PTR_ALIGN(buf + PAGE_SIZE - 1, PAGE_SIZE) - buf;
	int n = 0;

	if (len > 1) {
		const struct cpumask *mask;

		mask = to_cpumask(this_leaf->shared_cpu_map);
		n = type ?
			cpulist_scnprintf(buf, len-2, mask) :
			cpumask_scnprintf(buf, len-2, mask);
		buf[n++] = '\n';
		buf[n] = '\0';
	}
	return n;
}

static inline ssize_t show_shared_cpu_map(struct _cpuid4_info *leaf, char *buf)
{
	return show_shared_cpu_map_func(leaf, 0, buf);
}

static inline ssize_t show_shared_cpu_list(struct _cpuid4_info *leaf, char *buf)
{
	return show_shared_cpu_map_func(leaf, 1, buf);
}

static ssize_t show_type(struct _cpuid4_info *this_leaf, char *buf)
{
	switch (this_leaf->eax.split.type) {
	case CACHE_TYPE_DATA:
		return sprintf(buf, "Data\n");
	case CACHE_TYPE_INST:
		return sprintf(buf, "Instruction\n");
	case CACHE_TYPE_UNIFIED:
		return sprintf(buf, "Unified\n");
	default:
		return sprintf(buf, "Unknown\n");
	}
}

#define to_object(k)	container_of(k, struct _index_kobject, kobj)
#define to_attr(a)	container_of(a, struct _cache_attr, attr)

#define define_one_ro(_name) \
static struct _cache_attr _name = \
	__ATTR(_name, 0444, show_##_name, NULL)

define_one_ro(level);
define_one_ro(type);
define_one_ro(coherency_line_size);
define_one_ro(physical_line_partition);
define_one_ro(ways_of_associativity);
define_one_ro(number_of_sets);
define_one_ro(size);
define_one_ro(shared_cpu_map);
define_one_ro(shared_cpu_list);

#define DEFAULT_SYSFS_CACHE_ATTRS	\
	&type.attr,			\
	&level.attr,			\
	&coherency_line_size.attr,	\
	&physical_line_partition.attr,	\
	&ways_of_associativity.attr,	\
	&number_of_sets.attr,		\
	&size.attr,			\
	&shared_cpu_map.attr,		\
	&shared_cpu_list.attr

static struct attribute *default_attrs[] = {
	DEFAULT_SYSFS_CACHE_ATTRS,
	NULL
};

static struct attribute *default_l3_attrs[] = {
	DEFAULT_SYSFS_CACHE_ATTRS,
#ifdef CONFIG_CPU_SUP_AMD
	&cache_disable_0.attr,
	&cache_disable_1.attr,
#endif
	NULL
};

static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct _cache_attr *fattr = to_attr(attr);
	struct _index_kobject *this_leaf = to_object(kobj);
	ssize_t ret;

	ret = fattr->show ?
		fattr->show(CPUID4_INFO_IDX(this_leaf->cpu, this_leaf->index),
			buf) :
		0;
	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		     const char *buf, size_t count)
{
	struct _cache_attr *fattr = to_attr(attr);
	struct _index_kobject *this_leaf = to_object(kobj);
	ssize_t ret;

	ret = fattr->store ?
		fattr->store(CPUID4_INFO_IDX(this_leaf->cpu, this_leaf->index),
			buf, count) :
		0;
	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = store,
};

static struct kobj_type ktype_cache = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};

static struct kobj_type ktype_percpu_entry = {
	.sysfs_ops	= &sysfs_ops,
};

static void __cpuinit cpuid4_cache_sysfs_exit(unsigned int cpu)
{
	kfree(per_cpu(ici_cache_kobject, cpu));
	kfree(per_cpu(ici_index_kobject, cpu));
	per_cpu(ici_cache_kobject, cpu) = NULL;
	per_cpu(ici_index_kobject, cpu) = NULL;
	free_cache_attributes(cpu);
}

static int __cpuinit cpuid4_cache_sysfs_init(unsigned int cpu)
{
	int err;

	if (num_cache_leaves == 0)
		return -ENOENT;

	err = detect_cache_attributes(cpu);
	if (err)
		return err;

	/* Allocate all required memory */
	per_cpu(ici_cache_kobject, cpu) =
		kzalloc(sizeof(struct kobject), GFP_KERNEL);
	if (unlikely(per_cpu(ici_cache_kobject, cpu) == NULL))
		goto err_out;

	per_cpu(ici_index_kobject, cpu) = kzalloc(
	    sizeof(struct _index_kobject) * num_cache_leaves, GFP_KERNEL);
	if (unlikely(per_cpu(ici_index_kobject, cpu) == NULL))
		goto err_out;

	return 0;

err_out:
	cpuid4_cache_sysfs_exit(cpu);
	return -ENOMEM;
}

static DECLARE_BITMAP(cache_dev_map, NR_CPUS);

/* Add/Remove cache interface for CPU device */
static int __cpuinit cache_add_dev(struct sys_device * sys_dev)
{
	unsigned int cpu = sys_dev->id;
	unsigned long i, j;
	struct _index_kobject *this_object;
	struct _cpuid4_info   *this_leaf;
	int retval;

	retval = cpuid4_cache_sysfs_init(cpu);
	if (unlikely(retval < 0))
		return retval;

	retval = kobject_init_and_add(per_cpu(ici_cache_kobject, cpu),
				      &ktype_percpu_entry,
				      &sys_dev->kobj, "%s", "cache");
	if (retval < 0) {
		cpuid4_cache_sysfs_exit(cpu);
		return retval;
	}

	for (i = 0; i < num_cache_leaves; i++) {
		this_object = INDEX_KOBJECT_PTR(cpu, i);
		this_object->cpu = cpu;
		this_object->index = i;

		this_leaf = CPUID4_INFO_IDX(cpu, i);

		if (this_leaf->l3 && this_leaf->l3->can_disable)
			ktype_cache.default_attrs = default_l3_attrs;
		else
			ktype_cache.default_attrs = default_attrs;

		retval = kobject_init_and_add(&(this_object->kobj),
					      &ktype_cache,
					      per_cpu(ici_cache_kobject, cpu),
					      "index%1lu", i);
		if (unlikely(retval)) {
			for (j = 0; j < i; j++)
				kobject_put(&(INDEX_KOBJECT_PTR(cpu, j)->kobj));
			kobject_put(per_cpu(ici_cache_kobject, cpu));
			cpuid4_cache_sysfs_exit(cpu);
			return retval;
		}
		kobject_uevent(&(this_object->kobj), KOBJ_ADD);
	}
	cpumask_set_cpu(cpu, to_cpumask(cache_dev_map));

	kobject_uevent(per_cpu(ici_cache_kobject, cpu), KOBJ_ADD);
	return 0;
}

static void __cpuinit cache_remove_dev(struct sys_device * sys_dev)
{
	unsigned int cpu = sys_dev->id;
	unsigned long i;

	if (per_cpu(ici_cpuid4_info, cpu) == NULL)
		return;
	if (!cpumask_test_cpu(cpu, to_cpumask(cache_dev_map)))
		return;
	cpumask_clear_cpu(cpu, to_cpumask(cache_dev_map));

	for (i = 0; i < num_cache_leaves; i++)
		kobject_put(&(INDEX_KOBJECT_PTR(cpu, i)->kobj));
	kobject_put(per_cpu(ici_cache_kobject, cpu));
	cpuid4_cache_sysfs_exit(cpu);
}

static int __cpuinit cacheinfo_cpu_callback(struct notifier_block *nfb,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct sys_device *sys_dev;

	sys_dev = get_cpu_sysdev(cpu);
	switch (action) {
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		cache_add_dev(sys_dev);
		break;
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		cache_remove_dev(sys_dev);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cacheinfo_cpu_notifier = {
	.notifier_call = cacheinfo_cpu_callback,
};

static int __cpuinit cache_sysfs_init(void)
{
	int i;

	if (num_cache_leaves == 0)
		return 0;

	for_each_online_cpu(i) {
		int err;
		struct sys_device *sys_dev = get_cpu_sysdev(i);

		err = cache_add_dev(sys_dev);
		if (err)
			return err;
	}
	register_hotcpu_notifier(&cacheinfo_cpu_notifier);
	return 0;
}

device_initcall(cache_sysfs_init);

#endif
