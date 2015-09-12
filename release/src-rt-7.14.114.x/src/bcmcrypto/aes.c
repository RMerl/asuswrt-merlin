/*
 * aes.c
 * AES encrypt/decrypt wrapper functions used around Rijndael reference
 * implementation
 *
 * Copyright (C) 2015, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: aes.c 408798 2013-06-20 18:57:33Z $
 */

#include <bcm_cfg.h>
#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif	/* BCMDRIVER */

#include <bcmendian.h>
#include <bcmutils.h>
#include <proto/802.11.h>
#include <bcmcrypto/aes.h>
#include <bcmcrypto/rijndael-alg-fst.h>

#ifdef BCMAES_TEST
#include <stdio.h>

#define	dbg(args)	printf args

void
pinter(const char *label, const uint8 *A, const size_t il, const uint8 *R)
{
	int k;
	printf("%s", label);
	for (k = 0; k < AES_BLOCK_SZ; k++)
		printf("%02X", A[k]);
	printf(" ");
	for (k = 0; k < il; k++) {
		printf("%02X", R[k]);
		if (!((k + 1) % AES_BLOCK_SZ))
			printf(" ");
		}
	printf("\n");
}

void
pres(const char *label, const size_t len, const uint8 *data)
{
	int k;
	printf("%s\n", label);
	for (k = 0; k < len; k++) {
		printf("%02x ", data[k]);
		if (!((k + 1) % AES_BLOCK_SZ))
			printf("\n");
	}
	printf("\n");
}

#ifdef BCMAES_GENTABLE
void
ptable(const char *tablename, const uint32 *table)
{
	int k;
	printf("static const uint32 %s[256] = {\n	", tablename);
	for (k = 0; k < 256; k++) {
		printf("0x%08xU", table[k]);
		if ((k+1) % 4)
			printf(", ");
		else
			if (k != 255)
				printf(",\n	");
			else
				printf("\n");
	}
	printf("};\n");
}
#endif /* BCMAES_GENTABLE */

#else
#define	dbg(args)
#define pinter(label, A, il, R)
#define pres(label, len, data)
#endif /* BCMAES_TEST */

/*
* ptxt - plain text
* ctxt - cipher text
*/

/* Perform AES block encryption, including key schedule setup */
void
BCMROMFN(aes_encrypt)(const size_t kl, const uint8 *K, const uint8 *ptxt, uint8 *ctxt)
{
	uint32 rk[4 * (AES_MAXROUNDS + 1)];
	rijndaelKeySetupEnc(rk, K, (int)AES_KEY_BITLEN(kl));
	rijndaelEncrypt(rk, (int)AES_ROUNDS(kl), ptxt, ctxt);
}

/* Perform AES block decryption, including key schedule setup */
void
BCMROMFN(aes_decrypt)(const size_t kl, const uint8 *K, const uint8 *ctxt, uint8 *ptxt)
{
	uint32 rk[4 * (AES_MAXROUNDS + 1)];
	rijndaelKeySetupDec(rk, K, (int)AES_KEY_BITLEN(kl));
	rijndaelDecrypt(rk, (int)AES_ROUNDS(kl), ctxt, ptxt);
}

/* AES-CBC mode encryption algorithm
 *	- handle partial blocks with padding of type as above
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error or final length of output
 */


int
BCMROMFN(aes_cbc_encrypt_pad)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt,
                          uint8 padd_type)
{

	uint8 tmp[AES_BLOCK_SZ];
	uint32 encrypt_len = 0;
	uint32 j;

	/* First block get XORed with nonce/IV */
	const unsigned char *iv = nonce;
	unsigned char *crypt_data = ctxt;
	const unsigned char *plain_data = ptxt;
	uint32 remaining = (uint32)data_len;

	while (remaining >= AES_BLOCK_SZ) {
		xor_128bit_block(iv, plain_data, tmp);
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, crypt_data);
		remaining -= AES_BLOCK_SZ;
		iv = crypt_data;
		crypt_data += AES_BLOCK_SZ;
		plain_data += AES_BLOCK_SZ;
		encrypt_len += AES_BLOCK_SZ;
	}

	if (padd_type == NO_PADDING)
		return encrypt_len;

	if (remaining) {
		for (j = 0; j < remaining; j++) {
			tmp[j] = plain_data[j] ^ iv[j];
		}
	}
	switch (padd_type) {
	case PAD_LEN_PADDING:
		for (j = remaining; j < AES_BLOCK_SZ; j++) {
			tmp[j] = (AES_BLOCK_SZ - remaining) ^  iv[j];
		}
		break;
	default:
		return -1;
	}

	aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, crypt_data);
	encrypt_len += AES_BLOCK_SZ;

	return (encrypt_len);
}

