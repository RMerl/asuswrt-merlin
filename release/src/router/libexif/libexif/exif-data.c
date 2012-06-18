/* exif-data.c
 *
 * Copyright (c) 2001 Lutz Mueller <lutz@users.sourceforge.net>
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

#include <config.h>

#include <libexif/exif-mnote-data.h>
#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-mnote-data-priv.h>
#include <libexif/exif-utils.h>
#include <libexif/exif-loader.h>
#include <libexif/exif-log.h>
#include <libexif/i18n.h>
#include <libexif/exif-system.h>

#include <libexif/canon/exif-mnote-data-canon.h>
#include <libexif/fuji/exif-mnote-data-fuji.h>
#include <libexif/olympus/exif-mnote-data-olympus.h>
#include <libexif/pentax/exif-mnote-data-pentax.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__WATCOMC__) || defined(_MSC_VER)
#      define strncasecmp strnicmp
#endif

#undef JPEG_MARKER_SOI
#define JPEG_MARKER_SOI  0xd8
#undef JPEG_MARKER_APP0
#define JPEG_MARKER_APP0 0xe0
#undef JPEG_MARKER_APP1
#define JPEG_MARKER_APP1 0xe1

static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};

struct _ExifDataPrivate
{
	ExifByteOrder order;

	ExifMnoteData *md;

	ExifLog *log;
	ExifMem *mem;

	unsigned int ref_count;

	/* Temporarily used while loading data */
	unsigned int offset_mnote;

	ExifDataOption options;
	ExifDataType data_type;
};

static void *
exif_data_alloc (ExifData *data, unsigned int i)
{
	void *d;

	if (!data || !i) 
		return NULL;

	d = exif_mem_alloc (data->priv->mem, i);
	if (d) 
		return d;

	EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData", i);
	return NULL;
}

ExifMnoteData *
exif_data_get_mnote_data (ExifData *d)
{
	return (d && d->priv) ? d->priv->md : NULL;
}

ExifData *
exif_data_new (void)
{
	ExifMem *mem = exif_mem_new_default ();
	ExifData *d = exif_data_new_mem (mem);

	exif_mem_unref (mem);

	return d;
}

ExifData *
exif_data_new_mem (ExifMem *mem)
{
	ExifData *data;
	unsigned int i;

	if (!mem) 
		return NULL;

	data = exif_mem_alloc (mem, sizeof (ExifData));
	if (!data) 
		return (NULL);
	data->priv = exif_mem_alloc (mem, sizeof (ExifDataPrivate));
	if (!data->priv) { 
	  	exif_mem_free (mem, data); 
		return (NULL); 
	}
	data->priv->ref_count = 1;

	data->priv->mem = mem;
	exif_mem_ref (mem);

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		data->ifd[i] = exif_content_new_mem (data->priv->mem);
		if (!data->ifd[i]) {
			exif_data_free (data);
			return (NULL);
		}
		data->ifd[i]->parent = data;
	}

	/* Default options */
#ifndef NO_VERBOSE_TAG_STRINGS
	/*
	 * When the tag list is compiled away, setting this option prevents
	 * any tags from being loaded
	 */
	exif_data_set_option (data, EXIF_DATA_OPTION_IGNORE_UNKNOWN_TAGS);
#endif
	exif_data_set_option (data, EXIF_DATA_OPTION_FOLLOW_SPECIFICATION);

	/* Default data type: none */
	exif_data_set_data_type (data, EXIF_DATA_TYPE_COUNT);

	return (data);
}

ExifData *
exif_data_new_from_data (const unsigned char *data, unsigned int size)
{
	ExifData *edata;

	edata = exif_data_new ();
	exif_data_load_data (edata, data, size);
	return (edata);
}

static int
exif_data_load_data_entry (ExifData *data, ExifEntry *entry,
			   const unsigned char *d,
			   unsigned int size, unsigned int offset)
{
	unsigned int s, doff;

	entry->tag        = exif_get_short (d + offset + 0, data->priv->order);
	entry->format     = exif_get_short (d + offset + 2, data->priv->order);
	entry->components = exif_get_long  (d + offset + 4, data->priv->order);

	/* FIXME: should use exif_tag_get_name_in_ifd here but entry->parent 
	 * has not been set yet
	 */
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Loading entry 0x%x ('%s')...", entry->tag,
		  exif_tag_get_name (entry->tag));

	/* {0,1,2,4,8} x { 0x00000000 .. 0xffffffff } 
	 *   -> { 0x000000000 .. 0x7fffffff8 } */
	s = exif_format_get_size(entry->format) * entry->components;
	if ((s < entry->components) || (s == 0)){
		return 0;
	}

	/*
	 * Size? If bigger than 4 bytes, the actual data is not
	 * in the entry but somewhere else (offset).
	 */
	if (s > 4)
		doff = exif_get_long (d + offset + 8, data->priv->order);
	else
		doff = offset + 8;

	/* Sanity checks */
	if ((doff + s < doff) || (doff + s < s) || (doff + s > size)) {
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
				  "Tag data past end of buffer (%u > %u)", doff+s, size);	
		return 0;
	}

	entry->data = exif_data_alloc (data, s);
	if (entry->data) {
		entry->size = s;
		memcpy (entry->data, d + doff, s);
	} else {
		/* FIXME: What do our callers do if (entry->data == NULL)? */
		EXIF_LOG_NO_MEMORY(data->priv->log, "ExifData", s);
	}

	/* If this is the MakerNote, remember the offset */
	if (entry->tag == EXIF_TAG_MAKER_NOTE) {
		if (!entry->data) {
			exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
					  "MakerNote found with empty data");	
		} else if (entry->size > 6) {
			exif_log (data->priv->log,
					       EXIF_LOG_CODE_DEBUG, "ExifData",
					       "MakerNote found (%02x %02x %02x %02x "
					       "%02x %02x %02x...).",
					       entry->data[0], entry->data[1], entry->data[2],
					       entry->data[3], entry->data[4], entry->data[5],
					       entry->data[6]);
		}
		data->priv->offset_mnote = doff;
	}
	return 1;
}

