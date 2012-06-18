/*
 *	asn1.c
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

/******************************************************************************/
/*
	On success, p will be updated to point to first character of value and
	valLen will contain number of bytes in value
	Return:
		0			Success
		< 0			Error
*/
int32 asnParseLength(unsigned char **p, int32 size, int32 *valLen)
{
	unsigned char	*c, *end;
	int32			len, olen;

	c = *p;
	end = c + size;
	if (end - c < 1) {
		return -1;
	}
/*
	If the length byte has high bit only set, it's an indefinite length
	We don't support this!
*/
	if (*c == 0x80) {
		*valLen = -1;
		matrixStrDebugMsg("Unsupported: ASN indefinite length\n", NULL);
		return -1;
	}
/*
	If the high bit is set, the lower 7 bits represent the number of 
	bytes that follow and represent length
	If the high bit is not set, the lower 7 represent the actual length
*/
	len = *c & 0x7F;
	if (*(c++) & 0x80) {
/*
		Make sure there aren't more than 4 bytes of length specifier,
		and that we have that many bytes left in the buffer
*/
		if (len > sizeof(int32) || len == 0x7f || (end - c) < len) {
			return -1;
		}
		olen = 0;
		while (len-- > 0) {
			olen = (olen << 8) | *c;
			c++;
		}
		if (olen < 0 || olen > INT_MAX) {
			return -1;
		}
		len = olen;
	}
	*p = c;
	*valLen = len;
	return 0;
}

