/*
   Unix SMB/CIFS implementation.
   No support for quotas :-).
   Copyright (C) Andrew Tridgell 1992-1998

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"

/*
 * Needed for auto generation of proto.h.
 */

bool disk_quotas(const char *path,uint64_t *bsize,uint64_t *dfree,uint64_t *dsize)
{
  (*bsize) = 512; /* This value should be ignored */

  /* And just to be sure we set some values that hopefully */
  /* will be larger that any possible real-world value     */
  (*dfree) = (uint64_t)-1;
  (*dsize) = (uint64_t)-1;

  /* As we have select not to use quotas, allways fail */
  return False;
}
