/*
 * i386 CMOS starts out with 14 bytes clock data alpha has something
 * similar, but with details depending on the machine type.
 *
 * byte 0: seconds		0-59
 * byte 2: minutes		0-59
 * byte 4: hours		0-23 in 24hr mode,
 *				1-12 in 12hr mode, with high bit unset/set
 *					if am/pm.
 * byte 6: weekday		1-7, Sunday=1
 * byte 7: day of the month	1-31
 * byte 8: month		1-12
 * byte 9: year			0-99
 *
 * Numbers are stored in BCD/binary if bit 2 of byte 11 is unset/set The
 * clock is in 12hr/24hr mode if bit 1 of byte 11 is unset/set The clock is
 * undefined (being updated) if bit 7 of byte 10 is set. The clock is frozen
 * (to be updated) by setting bit 7 of byte 11 Bit 7 of byte 14 indicates
 * whether the CMOS clock is reliable: it is 1 if RTC power has been good
 * since this bit was last read; it is 0 when the battery is dead and system
 * power has been off.
 *
 * Avoid setting the RTC clock within 2 seconds of the day rollover that
 * starts a new month or enters daylight saving time.
 *
 * The century situation is messy:
 *
 * Usually byte 50 (0x32) gives the century (in BCD, so 19 or 20 hex), but
 * IBM PS/2 has (part of) a checksum there and uses byte 55 (0x37).
 * Sometimes byte 127 (0x7f) or Bank 1, byte 0x48 gives the century. The
 * original RTC will not access any century byte; some modern versions will.
 * If a modern RTC or BIOS increments the century byte it may go from 0x19
 * to 0x20, but in some buggy cases 0x1a is produced.
 */
/*
 * A struct tm has int fields
 *   tm_sec	0-59, 60 or 61 only for leap seconds
 *   tm_min	0-59
 *   tm_hour	0-23
 *   tm_mday	1-31
 *   tm_mon	0-11
 *   tm_year	number of years since 1900
 *   tm_wday	0-6, 0=Sunday
 *   tm_yday	0-365
 *   tm_isdst	>0: yes, 0: no, <0: unknown
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "c.h"
#include "nls.h"

#if defined(__i386__)
# ifdef HAVE_SYS_IO_H
#  include <sys/io.h>
# elif defined(HAVE_ASM_IO_H)
#  include <asm/io.h>		/* for inb, outb */
# else
/*
 * Disable cmos access; we can no longer use asm/io.h, since the kernel does
 * not export that header.
 */
#undef __i386__
void outb(int a __attribute__ ((__unused__)),
	  int b __attribute__ ((__unused__)))
{
}

int inb(int c __attribute__ ((__unused__)))
{
	return 0;
}
#endif				/* __i386__ */

#elif defined(__alpha__)
/* <asm/io.h> fails to compile, probably because of u8 etc */
extern unsigned int inb(unsigned long port);
extern void outb(unsigned char b, unsigned long port);
#else
void outb(int a __attribute__ ((__unused__)),
	  int b __attribute__ ((__unused__)))
{
}

int inb(int c __attribute__ ((__unused__)))
{
	return 0;
}
#endif				/* __alpha__ */

#include "clock.h"

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)

/*
 * The epoch.
 *
 * Unix uses 1900 as epoch for a struct tm, and 1970 for a time_t. But what
 * was written to CMOS?
 *
 * Digital DECstations use 1928 - this is on a mips or alpha Digital Unix
 * uses 1952, e.g. on AXPpxi33. Windows NT uses 1980. The ARC console
 * expects to boot Windows NT and uses 1980. (But a Ruffian uses 1900, just
 * like SRM.) It is reported that ALPHA_PRE_V1_2_SRM_CONSOLE uses 1958.
 */
#define TM_EPOCH 1900
int cmos_epoch = 1900;

/*
 * Martin Ostermann writes:
 *
 * The problem with the Jensen is twofold: First, it has the clock at a
 * different address. Secondly, it has a distinction beween "local" and
 * normal bus addresses. The local ones pertain to the hardware integrated
 * into the chipset, like serial/parallel ports and of course, the RTC.
 * Those need to be addressed differently. This is handled fine in the
 * kernel, and it's not a problem, since this usually gets totally optimized
 * by the compile. But the i/o routines of (g)libc lack this support so far.
 * The result of this is, that the old clock program worked only on the
 * Jensen when USE_DEV_PORT was defined, but not with the normal inb/outb
 * functions.
 */
