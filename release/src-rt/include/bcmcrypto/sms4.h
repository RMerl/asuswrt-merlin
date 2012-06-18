#ifdef BCMWAPI_WPI
/*
 * sms4.h
 * SMS-4 block cipher
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sms4.h,v 1.16 2009-10-22 00:10:59 Exp $
 */

#ifndef _SMS4_H_
#define _SMS4_H_

#include <typedefs.h>
#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>  /* For size_t */
#endif

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

#define SMS4_BLOCK_SZ		16
#define SMS4_WORD_SZ		sizeof(uint32)
#define SMS4_RK_WORDS		32
#define SMS4_BLOCK_WORDS	(SMS4_BLOCK_SZ/SMS4_WORD_SZ)

#define SMS4_WPI_PN_LEN		SMS4_BLOCK_SZ
#define SMS4_WPI_HEADER_LEN	(SMS4_WPI_PN_LEN + 2)
#define SMS4_WPI_MAX_MPDU_LEN	2278

#define SMS4_WPI_MIN_AAD_LEN		32
#define SMS4_WPI_MAX_AAD_LEN		34
#define SMS4_OLD_KEY_MAXVALIDTIME	60

/* WPI IV, not to be confused with CBC-MAC and OFB IVs which are really just
 * the PN
 */
BWL_PRE_PACKED_STRUCT struct wpi_iv {
	uint8	key_idx;
	uint8	reserved;
	uint8	PN[SMS4_WPI_PN_LEN];
} BWL_POST_PACKED_STRUCT;

#define SMS4_WPI_IV_LEN	sizeof(struct wpi_iv)

void sms4_enc(uint32 *Y, uint32 *X, const uint32 *RK);

void sms4_dec(uint32 *Y, uint32 *X, uint32 *RK);

void sms4_key_exp(uint32 *MK, uint32 *RK);

int sms4_wpi_cbc_mac(const uint8 *ick,
	const uint8 *iv,
	const size_t aad_len,
	const uint8 *aad,
	uint8 *ptxt);

int sms4_ofb_crypt(const uint8 *ek,
	const uint8 *iv,
	const size_t data_len,
	uint8 *ptxt);

#define SMS4_WPI_SUCCESS		0
#define SMS4_WPI_ENCRYPT_ERROR		-1
#define SMS4_WPI_DECRYPT_ERROR		-2
#define SMS4_WPI_CBC_MAC_ERROR		-3
#define SMS4_WPI_DECRYPT_MIC_FAIL	SMS4_WPI_CBC_MAC_ERROR
#define SMS4_WPI_OFB_ERROR		-4

#define SMS4_WPI_SUBTYPE_LOW_MASK	0x0070
#define	SMS4_WPI_FC_MASK 		~(SMS4_WPI_SUBTYPE_LOW_MASK | \
					FC_RETRY | FC_PM | FC_MOREDATA)

int sms4_wpi_pkt_encrypt(const uint8 *ek,
	const uint8 *ick,
	const size_t data_len,
	uint8 *p);

int sms4_wpi_pkt_decrypt(const uint8 *ek,
	const uint8 *ick,
	const size_t data_len,
	uint8 *p);

INLINE void sxor_128bit_block(const uint8 *src1, const uint8 *src2, uint8 *dst);

#ifdef BCMSMS4_TEST
int sms4_test_enc_dec(void);
int sms4_test_cbc_mac(void);
int sms4_test_ofb_crypt(void);
int sms4_test_wpi_pkt_encrypt_decrypt_timing(int *t);
int sms4_test_wpi_pkt_encrypt(void);
int sms4_test_wpi_pkt_decrypt(void);
int sms4_test_wpi_pkt_micfail(void);
int sms4_test(int *t);
#endif /* BCMSMS4_TEST */

/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#endif /* _SMS4_H_ */
#endif /* BCMWAPI_WPI */
