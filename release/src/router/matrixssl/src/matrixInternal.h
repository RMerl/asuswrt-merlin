/*
 *	matrixInternal.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Internal header file used for the MatrixSSL implementation.
 *	Only modifiers of the library should be intersted in this file
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

#ifndef _h_MATRIXINTERNAL
#define _h_MATRIXINTERNAL
#define _h_EXPORT_SYMBOLS

#include "../matrixCommon.h"

/******************************************************************************/
/*
	At the highest SSL level.  Must include the lower level PKI
*/
#include "pki/pkiInternal.h"

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

#if WIN32
#define				fcntl(A, B, C)
#define				MSG_NOSIGNAL	0
#endif /* WIN32 */

#define	SSL_FLAGS_READ_SECURE	0x2
#define	SSL_FLAGS_WRITE_SECURE	0x4
#define SSL_FLAGS_PUBLIC_SECURE	0x8
#define SSL_FLAGS_RESUMED		0x10	
#define SSL_FLAGS_CLOSED		0x20
#define SSL_FLAGS_NEED_ENCODE	0x40
#define SSL_FLAGS_ERROR			0x80
	
#define SSL2_HEADER_LEN				2
#define SSL3_HEADER_LEN				5
#define SSL3_HANDSHAKE_HEADER_LEN	4

/*
	These are defines rather than enums because we want to store them as char,
	not int32 (enum size)
*/
#define SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC	20
#define SSL_RECORD_TYPE_ALERT				21
#define SSL_RECORD_TYPE_HANDSHAKE			22
#define SSL_RECORD_TYPE_APPLICATION_DATA	23

#define SSL_HS_HELLO_REQUEST		0
#define SSL_HS_CLIENT_HELLO			1
#define SSL_HS_SERVER_HELLO			2
#define SSL_HS_HELLO_VERIFY_REQUEST	3
#define SSL_HS_CERTIFICATE			11
#define SSL_HS_SERVER_KEY_EXCHANGE	12
#define SSL_HS_CERTIFICATE_REQUEST	13
#define SSL_HS_SERVER_HELLO_DONE	14
#define SSL_HS_CERTIFICATE_VERIFY	15
#define SSL_HS_CLIENT_KEY_EXCHANGE	16
#define SSL_HS_FINISHED				20
#define SSL_HS_DONE					255	/* Handshake complete (internal) */

#define	INIT_ENCRYPT_CIPHER		0
#define INIT_DECRYPT_CIPHER		1

#define RSA_SIGN	1

/*
	Additional ssl alert value, indicating no error has ocurred.
*/
#define SSL_ALERT_NONE					255	/* No error */

#define SSL_HS_RANDOM_SIZE			32
#define SSL_HS_RSA_PREMASTER_SIZE	48

#define SSL2_MAJ_VER	2
#define SSL3_MAJ_VER	3
#define SSL3_MIN_VER	0
#define TLS_MIN_VER		1



/*
	SSL cipher suite values
*/
#define SSL_NULL_WITH_NULL_NULL				0x0000
#define SSL_RSA_WITH_NULL_MD5				0x0001
#define SSL_RSA_WITH_NULL_SHA				0x0002
#define SSL_RSA_WITH_RC4_128_MD5			0x0004
#define SSL_RSA_WITH_RC4_128_SHA			0x0005
#define SSL_RSA_WITH_3DES_EDE_CBC_SHA		0x000A

/*
	Maximum key block size for any defined cipher
	This must be validated if new ciphers are added
	Value is largest total among all cipher suites for
		2*macSize + 2*keySize + 2*ivSize
*/
#define SSL_MAX_KEY_BLOCK_SIZE			2*20 + 2*24 + 2*8

/*
	Master secret is 48 bytes, sessionId is 32 bytes max
*/
#define		SSL_HS_MASTER_SIZE		48
#define		SSL_MAX_SESSION_ID_SIZE	32


/*
	Round up the given length to the correct length with SSLv3 padding
*/
#define sslRoundup018(LEN, BLOCKSIZE) \
	BLOCKSIZE <= 1 ? BLOCKSIZE : (((LEN) + 8) & ~7)

/******************************************************************************/
/*
	SSL record and session structures
*/
typedef struct {
	unsigned short	len;
	unsigned char	majVer;
	unsigned char	minVer;
	unsigned char	type;
	unsigned char	pad[3];		/* Padding for 64 bit compat */
} sslRec_t;

