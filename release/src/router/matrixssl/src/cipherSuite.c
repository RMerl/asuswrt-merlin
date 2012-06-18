/*
 *	cipherSuite.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Wrappers for the various cipher suites.
 *	Contributors should add additional cipher suites here.
 *	Enable specific suites at compile time in matrixConfig.h
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

#include "matrixInternal.h"

/******************************************************************************/
/*
	Using void *ssl rather than ssl_t *ssl is a workaround for a chicken-egg
	issue in matrixInternal.h where these function prototypes are used
	before ssl_t is defined.
*/
#ifdef USE_SHA1_MAC
static int32 sha1GenerateMac(void *ssl, unsigned char type, unsigned char *data, 
							int32 len, unsigned char *mac);
static int32 sha1VerifyMac(void *ssl, unsigned char type, unsigned char *data, 
							int32 len, unsigned char *mac);
#endif

#ifdef USE_MD5_MAC
static int32 md5GenerateMac(void *ssl, unsigned char type, unsigned char *data, 
							int32 len, unsigned char *mac);
static int32 md5VerifyMac(void *ssl, unsigned char type, unsigned char *data, 
							int32 len, unsigned char *mac);
#endif

#ifdef USE_SSL_RSA_WITH_RC4_128_MD5
static int32 matrixCipher4Init(sslSec_t *sec, int32 type);
#endif /* USE_SSL_RSA_WITH_RC4_128_MD5 */

#ifdef USE_SSL_RSA_WITH_RC4_128_SHA
static int32 matrixCipher5Init(sslSec_t *sec, int32 type);
#endif /* USE_SSL_RSA_WITH_RC4_128_SHA */

#ifdef USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA
static int32 matrixCipherAInit(sslSec_t *sec, int32 type);
#endif /* USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA */




static int32 nullInit(sslSec_t *sec, int32 type);
static int32 nullEncrypt(sslCipherContext_t *ctx, unsigned char *in,
					   unsigned char *out, int32 len);
static int32 nullDecrypt(sslCipherContext_t *ctx, unsigned char *in,
					   unsigned char *out, int32 len);
static int32 nullEncryptPub(psPool_t *pool, sslRsaKey_t *key, 
						  unsigned char *in, int32 inlen,
						  unsigned char *out, int32 outlen);
static int32 nullDecryptPriv(psPool_t *pool, sslRsaKey_t *key, 
						   unsigned char *in, int32 inlen,
						   unsigned char *out, int32 outlen);
static int32 nullGenerateMac(void *ssl, unsigned char type, unsigned char *data, 
						   int32 len, unsigned char *mac);
static int32 nullVerifyMac(void *ssl, unsigned char type, unsigned char *data, 
						 int32 len, unsigned char *mac);

/******************************************************************************/

static sslCipherSpec_t	supportedCiphers[] = {
/*
	New ciphers should be added here, similar to the ones below
	These ciphers should be in order of the most desireable to the 
	least desireable ciphers to negotiate.
*/
#ifdef USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA
	{SSL_RSA_WITH_3DES_EDE_CBC_SHA,
		20,			/* macSize */
		24,			/* keySize */
		8,			/* ivSize */
		8,			/* blocksize */
		matrixCipherAInit,
		matrix3desEncrypt, 
		matrix3desDecrypt, 
		NULL,
		matrixRsaDecryptPub,
		matrixRsaEncryptPub, 
		matrixRsaDecryptPriv, 
		sha1GenerateMac, 
		sha1VerifyMac},
#endif /* USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA */
#ifdef USE_SSL_RSA_WITH_RC4_128_SHA
	{SSL_RSA_WITH_RC4_128_SHA,
		20,			/* macSize */
		16,			/* keySize */
		0,			/* ivSize */
		1,			/* blocksize */
		matrixCipher5Init,
		matrixArc4, 
		matrixArc4, 
		NULL,
		matrixRsaDecryptPub,
		matrixRsaEncryptPub, 
		matrixRsaDecryptPriv, 
		sha1GenerateMac, 
		sha1VerifyMac},
#endif /* USE_SSL_RSA_WITH_RC4_128_SHA */
#ifdef USE_SSL_RSA_WITH_RC4_128_MD5
	{SSL_RSA_WITH_RC4_128_MD5,
		16,			/* macSize */
		16,			/* keySize */
		0,			/* ivSize */
		1,			/* blocksize */
		matrixCipher4Init,
		matrixArc4, 
		matrixArc4, 
		NULL,
		matrixRsaDecryptPub,
		matrixRsaEncryptPub, 
		matrixRsaDecryptPriv, 
		md5GenerateMac, 
		md5VerifyMac},
#endif /* USE_SSL_RSA_WITH_RC4_128_MD5 */
/*
	These two USE_SSL_RSA_WITH_NULL ciphers are not recommended for use
	in production applications.
*/
#ifdef USE_SSL_RSA_WITH_NULL_MD5
	{SSL_RSA_WITH_NULL_MD5,
		16,			/* macSize */
		0,			/* keySize */
		0,			/* ivSize */
		0,			/* blocksize */
		nullInit,
		nullEncrypt, 
		nullDecrypt, 
		matrixRsaEncryptPriv,
		matrixRsaDecryptPub,
		matrixRsaEncryptPub, 
		matrixRsaDecryptPriv, 
		md5GenerateMac, 
		md5VerifyMac},
#endif /* USE_SSL_RSA_WITH_NULL_MD5 */
#ifdef USE_SSL_RSA_WITH_NULL_SHA
	{SSL_RSA_WITH_NULL_SHA,
		20,			/* macSize */
		0,			/* keySize */
		0,			/* ivSize */
		0,			/* blocksize */
		nullInit,
		nullEncrypt, 
		nullDecrypt, 
		matrixRsaEncryptPriv,
		matrixRsaDecryptPub,
		matrixRsaEncryptPub, 
		matrixRsaDecryptPriv, 
		sha1GenerateMac, 
		sha1VerifyMac},
#endif /* USE_SSL_RSA_WITH_NULL_SHA */
/*
	The NULL Cipher suite must exist and be the last in this list
*/
	{SSL_NULL_WITH_NULL_NULL,
		0,
		0,
		0,
		0,
		nullInit,
		nullEncrypt, 
		nullDecrypt, 
		nullEncryptPub,
		nullDecryptPriv,
		nullEncryptPub, 
		nullDecryptPriv, 
		nullGenerateMac, 
		nullVerifyMac}
};

