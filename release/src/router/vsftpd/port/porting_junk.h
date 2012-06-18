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
#ifndef VSF_PORTINGJUNK_H
#define VSF_PORTINGJUNK_H

#ifdef __sun
#include "solaris_bogons.h"
#endif

#ifdef __sgi
#include "irix_bogons.h"
#endif

#ifdef __hpux
#include "hpux_bogons.h"
#endif

#ifdef _AIX
#include "aix_bogons.h"
#endif

#ifdef __osf__
#include "tru64_bogons.h"
#endif

/* So many older systems lack these, that it's too much hassle to list all
 * the errant systems
 */
#include "cmsg_extras.h"

#endif /* VSF_PORTINGJUNK_H */

