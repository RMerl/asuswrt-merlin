/*
 *  bcmtomcrypt.h  - Prototypes for API's interfacing tomcrypt library
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */
#ifndef HEADER_BCMTOMCRYPT_H
#define HEADER_BCMTOMCRYPT_H

#define RSA_KEY_SIZE		1024	/* keysize in bits */
#define LTC_PKCS_1_V1_5 	1	/* LTC_PKCS #1 v1.5 padding (\sa ltc_pkcs_1_v1_5_blocks) */

extern int rsa_make_save_key(char *prng_name, FILE *fprk, FILE *fpbk, FILE *fpn, int size, long e);
extern int rsa_hash_sign_hash(FILE *in, FILE *pk, unsigned char *out, unsigned long *outlen,
	int padding, char *hash_name);
extern int rsa_image_verify_hash(unsigned char *msg, unsigned long msgsz, unsigned char *pkb,
	unsigned long pkbsz, unsigned char *sig, unsigned long siglen,
	int padding, char *hash_name, int *stat);
extern const char *error_to_string(int err);

#endif /* HEADER_BCMTOMCRYPT_H */
