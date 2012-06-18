/*
 *	matrixPki.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Public header file for MatrixPKI extension
 *	Implementations interacting with the PKI portion of the
 *	matrixssl library should only use the APIs and definitions
 *	used in this file.
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

#ifndef _h_MATRIXPKI
#define _h_MATRIXPKI

#ifdef __cplusplus
extern "C" {
#endif

/*
	Because the set of APIs exposed here is dependent on the package, the
	crypto layer header must be parsed to determine what defines are configured
	(USE_RSA, and USE_X509 specifically)
*/
#include "../../matrixCommon.h"
#include "../crypto/matrixCrypto.h"

#define REQ_FILE_TYPE 0
#define KEY_FILE_TYPE 1
#define CERT_FILE_TYPE 2

#define MAX_CHAIN_LENGTH		16
typedef uint32 sslChainLen_t[MAX_CHAIN_LENGTH];
/******************************************************************************/
/*
 *	PKI documented APIs
 */
MATRIXPUBLIC int32 matrixPkiOpen(void);
MATRIXPUBLIC void matrixPkiClose(void);

#ifdef USE_RSA
/*
	Private key reading and conversions
*/
#ifdef USE_FILE_SYSTEM
MATRIXPUBLIC int32 matrixX509ReadPrivKey(psPool_t *pool, const char *fileName,
								  const char *password, unsigned char **out,
								  int32 *outLen);
#endif /* USE_FILE_SYSTEM */
MATRIXPUBLIC int32 matrixRsaParsePrivKey(psPool_t *pool, unsigned char *keyBuf,
									int32 keyBufLen, sslRsaKey_t **key);
MATRIXPUBLIC int32 matrixRsaParsePubKey(psPool_t *pool, unsigned char *keyBuf,
									int32 keyBufLen, sslRsaKey_t **key);
MATRIXPUBLIC void matrixRsaFreeKey(sslRsaKey_t *key);
MATRIXPUBLIC int32 matrixRsaConvertToPublicKey(psPool_t *pool,
								sslRsaKey_t *privKey, sslRsaKey_t **pubKey);

/*
	USE_X509 adds certificate support
*/
#ifdef USE_X509
MATRIXPUBLIC int32 matrixX509ReadKeysMem(sslKeys_t **keys,
						unsigned char *certBuf, int32 certLen,
						unsigned char *privBuf, int32 privLen,
						unsigned char *trustedCABuf, int32 trustedCALen);
MATRIXPUBLIC void	matrixRsaFreeKeys(sslKeys_t *keys);

#ifdef USE_FILE_SYSTEM
MATRIXPUBLIC int32	matrixX509ReadKeys(sslKeys_t **keys, const char *certFile,
						const char *privFile, const char *privPass,
						const char *trustedCAFile);
MATRIXPUBLIC int32 matrixX509ReadKeysEx(psPool_t *pool, sslKeys_t **keys,
				const char *certFile, const char *privFile,
				const char *privPass, const char *trustedCAFiles);
MATRIXPUBLIC int32 matrixX509ReadCert(psPool_t *pool, const char *fileName,
							   unsigned char **out, int32 *outLen,
							   sslChainLen_t *chain);
MATRIXPUBLIC int32 matrixX509ReadPubKey(psPool_t *pool, const char *certFile,
								 sslRsaKey_t **key);
#endif /* USE_FILE_SYSTEM */

MATRIXPUBLIC int32 matrixRsaParseKeysMem(psPool_t *pool, sslKeys_t **keys,
			unsigned char *certBuf, int32 certLen, unsigned char *privBuf,
			int32 privLen, unsigned char *trustedCABuf, int32 trustedCALen);
MATRIXPUBLIC int32 matrixX509ParseCert(psPool_t *pool, unsigned char *certBuf, 
							   int32 certlen, sslCert_t **cert);
MATRIXPUBLIC void matrixX509FreeCert(sslCert_t *cert);
MATRIXPUBLIC int32 matrixX509ParsePubKey(psPool_t *pool, unsigned char *certBuf,
									int32 certLen, sslRsaKey_t **key);
MATRIXPUBLIC int32 matrixX509ValidateCert(psPool_t *pool,
						sslCert_t *subjectCert, sslCert_t *issuerCert,
						int32 *valid);
MATRIXPUBLIC int32 matrixX509ValidateCertChain(psPool_t *pool,
						sslCert_t *chain, sslCert_t **subjectCert,
						int32 *valid);
MATRIXPUBLIC int32 matrixX509UserValidator(psPool_t *pool, 
						sslCert_t *subjectCert,
						int32 (*certValidator)(sslCertInfo_t *t, void *arg),
						void *arg);
#endif /* USE_X509 */



#endif /* USE_RSA */
	
	
/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIXPKI */

/******************************************************************************/

