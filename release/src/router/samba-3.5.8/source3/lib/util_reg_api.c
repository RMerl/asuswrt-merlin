/*
 * Unix SMB/CIFS implementation.
 * Registry helper routines
 * Copyright (C) Volker Lendecke 2006
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_REGISTRY

WERROR registry_pull_value(TALLOC_CTX *mem_ctx,
			   struct registry_value **pvalue,
			   enum winreg_Type type, uint8 *data,
			   uint32 size, uint32 length)
{
	struct registry_value *value;
	WERROR err;

	if (!(value = TALLOC_ZERO_P(mem_ctx, struct registry_value))) {
		return WERR_NOMEM;
	}

	value->type = type;

	switch (type) {
	case REG_DWORD:
		if ((size != 4) || (length != 4)) {
			err = WERR_INVALID_PARAM;
			goto error;
		}
		value->v.dword = IVAL(data, 0);
		break;
	case REG_SZ:
	case REG_EXPAND_SZ:
	{
		/*
		 * Make sure we get a NULL terminated string for
		 * convert_string_talloc().
		 */

		smb_ucs2_t *tmp;

		if (length == 1) {
			/* win2k regedit gives us a string of 1 byte when
			 * creating a new value of type REG_SZ. this workaround
			 * replaces the input by using the same string as
			 * winxp delivers. */
			length = 2;
			if (!(tmp = SMB_MALLOC_ARRAY(smb_ucs2_t, 2))) {
				err = WERR_NOMEM;
				goto error;
			}
			tmp[0] = 0;
			tmp[1] = 0;
			DEBUG(10, ("got REG_SZ value of length 1 - workaround "
				   "activated.\n"));
		}
		else if ((length % 2) != 0) {
			err = WERR_INVALID_PARAM;
			goto error;
		}
		else {
			uint32 num_ucs2 = length / 2;
			if (!(tmp = SMB_MALLOC_ARRAY(smb_ucs2_t, num_ucs2+1))) {
				err = WERR_NOMEM;
				goto error;
			}

			memcpy((void *)tmp, (const void *)data, length);
			tmp[num_ucs2] = 0;
		}

		if (length + 2 < length) {
			/* Integer wrap. */
			SAFE_FREE(tmp);
			err = WERR_INVALID_PARAM;
			goto error;
		}

		if (!convert_string_talloc(value, CH_UTF16LE, CH_UNIX, tmp,
					   length+2, (void *)&value->v.sz.str,
					   &value->v.sz.len, False)) {
			SAFE_FREE(tmp);
			err = WERR_INVALID_PARAM;
			goto error;
		}

		SAFE_FREE(tmp);
		break;
	}
	case REG_MULTI_SZ: {
		int i;
		const char **vals;
		DATA_BLOB blob;

		blob = data_blob_const(data, length);

		if (!pull_reg_multi_sz(mem_ctx, &blob, &vals)) {
			err = WERR_NOMEM;
			goto error;
		}

		for (i=0; vals[i]; i++) {
			;;
		}

		value->v.multi_sz.num_strings = i;
		value->v.multi_sz.strings = (char **)vals;

		break;
	}
	case REG_BINARY:
		value->v.binary = data_blob_talloc(mem_ctx, data, length);
		break;
	default:
		err = WERR_INVALID_PARAM;
		goto error;
	}

	*pvalue = value;
	return WERR_OK;

 error:
	TALLOC_FREE(value);
	return err;
}

WERROR registry_push_value(TALLOC_CTX *mem_ctx,
			   const struct registry_value *value,
			   DATA_BLOB *presult)
{
	switch (value->type) {
	case REG_DWORD: {
		char buf[4];
		SIVAL(buf, 0, value->v.dword);
		*presult = data_blob_talloc(mem_ctx, (void *)buf, 4);
		if (presult->data == NULL) {
			return WERR_NOMEM;
		}
		break;
	}
	case REG_SZ:
	case REG_EXPAND_SZ: {
		if (!push_reg_sz(mem_ctx, presult, value->v.sz.str))
		{
			return WERR_NOMEM;
		}
		break;
	}
	case REG_MULTI_SZ: {
		/* handle the case where we don't get a NULL terminated array */
		const char **array;
		int i;

		array = talloc_array(mem_ctx, const char *,
				     value->v.multi_sz.num_strings + 1);
		if (!array) {
			return WERR_NOMEM;
		}

		for (i=0; i < value->v.multi_sz.num_strings; i++) {
			array[i] = value->v.multi_sz.strings[i];
		}
		array[i] = NULL;

		if (!push_reg_multi_sz(mem_ctx, presult, array)) {
			talloc_free(array);
			return WERR_NOMEM;
		}
		talloc_free(array);
		break;
	}
	case REG_BINARY:
		*presult = data_blob_talloc(mem_ctx,
					    value->v.binary.data,
					    value->v.binary.length);
		break;
	default:
		return WERR_INVALID_PARAM;
	}

	return WERR_OK;
}
