/*
   Unix SMB/CIFS implementation.
   Reading Registry.pol PReg registry files

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

#include "includes.h"
#include "lib/registry/registry.h"
#include "system/filesys.h"
#include "librpc/gen_ndr/winreg.h"

struct preg_data {
	int fd;
	TALLOC_CTX *ctx;
};

static WERROR preg_read_utf16(int fd, char *c)
{
	uint16_t v;

	if (read(fd, &v, sizeof(uint16_t)) < sizeof(uint16_t)) {
		return WERR_GENERAL_FAILURE;
	}
	push_codepoint(c, v);
	return WERR_OK;
}
static WERROR preg_write_utf16(int fd, const char *string)
{
	uint16_t v;
	size_t i, size;

	for (i = 0; i < strlen(string); i+=size) {
		v = next_codepoint(&string[i], &size);
		if (write(fd, &v, sizeof(uint16_t)) < sizeof(uint16_t)) {
			return WERR_GENERAL_FAILURE;
		}
	}
	return WERR_OK;
}
/* PReg does not support adding keys. */
static WERROR reg_preg_diff_add_key(void *_data, const char *key_name)
{
	return WERR_OK;
}

static WERROR reg_preg_diff_set_value(void *_data, const char *key_name,
				      const char *value_name,
				      uint32_t value_type, DATA_BLOB value_data)
{
	struct preg_data *data = (struct preg_data *)_data;
	uint32_t buf;
	
	preg_write_utf16(data->fd, "[");
	preg_write_utf16(data->fd, key_name);
	preg_write_utf16(data->fd, ";");
	preg_write_utf16(data->fd, value_name);
	preg_write_utf16(data->fd, ";");
	SIVAL(&buf, 0, value_type);
	write(data->fd, &buf, sizeof(uint32_t));
	preg_write_utf16(data->fd, ";");
	SIVAL(&buf, 0, value_data.length);
	write(data->fd, &buf, sizeof(uint32_t));
	preg_write_utf16(data->fd, ";");
	write(data->fd, value_data.data, value_data.length);
	preg_write_utf16(data->fd, "]");
	
	return WERR_OK;
}

static WERROR reg_preg_diff_del_key(void *_data, const char *key_name)
{
	struct preg_data *data = (struct preg_data *)_data;
	char *parent_name;
	DATA_BLOB blob;
	WERROR werr;

	parent_name = talloc_strndup(data->ctx, key_name,
				     strrchr(key_name, '\\')-key_name);
	W_ERROR_HAVE_NO_MEMORY(parent_name);
	blob.data = (uint8_t*)talloc_strndup(data->ctx,
					     key_name+(strrchr(key_name, '\\')-key_name)+1,
					     strlen(key_name)-(strrchr(key_name, '\\')-key_name));
	W_ERROR_HAVE_NO_MEMORY(blob.data);
	blob.length = strlen((char *)blob.data)+1;
	

	/* FIXME: These values should be accumulated to be written at done(). */
	werr = reg_preg_diff_set_value(data, parent_name, "**DeleteKeys",
				       REG_SZ, blob);

	talloc_free(parent_name);
	talloc_free(blob.data);

	return werr;
}

static WERROR reg_preg_diff_del_value(void *_data, const char *key_name,
				      const char *value_name)
{
	struct preg_data *data = (struct preg_data *)_data;
	char *val;
	DATA_BLOB blob;
	WERROR werr;

	val = talloc_asprintf(data->ctx, "**Del.%s", value_name);
	W_ERROR_HAVE_NO_MEMORY(val);
	blob.data = (uint8_t *)talloc(data->ctx, uint32_t);
	W_ERROR_HAVE_NO_MEMORY(blob.data);
	SIVAL(blob.data, 0, 0);
	blob.length = sizeof(uint32_t);

	werr = reg_preg_diff_set_value(data, key_name, val, REG_DWORD, blob);

	talloc_free(val);
	talloc_free(blob.data);

	return werr;
}

static WERROR reg_preg_diff_del_all_values(void *_data, const char *key_name)
{
	struct preg_data *data = (struct preg_data *)_data;
	DATA_BLOB blob;
	WERROR werr;

	blob.data = (uint8_t *)talloc(data->ctx, uint32_t);
	W_ERROR_HAVE_NO_MEMORY(blob.data);
	SIVAL(blob.data, 0, 0);
	blob.length = sizeof(uint32_t);

	werr = reg_preg_diff_set_value(data, key_name, "**DelVals.", REG_DWORD,
				       blob);

	talloc_free(blob.data);

	return werr;
}

static WERROR reg_preg_diff_done(void *_data)
{
	struct preg_data *data = (struct preg_data *)_data;
	
	close(data->fd);
	talloc_free(data);
	return WERR_OK;
}

/**
 * Save registry diff
 */
_PUBLIC_ WERROR reg_preg_diff_save(TALLOC_CTX *ctx, const char *filename,
				   struct reg_diff_callbacks **callbacks,
				   void **callback_data)
{
	struct preg_data *data;
	struct {
		char hdr[4];
		uint32_t version;
	} preg_header;


	data = talloc_zero(ctx, struct preg_data);
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

	strncpy(preg_header.hdr, "PReg", 4);
	SIVAL(&preg_header.version, 0, 1);
	write(data->fd, (uint8_t *)&preg_header, sizeof(preg_header));

	data->ctx = ctx;

	*callbacks = talloc(ctx, struct reg_diff_callbacks);

	(*callbacks)->add_key = reg_preg_diff_add_key;
	(*callbacks)->del_key = reg_preg_diff_del_key;
	(*callbacks)->set_value = reg_preg_diff_set_value;
	(*callbacks)->del_value = reg_preg_diff_del_value;
	(*callbacks)->del_all_values = reg_preg_diff_del_all_values;
	(*callbacks)->done = reg_preg_diff_done;

	return WERR_OK;
}
/**
 * Load diff file
 */
