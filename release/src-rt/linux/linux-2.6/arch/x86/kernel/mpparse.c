/*
 *	Intel Multiprocessor Specification 1.1 and 1.4
 *	compliant MP-table parsing routines.
 *
 *	(c) 1995 Alan Cox, Building #3 <alan@lxorguk.ukuu.org.uk>
 *	(c) 1998, 1999, 2000, 2009 Ingo Molnar <mingo@redhat.com>
 *      (c) 2008 Alexey Starikovskiy <astarikovskiy@suse.de>
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/kernel_stat.h>
#include <linux/mc146818rtc.h>
#include <linux/bitops.h>
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/pci.h>

#include <asm/mtrr.h>
#include <asm/mpspec.h>
#include <asm/pgalloc.h>
#include <asm/io_apic.h>
#include <asm/proto.h>
#include <asm/bios_ebda.h>
#include <asm/e820.h>
#include <asm/trampoline.h>
#include <asm/setup.h>
#include <asm/smp.h>

#include <asm/apic.h>
/*
 * Checksum an MP configuration block.
 */

static int __init mpf_checksum(unsigned char *mp, int len)
{
	int sum = 0;

	while (len--)
		sum += *mp++;

	return sum & 0xFF;
}

int __init default_mpc_apic_id(struct mpc_cpu *m)
{
	return m->apicid;
}

static void __init MP_processor_info(struct mpc_cpu *m)
{
	int apicid;
	char *bootup_cpu = "";

	if (!(m->cpuflag & CPU_ENABLED)) {
		disabled_cpus++;
		return;
	}

	apicid = x86_init.mpparse.mpc_apic_id(m);

	if (m->cpuflag & CPU_BOOTPROCESSOR) {
		bootup_cpu = " (Bootup-CPU)";
		boot_cpu_physical_apicid = m->apicid;
	}

	printk(KERN_INFO "Processor #%d%s\n", m->apicid, bootup_cpu);
	generic_processor_info(apicid, m->apicver);
}

#ifdef CONFIG_X86_IO_APIC
void __init default_mpc_oem_bus_info(struct mpc_bus *m, char *str)
{
	memcpy(str, m->bustype, 6);
	str[6] = 0;
	apic_printk(APIC_VERBOSE, "Bus #%d is %s\n", m->busid, str);
}

static void __init MP_bus_info(struct mpc_bus *m)
{
	char str[7];

	x86_init.mpparse.mpc_oem_bus_info(m, str);

#if MAX_MP_BUSSES < 256
	if (m->busid >= MAX_MP_BUSSES) {
		printk(KERN_WARNING "MP table busid value (%d) for bustype %s "
		       " is too large, max. supported is %d\n",
		       m->busid, str, MAX_MP_BUSSES - 1);
		return;
	}
#endif

	if (strncmp(str, BUSTYPE_ISA, sizeof(BUSTYPE_ISA) - 1) == 0) {
		set_bit(m->busid, mp_bus_not_pci);
#if defined(CONFIG_EISA) || defined(CONFIG_MCA)
		mp_bus_id_to_type[m->busid] = MP_BUS_ISA;
#endif
	} else if (strncmp(str, BUSTYPE_PCI, sizeof(BUSTYPE_PCI) - 1) == 0) {
		if (x86_init.mpparse.mpc_oem_pci_bus)
			x86_init.mpparse.mpc_oem_pci_bus(m);

		clear_bit(m->busid, mp_bus_not_pci);
#if defined(CONFIG_EISA) || defined(CONFIG_MCA)
		mp_bus_id_to_type[m->busid] = MP_BUS_PCI;
	} else if (strncmp(str, BUSTYPE_EISA, sizeof(BUSTYPE_EISA) - 1) == 0) {
		mp_bus_id_to_type[m->busid] = MP_BUS_EISA;
	} else if (strncmp(str, BUSTYPE_MCA, sizeof(BUSTYPE_MCA) - 1) == 0) {
		mp_bus_id_to_type[m->busid] = MP_BUS_MCA;
#endif
	} else
		printk(KERN_WARNING "Unknown bustype %s - ignoring\n", str);
}

static void __init MP_ioapic_info(struct mpc_ioapic *m)
{
	if (m->flags & MPC_APIC_USABLE)
		mp_register_ioapic(m->apicid, m->apicaddr, gsi_top);
}

