/*
 *  (C) 2001-2004  Dave Jones. <davej@codemonkey.org.uk>
 *  (C) 2002  Padraig Brady. <padraig@antefacto.com>
 *
 *  Licensed under the terms of the GNU GPL License version 2.
 *  Based upon datasheets & sample CPUs kindly provided by VIA.
 *
 *  VIA have currently 3 different versions of Longhaul.
 *  Version 1 (Longhaul) uses the BCR2 MSR at 0x1147.
 *   It is present only in Samuel 1 (C5A), Samuel 2 (C5B) stepping 0.
 *  Version 2 of longhaul is backward compatible with v1, but adds
 *   LONGHAUL MSR for purpose of both frequency and voltage scaling.
 *   Present in Samuel 2 (steppings 1-7 only) (C5B), and Ezra (C5C).
 *  Version 3 of longhaul got renamed to Powersaver and redesigned
 *   to use only the POWERSAVER MSR at 0x110a.
 *   It is present in Ezra-T (C5M), Nehemiah (C5X) and above.
 *   It's pretty much the same feature wise to longhaul v2, though
 *   there is provision for scaling FSB too, but this doesn't work
 *   too well in practice so we don't even try to use this.
 *
 *  BIG FAT DISCLAIMER: Work in progress code. Possibly *dangerous*
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <asm/msr.h>
#include <asm/timex.h>
#include <asm/io.h>
#include <asm/acpi.h>
#include <linux/acpi.h>
#include <acpi/processor.h>

#include "longhaul.h"

#define PFX "longhaul: "

#define TYPE_LONGHAUL_V1	1
#define TYPE_LONGHAUL_V2	2
#define TYPE_POWERSAVER		3

#define	CPU_SAMUEL	1
#define	CPU_SAMUEL2	2
#define	CPU_EZRA	3
#define	CPU_EZRA_T	4
#define	CPU_NEHEMIAH	5
#define	CPU_NEHEMIAH_C	6

/* Flags */
#define USE_ACPI_C3		(1 << 1)
#define USE_NORTHBRIDGE		(1 << 2)
#define USE_VT8235		(1 << 3)

static int cpu_model;
static unsigned int numscales=16;
static unsigned int fsb;

static const struct mV_pos *vrm_mV_table;
static const unsigned char *mV_vrm_table;
struct f_msr {
	u8 vrm;
	u8 pos;
};
static struct f_msr f_msr_table[32];

static unsigned int highest_speed, lowest_speed; /* kHz */
static unsigned int minmult, maxmult;
static int can_scale_voltage;
static struct acpi_processor *pr = NULL;
static struct acpi_processor_cx *cx = NULL;
static u8 longhaul_flags;
static u8 longhaul_pos;

/* Module parameters */
static int scale_voltage;

#define dprintk(msg...) cpufreq_debug_printk(CPUFREQ_DEBUG_DRIVER, "longhaul", msg)


/* Clock ratios multiplied by 10 */
static int clock_ratio[32];
static int eblcr_table[32];
static int longhaul_version;
static struct cpufreq_frequency_table *longhaul_table;

#ifdef CONFIG_CPU_FREQ_DEBUG
static char speedbuffer[8];

static char *print_speed(int speed)
{
	if (speed < 1000) {
		snprintf(speedbuffer, sizeof(speedbuffer),"%dMHz", speed);
		return speedbuffer;
	}

	if (speed%1000 == 0)
		snprintf(speedbuffer, sizeof(speedbuffer),
			"%dGHz", speed/1000);
	else
		snprintf(speedbuffer, sizeof(speedbuffer),
			"%d.%dGHz", speed/1000, (speed%1000)/100);

	return speedbuffer;
}
#endif


static unsigned int calc_speed(int mult)
{
	int khz;
	khz = (mult/10)*fsb;
	if (mult%10)
		khz += fsb/2;
	khz *= 1000;
	return khz;
}


static int longhaul_get_cpu_mult(void)
{
	unsigned long invalue=0,lo, hi;

	rdmsr (MSR_IA32_EBL_CR_POWERON, lo, hi);
	invalue = (lo & (1<<22|1<<23|1<<24|1<<25)) >>22;
	if (longhaul_version==TYPE_LONGHAUL_V2 || longhaul_version==TYPE_POWERSAVER) {
		if (lo & (1<<27))
			invalue+=16;
	}
	return eblcr_table[invalue];
}

