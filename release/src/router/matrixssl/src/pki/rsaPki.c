/*
 *	rsaPki.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	RSA key and cert reading
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

#ifdef VXWORKS
#include <vxWorks.h>
#endif /* VXWORKS */

#include "pkiInternal.h"

#ifndef WINCE
#ifdef USE_FILE_SYSTEM
	#include <sys/stat.h>
	#include <signal.h>
#endif /* USE_FILE_SYSTEM */
#endif /* WINCE */
/*
	For our purposes USE_RSA is used to indicate RSA private key handling.
	USE_X509 indicates certificate handling and those blocks should be
	wrapped inside USE_RSA because that is the only key type currently supported
*/
#ifdef USE_RSA

#define ATTRIB_COUNTRY_NAME		6
#define ATTRIB_LOCALITY			7
#define ATTRIB_ORGANIZATION		10
#define ATTRIB_ORG_UNIT			11
#define ATTRIB_DN_QUALIFIER		46
#define ATTRIB_STATE_PROVINCE	8
#define ATTRIB_COMMON_NAME		3


#ifdef USE_3DES
static const char encryptHeader[] = "DEK-Info: DES-EDE3-CBC,";
static int32 hexToBinary(unsigned char *hex, unsigned char *bin, int32 binlen);
#endif

static int32 psAsnParsePrivateKey(psPool_t *pool, unsigned char **pp,
								  int32 size, sslRsaKey_t *key);
#endif /* USE_RSA */


/******************************************************************************/
/*
	Open and close the PKI module.  These routines are called once in the 
	lifetime of the application and initialize and clean up the library 
	respectively.
*/
int32 matrixPkiOpen(void)
{
	if (sslOpenOsdep() < 0) {
		matrixStrDebugMsg("Osdep open failure\n", NULL);
		return -1;
	}
	return 0;
}