static void
exif_data_save_data_entry (ExifData *data, ExifEntry *e,
			   unsigned char **d, unsigned int *ds,
			   unsigned int offset)
{
	unsigned int doff, s;
	unsigned char *t;
	unsigned int ts;

	if (!data || !data->priv) 
		return;

	/*
	 * Each entry is 12 bytes long. The memory for the entry has
	 * already been allocated.
	 */
	exif_set_short (*d + 6 + offset + 0,
			data->priv->order, (ExifShort) e->tag);
	exif_set_short (*d + 6 + offset + 2,
			data->priv->order, (ExifShort) e->format);

	if (!(data->priv->options & EXIF_DATA_OPTION_DONT_CHANGE_MAKER_NOTE)) {
		/* If this is the maker note tag, update it. */
		if ((e->tag == EXIF_TAG_MAKER_NOTE) && data->priv->md) {
			exif_mem_free (data->priv->mem, e->data);
			e->data = NULL;
			e->size = 0;
			exif_mnote_data_set_offset (data->priv->md, *ds - 6);
			exif_mnote_data_save (data->priv->md, &e->data, &e->size);
			e->components = e->size;
		}
	}

	exif_set_long  (*d + 6 + offset + 4,
			data->priv->order, e->components);

	/*
	 * Size? If bigger than 4 bytes, the actual data is not in
	 * the entry but somewhere else.
	 */
	s = exif_format_get_size (e->format) * e->components;
	if (s > 4) {
		doff = *ds - 6;
		ts = *ds + s;

		/*
		 * According to the TIFF specification,
		 * the offset must be an even number. If we need to introduce
		 * a padding byte, we set it to 0.
		 */
		if (s & 1)
			ts++;
		t = exif_mem_realloc (data->priv->mem, *d, ts);
		if (!t) {
			EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData", ts);
		  	return;
		}
		*d = t;
		*ds = ts;
		exif_set_long (*d + 6 + offset + 8, data->priv->order, doff);
		if (s & 1) 
			*(*d + *ds - 1) = '\0';

	} else
		doff = offset + 8;

	/* Write the data. Fill unneeded bytes with 0. Do not crash with
	 * e->data is NULL */
	if (e->data) {
		memcpy (*d + 6 + doff, e->data, s);
	} else {
		memset (*d + 6 + doff, 0, s);
	}
	if (s < 4) 
		memset (*d + 6 + doff + s, 0, (4 - s));
}

static void
exif_data_load_data_thumbnail (ExifData *data, const unsigned char *d,
			       unsigned int ds, ExifLong o, ExifLong s)
{
	/* Sanity checks */
	if ((o + s < o) || (o + s < s) || (o + s > ds) || (o > ds)) {
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
			  "Bogus thumbnail offset (%u) or size (%u).",
			  o, s);
		return;
	}

	if (data->data) 
		exif_mem_free (data->priv->mem, data->data);
	if (!(data->data = exif_data_alloc (data, s))) {
		EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData", s);
		data->size = 0;
		return;
	}
	data->size = s;
	memcpy (data->data, d + o, s);
}

#undef CHECK_REC
#define CHECK_REC(i) 					\
if ((i) == ifd) {				\
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, \
		"ExifData", "Recursive entry in IFD "	\
		"'%s' detected. Skipping...",		\
		exif_ifd_get_name (i));			\
	break;						\
}							\
if (data->ifd[(i)]->count) {				\
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG,	\
		"ExifData", "Attempt to load IFD "	\
		"'%s' multiple times detected. "	\
		"Skipping...",				\
		exif_ifd_get_name (i));			\
	break;						\
}

/*! Load data for an IFD.
 *
 * \param[in,out] data #ExifData
 * \param[in] ifd IFD to load
 * \param[in] d pointer to buffer containing raw IFD data
 * \param[in] ds size of raw data in buffer at \c d
 * \param[in] offset offset into buffer at \c d at which IFD starts
 * \param[in] recursion_depth number of times this function has been
 * recursively called without returning
 */