static void __init print_mp_irq_info(struct mpc_intsrc *mp_irq)
{
	apic_printk(APIC_VERBOSE, "Int: type %d, pol %d, trig %d, bus %02x,"
		" IRQ %02x, APIC ID %x, APIC INT %02x\n",
		mp_irq->irqtype, mp_irq->irqflag & 3,
		(mp_irq->irqflag >> 2) & 3, mp_irq->srcbus,
		mp_irq->srcbusirq, mp_irq->dstapic, mp_irq->dstirq);
}

#else /* CONFIG_X86_IO_APIC */
static inline void __init MP_bus_info(struct mpc_bus *m) {}
static inline void __init MP_ioapic_info(struct mpc_ioapic *m) {}
#endif /* CONFIG_X86_IO_APIC */

static void __init MP_lintsrc_info(struct mpc_lintsrc *m)
{
	apic_printk(APIC_VERBOSE, "Lint: type %d, pol %d, trig %d, bus %02x,"
		" IRQ %02x, APIC ID %x, APIC LINT %02x\n",
		m->irqtype, m->irqflag & 3, (m->irqflag >> 2) & 3, m->srcbusid,
		m->srcbusirq, m->destapic, m->destapiclint);
}

/*
 * Read/parse the MPC
 */
static int __init smp_check_mpc(struct mpc_table *mpc, char *oem, char *str)
{

	if (memcmp(mpc->signature, MPC_SIGNATURE, 4)) {
		printk(KERN_ERR "MPTABLE: bad signature [%c%c%c%c]!\n",
		       mpc->signature[0], mpc->signature[1],
		       mpc->signature[2], mpc->signature[3]);
		return 0;
	}
	if (mpf_checksum((unsigned char *)mpc, mpc->length)) {
		printk(KERN_ERR "MPTABLE: checksum error!\n");
		return 0;
	}
	if (mpc->spec != 0x01 && mpc->spec != 0x04) {
		printk(KERN_ERR "MPTABLE: bad table version (%d)!!\n",
		       mpc->spec);
		return 0;
	}
	if (!mpc->lapic) {
		printk(KERN_ERR "MPTABLE: null local APIC address!\n");
		return 0;
	}
	memcpy(oem, mpc->oem, 8);
	oem[8] = 0;
	printk(KERN_INFO "MPTABLE: OEM ID: %s\n", oem);

	memcpy(str, mpc->productid, 12);
	str[12] = 0;

	printk(KERN_INFO "MPTABLE: Product ID: %s\n", str);

	printk(KERN_INFO "MPTABLE: APIC at: 0x%X\n", mpc->lapic);

	return 1;
}

static void skip_entry(unsigned char **ptr, int *count, int size)
{
	*ptr += size;
	*count += size;
}

static void __init smp_dump_mptable(struct mpc_table *mpc, unsigned char *mpt)
{
	printk(KERN_ERR "Your mptable is wrong, contact your HW vendor!\n"
		"type %x\n", *mpt);
	print_hex_dump(KERN_ERR, "  ", DUMP_PREFIX_ADDRESS, 16,
			1, mpc, mpc->length, 1);
}

void __init default_smp_read_mpc_oem(struct mpc_table *mpc) { }

static int __init smp_read_mpc(struct mpc_table *mpc, unsigned early)
{
	char str[16];
	char oem[10];

	int count = sizeof(*mpc);
	unsigned char *mpt = ((unsigned char *)mpc) + count;

	if (!smp_check_mpc(mpc, oem, str))
		return 0;

#ifdef CONFIG_X86_32
	generic_mps_oem_check(mpc, oem, str);
#endif
	/* Initialize the lapic mapping */
	if (!acpi_lapic)
		register_lapic_address(mpc->lapic);

	if (early)
		return 1;

	if (mpc->oemptr)
		x86_init.mpparse.smp_read_mpc_oem(mpc);

	/*
	 *      Now process the configuration blocks.
	 */
	x86_init.mpparse.mpc_record(0);

	while (count < mpc->length) {
		switch (*mpt) {
		case MP_PROCESSOR:
			/* ACPI may have already provided this data */
			if (!acpi_lapic)
				MP_processor_info((struct mpc_cpu *)mpt);
			skip_entry(&mpt, &count, sizeof(struct mpc_cpu));
			break;
		case MP_BUS:
			MP_bus_info((struct mpc_bus *)mpt);
			skip_entry(&mpt, &count, sizeof(struct mpc_bus));
			break;
		case MP_IOAPIC:
			MP_ioapic_info((struct mpc_ioapic *)mpt);
			skip_entry(&mpt, &count, sizeof(struct mpc_ioapic));
			break;
		case MP_INTSRC:
			mp_save_irq((struct mpc_intsrc *)mpt);
			skip_entry(&mpt, &count, sizeof(struct mpc_intsrc));
			break;
		case MP_LINTSRC:
			MP_lintsrc_info((struct mpc_lintsrc *)mpt);
			skip_entry(&mpt, &count, sizeof(struct mpc_lintsrc));
			break;
		default:
			/* wrong mptable */
			smp_dump_mptable(mpc, mpt);
			count = mpc->length;
			break;
		}
		x86_init.mpparse.mpc_record(1);
	}

	if (!num_processors)
		printk(KERN_ERR "MPTABLE: no processors registered!\n");
	return num_processors;
}

