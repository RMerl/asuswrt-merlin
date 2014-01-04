/*
 * CFE Per architecture specific implementations for mips
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: bsp_priv.h 375038 2012-12-17 08:23:00Z $
 */

#ifndef _LANGUAGE_ASSEMBLY

/* Global SB handle */
extern si_t *bcm947xx_sih;
#define sih bcm947xx_sih

/* BSP UI initialization */
extern int ui_init_bcm947xxcmds(void);


extern void board_pinmux_init(si_t *sih);
extern void board_clock_init(si_t *sih);
extern void mach_device_init(si_t *sih);
extern void board_power_init(si_t *sih);
extern void board_cpu_init(si_t *sih);
extern void flash_memory_size_config(si_t *sih, void *probe);
extern void board_cfe_cpu_speed_upd(si_t *sih);

#if CFG_ET
extern void et_addcmd(void);
#endif

#if CFG_WLU
extern void wl_addcmd(void);
#endif

#endif	/* !_LANGUAGE_ASSEMBLY */
