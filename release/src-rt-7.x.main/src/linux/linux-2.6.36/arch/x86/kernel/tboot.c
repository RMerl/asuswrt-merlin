/*
 * tboot.c: main implementation of helper functions used by kernel for
 *          runtime support of Intel(R) Trusted Execution Technology
 *
 * Copyright (c) 2006-2009, Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/dma_remapping.h>
#include <linux/init_task.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/dmar.h>
#include <linux/cpu.h>
#include <linux/pfn.h>
#include <linux/mm.h>
#include <linux/tboot.h>

#include <asm/trampoline.h>
#include <asm/processor.h>
#include <asm/bootparam.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/proto.h>
#include <asm/setup.h>
#include <asm/e820.h>
#include <asm/io.h>

#include "acpi/realmode/wakeup.h"

/* Global pointer to shared data; NULL means no measured launch. */
struct tboot *tboot __read_mostly;
EXPORT_SYMBOL(tboot);

/* timeout for APs (in secs) to enter wait-for-SIPI state during shutdown */
#define AP_WAIT_TIMEOUT		1

#undef pr_fmt
#define pr_fmt(fmt)	"tboot: " fmt

static u8 tboot_uuid[16] __initdata = TBOOT_UUID;

void __init tboot_probe(void)
{
	/* Look for valid page-aligned address for shared page. */
	if (!boot_params.tboot_addr)
		return;
	/*
	 * also verify that it is mapped as we expect it before calling
	 * set_fixmap(), to reduce chance of garbage value causing crash
	 */
	if (!e820_any_mapped(boot_params.tboot_addr,
			     boot_params.tboot_addr, E820_RESERVED)) {
		pr_warning("non-0 tboot_addr but it is not of type E820_RESERVED\n");
		return;
	}

	/* only a natively booted kernel should be using TXT */
	if (paravirt_enabled()) {
		pr_warning("non-0 tboot_addr but pv_ops is enabled\n");
		return;
	}

	/* Map and check for tboot UUID. */
	set_fixmap(FIX_TBOOT_BASE, boot_params.tboot_addr);
	tboot = (struct tboot *)fix_to_virt(FIX_TBOOT_BASE);
	if (memcmp(&tboot_uuid, &tboot->uuid, sizeof(tboot->uuid))) {
		pr_warning("tboot at 0x%llx is invalid\n",
			   boot_params.tboot_addr);
		tboot = NULL;
		return;
	}
	if (tboot->version < 5) {
		pr_warning("tboot version is invalid: %u\n", tboot->version);
		tboot = NULL;
		return;
	}

	pr_info("found shared page at phys addr 0x%llx:\n",
		boot_params.tboot_addr);
	pr_debug("version: %d\n", tboot->version);
	pr_debug("log_addr: 0x%08x\n", tboot->log_addr);
	pr_debug("shutdown_entry: 0x%x\n", tboot->shutdown_entry);
	pr_debug("tboot_base: 0x%08x\n", tboot->tboot_base);
	pr_debug("tboot_size: 0x%x\n", tboot->tboot_size);
}

static pgd_t *tboot_pg_dir;
static struct mm_struct tboot_mm = {
	.mm_rb          = RB_ROOT,
	.pgd            = swapper_pg_dir,
	.mm_users       = ATOMIC_INIT(2),
	.mm_count       = ATOMIC_INIT(1),
	.mmap_sem       = __RWSEM_INITIALIZER(init_mm.mmap_sem),
	.page_table_lock =  __SPIN_LOCK_UNLOCKED(init_mm.page_table_lock),
	.mmlist         = LIST_HEAD_INIT(init_mm.mmlist),
	.cpu_vm_mask    = CPU_MASK_ALL,
};

static inline void switch_to_tboot_pt(void)
{
	write_cr3(virt_to_phys(tboot_pg_dir));
}

static int map_tboot_page(unsigned long vaddr, unsigned long pfn,
			  pgprot_t prot)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(&tboot_mm, vaddr);
	pud = pud_alloc(&tboot_mm, pgd, vaddr);
	if (!pud)
		return -1;
	pmd = pmd_alloc(&tboot_mm, pud, vaddr);
	if (!pmd)
		return -1;
	pte = pte_alloc_map(&tboot_mm, pmd, vaddr);
	if (!pte)
		return -1;
	set_pte_at(&tboot_mm, vaddr, pte, pfn_pte(pfn, prot));
	pte_unmap(pte);
	return 0;
}

static int map_tboot_pages(unsigned long vaddr, unsigned long start_pfn,
			   unsigned long nr)
{
	/* Reuse the original kernel mapping */
	tboot_pg_dir = pgd_alloc(&tboot_mm);
	if (!tboot_pg_dir)
		return -1;

	for (; nr > 0; nr--, vaddr += PAGE_SIZE, start_pfn++) {
		if (map_tboot_page(vaddr, start_pfn, PAGE_KERNEL_EXEC))
			return -1;
	}

	return 0;
}

