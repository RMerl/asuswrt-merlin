#ifndef HWCLOCK_CLOCK_H
#define HWCLOCK_CLOCK_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "c.h"

struct clock_ops {
	char *interface_name;
	int (*get_permissions) (void);
	int (*read_hardware_clock) (struct tm * tm);
	int (*set_hardware_clock) (const struct tm * tm);
	int (*synchronize_to_clock_tick) (void);
};

extern struct clock_ops *probe_for_cmos_clock(void);
extern struct clock_ops *probe_for_rtc_clock(void);
extern struct clock_ops *probe_for_kd_clock(void);

typedef int bool;

/* hwclock.c */
extern char *progname;
extern int debug;
extern unsigned long epoch_option;
extern double time_diff(struct timeval subtrahend, struct timeval subtractor);
/* cmos.c */
extern void set_cmos_epoch(int ARCconsole, int SRM);
extern void set_cmos_access(int Jensen, int funky_toy);

/* rtc.c */
extern int get_epoch_rtc(unsigned long *epoch, int silent);
extern int set_epoch_rtc(unsigned long epoch);
extern char *rtc_dev_name;

#ifdef HAVE_LIBAUDIT
extern void hwaudit_exit(int status);
# define hwclock_exit(_status)	hwaudit_exit(_status)
#else
# define hwclock_exit(_status)	exit(_status)
#endif

#endif				/* HWCLOCK_CLOCK_H */
