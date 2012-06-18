/*
 *	rsa.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	RSA crypto
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/

#include "../cryptoLayer.h"

/******************************************************************************/

static int32 ssl_rsa_crypt(psPool_t *pool, 
						const unsigned char *in, uint32 inlen,
						unsigned char *out, uint32 *outlen, 
						sslRsaKey_t *key, int32 type);

static int32 sslUnpadRSA(unsigned char *in, int32 inlen, unsigned char *out,
						 int32 outlen, int32 decryptType);
static int32 sslPadRSA(unsigned char *in, int32 inlen, unsigned char *out,
					   int32 outlen, int32 cryptType);

#ifdef USE_RSA_BLINDING
static int32 tim_mp_exptmod(psPool_t *pool, 
						mp_int *c, mp_int *e, mp_int *d, mp_int *n, mp_int *m);
#else
#define tim_mp_exptmod(p, c, e, d, n, m) mp_exptmod(p, c, d, n, m)
#endif

/******************************************************************************/

#define RSA_PUBLIC		0x01
#define RSA_PRIVATE		0x02

/******************************************************************************/
/*
	Public API wrapper around sslGetEntropy
*/
int32 matrixGetRandomBytes(unsigned char *bytes, int32 size)
{
		return sslGetEntropy(bytes, size);
}

/******************************************************************************/
/*
	Primary RSA encryption routine
*/
static int32 ssl_rsa_crypt(psPool_t *pool, 
						const unsigned char *in, uint32 inlen,
						unsigned char *out, uint32 *outlen, 
						sslRsaKey_t *key, int32 type)
{
	mp_int			tmp, tmpa, tmpb;
	unsigned long	x;
	int32			res;

	if (in == NULL || out == NULL || outlen == NULL || key == NULL) {
		return -1;
	}
/*
	init and copy into tmp
 */
	if (_mp_init_multi(pool, &tmp, &tmpa, &tmpb, NULL, NULL, NULL, NULL, NULL)
		!= MP_OKAY) {
		matrixStrDebugMsg("ssl_rsa_crypt error: mp_init_multi\n", NULL);
		goto error;
	}
	if (mp_read_unsigned_bin(&tmp, (unsigned char *)in, (int32)inlen) != 
			MP_OKAY) {
		matrixStrDebugMsg("ssl_rsa_crypt error: mp_read_unsigned_bin\n", NULL);
		goto error; 
	}
/*
	sanity check on the input
 */
	if (mp_cmp(&key->N, &tmp) == MP_LT) {
		res = -1;
		goto done;
	}
	if (type == RSA_PRIVATE) {
		if (key->optimized) {
			if (tim_mp_exptmod(pool, &tmp, &key->e, &key->dP, &key->p, &tmpa)
					!= MP_OKAY) {
				matrixStrDebugMsg("decrypt error: mp_exptmod dP, p\n", NULL);
				goto error;
			}
			if (tim_mp_exptmod(pool, &tmp, &key->e, &key->dQ, &key->q, &tmpb)
					!= MP_OKAY) {
				matrixStrDebugMsg("decrypt error: mp_exptmod dQ, q\n", NULL);
				goto error;
			}
			if (mp_sub(&tmpa, &tmpb, &tmp) != MP_OKAY) {
				matrixStrDebugMsg("decrypt error: sub tmpb, tmp\n", NULL);
				goto error;
			}
			if (mp_mulmod(pool, &tmp, &key->qP, &key->p, &tmp) != MP_OKAY) {
				matrixStrDebugMsg("decrypt error: mp_mulmod qP, p\n", NULL);
				goto error;
			}
			if (mp_mul(pool, &tmp, &key->q, &tmp) != MP_OKAY) {
				matrixStrDebugMsg("decrypt error: mp_mul q \n", NULL);
				goto error;
			}
			if (mp_add(&tmp, &tmpb, &tmp) != MP_OKAY) {
				matrixStrDebugMsg("decrypt error: mp_add tmp \n", NULL);
				goto error;
			}
		} else {
			if (tim_mp_exptmod(pool, &tmp, &key->e, &key->d, &key->N, &tmp) != MP_OKAY) {
				matrixStrDebugMsg("ssl_rsa_crypt error: mp_exptmod\n", NULL);
				goto error;
			}
		}
	} else if (type == RSA_PUBLIC) {
		if (mp_exptmod(pool, &tmp, &key->e, &key->N, &tmp) != MP_OKAY) {
			matrixStrDebugMsg("ssl_rsa_crypt error: mp_exptmod\n", NULL);
			goto error;
		}
	} else {
		matrixStrDebugMsg("ssl_rsa_crypt error: invalid type param\n", NULL);
		goto error;
	}
/*
	read it back
 */
	x = (unsigned long)mp_unsigned_bin_size(&key->N);
	if (x > *outlen) {
		res = -1;
		matrixStrDebugMsg("ssl_rsa_crypt error: mp_unsigned_bin_size\n", NULL);
		goto done;
	}
/*
	We want the encrypted value to always be the key size.  Pad with 0x0
*/
	while (x < (unsigned long)key->size) {
		*out++ = 0x0;
		x++;
	}

	*outlen = x;
/*
	convert it
 */
	memset(out, 0x0, x);
	if (mp_to_unsigned_bin(pool, &tmp, out+(x-mp_unsigned_bin_size(&tmp)))
			!= MP_OKAY) {
		matrixStrDebugMsg("ssl_rsa_crypt error: mp_to_unsigned_bin\n", NULL);
		goto error;
	}
/*
	clean up and return
 */
	res = 0;
	goto done;
error:
	res = -1;
done:
	_mp_clear_multi(&tmp, &tmpa, &tmpb, NULL, NULL, NULL, NULL, NULL);
	return res;
}