/* For processor with BCR2 MSR */

static void do_longhaul1(unsigned int clock_ratio_index)
{
	union msr_bcr2 bcr2;

	rdmsrl(MSR_VIA_BCR2, bcr2.val);
	/* Enable software clock multiplier */
	bcr2.bits.ESOFTBF = 1;
	bcr2.bits.CLOCKMUL = clock_ratio_index;

	/* Sync to timer tick */
	safe_halt();
	/* Change frequency on next halt or sleep */
	wrmsrl(MSR_VIA_BCR2, bcr2.val);
	/* Invoke transition */
	ACPI_FLUSH_CPU_CACHE();
	halt();

	/* Disable software clock multiplier */
	local_irq_disable();
	rdmsrl(MSR_VIA_BCR2, bcr2.val);
	bcr2.bits.ESOFTBF = 0;
	wrmsrl(MSR_VIA_BCR2, bcr2.val);
}

/* For processor with Longhaul MSR */

static void do_powersaver(int cx_address, unsigned int clock_ratio_index)
{
	union msr_longhaul longhaul;
	u8 dest_pos;
	u32 t;

	dest_pos = f_msr_table[clock_ratio_index].pos;

	rdmsrl(MSR_VIA_LONGHAUL, longhaul.val);
	/* Setup new frequency */
	longhaul.bits.RevisionKey = longhaul.bits.RevisionID;
	longhaul.bits.SoftBusRatio = clock_ratio_index & 0xf;
	longhaul.bits.SoftBusRatio4 = (clock_ratio_index & 0x10) >> 4;
	/* Setup new voltage */
	if (can_scale_voltage)
		longhaul.bits.SoftVID = f_msr_table[clock_ratio_index].vrm;
	/* Sync to timer tick */
	safe_halt();
	/* Raise voltage if necessary */
	if (can_scale_voltage && longhaul_pos < dest_pos) {
		longhaul.bits.EnableSoftVID = 1;
		wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);
		/* Change voltage */
		if (!cx_address) {
			ACPI_FLUSH_CPU_CACHE();
			halt();
		} else {
			ACPI_FLUSH_CPU_CACHE();
			/* Invoke C3 */
			inb(cx_address);
			/* Dummy op - must do something useless after P_LVL3
			 * read */
			t = inl(acpi_gbl_FADT.xpm_timer_block.address);
		}
		longhaul.bits.EnableSoftVID = 0;
		wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);
		longhaul_pos = dest_pos;
	}

	/* Change frequency on next halt or sleep */
	longhaul.bits.EnableSoftBusRatio = 1;
	wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);
	if (!cx_address) {
		ACPI_FLUSH_CPU_CACHE();
		halt();
	} else {
		ACPI_FLUSH_CPU_CACHE();
		/* Invoke C3 */
		inb(cx_address);
		/* Dummy op - must do something useless after P_LVL3 read */
		t = inl(acpi_gbl_FADT.xpm_timer_block.address);
	}
	/* Disable bus ratio bit */
	longhaul.bits.EnableSoftBusRatio = 0;
	wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);

	/* Reduce voltage if necessary */
	if (can_scale_voltage && longhaul_pos > dest_pos) {
		longhaul.bits.EnableSoftVID = 1;
		wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);
		/* Change voltage */
		if (!cx_address) {
			ACPI_FLUSH_CPU_CACHE();
			halt();
		} else {
			ACPI_FLUSH_CPU_CACHE();
			/* Invoke C3 */
			inb(cx_address);
			/* Dummy op - must do something useless after P_LVL3
			 * read */
			t = inl(acpi_gbl_FADT.xpm_timer_block.address);
		}
		longhaul.bits.EnableSoftVID = 0;
		wrmsrl(MSR_VIA_LONGHAUL, longhaul.val);
		longhaul_pos = dest_pos;
	}
}

/**
 * longhaul_set_cpu_frequency()
 * @clock_ratio_index : bitpattern of the new multiplier.
 *
 * Sets a new clock ratio.
 */

