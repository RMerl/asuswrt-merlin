/* exif-ifd.c
 *
 * Copyright (c) 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <libexif/exif-ifd.h>

#include <stdlib.h>

static const struct {
	ExifIfd ifd;
	const char *name;
} ExifIfdTable[] = {
	{EXIF_IFD_0, "0"},
	{EXIF_IFD_1, "1"},
	{EXIF_IFD_EXIF, "EXIF"},
	{EXIF_IFD_GPS, "GPS"},
	{EXIF_IFD_INTEROPERABILITY, "Interoperability"},
	{0, NULL}
};

const char *
exif_ifd_get_name (ExifIfd ifd)
{
	unsigned int i;

	for (i = 0; ExifIfdTable[i].name; i++)
		if (ExifIfdTable[i].ifd == ifd)
			break;

	return (ExifIfdTable[i].name);
}
