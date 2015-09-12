/*
 * IS-IS Rout(e)ing protocol - isis_dynhn.h
 *                             Dynamic hostname cache
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
#ifndef _ZEBRA_ISIS_DYNHN_H
#define _ZEBRA_ISIS_DYNHN_H

struct isis_dynhn
{
  u_char id[ISIS_SYS_ID_LEN];
  struct hostname name;
  time_t refresh;
  int level;
};

void dyn_cache_init (void);
void isis_dynhn_insert (u_char * id, struct hostname *hostname, int level);
void isis_dynhn_remove (u_char * id);
struct isis_dynhn *dynhn_find_by_id (u_char * id);
struct isis_dynhn *dynhn_find_by_name (const char *hostname);
void dynhn_print_all (struct vty *vty);

#endif /* _ZEBRA_ISIS_DYNHN_H */