static void longhaul_setstate(unsigned int clock_ratio_index)
{
	int speed, mult;
	struct cpufreq_freqs freqs;
	static unsigned int old_ratio=-1;
	unsigned long flags;
	unsigned int pic1_mask, pic2_mask;

	if (old_ratio == clock_ratio_index)
		return;
	old_ratio = clock_ratio_index;

	mult = clock_ratio[clock_ratio_index];
	if (mult == -1)
		return;

	speed = calc_speed(mult);
	if ((speed > highest_speed) || (speed < lowest_speed))
		return;

	freqs.old = calc_speed(longhaul_get_cpu_mult());
	freqs.new = speed;
	freqs.cpu = 0; /* longhaul.c is UP only driver */

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	dprintk ("Setting to FSB:%dMHz Mult:%d.%dx (%s)\n",
			fsb, mult/10, mult%10, print_speed(speed/1000));

	preempt_disable();
	local_irq_save(flags);

	pic2_mask = inb(0xA1);
	pic1_mask = inb(0x21);	/* works on C3. save mask. */
	outb(0xFF,0xA1);	/* Overkill */
	outb(0xFE,0x21);	/* TMR0 only */

	if (longhaul_flags & USE_NORTHBRIDGE) {
		/* Disable AGP and PCI arbiters */
		outb(3, 0x22);
	} else if ((pr != NULL) && pr->flags.bm_control) {
 		/* Disable bus master arbitration */
		acpi_set_register(ACPI_BITREG_ARB_DISABLE, 1);
	}
	switch (longhaul_version) {

	/*
	 * Longhaul v1. (Samuel[C5A] and Samuel2 stepping 0[C5B])
	 * Software controlled multipliers only.
	 */
	case TYPE_LONGHAUL_V1:
		do_longhaul1(clock_ratio_index);
		break;

	/*
	 * Longhaul v2 appears in Samuel2 Steppings 1->7 [C5B] and Ezra [C5C]
	 *
	 * Longhaul v3 (aka Powersaver). (Ezra-T [C5M] & Nehemiah [C5N])
	 * Nehemiah can do FSB scaling too, but this has never been proven
	 * to work in practice.
	 */
	case TYPE_LONGHAUL_V2:
	case TYPE_POWERSAVER:
		if (longhaul_flags & USE_ACPI_C3) {
			/* Don't allow wakeup */
			acpi_set_register(ACPI_BITREG_BUS_MASTER_RLD, 0);
			do_powersaver(cx->address, clock_ratio_index);
		} else {
			do_powersaver(0, clock_ratio_index);
		}
		break;
	}

	if (longhaul_flags & USE_NORTHBRIDGE) {
		/* Enable arbiters */
		outb(0, 0x22);
	} else if ((pr != NULL) && pr->flags.bm_control) {
		/* Enable bus master arbitration */
		acpi_set_register(ACPI_BITREG_ARB_DISABLE, 0);
	}
	outb(pic2_mask,0xA1);	/* restore mask */
	outb(pic1_mask,0x21);

	local_irq_restore(flags);
	preempt_enable();

	freqs.new = calc_speed(longhaul_get_cpu_mult());
	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
}

/*
 * Centaur decided to make life a little more tricky.
 * Only longhaul v1 is allowed to read EBLCR BSEL[0:1].
 * Samuel2 and above have to try and guess what the FSB is.
 * We do this by assuming we booted at maximum multiplier, and interpolate
 * between that value multiplied by possible FSBs and cpu_mhz which
 * was calculated at boot time. Really ugly, but no other way to do this.
 */

#define ROUNDING	0xf

static int guess_fsb(int mult)
{
	int speed = cpu_khz / 1000;
	int i;
	int speeds[] = { 666, 1000, 1333, 2000 };
	int f_max, f_min;

	for (i = 0; i < 4; i++) {
		f_max = ((speeds[i] * mult) + 50) / 100;
		f_max += (ROUNDING / 2);
		f_min = f_max - ROUNDING;
		if ((speed <= f_max) && (speed >= f_min))
			return speeds[i] / 10;
	}
	return 0;
}


