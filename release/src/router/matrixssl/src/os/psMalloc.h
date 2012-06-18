/*
 *	psMalloc.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *	
 *	Header for psMalloc functions
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

#ifndef _h_PS_MALLOC
#define _h_PS_MALLOC

#define PEERSEC_BASE_POOL 0
#define PEERSEC_NO_POOL	(void *)0x00001

/******************************************************************************/
/*
	Because a set of public APIs are exposed here there is a dependence on
	the package.  The config layer header must be parsed to determine what
	defines are configured
*/
#include "../../matrixCommon.h"

/*
	Native memory routines
*/
#define MAX_MEMORY_USAGE	0
#define psOpenMalloc(A) 0
#define psCloseMalloc()
#define psMalloc(A, B)		malloc(B)
#define psCalloc(A, B, C)	calloc(B, C)
#define psRealloc	realloc
#define psFree		free
#define psMemset	memset
#define psMemcpy	memcpy
typedef int32 psPool_t;

#endif /* _h_PS_MALLOC */



