/*
 * dxidec.c
 * Decoder tool for 802.11 encrypted packets
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

#include <typedefs.h>

#include <bcmcrypto/aes.h>
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/rijndael-alg-fst.h>
#include <bcmcrypto/tkhash.h>
#include <bcmcrypto/tkmic.h>
#include <bcmcrypto/wep.h>
#include <bcmutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>

#define true  TRUE 
#define false FALSE

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum alg_t {
	ALG_NONE=0,
	ALG_WEP=1,
	ALG_TKIP=2,
	ALG_AES_CCM=3,
	ALG_BIP=4
} alg_t;

typedef enum log_level {
	LL_ERR = 0,
	LL_WARN,
	LL_INFO,
	LL_DBG,
	LL_DBGV
} log_level_t;

typedef struct opts_t {
	const char* in_filename;
	const char* out_filename;
	uchar*      key;
	uint		key_sz;
	uchar*		tkip_rx_mic_key; /* sup->auth, to DS */
	uchar*		tkip_tx_mic_key; /* auth->sup, from DS */
	enum alg_t	alg;
	uint		debug;
	bool		tx;
} opts_t;

alg_t name_to_alg(const char* alg_name)
{
	alg_t alg;
	if (!strcasecmp(alg_name, "wep"))
		alg = ALG_WEP;
	else if (!strcasecmp(alg_name, "tkip"))
		alg = ALG_TKIP;
	else if (!strcasecmp(alg_name, "aes_ccm"))
		alg = ALG_AES_CCM;
	else if (!strcasecmp(alg_name, "bip"))
		alg = ALG_BIP;
	else
		alg = ALG_NONE;

	return alg;
}

const char* ll_to_str(log_level_t ll)
{
	switch (ll) {
		case LL_ERR:
			return "Error";
		case LL_WARN:
			return "Warning";
		case LL_INFO:
			return "Info";
		case LL_DBG:
			return "Debug";
		case LL_DBGV:
			return "Debug Verbose";
	};
	return "Internal Error"; 
}

void syserr(const char* fmt, ...)
{
	char serr[128];
	strerror_r(errno, serr, sizeof serr);
	fprintf(stderr, "Error '[%d] %s': ", errno, serr);
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

void report(log_level_t ll, uint line, const char* fmt, ... )
{
	if (ll > LL_INFO)
		fprintf(stderr, "Line %u: %s: ", line, ll_to_str(ll));
	else
		fprintf(stderr, "%s: ", ll_to_str(ll));

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "\n");
}
#define ERR(args...) do { report(LL_ERR, __LINE__, args); } while (0)
#define WARN(args...) do { report(LL_WARN, __LINE__, args); } while (0)
#define INFO(args...) do { report(LL_INFO,  __LINE__, args); } while (0)
#define DBG(args...) do {  if (opts->debug) \
		report(LL_DBG, __LINE__, args); } while (0)
#define DBGV(args...) do { if (opts->debug)  \
		report(LL_DBGV,  __LINE__, args); } while (0)

void usage(const char* p)
{
	fprintf(stderr, "usage: %s <args>\n", p);
	fprintf(stderr, "	-in <input filename> ; required with hex data w/ cv and fcs for decode but not for encode\n");
	fprintf(stderr, "	-out <output filename> ; defaults to stdout \n");
	fprintf(stderr, "	-key <key in ascii or hex>\n");
	fprintf(stderr, "	-alg <algorithm>\n");
	fprintf(stderr, "		wep; key is 5, 10 ascii or 13, 26 hex chars\n");
	fprintf(stderr, "		tkip ; key is 98 hex chars; tk.amick.smick\n");
	fprintf(stderr, "		aes_ccm; key is 32 hex chars\n");
	fprintf(stderr, "	-tx <value> - default is rx/decode\n");
	fprintf(stderr, "	-debug <non-zero value>\n");
}

unsigned h2i(int c)
{
        if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
        else if (c >= '0' && c <= '9')
                return c - '0';
        else
                return 0;
}

void hex2bin(char* in, uint isz, uchar* out, uint olim, uint* osz)
{
	*osz = 0;
	if (!in || !olim)
		return;

	int ch_hi;
	while (isz && olim) {
		int ch_lo;

		/* consume the first hex digit */
		ch_hi	= *in++; isz--;
		if (!ch_hi) break;
		if (!isxdigit(ch_hi)) continue;

		/* next digit */
		if (!isz) break;
		ch_lo = *in++; isz--;
		while (ch_lo && !isxdigit(ch_lo)) {
			ch_lo = *in++; isz--;
			if (!isz) break;
		}
		if (!ch_lo) break;
		*out++ =  h2i(ch_hi) << 4 | h2i(ch_lo); (*osz)++; olim--;
	}
}