#ifdef CONFIG_X86_IO_APIC

static int __init ELCR_trigger(unsigned int irq)
{
	unsigned int port;

	port = 0x4d0 + (irq >> 3);
	return (inb(port) >> (irq & 7)) & 1;
}

static void __init construct_default_ioirq_mptable(int mpc_default_type)
{
	struct mpc_intsrc intsrc;
	int i;
	int ELCR_fallback = 0;

	intsrc.type = MP_INTSRC;
	intsrc.irqflag = 0;	/* conforming */
	intsrc.srcbus = 0;
	intsrc.dstapic = mp_ioapics[0].apicid;

	intsrc.irqtype = mp_INT;

	/*
	 *  If true, we have an ISA/PCI system with no IRQ entries
	 *  in the MP table. To prevent the PCI interrupts from being set up
	 *  incorrectly, we try to use the ELCR. The sanity check to see if
	 *  there is good ELCR data is very simple - IRQ0, 1, 2 and 13 can
	 *  never be level sensitive, so we simply see if the ELCR agrees.
	 *  If it does, we assume it's valid.
	 */
	if (mpc_default_type == 5) {
		printk(KERN_INFO "ISA/PCI bus type with no IRQ information... "
		       "falling back to ELCR\n");

		if (ELCR_trigger(0) || ELCR_trigger(1) || ELCR_trigger(2) ||
		    ELCR_trigger(13))
			printk(KERN_ERR "ELCR contains invalid data... "
			       "not using ELCR\n");
		else {
			printk(KERN_INFO
			       "Using ELCR to identify PCI interrupts\n");
			ELCR_fallback = 1;
		}
	}

	for (i = 0; i < 16; i++) {
		switch (mpc_default_type) {
		case 2:
			if (i == 0 || i == 13)
				continue;	/* IRQ0 & IRQ13 not connected */
			/* fall through */
		default:
			if (i == 2)
				continue;	/* IRQ2 is never connected */
		}

		if (ELCR_fallback) {
			/*
			 *  If the ELCR indicates a level-sensitive interrupt, we
			 *  copy that information over to the MP table in the
			 *  irqflag field (level sensitive, active high polarity).
			 */
			if (ELCR_trigger(i))
				intsrc.irqflag = 13;
			else
				intsrc.irqflag = 0;
		}

		intsrc.srcbusirq = i;
		intsrc.dstirq = i ? i : 2;	/* IRQ0 to INTIN2 */
		mp_save_irq(&intsrc);
	}

	intsrc.irqtype = mp_ExtINT;
	intsrc.srcbusirq = 0;
	intsrc.dstirq = 0;	/* 8259A to INTIN0 */
	mp_save_irq(&intsrc);
}


