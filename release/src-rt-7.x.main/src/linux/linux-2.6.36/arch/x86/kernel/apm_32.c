

#include <linux/module.h>

#include <linux/poll.h>
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <linux/fcntl.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>
#include <linux/apm_bios.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/capability.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/freezer.h>
#include <linux/smp.h>
#include <linux/dmi.h>
#include <linux/suspend.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>

#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/desc.h>
#include <asm/i8253.h>
#include <asm/olpc.h>
#include <asm/paravirt.h>
#include <asm/reboot.h>

#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
extern int (*console_blank_hook)(int);
#endif

/*
 * The apm_bios device is one of the misc char devices.
 * This is its minor number.
 */
#define	APM_MINOR_DEV	134

/*
 * See Documentation/Config.help for the configuration options.
 *
 * Various options can be changed at boot time as follows:
 * (We allow underscores for compatibility with the modules code)
 *	apm=on/off			enable/disable APM
 *	    [no-]allow[-_]ints		allow interrupts during BIOS calls
 *	    [no-]broken[-_]psr		BIOS has a broken GetPowerStatus call
 *	    [no-]realmode[-_]power[-_]off	switch to real mode before
 *	    					powering off
 *	    [no-]debug			log some debugging messages
 *	    [no-]power[-_]off		power off on shutdown
 *	    [no-]smp			Use apm even on an SMP box
 *	    bounce[-_]interval=<n>	number of ticks to ignore suspend
 *	    				bounces
 *          idle[-_]threshold=<n>       System idle percentage above which to
 *                                      make APM BIOS idle calls. Set it to
 *                                      100 to disable.
 *          idle[-_]period=<n>          Period (in 1/100s of a second) over
 *                                      which the idle percentage is
 *                                      calculated.
 */


/*
 * Define as 1 to make the driver always call the APM BIOS busy
 * routine even if the clock was not reported as slowed by the
 * idle routine.  Otherwise, define as 0.
 */
#define ALWAYS_CALL_BUSY   1

/*
 * Define to make the APM BIOS calls zero all data segment registers (so
 * that an incorrect BIOS implementation will cause a kernel panic if it
 * tries to write to arbitrary memory).
 */
#define APM_ZERO_SEGS

#include <asm/apm.h>

/*
 * Define to re-initialize the interrupt 0 timer to 100 Hz after a suspend.
 * This patched by Chad Miller <cmiller@surfsouth.com>, original code by
 * David Chen <chen@ctpa04.mit.edu>
 */
#undef INIT_TIMER_AFTER_SUSPEND

#ifdef INIT_TIMER_AFTER_SUSPEND
#include <linux/timex.h>
#include <asm/io.h>
#include <linux/delay.h>
#endif

/*
 * Need to poll the APM BIOS every second
 */
#define APM_CHECK_TIMEOUT	(HZ)

/*
 * Ignore suspend events for this amount of time after a resume
 */
#define DEFAULT_BOUNCE_INTERVAL	(3 * HZ)

/*
 * Maximum number of events stored
 */
#define APM_MAX_EVENTS		20

/*
 * The per-file APM data
 */
struct apm_user {
	int		magic;
	struct apm_user *next;
	unsigned int	suser: 1;
	unsigned int	writer: 1;
	unsigned int	reader: 1;
	unsigned int	suspend_wait: 1;
	int		suspend_result;
	int		suspends_pending;
	int		standbys_pending;
	int		suspends_read;
	int		standbys_read;
	int		event_head;
	int		event_tail;
	apm_event_t	events[APM_MAX_EVENTS];
};

/*
 * The magic number in apm_user
 */
#define APM_BIOS_MAGIC		0x4101

/*
 * idle percentage above which bios idle calls are done
 */
#ifdef CONFIG_APM_CPU_IDLE
#define DEFAULT_IDLE_THRESHOLD	95
#else
#define DEFAULT_IDLE_THRESHOLD	100
#endif
#define DEFAULT_IDLE_PERIOD	(100 / 3)

/*
 * Local variables
 */
static struct {
	unsigned long	offset;
	unsigned short	segment;
} apm_bios_entry;
static int clock_slowed;
static int idle_threshold __read_mostly = DEFAULT_IDLE_THRESHOLD;
static int idle_period __read_mostly = DEFAULT_IDLE_PERIOD;
static int set_pm_idle;
static int suspends_pending;
static int standbys_pending;
static int ignore_sys_suspend;
static int ignore_normal_resume;
static int bounce_interval __read_mostly = DEFAULT_BOUNCE_INTERVAL;

static int debug __read_mostly;
static int smp __read_mostly;
static int apm_disabled = -1;
#ifdef CONFIG_SMP
static int power_off;
#else
static int power_off = 1;
#endif
static int realmode_power_off;
#ifdef CONFIG_APM_ALLOW_INTS
static int allow_ints = 1;
#else
static int allow_ints;
#endif
static int broken_psr;

static DECLARE_WAIT_QUEUE_HEAD(apm_waitqueue);
static DECLARE_WAIT_QUEUE_HEAD(apm_suspend_waitqueue);
static struct apm_user *user_list;
static DEFINE_SPINLOCK(user_list_lock);
static DEFINE_MUTEX(apm_mutex);

/*
 * Set up a segment that references the real mode segment 0x40
 * that extends up to the end of page zero (that we have reserved).
 * This is for buggy BIOS's that refer to (real mode) segment 0x40
 * even though they are called in protected mode.
 */
static struct desc_struct bad_bios_desc = GDT_ENTRY_INIT(0x4092,
			(unsigned long)__va(0x400UL), PAGE_SIZE - 0x400 - 1);

static const char driver_version[] = "1.16ac";	/* no spaces */

static struct task_struct *kapmd_task;

/*
 *	APM event names taken from the APM 1.2 specification. These are
 *	the message codes that the BIOS uses to tell us about events
 */
static const char * const apm_event_name[] = {
	"system standby",
	"system suspend",
	"normal resume",
	"critical resume",
	"low battery",
	"power status change",
	"update time",
	"critical suspend",
	"user standby",
	"user suspend",
	"system standby resume",
	"capabilities change"
};
#define NR_APM_EVENT_NAME ARRAY_SIZE(apm_event_name)

typedef struct lookup_t {
	int	key;
	char 	*msg;
} lookup_t;

/*
 *	The BIOS returns a set of standard error codes in AX when the
 *	carry flag is set.
 */

static const lookup_t error_table[] = {
/* N/A	{ APM_SUCCESS,		"Operation succeeded" }, */
	{ APM_DISABLED,		"Power management disabled" },
	{ APM_CONNECTED,	"Real mode interface already connected" },
	{ APM_NOT_CONNECTED,	"Interface not connected" },
	{ APM_16_CONNECTED,	"16 bit interface already connected" },
/* N/A	{ APM_16_UNSUPPORTED,	"16 bit interface not supported" }, */
	{ APM_32_CONNECTED,	"32 bit interface already connected" },
	{ APM_32_UNSUPPORTED,	"32 bit interface not supported" },
	{ APM_BAD_DEVICE,	"Unrecognized device ID" },
	{ APM_BAD_PARAM,	"Parameter out of range" },
	{ APM_NOT_ENGAGED,	"Interface not engaged" },
	{ APM_BAD_FUNCTION,     "Function not supported" },
	{ APM_RESUME_DISABLED,	"Resume timer disabled" },
	{ APM_BAD_STATE,	"Unable to enter requested state" },
/* N/A	{ APM_NO_EVENTS,	"No events pending" }, */
	{ APM_NO_ERROR,		"BIOS did not set a return code" },
	{ APM_NOT_PRESENT,	"No APM present" }
};
#define ERROR_COUNT	ARRAY_SIZE(error_table)

/**
 *	apm_error	-	display an APM error
 *	@str: information string
 *	@err: APM BIOS return code
 *
 *	Write a meaningful log entry to the kernel log in the event of
 *	an APM error.  Note that this also handles (negative) kernel errors.
 */

static void apm_error(char *str, int err)
{
	int i;

	for (i = 0; i < ERROR_COUNT; i++)
		if (error_table[i].key == err)
			break;
	if (i < ERROR_COUNT)
		printk(KERN_NOTICE "apm: %s: %s\n", str, error_table[i].msg);
	else if (err < 0)
		printk(KERN_NOTICE "apm: %s: linux error code %i\n", str, err);
	else
		printk(KERN_NOTICE "apm: %s: unknown error code %#2.2x\n",
		       str, err);
}

