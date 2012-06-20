/*
 *	wol - wake on lan client
 *
 *	create and assemble magic packets
 *
 *	$Id: magic.c,v 1.9 2003/08/12 16:45:22 wol Exp $
 *
 *	Copyright (C) 2000-2003 Thomas Krennwallner <krennwallner@aon.at>
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
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> /* memset() in HP-UX */

#include "wrappers.h"
#include "xalloc.h"
#include "wol.h"
#include "magic.h"



/*
 *	How a Magic Packet Frame looks like:
 *
 *	 _________________________________________
 *	| 0xff | 0xff | 0xff | 0xff | 0xff | 0xff |
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	|MAC[0]|MAC[1]|MAC[2]|MAC[3]|MAC[4]|MAC[5]|
 *	 -----------------------------------------
 *	 optional SecureON (tm) password:
 *	 _________________________________________
 *	|PASS0 |PASS1 |PASS2 |PASS3 |PASS4 |PASS5 |
 *	 -----------------------------------------
 */


/* struct for typecasting a normal magic packet */
struct
packet
{
	unsigned char header[MAGIC_HEADER];
	unsigned char addr[MAGIC_TIMES][MAC_LEN];
};

/* struct for typecasting a SecureON magic packet */
struct
secureon
{
	unsigned char header[MAGIC_HEADER];
	unsigned char addr[MAGIC_TIMES][MAC_LEN];
	unsigned char passwd[MAGIC_SECUREON];
};



struct magic *
magic_create (int with_passwd)
{
	struct magic *mag;

	mag = (struct magic *) xmalloc (sizeof (struct magic));

	if (with_passwd)
		mag->size = sizeof (struct secureon);
	else
		mag->size = sizeof (struct packet);

	mag->packet = (unsigned char *) xmalloc (mag->size);

	return mag;
}



void
magic_destroy (struct magic *m)
{
	XFREE ((void *) m->packet);
	XFREE ((void *) m);
}



int
magic_assemble (struct magic *magic_buf, const char *mac_str,
								const char *passwd_str)
{
	int m[MAC_LEN];
	int j, k;


	if (mac_str == NULL || magic_buf == NULL) return -1;

	/* split the MAC address string into it's hex components */
	if (sscanf (mac_str, "%2x:%2x:%2x:%2x:%2x:%2x",
							&m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != MAC_LEN)
		{
			struct ether_addr ea;

			/* lets parse /etc/ethers for ethernet name resolving
			 * FIXME: ether_hostton() is not implemented on every platform
			 */
			if (ether_hostton (mac_str, &ea))
				{
					errno = EINVAL;
					return -1;
				}

			for (j = 0; j < MAC_LEN; ++j)
				m[j] = ea.ETHER_ADDR_OCTET[j];
		}

	/* accommodate the packet chunk's size to the packet type */
	if (passwd_str && magic_buf->size != sizeof (struct secureon))
		{
			magic_buf->packet = \
									(unsigned char *) xrealloc ((void *) magic_buf->packet,
																							sizeof (struct secureon));
			magic_buf->size = sizeof (struct secureon);
		}
	else if (passwd_str == NULL && magic_buf->size != sizeof (struct packet))
		{
			magic_buf->packet = \
									(unsigned char *) xrealloc ((void *) magic_buf->packet,
																							sizeof (struct packet));
			magic_buf->size = sizeof (struct packet);
		}


	/* assemble magic packet header */
	memset ((void *) magic_buf->packet, 0xff, MAGIC_HEADER);

	/* and now the data */
	for (j = 0; j < MAGIC_TIMES; j++)
		{
			for (k = 0; k < MAC_LEN; k++)
				((struct packet *) magic_buf->packet)->addr[j][k] = \
																							(unsigned char) m[k];
		}

	/* add the SecureON passwd */
	if (passwd_str)
		{
			int s[MAGIC_SECUREON];

			/* split the password string into it's hex components */
			if (sscanf (passwd_str, "%2x-%2x-%2x-%2x-%2x-%2x",
									&s[0], &s[1], &s[2], &s[3], &s[4], &s[5]) != MAGIC_SECUREON)
				{
					errno = EINVAL;
					return -2;
				}

			for (j = 0; j < MAGIC_SECUREON; j++)
				((struct secureon *) magic_buf->packet)->passwd[j] = \
																									(unsigned char) s[j];
		}

	return 0;
}