/* AES-CBC mode encryption algorithm
 *	- does not handle partial blocks
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_cbc_encrypt)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt)
{
	if (data_len % AES_BLOCK_SZ)
		return (-1);
	if (data_len < AES_BLOCK_SZ)
		return (-1);

	return aes_cbc_encrypt_pad(rk, key_len, nonce, data_len, ptxt, ctxt, NO_PADDING);
}


/* AES-CBC mode decryption algorithm
 *	- handle partial plaintext blocks with padding
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */
int
BCMROMFN(aes_cbc_decrypt_pad)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt,
                          uint8 padd_type)
{
	uint8 tmp[AES_BLOCK_SZ];
	uint32 remaining = (uint32)data_len;
	/* First block get XORed with nonce/IV */
	const unsigned char *iv = nonce;
	const unsigned char *crypt_data = ctxt;
	uint32 plaintext_len = 0;
	unsigned char *plain_data = ptxt;

	if (data_len % AES_BLOCK_SZ)
		return (-1);
	if (data_len < AES_BLOCK_SZ)
		return (-1);

	while (remaining >= AES_BLOCK_SZ) {
		aes_block_decrypt((int)AES_ROUNDS(key_len), rk, crypt_data, tmp);
		xor_128bit_block(tmp, iv, plain_data);
		remaining -= AES_BLOCK_SZ;
		iv = crypt_data;
		crypt_data += AES_BLOCK_SZ;
		plain_data += AES_BLOCK_SZ;
		plaintext_len += AES_BLOCK_SZ;
	}
	if (padd_type == PAD_LEN_PADDING)
		plaintext_len -= ptxt[plaintext_len -1];
	return (plaintext_len);
}

int
BCMROMFN(aes_cbc_decrypt)(uint32 *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt)
{
	return aes_cbc_decrypt_pad(rk, key_len, nonce, data_len, ctxt, ptxt, NO_PADDING);
}

/* AES-CTR mode encryption/decryption algorithm
 *	- max data_len is (AES_BLOCK_SZ * 2^16)
 *	- nonce must be AES_BLOCK_SZ bytes
 *	- assumes nonce is ready to use as-is (i.e. any
 *		encryption/randomization of nonce/IV is handled by the caller)
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */
int
BCMROMFN(aes_ctr_crypt)(unsigned int *rk,
                        const size_t key_len,
                        const uint8 *nonce,
                        const size_t data_len,
                        const uint8 *ptxt,
                        uint8 *ctxt)
{
	size_t k;
	uint8 tmp[AES_BLOCK_SZ], ctr[AES_BLOCK_SZ];

	if (data_len > (AES_BLOCK_SZ * AES_CTR_MAXBLOCKS)) return (-1);

	memcpy(ctr, nonce, AES_BLOCK_SZ);

	for (k = 0; k < (data_len / AES_BLOCK_SZ); k++) {
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, ctr, tmp);
		xor_128bit_block(ptxt, tmp, ctxt);
		ctr[AES_BLOCK_SZ-1]++;
		if (!ctr[AES_BLOCK_SZ - 1]) ctr[AES_BLOCK_SZ - 2]++;
		ptxt += AES_BLOCK_SZ;
		ctxt += AES_BLOCK_SZ;
	}
	/* handle partial block */
	if (data_len % AES_BLOCK_SZ) {
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, ctr, tmp);
		for (k = 0; k < (data_len % AES_BLOCK_SZ); k++)
			ctxt[k] = ptxt[k] ^ tmp[k];
	}

	return (0);
}


/* AES-CCM mode MAC calculation
 *	- computes AES_CCM_AUTH_LEN MAC
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_mac)(unsigned int *rk,
                      const size_t key_len,
                      const uint8 *nonce,
                      const size_t aad_len,
                      const uint8 *aad,
                      const size_t data_len,
                      const uint8 *ptxt,
                      uint8 *mac)
{
	uint8 B_0[AES_BLOCK_SZ], X[AES_BLOCK_SZ];
	size_t j, k;

	if (aad_len > AES_CCM_AAD_MAX_LEN) return (-1);

	pres("aes_ccm_mac: nonce:", AES_CCM_NONCE_LEN, nonce);
	pres("aes_ccm_mac: aad:", aad_len, aad);
	pres("aes_ccm_mac: input:", data_len, ptxt);

	/* B_0 = Flags || Nonce || l(m) */
	B_0[0] = AES_CCM_AUTH_FLAGS;
	if (aad_len)
		B_0[0] |= AES_CCM_AUTH_AAD_FLAG;
	memcpy(&B_0[1], nonce, AES_CCM_NONCE_LEN);
	B_0[AES_BLOCK_SZ - 2] = (uint8)(data_len >> 8) & 0xff;
	B_0[AES_BLOCK_SZ - 1] = (uint8)(data_len & 0xff);

	/* X_1 := E( K, B_0 ) */
	pres("aes_ccm_mac: CBC IV in:", AES_BLOCK_SZ, B_0);
	aes_block_encrypt((int)AES_ROUNDS(key_len), rk, B_0, X);
	pres("aes_ccm_mac: CBC IV out:", AES_BLOCK_SZ, X);

	/* X_i + 1 := E( K, X_i XOR B_i )  for i = 1, ..., n */

	/* first the AAD */
	if (aad_len) {
		pres("aes_ccm_mac: aad:", aad_len, aad);
		X[0] ^= (aad_len >> 8) & 0xff;
		X[1] ^= aad_len & 0xff;
		k = 2;
		j = aad_len;
		while (j--) {
			X[k] ^= *aad++;
			k++;
			if (k == AES_BLOCK_SZ) {
				pres("aes_ccm_mac: After xor (full block aad):", AES_BLOCK_SZ, X);
				aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
				pres("aes_ccm_mac: After AES (full block aad):", AES_BLOCK_SZ, X);
				k = 0;
			}
		}
		/* handle partial last block */
		if (k % AES_BLOCK_SZ) {
			pres("aes_ccm_mac: After xor (partial block aad):", AES_BLOCK_SZ, X);
			aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
			pres("aes_ccm_mac: After AES (partial block aad):", AES_BLOCK_SZ, X);
		}
	}

	/* then the message data */
	for (k = 0; k < (data_len / AES_BLOCK_SZ); k++) {
		xor_128bit_block(X, ptxt, X);
		pres("aes_ccm_mac: After xor (full block data):", AES_BLOCK_SZ, X);
		ptxt += AES_BLOCK_SZ;
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
		pres("aes_ccm_mac: After AES (full block data):", AES_BLOCK_SZ, X);
	}
	/* last block may be partial, padding is implicit in this xor */
	for (k = 0; k < (data_len % AES_BLOCK_SZ); k++)
		X[k] ^= *ptxt++;
	if (data_len % AES_BLOCK_SZ) {
		pres("aes_ccm_mac: After xor (final block data):", AES_BLOCK_SZ, X);
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, X, X);
		pres("aes_ccm_mac: After AES (final block data):", AES_BLOCK_SZ, X);
	}

	/* T := first-M-bytes( X_n+1 ) */
	memcpy(mac, X, AES_CCM_AUTH_LEN);
	pres("aes_ccm_mac: MAC:", AES_CCM_AUTH_LEN, mac);

	return (0);
}

