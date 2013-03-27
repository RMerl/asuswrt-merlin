/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _DOUBLE_C_
#define _DOUBLE_C_

#include "basics.h"

#define SFtype unsigned32
#define DFtype unsigned64

#define HItype signed16
#define SItype signed32
#define DItype signed64

#define UHItype unsigned16
#define USItype unsigned32
#define UDItype unsigned64


#define US_SOFTWARE_GOFAST
#include "dp-bit.c"

#endif
