/*
 * IS-IS Rout(e)ing protocol - isis_flags.c
 *                             Routines for manipulation of SSN and SRM flags
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
 * This program is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU General Public Licenseas published by the Free 
 * Software Foundation; either version 2 of the License, or (at your option) 
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT 
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 * more details.
 *
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <zebra.h>
#include "log.h"
#include "linklist.h"

#include "isisd/isis_constants.h"
#include "isisd/isis_common.h"
#include "isisd/isis_flags.h"

void
flags_initialize (struct flags *flags)
{
  flags->maxindex = 0;
  flags->free_idcs = NULL;
}

long int
flags_get_index (struct flags *flags)
{
  struct listnode *node;
  long int index;

  if (flags->free_idcs == NULL || flags->free_idcs->count == 0)
    {
      index = flags->maxindex++;
    }
  else
    {
      node = listhead (flags->free_idcs);
      index = (long int) listgetdata (node);
      listnode_delete (flags->free_idcs, (void *) index);
      index--;
    }

  return index;
}

void
flags_free_index (struct flags *flags, long int index)
{
  if (index + 1 == flags->maxindex)
    {
      flags->maxindex--;
      return;
    }

  if (flags->free_idcs == NULL)
    {
      flags->free_idcs = list_new ();
    }

  listnode_add (flags->free_idcs, (void *) (index + 1));

  return;
}

int
flags_any_set (u_int32_t * flags)
{
  u_int32_t zero[ISIS_MAX_CIRCUITS];
  memset (zero, 0x00, ISIS_MAX_CIRCUITS * 4);

  return bcmp (flags, zero, ISIS_MAX_CIRCUITS * 4);
}
