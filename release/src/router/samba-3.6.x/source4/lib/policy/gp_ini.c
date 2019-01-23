
/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Object Support
 *  Copyright (C) Wilco Baan Hofman 2010
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
#include "lib/util/util.h"
#include "lib/policy/policy.h"

struct gp_parse_context {
	struct gp_ini_context *ini;
	int32_t cur_section;
};


static bool gp_add_ini_section(const char *name, void *callback_data)
{
	struct gp_parse_context *parse = callback_data;
	struct gp_ini_context *ini = parse->ini;

	ini->sections = talloc_realloc(ini, ini->sections, struct gp_ini_section, ini->num_sections+1);
	if (ini->sections == NULL) return false;
	ini->sections[ini->num_sections].name = talloc_strdup(ini, name);
	if (ini->sections[ini->num_sections].name == NULL) return false;
	parse->cur_section = ini->num_sections;
	ini->num_sections++;

	return true;
}

static bool gp_add_ini_param(const char *name, const char *value, void *callback_data)
{
	struct gp_parse_context *parse = callback_data;
	struct gp_ini_context *ini = parse->ini;
	struct gp_ini_section *section;

	if (parse->cur_section == -1) {
		return false;
	}

	section = &ini->sections[parse->cur_section];

	section->params = talloc_realloc(ini, ini->sections[parse->cur_section].params, struct gp_ini_param, section->num_params+1);
	if (section->params == NULL) return false;
	section->params[section->num_params].name = talloc_strdup(ini, name);
	if (section->params[section->num_params].name == NULL) return false;
	section->params[section->num_params].value = talloc_strdup(ini, value);
	if (section->params[section->num_params].value == NULL) return false;
	section->num_params++;

	return true;
}

NTSTATUS gp_parse_ini(TALLOC_CTX *mem_ctx, struct gp_context *gp_ctx, const char *filename, struct gp_ini_context **ret)
{
	struct gp_parse_context parse;
	bool rv;

	parse.ini = talloc_zero(mem_ctx, struct gp_ini_context);
	NT_STATUS_HAVE_NO_MEMORY(parse.ini);
	parse.cur_section = -1;

	rv = pm_process(filename, gp_add_ini_section, gp_add_ini_param, &parse);
	if (!rv) {
		DEBUG(0, ("Error while processing ini file %s\n", filename));
		return NT_STATUS_UNSUCCESSFUL;
	}

	*ret = parse.ini;
	return NT_STATUS_OK;
}

NTSTATUS gp_get_ini_string(struct gp_ini_context *ini, const char *section, const char *name, char **ret)
{
	uint16_t i;
	int32_t cur_sec = -1;
	for (i = 0; i < ini->num_sections; i++) {
		if (strcmp(ini->sections[i].name, section) == 0) {
			cur_sec = i;
			break;
		}
	}

	if (cur_sec == -1) {
		return NT_STATUS_NOT_FOUND;
	}

	for (i = 0; i < ini->sections[cur_sec].num_params; i++) {
		if (strcmp(ini->sections[cur_sec].params[i].name, name) == 0) {
			*ret = ini->sections[cur_sec].params[i].value;
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_NOT_FOUND;
}

NTSTATUS gp_get_ini_uint(struct gp_ini_context *ini, const char *section, const char *name, uint32_t *ret)
{
	uint16_t i;
	int32_t cur_sec = -1;
	for (i = 0; i < ini->num_sections; i++) {
		if (strcmp(ini->sections[i].name, section) == 0) {
			cur_sec = i;
			break;
		}
	}

	if (cur_sec == -1) {
		return NT_STATUS_NOT_FOUND;
	}

	for (i = 0; i < ini->sections[cur_sec].num_params; i++) {
		if (strcmp(ini->sections[cur_sec].params[i].name, name) == 0) {
			*ret = atol(ini->sections[cur_sec].params[i].value);
			return NT_STATUS_OK;
		}
	}
	return NT_STATUS_NOT_FOUND;
}