/*
 * These are the actual BIOS calls.  Depending on APM_ZERO_SEGS and
 * apm_info.allow_ints, we are being really paranoid here!  Not only
 * are interrupts disabled, but all the segment registers (except SS)
 * are saved and zeroed this means that if the BIOS tries to reference
 * any data without explicitly loading the segment registers, the kernel
 * will fault immediately rather than have some unforeseen circumstances
 * for the rest of the kernel.  And it will be very obvious!  :-) Doing
 * this depends on CS referring to the same physical memory as DS so that
 * DS can be zeroed before the call. Unfortunately, we can't do anything
 * about the stack segment/pointer.  Also, we tell the compiler that
 * everything could change.
 *
 * Also, we KNOW that for the non error case of apm_bios_call, there
 * is no useful data returned in the low order 8 bits of eax.
 */

static inline unsigned long __apm_irq_save(void)
{
	unsigned long flags;
	local_save_flags(flags);
	if (apm_info.allow_ints) {
		if (irqs_disabled_flags(flags))
			local_irq_enable();
	} else
		local_irq_disable();

	return flags;
}

#define apm_irq_save(flags) \
	do { flags = __apm_irq_save(); } while (0)

static inline void apm_irq_restore(unsigned long flags)
{
	if (irqs_disabled_flags(flags))
		local_irq_disable();
	else if (irqs_disabled())
		local_irq_enable();
}

#ifdef APM_ZERO_SEGS
#	define APM_DECL_SEGS \
		unsigned int saved_fs; unsigned int saved_gs;
#	define APM_DO_SAVE_SEGS \
		savesegment(fs, saved_fs); savesegment(gs, saved_gs)
#	define APM_DO_RESTORE_SEGS \
		loadsegment(fs, saved_fs); loadsegment(gs, saved_gs)
#else
#	define APM_DECL_SEGS
#	define APM_DO_SAVE_SEGS
#	define APM_DO_RESTORE_SEGS
#endif

struct apm_bios_call {
	u32 func;
	/* In and out */
	u32 ebx;
	u32 ecx;
	/* Out only */
	u32 eax;
	u32 edx;
	u32 esi;

	/* Error: -ENOMEM, or bits 8-15 of eax */
	int err;
};

/**
 *	__apm_bios_call - Make an APM BIOS 32bit call
 *	@_call: pointer to struct apm_bios_call.
 *
 *	Make an APM call using the 32bit protected mode interface. The
 *	caller is responsible for knowing if APM BIOS is configured and
 *	enabled. This call can disable interrupts for a long period of
 *	time on some laptops.  The return value is in AH and the carry
 *	flag is loaded into AL.  If there is an error, then the error
 *	code is returned in AH (bits 8-15 of eax) and this function
 *	returns non-zero.
 *
 *	Note: this makes the call on the current CPU.
 */
static long __apm_bios_call(void *_call)
{
	APM_DECL_SEGS
	unsigned long		flags;
	int			cpu;
	struct desc_struct	save_desc_40;
	struct desc_struct	*gdt;
	struct apm_bios_call	*call = _call;

	cpu = get_cpu();
	BUG_ON(cpu != 0);
	gdt = get_cpu_gdt_table(cpu);
	save_desc_40 = gdt[0x40 / 8];
	gdt[0x40 / 8] = bad_bios_desc;

	apm_irq_save(flags);
	APM_DO_SAVE_SEGS;
	apm_bios_call_asm(call->func, call->ebx, call->ecx,
			  &call->eax, &call->ebx, &call->ecx, &call->edx,
			  &call->esi);
	APM_DO_RESTORE_SEGS;
	apm_irq_restore(flags);
	gdt[0x40 / 8] = save_desc_40;
	put_cpu();

	return call->eax & 0xff;
}

/* Run __apm_bios_call or __apm_bios_call_simple on CPU 0 */
static int on_cpu0(long (*fn)(void *), struct apm_bios_call *call)
{
	int ret;

	/* Don't bother with work_on_cpu in the common case, so we don't
	 * have to worry about OOM or overhead. */
	if (get_cpu() == 0) {
		ret = fn(call);
		put_cpu();
	} else {
		put_cpu();
		ret = work_on_cpu(0, fn, call);
	}

	/* work_on_cpu can fail with -ENOMEM */
	if (ret < 0)
		call->err = ret;
	else
		call->err = (call->eax >> 8) & 0xff;

	return ret;
}

/**
 *	apm_bios_call	-	Make an APM BIOS 32bit call (on CPU 0)
 *	@call: the apm_bios_call registers.
 *
 *	If there is an error, it is returned in @call.err.
 */
static int apm_bios_call(struct apm_bios_call *call)
{
	return on_cpu0(__apm_bios_call, call);
}

/**
 *	__apm_bios_call_simple - Make an APM BIOS 32bit call (on CPU 0)
 *	@_call: pointer to struct apm_bios_call.
 *
 *	Make a BIOS call that returns one value only, or just status.
 *	If there is an error, then the error code is returned in AH
 *	(bits 8-15 of eax) and this function returns non-zero (it can
 *	also return -ENOMEM). This is used for simpler BIOS operations.
 *	This call may hold interrupts off for a long time on some laptops.
 *
 *	Note: this makes the call on the current CPU.
 */
static long __apm_bios_call_simple(void *_call)
{
	u8			error;
	APM_DECL_SEGS
	unsigned long		flags;
	int			cpu;
	struct desc_struct	save_desc_40;
	struct desc_struct	*gdt;
	struct apm_bios_call	*call = _call;

	cpu = get_cpu();
	BUG_ON(cpu != 0);
	gdt = get_cpu_gdt_table(cpu);
	save_desc_40 = gdt[0x40 / 8];
	gdt[0x40 / 8] = bad_bios_desc;

	apm_irq_save(flags);
	APM_DO_SAVE_SEGS;
	error = apm_bios_call_simple_asm(call->func, call->ebx, call->ecx,
					 &call->eax);
	APM_DO_RESTORE_SEGS;
	apm_irq_restore(flags);
	gdt[0x40 / 8] = save_desc_40;
	put_cpu();
	return error;
}

/**
 *	apm_bios_call_simple	-	make a simple APM BIOS 32bit call
 *	@func: APM function to invoke
 *	@ebx_in: EBX register value for BIOS call
 *	@ecx_in: ECX register value for BIOS call
 *	@eax: EAX register on return from the BIOS call
 *	@err: bits
 *
 *	Make a BIOS call that returns one value only, or just status.
 *	If there is an error, then the error code is returned in @err
 *	and this function returns non-zero. This is used for simpler
 *	BIOS operations.  This call may hold interrupts off for a long
 *	time on some laptops.
 */
static int apm_bios_call_simple(u32 func, u32 ebx_in, u32 ecx_in, u32 *eax,
				int *err)
{
	struct apm_bios_call call;
	int ret;

	call.func = func;
	call.ebx = ebx_in;
	call.ecx = ecx_in;

	ret = on_cpu0(__apm_bios_call_simple, &call);
	*eax = call.eax;
	*err = call.err;
	return ret;
}

/**
 *	apm_driver_version	-	APM driver version
 *	@val:	loaded with the APM version on return
 *
 *	Retrieve the APM version supported by the BIOS. This is only
 *	supported for APM 1.1 or higher. An error indicates APM 1.0 is
 *	probably present.
 *
 *	On entry val should point to a value indicating the APM driver
 *	version with the high byte being the major and the low byte the
 *	minor number both in BCD
 *
 *	On return it will hold the BIOS revision supported in the
 *	same format.
 */

static int apm_driver_version(u_short *val)
{
	u32 eax;
	int err;

	if (apm_bios_call_simple(APM_FUNC_VERSION, 0, *val, &eax, &err))
		return err;
	*val = eax;
	return APM_SUCCESS;
}

/**
 *	apm_get_event	-	get an APM event from the BIOS
 *	@event: pointer to the event
 *	@info: point to the event information
 *
 *	The APM BIOS provides a polled information for event
 *	reporting. The BIOS expects to be polled at least every second
 *	when events are pending. When a message is found the caller should
 *	poll until no more messages are present.  However, this causes
 *	problems on some laptops where a suspend event notification is
 *	not cleared until it is acknowledged.
 *
 *	Additional information is returned in the info pointer, providing
 *	that APM 1.2 is in use. If no messges are pending the value 0x80
 *	is returned (No power management events pending).
 */
