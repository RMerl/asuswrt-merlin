/*
 *	matrixSsl.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Public header file for MatrixSSL
 *	Implementations interacting with the matrixssl library should
 *	only use the APIs and definitions used in this file.
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

#ifndef _h_MATRIXSSL
#define _h_MATRIXSSL

#ifdef __cplusplus
extern "C" {
#endif

#include "matrixCommon.h"

/******************************************************************************/
/*
	Maximum SSL record size, per specification
*/
#define		SSL_MAX_PLAINTEXT_LEN	0x4000	/* 16KB */
#define		SSL_MAX_RECORD_LEN		SSL_MAX_PLAINTEXT_LEN + 2048
#define		SSL_MAX_BUF_SIZE		SSL_MAX_RECORD_LEN + 0x5

/*
	Return codes from public apis
	Not all apis return all codes.  See documentation for more details.
*/
#define		SSL_SUCCESS			0	/* Generic success */
#define		SSL_ERROR			-1	/* generic ssl error, see error code */
#define		SSL_FULL			-2	/* must call sslRead before decoding */
#define		SSL_PARTIAL			-3	/* more data reqired to parse full msg */
#define		SSL_SEND_RESPONSE	-4	/* decode produced output data */
#define		SSL_PROCESS_DATA	-5	/* succesfully decoded application data */
#define		SSL_ALERT			-6	/* we've decoded an alert */
#define		SSL_FILE_NOT_FOUND	-7	/* File not found */
#define		SSL_MEM_ERROR		-8	/* Memory allocation failure */

/*
	SSL Alert levels and descriptions
	This implementation treats all alerts as fatal
*/
#define SSL_ALERT_LEVEL_WARNING				1
#define SSL_ALERT_LEVEL_FATAL				2

#define SSL_ALERT_CLOSE_NOTIFY				0
#define SSL_ALERT_UNEXPECTED_MESSAGE		10
#define SSL_ALERT_BAD_RECORD_MAC			20
#define SSL_ALERT_DECOMPRESSION_FAILURE		30
#define SSL_ALERT_HANDSHAKE_FAILURE			40
#define SSL_ALERT_NO_CERTIFICATE			41
#define SSL_ALERT_BAD_CERTIFICATE			42
#define SSL_ALERT_UNSUPPORTED_CERTIFICATE	43
#define SSL_ALERT_CERTIFICATE_REVOKED		44
#define SSL_ALERT_CERTIFICATE_EXPIRED		45
#define SSL_ALERT_CERTIFICATE_UNKNOWN		46
#define SSL_ALERT_ILLEGAL_PARAMETER			47

/*
	Use as return code in user validation callback to allow
	anonymous connections to proceed
*/
#define	SSL_ALLOW_ANON_CONNECTION			67

/******************************************************************************/
/*
 *	Public API set
 */
MATRIXPUBLIC int32	matrixSslOpen(void);
MATRIXPUBLIC void	matrixSslClose(void);

MATRIXPUBLIC int32	matrixSslReadKeys(sslKeys_t **keys, const char *certFile,
						const char *privFile, const char *privPass,
						const char *trustedCAFile);

MATRIXPUBLIC int32	matrixSslReadKeysMem(sslKeys_t **keys,
						unsigned char *certBuf, int32 certLen,
						unsigned char *privBuf, int32 privLen,
						unsigned char *trustedCABuf, int32 trustedCALen);

MATRIXPUBLIC void	matrixSslFreeKeys(sslKeys_t *keys);

MATRIXPUBLIC int32	matrixSslNewSession(ssl_t **ssl, sslKeys_t *keys,
						sslSessionId_t *session, int32 flags);
MATRIXPUBLIC void	matrixSslDeleteSession(ssl_t *ssl);

MATRIXPUBLIC int32	matrixSslDecode(ssl_t *ssl, sslBuf_t *in, sslBuf_t *out, 
						unsigned char *error, unsigned char *alertLevel,
						unsigned char *alertDescription);
MATRIXPUBLIC int32	matrixSslEncode(ssl_t *ssl, unsigned char *in, int32 inlen,
						sslBuf_t *out);
MATRIXPUBLIC int32	matrixSslEncodeClosureAlert(ssl_t *ssl, sslBuf_t *out);

MATRIXPUBLIC int32	matrixSslHandshakeIsComplete(ssl_t *ssl);

MATRIXPUBLIC void	matrixSslSetCertValidator(ssl_t *ssl,
						int32 (*certValidator)(sslCertInfo_t *, void *arg),
						void *arg);

MATRIXPUBLIC void	matrixSslSetSessionOption(ssl_t *ssl, int32 option,
						void *arg);
MATRIXPUBLIC void	matrixSslGetAnonStatus(ssl_t *ssl, int32 *anonArg);
MATRIXPUBLIC void	matrixSslAssignNewKeys(ssl_t *ssl, sslKeys_t *keys);

/*
	Client side APIs
*/
MATRIXPUBLIC int32	matrixSslEncodeClientHello(ssl_t *ssl, sslBuf_t *out,
						unsigned short cipherSpec);

MATRIXPUBLIC int32	matrixSslGetSessionId(ssl_t *ssl,
						sslSessionId_t **sessionId);
MATRIXPUBLIC void	matrixSslFreeSessionId(sslSessionId_t *sessionId);


/*
	Server side APIs
*/
MATRIXPUBLIC int32	matrixSslEncodeHelloRequest(ssl_t *ssl, sslBuf_t *out);

MATRIXPUBLIC int32 matrixSslSetResumptionFlag(ssl_t *ssl, char flag);
MATRIXPUBLIC int32 matrixSslGetResumptionFlag(ssl_t *ssl, char *flag);






/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIXSSL */

/******************************************************************************/