/******************************************************************************/
/*
	Callback to extract a big int32 (stream of bytes) from the DER stream
*/
int32 getBig(psPool_t *pool, unsigned char **pp, int32 len, mp_int *big)
{
	unsigned char	*p = *pp;
	int32			vlen;

	if (len < 1 || *(p++) != ASN_INTEGER ||
			asnParseLength(&p, len - 1, &vlen) < 0 || (len -1) < vlen) {
		matrixStrDebugMsg("ASN getBig failed\n", NULL);
		return -1;
	}
	mp_init(pool, big);
	if (mp_read_unsigned_bin(big, p, vlen) != 0) {
		mp_clear(big);
		matrixStrDebugMsg("ASN getBig failed\n", NULL);
		return -1;
	}
	p += vlen;
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Although a certificate serial number is encoded as an integer type, that
	doesn't prevent it from being abused as containing a variable length
	binary value.  Get it here.
*/	
int32 getSerialNum(psPool_t *pool, unsigned char **pp, int32 len, 
						unsigned char **sn, int32 *snLen)
{
	unsigned char	*p = *pp;
	int32			vlen;

	if ((*p != (ASN_CONTEXT_SPECIFIC | ASN_PRIMITIVE | 2)) &&
			(*p != ASN_INTEGER)) {
		matrixStrDebugMsg("ASN getSerialNumber failed\n", NULL);
		return -1;
	}
	p++;

	if (len < 1 || asnParseLength(&p, len - 1, &vlen) < 0 || (len - 1) < vlen) {
		matrixStrDebugMsg("ASN getSerialNumber failed\n", NULL);
		return -1;
	}
	*snLen = vlen;
	*sn = psMalloc(pool, vlen);
	if (*sn == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memcpy(*sn, p, vlen);
	p += vlen;
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Callback to extract a sequence length from the DER stream
	Verifies that 'len' bytes are >= 'seqlen'
	Move pp to the first character in the sequence
*/
int32 getSequence(unsigned char **pp, int32 len, int32 *seqlen)
{
	unsigned char	*p = *pp;

	if (len < 1 || *(p++) != (ASN_SEQUENCE | ASN_CONSTRUCTED) || 
			asnParseLength(&p, len - 1, seqlen) < 0 || len < *seqlen) {
		return -1;
	}
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Extract a set length from the DER stream
*/
int32 getSet(unsigned char **pp, int32 len, int32 *setlen)
{
	unsigned char	*p = *pp;

	if (len < 1 || *(p++) != (ASN_SET | ASN_CONSTRUCTED) || 
			asnParseLength(&p, len - 1, setlen) < 0 || len < *setlen) {
		return -1;
	}
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Explicit value encoding has an additional tag layer
*/
int32 getExplicitVersion(unsigned char **pp, int32 len, int32 expVal, int32 *val)
{
	unsigned char	*p = *pp;
	int32			exLen;

	if (len < 1) {
		return -1;
	}
/*
	This is an optional value, so don't error if not present.  The default
	value is version 1
*/	
	if (*p != (ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | expVal)) {
		*val = 0;
		return 0;
	}
	p++;
	if (asnParseLength(&p, len - 1, &exLen) < 0 || (len - 1) < exLen) {
		return -1;
	}
	if (getInteger(&p, exLen, val) < 0) {
		return -1;
	}
	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Implementation specific OID parser for suppported RSA algorithms
*/
int32 getAlgorithmIdentifier(unsigned char **pp, int32 len, int32 *oi,
								  int32 isPubKey)
{
	unsigned char	*p = *pp, *end;
	int32			arcLen, llen;

	end = p + len;
	if (len < 1 || getSequence(&p, len, &llen) < 0) {
		return -1;
	}
	if (end - p < 1) {
		return -1;
	}
	if (*(p++) != ASN_OID || asnParseLength(&p, (int32)(end - p), &arcLen) < 0 ||
			llen < arcLen) {
		return -1;
	}
/*
	List of expected (currently supported) OIDs  - RFC 3279
	algorithm				  summed	length		hex
	sha1						 88		05		2b0e03021a
	md2							646		08		2a864886f70d0202
	md5							649		08		2a864886f70d0205
	
	rsaEncryption				645		09		2a864886f70d010101
	md2WithRSAEncryption:		646		09		2a864886f70d010102
	md5WithRSAEncryption		648		09		2a864886f70d010104
	sha-1WithRSAEncryption		649		09		2a864886f70d010105

	Yes, the summing isn't ideal (as can be seen with the duplicate 649),
	but the specific implementation makes this ok.
*/
	if (end - p < 2) {
		return -1;
	}
	if (isPubKey && (*p != 0x2a) && (*(p + 1) != 0x86)) {
/*
		Expecting DSA here if not RSA, but OID doesn't always match
*/
		matrixStrDebugMsg("Unsupported algorithm identifier\n", NULL);
		return -1;
	}
	*oi = 0;
	while (arcLen-- > 0) {
		*oi += (int32)*p++;
	}
/*
	Each of these cases might have a trailing NULL parameter.  Skip it
*/
	if (*p != ASN_NULL) {
		*pp = p;
		return 0;
	}
	if (end - p < 2) {
		return -1;
	}
	*pp = p + 2;
	return 0;
}

/******************************************************************************/
/*
	Implementation specific date parser.
	Does not actually verify the date
*/
int32 getValidity(psPool_t *pool, unsigned char **pp, int32 len,
					   char **notBefore, char **notAfter)
{
	unsigned char	*p = *pp, *end;
	int32			seqLen, timeLen;

	end = p + len;
	if (len < 1 || *(p++) != (ASN_SEQUENCE | ASN_CONSTRUCTED) || 
			asnParseLength(&p, len - 1, &seqLen) < 0 || (end - p) < seqLen) {
		return -1;
	}
/*
	Have notBefore and notAfter times in UTCTime or GeneralizedTime formats
*/
	if ((end - p) < 1 || ((*p != ASN_UTCTIME) && (*p != ASN_GENERALIZEDTIME))) {
		return -1;
	}
	p++;
/*
	Allocate them as null terminated strings
*/
	if (asnParseLength(&p, seqLen, &timeLen) < 0 || (end - p) < timeLen) {
		return -1;
	}
	*notBefore = psMalloc(pool, timeLen + 1);
	if (*notBefore == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memcpy(*notBefore, p, timeLen);
	(*notBefore)[timeLen] = '\0';
	p = p + timeLen;
	if ((end - p) < 1 || ((*p != ASN_UTCTIME) && (*p != ASN_GENERALIZEDTIME))) {
		return -1;
	}
	p++;
	if (asnParseLength(&p, seqLen - timeLen, &timeLen) < 0 || 
			(end - p) < timeLen) {
		return -1;
	}
	*notAfter = psMalloc(pool, timeLen + 1);
	if (*notAfter == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memcpy(*notAfter, p, timeLen);
	(*notAfter)[timeLen] = '\0';
	p = p + timeLen;

	*pp = p;
	return 0;
}

/******************************************************************************/
/*
	Currently just returning the raw BIT STRING and size in bytes
*/
int32 getSignature(psPool_t *pool, unsigned char **pp, int32 len,
						unsigned char **sig, int32 *sigLen)
{
	unsigned char	*p = *pp, *end;
	int32			ignore_bits, llen;
	
	end = p + len;
	if (len < 1 || (*(p++) != ASN_BIT_STRING) ||
			asnParseLength(&p, len - 1, &llen) < 0 || (end - p) < llen) {
		return -1;
	}
	ignore_bits = *p++;
/*
	We assume this is always 0.
*/
	sslAssert(ignore_bits == 0);
/*
	Length included the ignore_bits byte
*/
	*sigLen = llen - 1;
	*sig = psMalloc(pool, *sigLen);
	if (*sig == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memcpy(*sig, p, *sigLen);
	*pp = p + *sigLen;
	return 0;
}

/******************************************************************************/
/*
	Could be optional.  If the tag doesn't contain the value from the left
	of the IMPLICIT keyword we don't have a match and we don't incr the pointer.
*/
int32 getImplicitBitString(psPool_t *pool, unsigned char **pp, int32 len, 
						int32 impVal, unsigned char **bitString, int32 *bitLen)
{
	unsigned char	*p = *pp;
	int32			ignore_bits;

	if (len < 1) {
		return -1;
	}
/*
	We don't treat this case as an error, because of the optional nature.
*/	
	if (*p != (ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED | impVal)) {
		return 0;
	}

	p++;
	if (asnParseLength(&p, len, bitLen) < 0) {
		return -1;
	}
	ignore_bits = *p++;
	sslAssert(ignore_bits == 0);

	*bitString = psMalloc(pool, *bitLen);
	if (*bitString == NULL) {
		return -8; /* SSL_MEM_ERROR */
	}
	memcpy(*bitString, p, *bitLen);
	*pp = p + *bitLen;
	return 0;
}

/******************************************************************************/
/*
	Get an integer
*/
int32 getInteger(unsigned char **pp, int32 len, int32 *val)
{
	unsigned char	*p = *pp, *end;
	uint32			ui;
	int32			vlen;

	end = p + len;
	if (len < 1 || *(p++) != ASN_INTEGER ||
			asnParseLength(&p, len - 1, &vlen) < 0) {
		matrixStrDebugMsg("ASN getInteger failed\n", NULL);
		return -1;
	}
/*
	This check prevents us from having a big positive integer where the 
	high bit is set because it will be encoded as 5 bytes (with leading 
	blank byte).  If that is required, a getUnsigned routine should be used
*/
	if (vlen > sizeof(int32) || end - p < vlen) {
		matrixStrDebugMsg("ASN getInteger failed\n", NULL);
		return -1;
	}
	ui = 0;
/*
	If high bit is set, it's a negative integer, so perform the two's compliment
	Otherwise do a standard big endian read (most likely case for RSA)
*/
	if (*p & 0x80) {
		while (vlen-- > 0) {
			ui = (ui << 8) | (*p ^ 0xFF);
			p++;
		}
		vlen = (int32)ui;
		vlen++;
		vlen = -vlen;
		*val = vlen;
	} else {
		while (vlen-- > 0) {
			ui = (ui << 8) | *p;
			p++;
		}
		*val = (int32)ui;
	}
	*pp = p;
	return 0;
}

/******************************************************************************/