_PUBLIC_ WERROR reg_preg_diff_load(int fd,
				   const struct reg_diff_callbacks *callbacks,
				   void *callback_data)
{
	struct {
		char hdr[4];
		uint32_t version;
	} preg_header;
	char *buf;
	size_t buf_size = 1024;
	char *buf_ptr;
	TALLOC_CTX *mem_ctx = talloc_init("reg_preg_diff_load");
	WERROR ret = WERR_OK;
	DATA_BLOB data = {NULL, 0};
	char *key = NULL;
	char *value_name = NULL;

	buf = talloc_array(mem_ctx, char, buf_size);
	buf_ptr = buf;

	/* Read first 8 bytes (the header) */
	if (read(fd, &preg_header, sizeof(preg_header)) != sizeof(preg_header)) {
		DEBUG(0, ("Could not read PReg file: %s\n",
				strerror(errno)));
		ret = WERR_GENERAL_FAILURE;
		goto cleanup;
	}
	preg_header.version = IVAL(&preg_header.version, 0);

	if (strncmp(preg_header.hdr, "PReg", 4) != 0) {
		DEBUG(0, ("This file is not a valid preg registry file\n"));
		ret = WERR_GENERAL_FAILURE;
		goto cleanup;
	}
	if (preg_header.version > 1) {
		DEBUG(0, ("Warning: file format version is higher than expected.\n"));
	}

	/* Read the entries */
	while(1) {
		uint32_t value_type, length;

		if (!W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr))) {
			break;
		}
		if (*buf_ptr != '[') {
			DEBUG(0, ("Error in PReg file.\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}

		/* Get the path */
		buf_ptr = buf;
		while (W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr)) &&
		       *buf_ptr != ';' && buf_ptr-buf < buf_size) {
			buf_ptr++;
		}
		buf[buf_ptr-buf] = '\0';
		key = talloc_strdup(mem_ctx, buf);

		/* Get the name */
		buf_ptr = buf;
		while (W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr)) &&
		       *buf_ptr != ';' && buf_ptr-buf < buf_size) {
			buf_ptr++;
		}
		buf[buf_ptr-buf] = '\0';
		value_name = talloc_strdup(mem_ctx, buf);

		/* Get the type */
		if (read(fd, &value_type, sizeof(uint32_t)) < sizeof(uint32_t)) {
			DEBUG(0, ("Error while reading PReg\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}
		value_type = IVAL(&value_type, 0);

		/* Read past delimiter */
		buf_ptr = buf;
		if (!(W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr)) &&
		    *buf_ptr == ';') && buf_ptr-buf < buf_size) {
			DEBUG(0, ("Error in PReg file.\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}

		/* Get data length */
		if (read(fd, &length, sizeof(uint32_t)) < sizeof(uint32_t)) {
			DEBUG(0, ("Error while reading PReg\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}
		length = IVAL(&length, 0);

		/* Read past delimiter */
		buf_ptr = buf;
		if (!(W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr)) &&
		    *buf_ptr == ';') && buf_ptr-buf < buf_size) {
			DEBUG(0, ("Error in PReg file.\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}

		/* Get the data */
		buf_ptr = buf;
		if (length < buf_size &&
		    read(fd, buf_ptr, length) != length) {
			DEBUG(0, ("Error while reading PReg\n"));
			ret = WERR_GENERAL_FAILURE;
			goto cleanup;
		}
		data = data_blob_talloc(mem_ctx, buf, length);

		/* Check if delimiter is in place (whine if it isn't) */
		buf_ptr = buf;
		if (!(W_ERROR_IS_OK(preg_read_utf16(fd, buf_ptr)) &&
		    *buf_ptr == ']') && buf_ptr-buf < buf_size) {
			DEBUG(0, ("Warning: Missing ']' in PReg file, expected ']', got '%c' 0x%x.\n",
				*buf_ptr, *buf_ptr));
		}

		if (strcasecmp(value_name, "**DelVals") == 0) {
			callbacks->del_all_values(callback_data, key);
		} else if (strncasecmp(value_name, "**Del.",6) == 0) {
			char *p = value_name+6;

			callbacks->del_value(callback_data, key, p);
		} else 	if (strcasecmp(value_name, "**DeleteValues") == 0) {
			char *p, *q;

			p = (char *) data.data;

			while ((q = strchr_m(p, ';'))) {
				*q = '\0';
				q++;

				callbacks->del_value(callback_data, key, p);

				p = q;
			}
			callbacks->del_value(callback_data, key, p);
		} else if (strcasecmp(value_name, "**DeleteKeys") == 0) {
			char *p, *q, *full_key;

			p = (char *) data.data;

			while ((q = strchr_m(p, ';'))) {
				*q = '\0';
				q++;

				full_key = talloc_asprintf(mem_ctx, "%s\\%s",
							   key, p);
				callbacks->del_key(callback_data, full_key);
				talloc_free(full_key);

				p = q;
			}
			full_key = talloc_asprintf(mem_ctx, "%s\\%s", key, p);
			callbacks->del_key(callback_data, full_key);
			talloc_free(full_key);
		} else {
			callbacks->add_key(callback_data, key);
			callbacks->set_value(callback_data, key, value_name,
					     value_type, data);
 		}
	}
cleanup:
	close(fd);
	talloc_free(data.data);
	talloc_free(key);
	talloc_free(value_name);
	talloc_free(buf);
	return ret;
}
