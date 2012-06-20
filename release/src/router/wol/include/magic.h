/*
 *	wol - wake on lan client
 *
 *	$Id: magic.h,v 1.3 2003/08/10 08:21:28 wol Exp $
 *
 *	Copyright (C) 2000-2002 Thomas Krennwallner <krennwallner@aon.at>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *	USA.
 */

#ifndef _MAGIC_H
#define _MAGIC_H

#include <sys/types.h>

#define MAGIC_HEADER    6
#define MAC_LEN         6
#define MAGIC_TIMES    16
#define MAGIC_SECUREON  6



struct
magic
{
	unsigned char *packet;
	size_t size;
};



struct magic *magic_create (int with_passwd);


void magic_destroy (struct magic *m);


int magic_assemble (struct magic *magic_buf, const char *mac_str,
										const char *secureon);


#endif /* _MAGIC_H */
