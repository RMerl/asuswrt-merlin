/* exif-mnote-data-fuji.h
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

#ifndef __MNOTE_FUJI_CONTENT_H__
#define __MNOTE_FUJI_CONTENT_H__

#include <libexif/exif-mnote-data.h>

typedef struct _ExifMnoteDataFuji        ExifMnoteDataFuji;

#include <libexif/exif-mnote-data-priv.h>
#include <libexif/fuji/mnote-fuji-entry.h>

struct _ExifMnoteDataFuji {
	ExifMnoteData parent;

	MnoteFujiEntry *entries;
	unsigned int count;

	ExifByteOrder order;
	unsigned int offset;
};

ExifMnoteData *exif_mnote_data_fuji_new (ExifMem *);

#endif /* __MNOTE_FUJI_CONTENT_H__ */
