/* 
 * Copyright (C) 2006 Sun Microsystems, Inc.
 *
 * This file is part of Quagga.
 *
 * Quagga is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * Quagga is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Quagga; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  
 */

#include <zebra.h>
#include "zebra/rib.h"
#include "zebra/zserv.h"

#include "zebra/redistribute.h"

void zebra_redistribute_add (int a, struct zserv *b, int c)
{ return; }
#ifdef HAVE_SYS_WEAK_ALIAS_PRAGMA
#pragma weak zebra_redistribute_delete = zebra_redistribute_add
#pragma weak zebra_redistribute_default_add = zebra_redistribute_add
#pragma weak zebra_redistribute_default_delete = zebra_redistribute_add
#else
void zebra_redistribute_delete  (int a, struct zserv *b, int c)
{ return; }
void zebra_redistribute_default_add (int a, struct zserv *b, int c)
{ return; }
void zebra_redistribute_default_delete (int a, struct zserv *b, int c)
{ return; }
#endif

void redistribute_add (struct prefix *a, struct rib *b)
{ return; }
#ifdef HAVE_SYS_WEAK_ALIAS_PRAGMA
#pragma weak redistribute_delete = redistribute_add
#else
void redistribute_delete (struct prefix *a, struct rib *b)
{ return; }
#endif

void zebra_interface_up_update (struct interface *a)
{ return; }
#ifdef HAVE_SYS_WEAK_ALIAS_PRAGMA
#pragma weak zebra_interface_down_update = zebra_interface_up_update
#pragma weak zebra_interface_add_update = zebra_interface_up_update
#pragma weak zebra_interface_delete_update = zebra_interface_up_update
#else
void zebra_interface_down_update  (struct interface *a)
{ return; }
void zebra_interface_add_update (struct interface *a)
{ return; }
void zebra_interface_delete_update (struct interface *a)
{ return; }
#endif

void zebra_interface_address_add_update (struct interface *a,
					 	struct connected *b)
{ return; }
#ifdef HAVE_SYS_WEAK_ALIAS_PRAGMA
#pragma weak zebra_interface_address_delete_update = zebra_interface_address_add_update
#else
void zebra_interface_address_delete_update (struct interface *a,
                                                struct connected *b)
{ return; }
#endif
