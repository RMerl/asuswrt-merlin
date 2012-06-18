/* 
   Unix SMB/CIFS implementation.
   Samba utility functions
   Copyright (C) Andrew Tridgell 1992-2001
   Copyright (C) Simo Sorce 2001
   
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
#include "system/locale.h"
#include "dynconfig/dynconfig.h"

/**
 * @file
 * @brief Unicode string manipulation
 */

/* these 2 tables define the unicode case handling.  They are loaded
   at startup either via mmap() or read() from the lib directory */
static void *upcase_table;
static void *lowcase_table;


/*******************************************************************
load the case handling tables
********************************************************************/
void load_case_tables(void)
{
	TALLOC_CTX *mem_ctx;

	mem_ctx = talloc_init("load_case_tables");
	if (!mem_ctx) {
		smb_panic("No memory for case_tables");
	}
	upcase_table = map_file(talloc_asprintf(mem_ctx, "%s/upcase.dat", dyn_DATADIR), 0x20000);
	lowcase_table = map_file(talloc_asprintf(mem_ctx, "%s/lowcase.dat", dyn_DATADIR), 0x20000);
	talloc_free(mem_ctx);
	if (upcase_table == NULL) {
		/* try also under codepages for testing purposes */
		upcase_table = map_file("codepages/upcase.dat", 0x20000);
		if (upcase_table == NULL) {
			upcase_table = (void *)-1;
		}
	}
	if (lowcase_table == NULL) {
		/* try also under codepages for testing purposes */
		lowcase_table = map_file("codepages/lowcase.dat", 0x20000);
		if (lowcase_table == NULL) {
			lowcase_table = (void *)-1;
		}
	}
}

/**
 Convert a codepoint_t to upper case.
**/
_PUBLIC_ codepoint_t toupper_m(codepoint_t val)
{
	if (val < 128) {
		return toupper(val);
	}
	if (upcase_table == NULL) {
		load_case_tables();
	}
	if (upcase_table == (void *)-1) {
		return val;
	}
	if (val & 0xFFFF0000) {
		return val;
	}
	return SVAL(upcase_table, val*2);
}

/**
 Convert a codepoint_t to lower case.
**/
_PUBLIC_ codepoint_t tolower_m(codepoint_t val)
{
	if (val < 128) {
		return tolower(val);
	}
	if (lowcase_table == NULL) {
		load_case_tables();
	}
	if (lowcase_table == (void *)-1) {
		return val;
	}
	if (val & 0xFFFF0000) {
		return val;
	}
	return SVAL(lowcase_table, val*2);
}

/**
  compare two codepoints case insensitively
*/
_PUBLIC_ int codepoint_cmpi(codepoint_t c1, codepoint_t c2)
{
	if (c1 == c2 ||
	    toupper_m(c1) == toupper_m(c2)) {
		return 0;
	}
	return c1 - c2;
}


