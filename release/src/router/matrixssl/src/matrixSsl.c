/*
 *	matrixSsl.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Secure Sockets Layer session management
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

#ifndef WINCE
#include <time.h>
#endif

/******************************************************************************/

static char copyright[]= "Copyright PeerSec Networks Inc. All rights reserved.";

#ifdef USE_SERVER_SIDE_SSL
/*
	Static session table for session cache and lock for multithreaded env
*/
static sslMutex_t			sessionTableLock;
static sslSessionEntry_t	sessionTable[SSL_SESSION_TABLE_SIZE];
#endif /* USE_SERVER_SIDE_SSL */

/******************************************************************************/
/*
	Open and close the SSL module.  These routines are called once in the 
	lifetime of the application and initialize and clean up the library 
	respectively.
*/
int32 matrixSslOpen(void)
{
	if (matrixPkiOpen() < 0) {
		matrixStrDebugMsg("PKI open failure\n", NULL);
		return -1;
	}

#ifdef USE_SERVER_SIDE_SSL
	memset(sessionTable, 0x0, 
		sizeof(sslSessionEntry_t) * SSL_SESSION_TABLE_SIZE);
	sslCreateMutex(&sessionTableLock);
#endif /* USE_SERVER_SIDE_SSL */


	return 0;
}

void matrixSslClose(void)
{
#ifdef USE_SERVER_SIDE_SSL
	int32		i;

	sslLockMutex(&sessionTableLock);
	for (i = 0; i < SSL_SESSION_TABLE_SIZE; i++) {
		if (sessionTable[i].inUse == 1) {
			matrixStrDebugMsg("Warning: closing while session still in use\n",
				NULL);
		}
	}
	memset(sessionTable, 0x0, 
		sizeof(sslSessionEntry_t) * SSL_SESSION_TABLE_SIZE);
	sslUnlockMutex(&sessionTableLock);
	sslDestroyMutex(&sessionTableLock);
#endif /* USE_SERVER_SIDE_SSL */


	matrixPkiClose();
}

/******************************************************************************/
/*
	Wrappers around the RSA versions.  Necessary to keep API backwards compat
*/
#ifdef USE_FILE_SYSTEM
int32 matrixSslReadKeys(sslKeys_t **keys, const char *certFile,
						const char *privFile, const char *privPass,
						const char *trustedCAFile)
{
	return matrixX509ReadKeys(keys, certFile, privFile, privPass, trustedCAFile);
}
#endif /* USE_FILE_SYSTEM */

int32 matrixSslReadKeysMem(sslKeys_t **keys, unsigned char *certBuf,
						int32 certLen, unsigned char *privBuf, int32 privLen,
						unsigned char *trustedCABuf, int32 trustedCALen)
{
	return matrixX509ReadKeysMem(keys, certBuf, certLen, privBuf, privLen,
		trustedCABuf, trustedCALen);
}

void matrixSslFreeKeys(sslKeys_t *keys)
{
	matrixRsaFreeKeys(keys);
}