typedef struct {
	unsigned char	clientRandom[SSL_HS_RANDOM_SIZE];	/* From ClientHello */
	unsigned char	serverRandom[SSL_HS_RANDOM_SIZE];	/* From ServerHello */
	unsigned char	masterSecret[SSL_HS_MASTER_SIZE];
	unsigned char	*premaster;							/* variable size */
	int32			premasterSize;

	unsigned char	keyBlock[SSL_MAX_KEY_BLOCK_SIZE];	/* Storage for the next six items */
	unsigned char	*wMACptr;
	unsigned char	*rMACptr;
	unsigned char	*wKeyptr;
	unsigned char	*rKeyptr;
	unsigned char	*wIVptr;
	unsigned char	*rIVptr;

	/*	All maximum sizes for current cipher suites */
	unsigned char	writeMAC[SSL_MAX_MAC_SIZE];
	unsigned char	readMAC[SSL_MAX_MAC_SIZE];
	unsigned char	writeKey[SSL_MAX_SYM_KEY_SIZE];
	unsigned char	readKey[SSL_MAX_SYM_KEY_SIZE];
	unsigned char	writeIV[SSL_MAX_IV_SIZE];
	unsigned char	readIV[SSL_MAX_IV_SIZE];

	unsigned char	seq[8];
	unsigned char	remSeq[8];

#ifdef USE_CLIENT_SIDE_SSL
	sslCert_t		*cert;
	int32 (*validateCert)(sslCertInfo_t *certInfo, void *arg);
	void			*validateCertArg;
	int32				certMatch;
#endif /* USE_CLIENT_SIDE_SSL */

	sslMd5Context_t		msgHashMd5;
	sslSha1Context_t	msgHashSha1;

	sslCipherContext_t	encryptCtx;
	sslCipherContext_t	decryptCtx;
	int32				anon;
} sslSec_t;

typedef struct {
	uint32	id;
	unsigned char	macSize;
	unsigned char	keySize;
	unsigned char	ivSize;
	unsigned char	blockSize;
	/* Init function */
	int32 (*init)(sslSec_t *sec, int32 type);
	/* Cipher functions */
	int32 (*encrypt)(sslCipherContext_t *ctx, unsigned char *in,
		unsigned char *out, int32 len);
	int32 (*decrypt)(sslCipherContext_t *ctx, unsigned char *in,
		unsigned char *out, int32 len);
	int32 (*encryptPriv)(psPool_t *pool, sslRsaKey_t *key,	
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*decryptPub)(psPool_t *pool, sslRsaKey_t *key,	
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);	
	int32 (*encryptPub)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*decryptPriv)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*generateMac)(void *ssl, unsigned char type, unsigned char *data,
		int32 len, unsigned char *mac);
	int32 (*verifyMac)(void *ssl, unsigned char type, unsigned char *data,
		int32 len, unsigned char *mac);
} sslCipherSpec_t;

typedef struct ssl {
	sslRec_t		rec;			/* Current SSL record information*/
									
	sslSec_t		sec;			/* Security structure */

	sslKeys_t		*keys;			/* SSL public and private keys */

	psPool_t		*pool;			/* SSL session pool */
	psPool_t		*hsPool;		/* Full session handshake pool */

	unsigned char	sessionIdLen;
	char			sessionId[SSL_MAX_SESSION_ID_SIZE];

	/* Pointer to the negotiated cipher information */
	sslCipherSpec_t	*cipher;

	/* 	Symmetric cipher callbacks

		We duplicate these here from 'cipher' because we need to set the
		various callbacks at different times in the handshake protocol
		Also, there are 64 bit alignment issues in using the function pointers
		within 'cipher' directly
	*/
	int32 (*encrypt)(sslCipherContext_t *ctx, unsigned char *in,
		unsigned char *out, int32 len);
	int32 (*decrypt)(sslCipherContext_t *ctx, unsigned char *in,
		unsigned char *out, int32 len);
	/* Public key ciphers */
	int32 (*encryptPriv)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*decryptPub)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*encryptPub)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	int32 (*decryptPriv)(psPool_t *pool, sslRsaKey_t *key, 
		unsigned char *in, int32 inlen,
		unsigned char *out, int32 outlen);
	/* Message Authentication Codes */
	int32 (*generateMac)(void *ssl, unsigned char type, unsigned char *data,
		int32 len, unsigned char *mac);
	int32 (*verifyMac)(void *ssl, unsigned char type, unsigned char *data,
		int32 len, unsigned char *mac);

	/* Current encryption/decryption parameters */
	unsigned char	enMacSize;
	unsigned char	enIvSize;
	unsigned char	enBlockSize;
	unsigned char	deMacSize;
	unsigned char	deIvSize;
	unsigned char	deBlockSize;

	int32			flags;
	int32			hsState;		/* Next expected handshake message type */
	int32			err;			/* SSL errno of last api call */
	int32			ignoredMessageCount;

	unsigned char	reqMajVer;
	unsigned char	reqMinVer;
	unsigned char	majVer;
	unsigned char	minVer;
	int32			recordHeadLen;
	int32			hshakeHeadLen;
} ssl_t;

