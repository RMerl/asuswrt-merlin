/*
 *	debug.c
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
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

#include "osLayer.h"

/******************************************************************************/
/*
	Debugging APIs
*/
#ifdef DEBUG

/* message should contain one %s, unless value is NULL */
void matrixStrDebugMsg(char *message, char *value)
{
	if (value) {
		printf(message, value);
	} else {
		printf(message);
	}
}

/* message should contain one %d */
void matrixIntDebugMsg(char *message, int32 value)
{
	printf(message, value);
}

/* message should contain one %p */
void matrixPtrDebugMsg(char *message, void *value)
{
	printf(message, value);
}

#endif /* DEBUG */

/******************************************************************************/

