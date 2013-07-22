/*
 * Copyright (c) 1997 Zubin D. Dittia.  All rights reserved.
 * Copyright (c) 1995, 1996, 1998
 *	Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCI autoconfiguration support functions.
 */

#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#ifndef PCIVERBOSE
# if CFG_MINIMAL_SIZE
#  define PCIVERBOSE 0
# else
#  define PCIVERBOSE 1
# endif
#endif

#include "pcireg.h"
#include "pcivar.h"

const char *pci_findvendor(pcireg_t id_reg);

static void pci_conf_print_ids(pcireg_t rval, const char *prefix);
static int pci_conf_print_bar (pcitag_t, 
    const pcireg_t *, int, const char *, int, int);

/*
 * Descriptions of known PCI classes and subclasses.
 *
 * Subclasses are described in the same way as classes, but have a
 * NULL subclass pointer.
 */
struct pci_class {
    char	*name;
    int		val;		/* as wide as pci_{,sub}class_t */
    struct pci_class *subclasses;
};

struct pci_class pci_subclass_prehistoric[] = {
    { "miscellaneous",	PCI_SUBCLASS_PREHISTORIC_MISC,		},
    { "VGA",		PCI_SUBCLASS_PREHISTORIC_VGA,		},
    { 0 }
};

struct pci_class pci_subclass_mass_storage[] = {
    { "SCSI",		PCI_SUBCLASS_MASS_STORAGE_SCSI,		},
    { "IDE",		PCI_SUBCLASS_MASS_STORAGE_IDE,		},
    { "floppy",		PCI_SUBCLASS_MASS_STORAGE_FLOPPY,	},
    { "IPI",		PCI_SUBCLASS_MASS_STORAGE_IPI,		},
    { "RAID",		PCI_SUBCLASS_MASS_STORAGE_RAID,		},
    { "miscellaneous",	PCI_SUBCLASS_MASS_STORAGE_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_network[] = {
    { "ethernet",	PCI_SUBCLASS_NETWORK_ETHERNET,		},
    { "token ring",	PCI_SUBCLASS_NETWORK_TOKENRING,		},
    { "FDDI",		PCI_SUBCLASS_NETWORK_FDDI,		},
    { "ATM",		PCI_SUBCLASS_NETWORK_ATM,		},
    { "ISDN",		PCI_SUBCLASS_NETWORK_ISDN,		},
    { "WorldFip",	PCI_SUBCLASS_NETWORK_WORLDFIP,		},
    { "PCMIG MultiComp", PCI_SUBCLASS_NETWORK_PCIMGMULTICOMP,	},
    { "miscellaneous",	PCI_SUBCLASS_NETWORK_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_display[] = {
    { "VGA",		PCI_SUBCLASS_DISPLAY_VGA,		},
    { "XGA",		PCI_SUBCLASS_DISPLAY_XGA,		},
    { "3D",		PCI_SUBCLASS_DISPLAY_3D,		},
    { "miscellaneous",	PCI_SUBCLASS_DISPLAY_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_multimedia[] = {
    { "video",		PCI_SUBCLASS_MULTIMEDIA_VIDEO,		},
    { "audio",		PCI_SUBCLASS_MULTIMEDIA_AUDIO,		},
    { "telephony",	PCI_SUBCLASS_MULTIMEDIA_TELEPHONY,	},
    { "miscellaneous",	PCI_SUBCLASS_MULTIMEDIA_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_memory[] = {
    { "RAM",		PCI_SUBCLASS_MEMORY_RAM,		},
    { "flash",		PCI_SUBCLASS_MEMORY_FLASH,		},
    { "miscellaneous",	PCI_SUBCLASS_MEMORY_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_bridge[] = {
    { "host",		PCI_SUBCLASS_BRIDGE_HOST,		},
    { "ISA",		PCI_SUBCLASS_BRIDGE_ISA,		},
    { "EISA",		PCI_SUBCLASS_BRIDGE_EISA,		},
    { "MicroChannel",	PCI_SUBCLASS_BRIDGE_MCA,		},
    { "PCI",		PCI_SUBCLASS_BRIDGE_PCI,		},
    { "PCMCIA",		PCI_SUBCLASS_BRIDGE_PCMCIA,		},
    { "NuBus",		PCI_SUBCLASS_BRIDGE_NUBUS,		},
    { "CardBus",	PCI_SUBCLASS_BRIDGE_CARDBUS,		},
    { "RACEway",	PCI_SUBCLASS_BRIDGE_RACEWAY,		},
    { "Semi-transparent PCI",	PCI_SUBCLASS_BRIDGE_STPCI,	},
    { "InfiniBand",	PCI_SUBCLASS_BRIDGE_INFINIBAND,		},
    { "miscellaneous",	PCI_SUBCLASS_BRIDGE_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_communications[] = {
    { "serial",		PCI_SUBCLASS_COMMUNICATIONS_SERIAL,	},
    { "parallel",	PCI_SUBCLASS_COMMUNICATIONS_PARALLEL,	},
    { "multi-port serial",	PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL,	},
    { "modem",		PCI_SUBCLASS_COMMUNICATIONS_MODEM,	},
    { "miscellaneous",	PCI_SUBCLASS_COMMUNICATIONS_MISC,	},
    { 0 },
};

struct pci_class pci_subclass_system[] = {
    { "8259 PIC",	PCI_SUBCLASS_SYSTEM_PIC,		},
    { "8237 DMA",	PCI_SUBCLASS_SYSTEM_DMA,		},
    { "8254 timer",	PCI_SUBCLASS_SYSTEM_TIMER,		},
    { "RTC",		PCI_SUBCLASS_SYSTEM_RTC,		},
    { "PCI Hot-Plug",	PCI_SUBCLASS_SYSTEM_PCIHOTPLUG,		},
    { "miscellaneous",	PCI_SUBCLASS_SYSTEM_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_input[] = {
    { "keyboard",	PCI_SUBCLASS_INPUT_KEYBOARD,		},
    { "digitizer",	PCI_SUBCLASS_INPUT_DIGITIZER,		},
    { "mouse",		PCI_SUBCLASS_INPUT_MOUSE,		},
    { "scanner",	PCI_SUBCLASS_INPUT_SCANNER,		},
    { "game port",	PCI_SUBCLASS_INPUT_GAMEPORT,		},
    { "miscellaneous",	PCI_SUBCLASS_INPUT_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_dock[] = {
    { "generic",	PCI_SUBCLASS_DOCK_GENERIC,		},
    { "miscellaneous",	PCI_SUBCLASS_DOCK_MISC,			},
    { 0 },
};

struct pci_class pci_subclass_processor[] = {
    { "386",		PCI_SUBCLASS_PROCESSOR_386,		},
    { "486",		PCI_SUBCLASS_PROCESSOR_486,		},
    { "Pentium",	PCI_SUBCLASS_PROCESSOR_PENTIUM,		},
    { "Alpha",		PCI_SUBCLASS_PROCESSOR_ALPHA,		},
    { "PowerPC",	PCI_SUBCLASS_PROCESSOR_POWERPC,		},
    { "MIPS",		PCI_SUBCLASS_PROCESSOR_MIPS,		},
    { "Co-processor",	PCI_SUBCLASS_PROCESSOR_COPROC,		},
    { 0 },
};

struct pci_class pci_subclass_serialbus[] = {
    { "Firewire",	PCI_SUBCLASS_SERIALBUS_FIREWIRE,	},
    { "ACCESS.bus",	PCI_SUBCLASS_SERIALBUS_ACCESS,		},
    { "SSA",		PCI_SUBCLASS_SERIALBUS_SSA,		},
    { "USB",		PCI_SUBCLASS_SERIALBUS_USB,		},
    { "Fiber Channel",	PCI_SUBCLASS_SERIALBUS_FIBER,		},
    { "SMBus",		PCI_SUBCLASS_SERIALBUS_SMBUS,		},
    { "InfiniBand",	PCI_SUBCLASS_SERIALBUS_INFINIBAND,	},
    { "IPMI",		PCI_SUBCLASS_SERIALBUS_IPMI,		},
    { "SERCOS",		PCI_SUBCLASS_SERIALBUS_SERCOS,		},
    { "CANbus",		PCI_SUBCLASS_SERIALBUS_CANBUS,		},
    { 0 },
};

struct pci_class pci_subclass_wireless[] = {
    { "iRDA",		PCI_SUBCLASS_WIRELESS_IRDA,		},
    { "Consumer IR",	PCI_SUBCLASS_WIRELESS_CONSUMERIR,	},
    { "RF",		PCI_SUBCLASS_WIRELESS_RF,		},
    { "miscellaneous",	PCI_SUBCLASS_WIRELESS_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_i2o[] = {
    { "1.0",		PCI_SUBCLASS_I2O_STANDARD,		},
    { 0 },
};

struct pci_class pci_subclass_satcom[] = {
    { "TV",		PCI_SUBCLASS_SATCOM_TV,			},
    { "audio",		PCI_SUBCLASS_SATCOM_AUDIO,		},
    { "voice",		PCI_SUBCLASS_SATCOM_VOICE,		},
    { "data",		PCI_SUBCLASS_SATCOM_DATA,		},
    { 0 },
};

struct pci_class pci_subclass_crypto[] = {
    { "network/computing",	PCI_SUBCLASS_CRYPTO_NETCOMP,	},
    { "entertainment",	PCI_SUBCLASS_CRYPTO_ENTERTAINMENT,	},
    { "miscellaneous",	PCI_SUBCLASS_CRYPTO_MISC,		},
    { 0 },
};

struct pci_class pci_subclass_dasp[] = {
    { "DPIO",		PCI_SUBCLASS_DASP_DPIO,			},
    { "time and frequency",	PCI_SUBCLASS_DASP_TIMERFREQ,	},
    { "miscellaneous",	PCI_SUBCLASS_DASP_MISC,			},
    { 0 },
};

struct pci_class pci_class[] = {
    { "prehistoric",	PCI_CLASS_PREHISTORIC,
        pci_subclass_prehistoric,			},
    { "mass storage",	PCI_CLASS_MASS_STORAGE,
        pci_subclass_mass_storage,			},
    { "network",	PCI_CLASS_NETWORK,
        pci_subclass_network,				},
    { "display",	PCI_CLASS_DISPLAY,
        pci_subclass_display,				},
    { "multimedia",	PCI_CLASS_MULTIMEDIA,
        pci_subclass_multimedia,			},
    { "memory",		PCI_CLASS_MEMORY,
        pci_subclass_memory,				},
    { "bridge",		PCI_CLASS_BRIDGE,
        pci_subclass_bridge,				},
    { "communications",	PCI_CLASS_COMMUNICATIONS,
        pci_subclass_communications,			},
    { "system",		PCI_CLASS_SYSTEM,
        pci_subclass_system,				},
    { "input",		PCI_CLASS_INPUT,
        pci_subclass_input,				},
    { "dock",		PCI_CLASS_DOCK,
        pci_subclass_dock,				},
    { "processor",	PCI_CLASS_PROCESSOR,
        pci_subclass_processor,				},
    { "serial bus",	PCI_CLASS_SERIALBUS,
        pci_subclass_serialbus,				},
    { "wireless",	PCI_CLASS_WIRELESS,
        pci_subclass_wireless,				},
    { "I2O",		PCI_CLASS_I2O,
        pci_subclass_i2o,				},
    { "satellite comm",	PCI_CLASS_SATCOM,
        pci_subclass_satcom,				},
    { "crypto",		PCI_CLASS_CRYPTO,
        pci_subclass_crypto,				},
    { "DASP",		PCI_CLASS_DASP,
        pci_subclass_dasp,				},
    { "undefined",	PCI_CLASS_UNDEFINED,
        0,						},
    { 0 },
};

#if PCIVERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
#include "pcidevs.h"

struct pci_knowndev2 {
    pci_vendor_id_t	vendor;
    pci_product_id_t	product;
    int			flags;
    int	vendorname, productname;
};
#define	PCI_KNOWNDEV_NOPROD	0x01		/* match on vendor only */
#include "pcidevs_data2.h"

const char *
pci_findvendor(pcireg_t id_reg)
{
    pci_vendor_id_t vendor = PCI_VENDOR(id_reg);
    const struct pci_knowndev2 *kdp;

    kdp = pci_knowndevs;
    while (kdp->vendorname != PCI_STRING_NULL) {  /* all have vendor name */
        if (kdp->vendor == vendor)
            break;
        kdp++;
    }
    if (kdp->vendorname == PCI_STRING_NULL) return NULL;
    return PCI_STRING(kdp->vendorname);
}
#else
const char *
pci_findvendor(pcireg_t id_reg)
{
    return NULL;
}
#endif /* PCIVERBOSE */

void
pci_devinfo(pcireg_t id_reg, pcireg_t class_reg, int showclass, char *cp)
{
    pci_vendor_id_t vendor;
    pci_product_id_t product;
    pci_class_t class;
    pci_subclass_t subclass;
    pci_interface_t interface;
    pci_revision_t revision;
    const char *vendor_namep, *product_namep;
    struct pci_class *classp, *subclassp;
#if PCIVERBOSE
    const struct pci_knowndev2 *kdp;
    const char *unmatched = "unknown ";
#else
    const char *unmatched = "";
#endif

    vendor = PCI_VENDOR(id_reg);
    product = PCI_PRODUCT(id_reg);

    class = PCI_CLASS(class_reg);
    subclass = PCI_SUBCLASS(class_reg);
    interface = PCI_INTERFACE(class_reg);
    revision = PCI_REVISION(class_reg);

#if PCIVERBOSE
    kdp = pci_knowndevs;
    while (kdp->vendorname != PCI_STRING_NULL) {  /* all have vendor name */
        if (kdp->vendor == vendor && (kdp->product == product ||
            (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0))
            break;
        kdp++;
    }
    if (kdp->vendorname == PCI_STRING_NULL)
        vendor_namep = product_namep = NULL;
    else {
        vendor_namep = PCI_STRING(kdp->vendorname);
        product_namep = ((kdp->flags & PCI_KNOWNDEV_NOPROD) == 0 ?
			 PCI_STRING(kdp->productname) : NULL);
    }
#else /* PCIVERBOSE */
    vendor_namep = product_namep = NULL;
#endif /* PCIVERBOSE */

    classp = pci_class;
    while (classp->name != NULL) {
        if (class == classp->val)
            break;
        classp++;
    }

    subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
    while (subclassp && subclassp->name != NULL) {
        if (subclass == subclassp->val)
            break;
        subclassp++;
    }

    if (vendor_namep == NULL)
        cp += sprintf(cp, "%svendor 0x%04x product 0x%04x",
		      unmatched, vendor, product);
    else if (product_namep != NULL)
        cp += sprintf(cp, "%s %s", vendor_namep, product_namep);
    else
        cp += sprintf(cp, "%s product 0x%04x", vendor_namep, product);
    if (showclass) {
        cp += sprintf(cp, " (");
        if (classp->name == NULL)
            cp += sprintf(cp, "class 0x%02x, subclass 0x%02x",
			  class, subclass);
        else {
            if (subclassp == NULL || subclassp->name == NULL)
                cp += sprintf(cp, "%s subclass 0x%02x",
			      classp->name, subclass);
            else
                cp += sprintf(cp, "%s %s", subclassp->name, classp->name);
        }
        if (interface != 0)
            cp += sprintf(cp, ", interface 0x%02x", interface);
        if (revision != 0)
            cp += sprintf(cp, ", rev 0x%02x", revision);
        cp += sprintf(cp, ")");
    }
}


/*
 * Support routines for printing out PCI configuration registers.
 */

#define	i2o(i)	((i) * 4)
#define	o2i(o)	((o) / 4)
#define	onoff(str, bit)							\
    do {								\
        printf("      %s: %s\n", (str), (rval & (bit)) ? "on" : "off");	\
    } while (0)

#if PCIVERBOSE
static void
pci_conf_print_ids(pcireg_t rval, const char *prefix)
{
    const struct pci_knowndev2 *kdp;

    for (kdp = pci_knowndevs; kdp->vendorname != PCI_STRING_NULL; kdp++) {
        if (kdp->vendor == PCI_VENDOR(rval) &&
            (kdp->product == PCI_PRODUCT(rval) ||
            (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0)) {
            break;
        }
    }
    if (kdp->vendorname != PCI_STRING_NULL)
        printf("%sVendor Name: %s (0x%04x)\n", prefix,
	       PCI_STRING(kdp->vendorname), PCI_VENDOR(rval));
    else
        printf("%sVendor ID: 0x%04x\n", prefix, PCI_VENDOR(rval));
    if (kdp->productname != PCI_STRING_NULL
	&& (kdp->flags & PCI_KNOWNDEV_NOPROD) == 0)
        printf("%sDevice Name: %s (0x%04x)\n", prefix,
	       PCI_STRING(kdp->productname), PCI_PRODUCT(rval));
    else
        printf("%sDevice ID: 0x%04x\n", prefix, PCI_PRODUCT(rval));
}
#else
static void
pci_conf_print_ids(pcireg_t rval, const char *prefix)
{
    printf("%sVendor ID: 0x%04x\n", prefix, PCI_VENDOR(rval));
    printf("%sDevice ID: 0x%04x\n", prefix, PCI_PRODUCT(rval));
}
#endif /* PCIVERBOSE */

static int
pci_conf_print_bar(
    pcitag_t tag, const pcireg_t *regs,
    int reg, const char *name,
    int sizebar, int onlyimpl)
{
    int width;
    pcireg_t mask, rval;
    pcireg_t mask64h, rval64h;

#ifdef __GNUC__
    mask64h = rval64h = 0;
#endif

    width = 4;

    /*
     * Section 6.2.5.1, `Address Maps', tells us that:
     *
     * 1) The builtin software should have already mapped the
     * device in a reasonable way.
     *
     * 2) A device which wants 2^n bytes of memory will hardwire
     * the bottom n bits of the address to 0.  As recommended,
     * we write all 1s and see what we get back.
     */
    rval = regs[o2i(reg)];
    if (rval != 0 && sizebar) {
        uint32_t cmdreg;

        /*
         * The following sequence seems to make some devices
         * (e.g. host bus bridges, which don't normally
         * have their space mapped) very unhappy, to
         * the point of crashing the system.
         *
         * Therefore, if the mapping register is zero to
         * start out with, don't bother trying.
         */

        cmdreg = pci_conf_read(tag, PCI_COMMAND_STATUS_REG);
	cmdreg &= (PCI_COMMAND_MASK << PCI_COMMAND_SHIFT);  /* keep status */
        pci_conf_write(tag, PCI_COMMAND_STATUS_REG,
            cmdreg & ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE));

        pci_conf_write(tag, reg, 0xffffffff);
        mask = pci_conf_read(tag, reg);
        pci_conf_write(tag, reg, rval);
        if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM &&
            PCI_MAPREG_MEM_TYPE(rval) == PCI_MAPREG_MEM_TYPE_64BIT) {
            rval64h = regs[o2i(reg + 4)];
            pci_conf_write(tag, reg + 4, 0xffffffff);
            mask64h = pci_conf_read(tag, reg + 4);
            pci_conf_write(tag, reg + 4, rval64h);
            width = 8;
        }

        pci_conf_write(tag, PCI_COMMAND_STATUS_REG, cmdreg);
    } else
        mask = 0;

    if (rval == 0 && onlyimpl)
	return width;

    printf("    Base Address Register at 0x%02x", reg);
    if (name)
        printf(" (%s)", name);
    printf("\n      ");
    if (rval == 0) {
        printf("not implemented(?)\n");
        return width;
    } 
    printf("type: ");
    if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM) {
        const char *type, *prefetch;

        switch (PCI_MAPREG_MEM_TYPE(rval)) {
        case PCI_MAPREG_MEM_TYPE_32BIT:
            type = "32-bit";
            break;
        case PCI_MAPREG_MEM_TYPE_32BIT_1M:
            type = "32-bit-1M";
            break;
        case PCI_MAPREG_MEM_TYPE_64BIT:
            type = "64-bit";
            break;
        default:
            type = "unknown (XXX)";
            break;
        }
        if (PCI_MAPREG_MEM_PREFETCHABLE(rval))
            prefetch = "";
        else
            prefetch = "non";
        printf("%s %sprefetchable memory\n", type, prefetch);
        switch (PCI_MAPREG_MEM_TYPE(rval)) {
        case PCI_MAPREG_MEM_TYPE_64BIT:
            printf("      base: 0x%016llx, ",
		   PCI_MAPREG_MEM64_ADDR(
                       ((((long long) rval64h) << 32) | rval)));
            if (sizebar)
                printf("size: 0x%016llx",
		       PCI_MAPREG_MEM64_SIZE(
                           ((((long long) mask64h) << 32) | mask)));
            else
                printf("not sized");
            printf("\n");
            break;
        case PCI_MAPREG_MEM_TYPE_32BIT:
        case PCI_MAPREG_MEM_TYPE_32BIT_1M:
        default:
            printf("      base: 0x%08x, ", PCI_MAPREG_MEM_ADDR(rval));
            if (sizebar)
                printf("size: 0x%08x", PCI_MAPREG_MEM_SIZE(mask));
            else
                printf("not sized");
            printf("\n");
            break;
        }
    } else {
        if (sizebar)
            printf("%d-bit ", mask & ~0x0000ffff ? 32 : 16);
        printf("i/o\n");
        printf("      base: 0x%08x, ", PCI_MAPREG_IO_ADDR(rval));
        if (sizebar)
            printf("size: 0x%08x", PCI_MAPREG_IO_SIZE(mask));
        else
            printf("not sized");
        printf("\n");
    }

    return width;
}


/* Summary printing: Display device ID, status, memory map only. */

#define on(str, bit)                                                    \
    do { if (rval & (bit)) printf("    %s: on\n", (str)); } while (0)

void
pci_conf_print(pcitag_t tag)
{
    pcireg_t regs[o2i(256)];
    int off, width;
    pcireg_t rval;
    uint32_t base, limit;
    int sizebars;

    if (!pci_probe_tag(tag)) {
        printf("no device\n");
        return;
    }

    for (off = 0; off < 256; off += 4)
        regs[o2i(off)] = pci_conf_read(tag, off);

    rval = regs[o2i(PCI_ID_REG)];
    pci_conf_print_ids(rval, "  ");

    rval = regs[o2i(PCI_COMMAND_STATUS_REG)];

    printf("  Command: 0x%04x\n", rval & 0xffff);
    on("I/O space accesses", PCI_COMMAND_IO_ENABLE);
    on("Memory space accesses", PCI_COMMAND_MEM_ENABLE);
    on("Bus mastering", PCI_COMMAND_MASTER_ENABLE);

    printf("  Status: 0x%04x\n", PCI_STATUS(rval));
    on("Slave signaled Target Abort", PCI_STATUS_TARGET_TARGET_ABORT);
    on("Master received Target Abort", PCI_STATUS_MASTER_TARGET_ABORT);
    on("Master received Master Abort", PCI_STATUS_MASTER_ABORT);
    on("Asserted System Error (SERR)", PCI_STATUS_SYSTEM_ERROR);
    on("Parity error detected", PCI_STATUS_PARITY_DETECT);

    switch (PCI_HDRTYPE_TYPE(regs[o2i(PCI_BHLC_REG)])) {
    case 0:
        /* Standard device header */
        printf("  Type 0 (normal) header:\n");

	/* sizing host BARs is often bad news */
	sizebars = 1;
	if (PCI_CLASS(regs[o2i(PCI_CLASS_REG)]) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(regs[o2i(PCI_CLASS_REG)]) == PCI_SUBCLASS_BRIDGE_HOST)
	  sizebars = 0;
	for (off = PCI_MAPREG_START; off < PCI_MAPREG_END; off += width)
	  width = pci_conf_print_bar(tag, regs, off, NULL, sizebars, 1);

	rval = regs[o2i(PCI_BPARAM_INTERRUPT_REG)];
	printf("  Interrupt Line: 0x%02x\n", PCI_INTERRUPT_LINE(rval));
        break;

    case 1:
        /* PCI-PCI bridge header */
        printf("  Type 1 (PCI-PCI bridge) header:\n");

	rval = regs[o2i(PPB_BUSINFO_REG)];
	printf("    Buses:\n");
	printf("      Primary: %d,", PPB_BUSINFO_PRIMARY(rval));
	printf(" Secondary: %d,", PPB_BUSINFO_SECONDARY(rval));
	printf(" Subordinate: %d\n", PPB_BUSINFO_SUBORD(rval));

	rval = regs[o2i(PPB_IO_STATUS_REG)];
	printf("    Secondary Status: 0x%04x\n", PPB_SECSTATUS(rval));
	on("  Data parity error detected", PCI_STATUS_PARITY_ERROR);
	on("  Signaled Target Abort", PCI_STATUS_TARGET_TARGET_ABORT);
	on("  Received Target Abort", PCI_STATUS_MASTER_TARGET_ABORT);
	on("  Received Master Abort", PCI_STATUS_MASTER_ABORT);
	on("  System Error", PCI_STATUS_SYSTEM_ERROR);
	on("  Parity Error", PCI_STATUS_PARITY_DETECT);

	rval = regs[o2i(PPB_IO_STATUS_REG)];
	base  = PPB_IO_BASE(rval);
	limit = PPB_IO_LIMIT(rval);
	if (base != 0 || limit != 0) {
	    printf("    I/O Range:\n");

	    if ((base & 0xf) != 0 || (limit & 0xf) != 0) {
		base =  ((base & 0xf0)  << 8) | 0x000;	  
		limit = ((limit & 0xf0) << 8) | 0xfff;
		rval = regs[o2i(PPB_IO_UPPER_REG)];
		base |= PPB_BASE(rval) << 16;
	        limit |= PPB_LIMIT(rval) << 16;
		printf("      base: 0x%08x, limit: 0x%08x\n", base, limit);
	    } else {
		base = (base << 8) | 0x000;
		limit = (limit << 8) | 0xfff;
		printf("      base: 0x%04x, limit: 0x%04x\n", base, limit);
	    }
	}

	base  = PPB_BASE(regs[o2i(PPB_MEM_REG)])  & 0xfff0;
	limit = PPB_LIMIT(regs[o2i(PPB_MEM_REG)]) & 0xfff0;
	printf("    Memory Range:\n");
	base = (base << 16) | 0x00000;
	limit = (limit << 16) | 0xfffff;
	printf("      base: 0x%08x, limit: 0x%08x\n", base, limit);

	base  = PPB_BASE(regs[o2i(PPB_PREFMEM_REG)])  & 0xffff;
	limit = PPB_LIMIT(regs[o2i(PPB_PREFMEM_REG)]) & 0xffff;
	if (base != 0 || limit != 0
	    || regs[o2i(PPB_PREFMEM_BASE_UPPER_REG)] != 0
	    || regs[o2i(PPB_PREFMEM_LIMIT_UPPER_REG)] != 0) {
	    printf("    Prefetchable Memory Range:\n");
	    if ((base & 0xf) != 0 || (limit & 0xf) != 0) {
		base = ((base & 0xfff0) << 16) | 0x00000;
		limit = ((limit & 0xfff0) << 16) | 0xfffff;
		printf("      base: 0x%08x%08x, limit: 0x%08x%08x\n",
		       regs[o2i(PPB_PREFMEM_BASE_UPPER_REG)], base,
		       regs[o2i(PPB_PREFMEM_LIMIT_UPPER_REG)], limit);
	    } else {
		base = (base << 16) | 0x00000;
		limit = (limit << 16) | 0xfffff;
		printf("      base: 0x%08x, limit: 0x%08x\n", base, limit);
	    }
	}

	if (regs[o2i(PPB_MAPREG_ROM)] != 0)
	    printf("    Expansion ROM Base Address: 0x%08x\n",
		   regs[o2i(PPB_MAPREG_ROM)]);
        break;

    default:
        break;
    }
}