int use_dev_port = 0;		/* 1 for Jensen */
int dev_port_fd;
unsigned short clock_ctl_addr = 0x70;	/* 0x170 for Jensen */
unsigned short clock_data_addr = 0x71;	/* 0x171 for Jensen */

int century_byte = 0;		/* 0: don't access a century byte
				 * 50 (0x32): usual PC value
				 * 55 (0x37): PS/2
				 */

#ifdef __alpha__
int funkyTOY = 0;		/* 1 for PC164/LX164/SX164 type alpha */
#endif

#ifdef __alpha

static int is_in_cpuinfo(char *fmt, char *str)
{
	FILE *cpuinfo;
	char field[256];
	char format[256];
	int found = 0;

	sprintf(format, "%s : %s", fmt, "%255s");

	if ((cpuinfo = fopen("/proc/cpuinfo", "r")) != NULL) {
		while (!feof(cpuinfo)) {
			if (fscanf(cpuinfo, format, field) == 1) {
				if (strncmp(field, str, strlen(str)) == 0)
					found = 1;
				break;
			}
			fgets(field, 256, cpuinfo);
		}
		fclose(cpuinfo);
	}
	return found;
}

/*
 * Set cmos_epoch, either from user options, or by asking the kernel, or by
 * looking at /proc/cpu_info
 */
void set_cmos_epoch(int ARCconsole, int SRM)
{
	unsigned long epoch;

	/* Believe the user */
	if (epoch_option != -1) {
		cmos_epoch = epoch_option;
		return;
	}

	if (ARCconsole)
		cmos_epoch = 1980;

	if (ARCconsole || SRM)
		return;

#ifdef __linux__
	/*
	 * If we can ask the kernel, we don't need guessing from
	 * /proc/cpuinfo
	 */
	if (get_epoch_rtc(&epoch, 1) == 0) {
		cmos_epoch = epoch;
		return;
	}
#endif

	/*
	 * The kernel source today says: read the year.
	 *
	 * If it is in  0-19 then the epoch is 2000.
	 * If it is in 20-47 then the epoch is 1980.
	 * If it is in 48-69 then the epoch is 1952.
	 * If it is in 70-99 then the epoch is 1928.
	 *
	 * Otherwise the epoch is 1900.
	 * TODO: Clearly, this must be changed before 2019.
	 */
	/*
	 * See whether we are dealing with SRM or MILO, as they have
	 * different "epoch" ideas.
	 */
	if (is_in_cpuinfo("system serial number", "MILO")) {
		ARCconsole = 1;
		if (debug)
			printf(_("booted from MILO\n"));
	}

	/*
	 * See whether we are dealing with a RUFFIAN aka Alpha PC-164 UX (or
	 * BX), as they have REALLY different TOY (TimeOfYear) format: BCD,
	 * and not an ARC-style epoch. BCD is detected dynamically, but we
	 * must NOT adjust like ARC.
	 */
	if (ARCconsole && is_in_cpuinfo("system type", "Ruffian")) {
		ARCconsole = 0;
		if (debug)
			printf(_("Ruffian BCD clock\n"));
	}

	if (ARCconsole)
		cmos_epoch = 1980;
}

void set_cmos_access(int Jensen, int funky_toy)
{

	/*
	 * See whether we're dealing with a Jensen---it has a weird I/O
	 * system. DEC was just learning how to build Alpha PCs.
	 */
	if (Jensen || is_in_cpuinfo("system type", "Jensen")) {
		use_dev_port = 1;
		clock_ctl_addr = 0x170;
		clock_data_addr = 0x171;
		if (debug)
			printf(_("clockport adjusted to 0x%x\n"),
			       clock_ctl_addr);
	}

	/*
	 * See whether we are dealing with PC164/LX164/SX164, as they have a
	 * TOY that must be accessed differently to work correctly.
	 */
	/* Nautilus stuff reported by Neoklis Kyriazis */
	if (funky_toy ||
	    is_in_cpuinfo("system variation", "PC164") ||
	    is_in_cpuinfo("system variation", "LX164") ||
	    is_in_cpuinfo("system variation", "SX164") ||
	    is_in_cpuinfo("system type", "Nautilus")) {
		funkyTOY = 1;
		if (debug)
			printf(_("funky TOY!\n"));
	}
}
#endif				/* __alpha */

#if __alpha__
/*
 * The Alpha doesn't allow user-level code to disable interrupts (for good
 * reasons). Instead, we ensure atomic operation by performing the operation
 * and checking whether the high 32 bits of the cycle counter changed. If
 * they did, a context switch must have occurred and we redo the operation.
 * As long as the operation is reasonably short, it will complete
 * atomically, eventually.
 */
