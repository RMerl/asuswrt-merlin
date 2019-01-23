/*
 *  Unix SMB/CIFS implementation.
 *  Group Policy Support
 *  Copyright (C) Guenther Deschner 2007
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

struct keyval_pair {
	char *key;
	char *val;
};

struct gp_inifile_context {
	TALLOC_CTX *mem_ctx;
	uint32_t keyval_count;
	struct keyval_pair **data;
	char *current_section;
	const char *generated_filename;
};

/* prototypes */

NTSTATUS gp_inifile_init_context(TALLOC_CTX *mem_ctx, uint32_t flags,
				 const char *unix_path, const char *suffix,
				 struct gp_inifile_context **ctx_ret);

NTSTATUS parse_gpt_ini(TALLOC_CTX *ctx,
		       const char *filename,
		       uint32_t *version,
		       char **display_name);
NTSTATUS gp_inifile_getstring(struct gp_inifile_context *ctx, const char *key, char **ret);
NTSTATUS gp_inifile_getint(struct gp_inifile_context *ctx, const char *key, int *ret);

