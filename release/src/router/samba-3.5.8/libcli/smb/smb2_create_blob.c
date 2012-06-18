/*
   Unix SMB/CIFS implementation.

   SMB2 Create Context Blob handling

   Copyright (C) Andrew Tridgell 2005
   Copyright (C) Stefan Metzmacher 2008-2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "../libcli/smb/smb_common.h"

static size_t smb2_create_blob_padding(uint32_t offset, size_t n)
{
	if ((offset & (n-1)) == 0) return 0;
	return n - (offset & (n-1));
}

/*
  parse a set of SMB2 create blobs
*/
NTSTATUS smb2_create_blob_parse(TALLOC_CTX *mem_ctx, const DATA_BLOB buffer,
				struct smb2_create_blobs *blobs)
{
	const uint8_t *data = buffer.data;
	uint32_t remaining = buffer.length;

	while (remaining > 0) {
		uint32_t next;
		uint32_t name_offset, name_length;
		uint32_t reserved, data_offset;
		uint32_t data_length;
		char *tag;
		DATA_BLOB b;
		NTSTATUS status;

		if (remaining < 16) {
			return NT_STATUS_INVALID_PARAMETER;
		}
		next        = IVAL(data, 0);
		name_offset = SVAL(data, 4);
		name_length = SVAL(data, 6);
		reserved    = SVAL(data, 8);
		data_offset = SVAL(data, 10);
		data_length = IVAL(data, 12);

		if ((next & 0x7) != 0 ||
		    next > remaining ||
		    name_offset < 16 ||
		    name_offset > remaining ||
		    name_length != 4 || /* windows enforces this */
		    name_offset + name_length > remaining ||
		    data_offset < name_offset + name_length ||
		    data_offset > remaining ||
		    data_offset + (uint64_t)data_length > remaining) {
			return NT_STATUS_INVALID_PARAMETER;
		}

		tag = talloc_strndup(mem_ctx, (const char *)data + name_offset, name_length);
		if (tag == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		b = data_blob_const(data+data_offset, data_length);
		status = smb2_create_blob_add(mem_ctx, blobs, tag, b);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}

		talloc_free(tag);

		if (next == 0) break;

		remaining -= next;
		data += next;

		if (remaining < 16) {
			return NT_STATUS_INVALID_PARAMETER;
		}
	}

	return NT_STATUS_OK;
}


/*
  add a blob to a smb2_create attribute blob
*/
static NTSTATUS smb2_create_blob_push_one(TALLOC_CTX *mem_ctx, DATA_BLOB *buffer,
					  const struct smb2_create_blob *blob,
					  bool last)
{
	uint32_t ofs = buffer->length;
	size_t tag_length = strlen(blob->tag);
	uint8_t pad = smb2_create_blob_padding(blob->data.length+tag_length, 4);

	if (!data_blob_realloc(mem_ctx, buffer,
			       buffer->length + 0x14 + tag_length + blob->data.length + pad))
		return NT_STATUS_NO_MEMORY;

	if (last) {
		SIVAL(buffer->data, ofs+0x00, 0);
	} else {
		SIVAL(buffer->data, ofs+0x00, 0x14 + tag_length + blob->data.length + pad);
	}
	SSVAL(buffer->data, ofs+0x04, 0x10); /* offset of tag */
	SIVAL(buffer->data, ofs+0x06, tag_length); /* tag length */
	SSVAL(buffer->data, ofs+0x0A, 0x14 + tag_length); /* offset of data */
	SIVAL(buffer->data, ofs+0x0C, blob->data.length);
	memcpy(buffer->data+ofs+0x10, blob->tag, tag_length);
	SIVAL(buffer->data, ofs+0x10+tag_length, 0); /* pad? */
	memcpy(buffer->data+ofs+0x14+tag_length, blob->data.data, blob->data.length);
	memset(buffer->data+ofs+0x14+tag_length+blob->data.length, 0, pad);

	return NT_STATUS_OK;
}


/*
  create a buffer of a set of create blobs
*/
NTSTATUS smb2_create_blob_push(TALLOC_CTX *mem_ctx, DATA_BLOB *buffer,
			       const struct smb2_create_blobs blobs)
{
	int i;
	NTSTATUS status;

	*buffer = data_blob(NULL, 0);
	for (i=0; i < blobs.num_blobs; i++) {
		bool last = false;
		const struct smb2_create_blob *c;

		if ((i + 1) == blobs.num_blobs) {
			last = true;
		}

		c = &blobs.blobs[i];
		status = smb2_create_blob_push_one(mem_ctx, buffer, c, last);
		if (!NT_STATUS_IS_OK(status)) {
			return status;
		}
	}
	return NT_STATUS_OK;
}


NTSTATUS smb2_create_blob_add(TALLOC_CTX *mem_ctx, struct smb2_create_blobs *b,
			      const char *tag, DATA_BLOB data)
{
	struct smb2_create_blob *array;

	array = talloc_realloc(mem_ctx, b->blobs,
			       struct smb2_create_blob,
			       b->num_blobs + 1);
	NT_STATUS_HAVE_NO_MEMORY(array);
	b->blobs = array;

	b->blobs[b->num_blobs].tag = talloc_strdup(b->blobs, tag);
	NT_STATUS_HAVE_NO_MEMORY(b->blobs[b->num_blobs].tag);

	if (data.data) {
		b->blobs[b->num_blobs].data = data_blob_talloc(b->blobs,
							       data.data,
							       data.length);
		NT_STATUS_HAVE_NO_MEMORY(b->blobs[b->num_blobs].data.data);
	} else {
		b->blobs[b->num_blobs].data = data_blob(NULL, 0);
	}

	b->num_blobs += 1;

	return NT_STATUS_OK;
}

/*
 * return the first blob with the given tag
 */
struct smb2_create_blob *smb2_create_blob_find(const struct smb2_create_blobs *b,
					       const char *tag)
{
	uint32_t i;

	for (i=0; i < b->num_blobs; i++) {
		if (strcmp(b->blobs[i].tag, tag) == 0) {
			return &b->blobs[i];
		}
	}

	return NULL;
}