typedef struct {
	unsigned char	id[SSL_MAX_SESSION_ID_SIZE];
	unsigned char	masterSecret[SSL_HS_MASTER_SIZE];
	uint32	cipherId;
} sslSessionId_t;

typedef struct {
	unsigned char	id[SSL_MAX_SESSION_ID_SIZE];
	unsigned char	masterSecret[SSL_HS_MASTER_SIZE];
	sslCipherSpec_t	*cipher;
	unsigned char	majVer;
	unsigned char	minVer;
	char			flag;
	sslTime_t		startTime;
	sslTime_t		accessTime;
	int32			inUse;
} sslSessionEntry_t;

/******************************************************************************/
/*
	Have ssl_t and sslSessionId_t now for addition of the public api set
*/
#include "../matrixSsl.h"

/******************************************************************************/
/*
	sslEncode.c and sslDecode.c
*/
extern int32 psWriteRecordInfo(ssl_t *ssl, unsigned char type, int32 len,
							 unsigned char *c);
extern int32 psWriteHandshakeHeader(ssl_t *ssl, unsigned char type, int32 len, 
								int32 seq, int32 fragOffset, int32 fragLen,
								unsigned char *c);
extern int32 sslEncodeResponse(ssl_t *ssl, sslBuf_t *out);
extern int32 sslActivateReadCipher(ssl_t *ssl);
extern int32 sslActivateWriteCipher(ssl_t *ssl);
extern int32 sslActivatePublicCipher(ssl_t *ssl);
extern int32 sslUpdateHSHash(ssl_t *ssl, unsigned char *in, int32 len);
extern int32 sslInitHSHash(ssl_t *ssl);
extern int32 sslSnapshotHSHash(ssl_t *ssl, unsigned char *out, int32 senderFlag);
extern int32 sslWritePad(unsigned char *p, unsigned char padLen);
extern void sslResetContext(ssl_t *ssl);

#ifdef USE_SERVER_SIDE_SSL
extern int32 matrixRegisterSession(ssl_t *ssl);
extern int32 matrixResumeSession(ssl_t *ssl);
extern int32 matrixClearSession(ssl_t *ssl, int32 remove);
extern int32 matrixUpdateSession(ssl_t *ssl);
#endif /* USE_SERVER_SIDE_SSL */


/*
	cipherSuite.c
*/
extern sslCipherSpec_t *sslGetCipherSpec(int32 id);
extern int32 sslGetCipherSpecListLen(void);
extern int32 sslGetCipherSpecList(unsigned char *c, int32 len);

/******************************************************************************/
/*
	sslv3.c
*/
extern int32 sslGenerateFinishedHash(sslMd5Context_t *md5, sslSha1Context_t *sha1,
								unsigned char *masterSecret,
								unsigned char *out, int32 sender);

extern int32 sslDeriveKeys(ssl_t *ssl);

#ifdef USE_SHA1_MAC
extern int32 ssl3HMACSha1(unsigned char *key, unsigned char *seq, 
						unsigned char type, unsigned char *data, int32 len,
						unsigned char *mac);
#endif /* USE_SHA1_MAC */

#ifdef USE_MD5_MAC
extern int32 ssl3HMACMd5(unsigned char *key, unsigned char *seq, 
						unsigned char type, unsigned char *data, int32 len,
						unsigned char *mac);
#endif /* USE_MD5_MAC */




/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIXINTERNAL */

/******************************************************************************/


