/*
 * pcicfg.h: PCI configuration constants and structures.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: pcicfg.h 467120 2014-04-02 15:21:44Z $
 */

#ifndef	_h_pcicfg_
#define	_h_pcicfg_

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* The following inside ifndef's so we don't collide with NTDDK.H */
#ifndef PCI_MAX_BUS
#define PCI_MAX_BUS		0x100
#endif
#ifndef PCI_MAX_DEVICES
#define PCI_MAX_DEVICES		0x20
#endif
#ifndef PCI_MAX_FUNCTION
#define PCI_MAX_FUNCTION	0x8
#endif

#ifndef PCI_INVALID_VENDORID
#define PCI_INVALID_VENDORID	0xffff
#endif
#ifndef PCI_INVALID_DEVICEID
#define PCI_INVALID_DEVICEID	0xffff
#endif


/* Convert between bus-slot-function-register and config addresses */

#define	PCICFG_BUS_SHIFT	16	/* Bus shift */
#define	PCICFG_SLOT_SHIFT	11	/* Slot shift */
#define	PCICFG_FUN_SHIFT	8	/* Function shift */
#define	PCICFG_OFF_SHIFT	0	/* Register shift */

#define	PCICFG_BUS_MASK		0xff	/* Bus mask */
#define	PCICFG_SLOT_MASK	0x1f	/* Slot mask */
#define	PCICFG_FUN_MASK		7	/* Function mask */
#define	PCICFG_OFF_MASK		0xff	/* Bus mask */

#define	PCI_CONFIG_ADDR(b, s, f, o)					\
		((((b) & PCICFG_BUS_MASK) << PCICFG_BUS_SHIFT)		\
		 | (((s) & PCICFG_SLOT_MASK) << PCICFG_SLOT_SHIFT)	\
		 | (((f) & PCICFG_FUN_MASK) << PCICFG_FUN_SHIFT)	\
		 | (((o) & PCICFG_OFF_MASK) << PCICFG_OFF_SHIFT))

#define	PCI_CONFIG_BUS(a)	(((a) >> PCICFG_BUS_SHIFT) & PCICFG_BUS_MASK)
#define	PCI_CONFIG_SLOT(a)	(((a) >> PCICFG_SLOT_SHIFT) & PCICFG_SLOT_MASK)
#define	PCI_CONFIG_FUN(a)	(((a) >> PCICFG_FUN_SHIFT) & PCICFG_FUN_MASK)
#define	PCI_CONFIG_OFF(a)	(((a) >> PCICFG_OFF_SHIFT) & PCICFG_OFF_MASK)

/* PCIE Config space accessing MACROS */

#define	PCIECFG_BUS_SHIFT	24	/* Bus shift */
#define	PCIECFG_SLOT_SHIFT	19	/* Slot/Device shift */
#define	PCIECFG_FUN_SHIFT	16	/* Function shift */
#define	PCIECFG_OFF_SHIFT	0	/* Register shift */

#define	PCIECFG_BUS_MASK	0xff	/* Bus mask */
#define	PCIECFG_SLOT_MASK	0x1f	/* Slot/Device mask */
#define	PCIECFG_FUN_MASK	7	/* Function mask */
#define	PCIECFG_OFF_MASK	0xfff	/* Register mask */

#define	PCIE_CONFIG_ADDR(b, s, f, o)					\
		((((b) & PCIECFG_BUS_MASK) << PCIECFG_BUS_SHIFT)		\
		 | (((s) & PCIECFG_SLOT_MASK) << PCIECFG_SLOT_SHIFT)	\
		 | (((f) & PCIECFG_FUN_MASK) << PCIECFG_FUN_SHIFT)	\
		 | (((o) & PCIECFG_OFF_MASK) << PCIECFG_OFF_SHIFT))

#define	PCIE_CONFIG_BUS(a)	(((a) >> PCIECFG_BUS_SHIFT) & PCIECFG_BUS_MASK)
#define	PCIE_CONFIG_SLOT(a)	(((a) >> PCIECFG_SLOT_SHIFT) & PCIECFG_SLOT_MASK)
#define	PCIE_CONFIG_FUN(a)	(((a) >> PCIECFG_FUN_SHIFT) & PCIECFG_FUN_MASK)
#define	PCIE_CONFIG_OFF(a)	(((a) >> PCIECFG_OFF_SHIFT) & PCIECFG_OFF_MASK)

