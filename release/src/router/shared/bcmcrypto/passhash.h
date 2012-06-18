/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * passhash.h
 * Perform password->key hash algorithm as defined in WPA and 802.11i
 * specifications
 *
 * Copyright 2002, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.; the
 * contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#ifndef _PASSHASH_H_
#define _PASSHASH_H_

/* passhash: perform passwork->key hash algorithm as defined in WPA and 802.11i
	specifications
	password is an ascii string of 8 to 63 characters in length
	ssid is up to 32 bytes
	ssidlen is the length of ssid in bytes
	output must be at lest 40 bytes long, and returns a 256 bit key
	returns 0 on success, non-zero on failure */
int passhash(char *password, int passlen, unsigned char *ssid, int ssidlen, unsigned char *output);

#endif /* _PASSHASH_H_ */