static void __init construct_ioapic_table(int mpc_default_type)
{
	struct mpc_ioapic ioapic;
	struct mpc_bus bus;

	bus.type = MP_BUS;
	bus.busid = 0;
	switch (mpc_default_type) {
	default:
		printk(KERN_ERR "???\nUnknown standard configuration %d\n",
		       mpc_default_type);
		/* fall through */
	case 1:
	case 5:
		memcpy(bus.bustype, "ISA   ", 6);
		break;
	case 2:
	case 6:
	case 3:
		memcpy(bus.bustype, "EISA  ", 6);
		break;
	case 4:
	case 7:
		memcpy(bus.bustype, "MCA   ", 6);
	}
	MP_bus_info(&bus);
	if (mpc_default_type > 4) {
		bus.busid = 1;
		memcpy(bus.bustype, "PCI   ", 6);
		MP_bus_info(&bus);
	}

	ioapic.type	= MP_IOAPIC;
	ioapic.apicid	= 2;
	ioapic.apicver	= mpc_default_type > 4 ? 0x10 : 0x01;
	ioapic.flags	= MPC_APIC_USABLE;
	ioapic.apicaddr	= IO_APIC_DEFAULT_PHYS_BASE;
	MP_ioapic_info(&ioapic);

	/*
	 * We set up most of the low 16 IO-APIC pins according to MPS rules.
	 */
	construct_default_ioirq_mptable(mpc_default_type);
}
#else
static inline void __init construct_ioapic_table(int mpc_default_type) { }
#endif

static inline void __init construct_default_ISA_mptable(int mpc_default_type)
{
	struct mpc_cpu processor;
	struct mpc_lintsrc lintsrc;
	int linttypes[2] = { mp_ExtINT, mp_NMI };
	int i;

	/*
	 * local APIC has default address
	 */
	mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;

	/*
	 * 2 CPUs, numbered 0 & 1.
	 */
	processor.type = MP_PROCESSOR;
	/* Either an integrated APIC or a discrete 82489DX. */
	processor.apicver = mpc_default_type > 4 ? 0x10 : 0x01;
	processor.cpuflag = CPU_ENABLED;
	processor.cpufeature = (boot_cpu_data.x86 << 8) |
	    (boot_cpu_data.x86_model << 4) | boot_cpu_data.x86_mask;
	processor.featureflag = boot_cpu_data.x86_capability[0];
	processor.reserved[0] = 0;
	processor.reserved[1] = 0;
	for (i = 0; i < 2; i++) {
		processor.apicid = i;
		MP_processor_info(&processor);
	}

	construct_ioapic_table(mpc_default_type);

	lintsrc.type = MP_LINTSRC;
	lintsrc.irqflag = 0;		/* conforming */
	lintsrc.srcbusid = 0;
	lintsrc.srcbusirq = 0;
	lintsrc.destapic = MP_APIC_ALL;
	for (i = 0; i < 2; i++) {
		lintsrc.irqtype = linttypes[i];
		lintsrc.destapiclint = i;
		MP_lintsrc_info(&lintsrc);
	}
}

static struct mpf_intel *mpf_found;

static unsigned long __init get_mpc_size(unsigned long physptr)
{
	struct mpc_table *mpc;
	unsigned long size;

	mpc = early_ioremap(physptr, PAGE_SIZE);
	size = mpc->length;
	early_iounmap(mpc, PAGE_SIZE);
	apic_printk(APIC_VERBOSE, "  mpc: %lx-%lx\n", physptr, physptr + size);

	return size;
}

static int __init check_physptr(struct mpf_intel *mpf, unsigned int early)
{
	struct mpc_table *mpc;
	unsigned long size;

	size = get_mpc_size(mpf->physptr);
	mpc = early_ioremap(mpf->physptr, size);
	/*
	 * Read the physical hardware table.  Anything here will
	 * override the defaults.
	 */
	if (!smp_read_mpc(mpc, early)) {
#ifdef CONFIG_X86_LOCAL_APIC
		smp_found_config = 0;
#endif
		printk(KERN_ERR "BIOS bug, MP table errors detected!...\n"
			"... disabling SMP support. (tell your hw vendor)\n");
		early_iounmap(mpc, size);
		return -1;
	}
	early_iounmap(mpc, size);

	if (early)
		return -1;

#ifdef CONFIG_X86_IO_APIC
	/*
	 * If there are no explicit MP IRQ entries, then we are
	 * broken.  We set up most of the low 16 IO-APIC pins to
	 * ISA defaults and hope it will work.
	 */
	if (!mp_irq_entries) {
		struct mpc_bus bus;

		printk(KERN_ERR "BIOS bug, no explicit IRQ entries, "
		       "using default mptable. (tell your hw vendor)\n");

		bus.type = MP_BUS;
		bus.busid = 0;
		memcpy(bus.bustype, "ISA   ", 6);
		MP_bus_info(&bus);

		construct_default_ioirq_mptable(0);
	}
#endif

	return 0;
}

/*
 * Scan the memory blocks for an SMP configuration block.
 */