/******************************************************************************/
/*
	New SSL protocol context
	This structure is associated with a single SSL connection.  Each socket
	using SSL should be associated with a new SSL context.

	certBuf and privKey ARE NOT duplicated within the server context, in order
	to minimize memory usage with multiple simultaneous requests.  They must 
	not be deleted by caller until all server contexts using them are deleted.
*/
int32 matrixSslNewSession(ssl_t **ssl, sslKeys_t *keys, sslSessionId_t *session,
						int32 flags)
{
	psPool_t	*pool = NULL;
	ssl_t		*lssl;

/*
	First API level chance to make sure a user is not attempting to use
	client or server support that was not built into this library compile
*/
#ifndef USE_SERVER_SIDE_SSL
	if (flags & SSL_FLAGS_SERVER) {
		matrixStrDebugMsg("MatrixSSL lib not compiled with server support\n",
			NULL);
		return -1;
	}
#endif
#ifndef USE_CLIENT_SIDE_SSL
	if (!(flags & SSL_FLAGS_SERVER)) {
		matrixStrDebugMsg("MatrixSSL lib not compiled with client support\n",
			NULL);
		return -1;
	}
#endif
	if (flags & SSL_FLAGS_CLIENT_AUTH) {
		matrixStrDebugMsg("MatrixSSL lib not compiled with client " \
			"authentication support\n", NULL);
		return -1;
	}

	if (flags & SSL_FLAGS_SERVER) {
		if (keys == NULL) {
			matrixStrDebugMsg("NULL keys in  matrixSslNewSession\n", NULL);
			return -1;
		}
		if (session != NULL) {
			matrixStrDebugMsg("Server session must be NULL\n", NULL);
			return -1;
		}
	}

	*ssl = lssl = psMalloc(pool, sizeof(ssl_t));
	if (lssl == NULL) {
		return SSL_MEM_ERROR;
	}
	memset(lssl, 0x0, sizeof(ssl_t));

	lssl->pool = pool;
	lssl->cipher = sslGetCipherSpec(SSL_NULL_WITH_NULL_NULL);
	sslActivateReadCipher(lssl);
	sslActivateWriteCipher(lssl);
	sslActivatePublicCipher(lssl);
	
	lssl->recordHeadLen = SSL3_HEADER_LEN;
	lssl->hshakeHeadLen = SSL3_HANDSHAKE_HEADER_LEN;

	if (flags & SSL_FLAGS_SERVER) {
		lssl->flags |= SSL_FLAGS_SERVER;
/*
		Client auth can only be requested by server, not set by client
*/
		if (flags & SSL_FLAGS_CLIENT_AUTH) {
			lssl->flags |= SSL_FLAGS_CLIENT_AUTH;
		}
		lssl->hsState = SSL_HS_CLIENT_HELLO;
	} else {
/*
		Client is first to set protocol version information based on
		compile and/or the 'flags' parameter so header information in
		the handshake messages will be correctly set.
*/
		lssl->majVer = SSL3_MAJ_VER;
		lssl->minVer = SSL3_MIN_VER;
		lssl->hsState = SSL_HS_SERVER_HELLO;
		if (session != NULL && session->cipherId != SSL_NULL_WITH_NULL_NULL) {
			lssl->cipher = sslGetCipherSpec(session->cipherId);
			if (lssl->cipher == NULL) {
				matrixStrDebugMsg("Invalid session id to matrixSslNewSession\n",
					NULL);
			} else {
				memcpy(lssl->sec.masterSecret, session->masterSecret, 
					SSL_HS_MASTER_SIZE);
				lssl->sessionIdLen = SSL_MAX_SESSION_ID_SIZE;
				memcpy(lssl->sessionId, session->id, SSL_MAX_SESSION_ID_SIZE);
			}
		}
	}
	lssl->err = SSL_ALERT_NONE;
	lssl->keys = keys;

	return 0;
}

/******************************************************************************/
/*
	Delete an SSL session.  Some information on the session may stay around
	in the session resumption cache.
	SECURITY - We memset relevant values to zero before freeing to reduce 
	the risk of our keys floating around in memory after we're done.
*/
void matrixSslDeleteSession(ssl_t *ssl)
{

	if (ssl == NULL) {
		return;
	}
	ssl->flags |= SSL_FLAGS_CLOSED;
/*
	If we have a sessionId, for servers we need to clear the inUse flag in 
	the session cache so the ID can be replaced if needed.  In the client case
	the caller should have called matrixSslGetSessionId already to copy the
	master secret and sessionId, so free it now.

	In all cases except a successful updateSession call on the server, the
	master secret must be freed.
*/
#ifdef USE_SERVER_SIDE_SSL
	if (ssl->sessionIdLen > 0 && (ssl->flags & SSL_FLAGS_SERVER)) {
		matrixUpdateSession(ssl);
	}
#endif /* USE_SERVER_SIDE_SSL */
	ssl->sessionIdLen = 0;

#ifdef USE_CLIENT_SIDE_SSL
	if (ssl->sec.cert) {
		matrixX509FreeCert(ssl->sec.cert);
		ssl->sec.cert = NULL;
	}
#endif /* USE_CLIENT_SIDE_SSL */

/*
	Premaster could also be allocated if this DeleteSession is the result
	of a failed handshake.  This test is fine since all frees will NULL pointer
*/
	if (ssl->sec.premaster) {
		psFree(ssl->sec.premaster);
	}



/*
	The cipher and mac contexts are inline in the ssl structure, so
	clearing the structure clears those states as well.
*/
	memset(ssl, 0x0, sizeof(ssl_t));
	psFree(ssl);
}

