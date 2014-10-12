/* 
 *  Unix SMB/CIFS implementation.
 *  Copyright (C) Jelmer Vernooij			2005
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
#include "../lib/util/dlinklist.h"
#include "../lib/util/parmlist.h"
#include "param/param.h"
#include "param/loadparm.h"
#include "system/filesys.h"

struct param_section *param_get_section(struct param_context *ctx, const char *name)
{
	struct param_section *sect;

	if (name == NULL) 
		name = GLOBAL_NAME;

	for (sect = ctx->sections; sect; sect = sect->next) {
		if (!strcasecmp_m(sect->name, name)) 
			return sect;
	}

	return NULL;
}

struct parmlist_entry *param_section_get(struct param_section *section, const char *name)
{
	return parmlist_get(section->parameters, name);
}

struct param_section *param_add_section(struct param_context *ctx, const char *section_name)
{
	struct param_section *section;
	section = talloc_zero(ctx, struct param_section);
	if (section == NULL)
		return NULL;

	section->name = talloc_strdup(section, section_name);
	DLIST_ADD_END(ctx->sections, section, struct param_section *);
	return section;
}

/* Look up parameter. If it is not found, add it */
struct parmlist_entry *param_get_add(struct param_context *ctx, const char *name, const char *section_name)
{
	struct param_section *section;
	struct parmlist_entry *p;

	SMB_ASSERT(section_name != NULL);
	SMB_ASSERT(name != NULL);

	section = param_get_section(ctx, section_name);

	if (section == NULL) {
		section = param_add_section(ctx, section_name);
	}

	p = param_section_get(section, name);
	if (p == NULL) {
		p = talloc_zero(section, struct parmlist_entry);
		if (p == NULL)
			return NULL;

		p->key = talloc_strdup(p, name);
		DLIST_ADD_END(section->parameters->entries, p, struct parmlist_entry *);
	}
	
	return p;
}

const char *param_get_string(struct param_context *ctx, const char *param, const char *section_name)
{
	struct param_section *section = param_get_section(ctx, section_name);

	if (section == NULL)
		return NULL;

	return parmlist_get_string(section->parameters, param, NULL);
}

int param_set_string(struct param_context *ctx, const char *param, const char *value, const char *section_name)
{
	struct param_section *section = param_get_section(ctx, section_name);

	if (section == NULL)
		return -1;

	return parmlist_set_string(section->parameters, param, value);
}

const char **param_get_string_list(struct param_context *ctx, const char *param, const char *separator, const char *section_name)
{
	struct param_section *section = param_get_section(ctx, section_name);

	if (section == NULL)
		return NULL;

	return parmlist_get_string_list(section->parameters, param, separator);
}

int param_set_string_list(struct param_context *ctx, const char *param, const char **list, const char *section)
{
	struct parmlist_entry *p = param_get_add(ctx, param, section);	

	p->value = str_list_join(p, list, ' ');

	return 0;
}

int param_get_int(struct param_context *ctx, const char *param, int default_v, const char *section_name)
{
	struct param_section *section = param_get_section(ctx, section_name);

	if (section == NULL)
		return default_v;

	return parmlist_get_int(section->parameters, param, default_v);
}

void param_set_int(struct param_context *ctx, const char *param, int value, const char *section)
{
	struct parmlist_entry *p = param_get_add(ctx, section, param);

	if (!p) 
		return;

	p->value = talloc_asprintf(p, "%d", value);
}

unsigned long param_get_ulong(struct param_context *ctx, const char *param, unsigned long default_v, const char *section)
{
	const char *value = param_get_string(ctx, param, section);
	
	if (value)
		return strtoul(value, NULL, 0);

	return default_v;
}

void param_set_ulong(struct param_context *ctx, const char *name, unsigned long value, const char *section)
{
	struct parmlist_entry *p = param_get_add(ctx, name, section);

	if (!p)
		return;

	p->value = talloc_asprintf(p, "%lu", value);
}

static bool param_sfunc (const char *name, void *_ctx)
{
	struct param_context *ctx = (struct param_context *)_ctx;
	struct param_section *section = param_get_section(ctx, name);

	if (section == NULL) {
		section = talloc_zero(ctx, struct param_section);
		if (section == NULL)
			return false;

		section->name = talloc_strdup(section, name);

		DLIST_ADD_END(ctx->sections, section, struct param_section *);
	}

	/* Make sure this section is on top of the list for param_pfunc */
	DLIST_PROMOTE(ctx->sections, section);

	return true;
}

static bool param_pfunc (const char *name, const char *value, void *_ctx)
{
	struct param_context *ctx = (struct param_context *)_ctx;
	struct parmlist_entry *p = param_section_get(ctx->sections, name);

	if (!p) {
		p = talloc_zero(ctx->sections, struct parmlist_entry);
		if (p == NULL)
			return false;

		p->key = talloc_strdup(p, name);
		p->value = talloc_strdup(p, value);
		DLIST_ADD(ctx->sections->parameters->entries, p);
	} else { /* Replace current value */
		talloc_free(p->value);
		p->value = talloc_strdup(p, value);
	}

	return true;
}

struct param_context *param_init(TALLOC_CTX *mem_ctx)
{
	return talloc_zero(mem_ctx, struct param_context);
}


int param_read(struct param_context *ctx, const char *fn)
{
	ctx->sections = talloc_zero(ctx, struct param_section);
	if (ctx->sections == NULL)
		return -1;

	ctx->sections->name = talloc_strdup(ctx->sections, "global");
	if (!pm_process( fn, param_sfunc, param_pfunc, ctx)) {
		return -1;
	}

	return 0;
}

int param_use(struct loadparm_context *lp_ctx, struct param_context *ctx)
{
	struct param_section *section;

	for (section = ctx->sections; section; section = section->next) {
		struct parmlist_entry *param;
		bool isglobal = strcmp(section->name, "global") == 0;
		for (param = section->parameters->entries; param; param = param->next) {
			if (isglobal)
				lpcfg_do_global_parameter(lp_ctx, param->key,
						       param->value);
			else {
				struct loadparm_service *service = 
							lpcfg_service(lp_ctx, section->name);
				if (service == NULL)
					service = lpcfg_add_service(lp_ctx, lpcfg_default_service(lp_ctx), section->name);
				lpcfg_do_service_parameter(lp_ctx, service, param->key, param->value);
			}
		}
	}
	return 0;
}

int param_write(struct param_context *ctx, const char *fn)
{
	int file;
	struct param_section *section;

	if (fn == NULL || ctx == NULL)
		return -1;

	file = open(fn, O_WRONLY|O_CREAT, 0755);

	if (file == -1)
		return -1;
	
	for (section = ctx->sections; section; section = section->next) {
		struct parmlist_entry *param;
		
		fdprintf(file, "[%s]\n", section->name);
		for (param = section->parameters->entries; param; param = param->next) {
			fdprintf(file, "\t%s = %s\n", param->key, param->value);
		}
		fdprintf(file, "\n");
	}

	close(file);

	return 0;
}