/******************************************************************************/
/*
	Lookup the given cipher spec ID and return a pointer to the structure
	if found.  This is used when negotiating security, to find out what suites
	we support.
*/
sslCipherSpec_t *sslGetCipherSpec(int32 id)
{
	int32	i;

	i = 0;
	do {
		if ((int32)supportedCiphers[i].id == id) {
			return &supportedCiphers[i];
		}
	} while (supportedCiphers[i++].id != SSL_NULL_WITH_NULL_NULL) ;

	return NULL;
}

/******************************************************************************/
/*
	Write out a list of the supported cipher suites to the caller's buffer
	First 2 bytes are the number of cipher suite bytes, the remaining bytes are
	the cipher suites, as two byte, network byte order values.
*/
int32 sslGetCipherSpecList(unsigned char *c, int32 len)
{
	unsigned char	*end, *p;
	unsigned short	i;

	if (len < 4) {
		return -1;
	}
	end = c + len;
	p = c; c += 2;
	for (i = 0; supportedCiphers[i].id != SSL_NULL_WITH_NULL_NULL; i++) {
		if (end - c < 2) {
			return -1;
		}
		*c = (unsigned char)((supportedCiphers[i].id & 0xFF00) >> 8); c++;
		*c = (unsigned char)(supportedCiphers[i].id & 0xFF); c++;
	}
	i *= 2;
	*p = (unsigned char)(i >> 8); p++;
	*p = (unsigned char)(i & 0xFF);
	return i + 2;
}

/******************************************************************************/
/*
	Return the length of the cipher spec list, including initial length bytes
*/
int32 sslGetCipherSpecListLen(void)
{
	int32	i;

	for (i = 0; supportedCiphers[i].id != SSL_NULL_WITH_NULL_NULL; i++) {
	}
	return (i * 2) + 2;
}


/******************************************************************************/
/*
*/
#ifdef USE_SHA1_MAC
static int32 sha1GenerateMac(void *sslv, unsigned char type, unsigned char *data, 
							int32 len, unsigned char *mac)
{
	ssl_t	*ssl = (ssl_t*)sslv;
		return ssl3HMACSha1(ssl->sec.writeMAC, ssl->sec.seq, type, data, len, mac);
}

static int32 sha1VerifyMac(void *sslv, unsigned char type, unsigned char *data,
					int32 len, unsigned char *mac)
{
	ssl_t			*ssl = (ssl_t*)sslv;
	unsigned char	buf[SSL_SHA1_HASH_SIZE];

		ssl3HMACSha1(ssl->sec.readMAC, ssl->sec.remSeq, type, data, len, buf);
	if (memcmp(buf, mac, SSL_SHA1_HASH_SIZE) == 0) {
		return 0;
	}
	return -1;
}
#endif /* USE_SHA1_MAC */