/******************************************************************************/
/*
	Generic session option control for changing already connected sessions.
	(ie. rehandshake control).  arg param is future for options that may
	require a value.
*/
void matrixSslSetSessionOption(ssl_t *ssl, int32 option, void *arg)
{
	if (option == SSL_OPTION_DELETE_SESSION) {
#ifdef USE_SERVER_SIDE_SSL
		if (ssl->flags & SSL_FLAGS_SERVER) {
			matrixClearSession(ssl, 1);
		}
#endif /* USE_SERVER_SIDE_SSL */
		ssl->sessionIdLen = 0;
		memset(ssl->sessionId, 0x0, SSL_MAX_SESSION_ID_SIZE);
	}
}

/******************************************************************************/
/*
	The ssl_t struct is opaque to the socket layer.  Just needed an access
	routine for the 'anon' status
*/
void matrixSslGetAnonStatus(ssl_t *ssl, int32 *certArg)
{
	*certArg = ssl->sec.anon;
}

/******************************************************************************/
/*
	Again, the ssl_t is opaque.  Need to associate the new keys with
	the session for mid-session rekeying
*/
void matrixSslAssignNewKeys(ssl_t *ssl, sslKeys_t *keys)
{
	ssl->keys = keys;
}

/******************************************************************************/
/*
	Returns 1 if we've completed the SSL handshake.  0 if we're in process.
*/
int32 matrixSslHandshakeIsComplete(ssl_t *ssl)
{
	return (ssl->hsState == SSL_HS_DONE) ? 1 : 0;
}

#ifdef USE_CLIENT_SIDE_SSL
/******************************************************************************/
/*
	Set a custom callback to receive the certificate being presented to the
	session to perform custom authentication if needed
*/
void matrixSslSetCertValidator(ssl_t *ssl,
				int32 (*certValidator)(sslCertInfo_t *t, void *arg), void *arg)
{
	if (certValidator) {
		ssl->sec.validateCert = certValidator;
		ssl->sec.validateCertArg = arg;
	}
}
#else /* Public API, so should always be linkable */
void matrixSslSetCertValidator(ssl_t *ssl,
				int32 (*certValidator)(sslCertInfo_t *t, void *arg), void *arg)
{
	matrixStrDebugMsg("matrixSslSetCertValidator is not available\n", NULL);
	matrixStrDebugMsg("Library not built for cert validation support\n", NULL);
}
#endif /* USE_CLIENT_SIDE_SSL */

/******************************************************************************/
/*
	Initialize the SHA1 and MD5 hash contexts for the handshake messages
*/
int32 sslInitHSHash(ssl_t *ssl)
{
	matrixSha1Init(&ssl->sec.msgHashSha1);
	matrixMd5Init(&ssl->sec.msgHashMd5);
	return 0;
}

/******************************************************************************/
/*
	Add the given data to the running hash of the handshake messages
*/
int32 sslUpdateHSHash(ssl_t *ssl, unsigned char *in, int32 len)
{
	matrixMd5Update(&ssl->sec.msgHashMd5, in, len);
	matrixSha1Update(&ssl->sec.msgHashSha1, in, len);
	return 0;
}