/* The actual config space */

#define	PCI_BAR_MAX		6

#define	PCI_ROM_BAR		8

#define	PCR_RSVDA_MAX		2

/* Bits in PCI bars' flags */

#define	PCIBAR_FLAGS		0xf
#define	PCIBAR_IO		0x1
#define	PCIBAR_MEM1M		0x2
#define	PCIBAR_MEM64		0x4
#define	PCIBAR_PREFETCH		0x8
#define	PCIBAR_MEM32_MASK	0xFFFFFF80

/* pci config status reg has a bit to indicate that capability ptr is present */

#define PCI_CAPPTR_PRESENT	0x0010

typedef struct _pci_config_regs {
	uint16	vendor;
	uint16	device;
	uint16	command;
	uint16	status;
	uint8	rev_id;
	uint8	prog_if;
	uint8	sub_class;
	uint8	base_class;
	uint8	cache_line_size;
	uint8	latency_timer;
	uint8	header_type;
	uint8	bist;
	uint32	base[PCI_BAR_MAX];
	uint32	cardbus_cis;
	uint16	subsys_vendor;
	uint16	subsys_id;
	uint32	baserom;
	uint32	rsvd_a[PCR_RSVDA_MAX];
	uint8	int_line;
	uint8	int_pin;
	uint8	min_gnt;
	uint8	max_lat;
	uint8	dev_dep[192];
} pci_config_regs;

#define	SZPCR		(sizeof (pci_config_regs))
#define	MINSZPCR	64		/* offsetof (dev_dep[0] */

#endif /* !LINUX_POSTMOGRIFY_REMOVAL */
/* A structure for the config registers is nice, but in most
 * systems the config space is not memory mapped, so we need
 * field offsetts. :-(
 */
#define	PCI_CFG_VID		0
#define	PCI_CFG_DID		2
#define	PCI_CFG_CMD		4
#define	PCI_CFG_STAT		6
#define	PCI_CFG_REV		8
#define	PCI_CFG_PROGIF		9
#define	PCI_CFG_SUBCL		0xa
#define	PCI_CFG_BASECL		0xb
#define	PCI_CFG_CLSZ		0xc
#define	PCI_CFG_LATTIM		0xd
#define	PCI_CFG_HDR		0xe
#define	PCI_CFG_BIST		0xf
#define	PCI_CFG_BAR0		0x10
#define	PCI_CFG_BAR1		0x14
#define	PCI_CFG_BAR2		0x18
#define	PCI_CFG_BAR3		0x1c
#define	PCI_CFG_BAR4		0x20
#define	PCI_CFG_BAR5		0x24
#define	PCI_CFG_CIS		0x28
#define	PCI_CFG_SVID		0x2c
#define	PCI_CFG_SSID		0x2e
#define	PCI_CFG_ROMBAR		0x30
#define PCI_CFG_CAPPTR		0x34
#define	PCI_CFG_INT		0x3c
#define	PCI_CFG_PIN		0x3d
#define	PCI_CFG_MINGNT		0x3e
#define	PCI_CFG_MAXLAT		0x3f
#define	PCI_CFG_DEVCTRL		0xd8
#ifndef LINUX_POSTMOGRIFY_REMOVAL

#ifdef __NetBSD__
#undef	PCI_CLASS_DISPLAY
#undef	PCI_CLASS_MEMORY
#undef	PCI_CLASS_BRIDGE
#undef	PCI_CLASS_INPUT
#undef	PCI_CLASS_DOCK
#endif	/* __NetBSD__ */

#ifdef EFI
#undef PCI_CLASS_BRIDGE
#undef PCI_CLASS_OLD
#undef PCI_CLASS_DISPLAY
#undef PCI_CLASS_SERIAL
#undef PCI_CLASS_SATELLITE
#endif /* EFI */

/* Classes and subclasses */

