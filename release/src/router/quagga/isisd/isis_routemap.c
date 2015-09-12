/*
 * IS-IS Rout(e)ing protocol               - isis_routemap.c
 *
 * Copyright (C) 2001,2002   Sampo Saaristo
 *                           Tampere University of Technology      
 *                           Institute of Communications Engineering
 *
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

 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <zebra.h>

#include "thread.h"
#include "linklist.h"
#include "vty.h"
#include "log.h"
#include "memory.h"
#include "prefix.h"
#include "hash.h"
#include "if.h"
#include "table.h"
#include "routemap.h"

#include "isis_constants.h"
#include "isis_common.h"
#include "isis_flags.h"
#include "dict.h"
#include "isisd.h"
#include "isis_misc.h"
#include "isis_adjacency.h"
#include "isis_circuit.h"
#include "isis_tlv.h"
#include "isis_pdu.h"
#include "isis_lsp.h"
#include "isis_spf.h"
#include "isis_route.h"
#include "isis_zebra.h"

extern struct isis *isis;

/*
 * Prototypes.
 */
void isis_route_map_upd(const char *);
void isis_route_map_event(route_map_event_t, const char *);
void isis_route_map_init(void);


void
isis_route_map_upd (const char *name)
{
  int i = 0;

  if (!isis)
    return;

  for (i = 0; i <= ZEBRA_ROUTE_MAX; i++)
    {
      if (isis->rmap[i].name)
	isis->rmap[i].map = route_map_lookup_by_name (isis->rmap[i].name);
      else
	isis->rmap[i].map = NULL;
    }
  /* FIXME: do the address family sub-mode AF_INET6 here ? */
}

void
isis_route_map_event (route_map_event_t event, const char *name)
{
  int type;

  if (!isis)
    return;

  for (type = 0; type <= ZEBRA_ROUTE_MAX; type++)
    {
      if (isis->rmap[type].name && isis->rmap[type].map &&
	  !strcmp (isis->rmap[type].name, name))
	{
	  isis_distribute_list_update (type);
	}
    }
}

void
isis_route_map_init (void)
{
  route_map_init ();
  route_map_init_vty ();

  route_map_add_hook (isis_route_map_upd);
  route_map_delete_hook (isis_route_map_upd);
  route_map_event_hook (isis_route_map_event);
}