static int __init longhaul_get_ranges(void)
{
	unsigned int j, k = 0;
	int mult;

	/* Get current frequency */
	mult = longhaul_get_cpu_mult();
	if (mult == -1) {
		printk(KERN_INFO PFX "Invalid (reserved) multiplier!\n");
		return -EINVAL;
	}
	fsb = guess_fsb(mult);
	if (fsb == 0) {
		printk(KERN_INFO PFX "Invalid (reserved) FSB!\n");
		return -EINVAL;
	}
	/* Get max multiplier - as we always did.
	 * Longhaul MSR is usefull only when voltage scaling is enabled.
	 * C3 is booting at max anyway. */
	maxmult = mult;
	/* Get min multiplier */
	switch (cpu_model) {
	case CPU_NEHEMIAH:
		minmult = 50;
		break;
	case CPU_NEHEMIAH_C:
		minmult = 40;
		break;
	default:
		minmult = 30;
		break;
	}

	dprintk ("MinMult:%d.%dx MaxMult:%d.%dx\n",
		 minmult/10, minmult%10, maxmult/10, maxmult%10);

	highest_speed = calc_speed(maxmult);
	lowest_speed = calc_speed(minmult);
	dprintk ("FSB:%dMHz  Lowest speed: %s   Highest speed:%s\n", fsb,
		 print_speed(lowest_speed/1000),
		 print_speed(highest_speed/1000));

	if (lowest_speed == highest_speed) {
		printk (KERN_INFO PFX "highestspeed == lowest, aborting.\n");
		return -EINVAL;
	}
	if (lowest_speed > highest_speed) {
		printk (KERN_INFO PFX "nonsense! lowest (%d > %d) !\n",
			lowest_speed, highest_speed);
		return -EINVAL;
	}

	longhaul_table = kmalloc((numscales + 1) * sizeof(struct cpufreq_frequency_table), GFP_KERNEL);
	if(!longhaul_table)
		return -ENOMEM;

	for (j=0; j < numscales; j++) {
		unsigned int ratio;
		ratio = clock_ratio[j];
		if (ratio == -1)
			continue;
		if (ratio > maxmult || ratio < minmult)
			continue;
		longhaul_table[k].frequency = calc_speed(ratio);
		longhaul_table[k].index	= j;
		k++;
	}

	longhaul_table[k].frequency = CPUFREQ_TABLE_END;
	if (!k) {
		kfree (longhaul_table);
		return -EINVAL;
	}

	return 0;
}


static void __init longhaul_setup_voltagescaling(void)
{
	union msr_longhaul longhaul;
	struct mV_pos minvid, maxvid;
	unsigned int j, speed, pos, kHz_step, numvscales;
	int min_vid_speed;

	rdmsrl(MSR_VIA_LONGHAUL, longhaul.val);
	if (!(longhaul.bits.RevisionID & 1)) {
		printk(KERN_INFO PFX "Voltage scaling not supported by CPU.\n");
		return;
	}

	if (!longhaul.bits.VRMRev) {
		printk (KERN_INFO PFX "VRM 8.5\n");
		vrm_mV_table = &vrm85_mV[0];
		mV_vrm_table = &mV_vrm85[0];
	} else {
		printk (KERN_INFO PFX "Mobile VRM\n");
		if (cpu_model < CPU_NEHEMIAH)
			return;
		vrm_mV_table = &mobilevrm_mV[0];
		mV_vrm_table = &mV_mobilevrm[0];
	}

	minvid = vrm_mV_table[longhaul.bits.MinimumVID];
	maxvid = vrm_mV_table[longhaul.bits.MaximumVID];

	if (minvid.mV == 0 || maxvid.mV == 0 || minvid.mV > maxvid.mV) {
		printk (KERN_INFO PFX "Bogus values Min:%d.%03d Max:%d.%03d. "
					"Voltage scaling disabled.\n",
					minvid.mV/1000, minvid.mV%1000, maxvid.mV/1000, maxvid.mV%1000);
		return;
	}

	if (minvid.mV == maxvid.mV) {
		printk (KERN_INFO PFX "Claims to support voltage scaling but min & max are "
				"both %d.%03d. Voltage scaling disabled\n",
				maxvid.mV/1000, maxvid.mV%1000);
		return;
	}

	/* How many voltage steps */
	numvscales = maxvid.pos - minvid.pos + 1;
	printk(KERN_INFO PFX
		"Max VID=%d.%03d  "
		"Min VID=%d.%03d, "
		"%d possible voltage scales\n",
		maxvid.mV/1000, maxvid.mV%1000,
		minvid.mV/1000, minvid.mV%1000,
		numvscales);

	/* Calculate max frequency at min voltage */
	j = longhaul.bits.MinMHzBR;
	if (longhaul.bits.MinMHzBR4)
		j += 16;
	min_vid_speed = eblcr_table[j];
	if (min_vid_speed == -1)
		return;
	switch (longhaul.bits.MinMHzFSB) {
	case 0:
		min_vid_speed *= 13333;
		break;
	case 1:
		min_vid_speed *= 10000;
		break;
	case 3:
		min_vid_speed *= 6666;
		break;
	default:
		return;
		break;
	}
	if (min_vid_speed >= highest_speed)
		return;
	/* Calculate kHz for one voltage step */
	kHz_step = (highest_speed - min_vid_speed) / numvscales;


	j = 0;
	while (longhaul_table[j].frequency != CPUFREQ_TABLE_END) {
		speed = longhaul_table[j].frequency;
		if (speed > min_vid_speed)
			pos = (speed - min_vid_speed) / kHz_step + minvid.pos;
		else
			pos = minvid.pos;
		f_msr_table[longhaul_table[j].index].vrm = mV_vrm_table[pos];
		f_msr_table[longhaul_table[j].index].pos = pos;
		j++;
	}

	longhaul_pos = maxvid.pos;
	can_scale_voltage = 1;
	printk(KERN_INFO PFX "Voltage scaling enabled. "
		"Use of \"conservative\" governor is highly recommended.\n");
}


