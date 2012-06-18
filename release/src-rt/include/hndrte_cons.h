/*
 * Console support for hndrte.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: hndrte_cons.h,v 13.4 2009-10-08 21:49:58 Exp $
 */

#include <typedefs.h>

#if defined(RWL_DONGLE) || defined(UART_REFLECTOR)
/* For Dongle uart tranport max cmd len is 256 bytes + header length (16 bytes)
 *  In case of ASD commands we are not sure about how much is the command size
 *  To be on the safe side, input buf len CBUF_LEN is increased to max (512) bytes.
 */
#define RWL_MAX_DATA_LEN 	(512 + 8)	/* allow some extra bytes for '/n' termination */
#define CBUF_LEN	(RWL_MAX_DATA_LEN + 64)  /* allow 64 bytes for header ("rwl...") */
#else
#define CBUF_LEN	(128)
#endif /* RWL_DONGLE || UART_REFLECTOR */

#ifdef BCMDBG
#define LOG_BUF_LEN	(16 * 1024)
#else
#define LOG_BUF_LEN	1024
#endif

typedef struct {
	char		*buf;
	uint		buf_size;
	uint		idx;
	char		*_buf_compat;	/* redundant pointer for backward compat. */
} hndrte_log_t;

typedef struct {
	/* Virtual UART
	 *   When there is no UART (e.g. Quickturn), the host should write a complete
	 *   input line directly into cbuf and then write the length into vcons_in.
	 *   This may also be used when there is a real UART (at risk of conflicting with
	 *   the real UART).  vcons_out is currently unused.
	 */
	volatile uint	vcons_in;
	volatile uint	vcons_out;

	/* Output (logging) buffer
	 *   Console output is written to a ring buffer log_buf at index log_idx.
	 *   The host may read the output when it sees log_idx advance.
	 *   Output will be lost if the output wraps around faster than the host polls.
	 */
	hndrte_log_t	log;

	/* Console input line buffer
	 *   Characters are read one at a time into cbuf until <CR> is received, then
	 *   the buffer is processed as a command line.  Also used for virtual UART.
	 */
	uint		cbuf_idx;
	char		cbuf[CBUF_LEN];
} hndrte_cons_t;