static void
exif_data_load_data_content (ExifData *data, ExifIfd ifd,
			     const unsigned char *d,
			     unsigned int ds, unsigned int offset, unsigned int recursion_depth)
{
	ExifLong o, thumbnail_offset = 0, thumbnail_length = 0;
	ExifShort n;
	ExifEntry *entry;
	unsigned int i;
	ExifTag tag;

	if (!data || !data->priv) 
		return;

	/* check for valid ExifIfd enum range */
	if ((((int)ifd) < 0) || ( ((int)ifd) >= EXIF_IFD_COUNT))
	  return;

	if (recursion_depth > 30) {
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "ExifData",
			  "Deep recursion detected!");
		return;
	}

	/* Read the number of entries */
	if ((offset + 2 < offset) || (offset + 2 < 2) || (offset + 2 > ds)) {
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "ExifData",
			  "Tag data past end of buffer (%u > %u)", offset+2, ds);
		return;
	}
	n = exif_get_short (d + offset, data->priv->order);
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
	          "Loading %hu entries...", n);
	offset += 2;

	/* Check if we have enough data. */
	if (offset + 12 * n > ds) {
		n = (ds - offset) / 12;
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
				  "Short data; only loading %hu entries...", n);
	}

	for (i = 0; i < n; i++) {

		tag = exif_get_short (d + offset + 12 * i, data->priv->order);
		switch (tag) {
		case EXIF_TAG_EXIF_IFD_POINTER:
		case EXIF_TAG_GPS_INFO_IFD_POINTER:
		case EXIF_TAG_INTEROPERABILITY_IFD_POINTER:
		case EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH:
		case EXIF_TAG_JPEG_INTERCHANGE_FORMAT:
			o = exif_get_long (d + offset + 12 * i + 8,
					   data->priv->order);
			/* FIXME: IFD_POINTER tags aren't marked as being in a
			 * specific IFD, so exif_tag_get_name_in_ifd won't work
			 */
			exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
				  "Sub-IFD entry 0x%x ('%s') at %u.", tag,
				  exif_tag_get_name(tag), o);
			switch (tag) {
			case EXIF_TAG_EXIF_IFD_POINTER:
				CHECK_REC (EXIF_IFD_EXIF);
				exif_data_load_data_content (data, EXIF_IFD_EXIF, d, ds, o, recursion_depth + 1);
				break;
			case EXIF_TAG_GPS_INFO_IFD_POINTER:
				CHECK_REC (EXIF_IFD_GPS);
				exif_data_load_data_content (data, EXIF_IFD_GPS, d, ds, o, recursion_depth + 1);
				break;
			case EXIF_TAG_INTEROPERABILITY_IFD_POINTER:
				CHECK_REC (EXIF_IFD_INTEROPERABILITY);
				exif_data_load_data_content (data, EXIF_IFD_INTEROPERABILITY, d, ds, o, recursion_depth + 1);
				break;
			case EXIF_TAG_JPEG_INTERCHANGE_FORMAT:
				thumbnail_offset = o;
				if (thumbnail_offset && thumbnail_length)
					exif_data_load_data_thumbnail (data, d,
								       ds, thumbnail_offset,
								       thumbnail_length);
				break;
			case EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH:
				thumbnail_length = o;
				if (thumbnail_offset && thumbnail_length)
					exif_data_load_data_thumbnail (data, d,
								       ds, thumbnail_offset,
								       thumbnail_length);
				break;
			default:
				return;
			}
			break;
		default:

			/*
			 * If we don't know the tag, don't fail. It could be that new 
			 * versions of the standard have defined additional tags. Note that
			 * 0 is a valid tag in the GPS IFD.
			 */
			if (!exif_tag_get_name_in_ifd (tag, ifd)) {

				/*
				 * Special case: Tag and format 0. That's against specification
				 * (at least up to 2.2). But Photoshop writes it anyways.
				 */
				if (!memcmp (d + offset + 12 * i, "\0\0\0\0", 4)) {
					exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
						  "Skipping empty entry at position %u in '%s'.", i, 
						  exif_ifd_get_name (ifd));
					break;
				}
				exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
					  "Unknown tag 0x%04x (entry %u in '%s'). Please report this tag "
					  "to <libexif-devel@lists.sourceforge.net>.", tag, i,
					  exif_ifd_get_name (ifd));
				if (data->priv->options & EXIF_DATA_OPTION_IGNORE_UNKNOWN_TAGS)
					break;
			}
			entry = exif_entry_new_mem (data->priv->mem);
			if (exif_data_load_data_entry (data, entry, d, ds,
						   offset + 12 * i))
				exif_content_add_entry (data->ifd[ifd], entry);
			exif_entry_unref (entry);
			break;
		}
	}
}

static int
cmp_func (const unsigned char *p1, const unsigned char *p2, ExifByteOrder o)
{
	ExifShort tag1 = exif_get_short (p1, o);
	ExifShort tag2 = exif_get_short (p2, o);

	return (tag1 < tag2) ? -1 : (tag1 > tag2) ? 1 : 0;
}