static unsigned long
atomic(const char *name, unsigned long (*op) (unsigned long), unsigned long arg)
{
	unsigned long ts1, ts2, n, v;

	for (n = 0; n < 1000; ++n) {
		asm volatile ("rpcc %0":"r=" (ts1));
		v = (*op) (arg);
		asm volatile ("rpcc %0":"r=" (ts2));

		if ((ts1 ^ ts2) >> 32 == 0) {
			return v;
		}
	}
	errx(EXIT_FAILURE, _("atomic %s failed for 1000 iterations!"),
		name);
}
#else

/*
 * Hmmh, this isn't very atomic. Maybe we should force an error instead?
 *
 * TODO: optimize the access to CMOS by mlockall(MCL_CURRENT) and SCHED_FIFO
 */
static unsigned long
atomic(const char *name __attribute__ ((__unused__)),
       unsigned long (*op) (unsigned long),
       unsigned long arg)
{
	return (*op) (arg);
}

#endif

static inline unsigned long cmos_read(unsigned long reg)
{
	if (use_dev_port) {
		unsigned char v = reg | 0x80;
		lseek(dev_port_fd, clock_ctl_addr, 0);
		if (write(dev_port_fd, &v, 1) == -1 && debug)
			printf(_
			       ("cmos_read(): write to control address %X failed: %s\n"),
			       clock_ctl_addr, strerror(errno));
		lseek(dev_port_fd, clock_data_addr, 0);
		if (read(dev_port_fd, &v, 1) == -1 && debug)
			printf(_
			       ("cmos_read(): read data address %X failed: %s\n"),
			       clock_data_addr, strerror(errno));
		return v;
	} else {
		/*
		 * We only want to read CMOS data, but unfortunately writing
		 * to bit 7 disables (1) or enables (0) NMI; since this bit
		 * is read-only we have to guess the old status. Various
		 * docs suggest that one should disable NMI while
		 * reading/writing CMOS data, and enable it again
		 * afterwards. This would yield the sequence
		 *
		 *  outb (reg | 0x80, 0x70);
		 *  val = inb(0x71);
		 *  outb (0x0d, 0x70);  // 0x0d: random read-only location
		 *
		 * Other docs state that "any write to 0x70 should be
		 * followed by an action to 0x71 or the RTC wil be left in
		 * an unknown state". Most docs say that it doesnt matter at
		 * all what one does.
		 */
		/*
		 * bit 0x80: disable NMI while reading - should we? Let us
		 * follow the kernel and not disable. Called only with 0 <=
		 * reg < 128
		 */
		outb(reg, clock_ctl_addr);
		return inb(clock_data_addr);
	}
}

static inline unsigned long cmos_write(unsigned long reg, unsigned long val)
{
	if (use_dev_port) {
		unsigned char v = reg | 0x80;
		lseek(dev_port_fd, clock_ctl_addr, 0);
		if (write(dev_port_fd, &v, 1) == -1 && debug)
			printf(_
			       ("cmos_write(): write to control address %X failed: %s\n"),
			       clock_ctl_addr, strerror(errno));
		v = (val & 0xff);
		lseek(dev_port_fd, clock_data_addr, 0);
		if (write(dev_port_fd, &v, 1) == -1 && debug)
			printf(_
			       ("cmos_write(): write to data address %X failed: %s\n"),
			       clock_data_addr, strerror(errno));
	} else {
		outb(reg, clock_ctl_addr);
		outb(val, clock_data_addr);
	}
	return 0;
}