typedef enum {
	PCI_CLASS_OLD = 0,
	PCI_CLASS_DASDI,
	PCI_CLASS_NET,
	PCI_CLASS_DISPLAY,
	PCI_CLASS_MMEDIA,
	PCI_CLASS_MEMORY,
	PCI_CLASS_BRIDGE,
	PCI_CLASS_COMM,
	PCI_CLASS_BASE,
	PCI_CLASS_INPUT,
	PCI_CLASS_DOCK,
	PCI_CLASS_CPU,
	PCI_CLASS_SERIAL,
	PCI_CLASS_INTELLIGENT = 0xe,
	PCI_CLASS_SATELLITE,
	PCI_CLASS_CRYPT,
	PCI_CLASS_DSP,
	PCI_CLASS_XOR = 0xfe
} pci_classes;

typedef enum {
	PCI_DASDI_SCSI,
	PCI_DASDI_IDE,
	PCI_DASDI_FLOPPY,
	PCI_DASDI_IPI,
	PCI_DASDI_RAID,
	PCI_DASDI_OTHER = 0x80
} pci_dasdi_subclasses;

typedef enum {
	PCI_NET_ETHER,
	PCI_NET_TOKEN,
	PCI_NET_FDDI,
	PCI_NET_ATM,
	PCI_NET_OTHER = 0x80
} pci_net_subclasses;

typedef enum {
	PCI_DISPLAY_VGA,
	PCI_DISPLAY_XGA,
	PCI_DISPLAY_3D,
	PCI_DISPLAY_OTHER = 0x80
} pci_display_subclasses;

typedef enum {
	PCI_MMEDIA_VIDEO,
	PCI_MMEDIA_AUDIO,
	PCI_MMEDIA_PHONE,
	PCI_MEDIA_OTHER = 0x80
} pci_mmedia_subclasses;

typedef enum {
	PCI_MEMORY_RAM,
	PCI_MEMORY_FLASH,
	PCI_MEMORY_OTHER = 0x80
} pci_memory_subclasses;

typedef enum {
	PCI_BRIDGE_HOST,
	PCI_BRIDGE_ISA,
	PCI_BRIDGE_EISA,
	PCI_BRIDGE_MC,
	PCI_BRIDGE_PCI,
	PCI_BRIDGE_PCMCIA,
	PCI_BRIDGE_NUBUS,
	PCI_BRIDGE_CARDBUS,
	PCI_BRIDGE_RACEWAY,
	PCI_BRIDGE_OTHER = 0x80
} pci_bridge_subclasses;

typedef enum {
	PCI_COMM_UART,
	PCI_COMM_PARALLEL,
	PCI_COMM_MULTIUART,
	PCI_COMM_MODEM,
	PCI_COMM_OTHER = 0x80
} pci_comm_subclasses;

typedef enum {
	PCI_BASE_PIC,
	PCI_BASE_DMA,
	PCI_BASE_TIMER,
	PCI_BASE_RTC,
	PCI_BASE_PCI_HOTPLUG,
	PCI_BASE_OTHER = 0x80
} pci_base_subclasses;

typedef enum {
	PCI_INPUT_KBD,
	PCI_INPUT_PEN,
	PCI_INPUT_MOUSE,
	PCI_INPUT_SCANNER,
	PCI_INPUT_GAMEPORT,
	PCI_INPUT_OTHER = 0x80
} pci_input_subclasses;

typedef enum {
	PCI_DOCK_GENERIC,
	PCI_DOCK_OTHER = 0x80
} pci_dock_subclasses;

typedef enum {
	PCI_CPU_386,
	PCI_CPU_486,
	PCI_CPU_PENTIUM,
	PCI_CPU_ALPHA = 0x10,
	PCI_CPU_POWERPC = 0x20,
	PCI_CPU_MIPS = 0x30,
	PCI_CPU_COPROC = 0x40,
	PCI_CPU_OTHER = 0x80
} pci_cpu_subclasses;

typedef enum {
	PCI_SERIAL_IEEE1394,
	PCI_SERIAL_ACCESS,
	PCI_SERIAL_SSA,
	PCI_SERIAL_USB,
	PCI_SERIAL_FIBER,
	PCI_SERIAL_SMBUS,
	PCI_SERIAL_OTHER = 0x80
} pci_serial_subclasses;

typedef enum {
	PCI_INTELLIGENT_I2O
} pci_intelligent_subclasses;

typedef enum {
	PCI_SATELLITE_TV,
	PCI_SATELLITE_AUDIO,
	PCI_SATELLITE_VOICE,
	PCI_SATELLITE_DATA,
	PCI_SATELLITE_OTHER = 0x80
} pci_satellite_subclasses;

