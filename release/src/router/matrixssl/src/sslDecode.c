/*
 *	sslDecode.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Secure Sockets Layer message decoding
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

#define SSL_MAX_IGNORED_MESSAGE_COUNT	1024

static int32 parseSSLHandshake(ssl_t *ssl, char *inbuf, int32 len);
static int32 parseSingleCert(ssl_t *ssl, unsigned char *c, unsigned char *end, 
						   int32 certLen);

/******************************************************************************/
/*
	Parse incoming data per http://wp.netscape.com/eng/ssl3
*/
int32 matrixSslDecode(ssl_t *ssl, sslBuf_t *in, sslBuf_t *out, 
						   unsigned char *error, unsigned char *alertLevel, 
						   unsigned char *alertDescription)
{
	unsigned char	*c, *p, *end, *pend, *oend;
	unsigned char	*mac, macError;
	int32			rc;
	unsigned char	padLen;
/*
	If we've had a protocol error, don't allow further use of the session
*/
	*error = SSL_ALERT_NONE;
	if (ssl->flags & SSL_FLAGS_ERROR || ssl->flags & SSL_FLAGS_CLOSED) {
		return SSL_ERROR;
	}
/*
	This flag is set if the previous call to this routine returned an SSL_FULL
	error from encodeResponse, indicating that there is data to be encoded, 
	but the out buffer was not big enough to handle it.  If we fall in this 
	case, the user has increased the out buffer size and is re-calling this 
	routine
*/
	if (ssl->flags & SSL_FLAGS_NEED_ENCODE) {
		ssl->flags &= ~SSL_FLAGS_NEED_ENCODE;
		goto encodeResponse;
	}

	c = in->start;
	end = in->end;
	oend = out->end;
/*
	Processing the SSL Record header:
	If the high bit of the first byte is set and this is the first 
	message we've seen, we parse the request as an SSLv2 request
	http://wp.netscape.com/eng/security/SSL_2.html
	SSLv2 also supports a 3 byte header when padding is used, but this should 
	not be required for the initial plaintext message, so we don't support it
	v3 Header:
		1 byte type
		1 byte major version
		1 byte minor version
		2 bytes length
	v2 Header
		2 bytes length (ignore high bit)
*/
decodeMore:
	sslAssert(out->end == oend);
	if (end - c == 0) {
/*
		This case could happen if change cipher spec was last
		message	in the buffer
*/
		return SSL_SUCCESS;
	}

	if (end - c < SSL2_HEADER_LEN) {
		return SSL_PARTIAL;
	}
	if (ssl->majVer != 0 || (*c & 0x80) == 0) {
		if (end - c < ssl->recordHeadLen) {
			return SSL_PARTIAL;
		}
		ssl->rec.type = *c; c++;
		ssl->rec.majVer = *c; c++;
		ssl->rec.minVer = *c; c++;
		ssl->rec.len = *c << 8; c++;
		ssl->rec.len += *c; c++;
	} else {
		ssl->rec.type = SSL_RECORD_TYPE_HANDSHAKE;
		ssl->rec.majVer = 2;
		ssl->rec.minVer = 0;
		ssl->rec.len = (*c & 0x7f) << 8; c++;
		ssl->rec.len += *c; c++;
	}
/*
	Validate the various record headers.  The type must be valid,
	the major and minor versions must match the negotiated versions (if we're
	past ClientHello) and the length must be < 16K and > 0
*/
	if (ssl->rec.type != SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC &&
			ssl->rec.type != SSL_RECORD_TYPE_ALERT &&
			ssl->rec.type != SSL_RECORD_TYPE_HANDSHAKE &&
			ssl->rec.type != SSL_RECORD_TYPE_APPLICATION_DATA) {
		ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
		matrixIntDebugMsg("Record header type not valid: %d\n", ssl->rec.type);
		goto encodeResponse;
	}

/*
	Verify the record version numbers unless this is the first record we're
	reading.
*/
	if (ssl->hsState != SSL_HS_SERVER_HELLO &&
			ssl->hsState != SSL_HS_CLIENT_HELLO) {
		if (ssl->rec.majVer != ssl->majVer || ssl->rec.minVer != ssl->minVer) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Record header version not valid\n", NULL);
			goto encodeResponse;
		}
	}
/*
	Verify max and min record lengths
*/
	if (ssl->rec.len > SSL_MAX_RECORD_LEN || ssl->rec.len == 0) {
		ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
		matrixIntDebugMsg("Record header length not valid: %d\n", ssl->rec.len);
		goto encodeResponse;
	}
/*
	This implementation requires the entire SSL record to be in the 'in' buffer
	before we parse it.  This is because we need to MAC the entire record before
	allowing it to be used by the caller.  The only alternative would be to 
	copy the partial record to an internal buffer, but that would require more
	memory usage, which we're trying to keep low.
*/
	if (end - c < ssl->rec.len) {
		return SSL_PARTIAL;
	}

/*
	Make sure we have enough room to hold the decoded record
*/
	if ((out->buf + out->size) - out->end < ssl->rec.len) {
		return SSL_FULL;
	}

/*
	Decrypt the entire record contents.  The record length should be
	a multiple of block size, or decrypt will return an error
	If we're still handshaking and sending plaintext, the decryption 
	callback will point to a null provider that passes the data unchanged
*/
	if (ssl->decrypt(&ssl->sec.decryptCtx, c, out->end, ssl->rec.len) < 0) {
		ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
		goto encodeResponse;
	}
	c += ssl->rec.len;
