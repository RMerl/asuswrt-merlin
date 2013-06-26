/*
 *  mapfile.c
 *
 *  Copyright (C) Gerald Carter  <jerry@samba.org>
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
 *
 */

#include "includes.h"
#include "system/filesys.h"
#include "winbindd/winbindd.h"
#include "idmap.h"
#include "idmap_hash.h"
#include <stdio.h>

XFILE *lw_map_file = NULL;

/*********************************************************************
 ********************************************************************/

static bool mapfile_open(void)
{
	const char *mapfile_name = NULL;

	/* If we have an open handle, just reset it */

	if (lw_map_file) {
		return (x_tseek(lw_map_file, 0, SEEK_SET) == 0);
	}

	mapfile_name = lp_parm_const_string(-1, "idmap_hash", "name_map", NULL);
	if (!mapfile_name) {
		return false;
	}

	lw_map_file = x_fopen(mapfile_name, O_RDONLY, 0);
	if (!lw_map_file) {
		DEBUG(0,("can't open idmap_hash:name_map (%s). Error %s\n",
			 mapfile_name, strerror(errno) ));
		return false;
	}

	return true;
}

/*********************************************************************
 ********************************************************************/

static bool mapfile_read_line(fstring key, fstring value)
{
	char buffer[1024];
	char *p;
	int len;

	if (!lw_map_file)
		return false;

	if ((p = x_fgets(buffer, sizeof(buffer)-1, lw_map_file)) == NULL) {
		return false;
	}

	/* Strip newlines and carriage returns */

	len = strlen_m(buffer) - 1;
	while ((buffer[len] == '\n') || (buffer[len] == '\r')) {
		buffer[len--] = '\0';
	}


	if ((p = strchr_m(buffer, '=')) == NULL ) {
		DEBUG(0,("idmap_hash: Bad line in name_map (%s)\n", buffer));
		return false;
	}

	*p = '\0';
	p++;

	fstrcpy(key, buffer);
	fstrcpy(value, p);

	/* Eat whitespace */

	if (!trim_char(key, ' ', ' '))
		return false;

	if (!trim_char(value, ' ', ' '))
		return false;

	return true;
}

/*********************************************************************
 ********************************************************************/

static bool mapfile_close(void)
{
	int ret = 0;
	if (lw_map_file) {
		ret = x_fclose(lw_map_file);
		lw_map_file = NULL;
	}

	return (ret == 0);
}


/*********************************************************************
 ********************************************************************/

NTSTATUS mapfile_lookup_key(TALLOC_CTX *ctx, const char *value, char **key)
{
	fstring r_key, r_value;
	NTSTATUS ret = NT_STATUS_NOT_FOUND;

	if (!mapfile_open())
		return NT_STATUS_OBJECT_PATH_NOT_FOUND;

	while (mapfile_read_line(r_key, r_value))
	{
		if (strequal(r_value, value)) {
			ret = NT_STATUS_OK;

			/* We're done once finishing this block */
			*key = talloc_strdup(ctx, r_key);
			if (!*key) {
				ret = NT_STATUS_NO_MEMORY;
			}
			break;
		}
	}

	mapfile_close();

	return ret;
}

/*********************************************************************
 ********************************************************************/

NTSTATUS mapfile_lookup_value(TALLOC_CTX *ctx, const char *key, char **value)
{
	fstring r_key, r_value;
	NTSTATUS ret = NT_STATUS_NOT_FOUND;

	if (!mapfile_open())
		return NT_STATUS_OBJECT_PATH_NOT_FOUND;

	while (mapfile_read_line(r_key, r_value))
	{
		if (strequal(r_key, key)) {
			ret = NT_STATUS_OK;

			/* We're done once finishing this block */
			*value = talloc_strdup(ctx, r_value);
			if (!*key) {
				ret = NT_STATUS_NO_MEMORY;
			}
			break;
		}
	}

	mapfile_close();

	return ret;
}
