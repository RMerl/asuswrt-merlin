/*
 *	sslEncode.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Secure Sockets Layer message encoding
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

static int32 writeCertificate(ssl_t *ssl, sslBuf_t *out, int32 notEmpty);
static int32 writeChangeCipherSpec(ssl_t *ssl, sslBuf_t *out);
static int32 writeFinished(ssl_t *ssl, sslBuf_t *out);
static int32 writeAlert(ssl_t *ssl, unsigned char level, 
						unsigned char description, sslBuf_t *out);
static int32 writeRecordHeader(ssl_t *ssl, int32 type, int32 hsType, int32 *messageSize,
						char *padLen, unsigned char **encryptStart,
						unsigned char **end, unsigned char **c);

static int32 encryptRecord(ssl_t *ssl, int32 type, int32 messageSize, int32 padLen, 
						unsigned char *encryptStart, sslBuf_t *out,
						unsigned char **c);

#ifdef USE_CLIENT_SIDE_SSL
static int32 writeClientKeyExchange(ssl_t *ssl, sslBuf_t *out);
#endif /* USE_CLIENT_SIDE_SSL */

#ifdef USE_SERVER_SIDE_SSL
static int32 writeServerHello(ssl_t *ssl, sslBuf_t *out);
static int32 writeServerHelloDone(ssl_t *ssl, sslBuf_t *out);
#endif /* USE_SERVER_SIDE_SSL */


static int32 secureWriteAdditions(ssl_t *ssl, int32 numRecs);

/******************************************************************************/
/*
	Encode the incoming data into the out buffer for sending to remote peer.

	FUTURE SECURITY - If sending the first application data record, we could
	prepend it with a blank SSL record to avoid a timing attack.  We're fine
	for now, because this is an impractical attack and the solution is 
	incompatible with some SSL implementations (including some versions of IE).
	http://www.openssl.org/~bodo/tls-cbc.txt
*/
int32 matrixSslEncode(ssl_t *ssl, unsigned char *in, int32 inlen, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32			messageSize, rc;
/*
	If we've had a protocol error, don't allow further use of the session
	Also, don't allow a application data record to be encoded unless the
	handshake is complete.
*/
	if (ssl->flags & SSL_FLAGS_ERROR || ssl->hsState != SSL_HS_DONE ||
			ssl->flags & SSL_FLAGS_CLOSED) {
		return SSL_ERROR;
	}
	c = out->end;
	end = out->buf + out->size;
	messageSize = ssl->recordHeadLen + inlen;

/*
	Validate size constraint
*/
	if (messageSize > SSL_MAX_BUF_SIZE) {
		return SSL_ERROR;
	}

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_APPLICATION_DATA, 0,
			&messageSize, &padLen, &encryptStart, &end, &c)) < 0) {
		return rc;
	}

	memcpy(c, in, inlen);
	c += inlen;

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_APPLICATION_DATA,
			messageSize, padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}
	out->end = c;

	return (int32)(out->end - out->start);
}