/*
	If we're reading a secure message, we need to validate the MAC and 
	padding (if using a block cipher).  Insecure messages do not have 
	a trailing MAC or any padding.

	SECURITY - There are several vulnerabilities in block cipher padding
	that we handle in the below code.  For more information see:
	http://www.openssl.org/~bodo/tls-cbc.txt
*/
	if (ssl->flags & SSL_FLAGS_READ_SECURE) {
/*
		Verify the record is at least as big as the MAC
		Start tracking MAC errors, rather then immediately catching them to
		stop timing and alert description attacks that differentiate between
		a padding error and a MAC error.
*/
		if (ssl->rec.len < ssl->deMacSize) {
			ssl->err = SSL_ALERT_BAD_RECORD_MAC;
			matrixStrDebugMsg("Record length too short for MAC\n", NULL);
			goto encodeResponse;
		}
		macError = 0;
/*
		Decode padding only if blocksize is > 0 (we're using a block cipher),
		otherwise no padding will be present, and the mac is the last 
		macSize bytes of the record.
*/
		if (ssl->deBlockSize <= 1) {
			mac = out->end + ssl->rec.len - ssl->deMacSize;
		} else {
/*
			Verify the pad data for block ciphers
			c points within the cipher text, p points within the plaintext
			The last byte of the record is the pad length
*/
			p = out->end + ssl->rec.len;
			padLen = *(p - 1);
/*
			SSL3.0 requires the pad length to be less than blockSize
			TLS can have a pad length up to 255 for obfuscating the data len
*/
			if (ssl->majVer == SSL3_MAJ_VER && ssl->minVer == SSL3_MIN_VER && 
					padLen >= ssl->deBlockSize) {
				macError++;
			}
/*
			The minimum record length is the size of the mac, plus pad bytes
			plus one length byte
*/
			if (ssl->rec.len < ssl->deMacSize + padLen + 1) {
				macError++;
			}
/*
			The mac starts macSize bytes before the padding and length byte.
			If we have a macError, just fake the mac as the last macSize bytes
			of the record, so we are sure to have enough bytes to verify
			against, we'll fail anyway, so the actual contents don't matter.
*/
			if (!macError) {
				mac = p - padLen - 1 - ssl->deMacSize;
			} else {
				mac = out->end + ssl->rec.len - ssl->deMacSize;
			}
		}
/*
		Verify the MAC of the message by calculating our own MAC of the message
		and comparing it to the one in the message.  We do this step regardless
		of whether or not we've already set macError to stop timing attacks.
		Clear the mac in the callers buffer if we're successful
*/
		if (ssl->verifyMac(ssl, ssl->rec.type, out->end, 
				(int32)(mac - out->end), mac) < 0 || macError) {
			ssl->err = SSL_ALERT_BAD_RECORD_MAC;
			matrixStrDebugMsg("Couldn't verify MAC or pad of record data\n",
				NULL);
			goto encodeResponse;
		}
		memset(mac, 0x0, ssl->deMacSize);
/*
		Record data starts at out->end and ends at mac
*/
		p = out->end;
		pend = mac;
	} else {
/*
		The record data is the entire record as there is no MAC or padding
*/
		p = out->end;
		pend = mac = out->end + ssl->rec.len;
	}
/*
	Check now for maximum plaintext length of 16kb.  No appropriate
	SSL alert for this
*/
	if ((int32)(pend - p) > SSL_MAX_PLAINTEXT_LEN) {
		ssl->err = SSL_ALERT_BAD_RECORD_MAC;
		matrixStrDebugMsg("Record overflow\n", NULL);
		goto encodeResponse;
	}

/*
	Take action based on the actual record type we're dealing with
	'p' points to the start of the data, and 'pend' points to the end
*/
	switch (ssl->rec.type) {
	case SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC:
/*
		Body is single byte with value 1 to indicate that the next message
		will be encrypted using the negotiated cipher suite
*/
		if (pend - p < 1) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid length for CipherSpec\n", NULL);
			goto encodeResponse;
		}
		if (*p == 1) {
			p++;
		} else {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid value for CipherSpec\n", NULL);
			goto encodeResponse;
		}
		
/*
		If we're expecting finished, then this is the right place to get
		this record.  It is really part of the handshake but it has its
		own record type.
		Activate the read cipher callbacks, so we will decrypt incoming
		data from now on.
*/
		if (ssl->hsState == SSL_HS_FINISHED) {
			sslActivateReadCipher(ssl);
		} else {
			ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
			matrixIntDebugMsg("Invalid CipherSpec order: %d\n", ssl->hsState);
			goto encodeResponse;
		}
		in->start = c;
		goto decodeMore;

	case SSL_RECORD_TYPE_ALERT:
/*
		1 byte alert level (warning or fatal)
		1 byte alert description corresponding to SSL_ALERT_*
*/
		if (pend - p < 2) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Error in length of alert record\n", NULL);
			goto encodeResponse;
		}
		*alertLevel = *p; p++;
		*alertDescription = *p; p++;
/*
		If the alert is fatal, or is a close message (usually a warning),
		flag the session with ERROR so it cannot be used anymore.
		Caller can decide whether or not to close on other warnings.
*/
		if (*alertLevel == SSL_ALERT_LEVEL_FATAL) { 
			ssl->flags |= SSL_FLAGS_ERROR;
		}
		if (*alertDescription == SSL_ALERT_CLOSE_NOTIFY) {
			ssl->flags |= SSL_FLAGS_CLOSED;
		}
		return SSL_ALERT;

	case SSL_RECORD_TYPE_HANDSHAKE:
/*
		We've got one or more handshake messages in the record data.
		The handshake parsing function will take care of all messages
		and return an error if there is any problem.
		If there is a response to be sent (either a return handshake
		or an error alert, send it).  If the message was parsed, but no
		response is needed, loop up and try to parse another message
*/
		rc = parseSSLHandshake(ssl, (char*)p, (int32)(pend - p));
		switch (rc) {
		case SSL_SUCCESS:
			in->start = c;
			return SSL_SUCCESS;
		case SSL_PROCESS_DATA:
			in->start = c;
			goto encodeResponse;
		case SSL_ERROR:
			if (ssl->err == SSL_ALERT_NONE) {
				ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
			}
			goto encodeResponse;
		}
		break;

	case SSL_RECORD_TYPE_APPLICATION_DATA:
/*
		Data is in the out buffer, let user handle it
		Don't allow application data until handshake is complete, and we are
		secure.  It is ok to let application data through on the client
		if we are in the SERVER_HELLO state because this could mean that
		the client has sent a CLIENT_HELLO message for a rehandshake
		and is awaiting reply.
*/
		if ((ssl->hsState != SSL_HS_DONE && ssl->hsState != SSL_HS_SERVER_HELLO)
				|| !(ssl->flags & SSL_FLAGS_READ_SECURE)) {
			ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
			matrixIntDebugMsg("Incomplete handshake: %d\n", ssl->hsState);
			goto encodeResponse;
		}
/*
		SECURITY - If the mac is at the current out->end, then there is no data 
		in the record.  These records are valid, but are usually not sent by
		the application layer protocol.  Rather, they are initiated within the 
		remote SSL protocol implementation to avoid some types of attacks when
		using block ciphers.  For more information see:
		http://www.openssl.org/~bodo/tls-cbc.txt

		We eat these records here rather than passing them on to the caller.
		The rationale behind this is that if the caller's application protocol 
		is depending on zero length SSL messages, it will fail anyway if some of 
		those messages are initiated within the SSL protocol layer.  Also
		this clears up any confusion where the caller might interpret a zero
		length read as an end of file (EOF) or would block (EWOULDBLOCK) type
		scenario.

		SECURITY - Looping back up and ignoring the message has the potential
		for denial of service, because we are not changing the state of the
		system in any way when processing these messages.  To counteract this,
		we maintain a counter that we share with other types of ignored messages
*/
		in->start = c;
		if (out->end == mac) {
			if (ssl->ignoredMessageCount++ < SSL_MAX_IGNORED_MESSAGE_COUNT) {
				goto decodeMore;
			}
			ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
			matrixIntDebugMsg("Exceeded limit on ignored messages: %d\n", 
				SSL_MAX_IGNORED_MESSAGE_COUNT);
			goto encodeResponse;
		}
		if (ssl->ignoredMessageCount > 0) {
			ssl->ignoredMessageCount--;
		}
		out->end = mac;
		return SSL_PROCESS_DATA;
	}
/*
	Should not get here
*/
	matrixIntDebugMsg("Invalid record type in matrixSslDecode: %d\n",
		ssl->rec.type);
	return SSL_ERROR;

encodeResponse:
/*
	We decoded a record that needs a response, either a handshake response
	or an alert if we've detected an error.  

	SECURITY - Clear the decoded incoming record from outbuf before encoding
	the response into outbuf.  rec.len could be invalid, clear the minimum 
	of rec.len and remaining outbuf size
*/
	rc = min (ssl->rec.len, (int32)((out->buf + out->size) - out->end));
	if (rc > 0) {
		memset(out->end, 0x0, rc);
	}
	if (ssl->hsState == SSL_HS_HELLO_REQUEST) {
/*
		Don't clear the session info.  If receiving a HELLO_REQUEST from a 
		MatrixSSL enabled server the determination on whether to reuse the 
		session is made on that side, so always send the current session
*/
		rc = matrixSslEncodeClientHello(ssl, out, ssl->cipher->id);
	} else {
		rc = sslEncodeResponse(ssl, out);
	}
	if (rc == SSL_SUCCESS) {
		if (ssl->err != SSL_ALERT_NONE) {
			*error = (unsigned char)ssl->err;
			ssl->flags |= SSL_FLAGS_ERROR;
			return SSL_ERROR;
		}
		return SSL_SEND_RESPONSE;
	}
	if (rc == SSL_FULL) {
		ssl->flags |= SSL_FLAGS_NEED_ENCODE;
		return SSL_FULL;
	}
	return SSL_ERROR;
}