static int longhaul_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, longhaul_table);
}


static int longhaul_target(struct cpufreq_policy *policy,
			    unsigned int target_freq, unsigned int relation)
{
	unsigned int table_index = 0;
	unsigned int new_clock_ratio = 0;

	if (cpufreq_frequency_table_target(policy, longhaul_table, target_freq, relation, &table_index))
		return -EINVAL;

	new_clock_ratio = longhaul_table[table_index].index & 0xFF;

	longhaul_setstate(new_clock_ratio);

	return 0;
}


static unsigned int longhaul_get(unsigned int cpu)
{
	if (cpu)
		return 0;
	return calc_speed(longhaul_get_cpu_mult());
}

static acpi_status longhaul_walk_callback(acpi_handle obj_handle,
					  u32 nesting_level,
					  void *context, void **return_value)
{
	struct acpi_device *d;

	if ( acpi_bus_get_device(obj_handle, &d) ) {
		return 0;
	}
	*return_value = (void *)acpi_driver_data(d);
	return 1;
}

/* VIA don't support PM2 reg, but have something similar */
static int enable_arbiter_disable(void)
{
	struct pci_dev *dev;
	int status;
	int reg;
	u8 pci_cmd;

	status = 1;
	/* Find PLE133 host bridge */
	reg = 0x78;
	dev = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8601_0,
			     NULL);
	/* Find CLE266 host bridge */
	if (dev == NULL) {
		reg = 0x76;
		dev = pci_get_device(PCI_VENDOR_ID_VIA,
				     PCI_DEVICE_ID_VIA_862X_0, NULL);
		/* Find CN400 V-Link host bridge */
		if (dev == NULL)
			dev = pci_get_device(PCI_VENDOR_ID_VIA, 0x7259, NULL);
	}
	if (dev != NULL) {
		/* Enable access to port 0x22 */
		pci_read_config_byte(dev, reg, &pci_cmd);
		if (!(pci_cmd & 1<<7)) {
			pci_cmd |= 1<<7;
			pci_write_config_byte(dev, reg, pci_cmd);
			pci_read_config_byte(dev, reg, &pci_cmd);
			if (!(pci_cmd & 1<<7)) {
				printk(KERN_ERR PFX
					"Can't enable access to port 0x22.\n");
				status = 0;
			}
		}
		pci_dev_put(dev);
		return status;
	}
	return 0;
}

static int longhaul_setup_vt8235(void)
{
	struct pci_dev *dev;
	u8 pci_cmd;

	/* Find VT8235 southbridge */
	dev = pci_get_device(PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_8235, NULL);
	if (dev != NULL) {
		/* Set transition time to max */
		pci_read_config_byte(dev, 0xec, &pci_cmd);
		pci_cmd &= ~(1 << 2);
		pci_write_config_byte(dev, 0xec, pci_cmd);
		pci_read_config_byte(dev, 0xe4, &pci_cmd);
		pci_cmd &= ~(1 << 7);
		pci_write_config_byte(dev, 0xe4, pci_cmd);
		pci_read_config_byte(dev, 0xe5, &pci_cmd);
		pci_cmd |= 1 << 7;
		pci_write_config_byte(dev, 0xe5, pci_cmd);
		pci_dev_put(dev);
		return 1;
	}
	return 0;
}