static int apm_get_event(apm_event_t *event, apm_eventinfo_t *info)
{
	struct apm_bios_call call;

	call.func = APM_FUNC_GET_EVENT;
	call.ebx = call.ecx = 0;

	if (apm_bios_call(&call))
		return call.err;

	*event = call.ebx;
	if (apm_info.connection_version < 0x0102)
		*info = ~0; /* indicate info not valid */
	else
		*info = call.ecx;
	return APM_SUCCESS;
}

/**
 *	set_power_state	-	set the power management state
 *	@what: which items to transition
 *	@state: state to transition to
 *
 *	Request an APM change of state for one or more system devices. The
 *	processor state must be transitioned last of all. what holds the
 *	class of device in the upper byte and the device number (0xFF for
 *	all) for the object to be transitioned.
 *
 *	The state holds the state to transition to, which may in fact
 *	be an acceptance of a BIOS requested state change.
 */

static int set_power_state(u_short what, u_short state)
{
	u32 eax;
	int err;

	if (apm_bios_call_simple(APM_FUNC_SET_STATE, what, state, &eax, &err))
		return err;
	return APM_SUCCESS;
}

/**
 *	set_system_power_state - set system wide power state
 *	@state: which state to enter
 *
 *	Transition the entire system into a new APM power state.
 */

static int set_system_power_state(u_short state)
{
	return set_power_state(APM_DEVICE_ALL, state);
}

/**
 *	apm_do_idle	-	perform power saving
 *
 *	This function notifies the BIOS that the processor is (in the view
 *	of the OS) idle. It returns -1 in the event that the BIOS refuses
 *	to handle the idle request. On a success the function returns 1
 *	if the BIOS did clock slowing or 0 otherwise.
 */

static int apm_do_idle(void)
{
	u32 eax;
	u8 ret = 0;
	int idled = 0;
	int polling;
	int err = 0;

	polling = !!(current_thread_info()->status & TS_POLLING);
	if (polling) {
		current_thread_info()->status &= ~TS_POLLING;
		/*
		 * TS_POLLING-cleared state must be visible before we
		 * test NEED_RESCHED:
		 */
		smp_mb();
	}
	if (!need_resched()) {
		idled = 1;
		ret = apm_bios_call_simple(APM_FUNC_IDLE, 0, 0, &eax, &err);
	}
	if (polling)
		current_thread_info()->status |= TS_POLLING;

	if (!idled)
		return 0;

	if (ret) {
		static unsigned long t;

		/* This always fails on some SMP boards running UP kernels.
		 * Only report the failure the first 5 times.
		 */
		if (++t < 5) {
			printk(KERN_DEBUG "apm_do_idle failed (%d)\n", err);
			t = jiffies;
		}
		return -1;
	}
	clock_slowed = (apm_info.bios.flags & APM_IDLE_SLOWS_CLOCK) != 0;
	return clock_slowed;
}

/**
 *	apm_do_busy	-	inform the BIOS the CPU is busy
 *
 *	Request that the BIOS brings the CPU back to full performance.
 */

static void apm_do_busy(void)
{
	u32 dummy;
	int err;

	if (clock_slowed || ALWAYS_CALL_BUSY) {
		(void)apm_bios_call_simple(APM_FUNC_BUSY, 0, 0, &dummy, &err);
		clock_slowed = 0;
	}
}

/*
 * If no process has really been interested in
 * the CPU for some time, we want to call BIOS
 * power management - we probably want
 * to conserve power.
 */
#define IDLE_CALC_LIMIT	(HZ * 100)
#define IDLE_LEAKY_MAX	16

static void (*original_pm_idle)(void) __read_mostly;

/**
 * apm_cpu_idle		-	cpu idling for APM capable Linux
 *
 * This is the idling function the kernel executes when APM is available. It
 * tries to do BIOS powermanagement based on the average system idle time.
 * Furthermore it calls the system default idle routine.
 */

static void apm_cpu_idle(void)
{
	static int use_apm_idle; /* = 0 */
	static unsigned int last_jiffies; /* = 0 */
	static unsigned int last_stime; /* = 0 */

	int apm_idle_done = 0;
	unsigned int jiffies_since_last_check = jiffies - last_jiffies;
	unsigned int bucket;

recalc:
	if (jiffies_since_last_check > IDLE_CALC_LIMIT) {
		use_apm_idle = 0;
		last_jiffies = jiffies;
		last_stime = current->stime;
	} else if (jiffies_since_last_check > idle_period) {
		unsigned int idle_percentage;

		idle_percentage = current->stime - last_stime;
		idle_percentage *= 100;
		idle_percentage /= jiffies_since_last_check;
		use_apm_idle = (idle_percentage > idle_threshold);
		if (apm_info.forbid_idle)
			use_apm_idle = 0;
		last_jiffies = jiffies;
		last_stime = current->stime;
	}

	bucket = IDLE_LEAKY_MAX;

	while (!need_resched()) {
		if (use_apm_idle) {
			unsigned int t;

			t = jiffies;
			switch (apm_do_idle()) {
			case 0:
				apm_idle_done = 1;
				if (t != jiffies) {
					if (bucket) {
						bucket = IDLE_LEAKY_MAX;
						continue;
					}
				} else if (bucket) {
					bucket--;
					continue;
				}
				break;
			case 1:
				apm_idle_done = 1;
				break;
			default: /* BIOS refused */
				break;
			}
		}
		if (original_pm_idle)
			original_pm_idle();
		else
			default_idle();
		local_irq_disable();
		jiffies_since_last_check = jiffies - last_jiffies;
		if (jiffies_since_last_check > idle_period)
			goto recalc;
	}

	if (apm_idle_done)
		apm_do_busy();

	local_irq_enable();
}

/**
 *	apm_power_off	-	ask the BIOS to power off
 *
 *	Handle the power off sequence. This is the one piece of code we
 *	will execute even on SMP machines. In order to deal with BIOS
 *	bugs we support real mode APM BIOS power off calls. We also make
 *	the SMP call on CPU0 as some systems will only honour this call
 *	on their first cpu.
 */

static void apm_power_off(void)
{
	unsigned char po_bios_call[] = {
		0xb8, 0x00, 0x10,	/* movw  $0x1000,ax  */
		0x8e, 0xd0,		/* movw  ax,ss       */
		0xbc, 0x00, 0xf0,	/* movw  $0xf000,sp  */
		0xb8, 0x07, 0x53,	/* movw  $0x5307,ax  */
		0xbb, 0x01, 0x00,	/* movw  $0x0001,bx  */
		0xb9, 0x03, 0x00,	/* movw  $0x0003,cx  */
		0xcd, 0x15		/* int   $0x15       */
	};

	/* Some bioses don't like being called from CPU != 0 */
	if (apm_info.realmode_power_off) {
		set_cpus_allowed_ptr(current, cpumask_of(0));
		machine_real_restart(po_bios_call, sizeof(po_bios_call));
	} else {
		(void)set_system_power_state(APM_STATE_OFF);
	}
}

#ifdef CONFIG_APM_DO_ENABLE

/**
 *	apm_enable_power_management - enable BIOS APM power management
 *	@enable: enable yes/no
 *
 *	Enable or disable the APM BIOS power services.
 */

static int apm_enable_power_management(int enable)
{
	u32 eax;
	int err;

	if ((enable == 0) && (apm_info.bios.flags & APM_BIOS_DISENGAGED))
		return APM_NOT_ENGAGED;
	if (apm_bios_call_simple(APM_FUNC_ENABLE_PM, APM_DEVICE_BALL,
				 enable, &eax, &err))
		return err;
	if (enable)
		apm_info.bios.flags &= ~APM_BIOS_DISABLED;
	else
		apm_info.bios.flags |= APM_BIOS_DISABLED;
	return APM_SUCCESS;
}
#endif

/**
 *	apm_get_power_status	-	get current power state
 *	@status: returned status
 *	@bat: battery info
 *	@life: estimated life
 *
 *	Obtain the current power status from the APM BIOS. We return a
 *	status which gives the rough battery status, and current power
 *	source. The bat value returned give an estimate as a percentage
 *	of life and a status value for the battery. The estimated life
 *	if reported is a lifetime in secodnds/minutes at current powwer
 *	consumption.
 */

static int apm_get_power_status(u_short *status, u_short *bat, u_short *life)
{
	struct apm_bios_call call;

	call.func = APM_FUNC_GET_STATUS;
	call.ebx = APM_DEVICE_ALL;
	call.ecx = 0;

	if (apm_info.get_power_status_broken)
		return APM_32_UNSUPPORTED;
	if (apm_bios_call(&call))
		return call.err;
	*status = call.ebx;
	*bat = call.ecx;
	if (apm_info.get_power_status_swabinminutes) {
		*life = swab16((u16)call.edx);
		*life |= 0x8000;
	} else
		*life = call.edx;
	return APM_SUCCESS;
}