/* calculates BIP; mic must be at least AES_BLOCK_SZ */
static void bip_calc(const struct dot11_header *hdr,
    const uint8 *data, int data_len, const uint8 *key, uint8 * mic)
{
    uint len;
	uint8* micdata; 
	uint16 fc; 

	memset(mic, 0 , AES_BLOCK_SZ);
	micdata = malloc(2 + 3 * ETHER_ADDR_LEN + data_len);
	if (!micdata) 
		return;

	fc = htol16(ltoh16(hdr->fc) & ~(FC_RETRY | FC_PM | FC_MOREDATA));

	len = 0;
    memcpy((char *)&micdata[len], (uint8 *)&fc, 2);
    len += 2;
    memcpy(&micdata[len], (uint8 *)&hdr->a1, ETHER_ADDR_LEN);
    len += ETHER_ADDR_LEN;
    memcpy(&micdata[len], (uint8 *)&hdr->a2, ETHER_ADDR_LEN);
    len += ETHER_ADDR_LEN;
    memcpy(&micdata[len], (uint8 *)&hdr->a3, ETHER_ADDR_LEN);
    len += ETHER_ADDR_LEN;

    memcpy(&micdata[len], data, data_len);
    len += data_len;

    aes_cmac_calc(micdata, len, key, BIP_KEY_SIZE, mic);
	free(micdata);
}