typedef enum {
	PCI_CRYPT_NETWORK,
	PCI_CRYPT_ENTERTAINMENT,
	PCI_CRYPT_OTHER = 0x80
} pci_crypt_subclasses;

typedef enum {
	PCI_DSP_DPIO,
	PCI_DSP_OTHER = 0x80
} pci_dsp_subclasses;

typedef enum {
	PCI_XOR_QDMA,
	PCI_XOR_OTHER = 0x80
} pci_xor_subclasses;

/* Header types */
#define	PCI_HEADER_MULTI	0x80
#define	PCI_HEADER_MASK		0x7f
typedef enum {
	PCI_HEADER_NORMAL,
	PCI_HEADER_BRIDGE,
	PCI_HEADER_CARDBUS
} pci_header_types;


/* Overlay for a PCI-to-PCI bridge */

#define	PPB_RSVDA_MAX		2
#define	PPB_RSVDD_MAX		8

typedef struct _ppb_config_regs {
	uint16	vendor;
	uint16	device;
	uint16	command;
	uint16	status;
	uint8	rev_id;
	uint8	prog_if;
	uint8	sub_class;
	uint8	base_class;
	uint8	cache_line_size;
	uint8	latency_timer;
	uint8	header_type;
	uint8	bist;
	uint32	rsvd_a[PPB_RSVDA_MAX];
	uint8	prim_bus;
	uint8	sec_bus;
	uint8	sub_bus;
	uint8	sec_lat;
	uint8	io_base;
	uint8	io_lim;
	uint16	sec_status;
	uint16	mem_base;
	uint16	mem_lim;
	uint16	pf_mem_base;
	uint16	pf_mem_lim;
	uint32	pf_mem_base_hi;
	uint32	pf_mem_lim_hi;
	uint16	io_base_hi;
	uint16	io_lim_hi;
	uint16	subsys_vendor;
	uint16	subsys_id;
	uint32	rsvd_b;
	uint8	rsvd_c;
	uint8	int_pin;
	uint16	bridge_ctrl;
	uint8	chip_ctrl;
	uint8	diag_ctrl;
	uint16	arb_ctrl;
	uint32	rsvd_d[PPB_RSVDD_MAX];
	uint8	dev_dep[192];
} ppb_config_regs;


/* PCI CAPABILITY DEFINES */
#define PCI_CAP_POWERMGMTCAP_ID		0x01
#define PCI_CAP_MSICAP_ID		0x05
#define PCI_CAP_VENDSPEC_ID		0x09
#define PCI_CAP_PCIECAP_ID		0x10

/* Data structure to define the Message Signalled Interrupt facility
 * Valid for PCI and PCIE configurations
 */
typedef struct _pciconfig_cap_msi {
	uint8	capID;
	uint8	nextptr;
	uint16	msgctrl;
	uint32	msgaddr;
} pciconfig_cap_msi;
#define MSI_ENABLE	0x1		/* bit 0 of msgctrl */

/* Data structure to define the Power managment facility
 * Valid for PCI and PCIE configurations
 */
typedef struct _pciconfig_cap_pwrmgmt {
	uint8	capID;
	uint8	nextptr;
	uint16	pme_cap;
	uint16	pme_sts_ctrl;
	uint8	pme_bridge_ext;
	uint8	data;
} pciconfig_cap_pwrmgmt;

#define PME_CAP_PM_STATES (0x1f << 27)	/* Bits 31:27 states that can generate PME */
#define PME_CSR_OFFSET	    0x4		/* 4-bytes offset */
#define PME_CSR_PME_EN	  (1 << 8)	/* Bit 8 Enable generating of PME */
#define PME_CSR_PME_STAT  (1 << 15)	/* Bit 15 PME got asserted */

/* Data structure to define the PCIE capability */
typedef struct _pciconfig_cap_pcie {
	uint8	capID;
	uint8	nextptr;
	uint16	pcie_cap;
	uint32	dev_cap;
	uint16	dev_ctrl;
	uint16	dev_status;
	uint32	link_cap;
	uint16	link_ctrl;
	uint16	link_status;
	uint32	slot_cap;
	uint16	slot_ctrl;
	uint16	slot_status;
	uint16	root_ctrl;
	uint16	root_cap;
	uint32	root_status;
} pciconfig_cap_pcie;

