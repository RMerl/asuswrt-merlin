/*
 * Trace messages sent over HBUS
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: msgtrace.h 439325 2013-11-26 14:26:09Z $
 */

#ifndef	_MSGTRACE_H
#define	_MSGTRACE_H

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif


/* This marks the start of a packed structure section. */
#include <packed_section_start.h>
/* for osl_t */
#include <osl_decl.h>
#define MSGTRACE_VERSION 1

/* Message trace header */
typedef BWL_PRE_PACKED_STRUCT struct msgtrace_hdr {
	uint8	version;
	uint8   trace_type;
#define MSGTRACE_HDR_TYPE_MSG 0
#define MSGTRACE_HDR_TYPE_LOG 1
	uint16	len;	/* Len of the trace */
	uint32	seqnum;	/* Sequence number of message. Useful if the messsage has been lost
			 * because of DMA error or a bus reset (ex: SDIO Func2)
			 */
	/* Msgtrace type  only */
	uint32  discarded_bytes;  /* Number of discarded bytes because of trace overflow  */
	uint32  discarded_printf; /* Number of discarded printf because of trace overflow */
} BWL_POST_PACKED_STRUCT msgtrace_hdr_t;

#define MSGTRACE_HDRLEN 	sizeof(msgtrace_hdr_t)

/* The hbus driver generates traces when sending a trace message. This causes endless traces.
 * This flag must be set to TRUE in any hbus traces. The flag is reset in the function msgtrace_put.
 * This prevents endless traces but generates hasardous lost of traces only in bus device code.
 * It is recommendat to set this flag in macro SD_TRACE but not in SD_ERROR for avoiding missing
 * hbus error traces. hbus error trace should not generates endless traces.
 */
extern bool msgtrace_hbus_trace;

typedef void (*msgtrace_func_send_t)(void *hdl1, void *hdl2, uint8 *hdr,
                                     uint16 hdrlen, uint8 *buf, uint16 buflen);
extern void msgtrace_start(void);
extern void msgtrace_stop(void);
extern int msgtrace_sent(void);
extern void msgtrace_put(char *buf, int count);
extern void msgtrace_init(osl_t *osh, void *hdl1, void *hdl2, msgtrace_func_send_t func_send);
extern bool msgtrace_event_enabled(void);

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif	/* _MSGTRACE_H */