static int
cmp_func_intel (const void *elem1, const void *elem2)
{
	return cmp_func ((const unsigned char *) elem1,
			 (const unsigned char *) elem2, EXIF_BYTE_ORDER_INTEL);
}

static int
cmp_func_motorola (const void *elem1, const void *elem2)
{
	return cmp_func ((const unsigned char *) elem1,
			 (const unsigned char *) elem2, EXIF_BYTE_ORDER_MOTOROLA);
}

static void
exif_data_save_data_content (ExifData *data, ExifContent *ifd,
			     unsigned char **d, unsigned int *ds,
			     unsigned int offset)
{
	unsigned int j, n_ptr = 0, n_thumb = 0;
	ExifIfd i;
	unsigned char *t;
	unsigned int ts;

	if (!data || !data->priv || !ifd || !d || !ds) 
		return;

	for (i = 0; i < EXIF_IFD_COUNT; i++)
		if (ifd == data->ifd[i])
			break;
	if (i == EXIF_IFD_COUNT)
		return;	/* error */

	/*
	 * Check if we need some extra entries for pointers or the thumbnail.
	 */
	switch (i) {
	case EXIF_IFD_0:

		/*
		 * The pointer to IFD_EXIF is in IFD_0. The pointer to
		 * IFD_INTEROPERABILITY is in IFD_EXIF.
		 */
		if (data->ifd[EXIF_IFD_EXIF]->count ||
		    data->ifd[EXIF_IFD_INTEROPERABILITY]->count)
			n_ptr++;

		/* The pointer to IFD_GPS is in IFD_0. */
		if (data->ifd[EXIF_IFD_GPS]->count)
			n_ptr++;

		break;
	case EXIF_IFD_1:
		if (data->size)
			n_thumb = 2;
		break;
	case EXIF_IFD_EXIF:
		if (data->ifd[EXIF_IFD_INTEROPERABILITY]->count)
			n_ptr++;
	default:
		break;
	}

	/*
	 * Allocate enough memory for all entries
	 * and the number of entries.
	 */
	ts = *ds + (2 + (ifd->count + n_ptr + n_thumb) * 12 + 4);
	t = exif_mem_realloc (data->priv->mem, *d, ts);
	if (!t) {
		EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData", ts);
	  	return;
	}
	*d = t;
	*ds = ts;

	/* Save the number of entries */
	exif_set_short (*d + 6 + offset, data->priv->order,
			(ExifShort) (ifd->count + n_ptr + n_thumb));
	offset += 2;

	/*
	 * Save each entry. Make sure that no memcpys from NULL pointers are
	 * performed
	 */
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Saving %i entries (IFD '%s', offset: %i)...",
		  ifd->count, exif_ifd_get_name (i), offset);
	for (j = 0; j < ifd->count; j++) {
		if (ifd->entries[j]) {
			exif_data_save_data_entry (data, ifd->entries[j], d, ds,
				offset + 12 * j);
		}
	}

	offset += 12 * ifd->count;

	/* Now save special entries. */
	switch (i) {
	case EXIF_IFD_0:

		/*
		 * The pointer to IFD_EXIF is in IFD_0.
		 * However, the pointer to IFD_INTEROPERABILITY is in IFD_EXIF,
		 * therefore, if IFD_INTEROPERABILITY is not empty, we need
		 * IFD_EXIF even if latter is empty.
		 */
		if (data->ifd[EXIF_IFD_EXIF]->count ||
		    data->ifd[EXIF_IFD_INTEROPERABILITY]->count) {
			exif_set_short (*d + 6 + offset + 0, data->priv->order,
					EXIF_TAG_EXIF_IFD_POINTER);
			exif_set_short (*d + 6 + offset + 2, data->priv->order,
					EXIF_FORMAT_LONG);
			exif_set_long  (*d + 6 + offset + 4, data->priv->order,
					1);
			exif_set_long  (*d + 6 + offset + 8, data->priv->order,
					*ds - 6);
			exif_data_save_data_content (data,
						     data->ifd[EXIF_IFD_EXIF], d, ds, *ds - 6);
			offset += 12;
		}

		/* The pointer to IFD_GPS is in IFD_0, too. */
		if (data->ifd[EXIF_IFD_GPS]->count) {
			exif_set_short (*d + 6 + offset + 0, data->priv->order,
					EXIF_TAG_GPS_INFO_IFD_POINTER);
			exif_set_short (*d + 6 + offset + 2, data->priv->order,
					EXIF_FORMAT_LONG);
			exif_set_long  (*d + 6 + offset + 4, data->priv->order,
					1);
			exif_set_long  (*d + 6 + offset + 8, data->priv->order,
					*ds - 6);
			exif_data_save_data_content (data,
						     data->ifd[EXIF_IFD_GPS], d, ds, *ds - 6);
			offset += 12;
		}

		break;
	case EXIF_IFD_EXIF:

		/*
		 * The pointer to IFD_INTEROPERABILITY is in IFD_EXIF.
		 * See note above.
		 */
		if (data->ifd[EXIF_IFD_INTEROPERABILITY]->count) {
			exif_set_short (*d + 6 + offset + 0, data->priv->order,
					EXIF_TAG_INTEROPERABILITY_IFD_POINTER);
			exif_set_short (*d + 6 + offset + 2, data->priv->order,
					EXIF_FORMAT_LONG);
			exif_set_long  (*d + 6 + offset + 4, data->priv->order,
					1);
			exif_set_long  (*d + 6 + offset + 8, data->priv->order,
					*ds - 6);
			exif_data_save_data_content (data,
						     data->ifd[EXIF_IFD_INTEROPERABILITY], d, ds,
						     *ds - 6);
			offset += 12;
		}

		break;
	case EXIF_IFD_1:

		/*
		 * Information about the thumbnail (if any) is saved in
		 * IFD_1.
		 */
		if (data->size) {

			/* EXIF_TAG_JPEG_INTERCHANGE_FORMAT */
			exif_set_short (*d + 6 + offset + 0, data->priv->order,
					EXIF_TAG_JPEG_INTERCHANGE_FORMAT);
			exif_set_short (*d + 6 + offset + 2, data->priv->order,
					EXIF_FORMAT_LONG);
			exif_set_long  (*d + 6 + offset + 4, data->priv->order,
					1);
			exif_set_long  (*d + 6 + offset + 8, data->priv->order,
					*ds - 6);
			ts = *ds + data->size;
			t = exif_mem_realloc (data->priv->mem, *d, ts);
			if (!t) {
				EXIF_LOG_NO_MEMORY (data->priv->log, "ExifData",
						    ts);
			  	return;
			}
			*d = t;
			*ds = ts;
			memcpy (*d + *ds - data->size, data->data, data->size);
			offset += 12;

			/* EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH */
			exif_set_short (*d + 6 + offset + 0, data->priv->order,
					EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH);
			exif_set_short (*d + 6 + offset + 2, data->priv->order,
					EXIF_FORMAT_LONG);
			exif_set_long  (*d + 6 + offset + 4, data->priv->order,
					1);
			exif_set_long  (*d + 6 + offset + 8, data->priv->order,
					data->size);
			offset += 12;
		}

		break;
	default:
		break;
	}

	/* Sort the directory according to TIFF specification */
	qsort (*d + 6 + offset - (ifd->count + n_ptr + n_thumb) * 12,
	       (ifd->count + n_ptr + n_thumb), 12,
	       (data->priv->order == EXIF_BYTE_ORDER_INTEL) ? cmp_func_intel : cmp_func_motorola);

	/* Correctly terminate the directory */
	if (i == EXIF_IFD_0 && (data->ifd[EXIF_IFD_1]->count ||
				data->size)) {

		/*
		 * We are saving IFD 0. Tell where IFD 1 starts and save
		 * IFD 1.
		 */
		exif_set_long (*d + 6 + offset, data->priv->order, *ds - 6);
		exif_data_save_data_content (data, data->ifd[EXIF_IFD_1], d, ds,
					     *ds - 6);
	} else
		exif_set_long (*d + 6 + offset, data->priv->order, 0);
}