/* AES-CCM mode encryption
 *	- computes AES_CCM_AUTH_LEN MAC and then encrypts ptxt and MAC
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- ctxt must have sufficient tailroom for CCM MAC
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_encrypt)(unsigned int *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t aad_len,
                          const uint8 *aad,
                          const size_t data_len,
                          const uint8 *ptxt,
                          uint8 *ctxt,
                          uint8 *mac)
{
	uint8 A[AES_BLOCK_SZ], X[AES_BLOCK_SZ];

	/* initialize counter */
	A[0] = AES_CCM_CRYPT_FLAGS;
	memcpy(&A[1], nonce, AES_CCM_NONCE_LEN);
	A[AES_BLOCK_SZ-2] = 0;
	A[AES_BLOCK_SZ-1] = 0;
	pres("aes_ccm_encrypt: initial counter:", AES_BLOCK_SZ, A);

	/* calculate and encrypt MAC */
	if (aes_ccm_mac(rk, key_len, nonce, aad_len, aad, data_len, ptxt, X))
		return (-1);
	pres("aes_ccm_encrypt: MAC:", AES_CCM_AUTH_LEN, X);
	if (aes_ctr_crypt(rk, key_len, A, AES_CCM_AUTH_LEN, X, X))
		return (-1);
	pres("aes_ccm_encrypt: encrypted MAC:", AES_CCM_AUTH_LEN, X);
	memcpy(mac, X, AES_CCM_AUTH_LEN);

	/* encrypt data */
	A[AES_BLOCK_SZ - 1] = 1;
	if (aes_ctr_crypt(rk, key_len, A, data_len, ptxt, ctxt))
		return (-1);
	pres("aes_ccm_encrypt: encrypted data:", data_len, ctxt);

	return (0);
}

/* AES-CCM mode decryption
 *	- decrypts ctxt, then computes AES_CCM_AUTH_LEN MAC and checks it against
 *	  then decrypted MAC
 *	- the decrypted MAC is included in ptxt
 *	- nonce must be AES_CCM_NONCE_LEN bytes
 *	- ptxt and ctxt can point to the same location
 *	- returns -1 on error
 */

int
BCMROMFN(aes_ccm_decrypt)(unsigned int *rk,
                          const size_t key_len,
                          const uint8 *nonce,
                          const size_t aad_len,
                          const uint8 *aad,
                          const size_t data_len,
                          const uint8 *ctxt,
                          uint8 *ptxt)
{
	uint8 A[AES_BLOCK_SZ], X[AES_BLOCK_SZ];

	/* initialize counter */
	A[0] = AES_CCM_CRYPT_FLAGS;
	memcpy(&A[1], nonce, AES_CCM_NONCE_LEN);
	A[AES_BLOCK_SZ - 2] = 0;
	A[AES_BLOCK_SZ - 1] = 1;
	pres("aes_ccm_decrypt: initial counter:", AES_BLOCK_SZ, A);

	/* decrypt data */
	if (aes_ctr_crypt(rk, key_len, A, data_len-AES_CCM_AUTH_LEN, ctxt, ptxt))
		return (-1);
	pres("aes_ccm_decrypt: decrypted data:", data_len-AES_CCM_AUTH_LEN, ptxt);

	/* decrypt MAC */
	A[AES_BLOCK_SZ - 2] = 0;
	A[AES_BLOCK_SZ - 1] = 0;
	if (aes_ctr_crypt(rk, key_len, A, AES_CCM_AUTH_LEN,
	                  ctxt+data_len-AES_CCM_AUTH_LEN, ptxt + data_len - AES_CCM_AUTH_LEN))
		return (-1);
	pres("aes_ccm_decrypt: decrypted MAC:", AES_CCM_AUTH_LEN,
	     ptxt + data_len - AES_CCM_AUTH_LEN);

	/* calculate MAC */
	if (aes_ccm_mac(rk, key_len, nonce, aad_len, aad,
	                data_len - AES_CCM_AUTH_LEN, ptxt, X))
		return (-1);
	pres("aes_ccm_decrypt: MAC:", AES_CCM_AUTH_LEN, X);
	if (memcmp(X, ptxt + data_len - AES_CCM_AUTH_LEN, AES_CCM_AUTH_LEN) != 0)
		return (-1);

	return (0);
}

