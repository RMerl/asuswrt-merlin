/*
 *	x509.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	DER/BER coding
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

#include "pkiInternal.h"

/*
	X509 is wrapped in USE_RSA until more key types are added
*/
#ifdef USE_RSA
#ifdef USE_X509

#define IMPLICIT_ISSUER_ID		1
#define IMPLICIT_SUBJECT_ID		2
#define EXPLICIT_EXTENSION		3

#define	RSA_SIG					1
#define DSA_SIG					2

#define OID_SHA1				88
#define OID_MD2					646
#define OID_MD5					649

/*
	Certificate extension hash mappings
*/
#define EXT_BASIC_CONSTRAINTS	1
#define	EXT_KEY_USAGE			2
#define EXT_SUBJ_KEY_ID			3
#define EXT_AUTH_KEY_ID			4
#define EXT_ALT_SUBJECT_NAME	5

static const struct {
		unsigned char	hash[16];
		int32			id;
} extTable[] = {
	{ { 0xa5, 0xc4, 0x5e, 0x9a, 0xa3, 0xbb, 0x71, 0x2f, 0x07,
		0xf7, 0x4c, 0xd0, 0xcd, 0x95, 0x65, 0xda }, EXT_BASIC_CONSTRAINTS },
	{ { 0xf5, 0xab, 0x88, 0x49, 0xc4, 0xfd, 0xa2, 0x64, 0x6d,
		0x06, 0xa2, 0x3e, 0x83, 0x9b, 0xef, 0xbb }, EXT_KEY_USAGE },
	{ { 0x91, 0x54, 0x28, 0xcc, 0x81, 0x59, 0x8c, 0x71, 0x8c,
		0x53, 0xa8, 0x4d, 0xeb, 0xd3, 0xc2, 0x18 }, EXT_SUBJ_KEY_ID },
	{ { 0x48, 0x2d, 0xff, 0x49, 0xf7, 0xab, 0x93, 0xe8, 0x1f,
		0x57, 0xb5, 0xaf, 0x7f, 0xaa, 0x31, 0xbb }, EXT_AUTH_KEY_ID },
	{ { 0x5c, 0x70, 0xcb, 0xf5, 0xa4, 0x07, 0x5a, 0xcc, 0xd1,
		0x55, 0xd2, 0x44, 0xdd, 0x62, 0x2c, 0x0c }, EXT_ALT_SUBJECT_NAME },
	{ { 0 }, -1 } /* Must be last for proper termination */
};


static int32 getExplicitExtensions(psPool_t *pool, unsigned char **pp, 
								 int32 len, int32 expVal,
								 v3extensions_t *extensions);
static int32 matrixX509ValidateCertInternal(psPool_t *pool,
				sslCert_t *subjectCert, sslCert_t *issuerCert, int32 chain);
#ifdef USE_FILE_SYSTEM
static int32 parseList(psPool_t *pool, const char *list, const char *sep,
					   char **item);
#endif /* USE_FILE_SYSTEM */

/******************************************************************************/
/*
	Read in the certificate and private keys from the given files
	If privPass is non-NULL, it will be used to decode an encrypted private
	key file.

	The certificate is stored internally as a pointer to DER encoded X.509
	The private key is stored in a crypto provider specific structure
*/
#ifdef USE_FILE_SYSTEM
int32 matrixX509ReadKeys(sslKeys_t **keys, const char *certFile,
						const char *privFile, const char *privPass,
						const char *trustedCAFiles)
{
	return matrixX509ReadKeysEx(PEERSEC_BASE_POOL, keys, certFile, privFile,
		privPass, trustedCAFiles);
}
#else /* USE_FILE_SYSTEM */
int32 matrixX509ReadKeys(sslKeys_t **keys, char *certFile, char *privFile,
					 char *privPass, char *trustedCAFile)
{
	matrixStrDebugMsg("Error: Calling matrixX509ReadKeys against a library " \
					  "built without USE_FILE_SYSTEM defined\n", NULL);
	return -1;
}
#endif /* USE_FILE_SYSTEM */

/******************************************************************************/
/*
	In memory version of matrixX509ReadKeys.  The buffers are the ASN.1 raw
	stream (ie. not base64 PEM encoded)

	API CHANGE: 1.7 changed this protoype and buffer formats (ASN.1 now) but
	this function was never properly	documented.  Users who may have found
	this function on their own and are using it will need to convert to this
	new	version.
*/
int32 matrixX509ReadKeysMem(sslKeys_t **keys, unsigned char *certBuf,
			int32 certLen, unsigned char *privBuf, int32 privLen,
			unsigned char *trustedCABuf, int32 trustedCALen)
{
	return matrixRsaParseKeysMem(PEERSEC_BASE_POOL, keys, certBuf, certLen,
		privBuf, privLen, trustedCABuf, trustedCALen);
}

/******************************************************************************/
/*
	Free private key and cert and zero memory allocated by matrixSslReadKeys.
*/
void matrixRsaFreeKeys(sslKeys_t *keys)
{
	sslLocalCert_t	*current, *next;
	int32			i = 0;

	if (keys) {
		current = &keys->cert;
		while (current) {
			if (current->certBin) {
				memset(current->certBin, 0x0, current->certLen);
				psFree(current->certBin);
			}
			if (current->privKey) {
				matrixRsaFreeKey(current->privKey);
			}
			next = current->next;
			if (i++ > 0) {
				psFree(current);
			}
			current = next;
		}
#ifdef USE_CLIENT_SIDE_SSL
		if (keys->caCerts) {
			matrixX509FreeCert(keys->caCerts);
		}
#endif /* USE_CLIENT_SIDE_SSL */
		psFree(keys);
	}
}