static void tboot_create_trampoline(void)
{
	u32 map_base, map_size;

	/* Create identity map for tboot shutdown code. */
	map_base = PFN_DOWN(tboot->tboot_base);
	map_size = PFN_UP(tboot->tboot_size);
	if (map_tboot_pages(map_base << PAGE_SHIFT, map_base, map_size))
		panic("tboot: Error mapping tboot pages (mfns) @ 0x%x, 0x%x\n",
		      map_base, map_size);
}

#ifdef CONFIG_ACPI_SLEEP

static void add_mac_region(phys_addr_t start, unsigned long size)
{
	struct tboot_mac_region *mr;
	phys_addr_t end = start + size;

	if (tboot->num_mac_regions >= MAX_TB_MAC_REGIONS)
		panic("tboot: Too many MAC regions\n");

	if (start && size) {
		mr = &tboot->mac_regions[tboot->num_mac_regions++];
		mr->start = round_down(start, PAGE_SIZE);
		mr->size  = round_up(end, PAGE_SIZE) - mr->start;
	}
}

static int tboot_setup_sleep(void)
{
	int i;

	tboot->num_mac_regions = 0;

	for (i = 0; i < e820.nr_map; i++) {
		if ((e820.map[i].type != E820_RAM)
		 && (e820.map[i].type != E820_RESERVED_KERN))
			continue;

		add_mac_region(e820.map[i].addr, e820.map[i].size);
	}

	tboot->acpi_sinfo.kernel_s3_resume_vector = acpi_wakeup_address;

	return 0;
}

#else /* no CONFIG_ACPI_SLEEP */

static int tboot_setup_sleep(void)
{
	/* S3 shutdown requested, but S3 not supported by the kernel... */
	BUG();
	return -1;
}

#endif

void tboot_shutdown(u32 shutdown_type)
{
	void (*shutdown)(void);

	if (!tboot_enabled())
		return;

	/*
	 * if we're being called before the 1:1 mapping is set up then just
	 * return and let the normal shutdown happen; this should only be
	 * due to very early panic()
	 */
	if (!tboot_pg_dir)
		return;

	/* if this is S3 then set regions to MAC */
	if (shutdown_type == TB_SHUTDOWN_S3)
		if (tboot_setup_sleep())
			return;

	tboot->shutdown_type = shutdown_type;

	switch_to_tboot_pt();

	shutdown = (void(*)(void))(unsigned long)tboot->shutdown_entry;
	shutdown();

	/* should not reach here */
	while (1)
		halt();
}

static void tboot_copy_fadt(const struct acpi_table_fadt *fadt)
{
#define TB_COPY_GAS(tbg, g)			\
	tbg.space_id     = g.space_id;		\
	tbg.bit_width    = g.bit_width;		\
	tbg.bit_offset   = g.bit_offset;	\
	tbg.access_width = g.access_width;	\
	tbg.address      = g.address;

	TB_COPY_GAS(tboot->acpi_sinfo.pm1a_cnt_blk, fadt->xpm1a_control_block);
	TB_COPY_GAS(tboot->acpi_sinfo.pm1b_cnt_blk, fadt->xpm1b_control_block);
	TB_COPY_GAS(tboot->acpi_sinfo.pm1a_evt_blk, fadt->xpm1a_event_block);
	TB_COPY_GAS(tboot->acpi_sinfo.pm1b_evt_blk, fadt->xpm1b_event_block);

	/*
	 * We need phys addr of waking vector, but can't use virt_to_phys() on
	 * &acpi_gbl_FACS because it is ioremap'ed, so calc from FACS phys
	 * addr.
	 */
	tboot->acpi_sinfo.wakeup_vector = fadt->facs +
		offsetof(struct acpi_table_facs, firmware_waking_vector);
}

void tboot_sleep(u8 sleep_state, u32 pm1a_control, u32 pm1b_control)
{
	static u32 acpi_shutdown_map[ACPI_S_STATE_COUNT] = {
		/* S0,1,2: */ -1, -1, -1,
		/* S3: */ TB_SHUTDOWN_S3,
		/* S4: */ TB_SHUTDOWN_S4,
		/* S5: */ TB_SHUTDOWN_S5 };

	if (!tboot_enabled())
		return;

	tboot_copy_fadt(&acpi_gbl_FADT);
	tboot->acpi_sinfo.pm1a_cnt_val = pm1a_control;
	tboot->acpi_sinfo.pm1b_cnt_val = pm1b_control;
	/* we always use the 32b wakeup vector */
	tboot->acpi_sinfo.vector_width = 32;

	if (sleep_state >= ACPI_S_STATE_COUNT ||
	    acpi_shutdown_map[sleep_state] == -1) {
		pr_warning("unsupported sleep state 0x%x\n", sleep_state);
		return;
	}

	tboot_shutdown(acpi_shutdown_map[sleep_state]);
}

