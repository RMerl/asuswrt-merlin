/*
 *	pkiInternal.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Public header file for MatrixSSL PKI extension
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

#ifndef _h_PSPKI_INTERNAL
#define _h_PSPKI_INTERNAL
#define _h_EXPORT_SYMBOLS

#ifdef __cplusplus
extern "C" {
#endif

/*
	Require OS for PeerSec malloc and crypto to determine what we include here
*/
#include "../../matrixCommon.h"
#include "../os/osLayer.h"
#include "../crypto/cryptoLayer.h"

/******************************************************************************/
/*
	ASN defines

	8 bit bit masks for ASN.1 tag field
*/
#define ASN_PRIMITIVE			0x0
#define ASN_CONSTRUCTED			0x20

#define ASN_UNIVERSAL			0x0
#define ASN_APPLICATION			0x40
#define ASN_CONTEXT_SPECIFIC	0x80
#define ASN_PRIVATE				0xC0

/*
	ASN.1 primitive data types
*/
enum {
	ASN_BOOLEAN = 1,
	ASN_INTEGER,
	ASN_BIT_STRING,
	ASN_OCTET_STRING,
	ASN_NULL,
	ASN_OID,
	ASN_UTF8STRING = 12,
	ASN_SEQUENCE = 16,
	ASN_SET,
	ASN_PRINTABLESTRING = 19,
	ASN_T61STRING,
	ASN_IA5STRING = 22,
	ASN_UTCTIME,
	ASN_GENERALIZEDTIME,
	ASN_GENERAL_STRING = 27,
	ASN_BMPSTRING = 30
};


extern int32 getBig(psPool_t *pool, unsigned char **pp, int32 len, mp_int *big);
extern int32 getSerialNum(psPool_t *pool, unsigned char **pp, int32 len, 
						unsigned char **sn, int32 *snLen);
extern int32 getInteger(unsigned char **pp, int32 len, int32 *val);
extern int32 getSequence(unsigned char **pp, int32 len, int32 *outLen);
extern int32 getSet(unsigned char **pp, int32 len, int32 *outLen);
extern int32 getExplicitVersion(unsigned char **pp, int32 len, int32 expVal,
							  int32 *outLen);
extern int32 getAlgorithmIdentifier(unsigned char **pp, int32 len, int32 *oi,
								  int32 isPubKey);
extern int32 getValidity(psPool_t *pool, unsigned char **pp, int32 len, 
					   char **notBefore, char **notAfter);
extern int32 getSignature(psPool_t *pool, unsigned char **pp, int32 len,
						unsigned char **sig, int32 *sigLen);
extern int32 getImplicitBitString(psPool_t *pool, unsigned char **pp, int32 len, 
						int32 impVal, unsigned char **bitString, int32 *bitLen);

/*
	Define to enable more extension parsing
*/
#define USE_FULL_CERT_PARSE

/******************************************************************************/
/*
	The USE_RSA define is primarily for future compat when more key exchange
	protocols are added.  Crypto should always define this for now.
*/
#define OID_RSA_MD2		646
#define OID_RSA_MD5		648
#define OID_RSA_SHA1	649

/*
	DN attributes are used outside the X509 area for cert requests,
	which have been included in the RSA portions of the code
*/
typedef struct {
	char	*country;
	char	*state;
	char	*locality;
	char	*organization;
	char	*orgUnit;
	char	*commonName;
	char	hash[SSL_SHA1_HASH_SIZE];
} DNattributes_t;


#ifdef USE_X509

typedef struct {
	int32	ca;
	int32	pathLenConstraint;
} extBasicConstraints_t;

typedef struct {
	int32			len;
	unsigned char	*id;
} extSubjectKeyId_t;

typedef struct {
	int32			keyLen;
	unsigned char	*keyId;
	DNattributes_t	attribs;
	int32			serialNumLen;
	unsigned char	*serialNum;
} extAuthKeyId_t;
/*
	FUTURE:  add support for the other extensions
*/
typedef struct {
	extBasicConstraints_t	bc;
	sslSubjectAltName_t		*san;
#ifdef USE_FULL_CERT_PARSE
	extSubjectKeyId_t		sk;
	extAuthKeyId_t			ak;
	unsigned char			*keyUsage;
	int32					keyUsageLen;
#endif /* USE_FULL_CERT_PARSE */
} v3extensions_t;

typedef struct sslCert {
	int32			version;
	int32			valid;
	unsigned char	*serialNumber;
	int32			serialNumberLen;
	DNattributes_t	issuer;
	DNattributes_t	subject;
	char			*notBefore;
	char			*notAfter;
	sslRsaKey_t		publicKey;
	int32			certAlgorithm;
	int32			sigAlgorithm;
	int32			pubKeyAlgorithm;
	unsigned char	*signature;
	int32			signatureLen;
	unsigned char	sigHash[SSL_SHA1_HASH_SIZE];
	unsigned char	*uniqueUserId;
	int32			uniqueUserIdLen;
	unsigned char	*uniqueSubjectId;
	int32			uniqueSubjectIdLen;
	v3extensions_t	extensions;
	struct sslCert	*next;
} sslCert_t;

typedef struct sslLocalCert {
	sslRsaKey_t			*privKey;
	unsigned char		*certBin;
	uint32				certLen;
	struct sslLocalCert	*next;
} sslLocalCert_t;

typedef struct {
	sslLocalCert_t	cert;
#ifdef USE_CLIENT_SIDE_SSL
	sslCert_t		*caCerts;
#endif /* USE_CLIENT_SIDE_SSL */
} sslKeys_t;

#endif /* USE_X509 */



/*
	Helpers for inter-pki communications
*/
extern int32 asnParseLength(unsigned char **p, int32 size, int32 *valLen);
extern int32 psAsnConfirmSignature(unsigned char *sigHash,
									unsigned char *sigOut, int32 sigLen);
extern int32 getDNAttributes(psPool_t *pool, unsigned char **pp, int32 len,
						   DNattributes_t *attribs);
extern int32 getPubKey(psPool_t *pool, unsigned char **pp, int32 len,
					   sslRsaKey_t *pubKey);
extern void psFreeDNStruct(DNattributes_t *dn);

#ifdef USE_FILE_SYSTEM
extern int32 readCertChain(psPool_t *pool, const char *certFiles,
						   sslLocalCert_t *lkeys);
extern int32 psGetFileBin(psPool_t *pool, const char *fileName,
							unsigned char **bin, int32 *binLen);
extern int32 base64encodeAndWrite(psPool_t *pool, const char *fileName,
							unsigned char *bin, int32 binLen, int32 fileType,
							char *hexCipherIV, int32 hexCipherIVLen);
#endif /* USE_FILE_SYSTEM */

/*
	Finally, include the public header
*/
#include "matrixPki.h"

#ifdef __cplusplus
}
#endif

#endif /* _h_PSPKI_INTERNAL */

