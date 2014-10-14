/*
 * Copyright (C) 2007-2010 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <joerg.roedel@amd.com>
 *         Leo Duran <leo.duran@amd.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#ifndef _ASM_X86_AMD_IOMMU_TYPES_H
#define _ASM_X86_AMD_IOMMU_TYPES_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/spinlock.h>

/*
 * Maximum number of IOMMUs supported
 */
#define MAX_IOMMUS	32

/*
 * some size calculation constants
 */
#define DEV_TABLE_ENTRY_SIZE		32
#define ALIAS_TABLE_ENTRY_SIZE		2
#define RLOOKUP_TABLE_ENTRY_SIZE	(sizeof(void *))

/* Length of the MMIO region for the AMD IOMMU */
#define MMIO_REGION_LENGTH       0x4000

/* Capability offsets used by the driver */
#define MMIO_CAP_HDR_OFFSET	0x00
#define MMIO_RANGE_OFFSET	0x0c
#define MMIO_MISC_OFFSET	0x10

/* Masks, shifts and macros to parse the device range capability */
#define MMIO_RANGE_LD_MASK	0xff000000
#define MMIO_RANGE_FD_MASK	0x00ff0000
#define MMIO_RANGE_BUS_MASK	0x0000ff00
#define MMIO_RANGE_LD_SHIFT	24
#define MMIO_RANGE_FD_SHIFT	16
#define MMIO_RANGE_BUS_SHIFT	8
#define MMIO_GET_LD(x)  (((x) & MMIO_RANGE_LD_MASK) >> MMIO_RANGE_LD_SHIFT)
#define MMIO_GET_FD(x)  (((x) & MMIO_RANGE_FD_MASK) >> MMIO_RANGE_FD_SHIFT)
#define MMIO_GET_BUS(x) (((x) & MMIO_RANGE_BUS_MASK) >> MMIO_RANGE_BUS_SHIFT)
#define MMIO_MSI_NUM(x)	((x) & 0x1f)

/* Flag masks for the AMD IOMMU exclusion range */
#define MMIO_EXCL_ENABLE_MASK 0x01ULL
#define MMIO_EXCL_ALLOW_MASK  0x02ULL

/* Used offsets into the MMIO space */
#define MMIO_DEV_TABLE_OFFSET   0x0000
#define MMIO_CMD_BUF_OFFSET     0x0008
#define MMIO_EVT_BUF_OFFSET     0x0010
#define MMIO_CONTROL_OFFSET     0x0018
#define MMIO_EXCL_BASE_OFFSET   0x0020
#define MMIO_EXCL_LIMIT_OFFSET  0x0028
#define MMIO_CMD_HEAD_OFFSET	0x2000
#define MMIO_CMD_TAIL_OFFSET	0x2008
#define MMIO_EVT_HEAD_OFFSET	0x2010
#define MMIO_EVT_TAIL_OFFSET	0x2018
#define MMIO_STATUS_OFFSET	0x2020

/* MMIO status bits */
#define MMIO_STATUS_COM_WAIT_INT_MASK	0x04

/* event logging constants */
#define EVENT_ENTRY_SIZE	0x10
#define EVENT_TYPE_SHIFT	28
#define EVENT_TYPE_MASK		0xf
#define EVENT_TYPE_ILL_DEV	0x1
#define EVENT_TYPE_IO_FAULT	0x2
#define EVENT_TYPE_DEV_TAB_ERR	0x3
#define EVENT_TYPE_PAGE_TAB_ERR	0x4
#define EVENT_TYPE_ILL_CMD	0x5
#define EVENT_TYPE_CMD_HARD_ERR	0x6
#define EVENT_TYPE_IOTLB_INV_TO	0x7
#define EVENT_TYPE_INV_DEV_REQ	0x8
#define EVENT_DEVID_MASK	0xffff
#define EVENT_DEVID_SHIFT	0
#define EVENT_DOMID_MASK	0xffff
#define EVENT_DOMID_SHIFT	0
#define EVENT_FLAGS_MASK	0xfff
#define EVENT_FLAGS_SHIFT	0x10

