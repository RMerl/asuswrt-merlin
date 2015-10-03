/*
 * LICENSE NOTICE.
 *
 * Use of the Microsoft Windows Rally Development Kit is covered under
 * the Microsoft Windows Rally Development Kit License Agreement,
 * which is provided within the Microsoft Windows Rally Development
 * Kit or at http://www.microsoft.com/whdc/rally/rallykit.mspx. If you
 * want a license from Microsoft to use the software in the Microsoft
 * Windows Rally Development Kit, you must (1) complete the designated
 * "licensee" information in the Windows Rally Development Kit License
 * Agreement, and (2) sign and return the Agreement AS IS to Microsoft
 * at the address provided in the Agreement.
 */

/*
 * Copyright (c) Microsoft Corporation 2005.  All rights reserved.
 * This software is provided with NO WARRANTY.
 */

#ifndef UTIL_H
#define UTIL_H

#define ETHERADDR_EQUALS(x, y) \
  (((x)->a[0] == (y)->a[0]) && \
   ((x)->a[1] == (y)->a[1]) && \
   ((x)->a[2] == (y)->a[2]) && \
   ((x)->a[3] == (y)->a[3]) && \
   ((x)->a[4] == (y)->a[4]) && \
   ((x)->a[5] == (y)->a[5]))

#define ETHERADDR_IS_BCAST(x) \
  (((x)->a[0] == 0xff) && \
   ((x)->a[1] == 0xff) && \
   ((x)->a[2] == 0xff) && \
   ((x)->a[3] == 0xff) && \
   ((x)->a[4] == 0xff) && \
   ((x)->a[5] == 0xff))

#define ETHERADDR_IS_MCAST(x) \
    (((x)->a[0] & 0x01) == 0x01)

#define ETHERADDR_IS_ZERO(x) \
  (((x)->a[0] == 0x00) && \
   ((x)->a[1] == 0x00) && \
   ((x)->a[2] == 0x00) && \
   ((x)->a[3] == 0x00) && \
   ((x)->a[4] == 0x00) && \
   ((x)->a[5] == 0x00))

static const etheraddr_t Etheraddr_broadcast =
(etheraddr_t){{0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static const etheraddr_t Etheraddr_null =
(etheraddr_t){{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

/* Helper functions */
#ifdef __GNUC__
/* if we have GCC then we can check the format strings & specify
   non-returning functions */
#define ATTRIB_FORMAT_PRINTF __attribute__ ((format (printf, 1, 2)))
#define ATTRIB_NORETURN __attribute__ ((noreturn))
#else
/* otherwise we don't bother */
#define ATTRIB_FORMAT_PRINTF
#define ATTRIB_NORETURN
#endif

extern void cpy_hton64(void* destination, void* source);

extern void die(const char *msg, ...) ATTRIB_FORMAT_PRINTF ATTRIB_NORETURN;

extern void warn(const char *msg, ...) ATTRIB_FORMAT_PRINTF;

extern void dbgprintf(const char *msg, ...) ATTRIB_FORMAT_PRINTF;

/* Pick a random number in interval [0, upperlimit] (inclusive) */
extern uint32_t random_uniform(uint32_t upperlimit);

/* wrap all called to malloc/free */
extern void *xmalloc(size_t nbytes);
extern void  xfree(void *p);

/* Convert the ASCII string starting at "src" to UCS-2 (little-endian) and
 * copy it into "dst", using no more than "dst_bytes".  If truncation is required,
 * ensure "dst" is safely terminated by a double-\0. */
extern void util_copy_ascii_to_ucs2(ucs2char_t *dst, size_t dst_bytes, const char *src);

/* From now on, die(), warn(), and dprintf() should go to syslog facility, not stderr */
extern void util_use_syslog(void);

/* macros */

#define ETHERADDR_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHERADDR_PRINT(e) (e)->a[0], (e)->a[1], (e)->a[2],\
                           (e)->a[3], (e)->a[4], (e)->a[5]

#define timeval_add_ms(tv, ms) \
do {						\
    (tv)->tv_usec += (ms) * 1000;		\
    while ((tv)->tv_usec > 1000000)		\
    {						\
	(tv)->tv_sec++;				\
	(tv)->tv_usec -= 1000000; 		\
    }						\
} while(0)

#define timeval_add_us(tv, us) \
do {						\
    (tv)->tv_usec += (us);			\
    while ((tv)->tv_usec > 1000000)		\
    {						\
	(tv)->tv_sec++;				\
	(tv)->tv_usec -= 1000000; 		\
    }						\
} while(0)

/* handy #defines for operations on timers, adapted from sys/time.h: */
#define bsd_timersub(a, b, result)					      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)

#define timerle(a, b) 						      \
  (((a)->tv_sec == (b)->tv_sec) ? 				      \
   ((a)->tv_usec <= (b)->tv_usec) : 				      \
   ((a)->tv_sec <= (b)->tv_sec))


#define TOPOMAX(a,b) (((a)>(b))? (a) : (b))
#define TOPOMIN(a,b) (((a)<(b))? (a) : (b))

/* handy macro to cancel and NULL-out a timer */
#define CANCEL(tmr) \
do {						\
    if (tmr)				\
    {						\
	event_cancel(tmr);			\
	tmr = NULL;				\
    }						\
} while (0)

#endif /* UTIL_H */