/* AES-CCMP mode encryption algorithm
 *	- packet buffer should be an 802.11 MPDU, starting with Frame Control,
 *	  and including the CCMP extended IV
 *	- encrypts in-place
 *	- packet buffer must have sufficient tailroom for CCMP MAC
 *	- returns -1 on error
 */


int
BCMROMFN(aes_ccmp_encrypt)(unsigned int *rk,
                           const size_t key_len,
                           const size_t data_len,
                           uint8 *p,
                           bool legacy,
                           uint8 nonce_1st_byte)
{
	uint8 nonce[AES_CCMP_NONCE_LEN], aad[AES_CCMP_AAD_MAX_LEN];
	struct dot11_header *h = (struct dot11_header*) p;
	uint la, lh;
	int status = 0;

	aes_ccmp_cal_params(h, legacy, nonce_1st_byte, nonce, aad, &la, &lh);

	pres("aes_ccmp_encrypt: aad:", la, aad);

	/*
	 *	MData:
	 *	B3..Bn, n = floor((l(m)+(AES_BLOCK_SZ-1))/AES_BLOCK_SZ) + 2
	 *	m || pad(m)
	 */

	status = aes_ccm_encrypt(rk, key_len, nonce, la, aad,
	                         data_len - lh, p + lh, p + lh, p + data_len);

	pres("aes_ccmp_encrypt: Encrypted packet with MAC:",
	     data_len+AES_CCMP_AUTH_LEN, p);

	if (status)
		return (AES_CCMP_ENCRYPT_ERROR);
	else
		return (AES_CCMP_ENCRYPT_SUCCESS);
}

int
BCMROMFN(aes_ccmp_decrypt)(unsigned int *rk,
                           const size_t key_len,
                           const size_t data_len,
                           uint8 *p,
                           bool legacy,
                           uint8 nonce_1st_byte)
{
	uint8 nonce[AES_CCMP_NONCE_LEN], aad[AES_CCMP_AAD_MAX_LEN];
	struct dot11_header *h = (struct dot11_header *)p;
	uint la, lh;
	int status = 0;

	aes_ccmp_cal_params(h, legacy, nonce_1st_byte, nonce, aad, &la, &lh);

	pres("aes_ccmp_decrypt: aad:", la, aad);

	/*
	 *	MData:
	 *	B3..Bn, n = floor((l(m)+(AES_BLOCK_SZ-1))/AES_BLOCK_SZ) + 2
	 *	m || pad(m)
	 */

	status = aes_ccm_decrypt(rk, key_len, nonce, la, aad,
	                         data_len - lh, p + lh, p + lh);

	pres("aes_ccmp_decrypt: Decrypted packet with MAC:", data_len, p);

	if (status)
		return (AES_CCMP_DECRYPT_MIC_FAIL);
	else
		return (AES_CCMP_DECRYPT_SUCCESS);
}