/******************************************************************************/
/*
	We indicate to the caller through return codes in sslDecode when we need
	to write internal data to the remote host.  The caller will call this 
	function to generate a message appropriate to our state.
*/
int32 sslEncodeResponse(ssl_t *ssl, sslBuf_t *out)
{
	int32			messageSize;
	int32			rc = SSL_ERROR;
#ifdef USE_SERVER_SIDE_SSL
	int32			totalCertLen, i;
	sslLocalCert_t	*cert;
#endif /* USE_SERVER_SIDE_SSL */
#ifdef USE_CLIENT_SIDE_SSL
	int32			ckeSize;
#endif /* USE_CLIENT_SIDE_SSL */

/*
	We may be trying to encode an alert response if there is an error marked
	on the connection.
*/
	if (ssl->err != SSL_ALERT_NONE) {
		rc = writeAlert(ssl, SSL_ALERT_LEVEL_FATAL, ssl->err, out);
		if (rc == SSL_ERROR) {
			ssl->flags |= SSL_FLAGS_ERROR;
		}
		return rc;
	}

	
/*
	We encode a set of response messages based on our current state
	We have to pre-verify the size of the outgoing buffer against
	all the messages to make the routine transactional.  If the first
	write succeeds and the second fails because of size, we cannot
	rollback the state of the cipher and MAC.
*/
	switch (ssl->hsState) {
/*
	If we're waiting for the ClientKeyExchange message, then we need to
	send the messages that would prompt that result on the client
*/
#ifdef USE_SERVER_SIDE_SSL
	case SSL_HS_CLIENT_KEY_EXCHANGE:

/*
			This is the entry point for a server encoding the first flight
			of a non-DH, non-client-auth handshake.
*/
			totalCertLen = 0;
				cert = &ssl->keys->cert;
				for (i = 0; cert != NULL; i++) {
					totalCertLen += cert->certLen;
					cert = cert->next;
				}
				messageSize =
					3 * ssl->recordHeadLen +
					3 * ssl->hshakeHeadLen +
					38 + SSL_MAX_SESSION_ID_SIZE +  /* server hello */
					3 + (i * 3) + totalCertLen; /* certificate */


			messageSize += secureWriteAdditions(ssl, 3);

		if ((out->buf + out->size) - out->end < messageSize) {
			return SSL_FULL;
		}
/*
		Message size complete.  Begin the flight write
*/
		rc = writeServerHello(ssl, out);

		if (rc == SSL_SUCCESS) {
			rc = writeCertificate(ssl, out, 1);
		}

		if (rc == SSL_SUCCESS) {
			rc = writeServerHelloDone(ssl, out);
		}
		break;

#endif /* USE_SERVER_SIDE_SSL */

/*
	If we're not waiting for any message from client, then we need to
	send our finished message
*/
	case SSL_HS_DONE:
		messageSize = 2 * ssl->recordHeadLen +
			ssl->hshakeHeadLen +
			1 + /* change cipher spec */
			SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE; /* finished */
/*
		Account for possible overhead in CCS message with secureWriteAdditions
		then always account for the encrypted FINISHED message
*/
		messageSize += secureWriteAdditions(ssl, 1);
		messageSize += ssl->enMacSize + /* handshake msg hash */
			(ssl->cipher->blockSize - 1); /* max padding */
			
		if ((out->buf + out->size) - out->end < messageSize) {
			return SSL_FULL;
		}
		rc = writeChangeCipherSpec(ssl, out);
		if (rc == SSL_SUCCESS) {
			rc = writeFinished(ssl, out);
		}
		break;
/*
	If we're expecting a Finished message, as a server we're doing 
	session resumption.  As a client, we're completing a normal
	handshake
*/
	case SSL_HS_FINISHED:
#ifdef USE_SERVER_SIDE_SSL
		if (ssl->flags & SSL_FLAGS_SERVER) {
			messageSize =
				3 * ssl->recordHeadLen +
				2 * ssl->hshakeHeadLen +
				38 + SSL_MAX_SESSION_ID_SIZE + /* server hello */
				1 + /* change cipher spec */
				SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE; /* finished */
/*
			Account for possible overhead with secureWriteAdditions
			then always account for the encrypted FINISHED message
*/				
			messageSize += secureWriteAdditions(ssl, 2);
			messageSize += ssl->enMacSize + /* handshake msg hash */
				(ssl->cipher->blockSize - 1); /* max padding */
			if ((out->buf + out->size) - out->end < messageSize) {
				return SSL_FULL;
			}
			rc = writeServerHello(ssl, out);
			if (rc == SSL_SUCCESS) {
				rc = writeChangeCipherSpec(ssl, out);
			}
			if (rc == SSL_SUCCESS) {
				rc = writeFinished(ssl, out);
			}
		}
#endif /* USE_SERVER_SIDE_SSL */
#ifdef USE_CLIENT_SIDE_SSL
/*
		Encode entry point for client side final flight encodes.
		First task here is to find out size of ClientKeyExchange message
*/
		if (!(ssl->flags & SSL_FLAGS_SERVER)) {
			ckeSize = 0;
/*
					Normal RSA auth cipher suite case
*/
					if (ssl->sec.cert == NULL) {
						ssl->flags |= SSL_FLAGS_ERROR;
						return SSL_ERROR;
					}
					ckeSize = ssl->sec.cert->publicKey.size;

			messageSize = 0;
/*
			Client authentication requires the client to send a CERTIFICATE
			and CERTIFICATE_VERIFY message.  Account for the length.  It
			is possible the client didn't have a match for the requested cert.
			Send an empty certificate message in that case (or alert for SSLv3)
*/
			if (ssl->flags & SSL_FLAGS_CLIENT_AUTH) {
				if (ssl->sec.certMatch > 0) {
/*
					Account for the certificate and certificateVerify messages
*/
					messageSize += 6 + (2 * ssl->recordHeadLen) +
						(2 * ssl->hshakeHeadLen) + ssl->keys->cert.certLen +
						2 +	ssl->keys->cert.privKey->size;
				} else {
/*
					SSLv3 sends a no_certificate warning alert for no match
*/
					if (ssl->majVer == SSL3_MAJ_VER
							&& ssl->minVer == SSL3_MIN_VER) {
						messageSize += 2 + ssl->recordHeadLen;
					} else {
/*
						TLS just sends an empty certificate message
*/
						messageSize += 3 + ssl->recordHeadLen +
							ssl->hshakeHeadLen;
					}
				}
			}
/*
			Account for the header and message size for all records.  The
			finished message will always be encrypted, so account for one
			largest possible MAC size and block size.  Minus one
			for padding.  The finished message is not accounted for in the
			writeSecureAddition calls below since it is accounted for here.
*/
			messageSize +=
				3 * ssl->recordHeadLen +
				2 * ssl->hshakeHeadLen + /* change cipher has no hsHead */
				ckeSize + /* client key exchange */
				1 + /* change cipher spec */
				SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE + /* SSLv3 finished */
				SSL_MAX_MAC_SIZE + SSL_MAX_BLOCK_SIZE - 1;
			if (ssl->flags & SSL_FLAGS_CLIENT_AUTH) {
/*
				Secure write for ClientKeyExchange, ChangeCipherSpec,
				Certificate, and CertificateVerify.  Don't account for
				Certificate and/or CertificateVerify message if no auth cert.
				This will also cover the NO_CERTIFICATE alert sent in
				replacement of the NULL certificate message in SSLv3.
*/
				if (ssl->sec.certMatch > 0) {
					messageSize += secureWriteAdditions(ssl, 4);
				} else {
					messageSize += secureWriteAdditions(ssl, 3);
				}
			} else {
				messageSize += secureWriteAdditions(ssl, 2);
			}
			
			
/*
			The actual buffer size test to hold this flight
*/
			if ((out->buf + out->size) - out->end < messageSize) {
				return SSL_FULL;
			}
			rc = SSL_SUCCESS;

			if (ssl->flags & SSL_FLAGS_CLIENT_AUTH) {
				if (ssl->sec.certMatch == 0 && ssl->majVer == SSL3_MAJ_VER
							&& ssl->minVer == SSL3_MIN_VER) {
					rc = writeAlert(ssl, SSL_ALERT_LEVEL_WARNING,
						SSL_ALERT_NO_CERTIFICATE, out);
				} else {
					rc = writeCertificate(ssl, out, ssl->sec.certMatch);
				}
			}

			if (rc == SSL_SUCCESS) {
				rc = writeClientKeyExchange(ssl, out);
			}
			if (rc == SSL_SUCCESS) {
				rc = writeChangeCipherSpec(ssl, out);
			}
			if (rc == SSL_SUCCESS) {
				rc = writeFinished(ssl, out);
			}
		}
#endif /* USE_CLIENT_SIDE_SSL */
		break;
	}
	if (rc == SSL_ERROR) {
		ssl->flags |= SSL_FLAGS_ERROR;
	}
	return rc;
}

