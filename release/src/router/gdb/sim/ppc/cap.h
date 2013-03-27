/*  This file is part of the program psim.

    Copyright (C) 1994-1995,1997, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


/* Export a capability data base that maps between internal data
   values and those given to a simulation */

#ifndef _CAP_H_
#define _CAP_H_

#include "basics.h"

typedef struct _cap cap;

INLINE_CAP\
(cap *) cap_create
(const char *name);

INLINE_CAP\
(void) cap_init
(cap *db);

INLINE_CAP\
(signed_cell) cap_external
(cap *db,
 void *internal);

INLINE_CAP\
(void *) cap_internal
(cap *db,
 signed_cell external);

INLINE_CAP\
(void) cap_add
(cap *db,
 void *internal);

INLINE_CAP\
(void) cap_remove
(cap *db,
 void *internal);

#endif
