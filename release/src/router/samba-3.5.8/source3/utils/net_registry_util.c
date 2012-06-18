/*
 * Samba Unix/Linux SMB client library
 * Distributed SMB/CIFS Server Management Utility
 * registry utility functions
 *
 * Copyright (C) Michael Adam 2008
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "utils/net_registry_util.h"
#include "utils/net.h"

void print_registry_key(const char *keyname, NTTIME *modtime)
{
	d_printf(_("Keyname   = %s\n"), keyname);
	d_printf(_("Modtime   = %s\n"),
		 modtime
		 ? http_timestring(talloc_tos(), nt_time_to_unix(*modtime))
		 : _("None"));
	d_printf("\n");
}

void print_registry_value(const struct registry_value *valvalue, bool raw)
{
	if (!raw) {
		d_printf(_("Type       = %s\n"),
			 reg_type_lookup(valvalue->type));
	}
	switch(valvalue->type) {
	case REG_DWORD:
		if (!raw) {
			d_printf(_("Value      = "));
		}
		d_printf("%d\n", valvalue->v.dword);
		break;
	case REG_SZ:
	case REG_EXPAND_SZ:
		if (!raw) {
			d_printf(_("Value      = \""));
		}
		d_printf("%s", valvalue->v.sz.str);
		if (!raw) {
			d_printf("\"");
		}
		d_printf("\n");
		break;
	case REG_MULTI_SZ: {
		uint32 j;
		for (j = 0; j < valvalue->v.multi_sz.num_strings; j++) {
			if (!raw) {
				d_printf(_("Value[%3.3d] = \""), j);
			}
			d_printf("%s", valvalue->v.multi_sz.strings[j]);
			if (!raw) {
				d_printf("\"");
			}
			d_printf("\n");
		}
		break;
	}
	case REG_BINARY:
		if (!raw) {
			d_printf(_("Value      = "));
		}
		d_printf(_("%d bytes\n"), (int)valvalue->v.binary.length);
		break;
	default:
		if (!raw) {
			d_printf(_("Value      = "));
		}
		d_printf(_("<unprintable>\n"));
		break;
	}
}

void print_registry_value_with_name(const char *valname,
				    const struct registry_value *valvalue)
{
	d_printf(_("Valuename  = %s\n"), valname);
	print_registry_value(valvalue, false);
	d_printf("\n");
}

/**
 * Split path into hive name and subkeyname
 * normalizations performed:
 *  - convert '/' to '\\'
 *  - strip trailing '\\' chars
 */
WERROR split_hive_key(TALLOC_CTX *ctx, const char *path, char **hivename,
		      char **subkeyname)
{
	char *p;
	const char *tmp_subkeyname;

	if ((path == NULL) || (hivename == NULL) || (subkeyname == NULL)) {
		return WERR_INVALID_PARAM;
	}

	if (strlen(path) == 0) {
		return WERR_INVALID_PARAM;
	}

	*hivename = talloc_string_sub(ctx, path, "/", "\\");
	if (*hivename == NULL) {
		return WERR_NOMEM;
	}

	/* strip trailing '\\' chars */
	p = strrchr(*hivename, '\\');
	while ((p != NULL) && (p[1] == '\0')) {
		*p = '\0';
		p = strrchr(*hivename, '\\');
	}

	p = strchr(*hivename, '\\');

	if ((p == NULL) || (*p == '\0')) {
		/* just the hive - no subkey given */
		tmp_subkeyname = "";
	} else {
		*p = '\0';
		tmp_subkeyname = p+1;
	}
	*subkeyname = talloc_strdup(ctx, tmp_subkeyname);
	if (*subkeyname == NULL) {
		return WERR_NOMEM;
	}

	return WERR_OK;
}
