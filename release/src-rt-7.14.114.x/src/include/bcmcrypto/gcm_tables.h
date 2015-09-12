/*
 * gcm_tables.h
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: gcm_tables.h 349803 2012-08-09 18:49:41Z $
 */

#ifndef _GCM_TABLES_H_
#define _GCM_TABLES_H_

/* Support for various per-key tables.
 *  Shoup's 4 bit tables         - 256 bytes/key - Supported
 *  Shoup's 8 bit tables         - 4096 bytes/key - Supported
 *  Wei Dai (crypto-optimization)- 2048 bytes/key - NOT Supported
 *  Simple 4 bit tables          - 8192 bytes/key - NOT Supported
 *  Simple 8 bit tables          - 65536 bytes/key - NOT Supported
 *
 * Rationale: 4 bit tables are reasonable in terms of memory. 8 bit
 * tables would provide performance comparable to CCM. Wei Dai
 * claims performance equivalent to 8k bytes/key - i.e simple 4 bit and
 * within a factor of two compared to 64k bytes/key - i.e. simple 8 bit
 * Based on comments in openssl 1.0.1c implementation, simple tables
 * seem to be vulnerable to timing attacks...
 */

#if GCM_TABLE_SZ != 256 && GCM_TABLE_SZ != 4096
#error "Unsupported table size"
#endif

#define GCM_TABLE_NUM_ENTRIES (GCM_TABLE_SZ / sizeof(gcm_block_t))

struct gcm_table {
	gcm_block_t M0[GCM_TABLE_NUM_ENTRIES];
};

typedef struct gcm_table gcm_table_t;

void gcm_init_table(gcm_table_t *t, gcm_block_t hk);
void gcm_mul_with_table(const gcm_table_t *t, const gcm_block_t y,
	gcm_block_t out);

#endif /* _GCM_TABLES_H_ */
