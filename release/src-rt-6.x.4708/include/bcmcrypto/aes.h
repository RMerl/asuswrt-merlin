/*
 * aes.h
 * AES encrypt/decrypt wrapper functions used around Rijndael reference
 * implementation
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
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
 * $Id: aes.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _AES_H_
#define _AES_H_

#include <typedefs.h>
#ifdef BCMDRIVER
#include <osl.h>
#else
#include <stddef.h>  /* For size_t */
#endif
#include <bcmcrypto/rijndael-alg-fst.h>

/* forward declaration */
typedef struct dot11_header dot11_header_t;

#define AES_BLOCK_SZ    	16
#define AES_BLOCK_BITLEN	(AES_BLOCK_SZ * 8)
#define AES_KEY_BITLEN(kl)  	(kl * 8)
#define AES_ROUNDS(kl)		((AES_KEY_BITLEN(kl) / 32) + 6)
#define AES_MAXROUNDS		14

enum {
	NO_PADDING,
	PAD_LEN_PADDING /* padding with padding length  */
};


void BCMROMFN(aes_encrypt)(const size_t KL, const uint8 *K, const uint8 *ptxt, uint8 *ctxt);
void BCMROMFN(aes_decrypt)(const size_t KL, const uint8 *K, const uint8 *ctxt, uint8 *ptxt);

#define aes_block_encrypt(nr, rk, ptxt, ctxt) rijndaelEncrypt(rk, nr, ptxt, ctxt)
#define aes_block_decrypt(nr, rk, ctxt, ptxt) rijndaelDecrypt(rk, nr, ctxt, ptxt)

int
BCMROMFN(aes_cbc_encrypt_pad)(uint32 *rk, const size_t key_len, const uint8 *nonce,
                              const size_t data_len, const uint8 *ptxt, uint8 *ctxt,
                              uint8 pad_type);
int BCMROMFN(aes_cbc_encrypt)(uint32 *rk, const size_t key_len, const uint8 *nonce,
                              const size_t data_len, const uint8 *ptxt, uint8 *ctxt);
int BCMROMFN(aes_cbc_decrypt)(uint32 *rk, const size_t key_len, const uint8 *nonce,
                              const size_t data_len, const uint8 *ctxt, uint8 *ptxt);
int BCMROMFN(aes_cbc_decrypt_pad)(uint32 *rk, const size_t key_len, const uint8 *nonce,
                                  const size_t data_len, const uint8 *ctxt, uint8 *ptxt,
                                  uint8 pad_type);

#define AES_CTR_MAXBLOCKS	(1<<16)

int BCMROMFN(aes_ctr_crypt)(unsigned int *rk, const size_t key_len, const uint8 *nonce,
                            const size_t data_len, const uint8 *ptxt, uint8 *ctxt);

/* only support the 2 octet AAD length encoding */
#define AES_CCM_LEN_FIELD_LEN	2
#define AES_CCM_AAD_MAX_LEN	0xFEFF
#define AES_CCM_NONCE_LEN	13

#define AES_CCM_CRYPT_FLAGS	(AES_CCM_LEN_FIELD_LEN-1)

/* only support the 8 octet auth field */
#define AES_CCM_AUTH_LEN	8

#define AES_CCM_AUTH_FLAGS	(4*(AES_CCM_AUTH_LEN-2) + (AES_CCM_LEN_FIELD_LEN-1))
#define AES_CCM_AUTH_AAD_FLAG	0x40

#define AES_CCMP_AAD_MIN_LEN	22
#define AES_CCMP_AAD_MAX_LEN	30
#define AES_CCMP_NONCE_LEN	AES_CCM_NONCE_LEN
#define AES_CCMP_AUTH_LEN	AES_CCM_AUTH_LEN

#define AES_CCMP_FC_RETRY		0x800
#define AES_CCMP_FC_PM			0x1000
#define AES_CCMP_FC_MOREDATA		0x2000
#define AES_CCMP_FRAGNUM_MASK		0x000f
#define AES_CCMP_QOS_TID_MASK		0x000f
#define AES_CCMP_NF_PRIORITY		0x0f
#define AES_CCMP_NF_MANAGEMENT		0x10

/* mute PM */
#define	AES_CCMP_LEGACY_FC_MASK		~(AES_CCMP_FC_RETRY)
#define	AES_CCMP_LEGACY_SEQ_MASK	0x0000
#define	AES_CCMP_LEGACY_QOS_MASK	0xffff

/* mute MoreData, PM, Retry, Low 3 bits of subtype */
#define AES_CCMP_SUBTYPE_LOW_MASK	0x0070
#define	AES_CCMP_FC_MASK 		~(AES_CCMP_SUBTYPE_LOW_MASK | AES_CCMP_FC_RETRY | \
					  AES_CCMP_FC_PM | AES_CCMP_FC_MOREDATA)
#define	AES_CCMP_SEQ_MASK		AES_CCMP_FRAGNUM_MASK
#define	AES_CCMP_QOS_MASK		AES_CCMP_QOS_TID_MASK

#define AES_CCMP_ENCRYPT_SUCCESS	0
#define AES_CCMP_ENCRYPT_ERROR		-1

