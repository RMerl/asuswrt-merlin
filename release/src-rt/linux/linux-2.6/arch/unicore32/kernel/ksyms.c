/*
 * linux/arch/unicore32/kernel/ksyms.c
 *
 * Code specific to PKUnity SoC and UniCore ISA
 *
 * Copyright (C) 2001-2010 GUAN Xue-tao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/cryptohash.h>
#include <linux/delay.h>
#include <linux/in6.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include <asm/checksum.h>
#include <asm/system.h>

#include "ksyms.h"

EXPORT_SYMBOL(__uc32_find_next_zero_bit);
EXPORT_SYMBOL(__uc32_find_next_bit);

EXPORT_SYMBOL(__backtrace);

	/* platform dependent support */
EXPORT_SYMBOL(__udelay);
EXPORT_SYMBOL(__const_udelay);

	/* networking */
EXPORT_SYMBOL(csum_partial);
EXPORT_SYMBOL(csum_partial_copy_from_user);
EXPORT_SYMBOL(csum_partial_copy_nocheck);
EXPORT_SYMBOL(__csum_ipv6_magic);

	/* io */
#ifndef __raw_readsb
EXPORT_SYMBOL(__raw_readsb);
#endif
#ifndef __raw_readsw
EXPORT_SYMBOL(__raw_readsw);
#endif
#ifndef __raw_readsl
EXPORT_SYMBOL(__raw_readsl);
#endif
#ifndef __raw_writesb
EXPORT_SYMBOL(__raw_writesb);
#endif
#ifndef __raw_writesw
EXPORT_SYMBOL(__raw_writesw);
#endif
#ifndef __raw_writesl
EXPORT_SYMBOL(__raw_writesl);
#endif

	/* string / mem functions */
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(memchr);

	/* user mem (segment) */
EXPORT_SYMBOL(__strnlen_user);
EXPORT_SYMBOL(__strncpy_from_user);

EXPORT_SYMBOL(copy_page);

EXPORT_SYMBOL(__copy_from_user);
EXPORT_SYMBOL(__copy_to_user);
EXPORT_SYMBOL(__clear_user);

EXPORT_SYMBOL(__get_user_1);
EXPORT_SYMBOL(__get_user_2);
EXPORT_SYMBOL(__get_user_4);

EXPORT_SYMBOL(__put_user_1);
EXPORT_SYMBOL(__put_user_2);
EXPORT_SYMBOL(__put_user_4);
EXPORT_SYMBOL(__put_user_8);

EXPORT_SYMBOL(__ashldi3);
EXPORT_SYMBOL(__ashrdi3);
EXPORT_SYMBOL(__divsi3);
EXPORT_SYMBOL(__lshrdi3);
EXPORT_SYMBOL(__modsi3);
EXPORT_SYMBOL(__muldi3);
EXPORT_SYMBOL(__ucmpdi2);
EXPORT_SYMBOL(__udivsi3);
EXPORT_SYMBOL(__umodsi3);
EXPORT_SYMBOL(__bswapsi2);

