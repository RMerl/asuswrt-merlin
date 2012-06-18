/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Guenther Deschner                  2008.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

/*******************************************************************
 inits a structure.
********************************************************************/

void init_lsa_String(struct lsa_String *name, const char *s)
{
	name->string = s;
	name->size = 2 * strlen_m(s);
	name->length = name->size;
}

/*******************************************************************
 inits a structure.
********************************************************************/

void init_lsa_StringLarge(struct lsa_StringLarge *name, const char *s)
{
	name->string = s;
}

/*******************************************************************
 inits a structure.
********************************************************************/

void init_lsa_AsciiString(struct lsa_AsciiString *name, const char *s)
{
	name->string = s;
}

/*******************************************************************
 inits a structure.
********************************************************************/

void init_lsa_AsciiStringLarge(struct lsa_AsciiStringLarge *name, const char *s)
{
	name->string = s;
}