static atomic_t ap_wfs_count;

static int tboot_wait_for_aps(int num_aps)
{
	unsigned long timeout;

	timeout = AP_WAIT_TIMEOUT*HZ;
	while (atomic_read((atomic_t *)&tboot->num_in_wfs) != num_aps &&
	       timeout) {
		mdelay(1);
		timeout--;
	}

	if (timeout)
		pr_warning("tboot wait for APs timeout\n");

	return !(atomic_read((atomic_t *)&tboot->num_in_wfs) == num_aps);
}

static int __cpuinit tboot_cpu_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	switch (action) {
	case CPU_DYING:
		atomic_inc(&ap_wfs_count);
		if (num_online_cpus() == 1)
			if (tboot_wait_for_aps(atomic_read(&ap_wfs_count)))
				return NOTIFY_BAD;
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block tboot_cpu_notifier __cpuinitdata =
{
	.notifier_call = tboot_cpu_callback,
};

static __init int tboot_late_init(void)
{
	if (!tboot_enabled())
		return 0;

	tboot_create_trampoline();

	atomic_set(&ap_wfs_count, 0);
	register_hotcpu_notifier(&tboot_cpu_notifier);
	return 0;
}

late_initcall(tboot_late_init);

/*
 * TXT configuration registers (offsets from TXT_{PUB, PRIV}_CONFIG_REGS_BASE)
 */

#define TXT_PUB_CONFIG_REGS_BASE       0xfed30000
#define TXT_PRIV_CONFIG_REGS_BASE      0xfed20000

/* # pages for each config regs space - used by fixmap */
#define NR_TXT_CONFIG_PAGES     ((TXT_PUB_CONFIG_REGS_BASE -                \
				  TXT_PRIV_CONFIG_REGS_BASE) >> PAGE_SHIFT)

/* offsets from pub/priv config space */
#define TXTCR_HEAP_BASE             0x0300
#define TXTCR_HEAP_SIZE             0x0308

#define SHA1_SIZE      20

struct sha1_hash {
	u8 hash[SHA1_SIZE];
};

struct sinit_mle_data {
	u32               version;             /* currently 6 */
	struct sha1_hash  bios_acm_id;
	u32               edx_senter_flags;
	u64               mseg_valid;
	struct sha1_hash  sinit_hash;
	struct sha1_hash  mle_hash;
	struct sha1_hash  stm_hash;
	struct sha1_hash  lcp_policy_hash;
	u32               lcp_policy_control;
	u32               rlp_wakeup_addr;
	u32               reserved;
	u32               num_mdrs;
	u32               mdrs_off;
	u32               num_vtd_dmars;
	u32               vtd_dmars_off;
} __packed;

struct acpi_table_header *tboot_get_dmar_table(struct acpi_table_header *dmar_tbl)
{
	void *heap_base, *heap_ptr, *config;

	if (!tboot_enabled())
		return dmar_tbl;

	/*
	 * ACPI tables may not be DMA protected by tboot, so use DMAR copy
	 * SINIT saved in SinitMleData in TXT heap (which is DMA protected)
	 */

	/* map config space in order to get heap addr */
	config = ioremap(TXT_PUB_CONFIG_REGS_BASE, NR_TXT_CONFIG_PAGES *
			 PAGE_SIZE);
	if (!config)
		return NULL;

	/* now map TXT heap */
	heap_base = ioremap(*(u64 *)(config + TXTCR_HEAP_BASE),
			    *(u64 *)(config + TXTCR_HEAP_SIZE));
	iounmap(config);
	if (!heap_base)
		return NULL;

	/* walk heap to SinitMleData */
	/* skip BiosData */
	heap_ptr = heap_base + *(u64 *)heap_base;
	/* skip OsMleData */
	heap_ptr += *(u64 *)heap_ptr;
	/* skip OsSinitData */
	heap_ptr += *(u64 *)heap_ptr;
	/* now points to SinitMleDataSize; set to SinitMleData */
	heap_ptr += sizeof(u64);
	/* get addr of DMAR table */
	dmar_tbl = (struct acpi_table_header *)(heap_ptr +
		   ((struct sinit_mle_data *)heap_ptr)->vtd_dmars_off -
		   sizeof(u64));

	/* don't unmap heap because dmar.c needs access to this */

	return dmar_tbl;
}

int tboot_force_iommu(void)
{
	if (!tboot_enabled())
		return 0;

	if (no_iommu || swiotlb || dmar_disabled)
		pr_warning("Forcing Intel-IOMMU to enabled\n");

	dmar_disabled = 0;
#ifdef CONFIG_SWIOTLB
	swiotlb = 0;
#endif
	no_iommu = 0;

	return 1;
}
