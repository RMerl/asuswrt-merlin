/*
 * ADMtek switch setup functions
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
 * $Id: etc_adm.h 267700 2011-06-19 15:41:07Z $
 */

#ifndef _adm_h_
#define _adm_h_

/* forward declarations */
typedef struct adm_info_s adm_info_t;

/* interface prototypes */
extern adm_info_t *adm_attach(si_t *sih, char *vars);
extern void adm_detach(adm_info_t *adm);
extern int adm_enable_device(adm_info_t *adm);
extern int adm_config_vlan(adm_info_t *adm);
#ifdef BCMDBG
extern char *adm_dump_regs(adm_info_t *adm, char *buf);
#endif

#endif /* _adm_h_ */