void __init default_get_smp_config(unsigned int early)
{
	struct mpf_intel *mpf = mpf_found;

	if (!mpf)
		return;

	if (acpi_lapic && early)
		return;

	/*
	 * MPS doesn't support hyperthreading, aka only have
	 * thread 0 apic id in MPS table
	 */
	if (acpi_lapic && acpi_ioapic)
		return;

	printk(KERN_INFO "Intel MultiProcessor Specification v1.%d\n",
	       mpf->specification);
#if defined(CONFIG_X86_LOCAL_APIC) && defined(CONFIG_X86_32)
	if (mpf->feature2 & (1 << 7)) {
		printk(KERN_INFO "    IMCR and PIC compatibility mode.\n");
		pic_mode = 1;
	} else {
		printk(KERN_INFO "    Virtual Wire compatibility mode.\n");
		pic_mode = 0;
	}
#endif
	/*
	 * Now see if we need to read further.
	 */
	if (mpf->feature1 != 0) {
		if (early) {
			/*
			 * local APIC has default address
			 */
			mp_lapic_addr = APIC_DEFAULT_PHYS_BASE;
			return;
		}

		printk(KERN_INFO "Default MP configuration #%d\n",
		       mpf->feature1);
		construct_default_ISA_mptable(mpf->feature1);

	} else if (mpf->physptr) {
		if (check_physptr(mpf, early))
			return;
	} else
		BUG();

	if (!early)
		printk(KERN_INFO "Processors: %d\n", num_processors);
	/*
	 * Only use the first configuration found.
	 */
}

static void __init smp_reserve_memory(struct mpf_intel *mpf)
{
	unsigned long size = get_mpc_size(mpf->physptr);

	memblock_x86_reserve_range(mpf->physptr, mpf->physptr+size, "* MP-table mpc");
}

static int __init smp_scan_config(unsigned long base, unsigned long length)
{
	unsigned int *bp = phys_to_virt(base);
	struct mpf_intel *mpf;
	unsigned long mem;

	apic_printk(APIC_VERBOSE, "Scan SMP from %p for %ld bytes.\n",
			bp, length);
	BUILD_BUG_ON(sizeof(*mpf) != 16);

	while (length > 0) {
		mpf = (struct mpf_intel *)bp;
		if ((*bp == SMP_MAGIC_IDENT) &&
		    (mpf->length == 1) &&
		    !mpf_checksum((unsigned char *)bp, 16) &&
		    ((mpf->specification == 1)
		     || (mpf->specification == 4))) {
#ifdef CONFIG_X86_LOCAL_APIC
			smp_found_config = 1;
#endif
			mpf_found = mpf;

			printk(KERN_INFO "found SMP MP-table at [%p] %llx\n",
			       mpf, (u64)virt_to_phys(mpf));

			mem = virt_to_phys(mpf);
			memblock_x86_reserve_range(mem, mem + sizeof(*mpf), "* MP-table mpf");
			if (mpf->physptr)
				smp_reserve_memory(mpf);

			return 1;
		}
		bp += 4;
		length -= 16;
	}
	return 0;
}

void __init default_find_smp_config(void)
{
	unsigned int address;

	/*
	 * FIXME: Linux assumes you have 640K of base ram..
	 * this continues the error...
	 *
	 * 1) Scan the bottom 1K for a signature
	 * 2) Scan the top 1K of base RAM
	 * 3) Scan the 64K of bios
	 */
	if (smp_scan_config(0x0, 0x400) ||
	    smp_scan_config(639 * 0x400, 0x400) ||
	    smp_scan_config(0xF0000, 0x10000))
		return;
	/*
	 * If it is an SMP machine we should know now, unless the
	 * configuration is in an EISA/MCA bus machine with an
	 * extended bios data area.
	 *
	 * there is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E, calculate and scan it here.
	 *
	 * NOTE! There are Linux loaders that will corrupt the EBDA
	 * area, and as such this kind of SMP config may be less
	 * trustworthy, simply because the SMP table may have been
	 * stomped on during early boot. These loaders are buggy and
	 * should be fixed.
	 *
	 * MP1.4 SPEC states to only scan first 1K of 4K EBDA.
	 */

	address = get_bios_ebda();
	if (address)
		smp_scan_config(address, 0x400);
}

#ifdef CONFIG_X86_IO_APIC
static u8 __initdata irq_used[MAX_IRQ_SOURCES];

