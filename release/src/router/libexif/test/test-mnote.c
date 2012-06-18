/* exif-mnote.c
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <libexif/exif-data.h>

static int
test_exif_data (ExifData *d)
{
	unsigned int i, c;
	char v[1024], *p;
	ExifMnoteData *md;

	fprintf (stdout, "Byte order: %s\n",
		exif_byte_order_get_name (exif_data_get_byte_order (d)));

	fprintf (stdout, "Parsing maker note...\n");
	md = exif_data_get_mnote_data (d);
	if (!md) {
		fprintf (stderr, "Could not parse maker note!\n");
		exif_data_unref (d);
		return 1;
	}

	fprintf (stdout, "Increasing ref-count...\n");
	exif_mnote_data_ref (md);

	fprintf (stdout, "Decreasing ref-count...\n");
	exif_mnote_data_unref (md);

	fprintf (stdout, "Counting entries...\n");
	c = exif_mnote_data_count (md);
	fprintf (stdout, "Found %i entries.\n", c);
	for (i = 0; i < c; i++) {
		fprintf (stdout, "Dumping entry number %i...\n", i);
		fprintf (stdout, "  Name: '%s'\n",
				exif_mnote_data_get_name (md, i));
		fprintf (stdout, "  Title: '%s'\n",
				exif_mnote_data_get_title (md, i));
		fprintf (stdout, "  Description: '%s'\n",
				exif_mnote_data_get_description (md, i));
		p = exif_mnote_data_get_value (md, i, v, sizeof (v));
		if (p) { fprintf (stdout, "  Value: '%s'\n", v); }
	}

	return 0;
}

int
main (int argc, char **argv)
{
	ExifData *d;
	unsigned int buf_size;
	unsigned char *buf;
	int r;

	if (argc <= 1) {
		fprintf (stderr, "You need to supply a filename!\n");
		return 1;
	}

	fprintf (stdout, "Loading '%s'...\n", argv[1]);
	d = exif_data_new_from_file (argv[1]);
	if (!d) {
		fprintf (stderr, "Could not load data from '%s'!\n", argv[1]);
		return 1;
	}
	fprintf (stdout, "Loaded '%s'.\n", argv[1]);

	fprintf (stdout, "######### Test 1 #########\n");
	r = test_exif_data (d);
	if (r) return r;

	exif_data_save_data (d, &buf, &buf_size);
	exif_data_unref (d);
	d = exif_data_new_from_data (buf, buf_size);
	free (buf);

	fprintf (stdout, "######### Test 2 #########\n");
	r = test_exif_data (d);
	if (r) return r;

	fprintf (stdout, "Test successful!\n");

	return 1;
}