/* PCIE Enhanced CAPABILITY DEFINES */
#define PCIE_EXTCFG_OFFSET	0x100
#define PCIE_ADVERRREP_CAPID	0x0001
#define PCIE_VC_CAPID		0x0002
#define PCIE_DEVSNUM_CAPID	0x0003
#define PCIE_PWRBUDGET_CAPID	0x0004

/* PCIE Extended configuration */
#define PCIE_ADV_CORR_ERR_MASK	0x114
#define CORR_ERR_RE	(1 << 0) /* Receiver  */
#define CORR_ERR_BT 	(1 << 6) /* Bad TLP  */
#define CORR_ERR_BD	(1 << 7) /* Bad DLLP */
#define CORR_ERR_RR	(1 << 8) /* REPLAY_NUM rollover */
#define CORR_ERR_RT	(1 << 12) /* Reply timer timeout */
#define ALL_CORR_ERRORS (CORR_ERR_RE | CORR_ERR_BT | CORR_ERR_BD | \
			 CORR_ERR_RR | CORR_ERR_RT)

/* PCIE Root Control Register bits (Host mode only) */
#define	PCIE_RC_CORR_SERR_EN		0x0001
#define	PCIE_RC_NONFATAL_SERR_EN	0x0002
#define	PCIE_RC_FATAL_SERR_EN		0x0004
#define	PCIE_RC_PME_INT_EN		0x0008
#define	PCIE_RC_CRS_EN			0x0010

/* PCIE Root Capability Register bits (Host mode only) */
#define	PCIE_RC_CRS_VISIBILITY		0x0001

/* Header to define the PCIE specific capabilities in the extended config space */
typedef struct _pcie_enhanced_caphdr {
	uint16	capID;
	uint16	cap_ver : 4;
	uint16	next_ptr : 12;
} pcie_enhanced_caphdr;


/* Everything below is BRCM HND proprietary */


/* Brcm PCI configuration registers */
#define cap_list	rsvd_a[0]
#define bar0_window	dev_dep[0x80 - 0x40]
#define bar1_window	dev_dep[0x84 - 0x40]
#define sprom_control	dev_dep[0x88 - 0x40]
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#define	PCI_BAR0_WIN		0x80	/* backplane addres space accessed by BAR0 */
#define	PCI_BAR1_WIN		0x84	/* backplane addres space accessed by BAR1 */
#define	PCI_SPROM_CONTROL	0x88	/* sprom property control */
#define	PCI_BAR1_CONTROL	0x8c	/* BAR1 region burst control */
#define	PCI_INT_STATUS		0x90	/* PCI and other cores interrupts */
#define	PCI_INT_MASK		0x94	/* mask of PCI and other cores interrupts */
#define PCI_TO_SB_MB		0x98	/* signal backplane interrupts */
#define PCI_BACKPLANE_ADDR	0xa0	/* address an arbitrary location on the system backplane */
#define PCI_BACKPLANE_DATA	0xa4	/* data at the location specified by above address */
#define	PCI_CLK_CTL_ST		0xa8	/* pci config space clock control/status (>=rev14) */
#define	PCI_BAR0_WIN2		0xac	/* backplane addres space accessed by second 4KB of BAR0 */
#define	PCI_GPIO_IN		0xb0	/* pci config space gpio input (>=rev3) */
#define	PCI_GPIO_OUT		0xb4	/* pci config space gpio output (>=rev3) */
#define	PCI_GPIO_OUTEN		0xb8	/* pci config space gpio output enable (>=rev3) */
#define	PCI_L1SS_CTRL2		0x24c	/* The L1 PM Substates Control register */

/* Private Registers */
#define	PCI_STAT_CTRL		0xa80
#define	PCI_L0_EVENTCNT		0xa84
#define	PCI_L0_STATETMR		0xa88
#define	PCI_L1_EVENTCNT		0xa8c
#define	PCI_L1_STATETMR		0xa90
#define	PCI_L1_1_EVENTCNT	0xa94
#define	PCI_L1_1_STATETMR	0xa98
#define	PCI_L1_2_EVENTCNT	0xa9c
#define	PCI_L1_2_STATETMR	0xaa0
#define	PCI_L2_EVENTCNT		0xaa4
#define	PCI_L2_STATETMR		0xaa8