/**
 *	apm_engage_power_management	-	enable PM on a device
 *	@device: identity of device
 *	@enable: on/off
 *
 *	Activate or deactive power management on either a specific device
 *	or the entire system (%APM_DEVICE_ALL).
 */

static int apm_engage_power_management(u_short device, int enable)
{
	u32 eax;
	int err;

	if ((enable == 0) && (device == APM_DEVICE_ALL)
	    && (apm_info.bios.flags & APM_BIOS_DISABLED))
		return APM_DISABLED;
	if (apm_bios_call_simple(APM_FUNC_ENGAGE_PM, device, enable,
				 &eax, &err))
		return err;
	if (device == APM_DEVICE_ALL) {
		if (enable)
			apm_info.bios.flags &= ~APM_BIOS_DISENGAGED;
		else
			apm_info.bios.flags |= APM_BIOS_DISENGAGED;
	}
	return APM_SUCCESS;
}

#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)

/**
 *	apm_console_blank	-	blank the display
 *	@blank: on/off
 *
 *	Attempt to blank the console, firstly by blanking just video device
 *	zero, and if that fails (some BIOSes don't support it) then it blanks
 *	all video devices. Typically the BIOS will do laptop backlight and
 *	monitor powerdown for us.
 */

static int apm_console_blank(int blank)
{
	int error = APM_NOT_ENGAGED; /* silence gcc */
	int i;
	u_short state;
	static const u_short dev[3] = { 0x100, 0x1FF, 0x101 };

	state = blank ? APM_STATE_STANDBY : APM_STATE_READY;

	for (i = 0; i < ARRAY_SIZE(dev); i++) {
		error = set_power_state(dev[i], state);

		if ((error == APM_SUCCESS) || (error == APM_NO_ERROR))
			return 1;

		if (error == APM_NOT_ENGAGED)
			break;
	}

	if (error == APM_NOT_ENGAGED) {
		static int tried;
		int eng_error;
		if (tried++ == 0) {
			eng_error = apm_engage_power_management(APM_DEVICE_ALL, 1);
			if (eng_error) {
				apm_error("set display", error);
				apm_error("engage interface", eng_error);
				return 0;
			} else
				return apm_console_blank(blank);
		}
	}
	apm_error("set display", error);
	return 0;
}
#endif

static int queue_empty(struct apm_user *as)
{
	return as->event_head == as->event_tail;
}

static apm_event_t get_queued_event(struct apm_user *as)
{
	if (++as->event_tail >= APM_MAX_EVENTS)
		as->event_tail = 0;
	return as->events[as->event_tail];
}

static void queue_event(apm_event_t event, struct apm_user *sender)
{
	struct apm_user *as;

	spin_lock(&user_list_lock);
	if (user_list == NULL)
		goto out;
	for (as = user_list; as != NULL; as = as->next) {
		if ((as == sender) || (!as->reader))
			continue;
		if (++as->event_head >= APM_MAX_EVENTS)
			as->event_head = 0;

		if (as->event_head == as->event_tail) {
			static int notified;

			if (notified++ == 0)
			    printk(KERN_ERR "apm: an event queue overflowed\n");
			if (++as->event_tail >= APM_MAX_EVENTS)
				as->event_tail = 0;
		}
		as->events[as->event_head] = event;
		if (!as->suser || !as->writer)
			continue;
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_pending++;
			suspends_pending++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_pending++;
			standbys_pending++;
			break;
		}
	}
	wake_up_interruptible(&apm_waitqueue);
out:
	spin_unlock(&user_list_lock);
}

static void reinit_timer(void)
{
#ifdef INIT_TIMER_AFTER_SUSPEND
	unsigned long flags;

	raw_spin_lock_irqsave(&i8253_lock, flags);
	/* set the clock to HZ */
	outb_pit(0x34, PIT_MODE);		/* binary, mode 2, LSB/MSB, ch 0 */
	udelay(10);
	outb_pit(LATCH & 0xff, PIT_CH0);	/* LSB */
	udelay(10);
	outb_pit(LATCH >> 8, PIT_CH0);	/* MSB */
	udelay(10);
	raw_spin_unlock_irqrestore(&i8253_lock, flags);
#endif
}

static int suspend(int vetoable)
{
	int err;
	struct apm_user	*as;

	dpm_suspend_start(PMSG_SUSPEND);

	dpm_suspend_noirq(PMSG_SUSPEND);

	local_irq_disable();
	sysdev_suspend(PMSG_SUSPEND);

	local_irq_enable();

	save_processor_state();
	err = set_system_power_state(APM_STATE_SUSPEND);
	ignore_normal_resume = 1;
	restore_processor_state();

	local_irq_disable();
	reinit_timer();

	if (err == APM_NO_ERROR)
		err = APM_SUCCESS;
	if (err != APM_SUCCESS)
		apm_error("suspend", err);
	err = (err == APM_SUCCESS) ? 0 : -EIO;

	sysdev_resume();
	local_irq_enable();

	dpm_resume_noirq(PMSG_RESUME);

	dpm_resume_end(PMSG_RESUME);
	queue_event(APM_NORMAL_RESUME, NULL);
	spin_lock(&user_list_lock);
	for (as = user_list; as != NULL; as = as->next) {
		as->suspend_wait = 0;
		as->suspend_result = err;
	}
	spin_unlock(&user_list_lock);
	wake_up_interruptible(&apm_suspend_waitqueue);
	return err;
}

static void standby(void)
{
	int err;

	dpm_suspend_noirq(PMSG_SUSPEND);

	local_irq_disable();
	sysdev_suspend(PMSG_SUSPEND);
	local_irq_enable();

	err = set_system_power_state(APM_STATE_STANDBY);
	if ((err != APM_SUCCESS) && (err != APM_NO_ERROR))
		apm_error("standby", err);

	local_irq_disable();
	sysdev_resume();
	local_irq_enable();

	dpm_resume_noirq(PMSG_RESUME);
}

static apm_event_t get_event(void)
{
	int error;
	apm_event_t event = APM_NO_EVENTS; /* silence gcc */
	apm_eventinfo_t	info;

	static int notified;

	/* we don't use the eventinfo */
	error = apm_get_event(&event, &info);
	if (error == APM_SUCCESS)
		return event;

	if ((error != APM_NO_EVENTS) && (notified++ == 0))
		apm_error("get_event", error);

	return 0;
}

static void check_events(void)
{
	apm_event_t event;
	static unsigned long last_resume;
	static int ignore_bounce;

	while ((event = get_event()) != 0) {
		if (debug) {
			if (event <= NR_APM_EVENT_NAME)
				printk(KERN_DEBUG "apm: received %s notify\n",
				       apm_event_name[event - 1]);
			else
				printk(KERN_DEBUG "apm: received unknown "
				       "event 0x%02x\n", event);
		}
		if (ignore_bounce
		    && (time_after(jiffies, last_resume + bounce_interval)))
			ignore_bounce = 0;

		switch (event) {
		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			queue_event(event, NULL);
			if (standbys_pending <= 0)
				standby();
			break;

		case APM_USER_SUSPEND:
#ifdef CONFIG_APM_IGNORE_USER_SUSPEND
			if (apm_info.connection_version > 0x100)
				set_system_power_state(APM_STATE_REJECT);
			break;
#endif
		case APM_SYS_SUSPEND:
			if (ignore_bounce) {
				if (apm_info.connection_version > 0x100)
					set_system_power_state(APM_STATE_REJECT);
				break;
			}
			/*
			 * If we are already processing a SUSPEND,
			 * then further SUSPEND events from the BIOS
			 * will be ignored.  We also return here to
			 * cope with the fact that the Thinkpads keep
			 * sending a SUSPEND event until something else
			 * happens!
			 */
			if (ignore_sys_suspend)
				return;
			ignore_sys_suspend = 1;
			queue_event(event, NULL);
			if (suspends_pending <= 0)
				(void) suspend(1);
			break;

		case APM_NORMAL_RESUME:
		case APM_CRITICAL_RESUME:
		case APM_STANDBY_RESUME:
			ignore_sys_suspend = 0;
			last_resume = jiffies;
			ignore_bounce = 1;
			if ((event != APM_NORMAL_RESUME)
			    || (ignore_normal_resume == 0)) {
				dpm_resume_end(PMSG_RESUME);
				queue_event(event, NULL);
			}
			ignore_normal_resume = 0;
			break;

		case APM_CAPABILITY_CHANGE:
		case APM_LOW_BATTERY:
		case APM_POWER_STATUS_CHANGE:
			queue_event(event, NULL);
			/* If needed, notify drivers here */
			break;

		case APM_UPDATE_TIME:
			break;

		case APM_CRITICAL_SUSPEND:
			/*
			 * We are not allowed to reject a critical suspend.
			 */
			(void)suspend(0);
			break;
		}
	}
}