/******************************************************************************/
/*
	Message size must account for any additional length a secure-write
	would add to the message.  It would be too late to check length in
	the writeRecordHeader call since some of the handshake hashing could
	have already taken place and we can't rewind those hashes.
*/
static int32 secureWriteAdditions(ssl_t *ssl, int32 numRecs)
{
	int32 add = 0;
/*
	There is a slim chance for a false FULL message due to the fact that
	the maximum padding is being calculated rather than the actual number.
	Caller must simply grow buffer and try again.
*/
	if (ssl->flags & SSL_FLAGS_WRITE_SECURE) {
		add += (numRecs * ssl->enMacSize) + /* handshake msg hash */
			(numRecs * (ssl->enBlockSize - 1)); /* max padding */
	}
	return add;
}

/******************************************************************************/
/*
	Write out a closure alert message (the only user initiated alert)
	The user would call this when about to initate a socket close
*/
int32 matrixSslEncodeClosureAlert(ssl_t *ssl, sslBuf_t *out)
{
/*
	If we've had a protocol error, don't allow further use of the session
*/
	if (ssl->flags & SSL_FLAGS_ERROR) {
		return SSL_ERROR;
	}
	return writeAlert(ssl, SSL_ALERT_LEVEL_WARNING, SSL_ALERT_CLOSE_NOTIFY,
		out);
}

/******************************************************************************/
/*
	Generic record header construction for alerts, handshake messages, and
	change cipher spec.  Determines message length for encryption and
	writes out to buffer up to the real message data.
*/
static int32 writeRecordHeader(ssl_t *ssl, int32 type, int32 hsType, 
				int32 *messageSize,	char *padLen, unsigned char **encryptStart,
				unsigned char **end, unsigned char **c)
{
	int32	messageData, msn;

	messageData = *messageSize - ssl->recordHeadLen;
	if (type == SSL_RECORD_TYPE_HANDSHAKE) {
		 messageData -= ssl->hshakeHeadLen;
	}


/*
	If this session is already in a secure-write state, determine padding
*/
	*padLen = 0;
	if (ssl->flags & SSL_FLAGS_WRITE_SECURE) {
		*messageSize += ssl->enMacSize;
		*padLen = sslPadLenPwr2(*messageSize - ssl->recordHeadLen,
			ssl->enBlockSize);
		*messageSize += *padLen;
	}

	if (*end - *c < *messageSize) {
/*
		Callers other than sslEncodeResponse do not necessarily check for
		FULL before calling.  We do it here for them.
*/
		return SSL_FULL;
	}


	*c += psWriteRecordInfo(ssl, type, *messageSize - ssl->recordHeadLen, *c);

/*
	All data written after this point is to be encrypted (if secure-write)
*/
	*encryptStart = *c;
	msn = 0;


/*
	Handshake records have another header layer to write here
*/
	if (type == SSL_RECORD_TYPE_HANDSHAKE) {
		*c += psWriteHandshakeHeader(ssl, hsType, messageData, msn, 0,
			messageData, *c);
	}
	return 0;
}

