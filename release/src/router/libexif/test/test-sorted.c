/* test-sorted.c
 *
 * This test ensures that the ExifTagTable[] array is stored in sorted
 * order. If that were not so, then it a binary search of the array would
 * not give correct results.
 *
 * Copyright 2009 Dan Fandrich <dan@coneharvesters.com>
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
 * Boston, MA  02110-1301  USA
 */

#include <libexif/exif-tag.h>
#include <stdio.h>

int
main (void)
{
	int rc = 0;
	unsigned int i, num;
	ExifTag last = 0, current;
	num = exif_tag_table_count() - 1; /* last entry is a NULL terminator */
	for (i=0; i < num; ++i) {
		current = exif_tag_table_get_tag(i);
		if (current < last) {
			printf("Tag 0x%04x in ExifTagTable[] is out of order\n",
				current);
			rc = 1;
		}
		if (exif_tag_table_get_name(i) == NULL) {
			printf("Tag 0x%04x has a NULL name\n", current);
			rc = 1;
		}
		last = current;
	}

	return rc;
}