typedef enum {
	EXIF_DATA_TYPE_MAKER_NOTE_NONE		= 0,
	EXIF_DATA_TYPE_MAKER_NOTE_CANON		= 1,
	EXIF_DATA_TYPE_MAKER_NOTE_OLYMPUS	= 2,
	EXIF_DATA_TYPE_MAKER_NOTE_PENTAX		= 3,
	EXIF_DATA_TYPE_MAKER_NOTE_NIKON		= 4,
	EXIF_DATA_TYPE_MAKER_NOTE_CASIO		= 5,
	EXIF_DATA_TYPE_MAKER_NOTE_FUJI 		= 6
} ExifDataTypeMakerNote;

static ExifDataTypeMakerNote
exif_data_get_type_maker_note (ExifData *d)
{
	ExifEntry *e, *em;
	char value[1024];

	if (!d) 
		return EXIF_DATA_TYPE_MAKER_NOTE_NONE;
	
	e = exif_data_get_entry (d, EXIF_TAG_MAKER_NOTE);
	if (!e) 
		return EXIF_DATA_TYPE_MAKER_NOTE_NONE;

	/* Olympus & Nikon & Sanyo */
	if ((e->size >= 8) && ( !memcmp (e->data, "OLYMP", 6) ||
				!memcmp (e->data, "OLYMPUS", 8) ||
				!memcmp (e->data, "SANYO", 6) ||
				!memcmp (e->data, "EPSON", 6) ||
				!memcmp (e->data, "Nikon", 6)))
		return EXIF_DATA_TYPE_MAKER_NOTE_OLYMPUS;

	em = exif_data_get_entry (d, EXIF_TAG_MAKE);
	if (!em) 
		return EXIF_DATA_TYPE_MAKER_NOTE_NONE;

	/* Canon */
	if (!strcmp (exif_entry_get_value (em, value, sizeof (value)), "Canon"))
		return EXIF_DATA_TYPE_MAKER_NOTE_CANON;

	/* Pentax & some variant of Nikon */
	if ((e->size >= 2) && (e->data[0] == 0x00) && (e->data[1] == 0x1b)) {
		if (!strncasecmp (
			    exif_entry_get_value (em, value, sizeof(value)),
			    "Nikon", 5))
			return EXIF_DATA_TYPE_MAKER_NOTE_NIKON;
		else
			return EXIF_DATA_TYPE_MAKER_NOTE_PENTAX;
	}
	if ((e->size >= 8) && !memcmp (e->data, "AOC", 4)) {
		return EXIF_DATA_TYPE_MAKER_NOTE_PENTAX;
	}
	if ((e->size >= 8) && !memcmp (e->data, "QVC", 4)) {
		return EXIF_DATA_TYPE_MAKER_NOTE_CASIO;
	}
	if ((e->size >= 12) && !memcmp (e->data, "FUJIFILM", 8)) {
		return EXIF_DATA_TYPE_MAKER_NOTE_FUJI;
	}

	return EXIF_DATA_TYPE_MAKER_NOTE_NONE;
}

