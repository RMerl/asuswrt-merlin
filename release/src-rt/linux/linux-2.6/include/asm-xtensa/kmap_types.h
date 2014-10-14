/*
 * include/asm-xtensa/kmap_types.h
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 - 2005 Tensilica Inc.
 */

#ifndef _XTENSA_KMAP_TYPES_H
#define _XTENSA_KMAP_TYPES_H

enum km_type {
  KM_BOUNCE_READ,
  KM_SKB_SUNRPC_DATA,
  KM_SKB_DATA_SOFTIRQ,
  KM_USER0,
  KM_USER1,
  KM_BIO_SRC_IRQ,
  KM_BIO_DST_IRQ,
  KM_PTE0,
  KM_PTE1,
  KM_IRQ0,
  KM_IRQ1,
  KM_SOFTIRQ0,
  KM_SOFTIRQ1,
  KM_TYPE_NR
};

#endif	/* _XTENSA_KMAP_TYPES_H */
