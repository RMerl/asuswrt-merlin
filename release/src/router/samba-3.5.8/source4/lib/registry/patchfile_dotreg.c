/*
   Unix SMB/CIFS implementation.
   Reading .REG files

   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Wilco Baan Hofman 2006-2008

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/* FIXME Newer .REG files, created by Windows XP and above use unicode UCS-2 */

#include "includes.h"
#include "lib/registry/registry.h"
#include "system/filesys.h"

/**
 * @file
 * @brief Registry patch files
 */

#define HEADER_STRING "REGEDIT4"

struct dotreg_data {
	int fd;
	struct smb_iconv_convenience *iconv_convenience;
};

static WERROR reg_dotreg_diff_add_key(void *_data, const char *key_name)
{
	struct dotreg_data *data = (struct dotreg_data *)_data;

	fdprintf(data->fd, "\n[%s]\n", key_name);

	return WERR_OK;
}

static WERROR reg_dotreg_diff_del_key(void *_data, const char *key_name)
{
	struct dotreg_data *data = (struct dotreg_data *)_data;

	fdprintf(data->fd, "\n[-%s]\n", key_name);

	return WERR_OK;
}

static WERROR reg_dotreg_diff_set_value(void *_data, const char *path,
					const char *value_name,
					uint32_t value_type, DATA_BLOB value)
{
	struct dotreg_data *data = (struct dotreg_data *)_data;

	fdprintf(data->fd, "\"%s\"=%s:%s\n",
			value_name, str_regtype(value_type),
			reg_val_data_string(NULL, data->iconv_convenience, value_type, value));

	return WERR_OK;
}

static WERROR reg_dotreg_diff_del_value(void *_data, const char *path,
					const char *value_name)
{
	struct dotreg_data *data = (struct dotreg_data *)_data;

	fdprintf(data->fd, "\"%s\"=-\n", value_name);

	return WERR_OK;
}

static WERROR reg_dotreg_diff_done(void *_data)
{
	struct dotreg_data *data = (struct dotreg_data *)_data;

	close(data->fd);
	talloc_free(data);

	return WERR_OK;
}

static WERROR reg_dotreg_diff_del_all_values(void *callback_data,
					     const char *key_name)
{
	return WERR_NOT_SUPPORTED;
}

/**
 * Save registry diff
 */
_PUBLIC_ WERROR reg_dotreg_diff_save(TALLOC_CTX *ctx, const char *filename,
				     struct smb_iconv_convenience *iconv_convenience,
				     struct reg_diff_callbacks **callbacks,
				     void **callback_data)
{
	struct dotreg_data *data;

	data = talloc_zero(ctx, struct dotreg_data);
	*callback_data = data;

	data->iconv_convenience = iconv_convenience;

	if (filename) {
		data->fd = open(filename, O_CREAT|O_WRONLY, 0755);
		if (data->fd < 0) {
			DEBUG(0, ("Unable to open %s\n", filename));
			return WERR_BADFILE;
		}
	} else {
		data->fd = STDOUT_FILENO;
	}

	fdprintf(data->fd, "%s\n\n", HEADER_STRING);

	*callbacks = talloc(ctx, struct reg_diff_callbacks);

	(*callbacks)->add_key = reg_dotreg_diff_add_key;
	(*callbacks)->del_key = reg_dotreg_diff_del_key;
	(*callbacks)->set_value = reg_dotreg_diff_set_value;
	(*callbacks)->del_value = reg_dotreg_diff_del_value;
	(*callbacks)->del_all_values = reg_dotreg_diff_del_all_values;
	(*callbacks)->done = reg_dotreg_diff_done;

	return WERR_OK;
}

/**
 * Load diff file
 */
_PUBLIC_ WERROR reg_dotreg_diff_load(int fd,
				     struct smb_iconv_convenience *iconv_convenience,
				     const struct reg_diff_callbacks *callbacks,
				     void *callback_data)
{
	char *line, *p, *q;
	char *curkey = NULL;
	TALLOC_CTX *mem_ctx = talloc_init("reg_dotreg_diff_load");
	WERROR error;
	uint32_t value_type;
	DATA_BLOB value;

	line = afdgets(fd, mem_ctx, 0);
	if (!line) {
		DEBUG(0, ("Can't read from file.\n"));
		talloc_free(mem_ctx);
		close(fd);
		return WERR_GENERAL_FAILURE;
	}

	while ((line = afdgets(fd, mem_ctx, 0))) {
		/* Ignore comments and empty lines */
		if (strlen(line) == 0 || line[0] == ';') {
			talloc_free(line);

			if (curkey) {
				talloc_free(curkey);
			}
			curkey = NULL;
			continue;
		}

		/* Start of key */
		if (line[0] == '[') {
			p = strchr_m(line, ']');
			if (p[strlen(p)-1] != ']') {
				DEBUG(0, ("Missing ']'\n"));
				return WERR_GENERAL_FAILURE;
			}
			/* Deleting key */
			if (line[1] == '-') {
				curkey = talloc_strndup(line, line+2, strlen(line)-3);

				error = callbacks->del_key(callback_data,
							   curkey);
				if (!W_ERROR_IS_OK(error)) {
					DEBUG(0,("Error deleting key %s\n",
						curkey));
					talloc_free(mem_ctx);
					return error;
				}

				talloc_free(line);
				curkey = NULL;
				continue;
			}
			curkey = talloc_strndup(mem_ctx, line+1, strlen(line)-2);

			error = callbacks->add_key(callback_data, curkey);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0,("Error adding key %s\n", curkey));
				talloc_free(mem_ctx);
				return error;
			}

			talloc_free(line);
			continue;
		}

		/* Deleting/Changing value */
		p = strchr_m(line, '=');
		if (p == NULL) {
			DEBUG(0, ("Malformed line\n"));
			talloc_free(line);
			continue;
		}

		*p = '\0'; p++;

		if (curkey == NULL) {
			DEBUG(0, ("Value change without key\n"));
			talloc_free(line);
			continue;
		}

		/* Delete value */
		if (strcmp(p, "-") == 0) {
			error = callbacks->del_value(callback_data,
						     curkey, line);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Error deleting value %s in key %s\n",
					line, curkey));
				talloc_free(mem_ctx);
				return error;
			}

			talloc_free(line);
			continue;
		}

		q = strchr_m(p, ':');
		if (q) {
			*q = '\0';
			q++;
		}

		reg_string_to_val(line, iconv_convenience, 
				  q?p:"REG_SZ", q?q:p,
				  &value_type, &value);

		error = callbacks->set_value(callback_data, curkey, line,
					     value_type, value);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error setting value for %s in %s\n",
				line, curkey));
			talloc_free(mem_ctx);
			return error;
		}

		talloc_free(line);
	}

	close(fd);

	return WERR_OK;
}
