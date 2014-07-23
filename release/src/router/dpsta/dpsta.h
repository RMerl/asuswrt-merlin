/*
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
 * $Id: dpsta.h 300516 2011-12-04 17:39:44Z $
 */

#ifndef _DPSTA_H_
#define _DPSTA_H_

typedef struct psta_if psta_if_t;

/* Proxy STA instance data and exported functions */
typedef struct psta_if_api {
	void	*psta;
	void	*bsscfg;
	bool	(*is_ds_sta)(void *psta, struct ether_addr *ea);
	void	*(*psta_find)(void *psta, uint8 *ea);
	bool	(*bss_auth)(void *bsscfg);
} psta_if_api_t;

extern psta_if_t *dpsta_register(uint32 unit, psta_if_api_t *inst);
extern int32 dpsta_recv(void *p);

#endif /* _DPSTA_H_ */