static unsigned long cmos_set_time(unsigned long arg)
{
	unsigned char save_control, save_freq_select, pmbit = 0;
	struct tm tm = *(struct tm *)arg;
	unsigned int century;

/*
 * CMOS byte 10 (clock status register A) has 3 bitfields:
 * bit 7: 1 if data invalid, update in progress (read-only bit)
 *         (this is raised 224 us before the actual update starts)
 *  6-4    select base frequency
 *         010: 32768 Hz time base (default)
 *         111: reset
 *         all other combinations are manufacturer-dependent
 *         (e.g.: DS1287: 010 = start oscillator, anything else = stop)
 *  3-0    rate selection bits for interrupt
 *         0000 none (may stop RTC)
 *         0001, 0010 give same frequency as 1000, 1001
 *         0011 122 microseconds (minimum, 8192 Hz)
 *         .... each increase by 1 halves the frequency, doubles the period
 *         1111 500 milliseconds (maximum, 2 Hz)
 *         0110 976.562 microseconds (default 1024 Hz)
 */
	save_control = cmos_read(11);	/* tell the clock it's being set */
	cmos_write(11, (save_control | 0x80));
	save_freq_select = cmos_read(10);	/* stop and reset prescaler */
	cmos_write(10, (save_freq_select | 0x70));

	tm.tm_year += TM_EPOCH;
	century = tm.tm_year / 100;
	tm.tm_year -= cmos_epoch;
	tm.tm_year %= 100;
	tm.tm_mon += 1;
	tm.tm_wday += 1;

	if (!(save_control & 0x02)) {	/* 12hr mode; the default is 24hr mode */
		if (tm.tm_hour == 0)
			tm.tm_hour = 24;
		if (tm.tm_hour > 12) {
			tm.tm_hour -= 12;
			pmbit = 0x80;
		}
	}

	if (!(save_control & 0x04)) {	/* BCD mode - the default */
		BIN_TO_BCD(tm.tm_sec);
		BIN_TO_BCD(tm.tm_min);
		BIN_TO_BCD(tm.tm_hour);
		BIN_TO_BCD(tm.tm_wday);
		BIN_TO_BCD(tm.tm_mday);
		BIN_TO_BCD(tm.tm_mon);
		BIN_TO_BCD(tm.tm_year);
		BIN_TO_BCD(century);
	}

	cmos_write(0, tm.tm_sec);
	cmos_write(2, tm.tm_min);
	cmos_write(4, tm.tm_hour | pmbit);
	cmos_write(6, tm.tm_wday);
	cmos_write(7, tm.tm_mday);
	cmos_write(8, tm.tm_mon);
	cmos_write(9, tm.tm_year);
	if (century_byte)
		cmos_write(century_byte, century);

	/*
	 * The kernel sources, linux/arch/i386/kernel/time.c, have the
	 * following comment:
	 *
	 * The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ... -- Markus Kuhn
	 */
	cmos_write(11, save_control);
	cmos_write(10, save_freq_select);
	return 0;
}

static int hclock_read(unsigned long reg)
{
	return atomic("clock read", cmos_read, (reg));
}

static void hclock_set_time(const struct tm *tm)
{
	atomic("set time", cmos_set_time, (unsigned long)(tm));
}

static inline int cmos_clock_busy(void)
{
	return
#ifdef __alpha__
	    /* poll bit 4 (UF) of Control Register C */
	    funkyTOY ? (hclock_read(12) & 0x10) :
#endif
	    /* poll bit 7 (UIP) of Control Register A */
	    (hclock_read(10) & 0x80);
}

static int synchronize_to_clock_tick_cmos(void)
{
	int i;

	/*
	 * Wait for rise. Should be within a second, but in case something
	 * weird happens, we have a limit on this loop to reduce the impact
	 * of this failure.
	 */
	for (i = 0; !cmos_clock_busy(); i++)
		if (i >= 10000000)
			return 1;

	/* Wait for fall.  Should be within 2.228 ms. */
	for (i = 0; cmos_clock_busy(); i++)
		if (i >= 1000000)
			return 1;
	return 0;
}

/*
 * Read the hardware clock and return the current time via <tm> argument.
 * Assume we have an ISA machine and read the clock directly with CPU I/O
 * instructions.
 *
 * This function is not totally reliable.  It takes a finite and
 * unpredictable amount of time to execute the code below. During that time,
 * the clock may change and we may even read an invalid value in the middle
 * of an update. We do a few checks to minimize this possibility, but only
 * the kernel can actually read the clock properly, since it can execute
 * code in a short and predictable amount of time (by turning of
 * interrupts).
 *
 * In practice, the chance of this function returning the wrong time is
 * extremely remote.
 */
