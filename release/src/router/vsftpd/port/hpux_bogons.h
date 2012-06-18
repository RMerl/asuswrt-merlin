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
#ifndef VSF_HPUX_BOGONS_H
#define VSF_HPUX_BOGONS_H

#include <sys/mman.h>
/* HP-UX has MAP_ANONYMOUS but not MAP_ANON - I'm not sure which is more
 * standard!
 */
#ifdef MAP_ANONYMOUS
  #ifndef MAP_ANON
    #define MAP_ANON MAP_ANONYMOUS
  #endif
#endif

/* Ancient versions of HP-UX don't have MAP_FAILED */
#ifndef MAP_FAILED
  #define MAP_FAILED (void *) -1L
#endif

/* Need dirfd() */
#include "dirfd_extras.h"

#endif /* VSF_HPUX_BOGONS_H */

