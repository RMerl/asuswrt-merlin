/*
 *	matrixCrypto.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Public API set for matrixCrypto
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

#ifndef _h_MATRIX_CRYPTO
#define _h_MATRIX_CRYPTO

#ifdef __cplusplus
extern "C" {
#endif

#include "../../matrixCommon.h"

/*
	User application.  Now include cryptoLayer to get the proper algorithm
	defines so we know what API set to expose
*/
#include "cryptoLayer.h"

/******************************************************************************/


MATRIXPUBLIC int32 matrixGetRandomBytes(unsigned char *bytes, int32 size);

#ifdef USE_ARC4
MATRIXPUBLIC void matrixArc4Init(sslCipherContext_t *ctx, unsigned char *key,
						   int32 keylen);
MATRIXPUBLIC int32 matrixArc4(sslCipherContext_t *ctx, unsigned char *in,
					  unsigned char *out, int32 len);
#endif /* USE_ARC4 */

#ifdef USE_3DES
MATRIXPUBLIC int32 matrix3desInit(sslCipherContext_t *ctx, unsigned char *IV,
						  unsigned char *key, int32 keylen);
MATRIXPUBLIC int32 matrix3desEncrypt(sslCipherContext_t *ctx, unsigned char *pt,
							 unsigned char *ct, int32 len);
MATRIXPUBLIC int32 matrix3desDecrypt(sslCipherContext_t *ctx, unsigned char *ct,
							 unsigned char *pt, int32 len);
MATRIXPUBLIC void generate3DESKey(unsigned char *pass, int32 passlen, 
							unsigned char *salt, unsigned char *key);
#endif /* USE_3DES */



/*
	HMAC and message digests
*/
MATRIXPUBLIC int32 matrixHmacMd5(unsigned char *key, int32 keyLen, 
							const unsigned char *buf, unsigned long len, 
							unsigned char *hash, unsigned char *hmacKey,
							int32 *hmacKeyLen);
MATRIXPUBLIC void matrixHmacMd5Init(sslHmacContext_t *ctx, 
								unsigned char *key, int32 keyLen);
MATRIXPUBLIC void matrixHmacMd5Update(sslHmacContext_t *ctx, 
								const unsigned char *buf, unsigned long len);
MATRIXPUBLIC int32 matrixHmacMd5Final(sslHmacContext_t *ctx,
									  unsigned char *hash);

MATRIXPUBLIC int32 matrixHmacSha1(unsigned char *key, int32 keyLen, 
							const unsigned char *buf, unsigned long len, 
							unsigned char *hash, unsigned char *hmacKey,
							int32 *hmacKeyLen);
MATRIXPUBLIC void matrixHmacSha1Init(sslHmacContext_t *ctx, 
								unsigned char *key, int32 keyLen);
MATRIXPUBLIC void matrixHmacSha1Update(sslHmacContext_t *ctx, 
								const unsigned char *buf, unsigned long len);
MATRIXPUBLIC int32 matrixHmacSha1Final(sslHmacContext_t *ctx,
									   unsigned char *hash);

MATRIXPUBLIC void matrixSha1Init(sslSha1Context_t *ctx);
MATRIXPUBLIC void matrixSha1Update(sslSha1Context_t *ctx,
						const unsigned char *buf, unsigned long len);
MATRIXPUBLIC int32 matrixSha1Final(sslSha1Context_t *ctx, unsigned char *hash);


MATRIXPUBLIC void matrixMd5Init(sslMd5Context_t *ctx);
MATRIXPUBLIC void matrixMd5Update(sslMd5Context_t *ctx,
							const unsigned char *buf, unsigned long len);
MATRIXPUBLIC int32 matrixMd5Final(sslMd5Context_t *ctx, unsigned char *hash);


#ifdef USE_MD2
/*
	MD2 is provided for compatibility with V2 and older X509 certificates,
	it is known to have security problems and should not be used for any current
	development.
*/
MATRIXPUBLIC void matrixMd2Init(sslMd2Context_t *ctx);
MATRIXPUBLIC int32 matrixMd2Update(sslMd2Context_t *ctx,
						const unsigned char *buf, unsigned long len);
MATRIXPUBLIC int32 matrixMd2Final(sslMd2Context_t *ctx, unsigned char *hash);
#endif /* USE_MD2 */




#ifdef USE_RSA
MATRIXPUBLIC int32 matrixRsaEncryptPub(psPool_t *pool, sslRsaKey_t *key, 
							   unsigned char *in, int32 inlen,
							   unsigned char *out, int32 outlen);
MATRIXPUBLIC int32 matrixRsaDecryptPriv(psPool_t *pool, sslRsaKey_t *key, 
								unsigned char *in, int32 inlen,
								unsigned char *out, int32 outlen);
MATRIXPUBLIC int32 matrixRsaDecryptPub(psPool_t *pool, sslRsaKey_t *key, 
							   unsigned char *in, int32 inlen,
							   unsigned char *out, int32 outlen);
#endif /* USE_RSA */




#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIX_CRYPTO */

/******************************************************************************/
