/*
 * HND SOCRAM TCAM software interface.
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
 * $Id: hndtcam.h 412505 2013-07-15 08:40:14Z $
 */
#ifndef _hndtcam_h_
#define _hndtcam_h_

/*
 * 0 - 1
 * 1 - 2 Consecutive locations are patched
 * 2 - 4 Consecutive locations are patched
 * 3 - 8 Consecutive locations are patched
 * 4 - 16 Consecutive locations are patched
 * Define default to patch 2 locations
 */

#ifdef  PATCHCOUNT
#define SRPC_PATCHCOUNT PATCHCOUNT
#else
#define PATCHCOUNT 0
#define SRPC_PATCHCOUNT PATCHCOUNT
#endif

#if defined(__ARM_ARCH_7R__)
#if !defined(PATCHCOUNT) || (PATCHCOUNT == 0)
#undef PATCHCOUNT
#define PATCHCOUNT 1
#endif
#define	ARMCR4_TCAMPATCHCOUNT	PATCHCOUNT
#define ARMCR4_TCAMADDR_MASK (~((1 << (ARMCR4_TCAMPATCHCOUNT + 2))-1))
#define ARMCR4_PATCHNLOC (1 << ARMCR4_TCAMPATCHCOUNT)
#endif	/* defined(__ARM_ARCH_7R__) */

/* N Consecutive location to patch */
#define SRPC_PATCHNLOC (1 << (SRPC_PATCHCOUNT))

#define PATCHHDR(_p)		__attribute__ ((__section__ (".patchhdr."#_p))) _p
#define PATCHENTRY(_p)		__attribute__ ((__section__ (".patchentry."#_p))) _p

#if defined(__ARM_ARCH_7R__)
typedef struct {
	uint32	data[ARMCR4_PATCHNLOC];
} patch_entry_t;
#else
typedef struct {
	uint32	data[SRPC_PATCHNLOC];
} patch_entry_t;
#endif

typedef struct {
	void		*addr;		/* patch address */
	uint32		len;		/* bytes to patch in entry */
	patch_entry_t	*entry;		/* patch entry data */
} patch_hdr_t;

/* patch values and address structure */
typedef struct patchaddrvalue {
	uint32	addr;
	uint32	value;
} patchaddrvalue_t;

extern void *socram_regs;
extern uint32 socram_rev;

extern void *arm_regs;

extern void hnd_patch_init(void *srp);
extern void hnd_tcam_write(void *srp, uint16 idx, uint32 data);
extern void hnd_tcam_read(void *srp, uint16 idx, uint32 *content);
void * hnd_tcam_init(void *srp, int no_addrs);
extern void hnd_tcam_disablepatch(void *srp);
extern void hnd_tcam_patchdisable(void);
extern void hnd_tcam_enablepatch(void *srp);
#ifdef CONFIG_XIP
extern void hnd_tcam_bootloader_load(void *srp, char *pvars);
#else
extern void hnd_tcam_load(void *srp, const  patchaddrvalue_t *patchtbl);
#endif /* CONFIG_XIP */
extern void BCMATTACHFN(hnd_tcam_load_default)(void);
extern void hnd_tcam_reclaim(void);

#endif /* _hndtcam_h_ */