#define LOG_TOO_SMALL \
exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA, "ExifData", \
		_("Size of data too small to allow for EXIF data."));

void
exif_data_load_data (ExifData *data, const unsigned char *d_orig,
		     unsigned int ds_orig)
{
	unsigned int l;
	ExifLong offset;
	ExifShort n;
	const unsigned char *d = d_orig;
	unsigned int ds = ds_orig, len;

	if (!data || !data->priv || !d || !ds) 
		return;

	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Parsing %i byte(s) EXIF data...\n", ds);

	/*
	 * It can be that the data starts with the EXIF header. If it does
	 * not, search the EXIF marker.
	 */
	if (ds < 6) {
		LOG_TOO_SMALL;
		return;
	}
	if (!memcmp (d, ExifHeader, 6)) {
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
			  "Found EXIF header.");
	} else {
		while (1) {
			while ((d[0] == 0xff) && ds) {
				d++;
				ds--;
			}

			/* JPEG_MARKER_SOI */
			if (d[0] == JPEG_MARKER_SOI) {
				d++;
				ds--;
				continue;
			}

			/* JPEG_MARKER_APP0 */
			if (d[0] == JPEG_MARKER_APP0) {
				d++;
				ds--;
				l = (d[0] << 8) | d[1];
				if (l > ds)
					return;
				d += l;
				ds -= l;
				continue;
			}

			/* JPEG_MARKER_APP1 */
			if (d[0] == JPEG_MARKER_APP1)
				break;

			/* Unknown marker or data. Give up. */
			exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifData", _("EXIF marker not found."));
			return;
		}
		d++;
		ds--;
		if (ds < 2) {
			LOG_TOO_SMALL;
			return;
		}
		len = (d[0] << 8) | d[1];
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
			  "We have to deal with %i byte(s) of EXIF data.",
			  len);
		d += 2;
		ds -= 2;
	}

	/*
	 * Verify the exif header
	 * (offset 2, length 6).
	 */
	if (ds < 6) {
		LOG_TOO_SMALL;
		return;
	}
	if (memcmp (d, ExifHeader, 6)) {
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifData", _("EXIF header not found."));
		return;
	}

	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Found EXIF header.");

	/* Byte order (offset 6, length 2) */
	if (ds < 14)
		return;
	if (!memcmp (d + 6, "II", 2))
		data->priv->order = EXIF_BYTE_ORDER_INTEL;
	else if (!memcmp (d + 6, "MM", 2))
		data->priv->order = EXIF_BYTE_ORDER_MOTOROLA;
	else {
		exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA,
			  "ExifData", _("Unknown encoding."));
		return;
	}

	/* Fixed value */
	if (exif_get_short (d + 8, data->priv->order) != 0x002a)
		return;

	/* IFD 0 offset */
	offset = exif_get_long (d + 10, data->priv->order);
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData", 
		  "IFD 0 at %i.", (int) offset);

	/* Parse the actual exif data (usually offset 14 from start) */
	exif_data_load_data_content (data, EXIF_IFD_0, d + 6, ds - 6, offset, 0);

	/* IFD 1 offset */
	if (offset + 6 + 2 > ds) {
		return;
	}
	n = exif_get_short (d + 6 + offset, data->priv->order);
	if (offset + 6 + 2 + 12 * n + 4 > ds) {
		return;
	}
	offset = exif_get_long (d + 6 + offset + 2 + 12 * n, data->priv->order);
	if (offset) {
		exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
			  "IFD 1 at %i.", (int) offset);

		/* Sanity check. */
		if (offset > ds - 6) {
			exif_log (data->priv->log, EXIF_LOG_CODE_CORRUPT_DATA,
				  "ExifData", "Bogus offset of IFD1.");
		} else {
		   exif_data_load_data_content (data, EXIF_IFD_1, d + 6, ds - 6, offset, 0);
		}
	}

	/*
	 * If we got an EXIF_TAG_MAKER_NOTE, try to interpret it. Some
	 * cameras use pointers in the maker note tag that point to the
	 * space between IFDs. Here is the only place where we have access
	 * to that data.
	 */
	switch (exif_data_get_type_maker_note (data)) {
	case EXIF_DATA_TYPE_MAKER_NOTE_OLYMPUS:
	case EXIF_DATA_TYPE_MAKER_NOTE_NIKON:
		data->priv->md = exif_mnote_data_olympus_new (data->priv->mem);
		break;
	case EXIF_DATA_TYPE_MAKER_NOTE_PENTAX:
	case EXIF_DATA_TYPE_MAKER_NOTE_CASIO:
		data->priv->md = exif_mnote_data_pentax_new (data->priv->mem);
		break;
	case EXIF_DATA_TYPE_MAKER_NOTE_CANON:
		data->priv->md = exif_mnote_data_canon_new (data->priv->mem, data->priv->options);
		break;
	case EXIF_DATA_TYPE_MAKER_NOTE_FUJI:
		data->priv->md = exif_mnote_data_fuji_new (data->priv->mem);
		break;
	default:
		break;
	}

	/* 
	 * If we are able to interpret the maker note, do so.
	 */
	if (data->priv->md) {
		exif_mnote_data_log (data->priv->md, data->priv->log);
		exif_mnote_data_set_byte_order (data->priv->md,
						data->priv->order);
		exif_mnote_data_set_offset (data->priv->md,
					    data->priv->offset_mnote);
		exif_mnote_data_load (data->priv->md, d, ds);
	}

	if (data->priv->options & EXIF_DATA_OPTION_FOLLOW_SPECIFICATION)
		exif_data_fix (data);
}