/******************************************************************************/
/*
	Pad a value to be encrypted by RSA, according to PKCS#1 v1.5
	http://www.rsasecurity.com/rsalabs/pkcs/pkcs-1/
	When encrypting a value with RSA, the value is first padded to be 
	equal to the public key size using the following method:
		00 <id> <data> 00 <value to be encrypted>
	- id denotes a public or private key operation
	- if id is private, data is however many non-zero bytes it takes to pad the
		value to the key length (randomLen = keyLen - 3 - valueLen).
	- if id is public, data is FF for the same length as described above
	- There must be at least 8 bytes of data.
*/
static int32 sslPadRSA(unsigned char *in, int32 inlen, unsigned char *out,
					   int32 outlen, int32 cryptType)
{
	unsigned char	*c;
	int32			randomLen;
	
	randomLen = outlen - 3 - inlen;
	if (randomLen < 8) {
		matrixIntDebugMsg("RSA encryption data too large: %d\n", inlen);
		return -1;
	}
	c = out;
	*c = 0x00;
	c++;
	*c = (unsigned char)cryptType;
	c++;
	if (cryptType == RSA_PUBLIC) {
		while (randomLen-- > 0) {
			*c++ = 0xFF;
		}
	} else {
		if (sslGetEntropy(c, randomLen) < 0) {
			matrixStrDebugMsg("Error gathering RSA pad entropy\n", NULL);
			return -1;
		}
/*
		SECURITY:  Read through the random data and change all 0x0 to 0x01.
		This is per spec that no random bytes should be 0
*/
		while (randomLen-- > 0) {
			if (*c == 0x0) {
				*c = 0x01;
			}
			c++;
		}
	}
	*c = 0x00;
	c++;
	memcpy(c, in, inlen);
	
	return outlen;
}


#ifdef USE_RSA_PUBLIC_ENCRYPT
/******************************************************************************/
/*
	RSA public encryption.
	Always called by SSL client for server auth in ClientKeyExchange
	The outlen param must be set to the strength of the key:  key->size
*/
int32 matrixRsaEncryptPub(psPool_t *pool, sslRsaKey_t *key, 
						unsigned char *in, int32 inlen,
						unsigned char *out, int32 outlen)
{
	int32	size;

	size = key->size;
	if (outlen < size) {
		return -1;
	}
	
	if (sslPadRSA(in, inlen, out, size, RSA_PRIVATE) < 0) {
		return -1;
	}
	if (ssl_rsa_crypt(pool, out, size, out, (uint32*)&outlen, key,
			RSA_PUBLIC) < 0 || outlen != size) {
		return -1;
	}
	return size;
}

#else  /* USE_RSA_PUBLIC_ENCRYPT - Keeps the cipher suite definition clean */
int32 matrixRsaEncryptPub(psPool_t *pool, sslRsaKey_t *key, 
						unsigned char *in, int32 inlen,
						unsigned char *out, int32 outlen)
{
	if (inlen > outlen) {
		return -1;
	}
	memcpy(out, in, inlen);
	return inlen;
}
#endif /* USE_RSA_PUBLIC_ENCRYPT */

