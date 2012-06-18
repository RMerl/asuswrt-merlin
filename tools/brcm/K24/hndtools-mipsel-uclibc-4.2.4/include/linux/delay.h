#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

/*
 * Copyright (C) 1993 Linus Torvalds
 *
 * Delay routines, using a pre-computed "loops_per_jiffy" value.
 */

extern unsigned long loops_per_jiffy;

#include <linux/time.h>
#include <linux/sched.h>
#include <asm/delay.h>

/*
 * We define MAX_MSEC_OFFSET as the maximal value that can be accepted by
 * msecs_to_jiffies() without risking a multiply overflow. This function
 * returns MAX_JIFFY_OFFSET for arguments above those values.
 */

#if HZ <= 1000 && !(1000 % HZ)
#  define MAX_MSEC_OFFSET \
	(ULONG_MAX - (1000 / HZ) + 1)
#elif HZ > 1000 && !(HZ % 1000)
#  define MAX_MSEC_OFFSET \
	(ULONG_MAX / (HZ / 1000))
#else
#  define MAX_MSEC_OFFSET \
	((ULONG_MAX - 999) / HZ)
#endif


/*
 * Convert jiffies to milliseconds and back.
 *
 * Avoid unnecessary multiplications/divisions in the
 * two most common HZ cases:
 */
static inline unsigned int jiffies_to_msecs(const unsigned long j)
{
#if HZ <= 1000 && !(1000 % HZ)
	return (1000 / HZ) * j;
#elif HZ > 1000 && !(HZ % 1000)
	return (j + (HZ / 1000) - 1)/(HZ / 1000);
#else
	return (j * 1000) / HZ;
#endif
}

static inline unsigned int jiffies_to_usecs(const unsigned long j)
{
#if HZ <= 1000 && !(1000 % HZ)
	return (1000000 / HZ) * j;
#elif HZ > 1000 && !(HZ % 1000)
	return (j*1000 + (HZ - 1000))/(HZ / 1000);
#else
	return (j * 1000000) / HZ;
#endif
}

static inline unsigned long msecs_to_jiffies(const unsigned int m)
{
	if (MAX_MSEC_OFFSET < UINT_MAX && m > (unsigned int)MAX_MSEC_OFFSET)
		return MAX_JIFFY_OFFSET;
#if HZ <= 1000 && !(1000 % HZ)
	return ((unsigned long)m + (1000 / HZ) - 1) / (1000 / HZ);
#elif HZ > 1000 && !(HZ % 1000)
	return (unsigned long)m * (HZ / 1000);
#else
	return ((unsigned long)m * HZ + 999) / 1000;
#endif
}

static inline void msleep(unsigned long msecs)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(msecs) + 1);
}

static inline void ssleep(unsigned long secs)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout((HZ * secs) + 1);
}

/*
 * Using udelay() for intervals greater than a few milliseconds can
 * risk overflow for high loops_per_jiffy (high bogomips) machines. The
 * mdelay() provides a wrapper to prevent this.  For delays greater
 * than MAX_UDELAY_MS milliseconds, the wrapper is used.  Architecture
 * specific values can be defined in asm-???/delay.h as an override.
 * The 2nd mdelay() definition ensures GCC will optimize away the 
 * while loop for the common cases where n <= MAX_UDELAY_MS  --  Paul G.
 */

#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_MS	5
#endif

#ifdef notdef
#define mdelay(n) (\
	{unsigned long msec=(n); while (msec--) udelay(1000);})
#else
#define mdelay(n) (\
	(__builtin_constant_p(n) && (n)<=MAX_UDELAY_MS) ? udelay((n)*1000) : \
	({unsigned long msec=(n); while (msec--) udelay(1000);}))
#endif

#endif /* defined(_LINUX_DELAY_H) */