/* feature control bits */
#define CONTROL_IOMMU_EN        0x00ULL
#define CONTROL_HT_TUN_EN       0x01ULL
#define CONTROL_EVT_LOG_EN      0x02ULL
#define CONTROL_EVT_INT_EN      0x03ULL
#define CONTROL_COMWAIT_EN      0x04ULL
#define CONTROL_PASSPW_EN       0x08ULL
#define CONTROL_RESPASSPW_EN    0x09ULL
#define CONTROL_COHERENT_EN     0x0aULL
#define CONTROL_ISOC_EN         0x0bULL
#define CONTROL_CMDBUF_EN       0x0cULL
#define CONTROL_PPFLOG_EN       0x0dULL
#define CONTROL_PPFINT_EN       0x0eULL

/* command specific defines */
#define CMD_COMPL_WAIT          0x01
#define CMD_INV_DEV_ENTRY       0x02
#define CMD_INV_IOMMU_PAGES     0x03

#define CMD_COMPL_WAIT_STORE_MASK	0x01
#define CMD_COMPL_WAIT_INT_MASK		0x02
#define CMD_INV_IOMMU_PAGES_SIZE_MASK	0x01
#define CMD_INV_IOMMU_PAGES_PDE_MASK	0x02

#define CMD_INV_IOMMU_ALL_PAGES_ADDRESS	0x7fffffffffffffffULL

/* macros and definitions for device table entries */
#define DEV_ENTRY_VALID         0x00
#define DEV_ENTRY_TRANSLATION   0x01
#define DEV_ENTRY_IR            0x3d
#define DEV_ENTRY_IW            0x3e
#define DEV_ENTRY_NO_PAGE_FAULT	0x62
#define DEV_ENTRY_EX            0x67
#define DEV_ENTRY_SYSMGT1       0x68
#define DEV_ENTRY_SYSMGT2       0x69
#define DEV_ENTRY_INIT_PASS     0xb8
#define DEV_ENTRY_EINT_PASS     0xb9
#define DEV_ENTRY_NMI_PASS      0xba
#define DEV_ENTRY_LINT0_PASS    0xbe
#define DEV_ENTRY_LINT1_PASS    0xbf
#define DEV_ENTRY_MODE_MASK	0x07
#define DEV_ENTRY_MODE_SHIFT	0x09

/* constants to configure the command buffer */
#define CMD_BUFFER_SIZE    8192
#define CMD_BUFFER_UNINITIALIZED 1
#define CMD_BUFFER_ENTRIES 512
#define MMIO_CMD_SIZE_SHIFT 56
#define MMIO_CMD_SIZE_512 (0x9ULL << MMIO_CMD_SIZE_SHIFT)

/* constants for event buffer handling */
#define EVT_BUFFER_SIZE		8192 /* 512 entries */
#define EVT_LEN_MASK		(0x9ULL << 56)

#define PAGE_MODE_NONE    0x00
#define PAGE_MODE_1_LEVEL 0x01
#define PAGE_MODE_2_LEVEL 0x02
#define PAGE_MODE_3_LEVEL 0x03
#define PAGE_MODE_4_LEVEL 0x04
#define PAGE_MODE_5_LEVEL 0x05
#define PAGE_MODE_6_LEVEL 0x06

#define PM_LEVEL_SHIFT(x)	(12 + ((x) * 9))
#define PM_LEVEL_SIZE(x)	(((x) < 6) ? \
				  ((1ULL << PM_LEVEL_SHIFT((x))) - 1): \
				   (0xffffffffffffffffULL))
#define PM_LEVEL_INDEX(x, a)	(((a) >> PM_LEVEL_SHIFT((x))) & 0x1ffULL)
#define PM_LEVEL_ENC(x)		(((x) << 9) & 0xe00ULL)
#define PM_LEVEL_PDE(x, a)	((a) | PM_LEVEL_ENC((x)) | \
				 IOMMU_PTE_P | IOMMU_PTE_IR | IOMMU_PTE_IW)
#define PM_PTE_LEVEL(pte)	(((pte) >> 9) & 0x7ULL)