/******************************************************************************/
/*
	The workhorse for parsing handshake messages.  Also enforces the state
	machine	for proper ordering of handshake messages.
	Parameters:
	ssl - ssl context
	inbuf - buffer to read handshake message from
	len - data length for the current ssl record.  The ssl record
		can contain multiple handshake messages, so we may need to parse
		them all here.
	Return:
		SSL_SUCCESS
		SSL_PROCESS_DATA
		SSL_ERROR - see ssl->err for details
*/
static int32 parseSSLHandshake(ssl_t *ssl, char *inbuf, int32 len)
{
	unsigned char	*c;
	unsigned char	*end;
	unsigned char	hsType;
	int32			i, hsLen, rc, parseLen = 0; 
	uint32			cipher = 0;
	unsigned char	hsMsgHash[SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE];

#ifdef USE_SERVER_SIDE_SSL
	unsigned char	*p;
	int32			suiteLen, challengeLen, pubKeyLen, extLen;
#endif /* USE_SERVER_SIDE_SSL */

#ifdef USE_CLIENT_SIDE_SSL
	int32			sessionIdLen, certMatch, certTypeLen;
	sslCert_t		*subjectCert;
	int32			valid, certLen, certChainLen, anonCheck;
	sslCert_t		*cert, *currentCert;
#endif /* USE_CLIENT_SIDE_SSL */

	rc = SSL_SUCCESS;
	c = (unsigned char*)inbuf;
	end = (unsigned char*)(inbuf + len);


parseHandshake:
	if (end - c < 1) {
		ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
		matrixStrDebugMsg("Invalid length of handshake message\n", NULL);
		return SSL_ERROR;
	}
	hsType = *c; c++;

#ifndef ALLOW_SERVER_REHANDSHAKES
/*
	Disables server renegotiation.  This is in response to the HTTPS
	flaws discovered by Marsh Ray in which a man-in-the-middle may take
	advantage of the "authentication gap" in the SSL renegotiation protocol	
*/
	if (hsType == SSL_HS_CLIENT_HELLO && ssl->hsState == SSL_HS_DONE) {
		ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
		matrixStrDebugMsg("Server rehandshaking is disabled\n", NULL);
		return SSL_ERROR;
	}
#endif /* ALLOW_SERVER_REHANDSHAKES */

/*
	hsType is the received handshake type and ssl->hsState is the expected
	handshake type.  If it doesn't match, there are some possible cases
	that are not errors.  These are checked here. 
*/
	if (hsType != ssl->hsState && 
			(hsType != SSL_HS_CLIENT_HELLO || ssl->hsState != SSL_HS_DONE)) {

/*
		A mismatch is possible in the client authentication case.
		The optional CERTIFICATE_REQUEST may be appearing instead of 
		SERVER_HELLO_DONE.
*/
		if ((hsType == SSL_HS_CERTIFICATE_REQUEST) &&
				(ssl->hsState == SSL_HS_SERVER_HELLO_DONE)) {
/*
			This is where the client is first aware of requested client
			authentication so we set the flag here.
*/
			ssl->flags |= SSL_FLAGS_CLIENT_AUTH;
			ssl->hsState = SSL_HS_CERTIFICATE_REQUEST;
			goto hsStateDetermined;
		}
/*
		Another possible mismatch allowed is for a HELLO_REQEST message.
		Indicates a rehandshake initiated from the server.
*/
		if ((hsType == SSL_HS_HELLO_REQUEST) &&
				(ssl->hsState == SSL_HS_DONE) &&
				!(ssl->flags & SSL_FLAGS_SERVER)) {
			sslResetContext(ssl);
			ssl->hsState = hsType;
			goto hsStateDetermined;
		}



		ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
		matrixIntDebugMsg("Invalid type of handshake message: %d\n", hsType);
		return SSL_ERROR;
	}
	
hsStateDetermined:	
	if (hsType == SSL_HS_CLIENT_HELLO) { 
		sslInitHSHash(ssl);
		if (ssl->hsState == SSL_HS_DONE) {
/*
			Rehandshake. Server receiving client hello on existing connection
*/
			sslResetContext(ssl);
			ssl->hsState = hsType;
		}
	}

/*
	We need to get a copy of the message hashes to compare to those sent
	in the finished message (which does not include a hash of itself)
	before we update the handshake hashes
*/
	if (ssl->hsState == SSL_HS_FINISHED) {
		sslSnapshotHSHash(ssl, hsMsgHash, 
			(ssl->flags & SSL_FLAGS_SERVER) ? 0 : SSL_FLAGS_SERVER);
	}

/*
	Process the handshake header and update the ongoing handshake hash
	SSLv3:
		1 byte type
		3 bytes length
	SSLv2:
		1 byte type
*/
	if (ssl->rec.majVer >= SSL3_MAJ_VER) {
		if (end - c < 3) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid length of handshake message\n", NULL);
			return SSL_ERROR;
		}
		hsLen = *c << 16; c++;
		hsLen += *c << 8; c++;
		hsLen += *c; c++;
		if (end - c < hsLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid handshake length\n", NULL);
			return SSL_ERROR;
		}
			sslUpdateHSHash(ssl, c - ssl->hshakeHeadLen,
				hsLen + ssl->hshakeHeadLen);

	} else if (ssl->rec.majVer == SSL2_MAJ_VER) {
/*
		Assume that the handshake len is the same as the incoming ssl record
		length minus 1 byte (type), this is verified in SSL_HS_CLIENT_HELLO
*/
		hsLen = len - 1;
		sslUpdateHSHash(ssl, (unsigned char*)inbuf, len);
	} else {
		ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
		matrixIntDebugMsg("Invalid record version: %d\n", ssl->rec.majVer);
		return SSL_ERROR;
	}
/*
	Finished with header.  Process each type of handshake message.
*/
	switch(ssl->hsState) {

#ifdef USE_SERVER_SIDE_SSL
	case SSL_HS_CLIENT_HELLO:
/*
		First two bytes are the highest supported major and minor SSL versions
		We support only 3.0 (support 3.1 in commercial version)
*/
		if (end - c < 2) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid ssl header version length\n", NULL);
			return SSL_ERROR;
		}
		ssl->reqMajVer = *c; c++;
		ssl->reqMinVer = *c; c++;
		if (ssl->reqMajVer >= SSL3_MAJ_VER) {
			ssl->majVer = ssl->reqMajVer;
			ssl->minVer = SSL3_MIN_VER;

		} else {
			ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
			matrixIntDebugMsg("Unsupported ssl version: %d\n", ssl->reqMajVer);
			return SSL_ERROR;
		}
/*
		Support SSLv3 and SSLv2 ClientHello messages.  Browsers usually send v2
		messages for compatibility
*/
		if (ssl->rec.majVer > SSL2_MAJ_VER) {
/*
			Next is a 32 bytes of random data for key generation
			and a single byte with the session ID length
*/
			if (end - c < SSL_HS_RANDOM_SIZE + 1) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid length of random data\n", NULL);
				return SSL_ERROR;
			}
			memcpy(ssl->sec.clientRandom, c, SSL_HS_RANDOM_SIZE);
			c += SSL_HS_RANDOM_SIZE;
			ssl->sessionIdLen = *c; c++;