/******************************************************************************/
/*
*/
#ifdef USE_MD5_MAC
static int32 md5GenerateMac(void *sslv, unsigned char type, unsigned char *data,
						  int32 len, unsigned char *mac)
{
	ssl_t	*ssl = (ssl_t*)sslv;
		return ssl3HMACMd5(ssl->sec.writeMAC, ssl->sec.seq, type, data, len, mac);
}

static int32 md5VerifyMac(void *sslv, unsigned char type, unsigned char *data,
						int32 len, unsigned char *mac)
{
	ssl_t			*ssl = (ssl_t*)sslv;
	unsigned char	buf[SSL_MD5_HASH_SIZE];

		ssl3HMACMd5(ssl->sec.readMAC, ssl->sec.remSeq, type, data, len, buf);
	if (memcmp(buf, mac, SSL_MD5_HASH_SIZE) == 0) {
		return 0;
	}
	return -1;
}

#endif /* USE_MD5_MAC */

/******************************************************************************/
/*
	SSL_RSA_WITH_RC4_128_SHA cipher init
*/
#ifdef USE_SSL_RSA_WITH_RC4_128_MD5
static int32 matrixCipher4Init(sslSec_t *sec, int32 type)
{
	if (type == INIT_ENCRYPT_CIPHER) {
		matrixArc4Init(&(sec->encryptCtx), sec->writeKey, 16);
	} else {
		matrixArc4Init(&(sec->decryptCtx), sec->readKey, 16);
	}

	return 0;
}
#endif /* USE_SSL_RSA_WITH_RC4_128_MD5 */

/******************************************************************************/
/*
	SSL_RSA_WITH_RC4_128_SHA cipher init
*/
#ifdef USE_SSL_RSA_WITH_RC4_128_SHA
static int32 matrixCipher5Init(sslSec_t *sec, int32 type)
{
	if (type == INIT_ENCRYPT_CIPHER) {
		matrixArc4Init(&(sec->encryptCtx), sec->writeKey, 16);
	} else {
		matrixArc4Init(&(sec->decryptCtx), sec->readKey, 16);
	}

	return 0;
}
#endif /* USE_SSL_RSA_WITH_RC4_128_SHA */

/******************************************************************************/
/*
	SSL_RSA_WITH_3DES_EDE_CBC_SHA cipher init
*/
#ifdef USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA
static int32 matrixCipherAInit(sslSec_t *sec, int32 type)
{
	if (type == INIT_ENCRYPT_CIPHER) {
		if (matrix3desInit(&(sec->encryptCtx), sec->writeIV, sec->writeKey, 
			SSL_DES3_KEY_LEN) < 0) {
			return -1;
		}
	} else {
		if (matrix3desInit(&(sec->decryptCtx), sec->readIV, sec->readKey, 
			SSL_DES3_KEY_LEN) < 0) {
			return -1;
		}
	}
	return 0;
}
#endif /* USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA */


/******************************************************************************/
/*
	SSL_NULL_WITH_NULL_NULL cipher functions
	Used in handshaking before SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC message
	FUTURE - remove the memcpy to support in-situ decryption
*/
static int32 nullInit(sslSec_t *sec, int32 type)
{
	return 0;
}

static int32 nullEncrypt(sslCipherContext_t *ctx, unsigned char *in,
					   unsigned char *out, int32 len)
{
	if (out != in) {
		memcpy(out, in, len);
	}
	return len;
}

static int32 nullDecrypt(sslCipherContext_t *ctx, unsigned char *in,
					   unsigned char *out, int32 len)
{
	if (out != in) {
		memcpy(out, in, len);
	}
	return len;
}

/*
	FUTURE - remove both apis below
*/
static int32 nullEncryptPub(psPool_t *pool, sslRsaKey_t *key, 
						  unsigned char *in, int32 inlen,
						  unsigned char *out, int32 outlen)
{
	if (inlen <= outlen) {
		matrixStrDebugMsg("Error: output buffer too small for NULL encrypt\n",
			NULL);
		return -1;
	}
	memcpy(out, in, inlen);
	return inlen;
}

static int32 nullDecryptPriv(psPool_t *pool, sslRsaKey_t *key,
							unsigned char *in, int32 inlen,
							unsigned char *out, int32 outlen)
{
	if (inlen <= outlen) {
		matrixStrDebugMsg("Error: output buffer too small for NULL decrypt\n",
			NULL);
		return -1;
	}
	memcpy(out, in, inlen);
	return inlen;
}

static int32 nullGenerateMac(void *ssl, unsigned char type, unsigned char *data, 
						   int32 len, unsigned char *mac)
{
	return 0;
}

static int32 nullVerifyMac(void *ssl, unsigned char type, unsigned char *data, 
						 int32 len, unsigned char *mac)
{
	return 0;
}



/******************************************************************************/