static int __init longhaul_cpu_init(struct cpufreq_policy *policy)
{
	struct cpuinfo_x86 *c = cpu_data;
	char *cpuname=NULL;
	int ret;
	u32 lo, hi;
	int vt8235_present;

	/* Check what we have on this motherboard */
	switch (c->x86_model) {
	case 6:
		cpu_model = CPU_SAMUEL;
		cpuname = "C3 'Samuel' [C5A]";
		longhaul_version = TYPE_LONGHAUL_V1;
		memcpy (clock_ratio, samuel1_clock_ratio, sizeof(samuel1_clock_ratio));
		memcpy (eblcr_table, samuel1_eblcr, sizeof(samuel1_eblcr));
		break;

	case 7:
		switch (c->x86_mask) {
		case 0:
			longhaul_version = TYPE_LONGHAUL_V1;
			cpu_model = CPU_SAMUEL2;
			cpuname = "C3 'Samuel 2' [C5B]";
			/* Note, this is not a typo, early Samuel2's had
			 * Samuel1 ratios. */
			memcpy(clock_ratio, samuel1_clock_ratio,
				sizeof(samuel1_clock_ratio));
			memcpy(eblcr_table, samuel2_eblcr,
				sizeof(samuel2_eblcr));
			break;
		case 1 ... 15:
			longhaul_version = TYPE_LONGHAUL_V1;
			if (c->x86_mask < 8) {
				cpu_model = CPU_SAMUEL2;
				cpuname = "C3 'Samuel 2' [C5B]";
			} else {
				cpu_model = CPU_EZRA;
				cpuname = "C3 'Ezra' [C5C]";
			}
			memcpy(clock_ratio, ezra_clock_ratio,
				sizeof(ezra_clock_ratio));
			memcpy(eblcr_table, ezra_eblcr,
				sizeof(ezra_eblcr));
			break;
		}
		break;

	case 8:
		cpu_model = CPU_EZRA_T;
		cpuname = "C3 'Ezra-T' [C5M]";
		longhaul_version = TYPE_POWERSAVER;
		numscales=32;
		memcpy (clock_ratio, ezrat_clock_ratio, sizeof(ezrat_clock_ratio));
		memcpy (eblcr_table, ezrat_eblcr, sizeof(ezrat_eblcr));
		break;

	case 9:
		longhaul_version = TYPE_POWERSAVER;
		numscales = 32;
		memcpy(clock_ratio,
		       nehemiah_clock_ratio,
		       sizeof(nehemiah_clock_ratio));
		memcpy(eblcr_table, nehemiah_eblcr, sizeof(nehemiah_eblcr));
		switch (c->x86_mask) {
		case 0 ... 1:
			cpu_model = CPU_NEHEMIAH;
			cpuname = "C3 'Nehemiah A' [C5XLOE]";
			break;
		case 2 ... 4:
			cpu_model = CPU_NEHEMIAH;
			cpuname = "C3 'Nehemiah B' [C5XLOH]";
			break;
		case 5 ... 15:
			cpu_model = CPU_NEHEMIAH_C;
			cpuname = "C3 'Nehemiah C' [C5P]";
			break;
		}
		break;

	default:
		cpuname = "Unknown";
		break;
	}
	/* Check Longhaul ver. 2 */
	if (longhaul_version == TYPE_LONGHAUL_V2) {
		rdmsr(MSR_VIA_LONGHAUL, lo, hi);
		if (lo == 0 && hi == 0)
			/* Looks like MSR isn't present */
			longhaul_version = TYPE_LONGHAUL_V1;
	}

	printk (KERN_INFO PFX "VIA %s CPU detected.  ", cpuname);
	switch (longhaul_version) {
	case TYPE_LONGHAUL_V1:
	case TYPE_LONGHAUL_V2:
		printk ("Longhaul v%d supported.\n", longhaul_version);
		break;
	case TYPE_POWERSAVER:
		printk ("Powersaver supported.\n");
		break;
	};

	/* Doesn't hurt */
	vt8235_present = longhaul_setup_vt8235();

	/* Find ACPI data for processor */
	acpi_walk_namespace(ACPI_TYPE_PROCESSOR, ACPI_ROOT_OBJECT,
				ACPI_UINT32_MAX, &longhaul_walk_callback,
				NULL, (void *)&pr);

	/* Check ACPI support for C3 state */
	if (pr != NULL && longhaul_version == TYPE_POWERSAVER) {
		cx = &pr->power.states[ACPI_STATE_C3];
		if (cx->address > 0 && cx->latency <= 1000) {
			longhaul_flags |= USE_ACPI_C3;
			goto print_support_type;
		}
	}
	/* Check if northbridge is friendly */
	if (enable_arbiter_disable()) {
		longhaul_flags |= USE_NORTHBRIDGE;
		goto print_support_type;
	}
	/* Use VT8235 southbridge if present */
	if (longhaul_version == TYPE_POWERSAVER && vt8235_present) {
		longhaul_flags |= USE_VT8235;
		goto print_support_type;
	}
	/* Check ACPI support for bus master arbiter disable */
	if ((pr == NULL) || !(pr->flags.bm_control)) {
		printk(KERN_ERR PFX
			"No ACPI support. Unsupported northbridge.\n");
		return -ENODEV;
	}

print_support_type:
	if (longhaul_flags & USE_NORTHBRIDGE)
		printk (KERN_INFO PFX "Using northbridge support.\n");
	else if (longhaul_flags & USE_VT8235)
		printk (KERN_INFO PFX "Using VT8235 support.\n");
	else
		printk (KERN_INFO PFX "Using ACPI support.\n");

	ret = longhaul_get_ranges();
	if (ret != 0)
		return ret;

	if ((longhaul_version != TYPE_LONGHAUL_V1) && (scale_voltage != 0))
		longhaul_setup_voltagescaling();

	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
	policy->cpuinfo.transition_latency = 200000;	/* nsec */
	policy->cur = calc_speed(longhaul_get_cpu_mult());

	ret = cpufreq_frequency_table_cpuinfo(policy, longhaul_table);
	if (ret)
		return ret;

	cpufreq_frequency_table_get_attr(longhaul_table, policy->cpu);

	return 0;
}