/*
			If a session length was specified, the client is asking to
			resume a previously established session to speed up the handshake.
*/
			if (ssl->sessionIdLen > 0) {
				if (ssl->sessionIdLen > SSL_MAX_SESSION_ID_SIZE || 
						end - c < ssl->sessionIdLen) {
					ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
					return SSL_ERROR;
				}
				memcpy(ssl->sessionId, c, ssl->sessionIdLen);
				c += ssl->sessionIdLen;
/*
				Look up the session id for ssl session resumption.  If found, we
				load the pre-negotiated masterSecret and cipher.
				A resumed request must meet the following restrictions:
					The id must be present in the lookup table
					The requested version must match the original version
					The cipher suite list must contain the original cipher suite
*/
				if (matrixResumeSession(ssl) >= 0) {
					ssl->flags &= ~SSL_FLAGS_CLIENT_AUTH;
					ssl->flags |= SSL_FLAGS_RESUMED;
				} else {
					memset(ssl->sessionId, 0, SSL_MAX_SESSION_ID_SIZE);
					ssl->sessionIdLen = 0;
				}
			} else {
/*
				Always clear the RESUMED flag if no client session id specified
*/
				ssl->flags &= ~SSL_FLAGS_RESUMED;
			}
/*
			Next is the two byte cipher suite list length, network byte order.  
			It must not be zero, and must be a multiple of two.
*/
			if (end - c < 2) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid cipher suite list length\n", NULL);
				return SSL_ERROR;
			}
			suiteLen = *c << 8; c++;
			suiteLen += *c; c++;
			if (suiteLen == 0 || suiteLen & 1) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixIntDebugMsg("Unable to parse cipher suite list: %d\n", 
					suiteLen);
				return SSL_ERROR;
			}
/*
			Now is 'suiteLen' bytes of the supported cipher suite list,
			listed in order of preference.  Loop through and find the 
			first cipher suite we support.
*/
			if (end - c < suiteLen) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				return SSL_ERROR;
			}
			p = c + suiteLen;
			while (c < p) {
				cipher = *c << 8; c++;
				cipher += *c; c++;
/*
				A resumed session can only match the cipher originally 
				negotiated. Otherwise, match the first cipher that we support
*/
				if (ssl->flags & SSL_FLAGS_RESUMED) {
					sslAssert(ssl->cipher);
					if (ssl->cipher->id == cipher) {
						c = p;
						break;
					}
				} else {
					if ((ssl->cipher = sslGetCipherSpec(cipher)) != NULL) {
						c = p;
						break;
					}
				}
			}
/*
			If we fell to the default cipher suite, we didn't have
			any in common with the client, or the client is being bad
			and requesting the null cipher!
*/
			if (ssl->cipher == NULL || ssl->cipher->id != cipher || 
					cipher == SSL_NULL_WITH_NULL_NULL) {
				matrixStrDebugMsg("Can't support requested cipher\n", NULL);
				ssl->cipher = sslGetCipherSpec(SSL_NULL_WITH_NULL_NULL);
				ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
				return SSL_ERROR;
			}

/*
			Bypass the compression parameters.  Only supporting mandatory NULL
*/
			if (end - c < 1) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid compression header length\n", NULL);
				return SSL_ERROR;
			}
			extLen = *c++;
			if (end - c < extLen) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid compression header length\n", NULL);
				return SSL_ERROR;
			}
			c += extLen;
		} else {
/*
			Parse a SSLv2 ClientHello message.  The same information is 
			conveyed but the order and format is different.
			First get the cipher suite length, session id length and challenge
			(client random) length - all two byte values, network byte order.
*/
			if (end - c < 6) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Can't parse hello message\n", NULL);
				return SSL_ERROR;
			}
			suiteLen = *c << 8; c++;
			suiteLen += *c; c++;
			if (suiteLen == 0 || suiteLen % 3 != 0) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Can't parse hello message\n", NULL);
				return SSL_ERROR;
			}
			ssl->sessionIdLen = *c << 8; c++;
			ssl->sessionIdLen += *c; c++;
/*
			A resumed session would use a SSLv3 ClientHello, not SSLv2.
*/
			if (ssl->sessionIdLen != 0) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Bad resumption request\n", NULL);
				return SSL_ERROR;
			}
			challengeLen = *c << 8; c++;
			challengeLen += *c; c++;
			if (challengeLen < 16 || challengeLen > 32) {
				matrixStrDebugMsg("Bad challenge length\n", NULL);
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				return SSL_ERROR;
			}
/*
			Validate the three lengths that were just sent to us, don't
			want any buffer overflows while parsing the remaining data
*/
			if (end - c != suiteLen + ssl->sessionIdLen + challengeLen) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				return SSL_ERROR;
			}
/*
			Parse the cipher suite list similar to the SSLv3 method, except
			each suite is 3 bytes, instead of two bytes.  We define the suite
			as an integer value, so either method works for lookup.
			We don't support session resumption from V2 handshakes, so don't 
			need to worry about matching resumed cipher suite.
*/
			p = c + suiteLen;
			while (c < p) {
				cipher = *c << 16; c++;
				cipher += *c << 8; c++;
				cipher += *c; c++;
				if ((ssl->cipher = sslGetCipherSpec(cipher)) != NULL) {
					c = p;
					break;
				}
			}
			if (ssl->cipher == NULL || 
					ssl->cipher->id == SSL_NULL_WITH_NULL_NULL) {
				ssl->cipher = sslGetCipherSpec(SSL_NULL_WITH_NULL_NULL);
				ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
				matrixStrDebugMsg("Can't support requested cipher\n", NULL);
				return SSL_ERROR;
			}
/*
			We don't allow session IDs for v2 ClientHellos
*/
			if (ssl->sessionIdLen > 0) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("SSLv2 sessions not allowed\n", NULL);
				return SSL_ERROR;
			}
/*
			The client random (between 16 and 32 bytes) fills the least 
			significant bytes in the (always) 32 byte SSLv3 client random field.
*/
			memset(ssl->sec.clientRandom, 0x0, SSL_HS_RANDOM_SIZE);
			memcpy(ssl->sec.clientRandom + (SSL_HS_RANDOM_SIZE - challengeLen), 
				c, challengeLen);
			c += challengeLen;
		}
