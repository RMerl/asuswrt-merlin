/*
   Samba CIFS implementation
   Registry backend for REGF files
   Copyright (C) 2005-2007 Jelmer Vernooij, jelmer@samba.org
   Copyright (C) 2006-2010 Wilco Baan Hofman, wilco@baanhofman.nl

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "includes.h"
#include "system/filesys.h"
#include "system/time.h"
#include "lib/registry/tdr_regf.h"
#include "librpc/gen_ndr/ndr_security.h"
#include "librpc/gen_ndr/winreg.h"
#include "lib/registry/registry.h"
#include "libcli/security/security.h"


static struct hive_operations reg_backend_regf;

/**
 * There are several places on the web where the REGF format is explained;
 *
 * TODO: Links
 */

/* TODO:
 *  - Return error codes that make more sense
 *  - Locking
 *  - do more things in-memory
 */

/*
 * Read HBIN blocks into memory
 */

struct regf_data {
	int fd;
	struct hbin_block **hbins;
	struct regf_hdr *header;
	time_t last_write;
};

static WERROR regf_save_hbin(struct regf_data *data, bool flush);

struct regf_key_data {
	struct hive_key key;
	struct regf_data *hive;
	uint32_t offset;
	struct nk_block *nk;
};

static struct hbin_block *hbin_by_offset(const struct regf_data *data,
					 uint32_t offset, uint32_t *rel_offset)
{
	unsigned int i;

	for (i = 0; data->hbins[i]; i++) {
		if (offset >= data->hbins[i]->offset_from_first &&
			offset < data->hbins[i]->offset_from_first+
					 data->hbins[i]->offset_to_next) {
			if (rel_offset != NULL)
				*rel_offset = offset - data->hbins[i]->offset_from_first - 0x20;
			return data->hbins[i];
		}
	}

	return NULL;
}

/**
 * Validate a regf header
 * For now, do nothing, but we should check the checksum
 */
static uint32_t regf_hdr_checksum(const uint8_t *buffer)
{
	uint32_t checksum = 0, x;
	unsigned int i;

	for (i = 0; i < 0x01FB; i+= 4) {
		x = IVAL(buffer, i);
		checksum ^= x;
	}

	return checksum;
}

/**
 * Obtain the contents of a HBIN block
 */
static DATA_BLOB hbin_get(const struct regf_data *data, uint32_t offset)
{
	DATA_BLOB ret;
	struct hbin_block *hbin;
	uint32_t rel_offset;

	ret.data = NULL;
	ret.length = 0;

	hbin = hbin_by_offset(data, offset, &rel_offset);

	if (hbin == NULL) {
		DEBUG(1, ("Can't find HBIN at 0x%04x\n", offset));
		return ret;
	}

	ret.length = IVAL(hbin->data, rel_offset);
	if (!(ret.length & 0x80000000)) {
		DEBUG(0, ("Trying to use dirty block at 0x%04x\n", offset));
		return ret;
	}

	/* remove high bit */
	ret.length = (ret.length ^ 0xffffffff) + 1;

	ret.length -= 4; /* 4 bytes for the length... */
	ret.data = hbin->data +
		(offset - hbin->offset_from_first - 0x20) + 4;

	return ret;
}

static bool hbin_get_tdr(struct regf_data *regf, uint32_t offset,
			 TALLOC_CTX *ctx, tdr_pull_fn_t pull_fn, void *p)
{
	struct tdr_pull *pull = tdr_pull_init(regf);

	pull->data = hbin_get(regf, offset);
	if (!pull->data.data) {
		DEBUG(1, ("Unable to get data at 0x%04x\n", offset));
		talloc_free(pull);
		return false;
	}

	if (NT_STATUS_IS_ERR(pull_fn(pull, ctx, p))) {
		DEBUG(1, ("Error parsing record at 0x%04x using tdr\n",
			offset));
		talloc_free(pull);
		return false;
	}
	talloc_free(pull);

	return true;
}

/* Allocate some new data */
static DATA_BLOB hbin_alloc(struct regf_data *data, uint32_t size,
			    uint32_t *offset)
{
	DATA_BLOB ret;
	uint32_t rel_offset = (uint32_t) -1; /* Relative offset ! */
	struct hbin_block *hbin = NULL;
	unsigned int i;

	*offset = 0;

	if (size == 0)
		return data_blob(NULL, 0);

	size += 4; /* Need to include int32 for the length */

	/* Allocate as a multiple of 8 */
	size = (size + 7) & ~7;

	ret.data = NULL;
	ret.length = 0;

	for (i = 0; (hbin = data->hbins[i]); i++) {
		int j;
		int32_t my_size;
		for (j = 0; j < hbin->offset_to_next-0x20; j+= my_size) {
			my_size = IVALS(hbin->data, j);

			if (my_size == 0x0) {
				DEBUG(0, ("Invalid zero-length block! File is corrupt.\n"));
				return ret;
			}

			if (my_size % 8 != 0) {
				DEBUG(0, ("Encountered non-aligned block!\n"));
			}

			if (my_size < 0) { /* Used... */
				my_size = -my_size;
			} else if (my_size == size) { /* exact match */
				rel_offset = j;
				DEBUG(4, ("Found free block of exact size %d in middle of HBIN\n",
					size));
				break;
			} else if (my_size > size) { /* data will remain */
				rel_offset = j;
				/* Split this block and mark the next block as free */
				SIVAL(hbin->data, rel_offset+size, my_size-size);
				DEBUG(4, ("Found free block of size %d (needing %d) in middle of HBIN\n",
					my_size, size));
				break;
			}
		}

		if (rel_offset != -1)
			break;
	}

	/* No space available in previous hbins,
	 * allocate new one */
	if (data->hbins[i] == NULL) {
		DEBUG(4, ("No space available in other HBINs for block of size %d, allocating new HBIN\n",
			size));

		/* Add extra hbin block */
		data->hbins = talloc_realloc(data, data->hbins,
					     struct hbin_block *, i+2);
		hbin = talloc(data->hbins, struct hbin_block);
		SMB_ASSERT(hbin != NULL);

		data->hbins[i] = hbin;
		data->hbins[i+1] = NULL;

		/* Set hbin data */
		hbin->HBIN_ID = talloc_strdup(hbin, "hbin");
		hbin->offset_from_first = (i == 0?0:data->hbins[i-1]->offset_from_first+data->hbins[i-1]->offset_to_next);
		hbin->offset_to_next = 0x1000;
		hbin->unknown[0] = 0;
		hbin->unknown[1] = 0;
		unix_to_nt_time(&hbin->last_change, time(NULL));
		hbin->block_size = hbin->offset_to_next;
		hbin->data = talloc_zero_array(hbin, uint8_t, hbin->block_size - 0x20);
		/* Update the regf header */
		data->header->last_block += hbin->offset_to_next;

		/* Set the next block to it's proper size and set the
		 * rel_offset for this block */
		SIVAL(hbin->data, size, hbin->block_size - size - 0x20);
		rel_offset = 0x0;
	}

	/* Set size and mark as used */
	SIVAL(hbin->data, rel_offset, -size);

	ret.data = hbin->data + rel_offset + 0x4; /* Skip past length */
	ret.length = size - 0x4;
	if (offset) {
		uint32_t new_rel_offset;
		*offset = hbin->offset_from_first + rel_offset + 0x20;
		SMB_ASSERT(hbin_by_offset(data, *offset, &new_rel_offset) == hbin);
		SMB_ASSERT(new_rel_offset == rel_offset);
	}

	return ret;
}

/* Store a data blob. Return the offset at which it was stored */
static uint32_t hbin_store (struct regf_data *data, DATA_BLOB blob)
{
	uint32_t ret;
	DATA_BLOB dest = hbin_alloc(data, blob.length, &ret);

	memcpy(dest.data, blob.data, blob.length);

	/* Make sure that we have no tailing garbage in the block */
	if (dest.length > blob.length) {
		memset(dest.data + blob.length, 0, dest.length - blob.length);
	}

	return ret;
}

static uint32_t hbin_store_tdr(struct regf_data *data,
			       tdr_push_fn_t push_fn, void *p)
{
	struct tdr_push *push = tdr_push_init(data);
	uint32_t ret;

	if (NT_STATUS_IS_ERR(push_fn(push, p))) {
		DEBUG(0, ("Error during push\n"));
		return -1;
	}

	ret = hbin_store(data, push->data);

	talloc_free(push);

	return ret;
}


