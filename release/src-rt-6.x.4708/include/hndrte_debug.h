/*
 * HND Run Time Environment debug info area
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: hndrte_debug.h 342211 2012-07-02 02:23:04Z $
 */

#ifndef	_HNDRTE_DEBUG_H
#define	_HNDRTE_DEBUG_H

/* Magic number at a magic location to find HNDRTE_DEBUG pointers */
#define HNDRTE_DEBUG_PTR_PTR_MAGIC 0x50504244  	/* DBPP */

#ifndef _LANGUAGE_ASSEMBLY

/* Includes only when building dongle code */


#define NUM_EVENT_LOG_SETS 4

/* We use explicit sizes here since this gets included from different
 * systems.  The sizes must be the size of the creating system
 * (currently 32 bit ARM) since this is gleaned from  dump.
 */

/* Define pointers for use on other systems */
#define _HD_EVLOG_P	uint32
#define _HD_CONS_P	uint32
#define _HD_TRAP_P	uint32

typedef struct hndrte_debug {
	uint32	magic;
#define HNDRTE_DEBUG_MAGIC 0x47424544	/* 'DEBG' */

	uint32	version;		/* Debug struct version */
#define HNDRTE_DEBUG_VERSION 1

	uint32	fwid;			/* 4 bytes of fw info */
	char	epivers[32];

	_HD_TRAP_P trap_ptr;		/* trap_t data struct */
	_HD_CONS_P console;		/* Console  */

	uint32	ram_base;
	uint32	ram_size;

	uint32	rom_base;
	uint32	rom_size;

	_HD_EVLOG_P event_log_top;

} hndrte_debug_t;

/*
 * timeval_t and prstatus_t are copies of the Linux structures.
 * Included here because we need the definitions for the target processor
 * (32 bits) and not the definition on the host this is running on
 * (which could be 64 bits).
 */

typedef struct             {    /* Time value with microsecond resolution    */
	uint32 tv_sec;	/* Seconds                                   */
	uint32 tv_usec;	/* Microseconds                              */
} timeval_t;


/* Linux/ARM 32 prstatus for notes section */
typedef struct prstatus {
	  int32 si_signo; 	/* Signal number */
	  int32 si_code; 	/* Extra code */
	  int32 si_errno; 	/* Errno */
	  uint16 pr_cursig; 	/* Current signal.  */
	  uint16 unused;
	  uint32 pr_sigpend;	/* Set of pending signals.  */
	  uint32 pr_sighold;	/* Set of held signals.  */
	  uint32 pr_pid;
	  uint32 pr_ppid;
	  uint32 pr_pgrp;
	  uint32 pr_sid;
	  timeval_t pr_utime;	/* User time.  */
	  timeval_t pr_stime;	/* System time.  */
	  timeval_t pr_cutime;	/* Cumulative user time.  */
	  timeval_t pr_cstime;	/* Cumulative system time.  */
	  uint32 uregs[18];
	  int32 pr_fpvalid;	/* True if math copro being used.  */
} prstatus_t;

#ifdef __GNUC__
extern hndrte_debug_t hndrte_debug_info __attribute__ ((weak));
#endif /* __GNUC__ */

#endif /* !LANGUAGE_ASSEMBLY */

#endif /* _HNDRTE_DEBUG_H */
