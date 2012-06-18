/* exif-mnote-data-pentax.c
 *
 * Copyright (c) 2002, 2003 Lutz Mueller <lutz@users.sourceforge.net>
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

#include "config.h"
#include "exif-mnote-data-pentax.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libexif/exif-byte-order.h>
#include <libexif/exif-utils.h>

static void
exif_mnote_data_pentax_clear (ExifMnoteDataPentax *n)
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
exif_mnote_data_pentax_free (ExifMnoteData *n)
{
	if (!n) return;

	exif_mnote_data_pentax_clear ((ExifMnoteDataPentax *) n);
}

static char *
exif_mnote_data_pentax_get_value (ExifMnoteData *d, unsigned int i, char *val, unsigned int maxlen)
{
	ExifMnoteDataPentax *n = (ExifMnoteDataPentax *) d;

	if (!n) return NULL;
	if (n->count <= i) return NULL;
	return mnote_pentax_entry_get_value (&n->entries[i], val, maxlen);
}

/** 
 * @brief save the MnoteData from ne to buf
 * 
 * @param ne extract the data from this structure 
 * @param *buf write the mnoteData to this buffer (buffer will be allocated)
 * @param buf_size the final size of the buffer
 */
static void
exif_mnote_data_pentax_save (ExifMnoteData *ne,
		unsigned char **buf, unsigned int *buf_size)
{
	ExifMnoteDataPentax *n = (ExifMnoteDataPentax *) ne;
	size_t i,
	   base = 0,		/* internal MakerNote tag number offset */
	   o2 = 4 + 2;  	/* offset to first tag entry, past header */
        size_t datao = n->offset; /* this MakerNote style uses offsets
        			  based on main IFD, not makernote IFD */

	if (!n || !buf || !buf_size) return;

	/*
	 * Allocate enough memory for header, the number of entries, entries,
	 * and next IFD pointer
	 */
	*buf_size = o2 + 2 + n->count * 12 + 4;
	switch (n->version) {
	case casioV2:
		base = MNOTE_PENTAX2_TAG_BASE;
		*buf = exif_mem_alloc (ne->mem, *buf_size);
		if (!*buf) {
			EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteDataPentax", *buf_size);
			return;
		}
		/* Write the magic header */
		strcpy ((char *)*buf, "QVC");
		exif_set_short (*buf + 4, n->order, (ExifShort) 0);

		break;

	case pentaxV3:
		base = MNOTE_PENTAX2_TAG_BASE;
		*buf = exif_mem_alloc (ne->mem, *buf_size);
		if (!*buf) {
			EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteDataPentax", *buf_size);
			return;
		}

		/* Write the magic header */
		strcpy ((char *)*buf, "AOC");
		exif_set_short (*buf + 4, n->order, (ExifShort) (
			(n->order == EXIF_BYTE_ORDER_INTEL) ?
			('I' << 8) | 'I' :
			('M' << 8) | 'M'));
		break;

	case pentaxV2:
		base = MNOTE_PENTAX2_TAG_BASE;
		*buf = exif_mem_alloc (ne->mem, *buf_size);
		if (!*buf) {
			EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteDataPentax", *buf_size);
			return;
		}

		/* Write the magic header */
		strcpy ((char *)*buf, "AOC");
		exif_set_short (*buf + 4, n->order, (ExifShort) 0);
		break;

	case pentaxV1:
		/* It looks like this format doesn't have a magic header as
		 * such, just has a fixed number of entries equal to 0x001b */
		*buf_size -= 6;
		o2 -= 6;
		*buf = exif_mem_alloc (ne->mem, *buf_size);
		if (!*buf) {
			EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteDataPentax", *buf_size);
			return;
		}
		break;

	default:
		/* internal error */
		return;
	}

	/* Write the number of entries. */
	exif_set_short (*buf + o2, n->order, (ExifShort) n->count);
	o2 += 2;

	/* Save each entry */
	for (i = 0; i < n->count; i++) {
		size_t doff;	/* offset to current data portion of tag */
		size_t s;
		unsigned char *t;
		size_t o = o2 + i * 12;   /* current offset into output buffer */
		exif_set_short (*buf + o + 0, n->order,
				(ExifShort) (n->entries[i].tag - base));
		exif_set_short (*buf + o + 2, n->order,
				(ExifShort) n->entries[i].format);
		exif_set_long  (*buf + o + 4, n->order,
				n->entries[i].components);
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
			size_t ts = *buf_size + s;
			doff = *buf_size;
			t = exif_mem_realloc (ne->mem, *buf,
						 sizeof (char) * ts);
			if (!t) {
				EXIF_LOG_NO_MEMORY(ne->log, "ExifMnoteDataPentax", ts);
				return;
			}
			*buf = t;
			*buf_size = ts;
			exif_set_long (*buf + o, n->order, datao + doff);
		} else
			doff = o;

		/* Write the data. */
		if (n->entries[i].data) {
			memcpy (*buf + doff, n->entries[i].data, s);
		} else {
			/* Most certainly damaged input file */
			memset (*buf + doff, 0, s);
		}
	}

	/* Sanity check the buffer size */
	if (*buf_size < (o2 + n->count * 12 + 4)) {
		exif_log (ne->log, EXIF_LOG_CODE_CORRUPT_DATA, "ExifMnoteDataPentax",
			"Buffer overflow");
	}

	/* Reset next IFD pointer */
	exif_set_long (*buf + o2 + n->count * 12, n->order, 0);
}