#define PM_MAP_4k		0
#define PM_ADDR_MASK		0x000ffffffffff000ULL
#define PM_MAP_MASK(lvl)	(PM_ADDR_MASK & \
				(~((1ULL << (12 + ((lvl) * 9))) - 1)))
#define PM_ALIGNED(lvl, addr)	((PM_MAP_MASK(lvl) & (addr)) == (addr))

/*
 * Returns the page table level to use for a given page size
 * Pagesize is expected to be a power-of-two
 */
#define PAGE_SIZE_LEVEL(pagesize) \
		((__ffs(pagesize) - 12) / 9)
/*
 * Returns the number of ptes to use for a given page size
 * Pagesize is expected to be a power-of-two
 */
#define PAGE_SIZE_PTE_COUNT(pagesize) \
		(1ULL << ((__ffs(pagesize) - 12) % 9))

/*
 * Aligns a given io-virtual address to a given page size
 * Pagesize is expected to be a power-of-two
 */
#define PAGE_SIZE_ALIGN(address, pagesize) \
		((address) & ~((pagesize) - 1))
/*
 * Creates an IOMMU PTE for an address an a given pagesize
 * The PTE has no permission bits set
 * Pagesize is expected to be a power-of-two larger than 4096
 */
#define PAGE_SIZE_PTE(address, pagesize)		\
		(((address) | ((pagesize) - 1)) &	\
		 (~(pagesize >> 1)) & PM_ADDR_MASK)

/*
 * Takes a PTE value with mode=0x07 and returns the page size it maps
 */
#define PTE_PAGE_SIZE(pte) \
	(1ULL << (1 + ffz(((pte) | 0xfffULL))))

#define IOMMU_PTE_P  (1ULL << 0)
#define IOMMU_PTE_TV (1ULL << 1)
#define IOMMU_PTE_U  (1ULL << 59)
#define IOMMU_PTE_FC (1ULL << 60)
#define IOMMU_PTE_IR (1ULL << 61)
#define IOMMU_PTE_IW (1ULL << 62)

#define IOMMU_PAGE_MASK (((1ULL << 52) - 1) & ~0xfffULL)
#define IOMMU_PTE_PRESENT(pte) ((pte) & IOMMU_PTE_P)
#define IOMMU_PTE_PAGE(pte) (phys_to_virt((pte) & IOMMU_PAGE_MASK))
#define IOMMU_PTE_MODE(pte) (((pte) >> 9) & 0x07)

#define IOMMU_PROT_MASK 0x03
#define IOMMU_PROT_IR 0x01
#define IOMMU_PROT_IW 0x02

/* IOMMU capabilities */
#define IOMMU_CAP_IOTLB   24
#define IOMMU_CAP_NPCACHE 26

#define MAX_DOMAIN_ID 65536

/* FIXME: move this macro to <linux/pci.h> */
#define PCI_BUS(x) (((x) >> 8) & 0xff)

/* Protection domain flags */
#define PD_DMA_OPS_MASK		(1UL << 0) /* domain used for dma_ops */
#define PD_DEFAULT_MASK		(1UL << 1) /* domain is a default dma_ops
					      domain for an IOMMU */
#define PD_PASSTHROUGH_MASK	(1UL << 2) /* domain has no page
					      translation */