static int  __init get_MP_intsrc_index(struct mpc_intsrc *m)
{
	int i;

	if (m->irqtype != mp_INT)
		return 0;

	if (m->irqflag != 0x0f)
		return 0;

	/* not legacy */

	for (i = 0; i < mp_irq_entries; i++) {
		if (mp_irqs[i].irqtype != mp_INT)
			continue;

		if (mp_irqs[i].irqflag != 0x0f)
			continue;

		if (mp_irqs[i].srcbus != m->srcbus)
			continue;
		if (mp_irqs[i].srcbusirq != m->srcbusirq)
			continue;
		if (irq_used[i]) {
			/* already claimed */
			return -2;
		}
		irq_used[i] = 1;
		return i;
	}

	/* not found */
	return -1;
}

#define SPARE_SLOT_NUM 20

static struct mpc_intsrc __initdata *m_spare[SPARE_SLOT_NUM];

static void __init check_irq_src(struct mpc_intsrc *m, int *nr_m_spare)
{
	int i;

	apic_printk(APIC_VERBOSE, "OLD ");
	print_mp_irq_info(m);

	i = get_MP_intsrc_index(m);
	if (i > 0) {
		memcpy(m, &mp_irqs[i], sizeof(*m));
		apic_printk(APIC_VERBOSE, "NEW ");
		print_mp_irq_info(&mp_irqs[i]);
		return;
	}
	if (!i) {
		/* legacy, do nothing */
		return;
	}
	if (*nr_m_spare < SPARE_SLOT_NUM) {
		/*
		 * not found (-1), or duplicated (-2) are invalid entries,
		 * we need to use the slot later
		 */
		m_spare[*nr_m_spare] = m;
		*nr_m_spare += 1;
	}
}

static int
check_slot(unsigned long mpc_new_phys, unsigned long mpc_new_length, int count)
{
	int ret = 0;

	if (!mpc_new_phys || count <= mpc_new_length) {
		WARN(1, "update_mptable: No spare slots (length: %x)\n", count);
		return -1;
	}

	return ret;
}
#else /* CONFIG_X86_IO_APIC */
static
inline void __init check_irq_src(struct mpc_intsrc *m, int *nr_m_spare) {}
#endif /* CONFIG_X86_IO_APIC */

static int  __init replace_intsrc_all(struct mpc_table *mpc,
					unsigned long mpc_new_phys,
					unsigned long mpc_new_length)
{
#ifdef CONFIG_X86_IO_APIC
	int i;
#endif
	int count = sizeof(*mpc);
	int nr_m_spare = 0;
	unsigned char *mpt = ((unsigned char *)mpc) + count;

	printk(KERN_INFO "mpc_length %x\n", mpc->length);
	while (count < mpc->length) {
		switch (*mpt) {
		case MP_PROCESSOR:
			skip_entry(&mpt, &count, sizeof(struct mpc_cpu));
			break;
		case MP_BUS:
			skip_entry(&mpt, &count, sizeof(struct mpc_bus));
			break;
		case MP_IOAPIC:
			skip_entry(&mpt, &count, sizeof(struct mpc_ioapic));
			break;
		case MP_INTSRC:
			check_irq_src((struct mpc_intsrc *)mpt, &nr_m_spare);
			skip_entry(&mpt, &count, sizeof(struct mpc_intsrc));
			break;
		case MP_LINTSRC:
			skip_entry(&mpt, &count, sizeof(struct mpc_lintsrc));
			break;
		default:
			/* wrong mptable */
			smp_dump_mptable(mpc, mpt);
			goto out;
		}
	}

#ifdef CONFIG_X86_IO_APIC
	for (i = 0; i < mp_irq_entries; i++) {
		if (irq_used[i])
			continue;

		if (mp_irqs[i].irqtype != mp_INT)
			continue;

		if (mp_irqs[i].irqflag != 0x0f)
			continue;

		if (nr_m_spare > 0) {
			apic_printk(APIC_VERBOSE, "*NEW* found\n");
			nr_m_spare--;
			memcpy(m_spare[nr_m_spare], &mp_irqs[i], sizeof(mp_irqs[i]));
			m_spare[nr_m_spare] = NULL;
		} else {
			struct mpc_intsrc *m = (struct mpc_intsrc *)mpt;
			count += sizeof(struct mpc_intsrc);
			if (check_slot(mpc_new_phys, mpc_new_length, count) < 0)
				goto out;
			memcpy(m, &mp_irqs[i], sizeof(*m));
			mpc->length = count;
			mpt += sizeof(struct mpc_intsrc);
		}
		print_mp_irq_info(&mp_irqs[i]);
	}
#endif
out:
	/* update checksum */
	mpc->checksum = 0;
	mpc->checksum -= mpf_checksum((unsigned char *)mpc, mpc->length);

	return 0;
}

