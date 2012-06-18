/*
 *	matrixCommon.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Public common header file
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

#ifndef _h_MATRIXCOMMON
#define _h_MATRIXCOMMON

#ifdef __cplusplus
extern "C" {
#endif

#include "src/matrixConfig.h"

/******************************************************************************/
/*
	Platform integer sizes
*/
typedef int int32;
typedef unsigned int uint32;

/******************************************************************************/
/*
	Helpers
*/
#ifndef VXWORKS
#ifndef min
#define min(a,b)	(((a) < (b)) ? (a) : (b))
#endif /* min */

#ifndef max
#define max(a,b)	(((a) > (b)) ? (a) : (b))
#endif /* max */
#endif /* VXWORKS */

/******************************************************************************/
/*
	Flags for matrixSslNewSession
*/
#define	SSL_FLAGS_SERVER		0x1
#define SSL_FLAGS_CLIENT_AUTH	0x200

/******************************************************************************/
/*
	matrixSslSetSessionOption defines
*/
#define	SSL_OPTION_DELETE_SESSION		0


/******************************************************************************/
/*
	Typdefs required for public apis.  From an end user perspective, the 
	sslBuf_t and sslCertInfo_t types have internal fields that are public,
	but ssl_t, sslKeys_t, sslCert_t,and sslSessionId_t do not.  Defining
	those as 'int32' requires it to be treated as an opaque data type to be
	passed to public apis
*/
#ifndef _h_EXPORT_SYMBOLS

typedef int32		ssl_t;
typedef int32		sslKeys_t;
typedef int32		sslSessionId_t;
typedef int32		sslCert_t;

/******************************************************************************/
/*
	Explicitly import MATRIXPUBLIC apis on Windows.  If we're being included
	from an internal header, we export them instead!
*/
#ifdef WIN32
#define MATRIXPUBLIC extern __declspec(dllimport)
#endif /* WIN */
#else /* h_EXPORT_SYMOBOLS */
#ifdef WIN32
#define MATRIXPUBLIC extern __declspec(dllexport)
#endif /* WIN */
#endif /* h_EXPORT_SYMOBOLS */
#ifndef WIN32
#define MATRIXPUBLIC extern
#endif /* !WIN */

/******************************************************************************/
/*
	Public structures

	sslBuf_t
	Empty buffer:
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
	|.|.|.|.|.|.|.|.|.|.|.|.|.|.|.|.|
	 ^
	 \end
	 \start
	 \buf
	 size = 16
	 len = (end - start) = 0

	Buffer with data:

     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
	|.|.|a|b|c|d|e|f|g|h|i|j|.|.|.|.|
	 ^   ^                   ^
	 |   |                   \end
	 |   \start
	 \buf
	size = 16
	len = (end - start) = 10

	Read from start pointer
	Write to end pointer
*/
typedef struct {
	unsigned char	*buf;	/* Pointer to the start of the buffer */
	unsigned char	*start;	/* Pointer to start of valid data */
	unsigned char	*end;	/* Pointer to first byte of invalid data */
	int32			size;	/* Size of buffer in bytes */
} sslBuf_t;


/******************************************************************************/
/*
	Information provided to user callback for validating certificates.
	Register callback with call to matrixSslSetCertValidator
*/
typedef struct {
	char	*country;
	char	*state;
	char	*locality;
	char	*organization;
	char	*orgUnit;
	char	*commonName;
} sslDistinguishedName_t;

typedef struct sslSubjectAltNameEntry {
	int32							id;
	unsigned char					name[16];
	unsigned char					*data;
	int32							dataLen;
	struct sslSubjectAltNameEntry	*next;
} sslSubjectAltName_t;

typedef struct sslCertInfo {
	int32					verified;
	unsigned char			*serialNumber;
	int32					serialNumberLen;
	char					*notBefore;
	char					*notAfter;
	char					*sigHash;
	int32					sigHashLen;
	sslSubjectAltName_t		*subjectAltName;
	sslDistinguishedName_t	subject;
	sslDistinguishedName_t	issuer;
	struct sslCertInfo		*next;
} sslCertInfo_t;

/******************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIXCOMMON */

/******************************************************************************/