void
exif_data_save_data (ExifData *data, unsigned char **d, unsigned int *ds)
{
	if (ds)
		*ds = 0;	/* This means something went wrong */

	if (!data || !d || !ds)
		return;

	/* Header */
	*ds = 14;
	*d = exif_data_alloc (data, *ds);
	if (!*d)  {
		*ds = 0;
		return;
	}
	memcpy (*d, ExifHeader, 6);

	/* Order (offset 6) */
	if (data->priv->order == EXIF_BYTE_ORDER_INTEL) {
		memcpy (*d + 6, "II", 2);
	} else {
		memcpy (*d + 6, "MM", 2);
	}

	/* Fixed value (2 bytes, offset 8) */
	exif_set_short (*d + 8, data->priv->order, 0x002a);

	/*
	 * IFD 0 offset (4 bytes, offset 10).
	 * We will start 8 bytes after the
	 * EXIF header (2 bytes for order, another 2 for the test, and
	 * 4 bytes for the IFD 0 offset make 8 bytes together).
	 */
	exif_set_long (*d + 10, data->priv->order, 8);

	/* Now save IFD 0. IFD 1 will be saved automatically. */
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Saving IFDs...");
	exif_data_save_data_content (data, data->ifd[EXIF_IFD_0], d, ds,
				     *ds - 6);
	exif_log (data->priv->log, EXIF_LOG_CODE_DEBUG, "ExifData",
		  "Saved %i byte(s) EXIF data.", *ds);
}

ExifData *
exif_data_new_from_file (const char *path)
{
	ExifData *edata;
	ExifLoader *loader;

	loader = exif_loader_new ();
	exif_loader_write_file (loader, path);
	edata = exif_loader_get_data (loader);
	exif_loader_unref (loader);

	return (edata);
}

void
exif_data_ref (ExifData *data)
{
	if (!data)
		return;

	data->priv->ref_count++;
}

void
exif_data_unref (ExifData *data)
{
	if (!data) 
		return;

	data->priv->ref_count--;
	if (!data->priv->ref_count) 
		exif_data_free (data);
}

void
exif_data_free (ExifData *data)
{
	unsigned int i;
	ExifMem *mem = (data && data->priv) ? data->priv->mem : NULL;

	if (!data) 
		return;

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		if (data->ifd[i]) {
			exif_content_unref (data->ifd[i]);
			data->ifd[i] = NULL;
		}
	}

	if (data->data) {
		exif_mem_free (mem, data->data);
		data->data = NULL;
	}

	if (data->priv) {
		if (data->priv->log) {
			exif_log_unref (data->priv->log);
			data->priv->log = NULL;
		}
		if (data->priv->md) {
			exif_mnote_data_unref (data->priv->md);
			data->priv->md = NULL;
		}
		exif_mem_free (mem, data->priv);
		exif_mem_free (mem, data);
	}

	exif_mem_unref (mem);
}

void
exif_data_dump (ExifData *data)
{
	unsigned int i;

	if (!data)
		return;

	for (i = 0; i < EXIF_IFD_COUNT; i++) {
		if (data->ifd[i] && data->ifd[i]->count) {
			printf ("Dumping IFD '%s'...\n",
				exif_ifd_get_name (i));
			exif_content_dump (data->ifd[i], 0);
		}
	}

	if (data->data) {
		printf ("%i byte(s) thumbnail data available.", data->size);
		if (data->size >= 4) {
			printf ("0x%02x 0x%02x ... 0x%02x 0x%02x\n",
				data->data[0], data->data[1],
				data->data[data->size - 2],
				data->data[data->size - 1]);
		}
	}
}

