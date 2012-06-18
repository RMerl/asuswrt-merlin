/*
 *	cryptoLayer.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Cryptography provider layered header.  This layer decouples
 *	the cryptography implementation from the SSL protocol implementation.
 *	Contributors adding new providers must implement all functions 
 *	externed below.
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

#ifndef _h_CRYPTO_LAYER
#define _h_CRYPTO_LAYER
#define _h_EXPORT_SYMBOLS

/******************************************************************************/
/*
	Crypto may have some reliance on os layer (psMalloc in particular)
*/
#include "../os/osLayer.h"

/*
	Return the length of padding bytes required for a record of 'LEN' bytes
	The name Pwr2 indicates that calculations will work with 'BLOCKSIZE'
	that are powers of 2.
	Because of the trailing pad length byte, a length that is a multiple
	of the pad bytes
*/
#define sslPadLenPwr2(LEN, BLOCKSIZE) \
	BLOCKSIZE <= 1 ? (unsigned char)0 : \
	(unsigned char)(BLOCKSIZE - ((LEN) & (BLOCKSIZE - 1)))

/*
	Define the default crypto provider here
*/
#define	USE_PEERSEC_CRYPTO

#ifdef __cplusplus
extern "C" {
#endif

#define SSL_MD5_HASH_SIZE		16
#define SSL_SHA1_HASH_SIZE		20

#define SSL_MAX_MAC_SIZE		20
#define SSL_MAX_IV_SIZE			16
#define SSL_MAX_BLOCK_SIZE		16
#define SSL_MAX_SYM_KEY_SIZE	32

#define USE_X509 /* Must define for certificate support */
/*
	Enable the algorithms used for each cipher suite
*/

#ifdef USE_SSL_RSA_WITH_NULL_MD5
#define USE_RSA
#define USE_MD5_MAC
#endif

#ifdef USE_SSL_RSA_WITH_NULL_SHA
#define USE_RSA
#define USE_SHA1_MAC
#endif

#ifdef USE_SSL_RSA_WITH_RC4_128_SHA
#define USE_ARC4
#define USE_SHA1_MAC
#define USE_RSA
#endif

#ifdef USE_SSL_RSA_WITH_RC4_128_MD5
#define USE_ARC4
#define USE_MD5_MAC
#define USE_RSA
#endif

#ifdef USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA
#define USE_3DES
#define USE_SHA1_MAC
#define USE_RSA
#endif

/*
	Support for optionally encrypted private key files. These are
	usually encrypted with 3DES.
*/
#ifdef USE_ENCRYPTED_PRIVATE_KEYS
#define USE_3DES
#endif

/*
	Support for client side SSL
*/
#ifdef USE_CLIENT_SIDE_SSL
#define USE_RSA_PUBLIC_ENCRYPT
#endif

/*
	Support for client authentication
*/

/*
	Addtional crypt support
*/
/* #define USE_MD2 */

/*
	Now that we've set up the required defines, include the crypto provider
*/
#ifdef USE_PEERSEC_CRYPTO
#include "peersec/pscrypto.h"
#endif

/******************************************************************************/
/*
	Include the public prototypes now.  This level of indirection is needed
	to properly expose the public APIs to DLLs.  The circular reference
	between these two files is avoided with the top level defines and the
	order in which they are included is the key to making this work so edit
	with caution.
*/
#include "matrixCrypto.h"


#ifdef __cplusplus
   }
#endif

#endif /* _h_CRYPTO_LAYER */

/******************************************************************************/