/******************************************************************************/
/*
	Snapshot is called by the receiver of the finished message to produce
	a hash of the preceeding handshake messages for comparison to incoming
	message.
*/
int32 sslSnapshotHSHash(ssl_t *ssl, unsigned char *out, int32 senderFlag)
{
	sslMd5Context_t		md5;
	sslSha1Context_t	sha1;
	
/*
	Use a backup of the message hash-to-date because we don't want
	to destroy the state of the handshaking until truly complete
*/
	md5 = ssl->sec.msgHashMd5;
	sha1 = ssl->sec.msgHashSha1;

	return sslGenerateFinishedHash(&md5, &sha1, ssl->sec.masterSecret,
			out, senderFlag);
}

/******************************************************************************/
/*
	Cipher suites are chosen before they are activated with the 
	ChangeCipherSuite message.  Additionally, the read and write cipher suites
	are activated at different times in the handshake process.  The following
	APIs activate the selected cipher suite callback functions.
*/
int32 sslActivateReadCipher(ssl_t *ssl)
{
	ssl->decrypt = ssl->cipher->decrypt;
	ssl->verifyMac = ssl->cipher->verifyMac;
	ssl->deMacSize = ssl->cipher->macSize;
	ssl->deBlockSize = ssl->cipher->blockSize;
	ssl->deIvSize = ssl->cipher->ivSize;
/*
	Reset the expected incoming sequence number for the new suite
*/
	memset(ssl->sec.remSeq, 0x0, sizeof(ssl->sec.remSeq));

	if (ssl->cipher->id != SSL_NULL_WITH_NULL_NULL) {
		ssl->flags |= SSL_FLAGS_READ_SECURE;
/*
		Copy the newly activated read keys into the live buffers
*/
		memcpy(ssl->sec.readMAC, ssl->sec.rMACptr, ssl->cipher->macSize);
		memcpy(ssl->sec.readKey, ssl->sec.rKeyptr, ssl->cipher->keySize);
		memcpy(ssl->sec.readIV, ssl->sec.rIVptr, ssl->cipher->ivSize);
/*
		set up decrypt contexts
 */
		if (ssl->cipher->init(&(ssl->sec), INIT_DECRYPT_CIPHER) < 0) {
			matrixStrDebugMsg("Unable to initialize read cipher suite\n", NULL);
			return -1;
		}
	}
	return 0;
}

int32 sslActivateWriteCipher(ssl_t *ssl)
{

	ssl->encrypt = ssl->cipher->encrypt;
	ssl->generateMac = ssl->cipher->generateMac;
	ssl->enMacSize = ssl->cipher->macSize;
	ssl->enBlockSize = ssl->cipher->blockSize;
	ssl->enIvSize = ssl->cipher->ivSize;
/*
	Reset the outgoing sequence number for the new suite
*/
	memset(ssl->sec.seq, 0x0, sizeof(ssl->sec.seq));
	if (ssl->cipher->id != SSL_NULL_WITH_NULL_NULL) {
		ssl->flags |= SSL_FLAGS_WRITE_SECURE;
/*
		Copy the newly activated write keys into the live buffers
*/
		memcpy(ssl->sec.writeMAC, ssl->sec.wMACptr, ssl->cipher->macSize);
		memcpy(ssl->sec.writeKey, ssl->sec.wKeyptr, ssl->cipher->keySize);
		memcpy(ssl->sec.writeIV, ssl->sec.wIVptr, ssl->cipher->ivSize);
/*
		set up encrypt contexts
 */
		if (ssl->cipher->init(&(ssl->sec), INIT_ENCRYPT_CIPHER) < 0) {
			matrixStrDebugMsg("Unable to init write cipher suite\n", NULL);
			return -1;
		}
	}
	return 0;
}

int32 sslActivatePublicCipher(ssl_t *ssl)
{
	ssl->encryptPriv = ssl->cipher->encryptPriv;
	ssl->decryptPub = ssl->cipher->decryptPub;
	ssl->decryptPriv = ssl->cipher->decryptPriv;
	ssl->encryptPub = ssl->cipher->encryptPub;
	if (ssl->cipher->id != SSL_NULL_WITH_NULL_NULL) {
		ssl->flags |= SSL_FLAGS_PUBLIC_SECURE;
	}
	return 0;
}

