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
#ifndef VSF_SOLARIS_BOGONS_H
#define VSF_SOLARIS_BOGONS_H

/* This bogon ensures we get access to CMSG_DATA, CMSG_FIRSTHDR */
#define _XPG4_2

/* This bogon prevents _XPG4_2 breaking the include of signal.h! */
#define __EXTENSIONS__

/* Safe to always enable 64-bit file support. */
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1

/* Need dirfd() */
#include "dirfd_extras.h"

#endif /* VSF_SOLARIS_BOGONS_H */