/******************************************************************************/
/*
	Encrypt the message using the current cipher.  This call is used in
	conjunction with the writeRecordHeader function above to finish writing
	an SSL record.  Updates handshake hash if necessary, generates message
	MAC, writes the padding, and does the encrytion.
*/
static int32 encryptRecord(ssl_t *ssl, int32 type, int32 messageSize,
						   int32 padLen, unsigned char *encryptStart,
						   sslBuf_t *out, unsigned char **c)
{
	if (type == SSL_RECORD_TYPE_HANDSHAKE) {
		sslUpdateHSHash(ssl, encryptStart, (int32)(*c - encryptStart));
	}
	*c += ssl->generateMac(ssl, type, encryptStart, 
		(int32)(*c - encryptStart), *c);
	
	*c += sslWritePad(*c, padLen);

	if (ssl->encrypt(&ssl->sec.encryptCtx, encryptStart, encryptStart, 
			(int32)(*c - encryptStart)) < 0 || *c - out->end != messageSize) {
		matrixStrDebugMsg("Error encrypting message for write\n", NULL);
		return SSL_ERROR;
	}

	return 0;
}

#ifdef USE_SERVER_SIDE_SSL
/******************************************************************************/
/*
	Write out the ServerHello message
*/
static int32 writeServerHello(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, rc;
	time_t			t;

	c = out->end;
	end = out->buf + out->size;
/*
	Calculate the size of the message up front, and verify we have room
	We assume there will be a sessionId in the message, and make adjustments
	below if there is no sessionId.
*/
	messageSize =
		ssl->recordHeadLen +
		ssl->hshakeHeadLen +
		38 + SSL_MAX_SESSION_ID_SIZE;

/*
	 First 4 bytes of the serverRandom are the unix time to prevent replay
	 attacks, the rest are random
*/	
	t = time(0);
	ssl->sec.serverRandom[0] = (unsigned char)((t & 0xFF000000) >> 24);
	ssl->sec.serverRandom[1] = (unsigned char)((t & 0xFF0000) >> 16);
	ssl->sec.serverRandom[2] = (unsigned char)((t & 0xFF00) >> 8);
	ssl->sec.serverRandom[3] = (unsigned char)(t & 0xFF);
	if (sslGetEntropy(ssl->sec.serverRandom + 4, SSL_HS_RANDOM_SIZE - 4) < 0) {
		matrixStrDebugMsg("Error gathering serverRandom entropy\n", NULL);
		return SSL_ERROR;
	}
/*
	We register session here because at this point the serverRandom value is
	populated.  If we are able to register the session, the sessionID and
	sessionIdLen fields will be non-NULL, otherwise the session couldn't
	be registered.
*/
	if (!(ssl->flags & SSL_FLAGS_RESUMED)) {
		matrixRegisterSession(ssl);
	}
	messageSize -= (SSL_MAX_SESSION_ID_SIZE - ssl->sessionIdLen);

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_SERVER_HELLO, &messageSize, &padLen, &encryptStart,
			&end, &c)) < 0) {
		return rc;
	}
/*
	First two fields in the ServerHello message are the major and minor
	SSL protocol versions we agree to talk with
*/
	*c = ssl->majVer; c++;
	*c = ssl->minVer; c++;
/*
	The next 32 bytes are the server's random value, to be combined with
	the client random and premaster for key generation later
*/
	memcpy(c, ssl->sec.serverRandom, SSL_HS_RANDOM_SIZE);
	c += SSL_HS_RANDOM_SIZE;
/*
	The next data is a single byte containing the session ID length,
	and up to 32 bytes containing the session id.
	First register the session, which will give us a session id and length
	if not all session slots in the table are used
*/
	*c = ssl->sessionIdLen; c++;
	if (ssl->sessionIdLen > 0) {
        memcpy(c, ssl->sessionId, ssl->sessionIdLen);
		c += ssl->sessionIdLen;
	}