#ifdef USE_SERVER_SIDE_SSL
/******************************************************************************/
/*
	Register a session in the session resumption cache.  If successful (rc >=0),
	the ssl sessionId and sessionIdLength fields will be non-NULL upon
	return.
*/
int32 matrixRegisterSession(ssl_t *ssl)
{
	uint32		i, j;
	sslTime_t	t;

	if (!(ssl->flags & SSL_FLAGS_SERVER)) {
		return -1;
	}
/*
	Iterate the session table, looking for an empty entry (cipher null), and
	the oldest entry that is not in use
*/
	sslLockMutex(&sessionTableLock);
	j = SSL_SESSION_TABLE_SIZE;
	t = sessionTable[0].accessTime;
	for (i = 0; i < SSL_SESSION_TABLE_SIZE; i++) {
		if (sessionTable[i].cipher == NULL) {
			break;
		}
		if (sslCompareTime(sessionTable[i].accessTime, t) &&
				sessionTable[i].inUse == 0) {
			t = sessionTable[i].accessTime;
			j = i;
		}
	}
/*
	If there were no empty entries, get the oldest unused entry.
	If all entries are in use, return -1, meaning we can't cache the
	session at this time
*/
	if (i >= SSL_SESSION_TABLE_SIZE) {
		if (j < SSL_SESSION_TABLE_SIZE) {
			i = j;
		} else {
			sslUnlockMutex(&sessionTableLock);
			return -1;
		}
	}
/*
	Register the incoming masterSecret and cipher, which could still be null, 
	depending on when we're called.
*/
	memcpy(sessionTable[i].masterSecret, ssl->sec.masterSecret,
		SSL_HS_MASTER_SIZE);
	sessionTable[i].cipher = ssl->cipher;
	sessionTable[i].inUse = 1;
	sslUnlockMutex(&sessionTableLock);
/*
	The sessionId is the current serverRandom value, with the first 4 bytes
	replaced with the current cache index value for quick lookup later.
	FUTURE SECURITY - Should generate more random bytes here for the session
	id.  We re-use the server random as the ID, which is OK, since it is
	sent plaintext on the network, but an attacker listening to a resumed
	connection will also be able to determine part of the original server
	random used to generate the master key, even if he had not seen it
	initially.
*/
	memcpy(sessionTable[i].id, ssl->sec.serverRandom, 
		min(SSL_HS_RANDOM_SIZE, SSL_MAX_SESSION_ID_SIZE));
	ssl->sessionIdLen = SSL_MAX_SESSION_ID_SIZE;
	sessionTable[i].id[0] = (unsigned char)(i & 0xFF);
	sessionTable[i].id[1] = (unsigned char)((i & 0xFF00) >> 8);
	sessionTable[i].id[2] = (unsigned char)((i & 0xFF0000) >> 16);
	sessionTable[i].id[3] = (unsigned char)((i & 0xFF000000) >> 24);
	memcpy(ssl->sessionId, sessionTable[i].id, SSL_MAX_SESSION_ID_SIZE);
/*
	startTime is used to check expiry of the entry
	accessTime is used to for cache replacement logic
	The versions are stored, because a cached session must be reused 
	with same SSL version.
*/
	sslInitMsecs(&sessionTable[i].startTime);
	sessionTable[i].accessTime = sessionTable[i].startTime;
	sessionTable[i].majVer = ssl->majVer;
	sessionTable[i].minVer = ssl->minVer;
	sessionTable[i].flag = 0;

	return i;
}

