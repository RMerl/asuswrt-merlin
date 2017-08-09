/* 
   ldb database library

   Copyright (C) Andrew Tridgell  2004

     ** NOTE! The following LGPL license applies to the ldb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 *  Name: ldb
 *
 *  Component: ldb pack/unpack
 *
 *  Description: pack/unpack routines for ldb messages as key/value blobs
 *
 *  Author: Andrew Tridgell
 */

#include "ldb_tdb.h"

/* change this if the data format ever changes */
#define LTDB_PACKING_FORMAT 0x26011967

/* old packing formats */
#define LTDB_PACKING_FORMAT_NODN 0x26011966

/* use a portable integer format */
static void put_uint32(uint8_t *p, int ofs, unsigned int val)
{
	p += ofs;
	p[0] = val&0xFF;
	p[1] = (val>>8)  & 0xFF;
	p[2] = (val>>16) & 0xFF;
	p[3] = (val>>24) & 0xFF;
}

static unsigned int pull_uint32(uint8_t *p, int ofs)
{
	p += ofs;
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

static int attribute_storable_values(const struct ldb_message_element *el)
{
	if (el->num_values == 0) return 0;

	if (ldb_attr_cmp(el->name, "distinguishedName") == 0) return 0;

	return el->num_values;
}

/*
  pack a ldb message into a linear buffer in a TDB_DATA

  note that this routine avoids saving elements with zero values,
  as these are equivalent to having no element

  caller frees the data buffer after use
*/
int ltdb_pack_data(struct ldb_module *module,
		   const struct ldb_message *message,
		   struct TDB_DATA *data)
{
	struct ldb_context *ldb;
	unsigned int i, j, real_elements=0;
	size_t size;
	const char *dn;
	uint8_t *p;
	size_t len;

	ldb = ldb_module_get_ctx(module);

	dn = ldb_dn_get_linearized(message->dn);
	if (dn == NULL) {
		errno = ENOMEM;
		return -1;
	}

	/* work out how big it needs to be */
	size = 8;

	size += 1 + strlen(dn);

	for (i=0;i<message->num_elements;i++) {
		if (attribute_storable_values(&message->elements[i]) == 0) {
			continue;
		}

		real_elements++;

		size += 1 + strlen(message->elements[i].name) + 4;
		for (j=0;j<message->elements[i].num_values;j++) {
			size += 4 + message->elements[i].values[j].length + 1;
		}
	}

	/* allocate it */
	data->dptr = talloc_array(ldb, uint8_t, size);
	if (!data->dptr) {
		errno = ENOMEM;
		return -1;
	}
	data->dsize = size;

	p = data->dptr;
	put_uint32(p, 0, LTDB_PACKING_FORMAT); 
	put_uint32(p, 4, real_elements); 
	p += 8;

	/* the dn needs to be packed so we can be case preserving
	   while hashing on a case folded dn */
	len = strlen(dn);
	memcpy(p, dn, len+1);
	p += len + 1;
	
	for (i=0;i<message->num_elements;i++) {
		if (attribute_storable_values(&message->elements[i]) == 0) {
			continue;
		}
		len = strlen(message->elements[i].name);
		memcpy(p, message->elements[i].name, len+1);
		p += len + 1;
		put_uint32(p, 0, message->elements[i].num_values);
		p += 4;
		for (j=0;j<message->elements[i].num_values;j++) {
			put_uint32(p, 0, message->elements[i].values[j].length);
			memcpy(p+4, message->elements[i].values[j].data, 
			       message->elements[i].values[j].length);
			p[4+message->elements[i].values[j].length] = 0;
			p += 4 + message->elements[i].values[j].length + 1;
		}
	}

	return 0;
}

/*
  unpack a ldb message from a linear buffer in TDB_DATA

  Free with ltdb_unpack_data_free()
*/
int ltdb_unpack_data(struct ldb_module *module,
		     const struct TDB_DATA *data,
		     struct ldb_message *message)
{
	struct ldb_context *ldb;
	uint8_t *p;
	unsigned int remaining;
	unsigned int i, j;
	unsigned format;
	size_t len;

	ldb = ldb_module_get_ctx(module);
	message->elements = NULL;

	p = data->dptr;
	if (data->dsize < 8) {
		errno = EIO;
		goto failed;
	}

	format = pull_uint32(p, 0);
	message->num_elements = pull_uint32(p, 4);
	p += 8;

	remaining = data->dsize - 8;

	switch (format) {
	case LTDB_PACKING_FORMAT_NODN:
		message->dn = NULL;
		break;

	case LTDB_PACKING_FORMAT:
		len = strnlen((char *)p, remaining);
		if (len == remaining) {
			errno = EIO;
			goto failed;
		}
		message->dn = ldb_dn_new(message, ldb, (char *)p);
		if (message->dn == NULL) {
			errno = ENOMEM;
			goto failed;
		}
		remaining -= len + 1;
		p += len + 1;
		break;

	default:
		errno = EIO;
		goto failed;
	}

	if (message->num_elements == 0) {
		return 0;
	}
	
	if (message->num_elements > remaining / 6) {
		errno = EIO;
		goto failed;
	}

	message->elements = talloc_array(message, struct ldb_message_element, message->num_elements);
	if (!message->elements) {
		errno = ENOMEM;
		goto failed;
	}

	memset(message->elements, 0, 
	       message->num_elements * sizeof(struct ldb_message_element));

	for (i=0;i<message->num_elements;i++) {
		if (remaining < 10) {
			errno = EIO;
			goto failed;
		}
		len = strnlen((char *)p, remaining-6);
		if (len == remaining-6) {
			errno = EIO;
			goto failed;
		}
		if (len == 0) {
			errno = EIO;
			goto failed;
		}
		message->elements[i].flags = 0;
		message->elements[i].name = talloc_strndup(message->elements, (char *)p, len);
		if (message->elements[i].name == NULL) {
			errno = ENOMEM;
			goto failed;
		}
		remaining -= len + 1;
		p += len + 1;
		message->elements[i].num_values = pull_uint32(p, 0);
		message->elements[i].values = NULL;
		if (message->elements[i].num_values != 0) {
			message->elements[i].values = talloc_array(message->elements,
								     struct ldb_val, 
								     message->elements[i].num_values);
			if (!message->elements[i].values) {
				errno = ENOMEM;
				goto failed;
			}
		}
		p += 4;
		remaining -= 4;
		for (j=0;j<message->elements[i].num_values;j++) {
			len = pull_uint32(p, 0);
			if (len > remaining-5) {
				errno = EIO;
				goto failed;
			}

			message->elements[i].values[j].length = len;
			message->elements[i].values[j].data = talloc_size(message->elements[i].values, len+1);
			if (message->elements[i].values[j].data == NULL) {
				errno = ENOMEM;
				goto failed;
			}
			memcpy(message->elements[i].values[j].data, p+4, len);
			message->elements[i].values[j].data[len] = 0;
	
			remaining -= len+4+1;
			p += len+4+1;
		}
	}

	if (remaining != 0) {
		ldb_debug(ldb, LDB_DEBUG_ERROR, 
			  "Error: %d bytes unread in ltdb_unpack_data", remaining);
	}

	return 0;

failed:
	talloc_free(message->elements);
	return -1;
}