/* Free existing data */
static void hbin_free (struct regf_data *data, uint32_t offset)
{
	int32_t size;
	uint32_t rel_offset;
	int32_t next_size;
	struct hbin_block *hbin;

	SMB_ASSERT (offset > 0);

	hbin = hbin_by_offset(data, offset, &rel_offset);

	if (hbin == NULL)
		return;

	/* Get original size */
	size = IVALS(hbin->data, rel_offset);

	if (size > 0) {
		DEBUG(1, ("Trying to free already freed block at 0x%04x\n",
			offset));
		return;
	}
	/* Mark as unused */
	size = -size;

	/* If the next block is free, merge into big free block */
	if (rel_offset + size < hbin->offset_to_next - 0x20) {
		next_size = IVALS(hbin->data, rel_offset+size);
		if (next_size > 0) {
			size += next_size;
		}
	}

	/* Write block size */
	SIVALS(hbin->data, rel_offset, size);
}

/**
 * Store a data blob data was already stored, but has changed in size
 * Will try to save it at the current location if possible, otherwise
 * does a free + store */
static uint32_t hbin_store_resize(struct regf_data *data,
				  uint32_t orig_offset, DATA_BLOB blob)
{
	uint32_t rel_offset;
	struct hbin_block *hbin = hbin_by_offset(data, orig_offset,
						 &rel_offset);
	int32_t my_size;
	int32_t orig_size;
	int32_t needed_size;
	int32_t possible_size;
	unsigned int i;

	SMB_ASSERT(orig_offset > 0);

	if (!hbin)
		return hbin_store(data, blob);

	/* Get original size */
	orig_size = -IVALS(hbin->data, rel_offset);

	needed_size = blob.length + 4; /* Add int32 containing length */
	needed_size = (needed_size + 7) & ~7; /* Align */

	/* Fits into current allocated block */
	if (orig_size >= needed_size) {
		memcpy(hbin->data + rel_offset + 0x4, blob.data, blob.length);
		/* If the difference in size is greater than 0x4, split the block
		 * and free/merge it */
		if (orig_size - needed_size > 0x4) {
			SIVALS(hbin->data, rel_offset, -needed_size);
			SIVALS(hbin->data, rel_offset + needed_size,
			       needed_size-orig_size);
			hbin_free(data, orig_offset + needed_size);
		}
		return orig_offset;
	}

	possible_size = orig_size;

	/* Check if it can be combined with the next few free records */
	for (i = rel_offset; i < hbin->offset_to_next - 0x20; i += my_size) {
		if (IVALS(hbin->data, i) < 0) /* Used */
			break;

		my_size = IVALS(hbin->data, i);

		if (my_size == 0x0) {
			DEBUG(0, ("Invalid zero-length block! File is corrupt.\n"));
			break;
		} else {
			possible_size += my_size;
		}

		if (possible_size >= blob.length) {
			SIVAL(hbin->data, rel_offset, -possible_size);
			memcpy(hbin->data + rel_offset + 0x4,
			       blob.data, blob.length);
			return orig_offset;
		}
	}

	hbin_free(data, orig_offset);
	return hbin_store(data, blob);
}

static uint32_t hbin_store_tdr_resize(struct regf_data *regf,
				      tdr_push_fn_t push_fn,
				      uint32_t orig_offset, void *p)
{
	struct tdr_push *push = tdr_push_init(regf);
	uint32_t ret;

	if (NT_STATUS_IS_ERR(push_fn(push, p))) {
		DEBUG(0, ("Error during push\n"));
		return -1;
	}

	ret = hbin_store_resize(regf, orig_offset, push->data);

	talloc_free(push);

	return ret;
}

static uint32_t regf_create_lh_hash(const char *name)
{
	char *hash_name;
	uint32_t ret = 0;
	uint16_t i;

	hash_name = strupper_talloc(NULL, name);
	for (i = 0; *(hash_name + i) != 0; i++) {
		ret *= 37;
		ret += *(hash_name + i);
	}
	talloc_free(hash_name);
	return ret;
}

static WERROR regf_get_info(TALLOC_CTX *mem_ctx,
			    const struct hive_key *key,
			    const char **classname,
			    uint32_t *num_subkeys,
			    uint32_t *num_values,
			    NTTIME *last_mod_time,
			    uint32_t *max_subkeynamelen,
			    uint32_t *max_valnamelen,
			    uint32_t *max_valbufsize)
{
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)key;

	if (num_subkeys != NULL)
		*num_subkeys = private_data->nk->num_subkeys;

	if (num_values != NULL)
		*num_values = private_data->nk->num_values;

	if (classname != NULL) {
		if (private_data->nk->clsname_offset != -1) {
			DATA_BLOB data = hbin_get(private_data->hive,
						  private_data->nk->clsname_offset);
			*classname = talloc_strndup(mem_ctx,
						    (char*)data.data,
						    private_data->nk->clsname_length);
			W_ERROR_HAVE_NO_MEMORY(*classname);
		} else
			*classname = NULL;
	}

	/* TODO: Last mod time */

	/* TODO: max valnamelen */
	
	/* TODO: max valbufsize */

	/* TODO: max subkeynamelen */

	return WERR_OK;
}

static struct regf_key_data *regf_get_key(TALLOC_CTX *ctx,
					  struct regf_data *regf,
					  uint32_t offset)
{
	struct nk_block *nk;
	struct regf_key_data *ret;

	ret = talloc_zero(ctx, struct regf_key_data);
	ret->key.ops = &reg_backend_regf;
	ret->hive = talloc_reference(ret, regf);
	ret->offset = offset;
	nk = talloc(ret, struct nk_block);
	if (nk == NULL)
		return NULL;

	ret->nk = nk;

	if (!hbin_get_tdr(regf, offset, nk,
			  (tdr_pull_fn_t)tdr_pull_nk_block, nk)) {
		DEBUG(0, ("Unable to find HBIN data for offset 0x%x\n", offset));
		return NULL;
	}

	if (strcmp(nk->header, "nk") != 0) {
		DEBUG(0, ("Expected nk record, got %s\n", nk->header));
		talloc_free(ret);
		return NULL;
	}

	return ret;
}


static WERROR regf_get_value(TALLOC_CTX *ctx, struct hive_key *key,
			     uint32_t idx, const char **name,
			     uint32_t *data_type, DATA_BLOB *data)
{
	const struct regf_key_data *private_data =
			(const struct regf_key_data *)key;
	struct vk_block *vk;
	struct regf_data *regf = private_data->hive;
	uint32_t vk_offset;
	DATA_BLOB tmp;

	if (idx >= private_data->nk->num_values)
		return WERR_NO_MORE_ITEMS;

	tmp = hbin_get(regf, private_data->nk->values_offset);
	if (!tmp.data) {
		DEBUG(0, ("Unable to find value list at 0x%x\n",
				private_data->nk->values_offset));
		return WERR_GENERAL_FAILURE;
	}

	if (tmp.length < private_data->nk->num_values * 4) {
		DEBUG(1, ("Value counts mismatch\n"));
	}

	vk_offset = IVAL(tmp.data, idx * 4);

	vk = talloc(NULL, struct vk_block);
	W_ERROR_HAVE_NO_MEMORY(vk);

	if (!hbin_get_tdr(regf, vk_offset, vk,
			  (tdr_pull_fn_t)tdr_pull_vk_block, vk)) {
		DEBUG(0, ("Unable to get VK block at 0x%x\n", vk_offset));
		talloc_free(vk);
		return WERR_GENERAL_FAILURE;
	}

	/* FIXME: name character set ?*/
	if (name != NULL) {
		*name = talloc_strndup(ctx, vk->data_name, vk->name_length);
		W_ERROR_HAVE_NO_MEMORY(*name);
	}

	if (data_type != NULL)
		*data_type = vk->data_type;

	if (vk->data_length & 0x80000000) {
		/* this is data of type "REG_DWORD" or "REG_DWORD_BIG_ENDIAN" */
		data->data = talloc_size(ctx, sizeof(uint32_t));
		W_ERROR_HAVE_NO_MEMORY(data->data);
		SIVAL(data->data, 0, vk->data_offset);
		data->length = sizeof(uint32_t);
	} else {
		*data = hbin_get(regf, vk->data_offset);
	}

	if (data->length < vk->data_length) {
		DEBUG(1, ("Read data less than indicated data length!\n"));
	}

	talloc_free(vk);

	return WERR_OK;
}

static WERROR regf_get_value_by_name(TALLOC_CTX *mem_ctx,
				     struct hive_key *key, const char *name,
				     uint32_t *type, DATA_BLOB *data)
{
	unsigned int i;
	const char *vname;
	WERROR error;

	/* FIXME: Do binary search? Is this list sorted at all? */

	for (i = 0; W_ERROR_IS_OK(error = regf_get_value(mem_ctx, key, i,
							 &vname, type, data));
							 i++) {
		if (!strcmp(vname, name))
			return WERR_OK;
	}

	if (W_ERROR_EQUAL(error, WERR_NO_MORE_ITEMS))
		return WERR_BADFILE;

	return error;
}


