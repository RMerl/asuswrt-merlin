/* test-mem.c
 *
 * Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
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

#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-loader.h>

#include <stdio.h>
#include <stdlib.h>

int
main ()
{
	ExifData *ed;
	/* ExifEntry *e; */
	unsigned char *eb, size[2];
	unsigned int ebs;
	ExifLoader *loader;
	unsigned int i;

	printf ("Creating EXIF data...\n");
	ed = exif_data_new ();
	exif_data_set_data_type (ed, EXIF_DATA_TYPE_UNCOMPRESSED_CHUNKY);

	printf ("Fill EXIF data with all necessary entries to follow specs...\n");
	exif_data_fix (ed);

	exif_data_dump (ed);

	printf ("Saving EXIF data to memory...\n");
	exif_data_save_data (ed, &eb, &ebs);
	exif_data_unref (ed);

	printf ("Writing %i byte(s) EXIF data to loader...\n", ebs);
	loader = exif_loader_new ();
	size[0] = (unsigned char) ebs;
	size[1] = (unsigned char) (ebs >> 8);
	exif_loader_write (loader, size, 2);
	for (i = 0; i < ebs && exif_loader_write (loader, eb + i, 1); i++);
	printf ("Wrote %i byte(s).\n", i);
	free (eb);
	ed = exif_loader_get_data (loader);
	exif_loader_unref (loader);
	exif_data_dump (ed);
	exif_data_unref (ed);

	return 0;
}