/******************************************************************************/
/*
	Clear the inUse flag during re-handshakes so the entry may be found
*/
int32 matrixClearSession(ssl_t *ssl, int32 remove)
{
	char	*id;
	uint32	i;

	if (ssl->sessionIdLen <= 0) {
		return -1;
	}
	id = ssl->sessionId;
	
	i = (id[3] << 24) + (id[2] << 16) + (id[1] << 8) + id[0];
	if (i >= SSL_SESSION_TABLE_SIZE || i < 0) {
		return -1;
	}
	sslLockMutex(&sessionTableLock);
	sessionTable[i].inUse = 0;
	sessionTable[i].flag = 0;
/*
	If this is a full removal, actually delete the entry rather than
	just setting the inUse to 0.  Also need to clear any RESUME flag
	on the ssl connection so a new session will be correctly registered.
*/
	if (remove) {
		memset(ssl->sessionId, 0x0, SSL_MAX_SESSION_ID_SIZE);
		ssl->sessionIdLen = 0;
		memset(&sessionTable[i], 0x0, sizeof(sslSessionEntry_t));
		ssl->flags &= ~SSL_FLAGS_RESUMED;
	}
	sslUnlockMutex(&sessionTableLock);
	return 0;
}

/******************************************************************************/
/*
	Look up a session ID in the cache.  If found, set the ssl masterSecret
	and cipher to the pre-negotiated values
*/
int32 matrixResumeSession(ssl_t *ssl)
{
	char	*id;
	uint32	i;

	if (!(ssl->flags & SSL_FLAGS_SERVER)) {
		return -1;
	}
	if (ssl->sessionIdLen <= 0) {
		return -1;
	}
	id = ssl->sessionId;

	i = (id[3] << 24) + (id[2] << 16) + (id[1] << 8) + id[0];
	
	sslLockMutex(&sessionTableLock);
	if (i >= SSL_SESSION_TABLE_SIZE || i < 0 ||
			sessionTable[i].cipher == NULL) {
		sslUnlockMutex(&sessionTableLock);
		return -1;
	}
/*
	Id looks valid.  Update the access time for expiration check.
	Expiration is done on daily basis (86400 seconds)
*/
	sslInitMsecs(&sessionTable[i].accessTime);
	if (memcmp(sessionTable[i].id, id, 
				min(ssl->sessionIdLen, SSL_MAX_SESSION_ID_SIZE)) != 0 ||
			sslDiffSecs(sessionTable[i].startTime,
				sessionTable[i].accessTime) > 86400 ||
			sessionTable[i].inUse ||
			sessionTable[i].majVer != ssl->majVer ||
			sessionTable[i].minVer != ssl->minVer) {
		sslUnlockMutex(&sessionTableLock);
		return -1;
	}
	memcpy(ssl->sec.masterSecret, sessionTable[i].masterSecret,
		SSL_HS_MASTER_SIZE);
	ssl->cipher = sessionTable[i].cipher;
	sessionTable[i].inUse = 1;
	sslUnlockMutex(&sessionTableLock);
	return 0;
}

/******************************************************************************/
/*
	Update session information in the cache.
	This is called when we've determined the master secret and when we're
	closing the connection to update various values in the cache.
*/
int32 matrixUpdateSession(ssl_t *ssl)
{
	char	*id;
	uint32	i;

	if (!(ssl->flags & SSL_FLAGS_SERVER)) {
		return -1;
	}
	if ((id = ssl->sessionId) == NULL) {
		return -1;
	}
	i = (id[3] << 24) + (id[2] << 16) + (id[1] << 8) + id[0];
	if (i < 0 || i >= SSL_SESSION_TABLE_SIZE) {
		return -1;
	}
/*
	If there is an error on the session, invalidate for any future use
*/
	sslLockMutex(&sessionTableLock);
	sessionTable[i].inUse = ssl->flags & SSL_FLAGS_CLOSED ? 0 : 1;
	if (ssl->flags & SSL_FLAGS_ERROR) {
		memset(sessionTable[i].masterSecret, 0x0, SSL_HS_MASTER_SIZE);
		sessionTable[i].cipher = NULL;
		sslUnlockMutex(&sessionTableLock);
		return -1;
	}
	memcpy(sessionTable[i].masterSecret, ssl->sec.masterSecret,
		SSL_HS_MASTER_SIZE);
	sessionTable[i].cipher = ssl->cipher;
	sslUnlockMutex(&sessionTableLock);
	return 0;
}

