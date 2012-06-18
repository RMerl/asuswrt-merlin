/* mnote-olympus-entry.h
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

#ifndef __MNOTE_OLYMPUS_ENTRY_H__
#define __MNOTE_OLYMPUS_ENTRY_H__

#include <libexif/exif-format.h>
#include <libexif/exif-byte-order.h>
#include <libexif/olympus/mnote-olympus-tag.h>

typedef struct _MnoteOlympusEntry        MnoteOlympusEntry;

struct _MnoteOlympusEntry {
	MnoteOlympusTag tag;
	ExifFormat format;
	unsigned long components;

	unsigned char *data;
	unsigned int size;

	ExifByteOrder order;
};

char *mnote_olympus_entry_get_value (MnoteOlympusEntry *entry, char *val, unsigned int maxlen);

#endif /* __MNOTE_OLYMPUS_ENTRY_H__ */
