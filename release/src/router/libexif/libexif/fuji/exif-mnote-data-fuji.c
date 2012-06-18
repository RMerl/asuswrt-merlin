/* exif-mnote-data-fuji.c
 *
 * Copyright (c) 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA.
 */

#include <stdlib.h>
#include <string.h>


#include <config.h>
#include <libexif/exif-byte-order.h>
#include <libexif/exif-utils.h>

#include "exif-mnote-data-fuji.h"

struct _MNoteFujiDataPrivate {
	ExifByteOrder order;
};

static void
exif_mnote_data_fuji_clear (ExifMnoteDataFuji *n)
{
	ExifMnoteData *d = (ExifMnoteData *) n;
	unsigned int i;

	if (!n) return;

	if (n->entries) {
		for (i = 0; i < n->count; i++)
			if (n->entries[i].data) {
				exif_mem_free (d->mem, n->entries[i].data);
				n->entries[i].data = NULL;
			}
		exif_mem_free (d->mem, n->entries);
		n->entries = NULL;
		n->count = 0;
	}
}

static void
exif_mnote_data_fuji_free (ExifMnoteData *n)
{
	if (!n) return;

	exif_mnote_data_fuji_clear ((ExifMnoteDataFuji *) n);
}

static char *
exif_mnote_data_fuji_get_value (ExifMnoteData *d, unsigned int i, char *val, unsigned int maxlen)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) d;

	if (!d || !val) return NULL;
	if (i > n->count -1) return NULL;
	exif_log (d->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteDataFuji",
		  "Querying value for tag '%s'...",
		  mnote_fuji_tag_get_name (n->entries[i].tag));
	return mnote_fuji_entry_get_value (&n->entries[i], val, maxlen);
}

static void
exif_mnote_data_fuji_save (ExifMnoteData *ne, unsigned char **buf,
			   unsigned int *buf_size)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) ne;
	size_t i, o, s, doff;
	unsigned char *t;
	size_t ts;

	if (!n || !buf || !buf_size) return;

	/*
	 * Allocate enough memory for all entries and the number
	 * of entries.
	 */
	*buf_size = 8 + 4 + 2 + n->count * 12 + 4;
	*buf = exif_mem_alloc (ne->mem, *buf_size);
	if (!*buf) {
		*buf_size = 0;
		return;
	}

	/*
	 * Header: "FUJIFILM" and 4 bytes offset to the first entry.
	 * As the first entry will start right thereafter, the offset is 12.
	 */
	memcpy (*buf, "FUJIFILM", 8);
	exif_set_long (*buf + 8, n->order, 12);

	/* Save the number of entries */
	exif_set_short (*buf + 8 + 4, n->order, (ExifShort) n->count);
	
	/* Save each entry */
	for (i = 0; i < n->count; i++) {
		o = 8 + 4 + 2 + i * 12;
		exif_set_short (*buf + o + 0, n->order, (ExifShort) n->entries[i].tag);
		exif_set_short (*buf + o + 2, n->order, (ExifShort) n->entries[i].format);
		exif_set_long  (*buf + o + 4, n->order, n->entries[i].components);
		o += 8;
		s = exif_format_get_size (n->entries[i].format) *
						n->entries[i].components;
		if (s > 65536) {
			/* Corrupt data: EXIF data size is limited to the
			 * maximum size of a JPEG segment (64 kb).
			 */
			continue;
		}
		if (s > 4) {
			ts = *buf_size + s;

			/* Ensure even offsets. Set padding bytes to 0. */
			if (s & 1) ts += 1;
			t = exif_mem_realloc (ne->mem, *buf, ts);
			if (!t) {
				return;
			}
			*buf = t;
			*buf_size = ts;
			doff = *buf_size - s;
			if (s & 1) { doff--; *(*buf + *buf_size - 1) = '\0'; }
			exif_set_long (*buf + o, n->order, doff);
		} else
			doff = o;

		/*
		 * Write the data. Fill unneeded bytes with 0. Do not
		 * crash if data is NULL.
		 */
		if (!n->entries[i].data) memset (*buf + doff, 0, s);
		else memcpy (*buf + doff, n->entries[i].data, s);
	}
}