ExifByteOrder
exif_data_get_byte_order (ExifData *data)
{
	if (!data)
		return (0);

	return (data->priv->order);
}

void
exif_data_foreach_content (ExifData *data, ExifDataForeachContentFunc func,
			   void *user_data)
{
	unsigned int i;

	if (!data || !func)
		return;

	for (i = 0; i < EXIF_IFD_COUNT; i++)
		func (data->ifd[i], user_data);
}

typedef struct _ByteOrderChangeData ByteOrderChangeData;
struct _ByteOrderChangeData {
	ExifByteOrder old, new;
};

static void
entry_set_byte_order (ExifEntry *e, void *data)
{
	ByteOrderChangeData *d = data;

	if (!e)
		return;

	exif_array_set_byte_order (e->format, e->data, e->components, d->old, d->new);
}

static void
content_set_byte_order (ExifContent *content, void *data)
{
	exif_content_foreach_entry (content, entry_set_byte_order, data);
}

void
exif_data_set_byte_order (ExifData *data, ExifByteOrder order)
{
	ByteOrderChangeData d;

	if (!data || (order == data->priv->order))
		return;

	d.old = data->priv->order;
	d.new = order;
	exif_data_foreach_content (data, content_set_byte_order, &d);
	data->priv->order = order;
	if (data->priv->md)
		exif_mnote_data_set_byte_order (data->priv->md, order);
}

void
exif_data_log (ExifData *data, ExifLog *log)
{
	unsigned int i;

	if (!data || !data->priv) 
		return;
	exif_log_unref (data->priv->log);
	data->priv->log = log;
	exif_log_ref (log);

	for (i = 0; i < EXIF_IFD_COUNT; i++)
		exif_content_log (data->ifd[i], log);
}

/* Used internally within libexif */
ExifLog *exif_data_get_log (ExifData *);
ExifLog *
exif_data_get_log (ExifData *data)
{
	if (!data || !data->priv) 
		return NULL;
	return data->priv->log;
}

static const struct {
	ExifDataOption option;
	const char *name;
	const char *description;
} exif_data_option[] = {
	{EXIF_DATA_OPTION_IGNORE_UNKNOWN_TAGS, N_("Ignore unknown tags"),
	 N_("Ignore unknown tags when loading EXIF data.")},
	{EXIF_DATA_OPTION_FOLLOW_SPECIFICATION, N_("Follow specification"),
	 N_("Add, correct and remove entries to get EXIF data that follows "
	    "the specification.")},
	{EXIF_DATA_OPTION_DONT_CHANGE_MAKER_NOTE, N_("Do not change maker note"),
	 N_("When loading and resaving Exif data, save the maker note unmodified."
	    " Be aware that the maker note can get corrupted.")},
	{0, NULL, NULL}
};

const char *
exif_data_option_get_name (ExifDataOption o)
{
	unsigned int i;

	for (i = 0; exif_data_option[i].name; i++)
		if (exif_data_option[i].option == o) 
			break;
	return _(exif_data_option[i].name);
}

const char *
exif_data_option_get_description (ExifDataOption o)
{
	unsigned int i;

	for (i = 0; exif_data_option[i].description; i++)
		if (exif_data_option[i].option == o) 
			break;
	return _(exif_data_option[i].description);
}

void
exif_data_set_option (ExifData *d, ExifDataOption o)
{
	if (!d) 
		return;

	d->priv->options |= o;
}

void
exif_data_unset_option (ExifData *d, ExifDataOption o)
{
	if (!d) 
		return;

	d->priv->options &= ~o;
}

static void
fix_func (ExifContent *c, void *UNUSED(data))
{
	switch (exif_content_get_ifd (c)) {
	case EXIF_IFD_1:
		if (c->parent->data)
			exif_content_fix (c);
		else {
			exif_log (c->parent->priv->log, EXIF_LOG_CODE_DEBUG, "exif-data",
				  "No thumbnail but entries on thumbnail. These entries have been "
				  "removed.");
			while (c->count) {
				unsigned int cnt = c->count;
				exif_content_remove_entry (c, c->entries[c->count - 1]);
				if (cnt == c->count) {
					/* safety net */
					exif_log (c->parent->priv->log, EXIF_LOG_CODE_DEBUG, "exif-data",
					"failed to remove last entry from entries.");
					c->count--;
				}
			}
		}
		break;
	default:
		exif_content_fix (c);
	}
}

void
exif_data_fix (ExifData *d)
{
	exif_data_foreach_content (d, fix_func, NULL);
}

void
exif_data_set_data_type (ExifData *d, ExifDataType dt)
{
	if (!d || !d->priv) 
		return;

	d->priv->data_type = dt;
}

ExifDataType
exif_data_get_data_type (ExifData *d)
{
	return (d && d->priv) ? d->priv->data_type : EXIF_DATA_TYPE_UNKNOWN;
}
