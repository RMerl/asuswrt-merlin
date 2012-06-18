/* exif-loader.c
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

#include <config.h>

#include <libexif/exif-loader.h>
#include <libexif/exif-utils.h>
#include <libexif/i18n.h>

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#undef JPEG_MARKER_DHT
#define JPEG_MARKER_DHT  0xc4
#undef JPEG_MARKER_SOI
#define JPEG_MARKER_SOI  0xd8
#undef JPEG_MARKER_DQT
#define JPEG_MARKER_DQT  0xdb
#undef JPEG_MARKER_APP0
#define JPEG_MARKER_APP0 0xe0
#undef JPEG_MARKER_APP1
#define JPEG_MARKER_APP1 0xe1
#undef JPEG_MARKER_APP2
#define JPEG_MARKER_APP2 0xe2
#undef JPEG_MARKER_APP13
#define JPEG_MARKER_APP13 0xed
#undef JPEG_MARKER_COM
#define JPEG_MARKER_COM 0xfe

typedef enum {
	EL_READ = 0,
	EL_READ_SIZE_BYTE_24,
	EL_READ_SIZE_BYTE_16,
	EL_READ_SIZE_BYTE_08,
	EL_READ_SIZE_BYTE_00,
	EL_SKIP_BYTES,
	EL_EXIF_FOUND,
} ExifLoaderState;

typedef enum {
	EL_DATA_FORMAT_UNKNOWN,
	EL_DATA_FORMAT_EXIF,
	EL_DATA_FORMAT_JPEG,
	EL_DATA_FORMAT_FUJI_RAW
} ExifLoaderDataFormat;

/*! \internal */
struct _ExifLoader {
	ExifLoaderState state;
	ExifLoaderDataFormat data_format;

	/*! Small buffer used for detection of format */
	unsigned char b[12];

	/*! Number of bytes in the small buffer \c b */
	unsigned char b_len;

	unsigned int size;
	unsigned char *buf;
	unsigned int bytes_read;

	unsigned int ref_count;

	ExifLog *log;
	ExifMem *mem;
};

/*! Magic number for EXIF header */
static const unsigned char ExifHeader[] = {0x45, 0x78, 0x69, 0x66, 0x00, 0x00};

static void *
exif_loader_alloc (ExifLoader *l, unsigned int i)
{
	void *d;

	if (!l || !i) 
		return NULL;

	d = exif_mem_alloc (l->mem, i);
	if (d) 
		return d;

	EXIF_LOG_NO_MEMORY (l->log, "ExifLog", i);
	return NULL;
}

void
exif_loader_write_file (ExifLoader *l, const char *path)
{
	FILE *f;
	int size;
	unsigned char data[1024];

	if (!l) 
		return;

	f = fopen (path, "rb");
	if (!f) {
		exif_log (l->log, EXIF_LOG_CODE_NONE, "ExifLoader",
			  _("The file '%s' could not be opened."), path);
		return;
	}
	while (1) {
		size = fread (data, 1, sizeof (data), f);
		if (size <= 0) 
			break;
		if (!exif_loader_write (l, data, size)) 
			break;
	}
	fclose (f);
}

static unsigned int
exif_loader_copy (ExifLoader *eld, unsigned char *buf, unsigned int len)
{
	if (!eld || (len && !buf) || (eld->bytes_read >= eld->size)) 
		return 0;

	/* If needed, allocate the buffer. */
	if (!eld->buf) 
		eld->buf = exif_loader_alloc (eld, eld->size);
	if (!eld->buf) 
		return 0;

	/* Copy memory */
	len = MIN (len, eld->size - eld->bytes_read);
	memcpy (eld->buf + eld->bytes_read, buf, len);
	eld->bytes_read += len;

	return (eld->bytes_read >= eld->size) ? 0 : 1;
}