void
BCMROMFN(aes_ccmp_cal_params)(struct dot11_header *h, bool legacy,
	uint8 nonce_1st_byte, uint8 *nonce, uint8 *aad, uint *la, uint *lh)
{
	uint8 *iv_data;
	uint16 fc, subtype;
	uint16 seq, qc = 0;
	uint addlen = 0;
	bool wds, qos;

	memset(nonce, 0, AES_CCMP_NONCE_LEN);
	memset(aad, 0, AES_CCMP_AAD_MAX_LEN);

	fc = ltoh16(h->fc);
	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	qos = (FC_TYPE(fc) == FC_TYPE_DATA) && (subtype & FC_SUBTYPE_QOS_DATA);

	if (qos) {
		qc = ltoh16(*((uint16 *)((uchar *)h +
		                         (wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN))));
	}

	if (wds) {
		dbg(("aes_ccmp_cal_params: A4 present\n"));
		addlen += ETHER_ADDR_LEN;
	}
	if (qos) {
		dbg(("aes_ccmp_cal_params: QC present\n"));
		addlen += DOT11_QOS_LEN;
	}

	/* length of MPDU header, including IV */
	*lh = DOT11_A3_HDR_LEN + DOT11_IV_AES_CCM_LEN + addlen;
	/* length of AAD */
	*la = AES_CCMP_AAD_MIN_LEN + addlen;
	/* pointer to IV */
	iv_data = (uint8 *)h + DOT11_A3_HDR_LEN + addlen;

	*nonce++ = nonce_1st_byte;

	memcpy(nonce, (uchar *)&h->a2, ETHER_ADDR_LEN);
	nonce += ETHER_ADDR_LEN;

	/* PN[5] */
	*nonce++ = iv_data[7];
	/* PN[4] */
	*nonce++ = iv_data[6];
	/* PN[3] */
	*nonce++ = iv_data[5];
	/* PN[2] */
	*nonce++ = iv_data[4];
	/* PN[1] */
	*nonce++ = iv_data[1];
	/* PN[0] */
	*nonce++ = iv_data[0];

	pres("aes_ccmp_cal_params: nonce:", AES_CCM_NONCE_LEN, nonce - AES_CCM_NONCE_LEN);

	/* B1..B2 =  l(aad) || aad || pad(aad) */
	/* aad: maskedFC || A1 || A2 || A3 || maskedSC || A4 || maskedQC */

	if (!legacy) {
#ifdef MFP
		/* For a management frame, don't mask the the subtype bits */
		if (nonce_1st_byte & AES_CCMP_NF_MANAGEMENT)
			fc &= (FC_SUBTYPE_MASK | AES_CCMP_FC_MASK);
		else
#endif /* MFP */
			fc &= AES_CCMP_FC_MASK;
	} else {
		/* 802.11i Draft 3 inconsistencies:
		 * Clause 8.3.4.4.3: "FC - MPDU Frame Control field, with Retry bit masked
		 * to zero."  (8.3.4.4.3).
		 * Figure 29: "FC - MPDU Frame Control field, with Retry, MoreData, CF-ACK,
		 * CF-POLL bits masked to zero."
		 * F.10.4.1: "FC - MPDU Frame Control field, with Retry, MoreData,
		 * PwrMgmt bits masked to zero."
		 */

		/* Our implementation: match 8.3.4.4.3 */
		fc &= AES_CCMP_LEGACY_FC_MASK;
	}
	*aad++ = (uint8)(fc & 0xff);
	*aad++ = (uint8)((fc >> 8) & 0xff);

	memcpy(aad, (uchar *)&h->a1, 3*ETHER_ADDR_LEN);
	aad += 3*ETHER_ADDR_LEN;

	seq = ltoh16(h->seq);
	if (!legacy) {
		seq &= AES_CCMP_SEQ_MASK;
	} else {
		seq &= AES_CCMP_LEGACY_SEQ_MASK;
	}

	*aad++ = (uint8)(seq & 0xff);
	*aad++ = (uint8)((seq >> 8) & 0xff);

	if (wds) {
		memcpy(aad, (uchar *)&h->a4, ETHER_ADDR_LEN);
		aad += ETHER_ADDR_LEN;
	}
	if (qos) {
		if (!legacy) {
			/* 802.11i Draft 7.0 inconsistencies:
			 * Clause 8.3.3.3.2: "QC - The Quality of Service Control, a
			 * two-octet field that includes the MSDU priority, reserved
			 * for future use."
			 * I.7.4: TID portion of QoS
			 */

			/* Our implementation: match the test vectors */
			qc &= AES_CCMP_QOS_MASK;
			*aad++ = (uint8)(qc & 0xff);
			*aad++ = (uint8)((qc >> 8) & 0xff);
		} else {
			/* 802.11i Draft 3.0 inconsistencies: */
			/* Clause 8.3.4.4.3: "QC - The QoS Control, if present." */
			/* Figure 30: "QC - The QoS TC from QoS Control, if present." */
			/* F.10.4.1: "QC - The QoS TC from QoS Control, if present." */

			/* Our implementation: Match clause 8.3.4.4.3 */
			qc &= AES_CCMP_LEGACY_QOS_MASK;
			*aad++ = (uint8)(qc & 0xff);
			*aad++ = (uint8)((qc >> 8) & 0xff);
		}
	}
}

