/*
 *  Unix SMB/CIFS implementation.
 *  RPC Pipe client / server routines
 *  Copyright (C) Andrew Tridgell              1992-2000,
 *  Copyright (C) Jean François Micouleau      1998-2000.
 *  Copyright (C) Gerald Carter                2002-2005.
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
#include "printing/nt_printing_os2.h"

/****************************************************************************
 ***************************************************************************/

static char *win_driver;
static char *os2_driver;

static const char *get_win_driver(void)
{
	if (win_driver == NULL) {
		return "";
	}
	return win_driver;
}

static const char *get_os2_driver(void)
{
	if (os2_driver == NULL) {
		return "";
	}
	return os2_driver;
}

static bool set_driver_mapping(const char *from, const char *to)
{
	SAFE_FREE(win_driver);
	SAFE_FREE(os2_driver);

	win_driver = SMB_STRDUP(from);
	os2_driver = SMB_STRDUP(to);

	if (win_driver == NULL || os2_driver == NULL) {
		SAFE_FREE(win_driver);
		SAFE_FREE(os2_driver);
		return false;
	}
	return true;
}

/**
 * @internal
 *
 * @brief Map a Windows driver to a OS/2 driver.
 *
 * @param[in]  mem_ctx  The memory context to use.
 *
 * @param[in,out] pdrivername The drivername of Windows to remap.
 *
 * @return              WERR_OK on success, a corresponding WERROR on failure.
 */
WERROR spoolss_map_to_os2_driver(TALLOC_CTX *mem_ctx, const char **pdrivername)
{
	const char *mapfile = lp_os2_driver_map();
	char **lines = NULL;
	const char *drivername;
	int numlines = 0;
	int i;

	if (pdrivername == NULL || *pdrivername == NULL || *pdrivername[0] == '\0') {
		return WERR_INVALID_PARAMETER;
	}

	drivername = *pdrivername;

	if (mapfile[0] == '\0') {
		return WERR_BADFILE;
	}

	if (strequal(drivername, get_win_driver())) {
		DEBUG(3,("Mapped Windows driver %s to OS/2 driver %s\n",
			drivername, get_os2_driver()));
		drivername = talloc_strdup(mem_ctx, get_os2_driver());
		if (drivername == NULL) {
			return WERR_NOMEM;
		}
		*pdrivername = drivername;
		return WERR_OK;
	}

	lines = file_lines_load(mapfile, &numlines, 0, NULL);
	if (numlines == 0 || lines == NULL) {
		DEBUG(0,("No entries in OS/2 driver map %s\n", mapfile));
		TALLOC_FREE(lines);
		return WERR_EMPTY;
	}

	DEBUG(4,("Scanning OS/2 driver map %s\n",mapfile));

	for( i = 0; i < numlines; i++) {
		char *nt_name = lines[i];
		char *os2_name = strchr(nt_name, '=');

		if (os2_name == NULL) {
			continue;
		}

		*os2_name++ = '\0';

		while (isspace(*nt_name)) {
			nt_name++;
		}

		if (*nt_name == '\0' || strchr("#;", *nt_name)) {
			continue;
		}

		{
			int l = strlen(nt_name);
			while (l && isspace(nt_name[l - 1])) {
				nt_name[l - 1] = 0;
				l--;
			}
		}

		while (isspace(*os2_name)) {
			os2_name++;
		}

		{
			int l = strlen(os2_name);
			while (l && isspace(os2_name[l-1])) {
				os2_name[l-1] = 0;
				l--;
			}
		}

		if (strequal(nt_name, drivername)) {
			DEBUG(3,("Mapped Windows driver %s to OS/2 driver %s\n",drivername,os2_name));
			set_driver_mapping(drivername, os2_name);
			drivername = talloc_strdup(mem_ctx, os2_name);
			TALLOC_FREE(lines);
			if (drivername == NULL) {
				return WERR_NOMEM;
			}
			*pdrivername = drivername;
			return WERR_OK;
		}
	}

	TALLOC_FREE(lines);
	return WERR_OK;
}