static void apm_event_handler(void)
{
	static int pending_count = 4;
	int err;

	if ((standbys_pending > 0) || (suspends_pending > 0)) {
		if ((apm_info.connection_version > 0x100) &&
		    (pending_count-- <= 0)) {
			pending_count = 4;
			if (debug)
				printk(KERN_DEBUG "apm: setting state busy\n");
			err = set_system_power_state(APM_STATE_BUSY);
			if (err)
				apm_error("busy", err);
		}
	} else
		pending_count = 4;
	check_events();
}

/*
 * This is the APM thread main loop.
 */

static void apm_mainloop(void)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&apm_waitqueue, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		schedule_timeout(APM_CHECK_TIMEOUT);
		if (kthread_should_stop())
			break;
		/*
		 * Ok, check all events, check for idle (and mark us sleeping
		 * so as not to count towards the load average)..
		 */
		set_current_state(TASK_INTERRUPTIBLE);
		apm_event_handler();
	}
	remove_wait_queue(&apm_waitqueue, &wait);
}

static int check_apm_user(struct apm_user *as, const char *func)
{
	if (as == NULL || as->magic != APM_BIOS_MAGIC) {
		printk(KERN_ERR "apm: %s passed bad filp\n", func);
		return 1;
	}
	return 0;
}

static ssize_t do_read(struct file *fp, char __user *buf, size_t count, loff_t *ppos)
{
	struct apm_user *as;
	int i;
	apm_event_t event;

	as = fp->private_data;
	if (check_apm_user(as, "read"))
		return -EIO;
	if ((int)count < sizeof(apm_event_t))
		return -EINVAL;
	if ((queue_empty(as)) && (fp->f_flags & O_NONBLOCK))
		return -EAGAIN;
	wait_event_interruptible(apm_waitqueue, !queue_empty(as));
	i = count;
	while ((i >= sizeof(event)) && !queue_empty(as)) {
		event = get_queued_event(as);
		if (copy_to_user(buf, &event, sizeof(event))) {
			if (i < count)
				break;
			return -EFAULT;
		}
		switch (event) {
		case APM_SYS_SUSPEND:
		case APM_USER_SUSPEND:
			as->suspends_read++;
			break;

		case APM_SYS_STANDBY:
		case APM_USER_STANDBY:
			as->standbys_read++;
			break;
		}
		buf += sizeof(event);
		i -= sizeof(event);
	}
	if (i < count)
		return count - i;
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

static unsigned int do_poll(struct file *fp, poll_table *wait)
{
	struct apm_user *as;

	as = fp->private_data;
	if (check_apm_user(as, "poll"))
		return 0;
	poll_wait(fp, &apm_waitqueue, wait);
	if (!queue_empty(as))
		return POLLIN | POLLRDNORM;
	return 0;
}

static long do_ioctl(struct file *filp, u_int cmd, u_long arg)
{
	struct apm_user *as;
	int ret;

	as = filp->private_data;
	if (check_apm_user(as, "ioctl"))
		return -EIO;
	if (!as->suser || !as->writer)
		return -EPERM;
	switch (cmd) {
	case APM_IOC_STANDBY:
		mutex_lock(&apm_mutex);
		if (as->standbys_read > 0) {
			as->standbys_read--;
			as->standbys_pending--;
			standbys_pending--;
		} else
			queue_event(APM_USER_STANDBY, as);
		if (standbys_pending <= 0)
			standby();
		mutex_unlock(&apm_mutex);
		break;
	case APM_IOC_SUSPEND:
		mutex_lock(&apm_mutex);
		if (as->suspends_read > 0) {
			as->suspends_read--;
			as->suspends_pending--;
			suspends_pending--;
		} else
			queue_event(APM_USER_SUSPEND, as);
		if (suspends_pending <= 0) {
			ret = suspend(1);
			mutex_unlock(&apm_mutex);
		} else {
			as->suspend_wait = 1;
			mutex_unlock(&apm_mutex);
			wait_event_interruptible(apm_suspend_waitqueue,
					as->suspend_wait == 0);
			ret = as->suspend_result;
		}
		return ret;
	default:
		return -ENOTTY;
	}
	return 0;
}

static int do_release(struct inode *inode, struct file *filp)
{
	struct apm_user *as;

	as = filp->private_data;
	if (check_apm_user(as, "release"))
		return 0;
	filp->private_data = NULL;
	if (as->standbys_pending > 0) {
		standbys_pending -= as->standbys_pending;
		if (standbys_pending <= 0)
			standby();
	}
	if (as->suspends_pending > 0) {
		suspends_pending -= as->suspends_pending;
		if (suspends_pending <= 0)
			(void) suspend(1);
	}
	spin_lock(&user_list_lock);
	if (user_list == as)
		user_list = as->next;
	else {
		struct apm_user *as1;

		for (as1 = user_list;
		     (as1 != NULL) && (as1->next != as);
		     as1 = as1->next)
			;
		if (as1 == NULL)
			printk(KERN_ERR "apm: filp not in user list\n");
		else
			as1->next = as->next;
	}
	spin_unlock(&user_list_lock);
	kfree(as);
	return 0;
}

static int do_open(struct inode *inode, struct file *filp)
{
	struct apm_user *as;

	as = kmalloc(sizeof(*as), GFP_KERNEL);
	if (as == NULL) {
		printk(KERN_ERR "apm: cannot allocate struct of size %d bytes\n",
		       sizeof(*as));
		return -ENOMEM;
	}
	as->magic = APM_BIOS_MAGIC;
	as->event_tail = as->event_head = 0;
	as->suspends_pending = as->standbys_pending = 0;
	as->suspends_read = as->standbys_read = 0;
	as->suser = capable(CAP_SYS_ADMIN);
	as->writer = (filp->f_mode & FMODE_WRITE) == FMODE_WRITE;
	as->reader = (filp->f_mode & FMODE_READ) == FMODE_READ;
	spin_lock(&user_list_lock);
	as->next = user_list;
	user_list = as;
	spin_unlock(&user_list_lock);
	filp->private_data = as;
	return 0;
}

static int proc_apm_show(struct seq_file *m, void *v)
{
	unsigned short	bx;
	unsigned short	cx;
	unsigned short	dx;
	int		error;
	unsigned short  ac_line_status = 0xff;
	unsigned short  battery_status = 0xff;
	unsigned short  battery_flag   = 0xff;
	int		percentage     = -1;
	int             time_units     = -1;
	char            *units         = "?";

	if ((num_online_cpus() == 1) &&
	    !(error = apm_get_power_status(&bx, &cx, &dx))) {
		ac_line_status = (bx >> 8) & 0xff;
		battery_status = bx & 0xff;
		if ((cx & 0xff) != 0xff)
			percentage = cx & 0xff;

		if (apm_info.connection_version > 0x100) {
			battery_flag = (cx >> 8) & 0xff;
			if (dx != 0xffff) {
				units = (dx & 0x8000) ? "min" : "sec";
				time_units = dx & 0x7fff;
			}
		}
	}
	/* Arguments, with symbols from linux/apm_bios.h.  Information is
	   from the Get Power Status (0x0a) call unless otherwise noted.

	   0) Linux driver version (this will change if format changes)
	   1) APM BIOS Version.  Usually 1.0, 1.1 or 1.2.
	   2) APM flags from APM Installation Check (0x00):
	      bit 0: APM_16_BIT_SUPPORT
	      bit 1: APM_32_BIT_SUPPORT
	      bit 2: APM_IDLE_SLOWS_CLOCK
	      bit 3: APM_BIOS_DISABLED
	      bit 4: APM_BIOS_DISENGAGED
	   3) AC line status
	      0x00: Off-line
	      0x01: On-line
	      0x02: On backup power (BIOS >= 1.1 only)
	      0xff: Unknown
	   4) Battery status
	      0x00: High
	      0x01: Low
	      0x02: Critical
	      0x03: Charging
	      0x04: Selected battery not present (BIOS >= 1.2 only)
	      0xff: Unknown
	   5) Battery flag
	      bit 0: High
	      bit 1: Low
	      bit 2: Critical
	      bit 3: Charging
	      bit 7: No system battery
	      0xff: Unknown
	   6) Remaining battery life (percentage of charge):
	      0-100: valid
	      -1: Unknown
	   7) Remaining battery life (time units):
	      Number of remaining minutes or seconds
	      -1: Unknown
	   8) min = minutes; sec = seconds */

	seq_printf(m, "%s %d.%d 0x%02x 0x%02x 0x%02x 0x%02x %d%% %d %s\n",
		   driver_version,
		   (apm_info.bios.version >> 8) & 0xff,
		   apm_info.bios.version & 0xff,
		   apm_info.bios.flags,
		   ac_line_status,
		   battery_status,
		   battery_flag,
		   percentage,
		   time_units,
		   units);
	return 0;
}

static int proc_apm_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_apm_show, NULL);
}