/******************************************************************************/
/*
	Unpad a value decrypted by RSA, according to PKCS#1 v1.5
	http://www.rsasecurity.com/rsalabs/pkcs/pkcs-1/
	
	When decrypted, the data will look like the pad, including the inital
	byte (00).  Form:
		00 <decryptType> <random data (min 8 bytes)> 00 <value to be encrypted>

	We don't worry about v2 rollback issues because we don't support v2
*/
static int32 sslUnpadRSA(unsigned char *in, int32 inlen, unsigned char *out, 
						int32 outlen, int32 decryptType)
{
	unsigned char	*c, *end;

	if (inlen < outlen + 10) {
		return -1;
	}
	c = in;
	end = in + inlen;
/*
	Verify the first byte (block type) is correct.
*/
	if (*c++ != 0x00 || *c != decryptType) {
		return -1;
	}
	c++;
/*
	Skip over the random, non-zero bytes used as padding
*/
	while (c < end && *c != 0x0) {
		if (decryptType == RSA_PUBLIC) {
			if (*c != 0xFF) {
				return -1;
			}
		}
		c++;
	}
	c++;
/*
	The length of the remaining data should be equal to what was expected
	Combined with the initial length check, there must be >= 8 bytes of pad
	ftp://ftp.rsa.com/pub/pdfs/bulletn7.pdf
*/
	if (end - c != outlen) {
		return -1;
	}
/*
	Copy the value bytes to the out buffer
*/
	while (c < end) {
		*out = *c;
		out++; c++;
	}
	return outlen;
}

/******************************************************************************/
/*
	Always called by the server to decrypt the ClientKeyExchange
*/
int32 matrixRsaDecryptPriv(psPool_t *pool, sslRsaKey_t *key, 
						 unsigned char *in, int32 inlen,
						 unsigned char *out, int32 outlen)
{
	int32			ptLen;

	if (inlen != key->size) {
		return -1;
	}
	ptLen = inlen;
	if (ssl_rsa_crypt(pool, in, inlen, in, (uint32*)&ptLen, key,
			RSA_PRIVATE) < 0 || ptLen != inlen) {
		return -1;
	}
	ptLen = sslUnpadRSA(in, inlen, out, outlen, RSA_PRIVATE);
	memset(in, 0x0, inlen);
	return ptLen;
}

/******************************************************************************/
/*
	Called by client as normal part of signature validation from server cert.
	Called by the server if authenticating client in CertificateVerify
*/
int32 matrixRsaDecryptPub(psPool_t *pool, sslRsaKey_t *key, 
						unsigned char *in, int32 inlen,
						unsigned char *out,	int32 outlen)
{
	int32		ptLen;

	if (inlen != key->size) {
		return -1;
	}
	ptLen = inlen;
	if (ssl_rsa_crypt(pool, in, inlen, in, (uint32*)&ptLen, key,
			RSA_PUBLIC) < 0 || ptLen != inlen) {
		return -1;
	}
	if ((ptLen = sslUnpadRSA(in, inlen, out, outlen, RSA_PUBLIC)) < 0) {
		return ptLen;
	}
	return 0;
}


#ifdef USE_RSA_BLINDING

static int32 tim_mp_exptmod(psPool_t *pool, 
						  mp_int *c, mp_int *e, mp_int *d, mp_int *n, mp_int *m)
{
	int32			err;
	mp_int			r, tmp, tmp2;
	unsigned char	*rtmp;
	unsigned long	rlen;
/*
	pick random r
 */
	rlen = mp_unsigned_bin_size(n);

	rtmp = psMalloc(pool, rlen);
	if (rtmp == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	sslGetEntropy(rtmp, rlen);

	if ((err = _mp_init_multi(pool, &r, &tmp, &tmp2, NULL, NULL, NULL, NULL,
			NULL)) != MP_OKAY) {
		psFree(rtmp);
		return -1;
	}
/*
	read in r
 */
	if ((err = mp_read_unsigned_bin(&r, rtmp, rlen)) != MP_OKAY) {
		goto __ERR;
	}
/*
	compute tmp = r^e
 */
	if ((err = mp_exptmod(pool, &r, e, n, &tmp)) != MP_OKAY) {
		goto __ERR;
	}
/*
	multiply C into the mix
 */
	if ((err = mp_mulmod(pool, c, &tmp, n, &tmp)) != MP_OKAY) {
		goto __ERR;
	}
/*
	raise to d
 */
	if ((err = mp_exptmod(pool, &tmp, d, n, &tmp)) != MP_OKAY) {
		goto __ERR;
	}
/*
	invert r and multiply
 */
	if ((err = mp_invmod(pool, &r, n, &tmp2)) != MP_OKAY) {
		goto __ERR;
	}
/*
	multiply and we are totally set
 */
	if ((err = mp_mulmod(pool, &tmp, &tmp2, n, m)) != MP_OKAY) {
		goto __ERR;
	}

__ERR:  _mp_clear_multi(&r, &tmp, &tmp2, NULL, NULL, NULL, NULL, NULL);
	psFree(rtmp);
	return err;
}
#endif /* USE_RSA_BLINDING */

/******************************************************************************/





