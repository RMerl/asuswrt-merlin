///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2000-2003 Intel Corporation 
// All rights reserved. 
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met: 
//
// * Redistributions of source code must retain the above copyright notice, 
// this list of conditions and the following disclaimer. 
// * Redistributions in binary form must reproduce the above copyright notice, 
// this list of conditions and the following disclaimer in the documentation 
// and/or other materials provided with the distribution. 
// * Neither name of Intel Corporation nor the names of its contributors 
// may be used to endorse or promote products derived from this software 
// without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#ifndef GENLIB_UTIL_STRINTMAP_H
#define GENLIB_UTIL_STRINTMAP_H

#include <stdlib.h>
#include "util.h"

// Util to map from a string to an integer and vice versa

typedef struct // str_int_entry
{
	char *name;		// a value in string form
	int  id;		// same value in integer form
} str_int_entry;

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************
*	Function :	map_str_to_int
*
*	Parameters :
*		IN const char* name ;	string containing the name to be matched
*		IN size_t name_len ;	size of the string to be matched
*		IN str_int_entry* table ;	table of entries that need to be 
*					matched.
*		IN int num_entries ; number of entries in the table that need 
*					to be searched.
*		IN xboolean case_sensitive ; whether the case should be case
*					sensitive or not
*
*	Description : Match the given name with names from the entries in the 
*		table. Returns the index of the table when the entry is found.
*
*	Return : int ;
*		index - On Success
*		-1 - On failure
*
*	Note :
************************************************************************/
int map_str_to_int( IN const char* name, IN size_t name_len,
		IN str_int_entry* table, IN int num_entries, 
		IN xboolean case_sensitive );


/************************************************************************
*	Function :	map_int_to_str
*
*	Parameters :
*		IN int id ;	ID to be matched
*		IN str_int_entry* table ;	table of entries that need to be 
*					matched.
*		IN int num_entries ; number of entries in the table that need 
*					to be searched.
*
*	Description : Returns the index from the table where the id matches 
*		the entry from the table.
*
*	Return : int ;
*
*	Note :
************************************************************************/
int map_int_to_str( IN int id, IN str_int_entry* table,
		IN int num_entries );

#ifdef __cplusplus
} // extern C
#endif


#endif // GENLIB_UTIL_STRINTMAP_H