static const struct file_operations apm_file_ops = {
	.owner		= THIS_MODULE,
	.open		= proc_apm_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int apm(void *unused)
{
	unsigned short	bx;
	unsigned short	cx;
	unsigned short	dx;
	int		error;
	char 		*power_stat;
	char 		*bat_stat;

	/* 2002/08/01 - WT
	 * This is to avoid random crashes at boot time during initialization
	 * on SMP systems in case of "apm=power-off" mode. Seen on ASUS A7M266D.
	 * Some bioses don't like being called from CPU != 0.
	 * Method suggested by Ingo Molnar.
	 */
	set_cpus_allowed_ptr(current, cpumask_of(0));
	BUG_ON(smp_processor_id() != 0);

	if (apm_info.connection_version == 0) {
		apm_info.connection_version = apm_info.bios.version;
		if (apm_info.connection_version > 0x100) {
			/*
			 * We only support BIOSs up to version 1.2
			 */
			if (apm_info.connection_version > 0x0102)
				apm_info.connection_version = 0x0102;
			error = apm_driver_version(&apm_info.connection_version);
			if (error != APM_SUCCESS) {
				apm_error("driver version", error);
				/* Fall back to an APM 1.0 connection. */
				apm_info.connection_version = 0x100;
			}
		}
	}

	if (debug)
		printk(KERN_INFO "apm: Connection version %d.%d\n",
			(apm_info.connection_version >> 8) & 0xff,
			apm_info.connection_version & 0xff);

#ifdef CONFIG_APM_DO_ENABLE
	if (apm_info.bios.flags & APM_BIOS_DISABLED) {
		/*
		 * This call causes my NEC UltraLite Versa 33/C to hang if it
		 * is booted with PM disabled but not in the docking station.
		 * Unfortunate ...
		 */
		error = apm_enable_power_management(1);
		if (error) {
			apm_error("enable power management", error);
			return -1;
		}
	}
#endif

	if ((apm_info.bios.flags & APM_BIOS_DISENGAGED)
	    && (apm_info.connection_version > 0x0100)) {
		error = apm_engage_power_management(APM_DEVICE_ALL, 1);
		if (error) {
			apm_error("engage power management", error);
			return -1;
		}
	}

	if (debug && (num_online_cpus() == 1 || smp)) {
		error = apm_get_power_status(&bx, &cx, &dx);
		if (error)
			printk(KERN_INFO "apm: power status not available\n");
		else {
			switch ((bx >> 8) & 0xff) {
			case 0:
				power_stat = "off line";
				break;
			case 1:
				power_stat = "on line";
				break;
			case 2:
				power_stat = "on backup power";
				break;
			default:
				power_stat = "unknown";
				break;
			}
			switch (bx & 0xff) {
			case 0:
				bat_stat = "high";
				break;
			case 1:
				bat_stat = "low";
				break;
			case 2:
				bat_stat = "critical";
				break;
			case 3:
				bat_stat = "charging";
				break;
			default:
				bat_stat = "unknown";
				break;
			}
			printk(KERN_INFO
			       "apm: AC %s, battery status %s, battery life ",
			       power_stat, bat_stat);
			if ((cx & 0xff) == 0xff)
				printk("unknown\n");
			else
				printk("%d%%\n", cx & 0xff);
			if (apm_info.connection_version > 0x100) {
				printk(KERN_INFO
				       "apm: battery flag 0x%02x, battery life ",
				       (cx >> 8) & 0xff);
				if (dx == 0xffff)
					printk("unknown\n");
				else
					printk("%d %s\n", dx & 0x7fff,
					       (dx & 0x8000) ?
					       "minutes" : "seconds");
			}
		}
	}

	/* Install our power off handler.. */
	if (power_off)
		pm_power_off = apm_power_off;

	if (num_online_cpus() == 1 || smp) {
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
		console_blank_hook = apm_console_blank;
#endif
		apm_mainloop();
#if defined(CONFIG_APM_DISPLAY_BLANK) && defined(CONFIG_VT)
		console_blank_hook = NULL;
#endif
	}

	return 0;
}

#ifndef MODULE
static int __init apm_setup(char *str)
{
	int invert;

	while ((str != NULL) && (*str != '\0')) {
		if (strncmp(str, "off", 3) == 0)
			apm_disabled = 1;
		if (strncmp(str, "on", 2) == 0)
			apm_disabled = 0;
		if ((strncmp(str, "bounce-interval=", 16) == 0) ||
		    (strncmp(str, "bounce_interval=", 16) == 0))
			bounce_interval = simple_strtol(str + 16, NULL, 0);
		if ((strncmp(str, "idle-threshold=", 15) == 0) ||
		    (strncmp(str, "idle_threshold=", 15) == 0))
			idle_threshold = simple_strtol(str + 15, NULL, 0);
		if ((strncmp(str, "idle-period=", 12) == 0) ||
		    (strncmp(str, "idle_period=", 12) == 0))
			idle_period = simple_strtol(str + 12, NULL, 0);
		invert = (strncmp(str, "no-", 3) == 0) ||
			(strncmp(str, "no_", 3) == 0);
		if (invert)
			str += 3;
		if (strncmp(str, "debug", 5) == 0)
			debug = !invert;
		if ((strncmp(str, "power-off", 9) == 0) ||
		    (strncmp(str, "power_off", 9) == 0))
			power_off = !invert;
		if (strncmp(str, "smp", 3) == 0) {
			smp = !invert;
			idle_threshold = 100;
		}
		if ((strncmp(str, "allow-ints", 10) == 0) ||
		    (strncmp(str, "allow_ints", 10) == 0))
			apm_info.allow_ints = !invert;
		if ((strncmp(str, "broken-psr", 10) == 0) ||
		    (strncmp(str, "broken_psr", 10) == 0))
			apm_info.get_power_status_broken = !invert;
		if ((strncmp(str, "realmode-power-off", 18) == 0) ||
		    (strncmp(str, "realmode_power_off", 18) == 0))
			apm_info.realmode_power_off = !invert;
		str = strchr(str, ',');
		if (str != NULL)
			str += strspn(str, ", \t");
	}
	return 1;
}

__setup("apm=", apm_setup);
#endif

static const struct file_operations apm_bios_fops = {
	.owner		= THIS_MODULE,
	.read		= do_read,
	.poll		= do_poll,
	.unlocked_ioctl	= do_ioctl,
	.open		= do_open,
	.release	= do_release,
};

static struct miscdevice apm_device = {
	APM_MINOR_DEV,
	"apm_bios",
	&apm_bios_fops
};


/* Simple "print if true" callback */
static int __init print_if_true(const struct dmi_system_id *d)
{
	printk("%s\n", d->ident);
	return 0;
}

/*
 * Some Bioses enable the PS/2 mouse (touchpad) at resume, even if it was
 * disabled before the suspend. Linux used to get terribly confused by that.
 */
static int __init broken_ps2_resume(const struct dmi_system_id *d)
{
	printk(KERN_INFO "%s machine detected. Mousepad Resume Bug "
	       "workaround hopefully not needed.\n", d->ident);
	return 0;
}

/* Some bioses have a broken protected mode poweroff and need to use realmode */
static int __init set_realmode_power_off(const struct dmi_system_id *d)
{
	if (apm_info.realmode_power_off == 0) {
		apm_info.realmode_power_off = 1;
		printk(KERN_INFO "%s bios detected. "
		       "Using realmode poweroff only.\n", d->ident);
	}
	return 0;
}

/* Some laptops require interrupts to be enabled during APM calls */
static int __init set_apm_ints(const struct dmi_system_id *d)
{
	if (apm_info.allow_ints == 0) {
		apm_info.allow_ints = 1;
		printk(KERN_INFO "%s machine detected. "
		       "Enabling interrupts during APM calls.\n", d->ident);
	}
	return 0;
}

/* Some APM bioses corrupt memory or just plain do not work */
static int __init apm_is_horked(const struct dmi_system_id *d)
{
	if (apm_info.disabled == 0) {
		apm_info.disabled = 1;
		printk(KERN_INFO "%s machine detected. "
		       "Disabling APM.\n", d->ident);
	}
	return 0;
}

static int __init apm_is_horked_d850md(const struct dmi_system_id *d)
{
	if (apm_info.disabled == 0) {
		apm_info.disabled = 1;
		printk(KERN_INFO "%s machine detected. "
		       "Disabling APM.\n", d->ident);
		printk(KERN_INFO "This bug is fixed in bios P15 which is available for\n");
		printk(KERN_INFO "download from support.intel.com\n");
	}
	return 0;
}

/* Some APM bioses hang on APM idle calls */
static int __init apm_likes_to_melt(const struct dmi_system_id *d)
{
	if (apm_info.forbid_idle == 0) {
		apm_info.forbid_idle = 1;
		printk(KERN_INFO "%s machine detected. "
		       "Disabling APM idle calls.\n", d->ident);
	}
	return 0;
}

/*
 *  Check for clue free BIOS implementations who use
 *  the following QA technique
 *
 *      [ Write BIOS Code ]<------
 *               |                ^
 *      < Does it Compile >----N--
 *               |Y               ^
 *	< Does it Boot Win98 >-N--
 *               |Y
 *           [Ship It]
 *
 *	Phoenix A04  08/24/2000 is known bad (Dell Inspiron 5000e)
 *	Phoenix A07  09/29/2000 is known good (Dell Inspiron 5000)
 */
static int __init broken_apm_power(const struct dmi_system_id *d)
{
	apm_info.get_power_status_broken = 1;
	printk(KERN_WARNING "BIOS strings suggest APM bugs, "
	       "disabling power status reporting.\n");
	return 0;
}

/*
 * This bios swaps the APM minute reporting bytes over (Many sony laptops
 * have this problem).
 */
static int __init swab_apm_power_in_minutes(const struct dmi_system_id *d)
{
	apm_info.get_power_status_swabinminutes = 1;
	printk(KERN_WARNING "BIOS strings suggest APM reports battery life "
	       "in minutes and wrong byte order.\n");
	return 0;
}

static struct dmi_system_id __initdata apm_dmi_table[] = {
	{
		print_if_true,
		KERN_WARNING "IBM T23 - BIOS 1.03b+ and controller firmware 1.02+ may be needed for Linux APM.",
		{	DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_BIOS_VERSION, "1AET38WW (1.01b)"), },
	},
	{	/* Handle problems with APM on the C600 */
		broken_ps2_resume, "Dell Latitude C600",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude C600"), },
	},
	{	/* Allow interrupts during suspend on Dell Latitude laptops*/
		set_apm_ints, "Dell Latitude",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Latitude C510"), }
	},
	{	/* APM crashes */
		apm_is_horked, "Dell Inspiron 2500",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 2500"),
			DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "A11"), },
	},
	{	/* Allow interrupts during suspend on Dell Inspiron laptops*/
		set_apm_ints, "Dell Inspiron", {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 4000"), },
	},
	{	/* Handle problems with APM on Inspiron 5000e */
		broken_apm_power, "Dell Inspiron 5000e",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "A04"),
			DMI_MATCH(DMI_BIOS_DATE, "08/24/2000"), },
	},
	{	/* Handle problems with APM on Inspiron 2500 */
		broken_apm_power, "Dell Inspiron 2500",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "A12"),
			DMI_MATCH(DMI_BIOS_DATE, "02/04/2002"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Dell Dimension 4100",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "XPS-Z"),
			DMI_MATCH(DMI_BIOS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_BIOS_VERSION, "A11"), },
	},
	{	/* Allow interrupts during suspend on Compaq Laptops*/
		set_apm_ints, "Compaq 12XL125",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Compaq"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Compaq PC"),
			DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "4.06"), },
	},
	{	/* Allow interrupts during APM or the clock goes slow */
		set_apm_ints, "ASUSTeK",
		{	DMI_MATCH(DMI_SYS_VENDOR, "ASUSTeK Computer Inc."),
			DMI_MATCH(DMI_PRODUCT_NAME, "L8400K series Notebook PC"), },
	},
	{	/* APM blows on shutdown */
		apm_is_horked, "ABIT KX7-333[R]",
		{	DMI_MATCH(DMI_BOARD_VENDOR, "ABIT"),
			DMI_MATCH(DMI_BOARD_NAME, "VT8367-8233A (KX7-333[R])"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Trigem Delhi3",
		{	DMI_MATCH(DMI_SYS_VENDOR, "TriGem Computer, Inc"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Delhi3"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Fujitsu-Siemens",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "hoenix/FUJITSU SIEMENS"),
			DMI_MATCH(DMI_BIOS_VERSION, "Version1.01"), },
	},
	{	/* APM crashes */
		apm_is_horked_d850md, "Intel D850MD",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_BIOS_VERSION, "MV85010A.86A.0016.P07.0201251536"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Intel D810EMO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_BIOS_VERSION, "MO81010A.86A.0008.P04.0004170800"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Dell XPS-Z",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Intel Corp."),
			DMI_MATCH(DMI_BIOS_VERSION, "A11"),
			DMI_MATCH(DMI_PRODUCT_NAME, "XPS-Z"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Sharp PC-PJ/AX",
		{	DMI_MATCH(DMI_SYS_VENDOR, "SHARP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PC-PJ/AX"),
			DMI_MATCH(DMI_BIOS_VENDOR, "SystemSoft"),
			DMI_MATCH(DMI_BIOS_VERSION, "Version R2.08"), },
	},
	{	/* APM crashes */
		apm_is_horked, "Dell Inspiron 2500",
		{	DMI_MATCH(DMI_SYS_VENDOR, "Dell Computer Corporation"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Inspiron 2500"),
			DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "A11"), },
	},
	{	/* APM idle hangs */
		apm_likes_to_melt, "Jabil AMD",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "American Megatrends Inc."),
			DMI_MATCH(DMI_BIOS_VERSION, "0AASNP06"), },
	},
	{	/* APM idle hangs */
		apm_likes_to_melt, "AMI Bios",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "American Megatrends Inc."),
			DMI_MATCH(DMI_BIOS_VERSION, "0AASNP05"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-N505X(DE) */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0206H"),
			DMI_MATCH(DMI_BIOS_DATE, "08/23/99"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-N505VX */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "W2K06H0"),
			DMI_MATCH(DMI_BIOS_DATE, "02/03/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-XG29 */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0117A0"),
			DMI_MATCH(DMI_BIOS_DATE, "04/25/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z600NE */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0121Z1"),
			DMI_MATCH(DMI_BIOS_DATE, "05/11/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z600NE */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "WME01Z1"),
			DMI_MATCH(DMI_BIOS_DATE, "08/11/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z600LEK(DE) */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0206Z3"),
			DMI_MATCH(DMI_BIOS_DATE, "12/25/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z505LS */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0203D0"),
			DMI_MATCH(DMI_BIOS_DATE, "05/12/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z505LS */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0203Z3"),
			DMI_MATCH(DMI_BIOS_DATE, "08/25/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-Z505LS (with updated BIOS) */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0209Z3"),
			DMI_MATCH(DMI_BIOS_DATE, "05/12/01"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-F104K */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0204K2"),
			DMI_MATCH(DMI_BIOS_DATE, "08/28/00"), },
	},

	{	/* Handle problems with APM on Sony Vaio PCG-C1VN/C1VE */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0208P1"),
			DMI_MATCH(DMI_BIOS_DATE, "11/09/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-C1VE */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "R0204P1"),
			DMI_MATCH(DMI_BIOS_DATE, "09/12/00"), },
	},
	{	/* Handle problems with APM on Sony Vaio PCG-C1VE */
		swab_apm_power_in_minutes, "Sony VAIO",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Phoenix Technologies LTD"),
			DMI_MATCH(DMI_BIOS_VERSION, "WXPO1Z3"),
			DMI_MATCH(DMI_BIOS_DATE, "10/26/01"), },
	},
	{	/* broken PM poweroff bios */
		set_realmode_power_off, "Award Software v4.60 PGMA",
		{	DMI_MATCH(DMI_BIOS_VENDOR, "Award Software International, Inc."),
			DMI_MATCH(DMI_BIOS_VERSION, "4.60 PGMA"),
			DMI_MATCH(DMI_BIOS_DATE, "134526184"), },
	},

	/* Generic per vendor APM settings  */

	{	/* Allow interrupts during suspend on IBM laptops */
		set_apm_ints, "IBM",
		{	DMI_MATCH(DMI_SYS_VENDOR, "IBM"), },
	},

	{ }
};

