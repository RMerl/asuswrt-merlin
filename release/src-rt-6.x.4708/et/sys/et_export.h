/*
 * Required functions exported by the port-specific (os-dependent) driver
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2013, Broadcom Corporation. All Rights Reserved.
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
 * $Id: et_export.h 418172 2013-08-14 06:08:25Z $
 */

#ifndef _et_export_h_
#define _et_export_h_

/* misc callbacks */
extern void et_init(void *et, uint options);
extern void et_reset(void *et);
extern void et_link_up(void *et);
extern void et_link_down(void *et);
extern int et_up(void *et);
extern int et_down(void *et, int reset);
extern void et_dump(void *et, struct bcmstrbuf *b);
extern void et_intrson(void *et);

/* for BCM5222 dual-phy shared mdio contortion */
extern void *et_phyfind(void *et, uint coreunit);
extern uint16 et_phyrd(void *et, uint phyaddr, uint reg);
extern void et_phywr(void *et, uint reg, uint phyaddr, uint16 val);
#ifdef HNDCTF
extern void et_dump_ctf(void *et, struct bcmstrbuf *b);
#endif
#ifdef BCMDBG_CTRACE
extern void et_dump_ctrace(void *et, struct bcmstrbuf *b);
#endif
#ifdef ETFA
extern void et_fa_lock_init(void *et);
extern void et_fa_lock(void *et);
extern void et_fa_unlock(void *et);
extern void *et_fa_get_fa_dev(void *et);
extern bool et_fa_dev_on(void *dev);
extern void et_fa_set_dev_on(void *et);
extern void *et_fa_fs_create(void);
extern void et_fa_fs_clean(void);
#endif /* ETFA */
#endif	/* _et_export_h_ */
