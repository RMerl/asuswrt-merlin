/*
 *	matrixConfig.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	Configuration settings for building the MatrixSSL library.
 *	These options affect the size and algorithms present in the library.
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

#ifndef _h_MATRIXCONFIG
#define _h_MATRIXCONFIG

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
/*
	Define the number of sessions to cache here.
	Minimum value is 1
	Session caching provides such an increase in performance that it isn't
	an option to disable.
*/
#define SSL_SESSION_TABLE_SIZE	32

/******************************************************************************/
/*
	Define the following to enable various cipher suites
	At least one of these must be defined.  If multiple are defined,
	the handshake will determine which is best for the connection.
*/
#define USE_SSL_RSA_WITH_RC4_128_MD5
#define USE_SSL_RSA_WITH_RC4_128_SHA
#define USE_SSL_RSA_WITH_3DES_EDE_CBC_SHA

/******************************************************************************/
/*
	Support for encrypted private key files, using 3DES
*/
#define USE_ENCRYPTED_PRIVATE_KEYS

/******************************************************************************/
/*
	Support for client side SSL
*/
#define USE_CLIENT_SIDE_SSL
#define USE_SERVER_SIDE_SSL


/******************************************************************************/
/*
	Use native 64 bit integers (long longs)
*/
#define USE_INT64

/******************************************************************************/
/*
	Hi-res POSIX timer.  Use rdtscll() for timing routines in linux.c
*/
/* #define USE_RDTSCLL_TIME */

/******************************************************************************/
/*
	Support for multithreading environment.  This should be enabled
	if multiple SSL sessions will be active at the same time in 
	different threads. The library will serialize access to the session 
	cache and memory pools with a mutex.
	By default this is off, so that on POSIX platforms, pthreads isn't req'd
*/
#define USE_MULTITHREADING

/******************************************************************************/
/*
	Support for file system.
*/
#define USE_FILE_SYSTEM


/******************************************************************************/
/*
    Allow servers to proceed with rehandshakes.

    SECURITY: A protocol flaw has been demonstrated in which an "authentication
    gap" is possible during rehandshakes that enable a man-in-the-middle to
    inject plain-text HTTP traffic into an authenticated client-server session

    It is advised to leave this disabled if you are using HTTPS
*/
/* #define ALLOW_SERVER_REHANDSHAKES */

#ifdef __cplusplus
}
#endif

#endif /* _h_MATRIXCONFIG */

/******************************************************************************/