/*
	Two byte cipher suite we've chosen based on the list sent by the client
	and what we support.
	One byte compression method (always zero)
*/
	*c = (ssl->cipher->id & 0xFF00) >> 8; c++;
	*c = ssl->cipher->id & 0xFF; c++;
	*c = 0; c++;

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}
/*
	If we're resuming a session, we now have the clientRandom, master and 
	serverRandom, so we can derive keys which we'll be using shortly.
*/
	if (ssl->flags & SSL_FLAGS_RESUMED) {
		sslDeriveKeys(ssl);
	}
/*
	Verify that we've calculated the messageSize correctly, really this
	should never fail; it's more of an implementation verification
*/
	if (c - out->end != messageSize) {
		return SSL_ERROR;
	}
	out->end = c;
	return SSL_SUCCESS;
}

/******************************************************************************/
/*
	ServerHelloDone message is a blank handshake message
*/
static int32 writeServerHelloDone(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, rc;

	c = out->end;
	end = out->buf + out->size;
	messageSize =
		ssl->recordHeadLen +
		ssl->hshakeHeadLen;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_SERVER_HELLO_DONE, &messageSize, &padLen,
			&encryptStart, &end, &c)) < 0) {
		return rc;
	}

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Error generating hello done for write\n", NULL);
		return SSL_ERROR;
	}
	out->end = c;
	return SSL_SUCCESS;
}


/******************************************************************************/
/*
	Server initiated rehandshake public API call.
*/
int32 matrixSslEncodeHelloRequest(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, rc;

	if (ssl->flags & SSL_FLAGS_ERROR || ssl->flags & SSL_FLAGS_CLOSED) {
		return SSL_ERROR;
	}
	if (!(ssl->flags & SSL_FLAGS_SERVER) || (ssl->hsState != SSL_HS_DONE)) {
		return SSL_ERROR;
	}

	c = out->end;
	end = out->buf + out->size;
	messageSize =
		ssl->recordHeadLen +
		ssl->hshakeHeadLen;
	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_HELLO_REQUEST, &messageSize, &padLen,
			&encryptStart, &end, &c)) < 0) {
		return rc;
	}

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Error generating hello request for write\n", NULL);
		return SSL_ERROR;
	}
	out->end = c;

	return SSL_SUCCESS;
}
#else /* USE_SERVER_SIDE_SSL */
int32 matrixSslEncodeHelloRequest(ssl_t *ssl, sslBuf_t *out)
{
		matrixStrDebugMsg("Library not built with USE_SERVER_SIDE_SSL\n", NULL);
		return -1;
}
#endif /* USE_SERVER_SIDE_SSL */

/******************************************************************************/
/*
	Write a Certificate message.
	The encoding of the message is as follows:
		3 byte length of certificate data (network byte order)
		If there is no certificate,
			3 bytes of 0
		If there is one certificate,
			3 byte length of certificate + 3
			3 byte length of certificate
			certificate data
		For more than one certificate:
			3 byte length of all certificate data
			3 byte length of first certificate
			first certificate data
			3 byte length of second certificate
			second certificate data
	Certificate data is the base64 section of an X.509 certificate file
	in PEM format decoded to binary.  No additional interpretation is required.
*/
static int32 writeCertificate(ssl_t *ssl, sslBuf_t *out, int32 notEmpty)
{
	sslLocalCert_t	*cert;
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32			totalCertLen, certLen, lsize, messageSize, i, rc;


	c = out->end;
	end = out->buf + out->size;

/*
	Determine total length of certs
*/
	totalCertLen = i = 0;
	if (notEmpty) {
		cert = &ssl->keys->cert;
		for (; cert != NULL; i++) {
			totalCertLen += cert->certLen;
			cert = cert->next;
		}
	}
/*
	Account for the 3 bytes of certChain len for each cert and get messageSize
*/
	lsize = 3 + (i * 3);
	messageSize =
		ssl->recordHeadLen +
		ssl->hshakeHeadLen +
		lsize + totalCertLen;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_CERTIFICATE, &messageSize, &padLen, &encryptStart,
			&end, &c)) < 0) {
		return rc;
	}

/*
	Write out the certs
*/
	*c = ((totalCertLen + (lsize - 3)) & 0xFF0000) >> 16; c++;
	*c = ((totalCertLen + (lsize - 3)) & 0xFF00) >> 8; c++;
	*c = ((totalCertLen + (lsize - 3)) & 0xFF); c++;

	if (notEmpty) {
		cert = &ssl->keys->cert;
		while (cert) {
			certLen = cert->certLen;
			if (certLen > 0) {
				*c = (certLen & 0xFF0000) >> 16; c++;
				*c = (certLen & 0xFF00) >> 8; c++;
				*c = (certLen & 0xFF); c++;
				memcpy(c, cert->certBin, certLen);
				c += certLen;
			}
			cert = cert->next;
		}
	}
	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Error parsing certificate for write\n", NULL);
		return SSL_ERROR;
	}
	out->end = c;
	return SSL_SUCCESS;
}