void matrixPkiClose(void)
{
	sslCloseOsdep();
}
#ifdef USE_FILE_SYSTEM
/******************************************************************************/
/*
	Return the file contents given a file name in a single allocated buffer.
	Not a good routine to use generally with the fixed mem stuff.  Not 
	actually doing a 'binary' file read.  Only using the 'r' attribute since
	all the cert and key files are text.
*/
int32 psGetFileBin(psPool_t *pool, const char *fileName, unsigned char **bin,
				 int32 *binLen)
{
	FILE	*fp;
	struct	stat	fstat;
	size_t	tmp = 0;

	*binLen = 0;
	*bin = NULL;

	if (fileName == NULL) {
		return -1;
	}
	if ((stat(fileName, &fstat) != 0) || (fp = fopen(fileName, "r")) == NULL) {
		return -7; /* FILE_NOT_FOUND */
	}

	*bin = psMalloc(pool, fstat.st_size + 1);
	if (*bin == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(*bin, 0x0, fstat.st_size + 1);
	while (((tmp = fread(*bin + *binLen, sizeof(char), 512, fp)) > 0) &&
			(*binLen < fstat.st_size)) { 
		*binLen += (int32)tmp;
	}
	fclose(fp);
	return 0;
}

/******************************************************************************/
/*
 *	Public API to return an ASN.1 encoded key stream from a PEM private
 *	key file
 *
 *	If password is provided, we only deal with 3des cbc encryption
 *	Function allocates key on success.  User must free.
 */
int32 matrixX509ReadPrivKey(psPool_t *pool, const char *fileName,
				const char *password, unsigned char **keyMem, int32 *keyMemLen)
{
	unsigned char	*keyBuf, *DERout;
	char			*start, *end, *endTmp;
	int32			keyBufLen, rc, DERlen, PEMlen = 0;
#ifdef USE_3DES
	sslCipherContext_t	ctx;
	unsigned char	passKey[SSL_DES3_KEY_LEN];
	unsigned char	cipherIV[SSL_DES3_IV_LEN];
	int32			tmp, encrypted = 0;
#endif /* USE_3DES */

	if (fileName == NULL) {
		return 0;
	}
	*keyMem = NULL;
	if ((rc = psGetFileBin(pool, fileName, &keyBuf, &keyBufLen)) < 0) {
		return rc;
	}
	start = end = NULL;

/*
 *	Check header and encryption parameters.
 */
	if (((start = strstr((char*)keyBuf, "-----BEGIN")) != NULL) && 
			((start = strstr((char*)keyBuf, "PRIVATE KEY-----")) != NULL) &&
			((end = strstr(start, "-----END")) != NULL) &&
			((endTmp = strstr(end, "PRIVATE KEY-----")) != NULL)) {
		start += strlen("PRIVATE KEY-----");
		while (*start == '\r' || *start == '\n') {
			start++;
		}
		PEMlen = (int32)(end - start);
	} else {
		matrixStrDebugMsg("Error parsing private key buffer\n", NULL);
		psFree(keyBuf);
		return -1;
	}

	if (strstr((char*)keyBuf, "Proc-Type:") &&
			strstr((char*)keyBuf, "4,ENCRYPTED")) {
#ifdef USE_3DES
		encrypted++;
		if (password == NULL) {
			matrixStrDebugMsg("No password given for encrypted private key\n",
				NULL);
			psFree(keyBuf);
			return -1;
		}
		if ((start = strstr((char*)keyBuf, encryptHeader)) == NULL) {
			matrixStrDebugMsg("Unrecognized private key file encoding\n",
				NULL);
			psFree(keyBuf);
			return -1;
		}
		start += strlen(encryptHeader);
		/* SECURITY - we assume here that header points to at least 16 bytes of data */
		tmp = hexToBinary((unsigned char*)start, cipherIV, SSL_DES3_IV_LEN);
		if (tmp < 0) {
			matrixStrDebugMsg("Invalid private key file salt\n", NULL);
			psFree(keyBuf);
			return -1;
		}
		start += tmp;
		generate3DESKey((unsigned char*)password, (int32)strlen(password),
			cipherIV, (unsigned char*)passKey);
		PEMlen = (int32)(end - start);
#else  /* !USE_3DES */
/*
 *		The private key is encrypted, but 3DES support has been turned off
 */
		matrixStrDebugMsg("3DES has been disabled for private key decrypt\n", NULL);
		psFree(keyBuf);
		return -1;  
#endif /* USE_3DES */
	}

/*
	Take the raw input and do a base64 decode
 */
	DERout = psMalloc(pool, PEMlen);
	if (DERout == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	DERlen = PEMlen;
	if (ps_base64_decode((unsigned char*)start, PEMlen, DERout,
			(uint32*)&DERlen) != 0) {
		psFree(DERout);
		psFree(keyBuf);
		matrixStrDebugMsg("Unable to base64 decode private key\n", NULL);
		return -1;
	}
	psFree(keyBuf);
#ifdef USE_3DES
/*
 *	Decode
 */
	if (encrypted == 1 && password) {
		matrix3desInit(&ctx, cipherIV, passKey, SSL_DES3_KEY_LEN);
		matrix3desDecrypt(&ctx, DERout, DERout, DERlen);
	}
#endif /* USE_3DES */
/*
	Don't parse this here.  Return the ASN.1 encoded buf to be
	consistent with the other mem APIs.  Use the ParsePrivKey 
	function if you want the structure format
*/
	*keyMem = DERout;
	*keyMemLen = DERlen;
	return rc;
}

#ifdef USE_3DES
/******************************************************************************/
/*
	Convert an ASCII hex representation to a binary buffer.
	Decode enough data out of 'hex' buffer to produce 'binlen' bytes in 'bin'
	Two digits of ASCII hex map to the high and low nybbles (in that order),
	so this function assumes that 'hex' points to 2x 'binlen' bytes of data.
	Return the number of bytes processed from hex (2x binlen) or < 0 on error.
*/
static int32 hexToBinary(unsigned char *hex, unsigned char *bin, int32 binlen)
{
	unsigned char	*end, c, highOrder;

	highOrder = 1;
	for (end = hex + binlen * 2; hex < end; hex++) {
		c = *hex;
		if ('0' <= c && c <='9') {
			c -= '0';
		} else if ('a' <= c && c <='f') {
			c -= ('a' - 10);
		} else if ('A' <= c && c <='F') {
			c -= ('A' - 10);
		} else {
			return -1;
		}
		if (highOrder++ & 0x1) {
			*bin = c << 4;
		} else {
			*bin |= c;
			bin++;
		}
	}
	return binlen * 2;
}
#endif /* USE_3DES */
#endif /* USE_FILE_SYSTEM */

/******************************************************************************/
/*
 *	In memory version of matrixRsaReadPrivKey.  The keyBuf is the raw
 *	ASN.1 encoded buffer.
 */
int32 matrixRsaParsePrivKey(psPool_t *pool, unsigned char *keyBuf,
							  int32 keyBufLen, sslRsaKey_t **key)
{
	unsigned char	*asnp;

/*
	Now have the DER stream to extract from in asnp
 */
	*key = psMalloc(pool, sizeof(sslRsaKey_t));
	if (*key == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(*key, 0x0, sizeof(sslRsaKey_t));

	asnp = keyBuf;
	if (psAsnParsePrivateKey(pool, &asnp, keyBufLen, *key) < 0) {
		matrixRsaFreeKey(*key);
		*key = NULL;
		matrixStrDebugMsg("Unable to ASN parse private key.\n", NULL);
		return -1;
	}

	return 0;
}

/******************************************************************************/
/*
	Binary to struct helper for RSA public keys.
*/
int32 matrixRsaParsePubKey(psPool_t *pool, unsigned char *keyBuf,
							  int32 keyBufLen, sslRsaKey_t **key)
{
	unsigned char	*p, *end;
	int32			len;

	p = keyBuf;
	end = p + keyBufLen;
/*
	Supporting both the PKCS#1 RSAPublicKey format and the
	X.509 SubjectPublicKeyInfo format.  If encoding doesn't start with
	the SEQUENCE identifier for the SubjectPublicKeyInfo format, jump down
	to the RSAPublicKey subset parser and try that
*/
	if (getSequence(&p, (int32)(end - p), &len) == 0) {
		if (getAlgorithmIdentifier(&p, (int32)(end - p), &len, 1) < 0) {
			return -1;
		}
	}
/*
	Now have the DER stream to extract from in asnp
 */
	*key = psMalloc(pool, sizeof(sslRsaKey_t));
	if (*key == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memset(*key, 0x0, sizeof(sslRsaKey_t));
	if (getPubKey(pool, &p, (int32)(end - p), *key) < 0) {
		matrixRsaFreeKey(*key);
		*key = NULL;
		matrixStrDebugMsg("Unable to ASN parse public key\n", NULL);
		return -1;
	}
	return 0;
}

/******************************************************************************/
/*
 *	Free an RSA key.  mp_clear will zero the memory of each element and free it.
 */
void matrixRsaFreeKey(sslRsaKey_t *key)
{
	mp_clear(&(key->N));
	mp_clear(&(key->e));
	mp_clear(&(key->d));
	mp_clear(&(key->p));
	mp_clear(&(key->q));
	mp_clear(&(key->dP));
	mp_clear(&(key->dQ));
	mp_clear(&(key->qP));
	psFree(key);
}

/******************************************************************************/
/*
	Parse a a private key structure in DER formatted ASN.1
	Per ftp://ftp.rsasecurity.com/pub/pkcs/pkcs-1/pkcs-1v2-1.pdf
	RSAPrivateKey ::= SEQUENCE {
		version Version,
		modulus INTEGER, -- n
		publicExponent INTEGER, -- e
		privateExponent INTEGER, -- d
		prime1 INTEGER, -- p
		prime2 INTEGER, -- q
		exponent1 INTEGER, -- d mod (p-1)
		exponent2 INTEGER, -- d mod (q-1)
		coefficient INTEGER, -- (inverse of q) mod p
		otherPrimeInfos OtherPrimeInfos OPTIONAL
	}
	Version ::= INTEGER { two-prime(0), multi(1) }
	  (CONSTRAINED BY {-- version must be multi if otherPrimeInfos present --})

	Which should look something like this in hex (pipe character 
	is used as a delimiter):
	ftp://ftp.rsa.com/pub/pkcs/ascii/layman.asc
	30	Tag in binary: 00|1|10000 -> UNIVERSAL | CONSTRUCTED | SEQUENCE (16)
	82	Length in binary: 1 | 0000010 -> LONG LENGTH | LENGTH BYTES (2)
	04 A4	Length Bytes (1188)
	02	Tag in binary: 00|0|00010 -> UNIVERSAL | PRIMITIVE | INTEGER (2)
	01	Length in binary: 0|0000001 -> SHORT LENGTH | LENGTH (1)
	00	INTEGER value (0) - RSAPrivateKey.version
	02	Tag in binary: 00|0|00010 -> UNIVERSAL | PRIMITIVE | INTEGER (2)
	82	Length in binary: 1 | 0000010 -> LONG LENGTH | LENGTH BYTES (2)
	01 01	Length Bytes (257)
	[]	257 Bytes of data - RSAPrivateKey.modulus (2048 bit key)
	02	Tag in binary: 00|0|00010 -> UNIVERSAL | PRIMITIVE | INTEGER (2)
	03	Length in binary: 0|0000011 -> SHORT LENGTH | LENGTH (3)
	01 00 01	INTEGER value (65537) - RSAPrivateKey.publicExponent
	...

	OtherPrimeInfos is not supported in this routine, and an error will be
	returned if they are present
*/

static int32 psAsnParsePrivateKey(psPool_t *pool, unsigned char **pp,
								  int32 size, sslRsaKey_t *key)
{
	unsigned char	*p, *end, *seq;
	int32			version, seqlen;

	key->optimized = 0;
	p = *pp;
	end = p + size;

	if (getSequence(&p, size, &seqlen) < 0) {
		matrixStrDebugMsg("ASN sequence parse error\n", NULL);
		return -1;
	}
	seq = p;
	if (getInteger(&p, (int32)(end - p), &version) < 0 || version != 0 ||
		getBig(pool, &p, (int32)(end - p), &(key->N)) < 0 ||
		mp_shrink(&key->N) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->e)) < 0 ||
		mp_shrink(&key->e) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->d)) < 0 ||
		mp_shrink(&key->d) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->p)) < 0 ||
		mp_shrink(&key->p) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->q)) < 0 ||
		mp_shrink(&key->q) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->dP)) < 0 ||
		mp_shrink(&key->dP) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->dQ)) < 0 ||
		mp_shrink(&key->dQ) != MP_OKAY ||
		getBig(pool, &p, (int32)(end - p), &(key->qP)) < 0 ||
		mp_shrink(&key->qP) != MP_OKAY ||
		(int32)(p - seq) != seqlen) {
		matrixStrDebugMsg("ASN key extract parse error\n", NULL);
		return -1;
	}