static int __devexit longhaul_cpu_exit(struct cpufreq_policy *policy)
{
	cpufreq_frequency_table_put_attr(policy->cpu);
	return 0;
}

static struct freq_attr* longhaul_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver longhaul_driver = {
	.verify	= longhaul_verify,
	.target	= longhaul_target,
	.get	= longhaul_get,
	.init	= longhaul_cpu_init,
	.exit	= __devexit_p(longhaul_cpu_exit),
	.name	= "longhaul",
	.owner	= THIS_MODULE,
	.attr	= longhaul_attr,
};


static int __init longhaul_init(void)
{
	struct cpuinfo_x86 *c = cpu_data;

	if (c->x86_vendor != X86_VENDOR_CENTAUR || c->x86 != 6)
		return -ENODEV;

#ifdef CONFIG_SMP
	if (num_online_cpus() > 1) {
		printk(KERN_ERR PFX "More than 1 CPU detected, longhaul disabled.\n");
		return -ENODEV;
	}
#endif
#ifdef CONFIG_X86_IO_APIC
	if (cpu_has_apic) {
		printk(KERN_ERR PFX "APIC detected. Longhaul is currently broken in this configuration.\n");
		return -ENODEV;
	}
#endif
	switch (c->x86_model) {
	case 6 ... 9:
		return cpufreq_register_driver(&longhaul_driver);
	case 10:
		printk(KERN_ERR PFX "Use acpi-cpufreq driver for VIA C7\n");
	default:
		;;
	}

	return -ENODEV;
}


static void __exit longhaul_exit(void)
{
	int i;

	for (i=0; i < numscales; i++) {
		if (clock_ratio[i] == maxmult) {
			longhaul_setstate(i);
			break;
		}
	}

	cpufreq_unregister_driver(&longhaul_driver);
	kfree(longhaul_table);
}

module_param (scale_voltage, int, 0644);
MODULE_PARM_DESC(scale_voltage, "Scale voltage of processor");

MODULE_AUTHOR ("Dave Jones <davej@codemonkey.org.uk>");
MODULE_DESCRIPTION ("Longhaul driver for VIA Cyrix processors.");
MODULE_LICENSE ("GPL");

late_initcall(longhaul_init);
module_exit(longhaul_exit);
