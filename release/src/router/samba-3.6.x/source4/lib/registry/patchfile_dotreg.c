/*
   Unix SMB/CIFS implementation.
   Reading .REG files

   Copyright (C) Jelmer Vernooij 2004-2007
   Copyright (C) Wilco Baan Hofman 2006-2010

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

/* FIXME:
 * - Newer .REG files, created by Windows XP and above use unicode UCS-2
 * - @="" constructions should write value with empty name.
*/

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
};

/* 
 * This is basically a copy of data_blob_hex_string_upper, but with comma's 
 * between the bytes in hex.
 */
static char *dotreg_data_blob_hex_string(TALLOC_CTX *mem_ctx, const DATA_BLOB *blob)
{
	size_t i;
	char *hex_string;

	hex_string = talloc_array(mem_ctx, char, (blob->length*3)+1);
	if (!hex_string) {
		return NULL;
	}

	for (i = 0; i < blob->length; i++)
		slprintf(&hex_string[i*3], 4, "%02X,", blob->data[i]);

	/* Remove last comma and NULL-terminate the string */
	hex_string[(blob->length*3)-1] = '\0';
	return hex_string;
}

/* 
 * This is basically a copy of reg_val_data_string, except that this function
 * has no 0x for dwords, everything else is regarded as binary, and binary 
 * strings are represented with bytes comma-separated.
 */
