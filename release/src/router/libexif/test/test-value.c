/* test-value.c
 *
 * Copyright 2002 Lutz M\uffffller <lutz@users.sourceforge.net>
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

#include <libexif/exif-utils.h>
#include <libexif/exif-data.h>

#include <stdio.h>
#include <stdlib.h>

int
main ()
{
	ExifData *d;
	ExifEntry *e;
	char v[1024];
	ExifSRational r = {1., 20.};
	unsigned int i;

	d = exif_data_new ();
	if (!d) {
		printf ("Error running exif_data_new()\n");
		exit(13);
	}

	e = exif_entry_new ();
	if (!e) {
		printf ("Error running exif_entry_new()\n");
		exit(13);
	}

	exif_content_add_entry (d->ifd[EXIF_IFD_0], e);
	exif_entry_initialize (e, EXIF_TAG_SHUTTER_SPEED_VALUE);
	exif_set_srational (e->data, exif_data_get_byte_order (d), r);

	for (i = 30; i > 0; i--) {
		printf ("Length %2i: '%s'\n", i, 
			exif_entry_get_value (e, v, i));
	}

	exif_entry_unref (e);
	exif_data_unref (d);

	return 0;
}