int enable_update_mptable;

static int __init update_mptable_setup(char *str)
{
	enable_update_mptable = 1;
#ifdef CONFIG_PCI
	pci_routeirq = 1;
#endif
	return 0;
}
early_param("update_mptable", update_mptable_setup);

static unsigned long __initdata mpc_new_phys;
static unsigned long mpc_new_length __initdata = 4096;

/* alloc_mptable or alloc_mptable=4k */
static int __initdata alloc_mptable;
static int __init parse_alloc_mptable_opt(char *p)
{
	enable_update_mptable = 1;
#ifdef CONFIG_PCI
	pci_routeirq = 1;
#endif
	alloc_mptable = 1;
	if (!p)
		return 0;
	mpc_new_length = memparse(p, &p);
	return 0;
}
early_param("alloc_mptable", parse_alloc_mptable_opt);

void __init early_reserve_e820_mpc_new(void)
{
	if (enable_update_mptable && alloc_mptable) {
		u64 startt = 0;
		mpc_new_phys = early_reserve_e820(startt, mpc_new_length, 4);
	}
}

static int __init update_mp_table(void)
{
	char str[16];
	char oem[10];
	struct mpf_intel *mpf;
	struct mpc_table *mpc, *mpc_new;

	if (!enable_update_mptable)
		return 0;

	mpf = mpf_found;
	if (!mpf)
		return 0;

	/*
	 * Now see if we need to go further.
	 */
	if (mpf->feature1 != 0)
		return 0;

	if (!mpf->physptr)
		return 0;

	mpc = phys_to_virt(mpf->physptr);

	if (!smp_check_mpc(mpc, oem, str))
		return 0;

	printk(KERN_INFO "mpf: %llx\n", (u64)virt_to_phys(mpf));
	printk(KERN_INFO "physptr: %x\n", mpf->physptr);

	if (mpc_new_phys && mpc->length > mpc_new_length) {
		mpc_new_phys = 0;
		printk(KERN_INFO "mpc_new_length is %ld, please use alloc_mptable=8k\n",
			 mpc_new_length);
	}

	if (!mpc_new_phys) {
		unsigned char old, new;
		/* check if we can change the position */
		mpc->checksum = 0;
		old = mpf_checksum((unsigned char *)mpc, mpc->length);
		mpc->checksum = 0xff;
		new = mpf_checksum((unsigned char *)mpc, mpc->length);
		if (old == new) {
			printk(KERN_INFO "mpc is readonly, please try alloc_mptable instead\n");
			return 0;
		}
		printk(KERN_INFO "use in-position replacing\n");
	} else {
		mpf->physptr = mpc_new_phys;
		mpc_new = phys_to_virt(mpc_new_phys);
		memcpy(mpc_new, mpc, mpc->length);
		mpc = mpc_new;
		/* check if we can modify that */
		if (mpc_new_phys - mpf->physptr) {
			struct mpf_intel *mpf_new;
			/* steal 16 bytes from [0, 1k) */
			printk(KERN_INFO "mpf new: %x\n", 0x400 - 16);
			mpf_new = phys_to_virt(0x400 - 16);
			memcpy(mpf_new, mpf, 16);
			mpf = mpf_new;
			mpf->physptr = mpc_new_phys;
		}
		mpf->checksum = 0;
		mpf->checksum -= mpf_checksum((unsigned char *)mpf, 16);
		printk(KERN_INFO "physptr new: %x\n", mpf->physptr);
	}

	/*
	 * only replace the one with mp_INT and
	 *	 MP_IRQ_TRIGGER_LEVEL|MP_IRQ_POLARITY_LOW,
	 * already in mp_irqs , stored by ... and mp_config_acpi_gsi,
	 * may need pci=routeirq for all coverage
	 */
	replace_intsrc_all(mpc, mpc_new_phys, mpc_new_length);

	return 0;
}

late_initcall(update_mp_table);