static void
exif_mnote_data_pentax_load (ExifMnoteData *en,
		const unsigned char *buf, unsigned int buf_size)
{
	ExifMnoteDataPentax *n = (ExifMnoteDataPentax *) en;
	size_t i, tcount, o, datao, base = 0;
	ExifShort c;

	if (!n || !buf || !buf_size) {
		exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteDataPentax", "Short MakerNote");
		return;
	}
	datao = 6 + n->offset;
	if ((datao + 8 < datao) || (datao + 8 < 8) || (datao + 8 > buf_size)) {
		exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifMnoteDataPentax", "Short MakerNote");
		return;
	}

	/* Detect variant of Pentax/Casio MakerNote found */
	if (!memcmp(buf + datao, "AOC", 4)) {
		if ((buf[datao + 4] == 'I') && (buf[datao + 5] == 'I')) {
			n->version = pentaxV3;
			n->order = EXIF_BYTE_ORDER_INTEL;
		} else if ((buf[datao + 4] == 'M') && (buf[datao + 5] == 'M')) {
			n->version = pentaxV3;
			n->order = EXIF_BYTE_ORDER_MOTOROLA;
		} else {
			/* Uses Casio v2 tags */
			n->version = pentaxV2;
		}
		exif_log (en->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteDataPentax",
			"Parsing Pentax maker note v%d...", (int)n->version);
		datao += 4 + 2;
		base = MNOTE_PENTAX2_TAG_BASE;
	} else if (!memcmp(buf + datao, "QVC", 4)) {
		exif_log (en->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteDataPentax",
			"Parsing Casio maker note v2...");
		n->version = casioV2;
		base = MNOTE_CASIO2_TAG_BASE;
		datao += 4 + 2;
	} else {
		/* probably assert(!memcmp(buf + datao, "\x00\x1b", 2)) */
		exif_log (en->log, EXIF_LOG_CODE_DEBUG, "ExifMnoteDataPentax",
			"Parsing Pentax maker note v1...");
		n->version = pentaxV1;
	}

	/* Read the number of tags */
	c = exif_get_short (buf + datao, n->order);
	datao += 2;

	/* Remove any old entries */
	exif_mnote_data_pentax_clear (n);

	/* Reserve enough space for all the possible MakerNote tags */
	n->entries = exif_mem_alloc (en->mem, sizeof (MnotePentaxEntry) * c);
	if (!n->entries) {
		EXIF_LOG_NO_MEMORY(en->log, "ExifMnoteDataPentax", sizeof (MnotePentaxEntry) * c);
		return;
	}

	/* Parse all c entries, storing ones that are successfully parsed */
	tcount = 0;
	for (i = c, o = datao; i; --i, o += 12) {
		size_t s;
		if ((o + 12 < o) || (o + 12 < 12) || (o + 12 > buf_size)) {
			exif_log (en->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifMnoteDataPentax", "Short MakerNote");
			break;
		}

		n->entries[tcount].tag        = exif_get_short (buf + o + 0, n->order) + base;
		n->entries[tcount].format     = exif_get_short (buf + o + 2, n->order);
		n->entries[tcount].components = exif_get_long  (buf + o + 4, n->order);
		n->entries[tcount].order      = n->order;

		exif_log (en->log, EXIF_LOG_CODE_DEBUG, "ExifMnotePentax",
			  "Loading entry 0x%x ('%s')...", n->entries[tcount].tag,
			  mnote_pentax_tag_get_name (n->entries[tcount].tag));

		/*
		 * Size? If bigger than 4 bytes, the actual data is not
		 * in the entry but somewhere else (offset).
		 */
		s = exif_format_get_size (n->entries[tcount].format) *
                                      n->entries[tcount].components;
		n->entries[tcount].size = s;
		if (s) {
			size_t dataofs = o + 8;
			if (s > 4)
				/* The data in this case is merely a pointer */
			   	dataofs = exif_get_long (buf + dataofs, n->order) + 6;
			if ((dataofs + s < dataofs) || (dataofs + s < s) ||
				(dataofs + s > buf_size)) {
				exif_log (en->log, EXIF_LOG_CODE_DEBUG,
						  "ExifMnoteDataPentax", "Tag data past end "
					  "of buffer (%u > %u)", dataofs + s, buf_size);
				continue;
			}

			n->entries[tcount].data = exif_mem_alloc (en->mem, s);
			if (!n->entries[tcount].data) {
				EXIF_LOG_NO_MEMORY(en->log, "ExifMnoteDataPentax", s);
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
exif_mnote_data_pentax_count (ExifMnoteData *n)
{
	return n ? ((ExifMnoteDataPentax *) n)->count : 0;
}

static unsigned int
exif_mnote_data_pentax_get_id (ExifMnoteData *d, unsigned int n)
{
	ExifMnoteDataPentax *note = (ExifMnoteDataPentax *) d;

	if (!note) return 0;
	if (note->count <= n) return 0;
	return note->entries[n].tag;
}

static const char *
exif_mnote_data_pentax_get_name (ExifMnoteData *d, unsigned int n)
{
	ExifMnoteDataPentax *note = (ExifMnoteDataPentax *) d;

	if (!note) return NULL;
	if (note->count <= n) return NULL;
	return mnote_pentax_tag_get_name (note->entries[n].tag);
}

static const char *
exif_mnote_data_pentax_get_title (ExifMnoteData *d, unsigned int n)
{
	ExifMnoteDataPentax *note = (ExifMnoteDataPentax *) d;

	if (!note) return NULL;
	if (note->count <= n) return NULL;
	return mnote_pentax_tag_get_title (note->entries[n].tag);
}

static const char *
exif_mnote_data_pentax_get_description (ExifMnoteData *d, unsigned int n)
{
	ExifMnoteDataPentax *note = (ExifMnoteDataPentax *) d;
	
	if (!note) return NULL;
	if (note->count <= n) return NULL;
	return mnote_pentax_tag_get_description (note->entries[n].tag);
}

static void
exif_mnote_data_pentax_set_offset (ExifMnoteData *d, unsigned int o)
{
	if (d) ((ExifMnoteDataPentax *) d)->offset = o;
}

static void
exif_mnote_data_pentax_set_byte_order (ExifMnoteData *d, ExifByteOrder o)
{
	ExifByteOrder o_orig;
	ExifMnoteDataPentax *n = (ExifMnoteDataPentax *) d;
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

ExifMnoteData *
exif_mnote_data_pentax_new (ExifMem *mem)
{
	ExifMnoteData *d;

	if (!mem) return NULL;

	d = exif_mem_alloc (mem, sizeof (ExifMnoteDataPentax));
	if (!d) return NULL;

	exif_mnote_data_construct (d, mem);

	/* Set up function pointers */
	d->methods.free            = exif_mnote_data_pentax_free;
	d->methods.set_byte_order  = exif_mnote_data_pentax_set_byte_order;
	d->methods.set_offset      = exif_mnote_data_pentax_set_offset;
	d->methods.load            = exif_mnote_data_pentax_load;
	d->methods.save            = exif_mnote_data_pentax_save;
	d->methods.count           = exif_mnote_data_pentax_count;
	d->methods.get_id          = exif_mnote_data_pentax_get_id;
	d->methods.get_name        = exif_mnote_data_pentax_get_name;
	d->methods.get_title       = exif_mnote_data_pentax_get_title;
	d->methods.get_description = exif_mnote_data_pentax_get_description;
	d->methods.get_value       = exif_mnote_data_pentax_get_value;

	return d;
}