unsigned char
exif_loader_write (ExifLoader *eld, unsigned char *buf, unsigned int len)
{
	unsigned int i;

	if (!eld || (len && !buf)) 
		return 0;

	switch (eld->state) {
	case EL_EXIF_FOUND:
		return exif_loader_copy (eld, buf, len);
	case EL_SKIP_BYTES:
		if (eld->size > len) { 
			eld->size -= len; 
			return 1; 
		}
		len -= eld->size;
		buf += eld->size;
		eld->size = 0;
		eld->b_len = 0;
		switch (eld->data_format) {
		case EL_DATA_FORMAT_FUJI_RAW:
			eld->state = EL_READ_SIZE_BYTE_24;
			break;
		default:
			eld->state = EL_READ;
			break;
		}
		break;

	case EL_READ:
	default:
		break;
	}

	if (!len)
		return 1;
	exif_log (eld->log, EXIF_LOG_CODE_DEBUG, "ExifLoader",
		  "Scanning %i byte(s) of data...", len);

	/*
	 * First fill the small buffer. Only continue if the buffer
	 * is filled. Note that EXIF data contains at least 12 bytes.
	 */
	i = MIN (len, sizeof (eld->b) - eld->b_len);
	if (i) {
		memcpy (&eld->b[eld->b_len], buf, i);
		eld->b_len += i;
		if (eld->b_len < sizeof (eld->b)) 
			return 1;
		buf += i;
		len -= i;
	}

	switch (eld->data_format) {
	case EL_DATA_FORMAT_UNKNOWN:

		/* Check the small buffer against known formats. */
		if (!memcmp (eld->b, "FUJIFILM", 8)) {

			/* Skip to byte 84. There is another offset there. */
			eld->data_format = EL_DATA_FORMAT_FUJI_RAW;
			eld->size = 84;
			eld->state = EL_SKIP_BYTES;
			eld->size = 84;

		} else if (!memcmp (eld->b + 2, ExifHeader, sizeof (ExifHeader))) {

			/* Read the size (2 bytes). */
			eld->data_format = EL_DATA_FORMAT_EXIF;
			eld->state = EL_READ_SIZE_BYTE_08;
		}
	default:
		break;
	}

	for (i = 0; i < sizeof (eld->b); i++)
		switch (eld->state) {
		case EL_EXIF_FOUND:
			if (!exif_loader_copy (eld, eld->b + i,
					sizeof (eld->b) - i)) 
				return 0;
			return exif_loader_copy (eld, buf, len);
		case EL_SKIP_BYTES:
			eld->size--;
			if (!eld->size) 
				eld->state = EL_READ;
			break;

		case EL_READ_SIZE_BYTE_24:
			eld->size |= eld->b[i] << 24;
			eld->state = EL_READ_SIZE_BYTE_16;
			break;
		case EL_READ_SIZE_BYTE_16:
			eld->size |= eld->b[i] << 16;
			eld->state = EL_READ_SIZE_BYTE_08;
			break;
		case EL_READ_SIZE_BYTE_08:
			eld->size |= eld->b[i] << 8;
			eld->state = EL_READ_SIZE_BYTE_00;
			break;
		case EL_READ_SIZE_BYTE_00:
			eld->size |= eld->b[i] << 0;
			switch (eld->data_format) {
			case EL_DATA_FORMAT_JPEG:
				eld->state = EL_SKIP_BYTES;
				eld->size -= 2;
				break;
			case EL_DATA_FORMAT_FUJI_RAW:
				eld->data_format = EL_DATA_FORMAT_EXIF;
				eld->state = EL_SKIP_BYTES;
				eld->size -= 86;
				break;
			case EL_DATA_FORMAT_EXIF:
				eld->state = EL_EXIF_FOUND;
				break;
			default:
				break;
			}
			break;

		default:
			switch (eld->b[i]) {
			case JPEG_MARKER_APP1:
			  if (!memcmp (eld->b + i + 3, ExifHeader, MIN((ssize_t)(sizeof(ExifHeader)), MAX(0, ((ssize_t)(sizeof(eld->b))) - ((ssize_t)i) - 3)))) {
					eld->data_format = EL_DATA_FORMAT_EXIF;
				} else {
					eld->data_format = EL_DATA_FORMAT_JPEG; /* Probably JFIF - keep searching for APP1 EXIF*/
				}
				eld->size = 0;
				eld->state = EL_READ_SIZE_BYTE_08;
				break;
			case JPEG_MARKER_DHT:
			case JPEG_MARKER_DQT:
			case JPEG_MARKER_APP0:
			case JPEG_MARKER_APP2:
			case JPEG_MARKER_APP13:
			case JPEG_MARKER_COM:
				eld->data_format = EL_DATA_FORMAT_JPEG;
				eld->size = 0;
				eld->state = EL_READ_SIZE_BYTE_08;
				break;
			case 0xff:
			case JPEG_MARKER_SOI:
				break;
			default:
				exif_log (eld->log,
					EXIF_LOG_CODE_CORRUPT_DATA,
					"ExifLoader", _("The data supplied "
						"does not seem to contain "
						"EXIF data."));
				exif_loader_reset (eld);
				return 0;
			}
		}

	/*
	 * If we reach this point, the buffer has not been big enough
	 * to read all data we need. Fill it with new data.
	 */
	eld->b_len = 0;
	return exif_loader_write (eld, buf, len);
}