/*
		ClientHello should be the only one in the record.
*/
		if (c != end) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid final client hello length\n", NULL);
			return SSL_ERROR;
		}


/*
		If we're resuming a handshake, then the next handshake message we
		expect is the finished message.  Otherwise we do the full handshake.
*/
		if (ssl->flags & SSL_FLAGS_RESUMED) {
			ssl->hsState = SSL_HS_FINISHED;
		} else {
			ssl->hsState = SSL_HS_CLIENT_KEY_EXCHANGE;
		}
/*
		Now that we've parsed the ClientHello, we need to tell the caller that
		we have a handshake response to write out.
		The caller should call sslWrite upon receiving this return code.
*/
		rc = SSL_PROCESS_DATA;
		break;

	case SSL_HS_CLIENT_KEY_EXCHANGE:
/*
		RSA: This message contains the premaster secret encrypted with the 
		server's public key (from the Certificate).  The premaster
		secret is 48 bytes of random data, but the message may be longer
		than that because the 48 bytes are padded before encryption 
		according to PKCS#1v1.5.  After encryption, we should have the 
		correct length.
*/
		if (end - c < hsLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid ClientKeyExchange length\n", NULL);
			return SSL_ERROR;
		}
		sslActivatePublicCipher(ssl);

		pubKeyLen = hsLen;


/*
				Now have a handshake pool to allocate the premaster storage
*/
				ssl->sec.premasterSize = SSL_HS_RSA_PREMASTER_SIZE;
				ssl->sec.premaster = psMalloc(ssl->hsPool,
					SSL_HS_RSA_PREMASTER_SIZE);

				if (ssl->decryptPriv(ssl->hsPool, ssl->keys->cert.privKey, c,
						pubKeyLen, ssl->sec.premaster, ssl->sec.premasterSize)
						!= ssl->sec.premasterSize) {
					ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
					matrixStrDebugMsg("Failed to decrypt premaster\n", NULL);
					return SSL_ERROR;
				}

/*
				The first two bytes of the decrypted message should be the
				client's requested version number (which may not be the same
				as the final negotiated version). The other 46 bytes -
				pure random!
			
				SECURITY - 
				Some SSL clients (Including Microsoft IE 6.0) incorrectly set
				the first two bytes to the negotiated version rather than the
				requested version.  This is known in OpenSSL as the
				SSL_OP_TLS_ROLLBACK_BUG. We allow this to slide only if we
				don't support TLS, TLS was requested and the negotiated
				versions match.
*/
				if (*ssl->sec.premaster != ssl->reqMajVer) {
					ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
					matrixStrDebugMsg("Incorrect version in ClientKeyExchange\n",
						NULL);
					return SSL_ERROR;
				}
				if (*(ssl->sec.premaster + 1) != ssl->reqMinVer) {
					if (ssl->reqMinVer < TLS_MIN_VER ||
							*(ssl->sec.premaster + 1) != ssl->minVer) {
						ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
						matrixStrDebugMsg("Incorrect version in ClientKeyExchange\n",
							NULL);
						return SSL_ERROR;
					}
				}

/*
		Now that we've got the premaster secret, derive the various
		symmetric keys using it and the client and server random values.
		Update the cached session (if found) with the masterSecret and
		negotiated cipher.	
*/
		sslDeriveKeys(ssl);

		matrixUpdateSession(ssl);

		c += pubKeyLen;
		ssl->hsState = SSL_HS_FINISHED;


		break;
#endif /* USE_SERVER_SIDE_SSL */

	case SSL_HS_FINISHED:
/*
		Before the finished handshake message, we should have seen the
		CHANGE_CIPHER_SPEC message come through in the record layer, which
		would have activated the read cipher, and set the READ_SECURE flag.
		This is the first handshake message that was sent securely.
*/
		if (!(ssl->flags & SSL_FLAGS_READ_SECURE)) {
			ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
			matrixStrDebugMsg("Finished before ChangeCipherSpec\n", NULL);
			return SSL_ERROR;
		}
/*
		The contents of the finished message is a 16 byte MD5 hash followed
		by a 20 byte sha1 hash of all the handshake messages so far, to verify
		that nothing has been tampered with while we were still insecure.
		Compare the message to the value we calculated at the beginning of
		this function.
*/
			if (hsLen != SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid Finished length\n", NULL);
				return SSL_ERROR;
			}
		if (end - c < hsLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Finished length\n", NULL);
			return SSL_ERROR;
		}
		if (memcmp(c, hsMsgHash, hsLen) != 0) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid handshake msg hash\n", NULL);
			return SSL_ERROR;
		}
		c += hsLen;
		ssl->hsState = SSL_HS_DONE;
/*
		Now that we've parsed the Finished message, if we're a resumed 
		connection, we're done with handshaking, otherwise, we return
		SSL_PROCESS_DATA to get our own cipher spec and finished messages
		sent out by the caller.
*/
		if (ssl->flags & SSL_FLAGS_SERVER) {
			if (!(ssl->flags & SSL_FLAGS_RESUMED)) {
				rc = SSL_PROCESS_DATA;
			}
		} else {
			if (ssl->flags & SSL_FLAGS_RESUMED) {
				rc = SSL_PROCESS_DATA;
			}
		}
#ifdef USE_CLIENT_SIDE_SSL
/*
		Free handshake pool, of which the cert is the primary member.
		There is also an attempt to free the handshake pool during
		the sending of the finished message to deal with client
		and server and differing handshake types.  Both cases are 
		attempted keep the lifespan of this pool as short as possible.
		This is the default case for the server side.
*/
		if (ssl->sec.cert) {
			matrixX509FreeCert(ssl->sec.cert);
			ssl->sec.cert = NULL;
		}
#endif /* USE_CLIENT_SIDE */
		break;

#ifdef USE_CLIENT_SIDE_SSL
	case SSL_HS_HELLO_REQUEST:
/*	
		No body message and the only one in record flight
*/
		if (end - c != 0) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid hello request message\n", NULL);
			return SSL_ERROR;
		}
/*
		Intentionally not changing state here to SERVER_HELLO.  The
		encodeResponse case	this will fall into needs to distinguish
		between calling the normal sslEncodeResponse or encodeClientHello.
		The HELLO_REQUEST state is used to make that determination and the
		writing of CLIENT_HELLO will properly move the state along itself.
*/
		rc = SSL_PROCESS_DATA;
		break;

	case SSL_HS_SERVER_HELLO: 
/*
		First two bytes are the negotiated SSL version
		We support only 3.0 (other options are 2.0 or 3.1)
*/
		if (end - c < 2) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid ssl header version length\n", NULL);
			return SSL_ERROR;
		}
		ssl->reqMajVer = *c; c++;
		ssl->reqMinVer = *c; c++;
		if (ssl->reqMajVer != ssl->majVer) {
			ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
			matrixIntDebugMsg("Unsupported ssl version: %d\n", ssl->reqMajVer);
			return SSL_ERROR;
		}

/*
		Next is a 32 bytes of random data for key generation
		and a single byte with the session ID length
*/
		if (end - c < SSL_HS_RANDOM_SIZE + 1) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid length of random data\n", NULL);
			return SSL_ERROR;
		}
		memcpy(ssl->sec.serverRandom, c, SSL_HS_RANDOM_SIZE);
		c += SSL_HS_RANDOM_SIZE;
		sessionIdLen = *c; c++;
		if (sessionIdLen > SSL_MAX_SESSION_ID_SIZE || 
				end - c < sessionIdLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			return SSL_ERROR;
		}
/*
		If a session length was specified, the server has sent us a
		session Id.  We may have requested a specific session, and the
		server may or may not agree to use that session.
*/
		if (sessionIdLen > 0) {
			if (ssl->sessionIdLen > 0) {
				if (memcmp(ssl->sessionId, c, sessionIdLen) == 0) {
					ssl->flags |= SSL_FLAGS_RESUMED;
				} else {
					ssl->cipher = sslGetCipherSpec(SSL_NULL_WITH_NULL_NULL);
					memset(ssl->sec.masterSecret, 0x0, SSL_HS_MASTER_SIZE);
					ssl->sessionIdLen = sessionIdLen;
					memcpy(ssl->sessionId, c, sessionIdLen);
					ssl->flags &= ~SSL_FLAGS_RESUMED;
				}
			} else {
				ssl->sessionIdLen = sessionIdLen;
				memcpy(ssl->sessionId, c, sessionIdLen);
			}
			c += sessionIdLen;
		} else {
			if (ssl->sessionIdLen > 0) {
				ssl->cipher = sslGetCipherSpec(SSL_NULL_WITH_NULL_NULL);
				memset(ssl->sec.masterSecret, 0x0, SSL_HS_MASTER_SIZE);
				ssl->sessionIdLen = 0;
				memset(ssl->sessionId, 0x0, SSL_MAX_SESSION_ID_SIZE);
				ssl->flags &= ~SSL_FLAGS_RESUMED;
			}
		}
/*
		Next is the two byte cipher suite
*/
		if (end - c < 2) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid cipher suite length\n", NULL);
			return SSL_ERROR;
		}
		cipher = *c << 8; c++;
		cipher += *c; c++;

/*
		A resumed session can only match the cipher originally 
		negotiated. Otherwise, match the first cipher that we support
*/
		if (ssl->flags & SSL_FLAGS_RESUMED) {
			sslAssert(ssl->cipher);
			if (ssl->cipher->id != cipher) {
				ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
				matrixStrDebugMsg("Can't support resumed cipher\n", NULL);
				return SSL_ERROR;
			}
		} else {
			if ((ssl->cipher = sslGetCipherSpec(cipher)) == NULL) {
				ssl->err = SSL_ALERT_HANDSHAKE_FAILURE;
				matrixStrDebugMsg("Can't support requested cipher\n", NULL);
				return SSL_ERROR;
			}
		}


/*
		Decode the compression parameters.  Always zero.
		There are no compression schemes defined for SSLv3
*/
		if (end - c < 1 || *c != 0) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid compression value\n", NULL);
			return SSL_ERROR;
		}
/*
		At this point, if we're resumed, we have all the required info
		to derive keys.  The next handshake message we expect is
		the Finished message.
*/
		c++;
		
		if (ssl->flags & SSL_FLAGS_RESUMED) {
			sslDeriveKeys(ssl);
			ssl->hsState = SSL_HS_FINISHED;
		} else {
			ssl->hsState = SSL_HS_CERTIFICATE;
		}
		break;

	case SSL_HS_CERTIFICATE: 
		if (end - c < 3) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Certificate message\n", NULL);
			return SSL_ERROR;
		}
		certChainLen = *c << 16; c++;
		certChainLen |= *c << 8; c++;
		certChainLen |= *c; c++;
		if (certChainLen == 0) {
			if (ssl->majVer == SSL3_MAJ_VER && ssl->minVer == SSL3_MIN_VER) {
				ssl->err = SSL_ALERT_NO_CERTIFICATE;
			} else {
				ssl->err = SSL_ALERT_BAD_CERTIFICATE;
			}
			matrixStrDebugMsg("No certificate sent to verify\n", NULL);
			return SSL_ERROR;
		}
		if (end - c < 3) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Certificate message\n", NULL);
			return SSL_ERROR;
		}

		i = 0;
		while (certChainLen > 0) {
			certLen = *c << 16; c++;
			certLen |= *c << 8; c++;
			certLen |= *c; c++;

			if (end - c < certLen) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid certificate length\n", NULL);
				return SSL_ERROR;
			}