/******************************************************************************/
/*
	Write the ChangeCipherSpec message.  It has its own message type
	and contains just one byte of value one.  It is not a handshake 
	message, so it isn't included in the handshake hash.
*/
static int32 writeChangeCipherSpec(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, rc;

	c = out->end;
	end = out->buf + out->size;
	messageSize = ssl->recordHeadLen + 1;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC, 0,
			&messageSize, &padLen, &encryptStart, &end, &c)) < 0) {
		return rc;
	}
	*c = 1; c++;

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_CHANGE_CIPHER_SPEC,
			messageSize, padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Error generating change cipher for write\n", NULL);
		return SSL_ERROR;
	}
	out->end = c;
/*
	After the peer parses the ChangeCipherSpec message, it will expect
	the next message to be encrypted, so activate encryption on outgoing
	data now
*/
	sslActivateWriteCipher(ssl);


	return SSL_SUCCESS;
}

/******************************************************************************/
/*
	Write the Finished message
	The message contains the 36 bytes, the 16 byte MD5 and 20 byte SHA1 hash
	of all the handshake messages so far (excluding this one!)
*/
static int32 writeFinished(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, verifyLen, rc;

	c = out->end;
	end = out->buf + out->size;
	verifyLen = SSL_MD5_HASH_SIZE + SSL_SHA1_HASH_SIZE;
	messageSize = ssl->recordHeadLen + ssl->hshakeHeadLen + verifyLen;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE, SSL_HS_FINISHED,
			&messageSize, &padLen, &encryptStart, &end, &c)) < 0) {
		return rc;
	}
/*
	Output the hash of messages we've been collecting so far into the buffer
*/
	c += sslSnapshotHSHash(ssl, c, ssl->flags & SSL_FLAGS_SERVER);

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Error generating finished for write\n", NULL);
		return SSL_ERROR;
	}
	out->end = c;


#ifdef USE_CLIENT_SIDE_SSL
/*
	Free handshake pool, of which the cert is the primary member.
	There is also an attempt to free the handshake pool during
	the reciept of the finished message.  Both cases are attempted
	to keep the lifespan of this pool as short as possible. This
	is the default case for the client side.
*/
	if (ssl->sec.cert) {
		matrixX509FreeCert(ssl->sec.cert);
		ssl->sec.cert = NULL;
	}
#endif /* USE_CLIENT_SIDE */


	return SSL_SUCCESS;
}

/******************************************************************************/
/*
	Write an Alert message
	The message contains two bytes: AlertLevel and AlertDescription
*/
static int32 writeAlert(ssl_t *ssl, unsigned char level, 
						unsigned char description, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32				messageSize, rc;

	c = out->end;
	end = out->buf + out->size;
	messageSize = 2 + ssl->recordHeadLen;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_ALERT, 0, &messageSize,
			&padLen, &encryptStart, &end, &c)) < 0) {
		return rc;
	}
	*c = level; c++;
	*c = description; c++;

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_ALERT, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	out->end = c;
	return SSL_SUCCESS;
}

#ifdef USE_CLIENT_SIDE_SSL
/******************************************************************************/
/*
	Write out the ClientHello message to a buffer
*/
int32 matrixSslEncodeClientHello(ssl_t *ssl, sslBuf_t *out,
							   unsigned short cipherSpec)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32			messageSize, rc, cipherLen, cookieLen;
	time_t			t;

	if (ssl->flags & SSL_FLAGS_ERROR || ssl->flags & SSL_FLAGS_CLOSED) {
		return SSL_ERROR;
	}
	if (ssl->flags & SSL_FLAGS_SERVER || (ssl->hsState != SSL_HS_SERVER_HELLO &&
			ssl->hsState != SSL_HS_DONE &&
			ssl->hsState != SSL_HS_HELLO_REQUEST )) {
		return SSL_ERROR;
	}
	
	sslInitHSHash(ssl);

	cookieLen = 0;
/*
	If session resumption is being done on a rehandshake, make sure we are
	sending	the same cipher	spec as the currently negotiated one. If no
	resumption, clear the RESUMED flag in case the caller forgot to clear
	it with	matrixSslSetSessionOption.
*/
	if (ssl->sessionIdLen > 0) {
		cipherSpec = ssl->cipher->id;
	} else {
		ssl->flags &= ~SSL_FLAGS_RESUMED;
	}

/*
	If a cipher is specified it is two bytes length and two bytes data. 
*/
	if (cipherSpec == 0) {
		cipherLen = sslGetCipherSpecListLen();
	} else {
		if (sslGetCipherSpec(cipherSpec) == NULL) {
			matrixIntDebugMsg("Cipher suite not supported: %d\n", cipherSpec);
			return SSL_ERROR;
		}
		cipherLen = 4;
	}

	c = out->end;
	end = out->buf + out->size;