/*
 * Just start the APM thread. We do NOT want to do APM BIOS
 * calls from anything but the APM thread, if for no other reason
 * than the fact that we don't trust the APM BIOS. This way,
 * most common APM BIOS problems that lead to protection errors
 * etc will have at least some level of being contained...
 *
 * In short, if something bad happens, at least we have a choice
 * of just killing the apm thread..
 */
static int __init apm_init(void)
{
	struct desc_struct *gdt;
	int err;

	dmi_check_system(apm_dmi_table);

	if (apm_info.bios.version == 0 || paravirt_enabled() || machine_is_olpc()) {
		printk(KERN_INFO "apm: BIOS not found.\n");
		return -ENODEV;
	}
	printk(KERN_INFO
	       "apm: BIOS version %d.%d Flags 0x%02x (Driver version %s)\n",
	       ((apm_info.bios.version >> 8) & 0xff),
	       (apm_info.bios.version & 0xff),
	       apm_info.bios.flags,
	       driver_version);
	if ((apm_info.bios.flags & APM_32_BIT_SUPPORT) == 0) {
		printk(KERN_INFO "apm: no 32 bit BIOS support\n");
		return -ENODEV;
	}

	if (allow_ints)
		apm_info.allow_ints = 1;
	if (broken_psr)
		apm_info.get_power_status_broken = 1;
	if (realmode_power_off)
		apm_info.realmode_power_off = 1;
	/* User can override, but default is to trust DMI */
	if (apm_disabled != -1)
		apm_info.disabled = apm_disabled;

	/*
	 * Fix for the Compaq Contura 3/25c which reports BIOS version 0.1
	 * but is reportedly a 1.0 BIOS.
	 */
	if (apm_info.bios.version == 0x001)
		apm_info.bios.version = 0x100;

	/* BIOS < 1.2 doesn't set cseg_16_len */
	if (apm_info.bios.version < 0x102)
		apm_info.bios.cseg_16_len = 0; /* 64k */

	if (debug) {
		printk(KERN_INFO "apm: entry %x:%x cseg16 %x dseg %x",
			apm_info.bios.cseg, apm_info.bios.offset,
			apm_info.bios.cseg_16, apm_info.bios.dseg);
		if (apm_info.bios.version > 0x100)
			printk(" cseg len %x, dseg len %x",
				apm_info.bios.cseg_len,
				apm_info.bios.dseg_len);
		if (apm_info.bios.version > 0x101)
			printk(" cseg16 len %x", apm_info.bios.cseg_16_len);
		printk("\n");
	}

	if (apm_info.disabled) {
		printk(KERN_NOTICE "apm: disabled on user request.\n");
		return -ENODEV;
	}
	if ((num_online_cpus() > 1) && !power_off && !smp) {
		printk(KERN_NOTICE "apm: disabled - APM is not SMP safe.\n");
		apm_info.disabled = 1;
		return -ENODEV;
	}
	if (pm_flags & PM_ACPI) {
		printk(KERN_NOTICE "apm: overridden by ACPI.\n");
		apm_info.disabled = 1;
		return -ENODEV;
	}
	pm_flags |= PM_APM;

	/*
	 * Set up the long jump entry point to the APM BIOS, which is called
	 * from inline assembly.
	 */
	apm_bios_entry.offset = apm_info.bios.offset;
	apm_bios_entry.segment = APM_CS;

	/*
	 * The APM 1.1 BIOS is supposed to provide limit information that it
	 * recognizes.  Many machines do this correctly, but many others do
	 * not restrict themselves to their claimed limit.  When this happens,
	 * they will cause a segmentation violation in the kernel at boot time.
	 * Most BIOS's, however, will respect a 64k limit, so we use that.
	 *
	 * Note we only set APM segments on CPU zero, since we pin the APM
	 * code to that CPU.
	 */
	gdt = get_cpu_gdt_table(0);
	set_desc_base(&gdt[APM_CS >> 3],
		 (unsigned long)__va((unsigned long)apm_info.bios.cseg << 4));
	set_desc_base(&gdt[APM_CS_16 >> 3],
		 (unsigned long)__va((unsigned long)apm_info.bios.cseg_16 << 4));
	set_desc_base(&gdt[APM_DS >> 3],
		 (unsigned long)__va((unsigned long)apm_info.bios.dseg << 4));

	proc_create("apm", 0, NULL, &apm_file_ops);

	kapmd_task = kthread_create(apm, NULL, "kapmd");
	if (IS_ERR(kapmd_task)) {
		printk(KERN_ERR "apm: disabled - Unable to start kernel "
				"thread.\n");
		err = PTR_ERR(kapmd_task);
		kapmd_task = NULL;
		remove_proc_entry("apm", NULL);
		return err;
	}
	wake_up_process(kapmd_task);

	if (num_online_cpus() > 1 && !smp) {
		printk(KERN_NOTICE
		       "apm: disabled - APM is not SMP safe (power off active).\n");
		return 0;
	}

	/*
	 * Note we don't actually care if the misc_device cannot be registered.
	 * this driver can do its job without it, even if userspace can't
	 * control it.  just log the error
	 */
	if (misc_register(&apm_device))
		printk(KERN_WARNING "apm: Could not register misc device.\n");

	if (HZ != 100)
		idle_period = (idle_period * HZ) / 100;
	if (idle_threshold < 100) {
		original_pm_idle = pm_idle;
		pm_idle  = apm_cpu_idle;
		set_pm_idle = 1;
	}

	return 0;
}