static char *reg_val_dotreg_string(TALLOC_CTX *mem_ctx, uint32_t type,
				   const DATA_BLOB data)
{
	char *ret = NULL;

	if (data.length == 0)
		return talloc_strdup(mem_ctx, "");

	switch (type) {
		case REG_EXPAND_SZ:
		case REG_SZ:
			convert_string_talloc(mem_ctx,
							  CH_UTF16, CH_UNIX, data.data, data.length,
							  (void **)&ret, NULL, false);
			break;
		case REG_DWORD:
		case REG_DWORD_BIG_ENDIAN:
			SMB_ASSERT(data.length == sizeof(uint32_t));
			ret = talloc_asprintf(mem_ctx, "%08x",
					      IVAL(data.data, 0));
			break;
		default: /* default means treat as binary */
		case REG_BINARY:
			ret = dotreg_data_blob_hex_string(mem_ctx, &data);
			break;
	}

	return ret;
}

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
	char *data_string = reg_val_dotreg_string(NULL, 
						value_type, value);
	char *data_incl_type;

	W_ERROR_HAVE_NO_MEMORY(data_string);

	switch (value_type) {
		case REG_SZ:
			data_incl_type = talloc_asprintf(data_string, "\"%s\"", 
					data_string);
			break;
		case REG_DWORD:
			data_incl_type = talloc_asprintf(data_string, 
					"dword:%s", data_string);
			break;
		case REG_BINARY:
			data_incl_type = talloc_asprintf(data_string, "hex:%s",
					data_string);
			break;
		default:
			data_incl_type = talloc_asprintf(data_string, "hex(%x):%s", 
					value_type, data_string);
			break;
	}

	if (value_name[0] == '\0') {
		fdprintf(data->fd, "@=%s\n", data_incl_type);
	} else {
		fdprintf(data->fd, "\"%s\"=%s\n",
			 value_name, data_incl_type);
	}

	talloc_free(data_string);

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
				     struct reg_diff_callbacks **callbacks,
				     void **callback_data)
{
	struct dotreg_data *data;

	data = talloc_zero(ctx, struct dotreg_data);
	*callback_data = data;

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
				     const struct reg_diff_callbacks *callbacks,
				     void *callback_data)
{
	char *line, *p, *q;
	char *curkey = NULL;
	TALLOC_CTX *mem_ctx = talloc_init("reg_dotreg_diff_load");
	WERROR error;
	uint32_t value_type;
	DATA_BLOB data;
	bool result;
	char *type_str = NULL;
	char *data_str;
	char *value;
	bool continue_next_line = 0;

	line = afdgets(fd, mem_ctx, 0);
	if (!line) {
		DEBUG(0, ("Can't read from file.\n"));
		talloc_free(mem_ctx);
		close(fd);
		return WERR_GENERAL_FAILURE;
	}

	while ((line = afdgets(fd, mem_ctx, 0))) {
		/* Remove '\r' if it's a Windows text file */
		if (line[strlen(line)-1] == '\r') {
			line[strlen(line)-1] = '\0';
		}

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
			if (line[strlen(line)-1] != ']') {
				DEBUG(0, ("Missing ']' on line: %s\n", line));
				talloc_free(line);
				continue;
			}

			/* Deleting key */
			if (line[1] == '-') {
				curkey = talloc_strndup(line, line+2, strlen(line)-3);
				W_ERROR_HAVE_NO_MEMORY(curkey);

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
			W_ERROR_HAVE_NO_MEMORY(curkey);

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
		if (continue_next_line) {
			continue_next_line = 0;

			/* Continued data start with two whitespaces */
			if (line[0] != ' ' || line[1] != ' ') {
				DEBUG(0, ("Malformed line: %s\n", line));
				talloc_free(line);
				continue;
			}
			p = line + 2;

			/* Continue again if line ends with a backslash */
			if (line[strlen(line)-1] == '\\') {
				line[strlen(line)-1] = '\0';
				continue_next_line = 1;
				data_str = talloc_strdup_append(data_str, p);
				talloc_free(line);
				continue;
			}
			data_str = talloc_strdup_append(data_str, p);
		} else {
			p = strchr_m(line, '=');
			if (p == NULL) {
				DEBUG(0, ("Malformed line: %s\n", line));
				talloc_free(line);
				continue;
			}

			*p = '\0'; p++;


			if (curkey == NULL) {
				DEBUG(0, ("Value change without key\n"));
				talloc_free(line);
				continue;
			}

			/* Values should be double-quoted */
			if (line[0] != '"') {
				DEBUG(0, ("Malformed line\n"));
				talloc_free(line);
				continue;
			}

			/* Chop of the quotes and store as value */
			value = talloc_strndup(mem_ctx, line+1,strlen(line)-2);

			/* Delete value */
			if (p[0] == '-') {
				error = callbacks->del_value(callback_data,
						     curkey, value);

				/* Ignore if key does not exist (WERR_BADFILE)
				 * Consistent with Windows behaviour */
				if (!W_ERROR_IS_OK(error) &&
				    !W_ERROR_EQUAL(error, WERR_BADFILE)) {
					DEBUG(0, ("Error deleting value %s in key %s\n",
						value, curkey));
					talloc_free(mem_ctx);
					return error;
				}

				talloc_free(line);
				talloc_free(value);
				continue;
			}

			/* Do not look for colons in strings */
			if (p[0] == '"') {
				q = NULL;
				data_str = talloc_strndup(mem_ctx, p+1,strlen(p)-2);
			} else {
				/* Split the value type from the data */
				q = strchr_m(p, ':');
				if (q) {
					*q = '\0';
					q++;
					type_str = talloc_strdup(mem_ctx, p);
					data_str = talloc_strdup(mem_ctx, q);
				} else {
					data_str = talloc_strdup(mem_ctx, p);
				}
			}

			/* Backslash before the CRLF means continue on next line */
			if (data_str[strlen(data_str)-1] == '\\') {
				data_str[strlen(data_str)-1] = '\0';
				talloc_free(line);
				continue_next_line = 1;
				continue;
			}
		}
		DEBUG(9, ("About to write %s with type %s, length %ld: %s\n", value, type_str, (long) strlen(data_str), data_str));
		result = reg_string_to_val(value,
				  type_str?type_str:"REG_SZ", data_str,
				  &value_type, &data);
		if (!result) {
			DEBUG(0, ("Error converting string to value for line:\n%s\n",
					line));
			return WERR_GENERAL_FAILURE;
		}

		error = callbacks->set_value(callback_data, curkey, value,
					     value_type, data);
		if (!W_ERROR_IS_OK(error)) {
			DEBUG(0, ("Error setting value for %s in %s\n",
				value, curkey));
			talloc_free(mem_ctx);
			return error;
		}

		/* Clean up buffers */
		if (type_str != NULL) {
			talloc_free(type_str);
			type_str = NULL;
		}
		talloc_free(data_str);
		talloc_free(value);
		talloc_free(line);
	}

	close(fd);

	talloc_free(mem_ctx);

	return WERR_OK;
}
