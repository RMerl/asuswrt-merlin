/*
 * NVRAM functions for BCA platforms
 *
 * Copyright (C) 2016, Broadcom. All Rights Reserved.
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
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: nvram_ext.c 622857 2016-03-04 00:34:21Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <bcmnvram.h>
#include <wlcsm_linux.h>

int
nvram_init(void *sih)
{
	return 0;
}

int
nvram_append(void *sb, char *vars, uint varsz)
{
	return 0;
}

void
nvram_exit(void *sih)
{
}

char *
nvram_get(const char *name)
{
	return wlcsm_nvram_k_get((char *)name);
}

int
nvram_set(const char *name, const char *value)
{
	wlcsm_nvram_k_set((char *)name, (char *)value);

	return 0;
}

int
nvram_unset(const char *name)
{
	return 0;
}

int
nvram_commit(void)
{
	return 0;
}

/* Return: 0 - success */
int
nvram_getall(char *buf, int count)
{
	if (count < 1)
		return BCME_BUFTOOSHORT;

	if (!wlcsm_nvram_getall(buf, count))
		return BCME_NOTFOUND;

	return BCME_OK;
}