static void __exit apm_exit(void)
{
	int error;

	if (set_pm_idle) {
		pm_idle = original_pm_idle;
		/*
		 * We are about to unload the current idle thread pm callback
		 * (pm_idle), Wait for all processors to update cached/local
		 * copies of pm_idle before proceeding.
		 */
		cpu_idle_wait();
	}
	if (((apm_info.bios.flags & APM_BIOS_DISENGAGED) == 0)
	    && (apm_info.connection_version > 0x0100)) {
		error = apm_engage_power_management(APM_DEVICE_ALL, 0);
		if (error)
			apm_error("disengage power management", error);
	}
	misc_deregister(&apm_device);
	remove_proc_entry("apm", NULL);
	if (power_off)
		pm_power_off = NULL;
	if (kapmd_task) {
		kthread_stop(kapmd_task);
		kapmd_task = NULL;
	}
	pm_flags &= ~PM_APM;
}

module_init(apm_init);
module_exit(apm_exit);

MODULE_AUTHOR("Stephen Rothwell");
MODULE_DESCRIPTION("Advanced Power Management");
MODULE_LICENSE("GPL");
module_param(debug, bool, 0644);
MODULE_PARM_DESC(debug, "Enable debug mode");
module_param(power_off, bool, 0444);
MODULE_PARM_DESC(power_off, "Enable power off");
module_param(bounce_interval, int, 0444);
MODULE_PARM_DESC(bounce_interval,
		"Set the number of ticks to ignore suspend bounces");
module_param(allow_ints, bool, 0444);
MODULE_PARM_DESC(allow_ints, "Allow interrupts during BIOS calls");
module_param(broken_psr, bool, 0444);
MODULE_PARM_DESC(broken_psr, "BIOS has a broken GetPowerStatus call");
module_param(realmode_power_off, bool, 0444);
MODULE_PARM_DESC(realmode_power_off,
		"Switch to real mode before powering off");
module_param(idle_threshold, int, 0444);
MODULE_PARM_DESC(idle_threshold,
	"System idle percentage above which to make APM BIOS idle calls");
module_param(idle_period, int, 0444);
MODULE_PARM_DESC(idle_period,
	"Period (in sec/100) over which to caculate the idle percentage");
module_param(smp, bool, 0444);
MODULE_PARM_DESC(smp,
	"Set this to enable APM use on an SMP platform. Use with caution on older systems");
MODULE_ALIAS_MISCDEV(APM_MINOR_DEV);
