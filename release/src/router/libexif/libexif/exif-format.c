/* exif-format.c
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

#include <libexif/exif-format.h>
#include <libexif/i18n.h>

#include <stdlib.h>

/*! Table of data format types, descriptions and sizes.
 * This table should be sorted in decreasing order of popularity in order
 * to decrease the total average lookup time.
 */
static const struct {
        ExifFormat format;
	const char *name;
        unsigned char size;
} ExifFormatTable[] = {
        {EXIF_FORMAT_SHORT,     N_("Short"),     2},
        {EXIF_FORMAT_RATIONAL,  N_("Rational"),  8},
        {EXIF_FORMAT_SRATIONAL, N_("SRational"), 8},
        {EXIF_FORMAT_UNDEFINED, N_("Undefined"), 1},
        {EXIF_FORMAT_ASCII,     N_("ASCII"),     1},
        {EXIF_FORMAT_LONG,      N_("Long"),      4},
        {EXIF_FORMAT_BYTE,      N_("Byte"),      1},
	{EXIF_FORMAT_SBYTE,     N_("SByte"),     1},
	{EXIF_FORMAT_SSHORT,    N_("SShort"),    2},
        {EXIF_FORMAT_SLONG,     N_("SLong"),     4},
	{EXIF_FORMAT_FLOAT,     N_("Float"),     4},
	{EXIF_FORMAT_DOUBLE,    N_("Double"),    8},
        {0, NULL, 0}
};

const char *
exif_format_get_name (ExifFormat format)
{
	unsigned int i;

	/* FIXME: This belongs to somewhere else. */
	/* libexif should use the default system locale.
	 * If an application specifically requires UTF-8, then we
	 * must give the application a way to tell libexif that.
	 * 
	 * bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	 */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);

	for (i = 0; ExifFormatTable[i].name; i++)
		if (ExifFormatTable[i].format == format)
			return _(ExifFormatTable[i].name);
	return NULL;
}

unsigned char
exif_format_get_size (ExifFormat format)
{
	unsigned int i;

	for (i = 0; ExifFormatTable[i].size; i++)
		if (ExifFormatTable[i].format == format)
			return ExifFormatTable[i].size;
	return 0;
}
