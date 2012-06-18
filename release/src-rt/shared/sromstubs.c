/*
 * Broadcom Home Networking Division (HND) srom stubs
 *
 * Should be called bcmsromstubs.c .
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
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
 * $Id: sromstubs.c,v 1.19 2008-12-17 13:32:15 Exp $
 */

#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmsrom.h>

uint8 patch_pair = 0;

int
srom_var_init(si_t *sih, uint bus, void *curmap, osl_t *osh, char **vars, uint *count)
{
	return 0;
}

int
srom_read(si_t *sih, uint bus, void *curmap, osl_t *osh, uint byteoff, uint nbytes, uint16 *buf,
          bool check_crc)
{
	return 0;
}

int
srom_write(si_t *sih, uint bus, void *curmap, osl_t *osh, uint byteoff, uint nbytes, uint16 *buf)
{
	return 0;
}

#if defined(WLTEST) || defined(BCMDBG)
int
srom_otp_write_region_crc(si_t *sih, uint nbytes, uint16* buf16, bool write)
{
	return 0;
}
#endif 