static WERROR regf_get_subkey_by_index(TALLOC_CTX *ctx,
				       const struct hive_key *key,
				       uint32_t idx, const char **name,
				       const char **classname,
				       NTTIME *last_mod_time)
{
	DATA_BLOB data;
	struct regf_key_data *ret;
	const struct regf_key_data *private_data = (const struct regf_key_data *)key;
	struct nk_block *nk = private_data->nk;
	uint32_t key_off=0;

	if (idx >= nk->num_subkeys)
		return WERR_NO_MORE_ITEMS;

	/* Make sure that we don't crash if the key is empty */
	if (nk->subkeys_offset == -1) {
		return WERR_NO_MORE_ITEMS;
	}

	data = hbin_get(private_data->hive, nk->subkeys_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find subkey list at 0x%x\n",
			nk->subkeys_offset));
		return WERR_GENERAL_FAILURE;
	}

	if (!strncmp((char *)data.data, "li", 2)) {
		struct li_block li;
		struct tdr_pull *pull = tdr_pull_init(private_data->hive);

		DEBUG(10, ("Subkeys in LI list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull, nk, &li))) {
			DEBUG(0, ("Error parsing LI list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(li.header, "li", 2));

		if (li.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}
		key_off = li.nk_offset[idx];

	} else if (!strncmp((char *)data.data, "lf", 2)) {
		struct lf_block lf;
		struct tdr_pull *pull = tdr_pull_init(private_data->hive);

		DEBUG(10, ("Subkeys in LF list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lf_block(pull, nk, &lf))) {
			DEBUG(0, ("Error parsing LF list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lf.header, "lf", 2));

		if (lf.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}

		key_off = lf.hr[idx].nk_offset;
	} else if (!strncmp((char *)data.data, "lh", 2)) {
		struct lh_block lh;
		struct tdr_pull *pull = tdr_pull_init(private_data->hive);

		DEBUG(10, ("Subkeys in LH list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull, nk, &lh))) {
			DEBUG(0, ("Error parsing LH list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lh.header, "lh", 2));

		if (lh.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}
		key_off = lh.hr[idx].nk_offset;
	} else if (!strncmp((char *)data.data, "ri", 2)) {
		struct ri_block ri;
		struct tdr_pull *pull = tdr_pull_init(ctx);
		uint16_t i;
		uint16_t sublist_count = 0;

		DEBUG(10, ("Subkeys in RI list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_ri_block(pull, nk, &ri))) {
			DEBUG(0, ("Error parsing RI list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		SMB_ASSERT(!strncmp(ri.header, "ri", 2));

		for (i = 0; i < ri.key_count; i++) {
			DATA_BLOB list_data;

			/* Get sublist data blob */
			list_data = hbin_get(private_data->hive, ri.offset[i]);
			if (!list_data.data) {
				DEBUG(0, ("Error getting RI list."));
				talloc_free(pull);
				return WERR_GENERAL_FAILURE;
			}

			pull->data = list_data;

			if (!strncmp((char *)list_data.data, "li", 2)) {
				struct li_block li;

				DEBUG(10, ("Subkeys in RI->LI list\n"));

				if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull,
								       nk,
								       &li))) {
					DEBUG(0, ("Error parsing LI list from RI\n"));
					talloc_free(pull);
					return WERR_GENERAL_FAILURE;
				}
				SMB_ASSERT(!strncmp(li.header, "li", 2));

				/* Advance to next sublist if necessary */
				if (idx >= sublist_count + li.key_count) {
					sublist_count += li.key_count;
					continue;
				}
				key_off = li.nk_offset[idx - sublist_count];
				sublist_count += li.key_count;
				break;
			} else if (!strncmp((char *)list_data.data, "lh", 2)) {
				struct lh_block lh;

				DEBUG(10, ("Subkeys in RI->LH list\n"));

				if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull,
								       nk,
								       &lh))) {
					DEBUG(0, ("Error parsing LH list from RI\n"));
					talloc_free(pull);
					return WERR_GENERAL_FAILURE;
				}
				SMB_ASSERT(!strncmp(lh.header, "lh", 2));

				/* Advance to next sublist if necessary */
				if (idx >= sublist_count + lh.key_count) {
					sublist_count += lh.key_count;
					continue;
				}
				key_off = lh.hr[idx - sublist_count].nk_offset;
				sublist_count += lh.key_count;
				break;
			} else {
				DEBUG(0,("Unknown sublist in ri block\n"));
				talloc_free(pull);

				return WERR_GENERAL_FAILURE;
			}

		}
		talloc_free(pull);


		if (idx > sublist_count) {
			return WERR_NO_MORE_ITEMS;
		}

	} else {
		DEBUG(0, ("Unknown type for subkey list (0x%04x): %c%c\n",
				  nk->subkeys_offset, data.data[0], data.data[1]));
		return WERR_GENERAL_FAILURE;
	}

	ret = regf_get_key (ctx, private_data->hive, key_off);

	if (classname != NULL) {
		if (ret->nk->clsname_offset != -1) {
			DATA_BLOB db = hbin_get(ret->hive,
						ret->nk->clsname_offset);
			*classname = talloc_strndup(ctx,
						    (char*)db.data,
						    ret->nk->clsname_length);
			W_ERROR_HAVE_NO_MEMORY(*classname);
		} else
			*classname = NULL;
	}

	if (last_mod_time != NULL)
		*last_mod_time = ret->nk->last_change;

	if (name != NULL)
		*name = talloc_steal(ctx, ret->nk->key_name);

	talloc_free(ret);

	return WERR_OK;
}

static WERROR regf_match_subkey_by_name(TALLOC_CTX *ctx,
					const struct hive_key *key,
					uint32_t offset,
					const char *name, uint32_t *ret)
{
	DATA_BLOB subkey_data;
	struct nk_block subkey;
	struct tdr_pull *pull;
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)key;

	subkey_data = hbin_get(private_data->hive, offset);
	if (!subkey_data.data) {
		DEBUG(0, ("Unable to retrieve subkey HBIN\n"));
		return WERR_GENERAL_FAILURE;
	}

	pull = tdr_pull_init(ctx);

	pull->data = subkey_data;

	if (NT_STATUS_IS_ERR(tdr_pull_nk_block(pull, ctx, &subkey))) {
		DEBUG(0, ("Error parsing NK structure.\n"));
		talloc_free(pull);
		return WERR_GENERAL_FAILURE;
	}
	talloc_free(pull);

	if (strncmp(subkey.header, "nk", 2)) {
		DEBUG(0, ("Not an NK structure.\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (!strcasecmp(subkey.key_name, name)) {
		*ret = offset;
	} else {
		*ret = 0;
	}
	return WERR_OK;
}

static WERROR regf_get_subkey_by_name(TALLOC_CTX *ctx,
				      const struct hive_key *key,
				      const char *name,
				      struct hive_key **ret)
{
	DATA_BLOB data;
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)key;
	struct nk_block *nk = private_data->nk;
	uint32_t key_off = 0;

	/* Make sure that we don't crash if the key is empty */
	if (nk->subkeys_offset == -1) {
		return WERR_BADFILE;
	}

	data = hbin_get(private_data->hive, nk->subkeys_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find subkey list\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (!strncmp((char *)data.data, "li", 2)) {
		struct li_block li;
		struct tdr_pull *pull = tdr_pull_init(ctx);
		uint16_t i;

		DEBUG(10, ("Subkeys in LI list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull, nk, &li))) {
			DEBUG(0, ("Error parsing LI list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(li.header, "li", 2));

		if (li.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}

		for (i = 0; i < li.key_count; i++) {
			W_ERROR_NOT_OK_RETURN(regf_match_subkey_by_name(nk, key,
									li.nk_offset[i],
									name,
									&key_off));
			if (key_off != 0)
				break;
		}
		if (key_off == 0)
			return WERR_BADFILE;
	} else if (!strncmp((char *)data.data, "lf", 2)) {
		struct lf_block lf;
		struct tdr_pull *pull = tdr_pull_init(ctx);
		uint16_t i;

		DEBUG(10, ("Subkeys in LF list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lf_block(pull, nk, &lf))) {
			DEBUG(0, ("Error parsing LF list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lf.header, "lf", 2));

		if (lf.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}

		for (i = 0; i < lf.key_count; i++) {
			if (strncmp(lf.hr[i].hash, name, 4)) {
				continue;
			}
			W_ERROR_NOT_OK_RETURN(regf_match_subkey_by_name(nk,
									key,
									lf.hr[i].nk_offset,
									name,
									&key_off));
			if (key_off != 0)
				break;
		}
		if (key_off == 0)
			return WERR_BADFILE;
	} else if (!strncmp((char *)data.data, "lh", 2)) {
		struct lh_block lh;
		struct tdr_pull *pull = tdr_pull_init(ctx);
		uint16_t i;
		uint32_t hash;

		DEBUG(10, ("Subkeys in LH list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull, nk, &lh))) {
			DEBUG(0, ("Error parsing LH list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lh.header, "lh", 2));

		if (lh.key_count != nk->num_subkeys) {
			DEBUG(0, ("Subkey counts don't match\n"));
			return WERR_GENERAL_FAILURE;
		}

		hash = regf_create_lh_hash(name);
		for (i = 0; i < lh.key_count; i++) {
			if (lh.hr[i].base37 != hash) {
				continue;
			}
			W_ERROR_NOT_OK_RETURN(regf_match_subkey_by_name(nk,
									key,
									lh.hr[i].nk_offset,
									name,
									&key_off));
			if (key_off != 0)
				break;
		}
		if (key_off == 0)
			return WERR_BADFILE;
	} else if (!strncmp((char *)data.data, "ri", 2)) {
		struct ri_block ri;
		struct tdr_pull *pull = tdr_pull_init(ctx);
		uint16_t i, j;

		DEBUG(10, ("Subkeys in RI list\n"));
		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_ri_block(pull, nk, &ri))) {
			DEBUG(0, ("Error parsing RI list\n"));
			talloc_free(pull);
			return WERR_GENERAL_FAILURE;
		}
		SMB_ASSERT(!strncmp(ri.header, "ri", 2));

		for (i = 0; i < ri.key_count; i++) {
			DATA_BLOB list_data;

			/* Get sublist data blob */
			list_data = hbin_get(private_data->hive, ri.offset[i]);
			if (list_data.data == NULL) {
				DEBUG(0, ("Error getting RI list."));
				talloc_free(pull);
				return WERR_GENERAL_FAILURE;
			}

			pull->data = list_data;

			if (!strncmp((char *)list_data.data, "li", 2)) {
				struct li_block li;

				if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull,
								       nk,
								       &li))) {
					DEBUG(0, ("Error parsing LI list from RI\n"));
					talloc_free(pull);
					return WERR_GENERAL_FAILURE;
				}
				SMB_ASSERT(!strncmp(li.header, "li", 2));

				for (j = 0; j < li.key_count; j++) {
					W_ERROR_NOT_OK_RETURN(regf_match_subkey_by_name(nk, key,
											li.nk_offset[j],
											name,
											&key_off));
					if (key_off)
						break;
				}
			} else if (!strncmp((char *)list_data.data, "lh", 2)) {
				struct lh_block lh;
				uint32_t hash;

				if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull,
								       nk,
								       &lh))) {
					DEBUG(0, ("Error parsing LH list from RI\n"));
					talloc_free(pull);
					return WERR_GENERAL_FAILURE;
				}
				SMB_ASSERT(!strncmp(lh.header, "lh", 2));

				hash = regf_create_lh_hash(name);
				for (j = 0; j < lh.key_count; j++) {
					if (lh.hr[j].base37 != hash) {
						continue;
					}
					W_ERROR_NOT_OK_RETURN(regf_match_subkey_by_name(nk, key,
											lh.hr[j].nk_offset,
											name,
											&key_off));
					if (key_off)
						break;
				}
			}
			if (key_off)
				break;
		}
		talloc_free(pull);
		if (!key_off)
			return WERR_BADFILE;
	} else {
		DEBUG(0, ("Unknown subkey list type.\n"));
		return WERR_GENERAL_FAILURE;
	}

	*ret = (struct hive_key *)regf_get_key(ctx, private_data->hive,
					       key_off);
	return WERR_OK;
}

static WERROR regf_set_sec_desc(struct hive_key *key,
				const struct security_descriptor *sec_desc)
{
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)key;
	struct sk_block cur_sk, sk, new_sk;
	struct regf_data *regf = private_data->hive;
	struct nk_block root;
	DATA_BLOB data;
	uint32_t sk_offset, cur_sk_offset;
	bool update_cur_sk = false;

	/* Get the root nk */
 	hbin_get_tdr(regf, regf->header->data_offset, regf,
		     (tdr_pull_fn_t) tdr_pull_nk_block, &root);

	/* Push the security descriptor to a blob */
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_push_struct_blob(&data, regf, 
							  sec_desc, (ndr_push_flags_fn_t)ndr_push_security_descriptor))) {
		DEBUG(0, ("Unable to push security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}

	/* Get the current security descriptor for the key */
	if (!hbin_get_tdr(regf, private_data->nk->sk_offset, regf,
			  (tdr_pull_fn_t) tdr_pull_sk_block, &cur_sk)) {
		DEBUG(0, ("Unable to find security descriptor for current key\n"));
		return WERR_BADFILE;
	}
	/* If there's no change, change nothing. */
	if (memcmp(data.data, cur_sk.sec_desc,
		   MIN(data.length, cur_sk.rec_size)) == 0) {
		return WERR_OK;
	}

	/* Delete the current sk if only this key is using it */
	if (cur_sk.ref_cnt == 1) {
		/* Get the previous security descriptor for the key */
		if (!hbin_get_tdr(regf, cur_sk.prev_offset, regf,
				  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
			DEBUG(0, ("Unable to find prev security descriptor for current key\n"));
			return WERR_BADFILE;
		}
		/* Change and store the previous security descriptor */
		sk.next_offset = cur_sk.next_offset;
		hbin_store_tdr_resize(regf, (tdr_push_fn_t) tdr_push_sk_block,
				      cur_sk.prev_offset, &sk);

		/* Get the next security descriptor for the key */
		if (!hbin_get_tdr(regf, cur_sk.next_offset, regf,
				  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
			DEBUG(0, ("Unable to find next security descriptor for current key\n"));
			return WERR_BADFILE;
		}
		/* Change and store the next security descriptor */
		sk.prev_offset = cur_sk.prev_offset;
		hbin_store_tdr_resize(regf, (tdr_push_fn_t) tdr_push_sk_block,
				      cur_sk.next_offset, &sk);

		hbin_free(regf, private_data->nk->sk_offset);
	} else {
		/* This key will no longer be referring to this sk */
		cur_sk.ref_cnt--;
		update_cur_sk = true;
	}

	sk_offset = root.sk_offset;

	do {
		cur_sk_offset = sk_offset;
		if (!hbin_get_tdr(regf, sk_offset, regf,
				  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
			DEBUG(0, ("Unable to find security descriptor\n"));
			return WERR_BADFILE;
		}
		if (memcmp(data.data, sk.sec_desc, MIN(data.length, sk.rec_size)) == 0) {
			private_data->nk->sk_offset = sk_offset;
			sk.ref_cnt++;
			hbin_store_tdr_resize(regf,
					      (tdr_push_fn_t) tdr_push_sk_block,
					      sk_offset, &sk);
			hbin_store_tdr_resize(regf,
					      (tdr_push_fn_t) tdr_push_nk_block,
					      private_data->offset,
					      private_data->nk);
			return WERR_OK;
		}
		sk_offset = sk.next_offset;
	} while (sk_offset != root.sk_offset);

	ZERO_STRUCT(new_sk);
	new_sk.header = "sk";
	new_sk.prev_offset = cur_sk_offset;
	new_sk.next_offset = root.sk_offset;
	new_sk.ref_cnt = 1;
	new_sk.rec_size = data.length;
	new_sk.sec_desc = data.data;

	sk_offset = hbin_store_tdr(regf,
				   (tdr_push_fn_t) tdr_push_sk_block,
				   &new_sk);
	if (sk_offset == -1) {
		DEBUG(0, ("Error storing sk block\n"));
		return WERR_GENERAL_FAILURE;
	}
	private_data->nk->sk_offset = sk_offset;

	if (update_cur_sk) {
		hbin_store_tdr_resize(regf,
				      (tdr_push_fn_t) tdr_push_sk_block,
				      private_data->nk->sk_offset, &cur_sk);
	}

	/* Get the previous security descriptor for the key */
	if (!hbin_get_tdr(regf, new_sk.prev_offset, regf,
			  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
		DEBUG(0, ("Unable to find security descriptor for previous key\n"));
		return WERR_BADFILE;
	}
	/* Change and store the previous security descriptor */
	sk.next_offset = sk_offset;
	hbin_store_tdr_resize(regf,
			      (tdr_push_fn_t) tdr_push_sk_block,
			      cur_sk.prev_offset, &sk);

	/* Get the next security descriptor for the key (always root, as we append) */
	if (!hbin_get_tdr(regf, new_sk.next_offset, regf,
			  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
		DEBUG(0, ("Unable to find security descriptor for current key\n"));
		return WERR_BADFILE;
	}
	/* Change and store the next security descriptor (always root, as we append) */
	sk.prev_offset = sk_offset;
	hbin_store_tdr_resize(regf,
			      (tdr_push_fn_t) tdr_push_sk_block,
			      root.sk_offset, &sk);


	/* Store the nk. */
	hbin_store_tdr_resize(regf,
			      (tdr_push_fn_t) tdr_push_sk_block,
			      private_data->offset, private_data->nk);
	return WERR_OK;
}

static WERROR regf_get_sec_desc(TALLOC_CTX *ctx, const struct hive_key *key,
				struct security_descriptor **sd)
{
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)key;
	struct sk_block sk;
	struct regf_data *regf = private_data->hive;
	DATA_BLOB data;

	if (!hbin_get_tdr(regf, private_data->nk->sk_offset, ctx,
			  (tdr_pull_fn_t) tdr_pull_sk_block, &sk)) {
		DEBUG(0, ("Unable to find security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (strcmp(sk.header, "sk") != 0) {
		DEBUG(0, ("Expected 'sk', got '%s'\n", sk.header));
		return WERR_GENERAL_FAILURE;
	}

	*sd = talloc(ctx, struct security_descriptor);
	W_ERROR_HAVE_NO_MEMORY(*sd);

	data.data = sk.sec_desc;
	data.length = sk.rec_size;
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_pull_struct_blob(&data, ctx, *sd,
						  (ndr_pull_flags_fn_t)ndr_pull_security_descriptor))) {
		DEBUG(0, ("Error parsing security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}

	return WERR_OK;
}

static WERROR regf_sl_add_entry(struct regf_data *regf, uint32_t list_offset,
				const char *name,
				uint32_t key_offset, uint32_t *ret)
{
	DATA_BLOB data;

	/* Create a new key if necessary */
	if (list_offset == -1) {
		if (regf->header->version.major != 1) {
			DEBUG(0, ("Can't store keys in unknown registry format\n"));
			return WERR_NOT_SUPPORTED;
		}
		if (regf->header->version.minor < 3) {
			/* Store LI */
			struct li_block li;
			ZERO_STRUCT(li);
			li.header = "li";
			li.key_count = 1;

			li.nk_offset = talloc_array(regf, uint32_t, 1);
			W_ERROR_HAVE_NO_MEMORY(li.nk_offset);
			li.nk_offset[0] = key_offset;

			*ret = hbin_store_tdr(regf,
					      (tdr_push_fn_t) tdr_push_li_block,
					      &li);

			talloc_free(li.nk_offset);
		} else if (regf->header->version.minor == 3 ||
			   regf->header->version.minor == 4) {
			/* Store LF */
			struct lf_block lf;
			ZERO_STRUCT(lf);
			lf.header = "lf";
			lf.key_count = 1;

			lf.hr = talloc_array(regf, struct hash_record, 1);
			W_ERROR_HAVE_NO_MEMORY(lf.hr);
			lf.hr[0].nk_offset = key_offset;
			lf.hr[0].hash = talloc_strndup(lf.hr, name, 4);
			W_ERROR_HAVE_NO_MEMORY(lf.hr[0].hash);

			*ret = hbin_store_tdr(regf,
					      (tdr_push_fn_t) tdr_push_lf_block,
					      &lf);

			talloc_free(lf.hr);
		} else if (regf->header->version.minor == 5) {
			/* Store LH */
			struct lh_block lh;
			ZERO_STRUCT(lh);
			lh.header = "lh";
			lh.key_count = 1;

			lh.hr = talloc_array(regf, struct lh_hash, 1);
			W_ERROR_HAVE_NO_MEMORY(lh.hr);
			lh.hr[0].nk_offset = key_offset;
			lh.hr[0].base37 = regf_create_lh_hash(name);

			*ret = hbin_store_tdr(regf,
					      (tdr_push_fn_t) tdr_push_lh_block,
					      &lh);

			talloc_free(lh.hr);
		}
		return WERR_OK;
	}

	data = hbin_get(regf, list_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find subkey list\n"));
		return WERR_BADFILE;
	}

	if (!strncmp((char *)data.data, "li", 2)) {
		struct tdr_pull *pull = tdr_pull_init(regf);
		struct li_block li;
		struct nk_block sub_nk;
		int32_t i, j;

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull, regf, &li))) {
			DEBUG(0, ("Error parsing LI list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);

		if (strncmp(li.header, "li", 2) != 0) {
			abort();
			DEBUG(0, ("LI header corrupt\n"));
			return WERR_BADFILE;
		}

		/* 
		 * Find the position to store the pointer
		 * Extensive testing reveils that at least on windows 7 subkeys 
		 * *MUST* be stored in alphabetical order
		 */
		for (i = 0; i < li.key_count; i++) {
			/* Get the nk */
 			hbin_get_tdr(regf, li.nk_offset[i], regf,
					(tdr_pull_fn_t) tdr_pull_nk_block, &sub_nk);
			if (strcasecmp(name, sub_nk.key_name) < 0) {
				break;
			}
		}

		li.nk_offset = talloc_realloc(regf, li.nk_offset,
					      uint32_t, li.key_count+1);
		W_ERROR_HAVE_NO_MEMORY(li.nk_offset);

		/* Move everything behind this offset */
		for (j = li.key_count - 1; j >= i; j--) {
			li.nk_offset[j+1] = li.nk_offset[j];
		}
			
		li.nk_offset[i] = key_offset;
		li.key_count++;
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t)tdr_push_li_block,
					     list_offset, &li);

		talloc_free(li.nk_offset);
	} else if (!strncmp((char *)data.data, "lf", 2)) {
		struct tdr_pull *pull = tdr_pull_init(regf);
		struct lf_block lf;
		struct nk_block sub_nk;
		int32_t i, j;

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lf_block(pull, regf, &lf))) {
			DEBUG(0, ("Error parsing LF list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lf.header, "lf", 2));

		/* 
		 * Find the position to store the hash record
		 * Extensive testing reveils that at least on windows 7 subkeys 
		 * *MUST* be stored in alphabetical order
		 */
		for (i = 0; i < lf.key_count; i++) {
			/* Get the nk */
 			hbin_get_tdr(regf, lf.hr[i].nk_offset, regf,
					(tdr_pull_fn_t) tdr_pull_nk_block, &sub_nk);
			if (strcasecmp(name, sub_nk.key_name) < 0) {
				break;
			}
		}

		lf.hr = talloc_realloc(regf, lf.hr, struct hash_record,
				       lf.key_count+1);
		W_ERROR_HAVE_NO_MEMORY(lf.hr);

		/* Move everything behind this hash record */
		for (j = lf.key_count - 1; j >= i; j--) {
			lf.hr[j+1] = lf.hr[j];
		}

		lf.hr[i].nk_offset = key_offset;
		lf.hr[i].hash = talloc_strndup(lf.hr, name, 4);
		W_ERROR_HAVE_NO_MEMORY(lf.hr[lf.key_count].hash);
		lf.key_count++;
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t)tdr_push_lf_block,
					     list_offset, &lf);

		talloc_free(lf.hr);
	} else if (!strncmp((char *)data.data, "lh", 2)) {
		struct tdr_pull *pull = tdr_pull_init(regf);
		struct lh_block lh;
		struct nk_block sub_nk;
		int32_t i, j;

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull, regf, &lh))) {
			DEBUG(0, ("Error parsing LH list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);
		SMB_ASSERT(!strncmp(lh.header, "lh", 2));

		/* 
		 * Find the position to store the hash record
		 * Extensive testing reveils that at least on windows 7 subkeys 
		 * *MUST* be stored in alphabetical order
		 */
		for (i = 0; i < lh.key_count; i++) {
			/* Get the nk */
 			hbin_get_tdr(regf, lh.hr[i].nk_offset, regf,
					(tdr_pull_fn_t) tdr_pull_nk_block, &sub_nk);
			if (strcasecmp(name, sub_nk.key_name) < 0) {
				break;
			}
		}

		lh.hr = talloc_realloc(regf, lh.hr, struct lh_hash,
				       lh.key_count+1);
		W_ERROR_HAVE_NO_MEMORY(lh.hr);

		/* Move everything behind this hash record */
		for (j = lh.key_count - 1; j >= i; j--) {
			lh.hr[j+1] = lh.hr[j];
		}

		lh.hr[i].nk_offset = key_offset;
		lh.hr[i].base37 = regf_create_lh_hash(name);
		lh.key_count++;
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t)tdr_push_lh_block,
					     list_offset, &lh);

		talloc_free(lh.hr);
	} else if (!strncmp((char *)data.data, "ri", 2)) {
		/* FIXME */
		DEBUG(0, ("Adding to 'ri' subkey list is not supported yet.\n"));
		return WERR_NOT_SUPPORTED;
	} else {
		DEBUG(0, ("Cannot add to unknown subkey list\n"));
		return WERR_BADFILE;
	}

	return WERR_OK;
}

static WERROR regf_sl_del_entry(struct regf_data *regf, uint32_t list_offset,
				uint32_t key_offset, uint32_t *ret)
{
	DATA_BLOB data;

	data = hbin_get(regf, list_offset);
	if (!data.data) {
		DEBUG(0, ("Unable to find subkey list\n"));
		return WERR_BADFILE;
	}

	if (strncmp((char *)data.data, "li", 2) == 0) {
		struct li_block li;
		struct tdr_pull *pull = tdr_pull_init(regf);
		uint16_t i;
		bool found_offset = false;

		DEBUG(10, ("Subkeys in LI list\n"));

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_li_block(pull, regf, &li))) {
			DEBUG(0, ("Error parsing LI list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);

		SMB_ASSERT(!strncmp(li.header, "li", 2));

		for (i = 0; i < li.key_count; i++) {
			if (found_offset) {
				li.nk_offset[i-1] = li.nk_offset[i];
			}
			if (li.nk_offset[i] == key_offset) {
				found_offset = true;
				continue;
			}
		}
		if (!found_offset) {
			DEBUG(2, ("Subkey not found\n"));
			return WERR_BADFILE;
		}
		li.key_count--;

		/* If the there are no entries left, free the subkey list */
		if (li.key_count == 0) {
			hbin_free(regf, list_offset);
			*ret = -1;
		}

		/* Store li block */
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t) tdr_push_li_block,
					     list_offset, &li);
	} else if (strncmp((char *)data.data, "lf", 2) == 0) {
		struct lf_block lf;
		struct tdr_pull *pull = tdr_pull_init(regf);
		uint16_t i;
		bool found_offset = false;

		DEBUG(10, ("Subkeys in LF list\n"));

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lf_block(pull, regf, &lf))) {
			DEBUG(0, ("Error parsing LF list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);

		SMB_ASSERT(!strncmp(lf.header, "lf", 2));

		for (i = 0; i < lf.key_count; i++) {
			if (found_offset) {
				lf.hr[i-1] = lf.hr[i];
				continue;
			}
			if (lf.hr[i].nk_offset == key_offset) {
				found_offset = 1;
				continue;
			}
		}
		if (!found_offset) {
			DEBUG(2, ("Subkey not found\n"));
			return WERR_BADFILE;
		}
		lf.key_count--;

		/* If the there are no entries left, free the subkey list */
		if (lf.key_count == 0) {
			hbin_free(regf, list_offset);
			*ret = -1;
			return WERR_OK;
		}

		/* Store lf block */
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t) tdr_push_lf_block,
					     list_offset, &lf);
	} else if (strncmp((char *)data.data, "lh", 2) == 0) {
		struct lh_block lh;
		struct tdr_pull *pull = tdr_pull_init(regf);
		uint16_t i;
		bool found_offset = false;

		DEBUG(10, ("Subkeys in LH list\n"));

		pull->data = data;

		if (NT_STATUS_IS_ERR(tdr_pull_lh_block(pull, regf, &lh))) {
			DEBUG(0, ("Error parsing LF list\n"));
			talloc_free(pull);
			return WERR_BADFILE;
		}
		talloc_free(pull);

		SMB_ASSERT(!strncmp(lh.header, "lh", 2));

		for (i = 0; i < lh.key_count; i++) {
			if (found_offset) {
				lh.hr[i-1] = lh.hr[i];
				continue;
			}
			if (lh.hr[i].nk_offset == key_offset) {
				found_offset = 1;
				continue;
			}
		}
		if (!found_offset) {
			DEBUG(0, ("Subkey not found\n"));
			return WERR_BADFILE;
		}
		lh.key_count--;

		/* If the there are no entries left, free the subkey list */
		if (lh.key_count == 0) {
			hbin_free(regf, list_offset);
			*ret = -1;
			return WERR_OK;
		}

		/* Store lh block */
		*ret = hbin_store_tdr_resize(regf,
					     (tdr_push_fn_t) tdr_push_lh_block,
					     list_offset, &lh);
	} else if (strncmp((char *)data.data, "ri", 2) == 0) {
		/* FIXME */
		DEBUG(0, ("Sorry, deletion from ri block is not supported yet.\n"));
		return WERR_NOT_SUPPORTED;
	} else {
		DEBUG (0, ("Unknown header found in subkey list.\n"));
		return WERR_BADFILE;
	}
	return WERR_OK;
}

static WERROR regf_del_value(TALLOC_CTX *mem_ctx, struct hive_key *key,
			     const char *name)
{
	struct regf_key_data *private_data = (struct regf_key_data *)key;
	struct regf_data *regf = private_data->hive;
	struct nk_block *nk = private_data->nk;
	struct vk_block vk;
	uint32_t vk_offset;
	bool found_offset = false;
	DATA_BLOB values;
	unsigned int i;

	if (nk->values_offset == -1) {
		return WERR_BADFILE;
	}

	values = hbin_get(regf, nk->values_offset);

	for (i = 0; i < nk->num_values; i++) {
		if (found_offset) {
			((uint32_t *)values.data)[i-1] = ((uint32_t *) values.data)[i];
		} else {
			vk_offset = IVAL(values.data, i * 4);
			if (!hbin_get_tdr(regf, vk_offset, private_data,
					  (tdr_pull_fn_t)tdr_pull_vk_block,
					  &vk)) {
				DEBUG(0, ("Unable to get VK block at %d\n",
					vk_offset));
				return WERR_BADFILE;
			}
			if (strcmp(vk.data_name, name) == 0) {
				hbin_free(regf, vk_offset);
				found_offset = true;
			}
		}
	}
	if (!found_offset) {
		return WERR_BADFILE;
	} else {
		nk->num_values--;
		values.length = (nk->num_values)*4;
	}

	/* Store values list and nk */
	if (nk->num_values == 0) {
		hbin_free(regf, nk->values_offset);
		nk->values_offset = -1;
	} else {
		nk->values_offset = hbin_store_resize(regf,
						      nk->values_offset,
						      values);
	}
	hbin_store_tdr_resize(regf, (tdr_push_fn_t) tdr_push_nk_block,
			      private_data->offset, nk);

	return regf_save_hbin(private_data->hive, 0);
}


static WERROR regf_del_key(TALLOC_CTX *mem_ctx, const struct hive_key *parent,
			   const char *name)
{
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)parent;
	struct regf_key_data *key;
	struct nk_block *parent_nk;
	WERROR error;

	SMB_ASSERT(private_data);

	parent_nk = private_data->nk;

	if (parent_nk->subkeys_offset == -1) {
		DEBUG(4, ("Subkey list is empty, this key cannot contain subkeys.\n"));
		return WERR_BADFILE;
	}

	/* Find the key */
	if (!W_ERROR_IS_OK(regf_get_subkey_by_name(parent_nk, parent, name,
						   (struct hive_key **)&key))) {
		DEBUG(2, ("Key '%s' not found\n", name));
		return WERR_BADFILE;
	}

	if (key->nk->subkeys_offset != -1) {
		char *sk_name;
		struct hive_key *sk = (struct hive_key *)key;
		unsigned int i = key->nk->num_subkeys;
		while (i--) {
			/* Get subkey information. */
			error = regf_get_subkey_by_index(parent_nk, sk, 0,
							 (const char **)&sk_name,
							 NULL, NULL);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Can't retrieve subkey by index.\n"));
				return error;
			}

			/* Delete subkey. */
			error = regf_del_key(NULL, sk, sk_name);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Can't delete key '%s'.\n", sk_name));
				return error;
			}

			talloc_free(sk_name);
		}
	}

	if (key->nk->values_offset != -1) {
		char *val_name;
		struct hive_key *sk = (struct hive_key *)key;
		DATA_BLOB data;
		unsigned int i = key->nk->num_values;
		while (i--) {
			/* Get value information. */
			error = regf_get_value(parent_nk, sk, 0,
					       (const char **)&val_name,
					       NULL, &data);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Can't retrieve value by index.\n"));
				return error;
			}

			/* Delete value. */
			error = regf_del_value(NULL, sk, val_name);
			if (!W_ERROR_IS_OK(error)) {
				DEBUG(0, ("Can't delete value '%s'.\n", val_name));
				return error;
			}

			talloc_free(val_name);
		}
	}

	/* Delete it from the subkey list. */
	error = regf_sl_del_entry(private_data->hive, parent_nk->subkeys_offset,
				  key->offset, &parent_nk->subkeys_offset);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Can't store new subkey list for parent key. Won't delete.\n"));
		return error;
	}

	/* Re-store parent key */
	parent_nk->num_subkeys--;
	hbin_store_tdr_resize(private_data->hive,
			      (tdr_push_fn_t) tdr_push_nk_block,
			      private_data->offset, parent_nk);

	if (key->nk->clsname_offset != -1) {
		hbin_free(private_data->hive, key->nk->clsname_offset);
	}
	hbin_free(private_data->hive, key->offset);

	return regf_save_hbin(private_data->hive, 0);
}

static WERROR regf_add_key(TALLOC_CTX *ctx, const struct hive_key *parent,
			   const char *name, const char *classname,
			   struct security_descriptor *sec_desc,
			   struct hive_key **ret)
{
	const struct regf_key_data *private_data =
		(const struct regf_key_data *)parent;
	struct nk_block *parent_nk = private_data->nk, nk;
	struct nk_block *root;
	struct regf_data *regf = private_data->hive;
	uint32_t offset;
	WERROR error;

	nk.header = "nk";
	nk.type = REG_SUB_KEY;
	unix_to_nt_time(&nk.last_change, time(NULL));
	nk.uk1 = 0;
	nk.parent_offset = private_data->offset;
	nk.num_subkeys = 0;
	nk.uk2 = 0;
	nk.subkeys_offset = -1;
	nk.unknown_offset = -1;
	nk.num_values = 0;
	nk.values_offset = -1;
	memset(nk.unk3, 0, sizeof(nk.unk3));
	nk.clsname_offset = -1; /* FIXME: fill in */
	nk.clsname_length = 0;
	nk.key_name = name;

	/* Get the security descriptor of the root key */
 	root = talloc_zero(ctx, struct nk_block);
	W_ERROR_HAVE_NO_MEMORY(root);

	if (!hbin_get_tdr(regf, regf->header->data_offset, root,
			  (tdr_pull_fn_t)tdr_pull_nk_block, root)) {
		DEBUG(0, ("Unable to find HBIN data for offset 0x%x\n",
			regf->header->data_offset));
		return WERR_GENERAL_FAILURE;
	}
	nk.sk_offset = root->sk_offset;
	talloc_free(root);

	/* Store the new nk key */
	offset = hbin_store_tdr(regf, (tdr_push_fn_t) tdr_push_nk_block, &nk);

	error = regf_sl_add_entry(regf, parent_nk->subkeys_offset, name, offset,
				  &parent_nk->subkeys_offset);
	if (!W_ERROR_IS_OK(error)) {
		hbin_free(regf, offset);
		return error;
	}

	parent_nk->num_subkeys++;

	/* Since the subkey offset of the parent can change, store it again */
	hbin_store_tdr_resize(regf, (tdr_push_fn_t) tdr_push_nk_block,
						  nk.parent_offset, parent_nk);

	*ret = (struct hive_key *)regf_get_key(ctx, regf, offset);

	DEBUG(9, ("Storing key %s\n", name));
	return regf_save_hbin(private_data->hive, 0);
}

static WERROR regf_set_value(struct hive_key *key, const char *name,
			     uint32_t type, const DATA_BLOB data)
{
	struct regf_key_data *private_data = (struct regf_key_data *)key;
	struct regf_data *regf = private_data->hive;
	struct nk_block *nk = private_data->nk;
	struct vk_block vk;
	uint32_t i;
	uint32_t tmp_vk_offset, vk_offset, old_vk_offset = (uint32_t) -1;
	DATA_BLOB values;

	ZERO_STRUCT(vk);

	/* find the value offset, if it exists */
	if (nk->values_offset != -1) {
		values = hbin_get(regf, nk->values_offset);

		for (i = 0; i < nk->num_values; i++) {
			tmp_vk_offset = IVAL(values.data, i * 4);
			if (!hbin_get_tdr(regf, tmp_vk_offset, private_data,
					  (tdr_pull_fn_t)tdr_pull_vk_block,
					  &vk)) {
				DEBUG(0, ("Unable to get VK block at 0x%x\n",
					tmp_vk_offset));
				return WERR_GENERAL_FAILURE;
			}
			if (strcmp(vk.data_name, name) == 0) {
				old_vk_offset = tmp_vk_offset;
				break;
			}
		}
	}

	/* If it's new, create the vk struct, if it's old, free the old data. */
	if (old_vk_offset == -1) {
		vk.header = "vk";
		vk.name_length = strlen(name);
		if (name != NULL && name[0] != 0) {
			vk.flag = 1;
			vk.data_name = name;
		} else {
			vk.data_name = NULL;
			vk.flag = 0;
		}
	} else {
		/* Free data, if any */
		if (!(vk.data_length & 0x80000000)) {
			hbin_free(regf, vk.data_offset);
		}
	}

	/* Set the type and data */
	vk.data_length = data.length;
	vk.data_type = type;
	if ((type == REG_DWORD) || (type == REG_DWORD_BIG_ENDIAN)) {
		if (vk.data_length != sizeof(uint32_t)) {
			DEBUG(0, ("DWORD or DWORD_BIG_ENDIAN value with size other than 4 byte!\n"));
			return WERR_NOT_SUPPORTED;
		}
		vk.data_length |= 0x80000000;
		vk.data_offset = IVAL(data.data, 0);
	} else {
		/* Store data somewhere */
		vk.data_offset = hbin_store(regf, data);
	}
	if (old_vk_offset == -1) {
		/* Store new vk */
		vk_offset = hbin_store_tdr(regf,
					   (tdr_push_fn_t) tdr_push_vk_block,
					   &vk);
	} else {
		/* Store vk at offset */
		vk_offset = hbin_store_tdr_resize(regf,
						  (tdr_push_fn_t) tdr_push_vk_block,
						  old_vk_offset ,&vk);
	}

	/* Re-allocate the value list */
	if (nk->values_offset == -1) {
		nk->values_offset = hbin_store_tdr(regf,
						   (tdr_push_fn_t) tdr_push_uint32,
						   &vk_offset);
		nk->num_values = 1;
	} else {

		/* Change if we're changing, otherwise we're adding the value */
		if (old_vk_offset != -1) {
			/* Find and overwrite the offset. */
			for (i = 0; i < nk->num_values; i++) {
				if (IVAL(values.data, i * 4) == old_vk_offset) {
					SIVAL(values.data, i * 4, vk_offset);
					break;
				}
			}
		} else {
			/* Create a new value list */
			DATA_BLOB value_list;

			value_list.length = (nk->num_values+1)*4;
			value_list.data = (uint8_t *)talloc_array(private_data,
								  uint32_t,
								  nk->num_values+1);
			W_ERROR_HAVE_NO_MEMORY(value_list.data);
			memcpy(value_list.data, values.data, nk->num_values * 4);

			SIVAL(value_list.data, nk->num_values * 4, vk_offset);
			nk->num_values++;
			nk->values_offset = hbin_store_resize(regf,
							      nk->values_offset,
							      value_list);
		}

	}
	hbin_store_tdr_resize(regf,
			      (tdr_push_fn_t) tdr_push_nk_block,
			      private_data->offset, nk);
	return regf_save_hbin(private_data->hive, 0);
}

static WERROR regf_save_hbin(struct regf_data *regf, bool flush)
{
	struct tdr_push *push = tdr_push_init(regf);
	unsigned int i;

	W_ERROR_HAVE_NO_MEMORY(push);

	/* Only write once every 5 seconds, or when flush is set */
	if (!flush && regf->last_write + 5 >= time(NULL)) {
		return WERR_OK;
	}

	regf->last_write = time(NULL);

	if (lseek(regf->fd, 0, SEEK_SET) == -1) {
		DEBUG(0, ("Error lseeking in regf file\n"));
		return WERR_GENERAL_FAILURE;
	}

	/* Recompute checksum */
	if (NT_STATUS_IS_ERR(tdr_push_regf_hdr(push, regf->header))) {
		DEBUG(0, ("Failed to push regf header\n"));
		return WERR_GENERAL_FAILURE;
	}
	regf->header->chksum = regf_hdr_checksum(push->data.data);
	talloc_free(push);

	if (NT_STATUS_IS_ERR(tdr_push_to_fd(regf->fd,
					    (tdr_push_fn_t)tdr_push_regf_hdr,
					    regf->header))) {
		DEBUG(0, ("Error writing registry file header\n"));
		return WERR_GENERAL_FAILURE;
	}

	if (lseek(regf->fd, 0x1000, SEEK_SET) == -1) {
		DEBUG(0, ("Error lseeking to 0x1000 in regf file\n"));
		return WERR_GENERAL_FAILURE;
	}

	for (i = 0; regf->hbins[i]; i++) {
		if (NT_STATUS_IS_ERR(tdr_push_to_fd(regf->fd, 
						    (tdr_push_fn_t)tdr_push_hbin_block,
						    regf->hbins[i]))) {
			DEBUG(0, ("Error writing HBIN block\n"));
			return WERR_GENERAL_FAILURE;
		}
	}

	return WERR_OK;
}

WERROR reg_create_regf_file(TALLOC_CTX *parent_ctx, 
			    const char *location,
			    int minor_version, struct hive_key **key)
{
	struct regf_data *regf;
	struct regf_hdr *regf_hdr;
	struct nk_block nk;
	struct sk_block sk;
	WERROR error;
	DATA_BLOB data;
	struct security_descriptor *sd;
	uint32_t sk_offset;

	regf = (struct regf_data *)talloc_zero(NULL, struct regf_data);

	W_ERROR_HAVE_NO_MEMORY(regf);

	DEBUG(5, ("Attempting to create registry file\n"));

	/* Get the header */
	regf->fd = creat(location, 0644);

	if (regf->fd == -1) {
		DEBUG(0,("Could not create file: %s, %s\n", location,
				 strerror(errno)));
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	regf_hdr = talloc_zero(regf, struct regf_hdr);
	W_ERROR_HAVE_NO_MEMORY(regf_hdr);
	regf_hdr->REGF_ID = "regf";
	unix_to_nt_time(&regf_hdr->modtime, time(NULL));
	regf_hdr->version.major = 1;
	regf_hdr->version.minor = minor_version;
	regf_hdr->last_block = 0x1000; /* Block size */
	regf_hdr->description = talloc_strdup(regf_hdr,
					      "Registry created by Samba 4");
	W_ERROR_HAVE_NO_MEMORY(regf_hdr->description);
	regf_hdr->chksum = 0;

	regf->header = regf_hdr;

	/* Create all hbin blocks */
	regf->hbins = talloc_array(regf, struct hbin_block *, 1);
	W_ERROR_HAVE_NO_MEMORY(regf->hbins);
	regf->hbins[0] = NULL;

	nk.header = "nk";
	nk.type = REG_ROOT_KEY;
	unix_to_nt_time(&nk.last_change, time(NULL));
	nk.uk1 = 0;
	nk.parent_offset = -1;
	nk.num_subkeys = 0;
	nk.uk2 = 0;
	nk.subkeys_offset = -1;
	nk.unknown_offset = -1;
	nk.num_values = 0;
	nk.values_offset = -1;
	memset(nk.unk3, 0, 5);
	nk.clsname_offset = -1;
	nk.clsname_length = 0;
	nk.sk_offset = 0x80;
	nk.key_name = "SambaRootKey";

	/*
	 * It should be noted that changing the key_name to something shorter
	 * creates a shorter nk block, which makes the position of the sk block
	 * change. All Windows registries I've seen have the sk at 0x80. 
	 * I therefore recommend that our regf files share that offset -- Wilco
	 */

	/* Create a security descriptor. */
	sd = security_descriptor_dacl_create(regf,
					 0,
					 NULL, NULL,
					 SID_NT_AUTHENTICATED_USERS,
					 SEC_ACE_TYPE_ACCESS_ALLOWED,
					 SEC_GENERIC_ALL,
					 SEC_ACE_FLAG_OBJECT_INHERIT,
					 NULL);
	
	/* Push the security descriptor to a blob */
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_push_struct_blob(&data, regf, 
				     sd, (ndr_push_flags_fn_t)ndr_push_security_descriptor))) {
		DEBUG(0, ("Unable to push security descriptor\n"));
		return WERR_GENERAL_FAILURE;
	}

	ZERO_STRUCT(sk);
	sk.header = "sk";
	sk.prev_offset = 0x80;
	sk.next_offset = 0x80;
	sk.ref_cnt = 1;
	sk.rec_size = data.length;
	sk.sec_desc = data.data;

	/* Store the new nk key */
	regf->header->data_offset = hbin_store_tdr(regf,
						   (tdr_push_fn_t)tdr_push_nk_block,
						   &nk);
	/* Store the sk block */
	sk_offset = hbin_store_tdr(regf,
				   (tdr_push_fn_t) tdr_push_sk_block,
				   &sk);
	if (sk_offset != 0x80) {
		DEBUG(0, ("Error storing sk block, should be at 0x80, stored at 0x%x\n", nk.sk_offset));
		return WERR_GENERAL_FAILURE;
	}


	*key = (struct hive_key *)regf_get_key(parent_ctx, regf,
					       regf->header->data_offset);

	error = regf_save_hbin(regf, 1);
	if (!W_ERROR_IS_OK(error)) {
		return error;
	}
	
	/* We can drop our own reference now that *key will have created one */
	talloc_unlink(NULL, regf);

	return WERR_OK;
}

static WERROR regf_flush_key(struct hive_key *key)
{
	struct regf_key_data *private_data = (struct regf_key_data *)key;
	struct regf_data *regf = private_data->hive;
	WERROR error;

	error = regf_save_hbin(regf, 1);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Failed to flush regf to disk\n"));
		return error;
	}

	return WERR_OK;
}

static int regf_destruct(struct regf_data *regf)
{
	WERROR error;

	/* Write to disk */
	error = regf_save_hbin(regf, 1);
	if (!W_ERROR_IS_OK(error)) {
		DEBUG(0, ("Failed to flush registry to disk\n"));
		return -1;
	}

	/* Close file descriptor */
	close(regf->fd);

	return 0;
}

WERROR reg_open_regf_file(TALLOC_CTX *parent_ctx, const char *location, 
			  struct hive_key **key)
{
	struct regf_data *regf;
	struct regf_hdr *regf_hdr;
	struct tdr_pull *pull;
	unsigned int i;

	regf = (struct regf_data *)talloc_zero(parent_ctx, struct regf_data);
	W_ERROR_HAVE_NO_MEMORY(regf);

	talloc_set_destructor(regf, regf_destruct);

	DEBUG(5, ("Attempting to load registry file\n"));

	/* Get the header */
	regf->fd = open(location, O_RDWR);

	if (regf->fd == -1) {
		DEBUG(0,("Could not load file: %s, %s\n", location,
				 strerror(errno)));
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	pull = tdr_pull_init(regf);

	pull->data.data = (uint8_t*)fd_load(regf->fd, &pull->data.length, 0, regf);

	if (pull->data.data == NULL) {
		DEBUG(0, ("Error reading data\n"));
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	regf_hdr = talloc(regf, struct regf_hdr);
	W_ERROR_HAVE_NO_MEMORY(regf_hdr);

	if (NT_STATUS_IS_ERR(tdr_pull_regf_hdr(pull, regf_hdr, regf_hdr))) {
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	regf->header = regf_hdr;

	if (strcmp(regf_hdr->REGF_ID, "regf") != 0) {
		DEBUG(0, ("Unrecognized NT registry header id: %s, %s\n",
			regf_hdr->REGF_ID, location));
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	/* Validate the header ... */
	if (regf_hdr_checksum(pull->data.data) != regf_hdr->chksum) {
		DEBUG(0, ("Registry file checksum error: %s: %d,%d\n",
			location, regf_hdr->chksum,
			regf_hdr_checksum(pull->data.data)));
		talloc_free(regf);
		return WERR_GENERAL_FAILURE;
	}

	pull->offset = 0x1000;

	i = 0;
	/* Read in all hbin blocks */
	regf->hbins = talloc_array(regf, struct hbin_block *, 1);
	W_ERROR_HAVE_NO_MEMORY(regf->hbins);

	regf->hbins[0] = NULL;

	while (pull->offset < pull->data.length &&
	       pull->offset <= regf->header->last_block) {
		struct hbin_block *hbin = talloc(regf->hbins,
						 struct hbin_block);

		W_ERROR_HAVE_NO_MEMORY(hbin);

		if (NT_STATUS_IS_ERR(tdr_pull_hbin_block(pull, hbin, hbin))) {
			DEBUG(0, ("[%d] Error parsing HBIN block\n", i));
			talloc_free(regf);
			return WERR_FOOBAR;
		}

		if (strcmp(hbin->HBIN_ID, "hbin") != 0) {
			DEBUG(0, ("[%d] Expected 'hbin', got '%s'\n",
				i, hbin->HBIN_ID));
			talloc_free(regf);
			return WERR_FOOBAR;
		}

		regf->hbins[i] = hbin;
		i++;
		regf->hbins = talloc_realloc(regf, regf->hbins,
					     struct hbin_block *, i+2);
		regf->hbins[i] = NULL;
	}

	talloc_free(pull);

	DEBUG(1, ("%d HBIN blocks read\n", i));

	*key = (struct hive_key *)regf_get_key(parent_ctx, regf,
					       regf->header->data_offset);

	/* We can drop our own reference now that *key will have created one */
	talloc_unlink(parent_ctx, regf);

	return WERR_OK;
}

static struct hive_operations reg_backend_regf = {
	.name = "regf",
	.get_key_info = regf_get_info,
	.enum_key = regf_get_subkey_by_index,
	.get_key_by_name = regf_get_subkey_by_name,
	.get_value_by_name = regf_get_value_by_name,
	.enum_value = regf_get_value,
	.get_sec_desc = regf_get_sec_desc,
	.set_sec_desc = regf_set_sec_desc,
	.add_key = regf_add_key,
	.set_value = regf_set_value,
	.del_key = regf_del_key,
	.delete_value = regf_del_value,
	.flush_key = regf_flush_key
};