/*
	If we made it here, the key is ready for optimized decryption
*/
	key->optimized = 1;

	*pp = p;
/*
	Set the key length of the key
*/
	key->size = mp_unsigned_bin_size(&key->N);
	return 0;
}


/******************************************************************************/
/*
	Implementations of this specification MUST be prepared to receive
	the following standard attribute types in issuer names:
	country, organization, organizational-unit, distinguished name qualifier,
	state or province name, and common name 
*/
int32 getDNAttributes(psPool_t *pool, unsigned char **pp, int32 len, 
						   DNattributes_t *attribs)
{
	sslSha1Context_t	hash;
	unsigned char		*p = *pp;
	unsigned char		*dnEnd, *dnStart;
	int32				llen, setlen, arcLen, id, stringType;
	char				*stringOut;

	dnStart = p;
	if (getSequence(&p, len, &llen) < 0) {
		return -1;
	}
	dnEnd = p + llen;

	matrixSha1Init(&hash);
	while (p < dnEnd) {
		if (getSet(&p, (int32)(dnEnd - p), &setlen) < 0) {
			return -1;
		}
		if (getSequence(&p, (int32)(dnEnd - p), &llen) < 0) {
			return -1;
		}
		if (dnEnd <= p || (*(p++) != ASN_OID) ||
				asnParseLength(&p, (int32)(dnEnd - p), &arcLen) < 0 || 
				(dnEnd - p) < arcLen) {
			return -1;
		}
/*
		id-at   OBJECT IDENTIFIER       ::=     {joint-iso-ccitt(2) ds(5) 4}
		id-at-commonName		OBJECT IDENTIFIER		::=		{id-at 3}
		id-at-countryName		OBJECT IDENTIFIER		::=		{id-at 6}
		id-at-localityName		OBJECT IDENTIFIER		::=		{id-at 7}
		id-at-stateOrProvinceName		OBJECT IDENTIFIER	::=	{id-at 8}
		id-at-organizationName			OBJECT IDENTIFIER	::=	{id-at 10}
		id-at-organizationalUnitName	OBJECT IDENTIFIER	::=	{id-at 11}
*/
		*pp = p;
/*
		FUTURE: Currently skipping OIDs not of type {joint-iso-ccitt(2) ds(5) 4}
		However, we could be dealing with an OID we MUST support per RFC.
		domainComponent is one such example.
*/
		if (dnEnd - p < 2) {
			return -1;
		}
		if ((*p++ != 85) || (*p++ != 4) ) {
			p = *pp;
/*
			Move past the OID and string type, get data size, and skip it.
			NOTE: Have had problems parsing older certs in this area.
*/
			if (dnEnd - p < arcLen + 1) {
				return -1;
			}
			p += arcLen + 1;
			if (asnParseLength(&p, (int32)(dnEnd - p), &llen) < 0 || 
					dnEnd - p < llen) {
				return -1;
			}
			p = p + llen;
			continue;
		}
/*
		Next are the id of the attribute type and the ASN string type
*/
		if (arcLen != 3 || dnEnd - p < 2) {
			return -1;
		}
		id = (int32)*p++;
/*
		Done with OID parsing
*/
		stringType = (int32)*p++;

		if (asnParseLength(&p, (int32)(dnEnd - p), &llen) < 0 ||
				dnEnd - p < llen) {
			return -1;
		}
		switch (stringType) {
			case ASN_PRINTABLESTRING:
			case ASN_UTF8STRING:
			case ASN_IA5STRING:
			case ASN_T61STRING:
			case ASN_BMPSTRING:
				stringOut = psMalloc(pool, llen + 1);
				if (stringOut == NULL) {
					return -8; /* SSL_MEM_ERROR */
				}
				memcpy(stringOut, p, llen);
				stringOut[llen] = '\0';
/*
                Catch any hidden \0 chars in these members to address the
                issue of www.goodguy.com\0badguy.com
*/
                if (strlen(stringOut) != llen) {
                    psFree(stringOut);
                    return -1;
                }

				p = p + llen;
				break;
			default:
				matrixStrDebugMsg("Parsing untested DN attrib type\n", NULL);
				return -1;
		}

		switch (id) {
			case ATTRIB_COUNTRY_NAME:
				if (attribs->country) {
					psFree(attribs->country);
				}
				attribs->country = stringOut;
				break;
			case ATTRIB_STATE_PROVINCE:
				if (attribs->state) {
					psFree(attribs->state);
				}
				attribs->state = stringOut;
				break;
			case ATTRIB_LOCALITY:
				if (attribs->locality) {
					psFree(attribs->locality);
				}
				attribs->locality = stringOut;
				break;
			case ATTRIB_ORGANIZATION:
				if (attribs->organization) {
					psFree(attribs->organization);
				}
				attribs->organization = stringOut;
				break;
			case ATTRIB_ORG_UNIT:
				if (attribs->orgUnit) {
					psFree(attribs->orgUnit);
				}
				attribs->orgUnit = stringOut;
				break;
			case ATTRIB_COMMON_NAME:
				if (attribs->commonName) {
					psFree(attribs->commonName);
				}
				attribs->commonName = stringOut;
				break;
/*
			Not a MUST support
*/
			default:
				psFree(stringOut);
				stringOut = NULL;
				break;
		}
/*
		Hash up the DN.  Nice for validation later
*/
		if (stringOut != NULL) {
			matrixSha1Update(&hash, (unsigned char*)stringOut, llen);
		}
	}
	matrixSha1Final(&hash, (unsigned char*)attribs->hash);
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Get the BIT STRING key and plug into RSA structure.  Not included in
	asn1.c because it deals directly with the sslRsaKey_t struct.
*/
int32 getPubKey(psPool_t *pool, unsigned char **pp, int32 len, 
					 sslRsaKey_t *pubKey)
{
	unsigned char	*p = *pp;
	int32			pubKeyLen, ignore_bits, seqLen;

	if (len < 1 || (*(p++) != ASN_BIT_STRING) ||
			asnParseLength(&p, len - 1, &pubKeyLen) < 0 || 
			(len - 1) < pubKeyLen) {
		return -1;
	}
	
	ignore_bits = *p++;
/*
	We assume this is always zero
*/
	sslAssert(ignore_bits == 0);

	if (getSequence(&p, pubKeyLen, &seqLen) < 0 ||
			getBig(pool, &p, seqLen, &pubKey->N) < 0 ||
			getBig(pool, &p, seqLen, &pubKey->e) < 0) {
		return -1;
	}
	pubKey->size = mp_unsigned_bin_size(&pubKey->N);

	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Free helper
*/
void psFreeDNStruct(DNattributes_t *dn)
{
	if (dn->country)		psFree(dn->country);
	if (dn->state)			psFree(dn->state);
	if (dn->locality)		psFree(dn->locality);
	if (dn->organization)	psFree(dn->organization);
	if (dn->orgUnit)		psFree(dn->orgUnit);
	if (dn->commonName)		psFree(dn->commonName);
}


/******************************************************************************/
