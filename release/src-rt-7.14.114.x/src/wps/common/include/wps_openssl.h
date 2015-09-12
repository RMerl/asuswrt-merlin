/*
 * OSL APIs for WPS to adapt to OpenSSL crypto library
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_openssl.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _WPS_OPENSSL_H_
#define _WPS_OPENSSL_H_

#ifdef EXTERNAL_OPENSSL

#include <typedefs.h>

#define PAD_LEN_PADDING	1

/* APIs implemented in wps_utils.c */
extern void hmac_sha256(const void *key, int key_len, const unsigned char *text,
	size_t text_len, unsigned char *digest, unsigned int *digest_len);
extern int rijndaelKeySetupEnc(uint32 rk[], const uint8 cipherKey[], int keyBits);
extern int rijndaelKeySetupDec(uint32 rk[], const uint8 cipherKey[], int keyBits);
extern int aes_cbc_encrypt_pad(uint32 *rk, const size_t key_len, const uint8 *nonce,
	const size_t data_len, const uint8 *ptxt, uint8 *ctxt, uint8 padd_type);
extern int aes_cbc_decrypt_pad(uint32 *rk, const size_t key_len, const uint8 *nonce,
	const size_t data_len, const uint8 *ctxt, uint8 *ptxt, uint8 padd_type);

#endif /* EXTERNAL_OPENSSL */
#endif /* _WPS_EXTRA_OPENSSL_H_ */