extern bool amd_iommu_dump;
#define DUMP_printk(format, arg...)					\
	do {								\
		if (amd_iommu_dump)						\
			printk(KERN_INFO "AMD-Vi: " format, ## arg);	\
	} while(0);

/* global flag if IOMMUs cache non-present entries */
extern bool amd_iommu_np_cache;

/*
 * Make iterating over all IOMMUs easier
 */
#define for_each_iommu(iommu) \
	list_for_each_entry((iommu), &amd_iommu_list, list)
#define for_each_iommu_safe(iommu, next) \
	list_for_each_entry_safe((iommu), (next), &amd_iommu_list, list)

#define APERTURE_RANGE_SHIFT	27	/* 128 MB */
#define APERTURE_RANGE_SIZE	(1ULL << APERTURE_RANGE_SHIFT)
#define APERTURE_RANGE_PAGES	(APERTURE_RANGE_SIZE >> PAGE_SHIFT)
#define APERTURE_MAX_RANGES	32	/* allows 4GB of DMA address space */
#define APERTURE_RANGE_INDEX(a)	((a) >> APERTURE_RANGE_SHIFT)
#define APERTURE_PAGE_INDEX(a)	(((a) >> 21) & 0x3fULL)

/*
 * This structure contains generic data for  IOMMU protection domains
 * independent of their use.
 */
struct protection_domain {
	struct list_head list;  /* for list of all protection domains */
	struct list_head dev_list; /* List of all devices in this domain */
	spinlock_t lock;	/* mostly used to lock the page table*/
	struct mutex api_lock;	/* protect page tables in the iommu-api path */
	u16 id;			/* the domain id written to the device table */
	int mode;		/* paging mode (0-6 levels) */
	u64 *pt_root;		/* page table root pointer */
	unsigned long flags;	/* flags to find out type of domain */
	bool updated;		/* complete domain flush required */
	unsigned dev_cnt;	/* devices assigned to this domain */
	unsigned dev_iommu[MAX_IOMMUS]; /* per-IOMMU reference count */
	void *priv;		/* private data */

};

/*
 * This struct contains device specific data for the IOMMU
 */
struct iommu_dev_data {
	struct list_head list;		  /* For domain->dev_list */
	struct device *dev;		  /* Device this data belong to */
	struct device *alias;		  /* The Alias Device */
	struct protection_domain *domain; /* Domain the device is bound to */
	atomic_t bind;			  /* Domain attach reverent count */
};

/*
 * For dynamic growth the aperture size is split into ranges of 128MB of
 * DMA address space each. This struct represents one such range.
 */
struct aperture_range {

	/* address allocation bitmap */
	unsigned long *bitmap;

	/*
	 * Array of PTE pages for the aperture. In this array we save all the
	 * leaf pages of the domain page table used for the aperture. This way
	 * we don't need to walk the page table to find a specific PTE. We can
	 * just calculate its address in constant time.
	 */
	u64 *pte_pages[64];

	unsigned long offset;
};

/*
 * Data container for a dma_ops specific protection domain
 */
struct dma_ops_domain {
	struct list_head list;

	/* generic protection domain information */
	struct protection_domain domain;

	/* size of the aperture for the mappings */
	unsigned long aperture_size;

	/* address we start to search for free addresses */
	unsigned long next_address;

	/* address space relevant data */
	struct aperture_range *aperture[APERTURE_MAX_RANGES];

	/* This will be set to true when TLB needs to be flushed */
	bool need_flush;

	/*
	 * if this is a preallocated domain, keep the device for which it was
	 * preallocated in this variable
	 */
	u16 target_dev;
};

/*
 * Structure where we save information about one hardware AMD IOMMU in the
 * system.
 */
struct amd_iommu {
	struct list_head list;

	/* Index within the IOMMU array */
	int index;

	/* locks the accesses to the hardware */
	spinlock_t lock;

	/* Pointer to PCI device of this IOMMU */
	struct pci_dev *dev;

	/* physical address of MMIO space */
	u64 mmio_phys;
	/* virtual address of MMIO space */
	u8 *mmio_base;

	/* capabilities of that IOMMU read from ACPI */
	u32 cap;

	/* flags read from acpi table */
	u8 acpi_flags;

	/*
	 * Capability pointer. There could be more than one IOMMU per PCI
	 * device function if there are more than one AMD IOMMU capability
	 * pointers.
	 */
	u16 cap_ptr;

	/* pci domain of this IOMMU */
	u16 pci_seg;

	/* first device this IOMMU handles. read from PCI */
	u16 first_device;
	/* last device this IOMMU handles. read from PCI */
	u16 last_device;

	/* start of exclusion range of that IOMMU */
	u64 exclusion_start;
	/* length of exclusion range of that IOMMU */
	u64 exclusion_length;

	/* command buffer virtual address */
	u8 *cmd_buf;
	/* size of command buffer */
	u32 cmd_buf_size;

	/* size of event buffer */
	u32 evt_buf_size;
	/* event buffer virtual address */
	u8 *evt_buf;
	/* MSI number for event interrupt */
	u16 evt_msi_num;

	/* true if interrupts for this IOMMU are already enabled */
	bool int_enabled;

	/* if one, we need to send a completion wait command */
	bool need_sync;

	/* becomes true if a command buffer reset is running */
	bool reset_in_progress;

	/* default dma_ops domain for that IOMMU */
	struct dma_ops_domain *default_dom;

	/*
	 * We can't rely on the BIOS to restore all values on reinit, so we
	 * need to stash them
	 */

	/* The iommu BAR */
	u32 stored_addr_lo;
	u32 stored_addr_hi;

	/*
	 * Each iommu has 6 l1s, each of which is documented as having 0x12
	 * registers
	 */
	u32 stored_l1[6][0x12];

	/* The l2 indirect registers */
	u32 stored_l2[0x83];
};

/*
 * List with all IOMMUs in the system. This list is not locked because it is
 * only written and read at driver initialization or suspend time
 */
extern struct list_head amd_iommu_list;

/*
 * Array with pointers to each IOMMU struct
 * The indices are referenced in the protection domains
 */
extern struct amd_iommu *amd_iommus[MAX_IOMMUS];

/* Number of IOMMUs present in the system */
extern int amd_iommus_present;

/*
 * Declarations for the global list of all protection domains
 */
extern spinlock_t amd_iommu_pd_lock;
extern struct list_head amd_iommu_pd_list;

/*
 * Structure defining one entry in the device table
 */
struct dev_table_entry {
	u32 data[8];
};

/*
 * One entry for unity mappings parsed out of the ACPI table.
 */
struct unity_map_entry {
	struct list_head list;

	/* starting device id this entry is used for (including) */
	u16 devid_start;
	/* end device id this entry is used for (including) */
	u16 devid_end;

	/* start address to unity map (including) */
	u64 address_start;
	/* end address to unity map (including) */
	u64 address_end;

	/* required protection */
	int prot;
};

/*
 * List of all unity mappings. It is not locked because as runtime it is only
 * read. It is created at ACPI table parsing time.
 */
extern struct list_head amd_iommu_unity_map;

/*
 * Data structures for device handling
 */

/*
 * Device table used by hardware. Read and write accesses by software are
 * locked with the amd_iommu_pd_table lock.
 */
extern struct dev_table_entry *amd_iommu_dev_table;

/*
 * Alias table to find requestor ids to device ids. Not locked because only
 * read on runtime.
 */
extern u16 *amd_iommu_alias_table;

/*
 * Reverse lookup table to find the IOMMU which translates a specific device.
 */
extern struct amd_iommu **amd_iommu_rlookup_table;

/* size of the dma_ops aperture as power of 2 */
extern unsigned amd_iommu_aperture_order;

/* largest PCI device id we expect translation requests for */
extern u16 amd_iommu_last_bdf;

/* allocation bitmap for domain ids */
extern unsigned long *amd_iommu_pd_alloc_bitmap;

/*
 * If true, the addresses will be flushed on unmap time, not when
 * they are reused
 */
extern bool amd_iommu_unmap_flush;

/* takes bus and device/function and returns the device id
 * FIXME: should that be in generic PCI code? */
static inline u16 calc_devid(u8 bus, u8 devfn)
{
	return (((u16)bus) << 8) | devfn;
}

#ifdef CONFIG_AMD_IOMMU_STATS

struct __iommu_counter {
	char *name;
	struct dentry *dent;
	u64 value;
};

#define DECLARE_STATS_COUNTER(nm) \
	static struct __iommu_counter nm = {	\
		.name = #nm,			\
	}

#define INC_STATS_COUNTER(name)		name.value += 1
#define ADD_STATS_COUNTER(name, x)	name.value += (x)
#define SUB_STATS_COUNTER(name, x)	name.value -= (x)

#else /* CONFIG_AMD_IOMMU_STATS */

#define DECLARE_STATS_COUNTER(name)
#define INC_STATS_COUNTER(name)
#define ADD_STATS_COUNTER(name, x)
#define SUB_STATS_COUNTER(name, x)

#endif /* CONFIG_AMD_IOMMU_STATS */

#endif /* _ASM_X86_AMD_IOMMU_TYPES_H */