/* AES-CCMP mode encryption algorithm
	- encrypts in-place
	- packet buffer must be contiguous
	- packet buffer must have sufficient tailroom for CCMP MIC
	- returns AES_CCMP_ENCRYPT_SUCCESS on success
	- returns AES_CCMP_ENCRYPT_ERROR on error
*/
int BCMROMFN(aes_ccmp_encrypt)(unsigned int *rk, const size_t key_len,
	const size_t data_len, uint8 *p, bool legacy, uint8 nonce_1st_byte);

#define AES_CCMP_DECRYPT_SUCCESS	0
#define AES_CCMP_DECRYPT_ERROR		-1
#define AES_CCMP_DECRYPT_MIC_FAIL	-2

/* AES-CCMP mode decryption algorithm
	- decrypts in-place
	- packet buffer must be contiguous
	- packet buffer must have sufficient tailroom for CCMP MIC
	- returns AES_CCMP_DECRYPT_SUCCESS on success
	- returns AES_CCMP_DECRYPT_ERROR on decrypt protocol/format error
	- returns AES_CCMP_DECRYPT_MIC_FAIL on message integrity check failure
*/
int BCMROMFN(aes_ccmp_decrypt)(unsigned int *rk, const size_t key_len,
	const size_t data_len, uint8 *p, bool legacy, uint8 nonce_1st_byte);

void BCMROMFN(aes_ccmp_cal_params)(dot11_header_t *h, bool legacy, uint8 nonce_1st_byte,
                               uint8 *nonce, uint8 *aad, uint *la, uint *lh);

int BCMROMFN(aes_ccm_mac)(unsigned int *rk, const size_t key_len, const uint8 *nonce,
                          const size_t aad_len, const uint8 *aad, const size_t data_len,
                          const uint8 *ptxt, uint8 *mac);
int BCMROMFN(aes_ccm_encrypt)(unsigned int *rk, const size_t key_len, const uint8 *nonce,
                              const size_t aad_len, const uint8 *aad, const size_t data_len,
                              const uint8 *ptxt, uint8 *ctxt, uint8 *mac);
int BCMROMFN(aes_ccm_decrypt)(unsigned int *rk, const size_t key_len, const uint8 *nonce,
                              const size_t aad_len, const uint8 *aad, const size_t data_len,
                              const uint8 *ctxt, uint8 *ptxt);

#define AES_CMAC_AUTH_LEN	AES_BLOCK_SZ

void aes_cmac_gen_subkeys(const size_t kl, const uint8 *K, uint8 *K1, uint8 *K2);

void aes_cmac(const size_t key_len, const uint8* K, const uint8 *K1,
              const uint8 *K2, const size_t data_len, const uint8 *ptxt, uint8 *mac);
void aes_cmac_calc(const uint8 *data, const size_t data_length,
                   const uint8 *mic_key, const size_t key_len, uint8 *mic);

#ifdef BCMAES_GENTABLE
/*
 * For rijndale-alg-fst.c, the tables are defined as:
 * Te0[x] = S[x].[02, 01, 01, 03]
 * Te1[x] = S[x].[03, 02, 01, 01]
 * Te2[x] = S[x].[01, 03, 02, 01]
 * Te3[x] = S[x].[01, 01, 03, 02]
 * Te4[x] = S[x].[01, 01, 01, 01]
 * Td0[x] = Si[x].[0e, 09, 0d, 0b]
 * Td1[x] = Si[x].[0b, 0e, 09, 0d]
 * Td2[x] = Si[x].[0d, 0b, 0e, 09]
 * Td3[x] = Si[x].[09, 0d, 0b, 0e]
 * Td4[x] = Si[x].[01, 01, 01, 01]
 * where multiplications are done in GF2
 */

#define ROTATE(x, n)     (((x) << (n)) | ((x) >> (32 - (n))))

typedef struct aes_tables {
	uint32 *Te0;
	uint32 *Te1;
	uint32 *Te2;
	uint32 *Te3;
	uint32 *Te4;
	uint32 *Td0;
	uint32 *Td1;
	uint32 *Td2;
	uint32 *Td3;
	uint32 *Td4;
} aes_tables_t;

#define AES_SBOX_ENTRIES	256

/* AES S-box.  This can also be re-generated from:
 * 	Te4 (any byte)
 * 	Te0, Te1, Te2 (need to select the correct byte)
 */
static const uint8 AES_Sbox[AES_SBOX_ENTRIES] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

/* AES Inverse S-box.  This can also be re-generated from Td4 (any byte) */
static const uint8 AES_Inverse_Sbox[AES_SBOX_ENTRIES] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

#define AES_MX	0x1b
#define	XTIME(s, sx2)	\
		if ((s) & 0x80)	\
			sx2 = (uint8) ((((s) << 1) ^ AES_MX) & 0xff);	\
		else	\
			sx2 = (uint8) (((s) << 1) & 0xff);

void aes_gen_tables(const uint8 *S, const uint8 *Si, aes_tables_t at);

#endif /* BCMAES_GENTABLE */

#endif /* _AES_H_ */