int32 matrixSslSetResumptionFlag(ssl_t *ssl, char flag)
{
	char	*id;
	uint32	i;

	if (!(ssl->flags & SSL_FLAGS_SERVER)) {
		return -1;
	}
	if ((id = ssl->sessionId) == NULL) {
		return -1;
	}
	i = (id[3] << 24) + (id[2] << 16) + (id[1] << 8) + id[0];
	if (i < 0 || i >= SSL_SESSION_TABLE_SIZE) {
		return -1;
	}
	sslLockMutex(&sessionTableLock);
	sessionTable[i].inUse = ssl->flags & SSL_FLAGS_CLOSED ? 0 : 1;
	if (ssl->flags & SSL_FLAGS_ERROR) {
		sslUnlockMutex(&sessionTableLock);
		return -1;
	}
	sessionTable[i].flag = flag;
	sslUnlockMutex(&sessionTableLock);
	return 0;
}

int32 matrixSslGetResumptionFlag(ssl_t *ssl, char *flag)
{
	char	*id;
	uint32	i;

	if (!(ssl->flags & SSL_FLAGS_SERVER)) {
		return -1;
	}
	if ((id = ssl->sessionId) == NULL) {
		return -1;
	}
	i = (id[3] << 24) + (id[2] << 16) + (id[1] << 8) + id[0];
	if (i < 0 || i >= SSL_SESSION_TABLE_SIZE) {
		return -1;
	}
	sslLockMutex(&sessionTableLock);
	sessionTable[i].inUse = ssl->flags & SSL_FLAGS_CLOSED ? 0 : 1;
	if (ssl->flags & SSL_FLAGS_ERROR) {
		sslUnlockMutex(&sessionTableLock);
		return -1;
	}
	*flag = sessionTable[i].flag;
	sslUnlockMutex(&sessionTableLock);
	return 0;
}
#endif /* USE_SERVER_SIDE_SSL */

#ifdef USE_CLIENT_SIDE_SSL
/******************************************************************************/
/*
	Get session information from the ssl structure and populate the given
	session structure.  Session will contain a copy of the relevant session
	information, suitable for creating a new, resumed session.
*/
int32 matrixSslGetSessionId(ssl_t *ssl, sslSessionId_t **session)
{
	sslSessionId_t *lsession;

	if (ssl == NULL || ssl->flags & SSL_FLAGS_SERVER) {
		return -1;
	}

	if (ssl->cipher != NULL && ssl->cipher->id != SSL_NULL_WITH_NULL_NULL && 
			ssl->sessionIdLen == SSL_MAX_SESSION_ID_SIZE) {
		*session = lsession = psMalloc(PEERSEC_BASE_POOL,
			sizeof(sslSessionId_t));
		if (lsession == NULL) {
			return SSL_MEM_ERROR;
		}
		lsession->cipherId = ssl->cipher->id;
		memcpy(lsession->id, ssl->sessionId, ssl->sessionIdLen);
		memcpy(lsession->masterSecret, ssl->sec.masterSecret, 
			SSL_HS_MASTER_SIZE);
		return 0;
	}
	return -1;
}

/******************************************************************************/
/*
	Must be called on session returned from matrixSslGetSessionId
*/
void matrixSslFreeSessionId(sslSessionId_t *sessionId)
{
	if (sessionId != NULL) {
		psFree(sessionId);
	}
}
#endif /* USE_CLIENT_SIDE_SSL */

/******************************************************************************/
/*
	Rehandshake. Free any allocated sec members that will be repopulated
*/
void sslResetContext(ssl_t *ssl)
{
#ifdef USE_SERVER_SIDE_SSL
	if (ssl->flags & SSL_FLAGS_SERVER) {
/*
		Clear the inUse flag of the current session so it may be found again
		if client attempts to reuse session id
*/
		matrixClearSession(ssl, 0);
	}
#endif /* USE_SERVER_SIDE_SSL */

}

/******************************************************************************/



