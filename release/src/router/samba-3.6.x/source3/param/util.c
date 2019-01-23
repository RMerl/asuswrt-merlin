/*
 *  Unix SMB/CIFS implementation.
 *  param helper routines
 *  Copyright (C) Gerald Carter                2003
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

/*********************************************************
 utility function to parse an integer parameter from
 "parameter = value"
**********************************************************/
uint32 get_int_param( const char* param )
{
	char *p;

	p = strchr( param, '=' );
	if ( !p )
		return 0;

	return atoi(p+1);
}

/*********************************************************
 utility function to parse an integer parameter from
 "parameter = value"
**********************************************************/
char* get_string_param( const char* param )
{
	char *p;

	p = strchr( param, '=' );
	if ( !p )
		return NULL;

	return (p+1);
}