/*
	Calculate the size of the message up front, and write header
*/
	messageSize = ssl->recordHeadLen + ssl->hshakeHeadLen +
		5 + SSL_HS_RANDOM_SIZE + ssl->sessionIdLen + cipherLen + cookieLen;


	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_CLIENT_HELLO, &messageSize, &padLen, &encryptStart,
			&end, &c)) < 0) {
		return rc;
	}

/*
	First 4 bytes of the serverRandom are the unix time to prevent replay
	attacks, the rest are random
*/

	t = time(0);
	ssl->sec.clientRandom[0] = (unsigned char)((t & 0xFF000000) >> 24);
	ssl->sec.clientRandom[1] = (unsigned char)((t & 0xFF0000) >> 16);
	ssl->sec.clientRandom[2] = (unsigned char)((t & 0xFF00) >> 8);
	ssl->sec.clientRandom[3] = (unsigned char)(t & 0xFF);
	if (sslGetEntropy(ssl->sec.clientRandom + 4, SSL_HS_RANDOM_SIZE - 4) < 0) {
		matrixStrDebugMsg("Error gathering clientRandom entropy\n", NULL);
		return SSL_ERROR;
	}
/*
	First two fields in the ClientHello message are the maximum major 
	and minor SSL protocol versions we support
*/
	*c = ssl->majVer; c++;
	*c = ssl->minVer; c++;
/*
	The next 32 bytes are the server's random value, to be combined with
	the client random and premaster for key generation later
*/
	memcpy(c, ssl->sec.clientRandom, SSL_HS_RANDOM_SIZE);
	c += SSL_HS_RANDOM_SIZE;
/*
	The next data is a single byte containing the session ID length,
	and up to 32 bytes containing the session id.
	If we are asking to resume a session, then the sessionId would have
	been set at session creation time.
*/
	*c = ssl->sessionIdLen; c++;
	if (ssl->sessionIdLen > 0) {
        memcpy(c, ssl->sessionId, ssl->sessionIdLen);
		c += ssl->sessionIdLen;
	}
/*
	Write out the length and ciphers we support
	Client can request a single specific cipher in the cipherSpec param
*/
	if (cipherSpec == 0) {
		if ((rc = sslGetCipherSpecList(c, (int32)(end - c))) < 0) {
			return SSL_FULL;
		}
		c += rc;
	} else {
		if ((int32)(end - c) < 4) {
			return SSL_FULL;
		}
		*c = 0; c++;
		*c = 2; c++;
		*c = (cipherSpec & 0xFF00) >> 8; c++;
		*c = cipherSpec & 0xFF; c++;
	}	
/*
	Followed by two bytes (len and compression method (always zero))
*/
	*c = 1; c++;
	*c = 0; c++;


	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}
/*
	Verify that we've calculated the messageSize correctly, really this
	should never fail; it's more of an implementation verification
*/
	if (c - out->end != messageSize) {
		return SSL_ERROR;
	}
	out->end = c;

/*
	Could be a rehandshake so clean	up old context if necessary.
	Always explicitly set state to beginning.  
*/
	if (ssl->hsState == SSL_HS_DONE) {
		sslResetContext(ssl);
	}

/*
	Could be a rehandshake on a previous connection that used client auth.
	Reset our local client auth state as the server is always the one
	responsible for initiating it.
*/
	ssl->flags &= ~SSL_FLAGS_CLIENT_AUTH;
	ssl->hsState = SSL_HS_SERVER_HELLO;
	return SSL_SUCCESS;
}