/*
			Extract the binary cert message into the cert structure
*/
			if ((parseLen = matrixX509ParseCert(ssl->hsPool, c, certLen, &cert)) < 0) {
				matrixX509FreeCert(cert);
				ssl->err = SSL_ALERT_BAD_CERTIFICATE;
				matrixStrDebugMsg("Can't parse certificate\n", NULL);
				return SSL_ERROR;
			}
			c += parseLen;

			if (i++ == 0) {
				ssl->sec.cert = cert;
				currentCert = ssl->sec.cert;
			} else {
				currentCert->next = cert;
				currentCert = currentCert->next;
			}
			certChainLen -= (certLen + 3);
		}
/*
		May have received a chain of certs in the message.  Spec says they
		must be in order so that each subsequent one is the parent of the
		previous.  Confirm this now.
*/
		if (matrixX509ValidateCertChain(ssl->hsPool, ssl->sec.cert,
				&subjectCert, &valid) < 0) {
			ssl->err = SSL_ALERT_BAD_CERTIFICATE;
			matrixStrDebugMsg("Couldn't validate certificate chain\n", NULL);
			return SSL_ERROR;	
		}
/*
		There may not be a caCert set.  The validate implemenation will just
		take the subject cert and make sure it is a self signed cert.
*/
		if (matrixX509ValidateCert(ssl->hsPool, subjectCert, 
				ssl->keys == NULL ? NULL : ssl->keys->caCerts,
				&subjectCert->valid) < 0) {
			ssl->err = SSL_ALERT_BAD_CERTIFICATE;
			matrixStrDebugMsg("Error validating certificate\n", NULL);
			return SSL_ERROR;
		}
		if (subjectCert->valid < 0) {
			matrixStrDebugMsg(
				"Warning: Cert did not pass default validation checks\n", NULL);
/*
			If there is no user callback, fail on validation check because there
			will be no intervention to give it a second look.
*/
			if (ssl->sec.validateCert == NULL) {
				ssl->err = SSL_ALERT_BAD_CERTIFICATE;
				return SSL_ERROR;
			}
		}
/*
		Call the user validation function with the entire cert chain.  The user
		will proabably want to drill down to the last cert to make sure it
		has been properly validated by a CA on this side.

		Need to return from user validation space with knowledge
		that this is an ANONYMOUS connection.
*/
		if ((anonCheck = matrixX509UserValidator(ssl->hsPool, ssl->sec.cert, 
				ssl->sec.validateCert, ssl->sec.validateCertArg)) < 0) {
			ssl->err = SSL_ALERT_BAD_CERTIFICATE;
			return SSL_ERROR;
		}
/*
		Set the flag that is checked by the matrixSslGetAnonStatus API
*/
		if (anonCheck == SSL_ALLOW_ANON_CONNECTION) {
			ssl->sec.anon = 1;
		} else {
			ssl->sec.anon = 0;
		}

/*
		Either a client or server could have been processing the cert as part of
		the authentication process.  If server, we move to the client key
		exchange state.
*/
		if (ssl->flags & SSL_FLAGS_SERVER) {
			ssl->hsState = SSL_HS_CLIENT_KEY_EXCHANGE;
		} else {
			ssl->hsState = SSL_HS_SERVER_HELLO_DONE;
		}
		break;

	case SSL_HS_SERVER_HELLO_DONE: 
		if (hsLen != 0) {
			ssl->err = SSL_ALERT_BAD_CERTIFICATE;
			matrixStrDebugMsg("Can't validate certificate\n", NULL);
			return SSL_ERROR;
		}
		ssl->hsState = SSL_HS_FINISHED;
		rc = SSL_PROCESS_DATA;
		break;

	case SSL_HS_CERTIFICATE_REQUEST: 
		if (hsLen < 4) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Certificate Request message\n", NULL);
			return SSL_ERROR;
		}
/*
		We only have RSA_SIGN types.  Make sure server can accept them
*/
		certMatch = 0;
		certTypeLen = *c++;
		if (end - c < certTypeLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Certificate Request message\n", NULL);
			return SSL_ERROR;
		}
		while (certTypeLen-- > 0) {
			if (*c++ == RSA_SIGN) {
				certMatch = 1;
			}
		}
		if (certMatch == 0) {
			ssl->err = SSL_ALERT_UNSUPPORTED_CERTIFICATE;
			matrixStrDebugMsg("Can only support RSA_SIGN cert authentication\n",
				NULL);
			return SSL_ERROR;
		}
		certChainLen = *c << 8; c++;
		certChainLen |= *c; c++;
        if (end - c < certChainLen) {
			ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
			matrixStrDebugMsg("Invalid Certificate Request message\n", NULL);
			return SSL_ERROR;
		}
/*
		Check the passed in DNs against our cert issuer to see if they match.
		Only supporting a single cert on the client side.
*/
		ssl->sec.certMatch = 0;
		while (certChainLen > 0) {
			certLen = *c << 8; c++;
			certLen |= *c; c++;
			if (end - c < certLen) {
				ssl->err = SSL_ALERT_ILLEGAL_PARAMETER;
				matrixStrDebugMsg("Invalid CertificateRequest message\n", NULL);
				return SSL_ERROR;
			}
			c += certLen;
			certChainLen -= (2 + certLen);
		}
		ssl->hsState = SSL_HS_SERVER_HELLO_DONE;
		break;
#endif /* USE_CLIENT_SIDE_SSL */

	case SSL_HS_SERVER_KEY_EXCHANGE:
		ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
		return SSL_ERROR;

	default:
		ssl->err = SSL_ALERT_UNEXPECTED_MESSAGE;
		return SSL_ERROR;
	}
	
	
/*
	if we've got more data in the record, the sender has packed
	multiple handshake messages in one record.  Parse the next one.
*/
	if (c < end) {
		goto parseHandshake;
	}
	return rc;
}

/******************************************************************************/