static int read_hardware_clock_cmos(struct tm *tm)
{
	bool got_time = FALSE;
	unsigned char status, pmbit;

	status = pmbit = 0;	/* just for gcc */

	while (!got_time) {
		/*
		 * Bit 7 of Byte 10 of the Hardware Clock value is the
		 * Update In Progress (UIP) bit, which is on while and 244
		 * uS before the Hardware Clock updates itself. It updates
		 * the counters individually, so reading them during an
		 * update would produce garbage. The update takes 2mS, so we
		 * could be spinning here that long waiting for this bit to
		 * turn off.
		 *
		 * Furthermore, it is pathologically possible for us to be
		 * in this code so long that even if the UIP bit is not on
		 * at first, the clock has changed while we were running. We
		 * check for that too, and if it happens, we start over.
		 */
		if (!cmos_clock_busy()) {
			/* No clock update in progress, go ahead and read */
			tm->tm_sec = hclock_read(0);
			tm->tm_min = hclock_read(2);
			tm->tm_hour = hclock_read(4);
			tm->tm_wday = hclock_read(6);
			tm->tm_mday = hclock_read(7);
			tm->tm_mon = hclock_read(8);
			tm->tm_year = hclock_read(9);
			status = hclock_read(11);
#if 0
			if (century_byte)
				century = hclock_read(century_byte);
#endif
			/*
			 * Unless the clock changed while we were reading,
			 * consider this a good clock read .
			 */
			if (tm->tm_sec == hclock_read(0))
				got_time = TRUE;
		}
		/*
		 * Yes, in theory we could have been running for 60 seconds
		 * and the above test wouldn't work!
		 */
	}

	if (!(status & 0x04)) {	/* BCD mode - the default */
		BCD_TO_BIN(tm->tm_sec);
		BCD_TO_BIN(tm->tm_min);
		pmbit = (tm->tm_hour & 0x80);
		tm->tm_hour &= 0x7f;
		BCD_TO_BIN(tm->tm_hour);
		BCD_TO_BIN(tm->tm_wday);
		BCD_TO_BIN(tm->tm_mday);
		BCD_TO_BIN(tm->tm_mon);
		BCD_TO_BIN(tm->tm_year);
#if 0
		BCD_TO_BIN(century);
#endif
	}

	/*
	 * We don't use the century byte of the Hardware Clock since we
	 * don't know its address (usually 50 or 55). Here, we follow the
	 * advice of the X/Open Base Working Group: "if century is not
	 * specified, then values in the range [69-99] refer to years in the
	 * twentieth century (1969 to 1999 inclusive), and values in the
	 * range [00-68] refer to years in the twenty-first century (2000 to
	 * 2068 inclusive)."
	 */
	tm->tm_wday -= 1;
	tm->tm_mon -= 1;
	tm->tm_year += (cmos_epoch - TM_EPOCH);
	if (tm->tm_year < 69)
		tm->tm_year += 100;
	if (pmbit) {
		tm->tm_hour += 12;
		if (tm->tm_hour == 24)
			tm->tm_hour = 0;
	}

	tm->tm_isdst = -1;	/* don't know whether it's daylight */
	return 0;
}

static int set_hardware_clock_cmos(const struct tm *new_broken_time)
{

	hclock_set_time(new_broken_time);
	return 0;
}

#if defined(__i386__) || defined(__alpha__)
# if defined(HAVE_IOPL)
static int i386_iopl(const int level)
{
	extern int iopl(const int lvl);
	return iopl(level);
}
# else
static int i386_iopl(const int level __attribute__ ((__unused__)))
{
	extern int ioperm(unsigned long from, unsigned long num, int turn_on);
	return ioperm(clock_ctl_addr, 2, 1);
}
# endif
#else
static int i386_iopl(const int level __attribute__ ((__unused__)))
{
	return -2;
}
#endif

static int get_permissions_cmos(void)
{
	int rc;

	if (use_dev_port) {
		if ((dev_port_fd = open("/dev/port", O_RDWR)) < 0) {
			warn(_("Cannot open /dev/port"));
			rc = 1;
		} else
			rc = 0;
	} else {
		rc = i386_iopl(3);
		if (rc == -2) {
			warnx(_("I failed to get permission because I didn't try."));
		} else if (rc != 0) {
			rc = errno;
			warn(_("unable to get I/O port access:  "
                               "the iopl(3) call failed."));
			if (rc == EPERM && geteuid())
				warnx(_("Probably you need root privileges.\n"));
		}
	}
	return rc ? 1 : 0;
}

static struct clock_ops cmos = {
	"direct I/O instructions to ISA clock",
	get_permissions_cmos,
	read_hardware_clock_cmos,
	set_hardware_clock_cmos,
	synchronize_to_clock_tick_cmos,
};

/*
 * return &cmos if cmos clock present, NULL otherwise choose this
 * construction to avoid gcc messages about unused variables
 */
struct clock_ops *probe_for_cmos_clock(void)
{
	int have_cmos =
#if defined(__i386__) || defined(__alpha__)
	    TRUE;
#else
	    FALSE;
#endif
	return have_cmos ? &cmos : NULL;
}