bool valid_ftype(const opts_t* opts, struct dot11_header *hdr, uint *type, 
	uint *subtype, bool *wds)
{
	uint ftype = (hdr->fc & FC_TYPE_MASK) >> FC_TYPE_SHIFT;
	uint fsubtype = (hdr->fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	
	if (ftype != FC_TYPE_DATA && ftype != FC_TYPE_MNG) {
		ERR("Not a management or data frame");
   		return false;
	}

	if (ftype == FC_TYPE_DATA && fsubtype != FC_SUBTYPE_DATA && 
		fsubtype != FC_SUBTYPE_QOS_DATA) {
		ERR("No support for non-data frames");
		return false;
	}

	if (ftype == FC_TYPE_MNG) {
		if (ETHER_ISMULTI(hdr->a1.octet)) {
			if (opts->alg != ALG_BIP) {
				ERR("Only ALG_BIP  is allowed for multicast management frames");
				return false;
			}
		}
		else {
			if (opts->alg != ALG_AES_CCM) {
				ERR("Only ALG_AES_CCM is allowed for unicast management frames");
				return false;	
			}
		}
	}

	*type = ftype;
	*subtype = fsubtype;
	*wds = (hdr->fc & FC_TODS) &&  (hdr->fc & FC_FROMDS);
	return true;
}


static bool tkip_enc(const opts_t *opts, struct dot11_header* hdr, uint qos,
					 uchar* data, uint data_len, 
	                 uchar* tk, uint tk_sz, uchar* mick, uint   mick_sz)
{
	uint32 iv32;
	uint16 iv16;
	uint16 p1_key[TKHASH_P1_KEY_SIZE/2];
	uchar rc4_key[TKHASH_P2_KEY_SIZE];
	rc4_ks_t rc4_ks;
	struct ether_header mic_eh;
	uint32 qos4 = qos & QOS_TID_MASK;
	uint32 mic_l, mic_r, l, r;
	uchar* p;

	assert(tk_sz == 16);
	assert(mick_sz == 8);

	if (data_len < (DOT11_IV_TKIP_LEN+1)) {
		ERR("Data length %d is too short for TKIP", data_len);
		return false;
	}

	if (!(data[3] & DOT11_EXT_IV_FLAG)) {
		ERR("Ext IV flag is not set for AES CCM");
		return false;
	}

	if (data[1] != ((data[0]|0x20)&0x7f)) {
		WARN("Mismatch for TSC1 vs. WEPSeed[1]. Fixing up");
		data[1] = (data[0]|0x20) & 0x7f;
	}

	iv32 = data[7] << 24 | data[6] << 16 | data[5] << 8 | data[4];
	tkhash_phase1(p1_key, tk, hdr->a2.octet, iv32);
	iv16 = data[0] << 8 | data[2];
	tkhash_phase2(rc4_key, tk, p1_key, iv16);

	prepare_key(rc4_key, TKHASH_P2_KEY_SIZE, &rc4_ks);

	/* skip past IV now  and compute the MIC*/
	data += DOT11_IV_TKIP_LEN;
	data_len -= DOT11_IV_TKIP_LEN;

	/* if this frame is part of a fragemented MSDU, skip  check */
	if ((hdr->fc & FC_MOREFRAG) || (hdr->seq & FRAGNUM_MASK)) {
		INFO("Skipping TKIP MIC computing for the fragment.");
		return true;
	}

	p = mick;
	mic_l = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	mic_r = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
	DBG("TKIP MIC: key %08x.%08x", mic_l, mic_r);

	memcpy(mic_eh.ether_dhost, hdr->fc & FC_TODS ? 
				hdr->a3.octet : hdr->a1.octet,  ETHER_ADDR_LEN);
	if ((hdr->fc & FC_FROMDS) && (hdr->fc & FC_TODS))
		memcpy(mic_eh.ether_shost, hdr->a4.octet, ETHER_ADDR_LEN);
	else if (hdr->fc & FC_FROMDS)
		 memcpy(mic_eh.ether_shost, hdr->a3.octet, ETHER_ADDR_LEN);
	else
		 memcpy(mic_eh.ether_shost, hdr->a2.octet, ETHER_ADDR_LEN);

	p = mic_eh.ether_shost;
	DBG("TKIP MIC: SA %02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3],
				p[4], p[5]);
	p = mic_eh.ether_dhost;
	DBG("TKIP MIC: DA %02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3],
				p[4], p[5]);

	tkip_mic(mic_l, mic_r, 2*ETHER_ADDR_LEN, (uint8*)&mic_eh, &l, &r);
	mic_l = l; mic_r = r;
	tkip_mic(mic_l, mic_r, 4, (uint8*)&qos4, &l, &r);
	mic_l = l; mic_r = r;

	/* pad as required */
	uint pad_data_len = tkip_mic_eom(data, data_len, 0);
	tkip_mic(mic_l, mic_r, pad_data_len, data, &l, &r);

	/* append the TKIP MIC */
	p = &data[data_len];
	*p++ = l & 0xff; *p++ = l >> 8 & 0xff; *p++ = l >> 16 & 0xff;
	*p++ = l >> 24 & 0xff;
	*p++ = r & 0xff; *p++ = r >> 8 & 0xff; *p++ = r >> 16 & 0xff;
	*p++ = r >> 24 & 0xff;
	data_len += TKIP_MIC_SIZE;

	/* append the ICV */
	uint32 icv = ~hndcrc32(data, data_len, CRC32_INIT_VALUE);
	data[data_len++] = icv & 0xff;
	data[data_len++] = icv >> 8 & 0xff;
	data[data_len++] = icv >> 16 & 0xff;
	data[data_len++] = icv >> 24 & 0xff;

	rc4(data, data_len, &rc4_ks);
	return true;
}

static bool tkip_dec(const opts_t *opts, struct dot11_header* hdr, uint qos,
					 uchar* data, uint data_len, 
	                 uchar* tk, uint tk_sz, uchar* mick, uint   mick_sz)
{
	uint32 iv32;
	uint16 iv16;
	uint16 p1_key[TKHASH_P1_KEY_SIZE/2];
	uchar rc4_key[TKHASH_P2_KEY_SIZE];
	rc4_ks_t rc4_ks;
	struct ether_header mic_eh;
	uint32 qos4 = qos & QOS_TID_MASK;
	uint32 mic_l, mic_r, l, r, pkt_l, pkt_r;
	uchar* p;

	assert(tk_sz == 16);
	assert(mick_sz == 8);

	if (data_len < (DOT11_IV_TKIP_LEN+TKIP_MIC_SIZE)) {
		ERR("Data length %d is too short for TKIP", data_len);
		return false;
	}

	if (!(data[3] & DOT11_EXT_IV_FLAG)) {
		ERR("Ext IV flag is not set for AES CCM");
		return false;
	}

	if (data[1] != ((data[0]|0x20)&0x7f)) {
		WARN("Mismatch for TSC1 vs. WEPSeed[1]");
	}

	iv32 = data[7] << 24 | data[6] << 16 | data[5] << 8 | data[4];
	tkhash_phase1(p1_key, tk, hdr->a2.octet, iv32);
	iv16 = data[0] << 8 | data[2];
	tkhash_phase2(rc4_key, tk, p1_key, iv16);

	prepare_key(rc4_key, TKHASH_P2_KEY_SIZE, &rc4_ks);
	rc4(data+DOT11_IV_TKIP_LEN, data_len-DOT11_IV_TKIP_LEN, &rc4_ks);
	if (hndcrc32(data+DOT11_IV_TKIP_LEN, data_len-DOT11_IV_TKIP_LEN, CRC32_INIT_VALUE) !=
			CRC32_GOOD_VALUE) {
		ERR("RC4 decryption failed after key mixing.");
		return false;
	}

	/* done with IV and ICV, lose it now */
	data += DOT11_IV_TKIP_LEN; data_len -= DOT11_IV_TKIP_LEN;
	data_len -= DOT11_IV_LEN;

	/* if this frame is part of a fragemented MSDU, skip  check */
	if ((hdr->fc & FC_MOREFRAG) || (hdr->seq & FRAGNUM_MASK)) {
		INFO("Skipping TKIP MIC check for the fragment.");
		return true;
	}

	p = mick;
	mic_l = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	mic_r = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
	DBG("TKIP MIC: key %08x.%08x", mic_l, mic_r);

	memcpy(mic_eh.ether_dhost, hdr->fc & FC_TODS ? 
				hdr->a3.octet : hdr->a1.octet,  ETHER_ADDR_LEN);
	if ((hdr->fc & FC_FROMDS) && (hdr->fc & FC_TODS))
		memcpy(mic_eh.ether_shost, hdr->a4.octet, ETHER_ADDR_LEN);
	else if (hdr->fc & FC_FROMDS)
		 memcpy(mic_eh.ether_shost, hdr->a3.octet, ETHER_ADDR_LEN);
	else
		 memcpy(mic_eh.ether_shost, hdr->a2.octet, ETHER_ADDR_LEN);

	p = mic_eh.ether_shost;
	DBG("TKIP MIC: SA %02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3],
				p[4], p[5]);
	p = mic_eh.ether_dhost;
	DBG("TKIP MIC: DA %02x:%02x:%02x:%02x:%02x:%02x", p[0], p[1], p[2], p[3],
				p[4], p[5]);

	tkip_mic(mic_l, mic_r, 2*ETHER_ADDR_LEN, (uint8*)&mic_eh, &l, &r);
	mic_l = l; mic_r = r;
	tkip_mic(mic_l, mic_r, 4, (uint8*)&qos4, &l, &r);
	mic_l = l; mic_r = r;

	/* keep received MIC */
	p = &data[data_len-TKIP_MIC_SIZE];
	pkt_l = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
	pkt_r = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);

	DBG("Received MIC %08x %08x", pkt_l, pkt_r);
	data_len -= TKIP_MIC_SIZE;

	/* pad as required */
	data_len = tkip_mic_eom(data, data_len, 0);
	tkip_mic(mic_l, mic_r, data_len, data, &l, &r);
	if (l != pkt_l || r != pkt_r) {
		ERR("TKIP MIC check failed. Expected %08x.%08x. Received %08x.%08x",
			l, r, pkt_l, pkt_r);
		return false;
	}

	return true;
}