/******************************************************************************/
/*
	Write a ClientKeyExchange message.
*/
static int32 writeClientKeyExchange(ssl_t *ssl, sslBuf_t *out)
{
	unsigned char	*c, *end, *encryptStart;
	char			padLen;
	int32			messageSize, keyLen, explicitLen, rc;

	c = out->end;
	end = out->buf + out->size;
	messageSize = keyLen = 0;



/*
	Determine messageSize for the record header
*/
			/* Standard RSA auth suites */
			keyLen = ssl->sec.cert->publicKey.size;

	messageSize += ssl->recordHeadLen + ssl->hshakeHeadLen + keyLen;
	explicitLen = 0;

	if ((rc = writeRecordHeader(ssl, SSL_RECORD_TYPE_HANDSHAKE,
			SSL_HS_CLIENT_KEY_EXCHANGE, &messageSize, &padLen,
			&encryptStart, &end, &c)) < 0) {
		return rc;
	}
		
/*
	ClientKeyExchange message contains the encrypted premaster secret.
	The base premaster is the original SSL protocol version we asked for
	followed by 46 bytes of random data.
	These 48 bytes are padded to the current RSA key length and encrypted
	with the RSA key.
*/
	if (explicitLen == 1) {

/*
		Add the two bytes of key length
*/
		if (keyLen > 0) {
			*c = (keyLen & 0xFF00) >> 8; c++;
			*c = (keyLen & 0xFF); c++;
		}
	}


/*
			Standard RSA suite
*/
			ssl->sec.premasterSize = SSL_HS_RSA_PREMASTER_SIZE;
			ssl->sec.premaster = psMalloc(ssl->hsPool,
										  SSL_HS_RSA_PREMASTER_SIZE);
										  
			ssl->sec.premaster[0] = ssl->reqMajVer;
			ssl->sec.premaster[1] = ssl->reqMinVer;
			if (sslGetEntropy(ssl->sec.premaster + 2,
					SSL_HS_RSA_PREMASTER_SIZE - 2) < 0) {
				matrixStrDebugMsg("Error gathering premaster entropy\n", NULL);
				return SSL_ERROR;
			}

			sslActivatePublicCipher(ssl);
			
			if (ssl->encryptPub(ssl->hsPool, &(ssl->sec.cert->publicKey),
					ssl->sec.premaster, ssl->sec.premasterSize, c,
					(int32)(end - c)) != keyLen) {
				matrixStrDebugMsg("Error encrypting premaster\n", NULL);
				return SSL_FULL;
			}

			c += keyLen;

	if ((rc = encryptRecord(ssl, SSL_RECORD_TYPE_HANDSHAKE, messageSize,
			padLen, encryptStart, out, &c)) < 0) {
		return rc;
	}

	if (c - out->end != messageSize) {
		matrixStrDebugMsg("Invalid ClientKeyExchange length\n", NULL);
		return SSL_ERROR;
	}

/*
	Now that we've got the premaster secret, derive the various symmetric
	keys using it and the client and server random values
*/
	sslDeriveKeys(ssl);

	out->end = c;
	return SSL_SUCCESS;
}


#else /* USE_CLIENT_SIDE_SSL */
/******************************************************************************/
/*
	Stub out this function rather than ifdef it out in the public header
*/
int32 matrixSslEncodeClientHello(ssl_t *ssl, sslBuf_t *out,
							   unsigned short cipherSpec)
{
	matrixStrDebugMsg("Library not built with USE_CLIENT_SIDE_SSL\n", NULL);
	return -1;
}
#endif /* USE_CLIENT_SIDE_SSL */


/******************************************************************************/
/*
	Write out a SSLv3 record header.
	Assumes 'c' points to a buffer of at least SSL3_HEADER_LEN bytes
		1 byte type (SSL_RECORD_TYPE_*)
		1 byte major version
		1 byte minor version
		2 bytes length (network byte order)
	Returns the number of bytes written
*/
int32 psWriteRecordInfo(ssl_t *ssl, unsigned char type, int32 len, unsigned char *c)
{
	*c = type; c++;
	*c = ssl->majVer; c++;
	*c = ssl->minVer; c++;
	*c = (len & 0xFF00) >> 8; c++;
	*c = (len & 0xFF);

	return ssl->recordHeadLen;
}

/******************************************************************************/
/*
	Write out an ssl handshake message header.
	Assumes 'c' points to a buffer of at least ssl->hshakeHeadLen bytes
		1 byte type (SSL_HS_*)
		3 bytes length (network byte order)
	Returns the number of bytes written
*/
int32 psWriteHandshakeHeader(ssl_t *ssl, unsigned char type, int32 len, 
								int32 seq, int32 fragOffset, int32 fragLen,
								unsigned char *c)
{
	*c = type; c++;
	*c = (len & 0xFF0000) >> 16; c++;
	*c = (len & 0xFF00) >> 8; c++;
	*c = (len & 0xFF);

	return ssl->hshakeHeadLen;
}

/******************************************************************************/
/*
	Write pad bytes and pad length per the TLS spec.  Most block cipher
	padding fills each byte with the number of padding bytes, but SSL/TLS
	pretends one of these bytes is a pad length, and the remaining bytes are
	filled with that length.  The end result is that the padding is identical
	to standard padding except the values are one less. For SSLv3 we are not
	required to have any specific pad values, but they don't hurt.

	PadLen	Result
	0
	1		00
	2		01 01
	3		02 02 02
	4		03 03 03 03
	5		04 04 04 04 04
	6		05 05 05 05 05 05
	7		06 06 06 06 06 06 06
	8		07 07 07 07 07 07 07 07

	We calculate the length of padding required for a record using
	sslPadLenPwr2()
*/
int32 sslWritePad(unsigned char *p, unsigned char padLen)
{
	unsigned char c = padLen;

	while (c-- > 0) {
		*p++ = padLen - 1;
	}
	return padLen;
}

/******************************************************************************/