/* 128-bit long string of zeros */
static const uint8 Z128[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* CMAC Subkey generation polynomial for 128-bit blocks */
static const uint8 R128[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

/* AES-CMAC subkey generation
 *	- computes subkeys K1 and K2 of length AES_BLOCK_SZ
 */
void
aes_cmac_gen_subkeys(const size_t kl, const uint8 *K, uint8 *K1, uint8 *K2)
{
	uint8 L[AES_BLOCK_SZ];
	uint8 high, low;
	int i;

	/* L = CIPHK(0b) */
	aes_encrypt(kl, K, Z128, L);
	pres("Key", kl, K);
	pres("CIPHK", AES_BLOCK_SZ, L);

	/* If MSB1(L) = 0, then K1 = L << 1 */
	/* Else K1 = (L << 1) ^ Rb */
	high = 0;
	for (i = AES_BLOCK_SZ-1; i >= 0; i--) {
		low = L[i] << 1;
		K1[i] = low | high;
		high = (L[i] & 0x80) >> 7;
	}
	if ((L[0] & 0x80) != 0)
		xor_128bit_block(K1, R128, K1);
	pres("K1", AES_BLOCK_SZ, K1);

	/* If MSB1(K1) = 0, then K2 = K1 << 1 */
	/* Else K2 = (K1 << 1) ^ Rb */
	high = 0;
	for (i = AES_BLOCK_SZ-1; i >= 0; i--) {
		low = K1[i] << 1;
		K2[i] = low | high;
		high = (K1[i] & 0x80) >> 7;
	}
	if ((K1[0] & 0x80) != 0)
		xor_128bit_block(K2, R128, K2);
	pres("K2", AES_BLOCK_SZ, K2);

	return;
}

/* AES-CMAC mode calculation
 *	- computes AES_CMAC_AUTH_LEN MAC
 *	- operates on complete bytes only (i.e. data_len is in bytes)
 */

void
aes_cmac(const size_t key_len, const uint8* K,
         const uint8 *K1, const uint8 *K2,
         const size_t data_len,
         const uint8 *ptxt,
         uint8 *mac)
{
	uint n, pblen, i;
	uint8 Mn[AES_BLOCK_SZ], tmp[AES_BLOCK_SZ], C[AES_BLOCK_SZ];
	uint32 rk[4 * (AES_MAXROUNDS + 1)];

	/* If Mlen = 0, let n = 1; else, let n = Mlen/b */
	if (data_len == 0)
		n = 1;
	else
		n = ((uint)data_len + AES_BLOCK_SZ - 1)/AES_BLOCK_SZ;

	/* Let M1, M2, ... , Mn-1, Mn* denote the unique sequence of bit
	   strings such that M = M1 || M2 || ... || Mn-1 || Mn*, where M1,
	   M2,..., Mn-1 are complete blocks.  If Mn* is a complete block, let
	   Mn = K1 ^ Mn*; else, let Mn = K2 ^ (Mn*||10j), where j = nb-Mlen-1
	*/

	if (data_len == (n * AES_BLOCK_SZ)) {
		/* Mn* is a complete block */
		xor_128bit_block(K1, &(ptxt[(n-1)*AES_BLOCK_SZ]), Mn);
	} else {
		/* Mn* is a partial block, pad with 0s and use K2 */
		pblen = (uint)data_len - ((n-1)*AES_BLOCK_SZ);
		for (i = 0; i < pblen; i++)
			Mn[i] = ptxt[((n-1)*AES_BLOCK_SZ) + i];
		Mn[pblen] = 0x80;
		for (i = pblen + 1; i < AES_BLOCK_SZ; i++)
			Mn[i] = 0;
		xor_128bit_block(K2, Mn, Mn);
	}

	/* Let C0 = 0b */
	memset(C, 0, AES_BLOCK_SZ);

	/* For i = 1 to n, let Ci = CIPHK(Ci-1 ^ Mi) */
	rijndaelKeySetupEnc(rk, K, (int)AES_KEY_BITLEN(key_len));
	for (i = 1; i < n; i++) {
		xor_128bit_block(C, &(ptxt[(i-1)*AES_BLOCK_SZ]), tmp);
		aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, C);
	}
	xor_128bit_block(C, Mn, tmp);
	aes_block_encrypt((int)AES_ROUNDS(key_len), rk, tmp, C);

	/* Let T = MSBTlen(Cn) */
	memcpy(mac, C, AES_CMAC_AUTH_LEN);

	return;
}

void
aes_cmac_calc(const uint8 *data, const size_t data_length, const uint8 *mic_key,
	const size_t key_len, uint8 *mic)
{
	uint8 K1[AES_BLOCK_SZ], K2[AES_BLOCK_SZ];

	aes_cmac_gen_subkeys(key_len, mic_key, K1, K2);
	aes_cmac(key_len, mic_key, K1, K2, data_length, data, mic);
}

#ifdef BCMAES_GENTABLE
/* AES table expansion for rijndael-alg-fst.c
 *	- can compute all 10 tables needed by rijndael-alg-fst.c
 *	- only stores table entries for non-NULL entries in "at" structure, so
 *	can be used to generate a subset of the tables if only some are needed
 */
void
aes_gen_tables(const uint8 *S, const uint8 *Si, aes_tables_t at)
{
	int k;
	uint8 s2, s3;
	uint8 si2, si4, si8, si9, sib, sid, sie;
	uint32 Te0tmp, Td0tmp;

	for (k = 0; k < AES_SBOX_ENTRIES; k++) {
		/* 2 X S[k] */
		XTIME(S[k], s2);

		/* 3 X S[k] */
		s3 = s2 ^ S[k];

		/* 2 X Si[k] */
		XTIME(Si[k], si2);

		/* 4 X Si[k] */
		XTIME(si2, si4);

		/* 8 X Si[k] */
		XTIME(si4, si8);

		/* 9 X S[k] */
		si9 = si8 ^ Si[k];

		/* 11 X S[k] */
		sib = si9 ^ si2;

		/* 13 X S[k] */
		sid = si8 ^ si4 ^ Si[k];

		/* 14 X S[k] */
		sie = si8 ^ si4 ^ si2;

		Te0tmp = (s2 << 24) | (S[k] << 16) | (S[k] << 8) | s3;
		if (at.Te0 != NULL)
			at.Te0[k] = Te0tmp;
		if (at.Te1 != NULL)
			at.Te1[k] = ROTATE(Te0tmp, 24);
		if (at.Te2 != NULL)
			at.Te2[k] = ROTATE(Te0tmp, 16);
		if (at.Te3 != NULL)
			at.Te3[k] = ROTATE(Te0tmp, 8);
		if (at.Te4 != NULL)
			at.Te4[k] = (S[k] << 24) | (S[k] << 16) | (S[k] << 8) | S[k];

		Td0tmp = (sie << 24) | (si9 << 16) | (sid << 8) | sib;
		if (at.Td0 != NULL)
			at.Td0[k] = Td0tmp;
		if (at.Td1 != NULL)
			at.Td1[k] = ROTATE(Td0tmp, 24);
		if (at.Td2 != NULL)
			at.Td2[k] = ROTATE(Td0tmp, 16);
		if (at.Td3 != NULL)
			at.Td3[k] = ROTATE(Td0tmp, 8);
		if (at.Td4 != NULL)
			at.Td4[k] = (Si[k] << 24) | (Si[k] << 16) | (Si[k] << 8) | Si[k];
	}
}
#endif /* BCMAES_GENTABLE */

#ifdef BCMAES_TEST
#include "aes_vectors.h"

static uint8
get_nonce_1st_byte(struct dot11_header *h)
{
	uint16 fc, subtype;
	uint16 qc;
	bool wds, qos;

	qc = 0;
	fc = ltoh16(h->fc);
	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	qos = (FC_TYPE(fc) == FC_TYPE_DATA) && (subtype & FC_SUBTYPE_QOS_DATA);

	if (qos) {
		qc = ltoh16(*((uint16 *)((uchar *)h +
			(wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN))));
	}

	/* nonce = priority octet || A2 || PN, !legacy */
	return (uint8)(QOS_TID(qc) & 0x0f);
}

int
main(int argc, char **argv)
{
	uint8 output[BUFSIZ], input2[BUFSIZ];
	int retv, k, fail = 0;
	uint32 rk[4 * (AES_MAXROUNDS + 1)];

#ifdef BCMAES_GENTABLE
	uint32 Te0test[256], Te1test[256], Te2test[256], Te3test[256], Te4test[256];
	uint32 Td0test[256], Td1test[256], Td2test[256], Td3test[256], Td4test[256];

	aes_tables_t at = {
		Te0test, Te1test, Te2test, Te3test, Te4test,
		Td0test, Td1test, Td2test, Td3test, Td4test
	};
#endif /* BCMAES_GENTABLE */

	/* AES-CBC */
	dbg(("%s: AES-CBC\n", *argv));
	for (k = 0; k < NUM_CBC_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_cbc_vec[k].key,
		                    AES_KEY_BITLEN(aes_cbc_vec[k].kl));
		retv = aes_cbc_encrypt(rk, aes_cbc_vec[k].kl,
		                       aes_cbc_vec[k].nonce, aes_cbc_vec[k].il,
		                       aes_cbc_vec[k].input, output);
		pres("AES CBC Encrypt: ", aes_cbc_vec[k].il, output);

		if (retv == -1) {
			dbg(("%s: aes_cbc_encrypt failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_cbc_vec[k].ref, aes_cbc_vec[k].il) != 0) {
			dbg(("%s: aes_cbc_encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupDec(rk, aes_cbc_vec[k].key,
		                    AES_KEY_BITLEN(aes_cbc_vec[k].kl));
		retv = aes_cbc_decrypt(rk, aes_cbc_vec[k].kl,
		                       aes_cbc_vec[k].nonce, aes_cbc_vec[k].il,
		                       aes_cbc_vec[k].ref, input2);
		pres("AES CBC Decrypt: ", aes_cbc_vec[k].il, input2);

		if (retv == -1) {
			dbg(("%s: aes_cbc_decrypt failed\n", *argv));
			fail++;
		}
		if (memcmp(aes_cbc_vec[k].input, input2, aes_cbc_vec[k].il) != 0) {
			dbg(("%s: aes_cbc_decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, full blocks */
	dbg(("%s: AES-CTR, full blocks\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, aes_ctr_vec[k].il,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Encrypt: ", aes_ctr_vec[k].il, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_ctr_vec[k].ref, aes_ctr_vec[k].il) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, aes_ctr_vec[k].il,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Decrypt: ", aes_ctr_vec[k].il, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(aes_ctr_vec[k].input, input2, aes_ctr_vec[k].il) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, one partial block */
	dbg(("%s: AES-CTR, one partial block\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, k+1,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Partial Block Encrypt: ", k+1, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_ctr_vec[k].ref, k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, k + 1,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Partial Block Decrypt: ", k + 1, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(aes_ctr_vec[k].input, input2, k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CTR, multi-block partial */
	dbg(("%s: AES-CTR, multi-block partial\n", *argv));
	for (k = 0; k < NUM_CTR_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, AES_BLOCK_SZ + NUM_CTR_VECTORS+k+1,
		                     aes_ctr_vec[k].input, output);
		pres("AES CTR Partial Multi-Block Encrypt: ",
		     AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, output);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_ctr_vec[k].ref, AES_BLOCK_SZ+NUM_CTR_VECTORS + k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ctr_vec[k].key,
		                    AES_KEY_BITLEN(aes_ctr_vec[k].kl));
		retv = aes_ctr_crypt(rk, aes_ctr_vec[k].kl,
		                     aes_ctr_vec[k].nonce, AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1,
		                     aes_ctr_vec[k].ref, input2);
		pres("AES CTR Partial Multi-Block Decrypt: ",
		     AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1, input2);

		if (retv) {
			dbg(("%s: aes_ctr_crypt failed\n", *argv));
			fail++;
		}
		if (memcmp(aes_ctr_vec[k].input, input2,
		         AES_BLOCK_SZ + NUM_CTR_VECTORS + k + 1) != 0) {
			dbg(("%s: aes_ctr_crypt decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CCM */
	dbg(("%s: AES-CCM\n", *argv));
	for (k = 0; k < NUM_CCM_VECTORS; k++) {
		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_mac(rk, aes_ccm_vec[k].kl,
		                   aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                   aes_ccm_vec[k].aad, aes_ccm_vec[k].il,
		                   aes_ccm_vec[k].input, output);

		if (retv) {
			dbg(("%s: aes_ccm_mac failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_ccm_vec[k].mac, AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccm_mac failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_encrypt(rk, aes_ccm_vec[k].kl,
		                       aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                       aes_ccm_vec[k].aad, aes_ccm_vec[k].il,
		                       aes_ccm_vec[k].input, output, &output[aes_ccm_vec[k].il]);
		pres("AES CCM Encrypt: ", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN, output);

		if (retv) {
			dbg(("%s: aes_ccm_encrypt failed\n", *argv));
			fail++;
		}
		if (memcmp(output, aes_ccm_vec[k].ref, aes_ccm_vec[k].il + AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccm_encrypt failed\n", *argv));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccm_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccm_vec[k].kl));
		retv = aes_ccm_decrypt(rk, aes_ccm_vec[k].kl,
		                       aes_ccm_vec[k].nonce, aes_ccm_vec[k].al,
		                       aes_ccm_vec[k].aad, aes_ccm_vec[k].il + AES_CCM_AUTH_LEN,
		                       aes_ccm_vec[k].ref, input2);
		pres("AES CCM Decrypt: ", aes_ccm_vec[k].il + AES_CCM_AUTH_LEN, input2);

		if (retv) {
			dbg(("%s: aes_ccm_decrypt failed\n", *argv));
			fail++;
		}
		if (memcmp(aes_ccm_vec[k].input, input2, aes_ccm_vec[k].il) != 0) {
			dbg(("%s: aes_ccm_decrypt failed\n", *argv));
			fail++;
		}
	}

	/* AES-CCMP */
	dbg(("%s: AES-CCMP\n", *argv));
	for (k = 0; k < NUM_CCMP_VECTORS; k++) {
		uint8 nonce_1st_byte;
		dbg(("%s: AES-CCMP vector %d\n", *argv, k));
		rijndaelKeySetupEnc(rk, aes_ccmp_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccmp_vec[k].kl));
		memcpy(output, aes_ccmp_vec[k].input, aes_ccmp_vec[k].il);
		nonce_1st_byte = get_nonce_1st_byte((struct dot11_header *)output);
		retv = aes_ccmp_encrypt(rk, aes_ccmp_vec[k].kl,
		                        aes_ccmp_vec[k].il, output,
		                        aes_ccmp_vec[k].flags[2],
		                        nonce_1st_byte);

		if (retv) {
			dbg(("%s: aes_ccmp_encrypt of vector %d returned error\n", *argv, k));
			fail++;
		}
		if (memcmp(output, aes_ccmp_vec[k].ref,
			aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN) != 0) {
			dbg(("%s: aes_ccmp_encrypt of vector %d reference mismatch\n", *argv, k));
			fail++;
		}

		rijndaelKeySetupEnc(rk, aes_ccmp_vec[k].key,
		                    AES_KEY_BITLEN(aes_ccmp_vec[k].kl));
		memcpy(output, aes_ccmp_vec[k].ref, aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN);
		nonce_1st_byte = get_nonce_1st_byte((struct dot11_header *)output);
		retv = aes_ccmp_decrypt(rk, aes_ccmp_vec[k].kl,
			aes_ccmp_vec[k].il+AES_CCM_AUTH_LEN, output,
			aes_ccmp_vec[k].flags[2],
			nonce_1st_byte);

		if (retv) {
			dbg(("%s: aes_ccmp_decrypt of vector %d returned error %d\n",
			     *argv, k, retv));
			fail++;
		}
		if (memcmp(output, aes_ccmp_vec[k].input, aes_ccmp_vec[k].il) != 0) {
			dbg(("%s: aes_ccmp_decrypt of vector %d reference mismatch\n", *argv, k));
			fail++;
		}
	}


#ifdef BCMAES_GENTABLE
	aes_gen_tables(AES_Sbox, AES_Inverse_Sbox, at);
	ptable("Te0", Te0test);
	ptable("Te1", Te1test);
	ptable("Te2", Te2test);
	ptable("Te3", Te3test);
	ptable("Te4", Te4test);

	ptable("Td0", Td0test);
	ptable("Td1", Td1test);
	ptable("Td2", Td2test);
	ptable("Td3", Td3test);
	ptable("Td4", Td4test);
#endif /* BCMAES_GENTABLE */

	fprintf(stderr, "%s: %s\n", *argv, fail ? "FAILED" : "PASSED");
	return (fail);
}

#endif /* BCMAES_TEST */