static bool aes_ccm_enc(const opts_t* opts, struct dot11_header* hdr, uint qos, 
	uchar* data, uint data_len, uchar* key, uint ksz)
{
	int rc;
	uint8* edata;
	uint edata_len;
	uchar aad[30];
	uint aad_len; 
	uint ftype;
	uint fsubtype;
	bool has_a4;
	uint fc, seq;
	uchar nonce[13];
	uint32 rk[4*(AES_MAXROUNDS + 1)];

	assert(data_len >= DOT11_IV_AES_CCM_LEN);

	if (!(data[3] & DOT11_EXT_IV_FLAG)) {
		ERR("Ext IV flag is not set for AES CCM");
		return false;
	}

	aad_len = 0;
	memset(nonce, 0, sizeof nonce);

	/* mask fc fields * endian*/
	if (!valid_ftype(opts, hdr, &ftype, &fsubtype, &has_a4)) {
		ERR("Internal error: bad frame type during aes ccm encode");
		return false;
	}

	fc = ltoh16(hdr->fc); 
	fc &= ~(AES_CCMP_FC_RETRY|AES_CCMP_FC_PM|AES_CCMP_FC_MOREDATA);
	if (ftype != FC_TYPE_MNG)
		fc &= ~AES_CCMP_SUBTYPE_LOW_MASK;
		
	seq = hdr->seq & FRAGNUM_MASK;

	aad[aad_len++] = fc & 0xff;
	aad[aad_len++] = (fc >> 8) & 0xff; 
	memcpy(aad+aad_len, hdr->a1.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	memcpy(aad+aad_len, hdr->a2.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	memcpy(aad+aad_len, hdr->a3.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	aad[aad_len++] = seq & 0xff;
	aad[aad_len++] = (seq >> 8) & 0xff;
	if (has_a4) {
		memcpy(aad+aad_len, hdr->a4.octet, ETHER_ADDR_LEN);
		aad_len += ETHER_ADDR_LEN; 
	}
	if (ftype == FC_TYPE_DATA && fsubtype == FC_SUBTYPE_QOS_DATA) {
		qos &= QOS_TID_MASK;
		aad[aad_len++] = qos & 0xff;
		aad[aad_len++] = (qos >> 8) & 0xff;
	}

	nonce[0] = qos & 0xff;
	if (ftype == FC_TYPE_MNG)
		nonce[0] |= AES_CCMP_NF_MANAGEMENT;

	memcpy(&nonce[1], hdr->a2.octet, ETHER_ADDR_LEN);
	nonce[7] =  data[7]; nonce[8] =  data[6];
	nonce[9] =  data[5]; nonce[10] = data[4];
	nonce[11] = data[1]; nonce[12] = data[0];

	edata =  data + DOT11_IV_AES_CCM_LEN;
	edata_len = data_len - DOT11_IV_AES_CCM_LEN;
	rijndaelKeySetupEnc(rk, key, AES_KEY_BITLEN(ksz));
	rc = aes_ccm_encrypt(rk, ksz, nonce, aad_len, aad, edata_len, edata, 
						 edata, &edata[edata_len]);
	return !rc;
}

static bool aes_ccm_dec(const opts_t* opts, struct dot11_header* hdr, uint qos, 
	uchar* data, uint data_len, uchar* key, uint ksz)
{
	int rc;
	uint8* edata;
	uint edata_len;
	uchar aad[30];
	uint aad_len; 
	uint ftype;
	uint fsubtype;
	bool has_a4;
	uint fc, seq;
	uchar nonce[13];
	uint32 rk[4*(AES_MAXROUNDS + 1)];

	assert(data_len >= (DOT11_IV_AES_CCM_LEN+DOT11_ICV_AES_LEN));

	if (!(data[3] & DOT11_EXT_IV_FLAG)) {
		ERR("Ext IV flag is not set for AES CCM");
		return false;
	}

	aad_len = 0;
	memset(nonce, 0, sizeof nonce);

	/* mask fc fields * endian*/
	if (!valid_ftype(opts, hdr, &ftype, &fsubtype, &has_a4)) {
		ERR("Internal error: bad frame type during aes ccm decode");
		return false;
	}

	fc = ltoh16(hdr->fc); 
	fc &= ~(AES_CCMP_FC_RETRY|AES_CCMP_FC_PM|AES_CCMP_FC_MOREDATA);
	if (ftype != FC_TYPE_MNG)
		fc &= ~AES_CCMP_SUBTYPE_LOW_MASK;

	seq = hdr->seq & FRAGNUM_MASK;

	aad[aad_len++] = fc & 0xff;
	aad[aad_len++] = (fc >> 8) & 0xff; 
	memcpy(aad+aad_len, hdr->a1.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	memcpy(aad+aad_len, hdr->a2.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	memcpy(aad+aad_len, hdr->a3.octet, ETHER_ADDR_LEN); 
	aad_len += ETHER_ADDR_LEN;
	aad[aad_len++] = seq & 0xff;
	aad[aad_len++] = (seq >> 8) & 0xff;
	if (has_a4) {
		memcpy(aad+aad_len, hdr->a4.octet, ETHER_ADDR_LEN);
		aad_len += ETHER_ADDR_LEN; 
	}

	if (ftype == FC_TYPE_DATA && fsubtype == FC_SUBTYPE_QOS_DATA) {
		qos &= QOS_TID_MASK;
		aad[aad_len++] = qos & 0xff;
		aad[aad_len++] = (qos >> 8) & 0xff;
	}

	nonce[0] = qos & 0xff;
	if (ftype == FC_TYPE_MNG)
		nonce[0] |= AES_CCMP_NF_MANAGEMENT;
	memcpy(&nonce[1], hdr->a2.octet, ETHER_ADDR_LEN);
	nonce[7] =  data[7]; nonce[8] =  data[6];
	nonce[9] =  data[5]; nonce[10] = data[4];
	nonce[11] = data[1]; nonce[12] = data[0];

	edata =  data + DOT11_IV_AES_CCM_LEN;
	edata_len = data_len - DOT11_IV_AES_CCM_LEN;
	rijndaelKeySetupEnc(rk, key, AES_KEY_BITLEN(ksz));
	rc = aes_ccm_decrypt(rk, ksz, nonce, aad_len, aad, edata_len, edata, edata);
	return !rc;
}

static bool bip_enc(const opts_t* opts, struct dot11_header* hdr, uint qos, 
	uchar* data, uint data_len, uchar* key, uint ksz)
{
	uint8  mic[AES_BLOCK_SZ];
	uint8* bmicp = data + data_len - BIP_MIC_SIZE;

	assert(data_len >= BIP_MIC_SIZE);
	memset(bmicp, 0, BIP_MIC_SIZE);
	bip_calc(hdr, data, data_len, key, mic);
	memcpy(bmicp, mic, BIP_MIC_SIZE);
	return true;
}

static bool bip_dec(const opts_t* opts, struct dot11_header* hdr, uint qos, 
	uchar* data, uint data_len, uchar* key, uint ksz)
{
	uint8  mic[AES_BLOCK_SZ];
    uint8* bmicp = data + data_len - BIP_MIC_SIZE;
	uint8 saved_mic[BIP_MIC_SIZE];

	assert(data_len >= BIP_MIC_SIZE);
	memcpy(saved_mic, bmicp, BIP_MIC_SIZE);
	memset(bmicp, 0, BIP_MIC_SIZE);
	bip_calc(hdr, data, data_len, key, mic);
	memcpy(bmicp, saved_mic, BIP_MIC_SIZE);

	if (memcmp(mic, saved_mic, BIP_MIC_SIZE)) {
		ERR("BIP MIC check failed. expected %02x%02x%02x%02x%02x%02x%02x%02x "
			"received  %02x%02x%02x%02x%02x%02x%02x%02x",
			mic[0], mic[1], mic[2], mic[3], mic[4], 
			mic[5], mic[6], mic[7], 
			saved_mic[0], saved_mic[1], saved_mic[2], 
			saved_mic[3], saved_mic[4], saved_mic[5], 
			saved_mic[6], saved_mic[7]);
		return false;
	}

	return true;
}

uchar* encode(const opts_t* opts, const uchar* in, uint isz, uint *osz)
{
	const size_t out_lim = (isz+63)/2;
	uchar* out = malloc(out_lim);
	struct dot11_header* hdr =0;
	uchar* data = 0;
	uint qos = 0;

	*osz=0;
	hex2bin((char*)in, isz, out, out_lim, osz);
	do {
		uint ftype, fsubtype;
		bool is_wds;	
		
		if (*osz < DOT11_A3_HDR_LEN) {
			ERR("Input size %d is too small", *osz);
			break;
		}
		hdr = (struct dot11_header*)out; /* endian */
		if (!valid_ftype(opts, hdr, &ftype, &fsubtype, &is_wds))
			break;
		
		data = &out[is_wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN];
		if (ftype == FC_TYPE_DATA && fsubtype == FC_SUBTYPE_QOS_DATA) {
			qos = (data[1] << 8) | data[0]; /* endian */
			data += 2;
		}

		if (ftype == FC_TYPE_DATA)
			hdr->fc |= FC_WEP;
		else if (ftype == FC_TYPE_MNG) {
			if (!ETHER_ISMULTI((uint8*)&hdr->a1))
				hdr->fc |= FC_WEP;
		}

		uint data_len = out + *osz - data;
		DBG("out=%p, data=%p, dlen=%lu, osz=%lu", out, data, data_len, *osz);

		bool ok = true;
		switch (opts->alg) {
			case ALG_WEP:
				if (data_len < DOT11_IV_LEN) {
					ERR("No IV for WEP encode");
					break;
				}
				wep_encrypt(data_len, data, opts->key_sz, opts->key);
				data_len += DOT11_ICV_LEN;
				break;
			case ALG_TKIP:
				ok = tkip_enc(opts, hdr, qos, data, data_len, opts->key, opts->key_sz,
								((hdr->fc & FC_FROMDS)? opts->tkip_tx_mic_key :
							  		opts->tkip_rx_mic_key), 
								opts->key_sz/2); 
				if (ok)
					data_len += TKIP_MIC_SIZE + DOT11_ICV_LEN;
				break;
			case ALG_AES_CCM:
				ok = aes_ccm_enc(opts, hdr, qos, data, data_len, opts->key, opts->key_sz);
				if (ok)
					data_len +=  DOT11_ICV_AES_LEN;
				break;
			case ALG_NONE:
				DBG("No algorithm was specified");
				ok = false;
				break;
			case ALG_BIP:
				ok = bip_enc(opts, hdr, qos, data, data_len, opts->key, opts->key_sz);
				break;
			default:
				ERR("Unknown algorithm");
				break;
		}

		if (ok) {
			DBG("Done encoding");
			memset(&data[data_len], 0, DOT11_FCS_LEN);
			data_len += DOT11_FCS_LEN;
			*osz = data_len + (data - out);
		} else
			DBG("Error encrypting");

	} while(0);

	return out;
}

uchar* decode(const opts_t* opts, const uchar* in, uint isz, uint *osz)
{
	const size_t out_lim = (isz+1)/2;
	uchar* out = malloc(out_lim);
	struct dot11_header* hdr =0;
	uchar* data = 0;
	uint qos = 0;

	*osz=0;
	hex2bin((char*)in, isz, out, out_lim, osz);
	do {
		uint ftype, fsubtype;
		bool is_wds;
		
		if (*osz < DOT11_A3_HDR_LEN) {
			ERR("Input size %d is too small", *osz);
			break;
		}
		hdr = (struct dot11_header*)out; /* endian */
		if (!valid_ftype(opts, hdr, &ftype, &fsubtype, &is_wds))
			break;

		data = &out[is_wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN];
		if (ftype == FC_TYPE_DATA && fsubtype == FC_SUBTYPE_QOS_DATA) {
			qos = (data[1] << 8) | data[0]; /* endian */
			data += 2;
		}

		if (!(hdr->fc & FC_WEP)) { /* no privacy? */
			if (ftype == FC_TYPE_DATA) {
				WARN("Protected frame bit not set");
			} else if (ftype == FC_TYPE_MNG) {
				if (!ETHER_ISMULTI((uint8*)&hdr->a1))
					WARN("Protected frame bit not set");
			}
			/* go on */
		}

		uint data_len = out + *osz - data - DOT11_FCS_LEN;
		DBG("out=%p, data=%p, dlen=%lu, osz=%lu", out, data, data_len, *osz);
		bool ok = true;
		switch (opts->alg) {
			case ALG_WEP:
				if (data_len < DOT11_IV_LEN) {
					ERR("No IV for WEP decode");
					break;
				}
				ok = wep_decrypt(data_len, data, opts->key_sz, opts->key);
				break;
			case ALG_TKIP:
				ok = tkip_dec(opts, hdr, qos, data, data_len, opts->key, opts->key_sz,
								((hdr->fc & FC_FROMDS)? opts->tkip_tx_mic_key :
							  		opts->tkip_rx_mic_key), 
								opts->key_sz/2);
				break;
			case ALG_AES_CCM:
				ok = aes_ccm_dec(opts, hdr, qos, data, data_len, opts->key, opts->key_sz);
				break;
			case ALG_NONE:
				ERR("No algorithm specified");
				ok = false;
				break;
			case ALG_BIP:
				ok = bip_dec(opts, hdr, qos, data, data_len, opts->key, opts->key_sz);
				break;
			default:
				ERR("Unknown algorithm");
				break;
		}

		if (!ok)
		 	ERR("Decoding failed");
		else
			DBG("Done decoding");
	} while(0);
	
	return out;
}

bool output(int ofd, uchar* out, size_t osz)
{
	if (!out)
		return true;

	uint nw=0;
	while (osz--) {
		if (nw %16 == 0) {
			char lbuf[16];
			if (nw)
				write(ofd, "\n", 1);
			sprintf(lbuf, " %04d: ", nw);
			write(ofd, lbuf, strlen(lbuf));
		}
			
		char hbuf[4];
		sprintf(hbuf, "%02x ", *out++);
		write(ofd, hbuf, 3);
		nw++;
	}
	write(ofd, "\n", 1);
	return true;
}

bool parse_opts(opts_t* opts, int argc, char** argv)
{
	int i;
	memset(opts, 0, sizeof(opts_t));
	for (i = 1; i < argc; i += 2) {
		const char* arg = argv[i];
		const char* val = 0;
		if (i+1 >= argc)
			return false;
		val = argv[i+1];

		if (!strncasecmp(arg, "-input", 2)) {
			opts->in_filename = val;
		} else if (!strncasecmp(arg, "-output", 2)) {
			opts->out_filename = val;
		} else if (!strncasecmp(arg, "-key", 2)) {
			if (!strncasecmp(val, "0x", 2))
				val += 2;
			opts->key_sz = strlen(val);
			opts->key = malloc(opts->key_sz);
			memcpy(opts->key, val, opts->key_sz);
			switch (opts->key_sz) {
				case 5: case 13: /* ASCII WEP 64, 128 */
					break;
				/* Hex WEP 128, AES CCM (128), TKIP */
				default:
					WARN("Key size %lu may be invalid.", opts->key_sz);
				case 26:  /* WEP 128 */
				case 32:  /* AES CCM */
				case 64:  /* TKIP */
					hex2bin((char*)opts->key, opts->key_sz, opts->key, 
							 opts->key_sz, &opts->key_sz);
					break;
			}
		} else if (!strncasecmp(arg, "-alg", 2)) {
			opts->alg = name_to_alg(val);
			if (opts->alg == ALG_NONE) {
				ERR("Invalid algorithm '%s'", val);
				return false;
			}
		} else if (!strncasecmp(arg, "-debug", 2)) {
			opts->debug = strtoul(val, NULL, 10);
		} else if (!strncasecmp(arg, "-tx", 3)) {
			opts->tx =  strtoul(val, NULL, 10);
		} else {
			ERR("Invalid option specified");
			return false;
		}
	}

	/* Some error checks */
	if (!opts->in_filename)
		return false;

	if (opts->alg != ALG_NONE && !opts->key) {
		ERR("Key must be specified with an algorithm");
		return false;
	}

	if (opts->alg == ALG_WEP) {
		if (opts->key_sz != 5 && opts->key_sz != 13) {
			ERR("Key size must be 40 or 104 bits with WEP");
			return false;
		}
	} else if (opts->alg == ALG_TKIP) {
		if (opts->key_sz != 32) {
			ERR("Invalid key size %d for TKIP", opts->key_sz);
			return false;
		}
		/* fix up tkip keys */
		opts->tkip_tx_mic_key = &opts->key[16];
		opts->tkip_rx_mic_key = &opts->key[24];
		opts->key_sz = 16;
	} else if (opts->alg == ALG_AES_CCM && opts->key_sz != 16) {
		ERR("Invalid key size for AES CCM");
		return false;
	}

	return true;
}

int
main(int argc, char **argv)
{
	int ret = -1;
	opts_t s_opts, *opts=&s_opts;
	struct stat stbuf;
	int in_fd = -1;
	size_t in_bufsz = 0;
	uchar *in_buf = 0;
	int out_fd = -1;
	uchar *out_buf = 0;
	uint out_bufsz = 0;

	if (!parse_opts(opts, argc, argv)) {
		usage(argv[0]);
		goto cleanup;
	}

	in_fd = open(opts->in_filename, O_RDONLY);
	if (in_fd < 0) {
		syserr("error opening input file %s", opts->in_filename);
		goto cleanup;
	}

	if (fstat(in_fd, &stbuf) < 0) {
		syserr("error stating input file %s", opts->in_filename);
		goto cleanup;
	}

	in_bufsz = stbuf.st_size;
	in_buf = malloc(in_bufsz);
	if (!in_buf) {
		syserr("allocating input buffer");
		goto cleanup;
	}

	if ((in_bufsz = read(in_fd, in_buf, in_bufsz)) < 0) {
		syserr("reading input file");
		goto cleanup;
	}

	assert(in_bufsz == stbuf.st_size);

	out_fd = opts->out_filename ? 
					open(opts->out_filename, O_WRONLY|O_CREAT, 0644) : 1;
	if (out_fd < 0) {
		syserr("error opening output file %s", opts->out_filename);
		goto cleanup;
	}	

	out_buf = opts->tx ? encode(opts, in_buf, in_bufsz, &out_bufsz) :
						 decode(opts, in_buf, in_bufsz, &out_bufsz);
	if (!out_buf || !out_bufsz) {
		ERR("decode failure");
		goto cleanup;
	}

	if (!output(out_fd, out_buf, out_bufsz)) {
		ERR("write output file");
		goto cleanup;
	}
	ret = 0;

cleanup:
	if (in_fd != -1)
		close(in_fd);
	if (out_fd != -1)
		close(out_fd);
	if (out_buf != in_buf)
		free(out_buf);
	free(in_buf);
	free(opts->key);
	return ret;
}
