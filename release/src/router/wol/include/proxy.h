/*
 * wol - wake on lan client
 *
 * $Id: proxy.h,v 1.1 2004/02/05 18:19:54 wol Exp $
 *
 * Copyright (C) 2004 Thomas Krennwallner <krennwallner@aon.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */


#ifndef _PROXY_H
#define _PROXY_H

int proxy_send (int sock,
		const char *macbuf,
		const char *passwd);

#endif /* _PROXY_H */