#define	PCI_PMCR_REFUP		0x1814	/* Trefup time */
#define	PCI_PMCR_REFUP_EXT	0x1818	/* Trefup extend Max */
#define PCI_TPOWER_SCALE_MASK 0x03	/* TPowerOnScale  1:0 This field specifies the scale */
#define PCI_TPOWER_VALUE_SHIFT 3	/* TPowerOnScale  7:3 This field contains the time */


#define	PCI_BAR0_SHADOW_OFFSET	(2 * 1024)	/* bar0 + 2K accesses sprom shadow (in pci core) */
#define	PCI_BAR0_SPROM_OFFSET	(4 * 1024)	/* bar0 + 4K accesses external sprom */
#define	PCI_BAR0_PCIREGS_OFFSET	(6 * 1024)	/* bar0 + 6K accesses pci core registers */
#define	PCI_BAR0_PCISBR_OFFSET	(4 * 1024)	/* pci core SB registers are at the end of the
						 * 8KB window, so their address is the "regular"
						 * address plus 4K
						 */
/*
 * PCIE GEN2 changed some of the above locations for
 * Bar0WrapperBase, SecondaryBAR0Window and SecondaryBAR0WrapperBase
 * BAR0 maps 32K of register space
*/
#define PCIE2_BAR0_WIN2		0x70 /* backplane addres space accessed by second 4KB of BAR0 */
#define PCIE2_BAR0_CORE2_WIN	0x74 /* backplane addres space accessed by second 4KB of BAR0 */
#define PCIE2_BAR0_CORE2_WIN2	0x78 /* backplane addres space accessed by second 4KB of BAR0 */

#define PCI_BAR0_WINSZ		(16 * 1024)	/* bar0 window size Match with corerev 13 */
/* On pci corerev >= 13 and all pcie, the bar0 is now 16KB and it maps: */
#define	PCI_16KB0_PCIREGS_OFFSET (8 * 1024)	/* bar0 + 8K accesses pci/pcie core registers */
#define	PCI_16KB0_CCREGS_OFFSET	(12 * 1024)	/* bar0 + 12K accesses chipc core registers */
#define PCI_16KBB0_WINSZ	(16 * 1024)	/* bar0 window size */

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* On AI chips we have a second window to map DMP regs are mapped: */
#define	PCI_16KB0_WIN2_OFFSET	(4 * 1024)	/* bar0 + 4K is "Window 2" */

/* PCI_INT_STATUS */
#define	PCI_SBIM_STATUS_SERR	0x4	/* backplane SBErr interrupt status */

/* PCI_INT_MASK */
#define	PCI_SBIM_SHIFT		8	/* backplane core interrupt mask bits offset */
#define	PCI_SBIM_MASK		0xff00	/* backplane core interrupt mask */
#define	PCI_SBIM_MASK_SERR	0x4	/* backplane SBErr interrupt mask */

#ifndef LINUX_POSTMOGRIFY_REMOVAL
/* PCI_SPROM_CONTROL */
#define SPROM_SZ_MSK		0x02	/* SPROM Size Mask */
#define SPROM_LOCKED		0x08	/* SPROM Locked */
#define	SPROM_BLANK		0x04	/* indicating a blank SPROM */
#define SPROM_WRITEEN		0x10	/* SPROM write enable */
#define SPROM_BOOTROM_WE	0x20	/* external bootrom write enable */
#define SPROM_BACKPLANE_EN	0x40	/* Enable indirect backplane access */
#define SPROM_OTPIN_USE		0x80	/* device OTP In use */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

/* Bits in PCI command and status regs */
#define PCI_CMD_IO		0x00000001	/* I/O enable */
#define PCI_CMD_MEMORY		0x00000002	/* Memory enable */
#define PCI_CMD_MASTER		0x00000004	/* Master enable */
#define PCI_CMD_SPECIAL		0x00000008	/* Special cycles enable */
#define PCI_CMD_INVALIDATE	0x00000010	/* Invalidate? */
#define PCI_CMD_VGA_PAL		0x00000040	/* VGA Palate */
#define PCI_STAT_TA		0x08000000	/* target abort status */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

#define PCI_CONFIG_SPACE_SIZE	256
#endif	/* _h_pcicfg_ */