#ifdef USE_FILE_SYSTEM
/******************************************************************************/
/*
	Preferred version for commercial users who make use of memory pools.

	This use of the sslKeys_t param implies this is for use in the MatrixSSL
	product (input to matrixSslNewSession).  However, we didn't want to
	expose this API at the matrixSsl.h level due to the pool parameter. This
	is strictly an API that commerical users will have access to
*/
int32 matrixX509ReadKeysEx(psPool_t *pool, sslKeys_t **keys,
				const char *certFile, const char *privFile,
				const char *privPass, const char *trustedCAFiles)
{
	sslKeys_t		*lkeys;
	unsigned char	*privKeyMem;
	int32			rc, privKeyMemLen;
#ifdef USE_CLIENT_SIDE_SSL
	sslCert_t		*currCert, *prevCert = NULL;
	unsigned char	*caCert, *caStream;
	sslChainLen_t	chain;
	int32			caCertLen, first, i;
#endif /* USE_CLIENT_SIDE_SSL */

	*keys = lkeys = psMalloc(pool, sizeof(sslKeys_t));
	if (lkeys == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(lkeys, 0x0, sizeof(sslKeys_t));
/*
	Load certificate files.  Any additional certificate files should chain
	to the root CA held on the other side.
*/
	rc = readCertChain(pool, certFile, &lkeys->cert);
	if (rc < 0 ) {
		matrixRsaFreeKeys(lkeys);
		return rc;
	}
/*
	The first cert in certFile must be associated with the provided
	private key. 
*/
	if (privFile) {
		rc = matrixX509ReadPrivKey(pool, privFile, privPass, &privKeyMem,
			&privKeyMemLen);
		if (rc < 0) {
			matrixStrDebugMsg("Error reading private key file: %s\n",
				(char*)privFile);
			matrixRsaFreeKeys(lkeys);
			return rc;
		}
			rc = matrixRsaParsePrivKey(pool, privKeyMem, privKeyMemLen,
				&lkeys->cert.privKey);
		if (rc < 0) {
			matrixStrDebugMsg("Error parsing private key file: %s\n",
				(char*)privFile);
			psFree(privKeyMem);
			matrixRsaFreeKeys(lkeys);
			return rc;
		}
		psFree(privKeyMem);
	}
	
#ifdef USE_CLIENT_SIDE_SSL
/*
	Now deal with Certificate Authorities
*/
	if (trustedCAFiles != NULL) {
		if (matrixX509ReadCert(pool, trustedCAFiles, &caCert, &caCertLen,
				&chain) < 0 || caCert == NULL) {
			matrixStrDebugMsg("Error reading CA cert files %s\n",
				(char*)trustedCAFiles);
			matrixRsaFreeKeys(lkeys);
			return -1;
		}

		caStream = caCert;
		i = first = 0;
		while (chain[i] != 0) {
/*
			Don't allow one bad cert to ruin the whole bunch if possible
*/
			if (matrixX509ParseCert(pool, caStream, chain[i], &currCert) < 0) {
				matrixX509FreeCert(currCert);
				matrixStrDebugMsg("Error parsing CA cert %s\n",
					(char*)trustedCAFiles);
				caStream += chain[i]; caCertLen -= chain[i];
				i++;
				continue;
			}
		
			if (first == 0) {
				lkeys->caCerts = currCert;
			} else {
				prevCert->next = currCert;
			}
			first++;
			prevCert = currCert;
			currCert = NULL;
			caStream += chain[i]; caCertLen -= chain[i];
			i++;
		}
		sslAssert(caCertLen == 0);
		psFree(caCert);
	}
/*
	Check to see that if a set of CAs were passed in at least
	one ended up being valid.
*/
	if (trustedCAFiles != NULL && lkeys->caCerts == NULL) {
		matrixStrDebugMsg("No valid CA certs in %s\n",
			(char*)trustedCAFiles);
		matrixRsaFreeKeys(lkeys);
		return -1;
	}
#endif /* USE_CLIENT_SIDE_SSL */
	return 0; 
}

/******************************************************************************/
/*
 *	Public API to return a binary buffer from a cert.  Suitable to send
 *	over the wire.  Caller must free 'out' if this function returns success (0)
 *	Parse .pem files according to http://www.faqs.org/rfcs/rfc1421.html
 */
int32 matrixX509ReadCert(psPool_t *pool, const char *fileName,
					unsigned char **out, int32 *outLen, sslChainLen_t *chain)
{
	int32			certBufLen, rc, certChainLen, i;
	unsigned char	*oneCert[MAX_CHAIN_LENGTH];
	unsigned char	*certPtr, *tmp;
	char			*certFile, *start, *end, *certBuf, *endTmp;
	const char		sep[] = ";";

/*
	Init chain array and output params
*/
	for (i=0; i < MAX_CHAIN_LENGTH; i++) {
		oneCert[i] = NULL;
		(*chain)[i] = 0;
	}
	*outLen = certChainLen = i = 0;
	rc = -1;

/*
	For PKI product purposes, this routine now accepts a chain of certs.
*/
	if (fileName != NULL) {
		fileName += parseList(pool, fileName, sep, &certFile);
	} else {
		return 0;
	}

	while (certFile != NULL) {
	
		if (i == MAX_CHAIN_LENGTH) {
			matrixIntDebugMsg("Exceeded maximum cert chain length of %d\n",
				MAX_CHAIN_LENGTH);
			psFree(certFile);
			rc = -1;
			goto err;
		}
		if ((rc = psGetFileBin(pool, certFile, (unsigned char**)&certBuf,
				&certBufLen)) < 0) {
			matrixStrDebugMsg("Couldn't open file %s\n", certFile);
			goto err;
		}
		psFree(certFile);
		certPtr = (unsigned char*)certBuf;
		start = end = endTmp = certBuf;

		while (certBufLen > 0) {
			if (((start = strstr(certBuf, "-----BEGIN")) != NULL) &&
					((start = strstr(certBuf, "CERTIFICATE-----")) != NULL) &&
					((end = strstr(start, "-----END")) != NULL) &&
					((endTmp = strstr(end,"CERTIFICATE-----")) != NULL)) {
				start += strlen("CERTIFICATE-----");
				(*chain)[i] = (int32)(end - start);
				end = endTmp + strlen("CERTIFICATE-----");
                while (*end == '\r' || *end == '\n' || *end == '\t'
						|| *end == ' ') {
					end++;
				}
			} else {
				psFree(certPtr);
				rc = -1;
				goto err;
			}
			oneCert[i] = psMalloc(pool, (*chain)[i]);
			certBufLen -= (int32)(end - certBuf);
			certBuf = end;
			memset(oneCert[i], '\0', (*chain)[i]);

			if (ps_base64_decode((unsigned char*)start, (*chain)[i], oneCert[i],
					&(*chain)[i]) != 0) {
				psFree(certPtr);
				matrixStrDebugMsg("Unable to base64 decode certificate\n", NULL);
				rc = -1;
				goto err;
			}
			certChainLen += (*chain)[i];
			i++;
			if (i == MAX_CHAIN_LENGTH) { 
				matrixIntDebugMsg("Exceeded maximum cert chain length of %d\n", 
					MAX_CHAIN_LENGTH); 
				psFree(certPtr); 
				rc = -1; 
				goto err; 
			} 
		}
		psFree(certPtr);
/*
		Check for more files
*/
		fileName += parseList(pool, fileName, sep, &certFile);
	}

	*outLen = certChainLen;
/*
	Don't bother stringing them together if only one was passed in
*/
	if (i == 1) {
		sslAssert(certChainLen == (*chain)[0]);
		*out = oneCert[0];
		return 0;
	} else {
		*out = tmp = psMalloc(pool, certChainLen);
		for (i=0; i < MAX_CHAIN_LENGTH; i++) {
			if (oneCert[i]) {
				memcpy(tmp, oneCert[i], (*chain)[i]);
				tmp += (*chain)[i];
			}
		}
		rc = 0;
	}

err:
	for (i=0; i < MAX_CHAIN_LENGTH; i++) {
		if (oneCert[i]) psFree(oneCert[i]);
	}
	return rc;
}

/******************************************************************************/
/*
	This function was written strictly for clarity in the PeerSec crypto API
	product.  It extracts only the public key from a certificate file for use
	in the lower level encrypt/decrypt RSA routines
*/
int32 matrixX509ReadPubKey(psPool_t *pool, const char *certFile,
						   sslRsaKey_t **key)
{
	unsigned char	*certBuf;
	sslChainLen_t	chain;
	int32			certBufLen;

	certBuf = NULL;
	if (matrixX509ReadCert(pool, certFile, &certBuf, &certBufLen, &chain) < 0) {
		matrixStrDebugMsg("Unable to read certificate file %s\n",
			(char*)certFile);
		if (certBuf) psFree(certBuf);
		return -1;
	}
	if (matrixX509ParsePubKey(pool, certBuf, certBufLen, key) < 0) {
		psFree(certBuf);
		return -1;
	}
	psFree(certBuf);
	return 0;
}

/******************************************************************************/
/*
	Allows for semi-colon delimited list of certificates for cert chaining.
	Also allows multiple certificiates in a single file.
	
	HOWERVER, in both cases the first in the list must be the identifying
	cert of the application. Each subsequent cert is the parent of the previous
*/
int32 readCertChain(psPool_t *pool, const char *certFiles,
					sslLocalCert_t *lkeys)
{
	sslLocalCert_t	*currCert;
	unsigned char	*certBin, *tmp;
	sslChainLen_t	chain;
	int32			certLen, i;

	if (certFiles == NULL) {
		return 0;
	}

	if (matrixX509ReadCert(pool, certFiles, &certBin, &certLen, &chain) < 0) {
		matrixStrDebugMsg("Error reading cert file %s\n", (char*)certFiles);
		return -1;
	}
/*
	The first cert is allocated in the keys struct.  All others in
	linked list are allocated here.
*/
	i = 0;
	tmp = certBin;
	while (chain[i] != 0) {
		if (i == 0) {
			currCert = lkeys;
		} else {
			currCert->next = psMalloc(pool, sizeof(sslLocalCert_t));
			if (currCert->next == NULL) {
				psFree(tmp);
				return -8; /* SSL_MEM_ERROR */
			}
			memset(currCert->next, 0x0, sizeof(sslLocalCert_t));
			currCert = currCert->next;
		}
		currCert->certBin = psMalloc(pool, chain[i]);
		memcpy(currCert->certBin, certBin, chain[i]);
		currCert->certLen = chain[i];
		certBin += chain[i]; certLen -= chain[i];
		i++;
	}
	psFree(tmp);
	sslAssert(certLen == 0);
	return 0;
}

/******************************************************************************/
/*
 *	Strtok substitute
 */
static int32 parseList(psPool_t *pool, const char *list, const char *sep,
					   char **item)
{
	int32	start, listLen;
	char	*tmp;

	start = listLen = (int32)strlen(list) + 1;
	if (start == 1) {
		*item = NULL;
		return 0;
	}
	tmp = *item = psMalloc(pool, listLen);
	if (tmp == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(*item, 0, listLen);
	while (listLen > 0) {
		if (*list == sep[0]) {
			list++;
			listLen--;
			break;
		}
		if (*list == 0) {
			break;
		}
		*tmp++ = *list++;
		listLen--;
	}
	return start - listLen;
}
#endif /* USE_FILE_SYSTEM */


/******************************************************************************/
/*
	Preferred version for commercial users who make use of memory pools.

	This use of the sslKeys_t param implies this is for use in the MatrixSSL
	product (input to matrixSslNewSession).  However, we didn't want to
	expose this API at the matrixSsl.h level due to the pool parameter. This
	is strictly an API that commerical users will have access to.
*/
int32 matrixRsaParseKeysMem(psPool_t *pool, sslKeys_t **keys,
			unsigned char *certBuf,	int32 certLen, unsigned char *privBuf,
			int32 privLen, unsigned char *trustedCABuf, int32 trustedCALen)
{
	sslKeys_t		*lkeys;
	sslLocalCert_t	*current, *next;
	unsigned char	*binPtr;
	int32			len, lenOh, i;
#ifdef USE_CLIENT_SIDE_SSL
	sslCert_t		*currentCA, *nextCA;
#endif /* USE_CLIENT_SIDE_SSL */

	*keys = lkeys = psMalloc(pool, sizeof(sslKeys_t));
	if (lkeys == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(lkeys, 0x0, sizeof(sslKeys_t));
/*
	The buffers are just the ASN.1 streams so the intermediate parse
	that used to be here is gone.  Doing a straight memcpy for this
	and passing that along to X509ParseCert
*/
	i = 0;
	current = &lkeys->cert;
	binPtr = certBuf;
/*
	Need to check for a chain here.  Only way to do this is to read off the
	length id from the DER stream for each.  The chain must be just a stream
	of DER certs with the child-most cert always first.
*/
	while (certLen > 0) {
		if (getSequence(&certBuf, certLen, &len) < 0) {
			matrixStrDebugMsg("Unable to parse length of cert stream\n", NULL);
			matrixRsaFreeKeys(lkeys);
			return -1;
		}
/*
		Account for the overhead of storing the length itself
*/
		lenOh = (int32)(certBuf - binPtr);
		len += lenOh;
		certBuf -= lenOh;
/*
		First cert is already malloced
*/
		if (i > 0) {
			next = psMalloc(pool, sizeof(sslLocalCert_t));
			memset(next, 0x0, sizeof(sslLocalCert_t));
			current->next = next;
			current = next;
		}
		current->certBin = psMalloc(pool, len);
		memcpy(current->certBin, certBuf, len);
		current->certLen = len;
		certLen -= len;
		certBuf += len;
		binPtr = certBuf;
		i++;
	}

/*
	Parse private key
*/
	if (privLen > 0) {
		if (matrixRsaParsePrivKey(pool, privBuf, privLen,
				&lkeys->cert.privKey) < 0) {
			matrixStrDebugMsg("Error reading private key mem\n", NULL);
			matrixRsaFreeKeys(lkeys);
			return -1;
		}
	}


/*
	Trusted CAs
*/
#ifdef USE_CLIENT_SIDE_SSL
	if (trustedCABuf != NULL && trustedCALen > 0) {
		i = 0;
		binPtr = trustedCABuf;
		currentCA = NULL;
/*
		Need to check for list here.  Only way to do this is to read off the
		length id from the DER stream for each.
*/
		while (trustedCALen > 0) {
			if (getSequence(&trustedCABuf, trustedCALen, &len) < 0) {
				matrixStrDebugMsg("Unable to parse length of CA stream\n",
					NULL);
				matrixRsaFreeKeys(lkeys);
				return -1;
			}
/*
			Account for the overhead of storing the length itself
*/
			lenOh = (int32)(trustedCABuf - binPtr);
			len += lenOh;
			trustedCABuf -= lenOh;

			if (matrixX509ParseCert(pool, trustedCABuf, len, &currentCA) < 0) {
				matrixX509FreeCert(currentCA);
				matrixStrDebugMsg("Error parsing CA cert\n", NULL);
				matrixRsaFreeKeys(lkeys);
				return -1;
			}
/*
			First cert should be assigned to lkeys
*/
			if (i == 0) {
				lkeys->caCerts = currentCA;
				nextCA = lkeys->caCerts;
			} else {
				nextCA->next = currentCA;
				nextCA = currentCA;
			}
			currentCA = currentCA->next;
			trustedCALen -= len;
			trustedCABuf += len;
			binPtr = trustedCABuf;
			i++;
		}
	}
#endif /* USE_CLIENT_SIDE_SSL */

	return 0;
}

/******************************************************************************/
/*
	In-memory version of matrixX509ReadPubKey.
	This function was written strictly for clarity in the PeerSec crypto API
	subset.  It extracts only the public key from a certificate file for use
	in the lower level encrypt/decrypt RSA routines.
*/
int32 matrixX509ParsePubKey(psPool_t *pool, unsigned char *certBuf,
							int32 certLen, sslRsaKey_t **key)
{
	sslRsaKey_t		*lkey;
	sslCert_t		*certStruct;
	int32			err;

	if (matrixX509ParseCert(pool, certBuf, certLen, &certStruct) < 0) {
		matrixX509FreeCert(certStruct);
		return -1;
	}
	lkey = *key = psMalloc(pool, sizeof(sslRsaKey_t));
	memset(lkey, 0x0, sizeof(sslRsaKey_t));

	if ((err = _mp_init_multi(pool, &lkey->e, &lkey->N, NULL,
			NULL, NULL,	NULL, NULL,	NULL)) != MP_OKAY) {
		matrixX509FreeCert(certStruct);
		psFree(lkey);
		return err;
	}
	mp_copy(&certStruct->publicKey.e, &lkey->e);
	mp_copy(&certStruct->publicKey.N, &lkey->N);

	mp_shrink(&lkey->e);
	mp_shrink(&lkey->N);

	lkey->size = certStruct->publicKey.size;

	matrixX509FreeCert(certStruct);

	return 0;
}


/******************************************************************************/
/*
	Parse an X509 ASN.1 certificate stream
	http://www.faqs.org/rfcs/rfc2459.html section 4.1
*/
int32 matrixX509ParseCert(psPool_t *pool, unsigned char *pp, int32 size, 
						sslCert_t **outcert)
{
	sslCert_t			*cert;
	sslMd5Context_t		md5Ctx;
	sslSha1Context_t	sha1Ctx;
	unsigned char		*p, *end, *certStart, *certEnd;
	int32				certLen, len, parsing;
#ifdef USE_MD2
	sslMd2Context_t		md2Ctx;
#endif /* USE_MD2 */

/*
	Allocate the cert structure right away.  User MUST always call
	matrixX509FreeCert regardless of whether this function succeeds.
	memset is important because the test for NULL is what is used
	to determine what to free
*/
	*outcert = cert = psMalloc(pool, sizeof(sslCert_t));
	if (cert == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(cert, '\0', sizeof(sslCert_t));

	p = pp;
	end = p + size;
/*
		Certificate  ::=  SEQUENCE  {
		tbsCertificate		TBSCertificate,
		signatureAlgorithm	AlgorithmIdentifier,
		signatureValue		BIT STRING	}
*/
	parsing = 1;
	while (parsing) {
		if (getSequence(&p, (int32)(end - p), &len) < 0) {
			matrixStrDebugMsg("Initial cert parse error\n", NULL);
			return -1;
		}
		certStart = p;
/*	
		TBSCertificate  ::=  SEQUENCE  {
		version			[0]		EXPLICIT Version DEFAULT v1,
		serialNumber			CertificateSerialNumber,
		signature				AlgorithmIdentifier,
		issuer					Name,
		validity				Validity,
		subject					Name,
		subjectPublicKeyInfo	SubjectPublicKeyInfo,
		issuerUniqueID	[1]		IMPLICIT UniqueIdentifier OPTIONAL,
							-- If present, version shall be v2 or v3
		subjectUniqueID	[2]	IMPLICIT UniqueIdentifier OPTIONAL,
							-- If present, version shall be v2 or v3
		extensions		[3]	EXPLICIT Extensions OPTIONAL
							-- If present, version shall be v3	}
*/
		if (getSequence(&p, (int32)(end - p), &len) < 0) {
			matrixStrDebugMsg("ASN sequence parse error\n", NULL);
			return -1;
		}
		certEnd = p + len;
		certLen = (int32)(certEnd - certStart);

/*
		Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
*/
		if (getExplicitVersion(&p, (int32)(end - p), 0, &cert->version) < 0) {
			matrixStrDebugMsg("ASN version parse error\n", NULL);
			return -1;
		}
		if (cert->version != 2) {		
			matrixIntDebugMsg("Warning: non-v3 certificate version: %d\n",
			cert->version);
		}
/*
		CertificateSerialNumber  ::=  INTEGER
*/
		if (getSerialNum(pool, &p, (int32)(end - p), &cert->serialNumber,
				&cert->serialNumberLen) < 0) {
			matrixStrDebugMsg("ASN serial number parse error\n", NULL);
			return -1;
		}
/*
		AlgorithmIdentifier  ::=  SEQUENCE  {
		algorithm				OBJECT IDENTIFIER,
		parameters				ANY DEFINED BY algorithm OPTIONAL }
*/
		if (getAlgorithmIdentifier(&p, (int32)(end - p),
				&cert->certAlgorithm, 0) < 0) {
			return -1;
		}
/*
		Name ::= CHOICE {
		RDNSequence }

		RDNSequence ::= SEQUENCE OF RelativeDistinguishedName

		RelativeDistinguishedName ::= SET OF AttributeTypeAndValue

		AttributeTypeAndValue ::= SEQUENCE {
		type	AttributeType,
		value	AttributeValue }

		AttributeType ::= OBJECT IDENTIFIER

		AttributeValue ::= ANY DEFINED BY AttributeType
*/
		if (getDNAttributes(pool, &p, (int32)(end - p), &cert->issuer) < 0) {
			return -1;
		}
/*
		Validity ::= SEQUENCE {
		notBefore	Time,
		notAfter	Time	}
*/
		if (getValidity(pool, &p, (int32)(end - p), &cert->notBefore,
				&cert->notAfter) < 0) {
			return -1;
		}
/*
		Subject DN
*/
		if (getDNAttributes(pool, &p, (int32)(end - p), &cert->subject) < 0) {
			return -1;
		}
/*
		SubjectPublicKeyInfo  ::=  SEQUENCE  {
		algorithm			AlgorithmIdentifier,
		subjectPublicKey	BIT STRING	}
*/
		if (getSequence(&p, (int32)(end - p), &len) < 0) {
			return -1;
		}
		if (getAlgorithmIdentifier(&p, (int32)(end - p),
				&cert->pubKeyAlgorithm, 1) < 0) {
			return -1;
		}

			if (getPubKey(pool, &p, (int32)(end - p), &cert->publicKey) < 0) {
				return -1;
			}

/*
		As the next three values are optional, we can do a specific test here
*/
		if (*p != (ASN_SEQUENCE | ASN_CONSTRUCTED)) {
			if (getImplicitBitString(pool, &p, (int32)(end - p), IMPLICIT_ISSUER_ID,
					&cert->uniqueUserId, &cert->uniqueUserIdLen) < 0 ||
				getImplicitBitString(pool, &p, (int32)(end - p), IMPLICIT_SUBJECT_ID,
					&cert->uniqueSubjectId, &cert->uniqueSubjectIdLen) < 0 ||
				getExplicitExtensions(pool, &p, (int32)(end - p), EXPLICIT_EXTENSION,
					&cert->extensions) < 0) {
				matrixStrDebugMsg("There was an error parsing a certificate\n", NULL);
				matrixStrDebugMsg("extension.  This is likely caused by an\n", NULL);
				matrixStrDebugMsg("extension format that is not currently\n", NULL);
				matrixStrDebugMsg("recognized.  Please email support@peersec.com\n", NULL);
				matrixStrDebugMsg("to add support for the extension.\n\n", NULL);
				return -1;
			}
		}
/*
		This is the end of the cert.  Do a check here to be certain
*/
		if (certEnd != p) {
			return -1;
		}
/*
		Certificate signature info
*/
		if (getAlgorithmIdentifier(&p, (int32)(end - p),
				&cert->sigAlgorithm, 0) < 0) {
			return -1;
		}
/*
		Signature algorithm must match that specified in TBS cert
*/
		if (cert->certAlgorithm != cert->sigAlgorithm) {
			matrixStrDebugMsg("Parse error: mismatched signature type\n", NULL);
			return -1; 
		}
/*
		Compute the hash of the cert here for CA validation
*/
		if (cert->certAlgorithm == OID_RSA_MD5) {
			matrixMd5Init(&md5Ctx);
			matrixMd5Update(&md5Ctx, certStart, certLen);
			matrixMd5Final(&md5Ctx, cert->sigHash);
		} else if (cert->certAlgorithm == OID_RSA_SHA1) {
			matrixSha1Init(&sha1Ctx);
			matrixSha1Update(&sha1Ctx, certStart, certLen);
			matrixSha1Final(&sha1Ctx, cert->sigHash);
		}
#ifdef USE_MD2
		else if (cert->certAlgorithm == OID_RSA_MD2) {
			matrixMd2Init(&md2Ctx);
			matrixMd2Update(&md2Ctx, certStart, certLen);
			matrixMd2Final(&md2Ctx, cert->sigHash);
		}
#endif /* USE_MD2 */

		if (getSignature(pool, &p, (int32)(end - p), &cert->signature,
				&cert->signatureLen) < 0) {
			return -1;
		}
/*
		The ability to parse additional chained certs is a PKI product
		feature addition.  Chaining in MatrixSSL is handled internally.
*/
		if (p != end) {
			cert->next = psMalloc(pool, sizeof(sslCert_t));
			cert = cert->next;
			memset(cert, '\0', sizeof(sslCert_t));
		} else {
			parsing = 0;
		}
	}
		
	return (int32)(p - pp);
}

/******************************************************************************/
/*
	User must call after all calls to matrixX509ParseCert
	(we violate the coding standard a bit here for clarity)
*/
void matrixX509FreeCert(sslCert_t *cert)
{
	sslCert_t			*curr, *next;
	sslSubjectAltName_t	*active, *inc;

	curr = cert;
	while (curr) {
		psFreeDNStruct(&curr->issuer);
		psFreeDNStruct(&curr->subject);
		if (curr->serialNumber)			psFree(curr->serialNumber);
		if (curr->notBefore)			psFree(curr->notBefore);
		if (curr->notAfter)				psFree(curr->notAfter);
		if (curr->publicKey.N.dp)		mp_clear(&(curr->publicKey.N));
		if (curr->publicKey.e.dp)		mp_clear(&(curr->publicKey.e));
		if (curr->signature)			psFree(curr->signature);
		if (curr->uniqueUserId)			psFree(curr->uniqueUserId);
		if (curr->uniqueSubjectId)		psFree(curr->uniqueSubjectId);

		if (curr->extensions.san) {
			active = curr->extensions.san;
			while (active != NULL) {
				inc = active->next;
				psFree(active->data);
				psFree(active);
				active = inc;
			}
		}


#ifdef USE_FULL_CERT_PARSE
		if (curr->extensions.keyUsage)	psFree(curr->extensions.keyUsage);
		if (curr->extensions.sk.id)		psFree(curr->extensions.sk.id);
		if (curr->extensions.ak.keyId)	psFree(curr->extensions.ak.keyId);
		if (curr->extensions.ak.serialNum)
						psFree(curr->extensions.ak.serialNum);
		if (curr->extensions.ak.attribs.commonName)
						psFree(curr->extensions.ak.attribs.commonName);
		if (curr->extensions.ak.attribs.country)
						psFree(curr->extensions.ak.attribs.country);
		if (curr->extensions.ak.attribs.state)
						psFree(curr->extensions.ak.attribs.state);
		if (curr->extensions.ak.attribs.locality)
						psFree(curr->extensions.ak.attribs.locality);
		if (curr->extensions.ak.attribs.organization)
						psFree(curr->extensions.ak.attribs.organization);
		if (curr->extensions.ak.attribs.orgUnit)
						psFree(curr->extensions.ak.attribs.orgUnit);
#endif /* SSL_FULL_CERT_PARSE */
		next = curr->next;
		psFree(curr);
		curr = next;
	}
}	

/******************************************************************************/
/*
	Do the signature validation for a subject certificate against a
	known CA certificate
*/
int32 psAsnConfirmSignature(unsigned char *sigHash, unsigned char *sigOut,
							int32 sigLen)
{
	unsigned char	*end, *p = sigOut;
	unsigned char	hash[SSL_SHA1_HASH_SIZE];
	int32			len, oi;

	end = p + sigLen;
/*
	DigestInfo ::= SEQUENCE {
		digestAlgorithm DigestAlgorithmIdentifier,
		digest Digest }

	DigestAlgorithmIdentifier ::= AlgorithmIdentifier

	Digest ::= OCTET STRING
*/
	if (getSequence(&p, (int32)(end - p), &len) < 0) {
		return -1;
	}

/*
	Could be MD5 or SHA1
 */
	if (getAlgorithmIdentifier(&p, (int32)(end - p), &oi, 0) < 0) {
		return -1;
	}
	if ((*p++ != ASN_OCTET_STRING) ||
			asnParseLength(&p, (int32)(end - p), &len) < 0 || (end - p) <  len) {
		return -1;
	}
	memcpy(hash, p, len);
	if (oi == OID_MD5 || oi == OID_MD2) {
		if (len != SSL_MD5_HASH_SIZE) {
			return -1;
		}
	} else if (oi == OID_SHA1) {
		if (len != SSL_SHA1_HASH_SIZE) {
			return -1;
		}
	} else {
		return -1;
	}
/*
	hash should match sigHash
*/
	if (memcmp(hash, sigHash, len) != 0) {
		return -1;
	}
	return 0;
}

/******************************************************************************/
/*
	Extension lookup
*/	
static int32 lookupExt(unsigned char md5hash[SSL_MD5_HASH_SIZE])
{
	int32				i, j;
	const unsigned char	*tmp;

	for (i = 0; ;i++) {
		if (extTable[i].id == -1) {
			return -1;
		}
		tmp = extTable[i].hash;
		for (j = 0; j < SSL_MD5_HASH_SIZE; j++) {
			if (md5hash[j] != tmp[j]) {
				break;
			}
			if (j == SSL_MD5_HASH_SIZE - 1) {
				return extTable[i].id;
			}
		}
	}
}

/******************************************************************************/
/*
	X509v3 extensions
*/
static int32 getExplicitExtensions(psPool_t *pool, unsigned char **pp, 
								 int32 inlen, int32 expVal,
								 v3extensions_t *extensions)
{
	unsigned char		*p = *pp, *end;
	unsigned char		*extEnd, *extStart;
	int32				len, noid, critical, fullExtLen;
	unsigned char		oid[SSL_MD5_HASH_SIZE];
	sslMd5Context_t		md5ctx;
	sslSubjectAltName_t	*activeName, *prevName;

	end = p + inlen;
	if (inlen < 1) {
		return -1;
	}
/*
	Not treating this as an error because it is optional.
*/
	if (*p != (ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | expVal)) {
		return 0;
	}
	p++;
	if (asnParseLength(&p, (int32)(end - p), &len) < 0 || (end - p) < len) {
		return -1;
	}
/*
	Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension

	Extension  ::=  SEQUENCE {
		extnID		OBJECT IDENTIFIER,
		extnValue	OCTET STRING	}
*/
	if (getSequence(&p, (int32)(end - p), &len) < 0) {
		return -1;
	}
	extEnd = p + len;
	while ((p != extEnd) && *p == (ASN_SEQUENCE | ASN_CONSTRUCTED)) {
		if (getSequence(&p, (int32)(extEnd - p), &fullExtLen) < 0) {
			return -1;
		}
		extStart = p;
/*
		Conforming CAs MUST support key identifiers, basic constraints,
		key usage, and certificate policies extensions
	
		id-ce-authorityKeyIdentifier	OBJECT IDENTIFIER ::=	{ id-ce 35 }
		id-ce-basicConstraints			OBJECT IDENTIFIER ::=	{ id-ce 19 } 133
		id-ce-keyUsage					OBJECT IDENTIFIER ::=	{ id-ce 15 }
		id-ce-certificatePolicies		OBJECT IDENTIFIER ::=	{ id-ce 32 }
		id-ce-subjectAltName			OBJECT IDENTIFIER ::=	{ id-ce 17 }  131
*/
		if (extEnd - p < 1 || *p++ != ASN_OID) {
			return -1;
		}
		
		if (asnParseLength(&p, (int32)(extEnd - p), &len) < 0 || 
				(extEnd - p) < len) {
			return -1;
		}
/*
		Send the OID through a digest to get the unique id
*/
		matrixMd5Init(&md5ctx);
		while (len-- > 0) {
			matrixMd5Update(&md5ctx, p, sizeof(char));
			p++;
		}
		matrixMd5Final(&md5ctx, oid);
		noid = lookupExt(oid);

/*
		Possible boolean value here for 'critical' id.  It's a failure if a
		critical extension is found that is not supported
*/
		critical = 0;
		if (*p == ASN_BOOLEAN) {
			p++;
			if (*p++ != 1) {
				matrixStrDebugMsg("Error parsing cert extension\n", NULL);
				return -1;
			}
			if (*p++ > 0) {
				critical = 1;
			}
		}
		if (extEnd - p < 1 || (*p++ != ASN_OCTET_STRING) ||
				asnParseLength(&p, (int32)(extEnd - p), &len) < 0 || 
				extEnd - p < len) {
			matrixStrDebugMsg("Expecting OCTET STRING in ext parse\n", NULL);
			return -1;
		}

		switch (noid) {
/*
			 BasicConstraints ::= SEQUENCE {
				cA						BOOLEAN DEFAULT FALSE,
				pathLenConstraint		INTEGER (0..MAX) OPTIONAL }
*/
			case EXT_BASIC_CONSTRAINTS:
				if (getSequence(&p, (int32)(extEnd - p), &len) < 0) {
					return -1;
				}
/*
				"This goes against PKIX guidelines but some CAs do it and some
				software requires this to avoid interpreting an end user
				certificate as a CA."
					- OpenSSL certificate configuration doc

				basicConstraints=CA:FALSE
*/
				if (len == 0) {
					break;
				}
/*
				Have seen some certs that don't include a cA bool.
*/
				if (*p == ASN_BOOLEAN) {
					p++;
					if (*p++ != 1) {
						return -1;
					}
					extensions->bc.ca = *p++;
				} else {
					extensions->bc.ca = 0;
				}
/*
				Now need to check if there is a path constraint. Only makes
				sense if cA is true.  If it's missing, there is no limit to
				the cert path
*/
				if (*p == ASN_INTEGER) {
					if (getInteger(&p, (int32)(extEnd - p),
							&(extensions->bc.pathLenConstraint)) < 0) {
						return -1;
					}
				} else {
					extensions->bc.pathLenConstraint = -1;
				}
				break;
			case EXT_ALT_SUBJECT_NAME:
				if (getSequence(&p, (int32)(extEnd - p), &len) < 0) {
					return -1;
				}
/*
				Looking only for DNS, URI, and email here to support
				FQDN for Web clients

				FUTURE:  Support all subject alt name members
				GeneralName ::= CHOICE {
					otherName						[0]		OtherName,
					rfc822Name						[1]		IA5String,
					dNSName							[2]		IA5String,
					x400Address						[3]		ORAddress,
					directoryName					[4]		Name,
					ediPartyName					[5]		EDIPartyName,
					uniformResourceIdentifier		[6]		IA5String,
					iPAddress						[7]		OCTET STRING,
					registeredID					[8]		OBJECT IDENTIFIER }
*/
				while (len > 0) {
					if (extensions->san == NULL) {
						activeName = extensions->san = psMalloc(pool,
							sizeof(sslSubjectAltName_t));
					} else {
/*
						Find the end
*/
						prevName = extensions->san;
						activeName = prevName->next;
						while (activeName != NULL) {
							prevName = activeName;
							activeName = prevName->next;
						}
						prevName->next = psMalloc(pool,
							sizeof(sslSubjectAltName_t));
						activeName = prevName->next;
					}
					activeName->next = NULL;
					activeName->data = NULL;
					memset(activeName->name, '\0', 16);

					activeName->id = *p & 0xF;
					switch (activeName->id) {
						case 0:
							memcpy(activeName->name, "other", 5);
							break;
						case 1:
							memcpy(activeName->name, "email", 5);
							break;
						case 2:
							memcpy(activeName->name, "DNS", 3);
							break;
						case 3:
							memcpy(activeName->name, "x400Address", 11);
							break;
						case 4:
							memcpy(activeName->name, "directoryName", 13);
							break;
						case 5:
							memcpy(activeName->name, "ediPartyName", 12);
							break;
						case 6:
							memcpy(activeName->name, "URI", 3);
							break;
						case 7:
							memcpy(activeName->name, "iPAddress", 9);
							break;
						case 8:
							memcpy(activeName->name, "registeredID", 12);
							break;
						default:
							memcpy(activeName->name, "unknown", 7);
							break;
					}
	
					p++;
					activeName->dataLen = *p++;
					if (extEnd - p < activeName->dataLen) {
						return -1;
					}
					activeName->data = psMalloc(pool, activeName->dataLen + 1);
					if (activeName->data == NULL) {
						return -8; /* SSL_MEM_ERROR */
					}
					memset(activeName->data, 0x0, activeName->dataLen + 1);
					memcpy(activeName->data, p, activeName->dataLen);
					
					p = p + activeName->dataLen;
					/* the magic 2 is the type and length */
					len -= activeName->dataLen + 2; 
				}
				break;
#ifdef USE_FULL_CERT_PARSE
			case EXT_AUTH_KEY_ID:
/*
				AuthorityKeyIdentifier ::= SEQUENCE {
				keyIdentifier				[0] KeyIdentifier			OPTIONAL,
				authorityCertIssuer			[1] GeneralNames			OPTIONAL,
				authorityCertSerialNumber	[2] CertificateSerialNumber	OPTIONAL }

				KeyIdentifier ::= OCTET STRING
*/
				if (getSequence(&p, (int32)(extEnd - p), &len) < 0) {
					return -1;
				}
/*
				Have seen a cert that has a zero length ext here.  Let it pass.
*/
				if (len == 0) {
					break;
				}
/*
				All memebers are optional
*/
				if (*p == (ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 0)) {
					p++;
					if (asnParseLength(&p, (int32)(extEnd - p), 
							&extensions->ak.keyLen) < 0 ||
							extEnd - p < extensions->ak.keyLen) {
						return -1;
					}
					extensions->ak.keyId = psMalloc(pool, extensions->ak.keyLen);
					if (extensions->ak.keyId == NULL) {
						return -8; /* SSL_MEM_ERROR */
					}
					memcpy(extensions->ak.keyId, p, extensions->ak.keyLen);
					p = p + extensions->ak.keyLen;
				}
				if (*p == (ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | 1)) {
					p++;
					if (asnParseLength(&p, (int32)(extEnd - p), &len) < 0 ||
							len < 1 || extEnd - p < len) {
						return -1;
					}
					if ((*p ^ ASN_CONTEXT_SPECIFIC ^ ASN_CONSTRUCTED) != 4) {
/*
						FUTURE: support other name types
						We are just dealing with DN formats here
*/
						matrixIntDebugMsg("Error auth key-id name type: %d\n",
							*p ^ ASN_CONTEXT_SPECIFIC ^ ASN_CONSTRUCTED);
						return -1;
					}
					p++;
					if (asnParseLength(&p, (int32)(extEnd - p), &len) < 0 || 
							extEnd - p < len) {
						return -1;
					}
					if (getDNAttributes(pool, &p, (int32)(extEnd - p),
							&(extensions->ak.attribs)) < 0) {
						return -1;
					}
				}
				if ((*p == (ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2)) ||
						(*p == ASN_INTEGER)){
/*
					Treat as a serial number (not a native INTEGER)
*/
					if (getSerialNum(pool, &p, (int32)(extEnd - p),
							&(extensions->ak.serialNum), &len) < 0) {
						return -1;
					}
					extensions->ak.serialNumLen = len;
				}
				break;

			case EXT_KEY_USAGE:
/*
				KeyUsage ::= BIT STRING {
					digitalSignature		(0),
					nonRepudiation			(1),
					keyEncipherment			(2),
					dataEncipherment		(3),
					keyAgreement			(4),
					keyCertSign				(5),

					cRLSign					(6),
					encipherOnly			(7),
					decipherOnly			(8) }
*/
				if (*p++ != ASN_BIT_STRING) {
					return -1;
				}
				if (asnParseLength(&p, (int32)(extEnd - p), &len) < 0 || 
						extEnd - p < len) {
					return -1;
				}
/*
				We'd expect a length of 3 with the first byte being '07' to
				account for the trailing ignore bits in the second byte.
				But it doesn't appear all certificates adhere to the ASN.1
				encoding standard very closely.  Just set it all aside for 
				user to interpret as necessary.
*/
				extensions->keyUsage = psMalloc(pool, len);
				memcpy(extensions->keyUsage, p, len);
				extensions->keyUsageLen = len;
				p = p + len;
				break;
			case EXT_SUBJ_KEY_ID:
/*
				The value of the subject key identifier MUST be the value
				placed in the key identifier field of the Auth Key Identifier
				extension of certificates issued by the subject of
				this certificate.
*/
				if (*p++ != ASN_OCTET_STRING || asnParseLength(&p,
						(int32)(extEnd - p), &(extensions->sk.len)) < 0 ||
						extEnd - p < extensions->sk.len) {
					return -1;
				}
				extensions->sk.id = psMalloc(pool, extensions->sk.len);
				if (extensions->sk.id == NULL) {
					return -8; /* SSL_MEM_ERROR */
				}
				memcpy(extensions->sk.id, p, extensions->sk.len);
				p = p + extensions->sk.len;
				break;
#endif /* USE_FULL_CERT_PARSE */
/*
			Unsupported or skipping because USE_FULL_CERT_PARSE is undefined
*/
			default:
				if (critical) {
/*
					SPEC DIFFERENCE:  Ignoring an unrecognized critical
					extension.  The specification dictates an error should
					occur, but real-world experience has shown this is not
					a realistic or desirable action.  Also, no other SSL
					implementations have been found to error in this case.
					It is NOT a security risk in an RSA authenticaion sense.
*/
					matrixStrDebugMsg("Unknown critical ext encountered\n",
						NULL);
				}
				p++;
/*
				Skip over based on the length reported from the ASN_SEQUENCE
				surrounding the entire extension.  It is not a guarantee that
				the value of the extension itself will contain it's own length.
*/
				p = p + (fullExtLen - (p - extStart));
				break;
		}
	}
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Walk through the certificate chain and validate it.  Return the final
	member of the chain as the subjectCert that can then be validated against
	the CAs.  The subjectCert points into the chain param (no need to free)
*/
int32 matrixX509ValidateCertChain(psPool_t *pool, sslCert_t *chain, 
							sslCert_t **subjectCert, int32 *valid)
{
	sslCert_t	*ic;

	*subjectCert = chain;
	*valid = 1;
	while ((*subjectCert)->next != NULL) {
		ic = (*subjectCert)->next;
		if (matrixX509ValidateCertInternal(pool, *subjectCert, ic, 1) < 0) {
			*valid = -1;
			return -1;
		}
/*
		If any portion is invalid, it's all invalid
*/
		if ((*subjectCert)->valid != 1) {
			*valid = -1;
		}
		*subjectCert = (*subjectCert)->next;
	}
	return 0;
}

/******************************************************************************/
/*
	A signature validation for certificates.  -1 return is an error.  The success
	of the validation is returned in the 'valid' param of the subjectCert.
	1 if the issuerCert	signed the subject cert. -1 if not
*/
int32 matrixX509ValidateCert(psPool_t *pool, sslCert_t *subjectCert, 
						   sslCert_t *issuerCert, int32 *valid)
{
	if (matrixX509ValidateCertInternal(pool, subjectCert, issuerCert, 0) < 0) {
		*valid = -1;
		return -1;
	}
	*valid = subjectCert->valid;
	return 0;
}

static int32 matrixX509ValidateCertInternal(psPool_t *pool,
				sslCert_t *subjectCert, sslCert_t *issuerCert, int32 chain)
{
	sslCert_t		*ic;
	unsigned char	sigOut[10 + SSL_SHA1_HASH_SIZE + 5];	/* See below */
	int32			sigLen, sigType, rc;

	subjectCert->valid = -1;
/*
	Supporting a one level chain or a self-signed cert.  If the issuer
	is NULL, the self-signed test is done.
*/
	if (issuerCert == NULL) {
		matrixStrDebugMsg("Warning:  No CA to validate cert with\n", NULL);
		matrixStrDebugMsg("\tPerforming self-signed CA test\n", NULL);
		ic = subjectCert;
	} else {
		ic = issuerCert;
	}
/*
	Path confirmation.	If this is a chain verification, do not allow
	any holes in the path.  Error out if issuer does not have CA permissions
	or if hashes do not match anywhere along the way.
*/
	while (ic) {
		if (subjectCert != ic) {
/*
			Certificate authority constraint only available in version 3 certs
*/
			if ((ic->version > 1) && (ic->extensions.bc.ca <= 0)) {
				if (chain) {
					return -1;
				}
				ic = ic->next;
				continue;
			}
/*
			Use sha1 hash of issuer fields computed at parse time to compare
*/
			if (memcmp(subjectCert->issuer.hash, ic->subject.hash,
					SSL_SHA1_HASH_SIZE) != 0) {
				if (chain) {
					return -1;
				}
				ic = ic->next;
				continue;
			}
		}
/*
		Signature confirmation
		The sigLen is the ASN.1 size in bytes for encoding the hash.
		The magic 10 is comprised of the SEQUENCE and ALGORITHM ID overhead.
		The magic 8 and 5 are the OID lengths of the corresponding algorithm.
		NOTE: if sigLen is modified, above sigOut static size must be changed
*/
		if (subjectCert->sigAlgorithm ==  OID_RSA_MD5 ||
				subjectCert->sigAlgorithm == OID_RSA_MD2) {
			sigType = RSA_SIG;
			sigLen = 10 + SSL_MD5_HASH_SIZE + 8;	/* See above */
		} else if (subjectCert->sigAlgorithm == OID_RSA_SHA1) {
			sigLen = 10 + SSL_SHA1_HASH_SIZE + 5;	/* See above */
			sigType = RSA_SIG;
		} else {
			matrixStrDebugMsg("Unsupported signature algorithm\n", NULL);
			return -1;
		}
		
		if (sigType == RSA_SIG) {
			sslAssert(sigLen <= sizeof(sigOut));

			/* note: on error & no CA, flag as invalid, but don't exit as error here (<1.8.7? behavior) -- zzz */
			if (matrixRsaDecryptPub(pool, &(ic->publicKey),
					subjectCert->signature,	subjectCert->signatureLen, sigOut,
					sigLen) < 0) {
				matrixStrDebugMsg("Unable to RSA decrypt signature\n", NULL);
				if (issuerCert) return -1;
				rc = -1;
			} else {
				rc = psAsnConfirmSignature(subjectCert->sigHash, sigOut, sigLen);
			}
		}
/*
		If this is a chain test, fail on any gaps in the chain
*/
		if (rc < 0) {
			if (chain) {
				return -1;
			}
			ic = ic->next;
			continue;
		}
/*
		Fall through to here only if passed signature check.
*/
		subjectCert->valid = 1;
		break;
	}
	return 0;
}

/******************************************************************************/
/*
	Calls a user defined callback to allow for manual validation of the
	certificate.
*/
int32 matrixX509UserValidator(psPool_t *pool, sslCert_t *subjectCert,
			int32 (*certValidator)(sslCertInfo_t *t, void *arg), void *arg)
{
	sslCertInfo_t	*cert, *current, *next;
	int32			rc;

	if (certValidator == NULL) {
		return 0;
	}
/*
	Pass the entire certificate chain to the user callback.
*/
	current = cert = psMalloc(pool, sizeof(sslCertInfo_t));
	if (current == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(cert, 0x0, sizeof(sslCertInfo_t));
	while (subjectCert) {
		
		current->issuer.commonName = subjectCert->issuer.commonName;
		current->issuer.country = subjectCert->issuer.country;
		current->issuer.locality = subjectCert->issuer.locality;
		current->issuer.organization = subjectCert->issuer.organization;
		current->issuer.orgUnit = subjectCert->issuer.orgUnit;
		current->issuer.state = subjectCert->issuer.state;

		current->subject.commonName = subjectCert->subject.commonName;
		current->subject.country = subjectCert->subject.country;
		current->subject.locality = subjectCert->subject.locality;
		current->subject.organization = subjectCert->subject.organization;
		current->subject.orgUnit = subjectCert->subject.orgUnit;
		current->subject.state = subjectCert->subject.state;

		current->serialNumber = subjectCert->serialNumber;
		current->serialNumberLen = subjectCert->serialNumberLen;
		current->verified = subjectCert->valid;
		current->notBefore = subjectCert->notBefore;
		current->notAfter = subjectCert->notAfter;

		current->subjectAltName = subjectCert->extensions.san;

		if (subjectCert->certAlgorithm == OID_RSA_MD5 ||
				subjectCert->certAlgorithm == OID_RSA_MD2) {
			current->sigHashLen = SSL_MD5_HASH_SIZE;
		} else if (subjectCert->certAlgorithm == OID_RSA_SHA1) {
			current->sigHashLen = SSL_SHA1_HASH_SIZE;
		}
		current->sigHash = (char*)subjectCert->sigHash;
		if (subjectCert->next) {
			next = psMalloc(pool, sizeof(sslCertInfo_t));
			if (next == NULL) {
				while (cert) {
					next = cert->next;
					psFree(cert);
					cert = next;
				}
				return -8; /* SSL_MEM_ERROR */
			}
			memset(next, 0x0, sizeof(sslCertInfo_t));
			current->next = next;
			current = next;
		}
		subjectCert = subjectCert->next;
	}
/*
	The user callback
*/
	rc = certValidator(cert, arg);
/*
	Free the chain
*/
	while (cert) {
		next = cert->next;
		psFree(cert);
		cert = next;
	}
	return rc;
}
#endif /* USE_X509 */
#endif /* USE_RSA */


/******************************************************************************/