static void
exif_mnote_data_fuji_load (ExifMnoteData *en,
	const unsigned char *buf, unsigned int buf_size)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji*) en;
	ExifLong c;
	size_t i, tcount, o, datao;

	if (!n || !buf || !buf_size) {
		exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteDataFuji", "Short MakerNote");
		return;
	}
	datao = 6 + n->offset;
	if ((datao + 12 < datao) || (datao + 12 < 12) || (datao + 12 > buf_size)) {
		exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteDataFuji", "Short MakerNote");
		return;
	}

	n->order = EXIF_BYTE_ORDER_INTEL;
	datao += exif_get_long (buf + datao + 8, EXIF_BYTE_ORDER_INTEL);
	if ((datao + 2 < datao) || (datao + 2 < 2) ||
	    (datao + 2 > buf_size)) {
		exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteDataFuji", "Short MakerNote");
		return;
	}

	/* Read the number of tags */
	c = exif_get_short (buf + datao, EXIF_BYTE_ORDER_INTEL);
	datao += 2;

	/* Remove any old entries */
	exif_mnote_data_fuji_clear (n);

	/* Reserve enough space for all the possible MakerNote tags */
	n->entries = exif_mem_alloc (en->mem, sizeof (MnoteFujiEntry) * c);
	if (!n->entries) {
		EXIF_LOG_NO_MEMORY(en->log, "ExifMnoteDataFuji", sizeof (MnoteFujiEntry) * c);
		return;
	}

	/* Parse all c entries, storing ones that are successfully parsed */
	tcount = 0;
	for (i = c, o = datao; i; --i, o += 12) {
		size_t s;
		if ((o + 12 < o) || (o + 12 < 12) || (o + 12 > buf_size)) {
			exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifMnoteDataFuji", "Short MakerNote");
			break;
		}

		n->entries[tcount].tag        = exif_get_short (buf + o, n->order);
		n->entries[tcount].format     = exif_get_short (buf + o + 2, n->order);
		n->entries[tcount].components = exif_get_long (buf + o + 4, n->order);
		n->entries[tcount].order      = n->order;

		exif_log (en->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteDataFuji",
			  "Loading entry 0x%x ('%s')...", n->entries[tcount].tag,
			  mnote_fuji_tag_get_name (n->entries[tcount].tag));

		/*
		 * Size? If bigger than 4 bytes, the actual data is not
		 * in the entry but somewhere else (offset).
		 */
		s = exif_format_get_size (n->entries[tcount].format) * n->entries[tcount].components;
		n->entries[tcount].size = s;
		if (s) {
			size_t dataofs = o + 8;
			if (s > 4)
				/* The data in this case is merely a pointer */
				dataofs = exif_get_long (buf + dataofs, n->order) + 6 + n->offset;
			if ((dataofs + s < dataofs) || (dataofs + s < s) ||
				(dataofs + s >= buf_size)) {
				exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
						  "ExifMnoteDataFuji", "Tag data past end of "
					  "buffer (%u >= %u)", dataofs + s, buf_size);
				continue;
			}

			n->entries[tcount].data = exif_mem_alloc (en->mem, s);
			if (!n->entries[tcount].data) {
				EXIF_LOG_NO_MEMORY(en->log, "ExifMnoteDataFuji", s);
				continue;
			}
			memcpy (n->entries[tcount].data, buf + dataofs, s);
		}

		/* Tag was successfully parsed */
		++tcount;
	}
	/* Store the count of successfully parsed tags */
	n->count = tcount;
}

static unsigned int
exif_mnote_data_fuji_count (ExifMnoteData *n)
{
	return n ? ((ExifMnoteDataFuji *) n)->count : 0;
}

static unsigned int
exif_mnote_data_fuji_get_id (ExifMnoteData *d, unsigned int n)
{
	ExifMnoteDataFuji *note = (ExifMnoteDataFuji *) d;

	if (!note) return 0;
	if (note->count <= n) return 0;
	return note->entries[n].tag;
}

static const char *
exif_mnote_data_fuji_get_name (ExifMnoteData *d, unsigned int i)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) d;

	if (!n) return NULL;
	if (i >= n->count) return NULL;
	return mnote_fuji_tag_get_name (n->entries[i].tag);
}

static const char *
exif_mnote_data_fuji_get_title (ExifMnoteData *d, unsigned int i)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) d;
	
	if (!n) return NULL;
	if (i >= n->count) return NULL;
        return mnote_fuji_tag_get_title (n->entries[i].tag);
}

static const char *
exif_mnote_data_fuji_get_description (ExifMnoteData *d, unsigned int i)
{
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) d;
	
	if (!n) return NULL;
	if (i >= n->count) return NULL;
        return mnote_fuji_tag_get_description (n->entries[i].tag);
}

static void
exif_mnote_data_fuji_set_byte_order (ExifMnoteData *d, ExifByteOrder o)
{
	ExifByteOrder o_orig;
	ExifMnoteDataFuji *n = (ExifMnoteDataFuji *) d;
	unsigned int i;

	if (!n) return;

	o_orig = n->order;
	n->order = o;
	for (i = 0; i < n->count; i++) {
		n->entries[i].order = o;
		exif_array_set_byte_order (n->entries[i].format, n->entries[i].data,
				n->entries[i].components, o_orig, o);
	}
}

static void
exif_mnote_data_fuji_set_offset (ExifMnoteData *n, unsigned int o)
{
	if (n) ((ExifMnoteDataFuji *) n)->offset = o;
}

ExifMnoteData *
exif_mnote_data_fuji_new (ExifMem *mem)
{
	ExifMnoteData *d;

	if (!mem) return NULL;

	d = exif_mem_alloc (mem, sizeof (ExifMnoteDataFuji));
	if (!d) return NULL;

	exif_mnote_data_construct (d, mem);

	/* Set up function pointers */
	d->methods.free            = exif_mnote_data_fuji_free;
	d->methods.set_byte_order  = exif_mnote_data_fuji_set_byte_order;
	d->methods.set_offset      = exif_mnote_data_fuji_set_offset;
	d->methods.load            = exif_mnote_data_fuji_load;
	d->methods.save            = exif_mnote_data_fuji_save;
	d->methods.count           = exif_mnote_data_fuji_count;
	d->methods.get_id          = exif_mnote_data_fuji_get_id;
	d->methods.get_name        = exif_mnote_data_fuji_get_name;
	d->methods.get_title       = exif_mnote_data_fuji_get_title;
	d->methods.get_description = exif_mnote_data_fuji_get_description;
	d->methods.get_value       = exif_mnote_data_fuji_get_value;

	return d;
}