ExifLoader *
exif_loader_new (void)
{
	ExifMem *mem = exif_mem_new_default ();
	ExifLoader *l = exif_loader_new_mem (mem);

	exif_mem_unref (mem);

	return l;
}

ExifLoader *
exif_loader_new_mem (ExifMem *mem)
{
	ExifLoader *loader;

	if (!mem) 
		return NULL;
	
	loader = exif_mem_alloc (mem, sizeof (ExifLoader));
	if (!loader) 
		return NULL;
	loader->ref_count = 1;

	loader->mem = mem;
	exif_mem_ref (mem);

	return loader;
}

void
exif_loader_ref (ExifLoader *loader)
{
	if (loader) 
		loader->ref_count++;
}

static void
exif_loader_free (ExifLoader *loader)
{
	ExifMem *mem;

	if (!loader) 
		return;

	mem = loader->mem;
	exif_loader_reset (loader);
	exif_log_unref (loader->log);
	exif_mem_free (mem, loader);
	exif_mem_unref (mem);
}
	
void
exif_loader_unref (ExifLoader *loader)
{
	if (!loader) 
		return;
	if (!--loader->ref_count)
		exif_loader_free (loader);
}

void
exif_loader_reset (ExifLoader *loader)
{
	if (!loader) 
		return;
	exif_mem_free (loader->mem, loader->buf); loader->buf = NULL;
	loader->size = 0;
	loader->bytes_read = 0;
	loader->state = 0;
	loader->b_len = 0;
	loader->data_format = EL_DATA_FORMAT_UNKNOWN;
}

ExifData *
exif_loader_get_data (ExifLoader *loader)
{
	ExifData *ed;

	if (!loader || (loader->data_format == EL_DATA_FORMAT_UNKNOWN) ||
	    !loader->bytes_read)
		return NULL;

	ed = exif_data_new_mem (loader->mem);
	exif_data_log (ed, loader->log);
	exif_data_load_data (ed, loader->buf, loader->bytes_read);

	return ed;
}

void
exif_loader_get_buf (ExifLoader *loader, const unsigned char **buf,
						  unsigned int *buf_size)
{
	const unsigned char* b = NULL;
	unsigned int s = 0;

	if (!loader || (loader->data_format == EL_DATA_FORMAT_UNKNOWN)) {
		exif_log (loader->log, EXIF_LOG_CODE_DEBUG, "ExifLoader",
			  "Loader format unknown");
	} else {
		b = loader->buf;
		s = loader->bytes_read;
	}
	if (buf)
		*buf = b;
	if (buf_size)
		*buf_size = s;
}

void
exif_loader_log (ExifLoader *loader, ExifLog *log)
{
	if (!loader) 
		return;
	exif_log_unref (loader->log);
	loader->log = log;
	exif_log_ref (log);
}
