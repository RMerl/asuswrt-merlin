/** \file test-parse.c
 * \brief Completely parse all files given on the command line.
 *
 * Copyright (C) 2007 Hans Ulrich Niedermann <gp@n-dimensional.de>
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
 *
 */

#include "libexif/exif-data.h"
#include "libexif/exif-system.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/** Callback function handling an ExifEntry. */
void content_foreach_func(ExifEntry *entry, void *callback_data);
void content_foreach_func(ExifEntry *entry, void *UNUSED(callback_data))
{
  char buf[2000];
  exif_entry_get_value(entry, buf, sizeof(buf));
  printf("    Entry %p: %s (%s)\n"
	 "      Size, Comps: %d, %d\n"
	 "      Value: %s\n", 
	 entry,
	 exif_tag_get_name(entry->tag),
	 exif_format_get_name(entry->format),
	 entry->size,
	 (int)(entry->components),
	 exif_entry_get_value(entry, buf, sizeof(buf)));
}


/** Callback function handling an ExifContent (corresponds 1:1 to an IFD). */
void data_foreach_func(ExifContent *content, void *callback_data);
void data_foreach_func(ExifContent *content, void *callback_data)
{
  printf("  Content %p: ifd=%d\n", content, exif_content_get_ifd(content));
  exif_content_foreach_entry(content, content_foreach_func, callback_data);
}


/** Run EXIF parsing test on the given file. */
void test_parse(const char *filename, void *callback_data);
void test_parse(const char *filename, void *callback_data)
{
  ExifData *d;
  printf("File %s\n", filename);

  d = exif_data_new_from_file(filename);
  exif_data_foreach_content(d, data_foreach_func, callback_data);
  exif_data_unref(d);
}


/** Callback function prototype for string parsing. */
typedef void (*test_parse_func) (const char *filename, void *callback_data);


/** Split string at whitespace and call callback with each substring. */
void split_ws_string(const char *string, test_parse_func func, void *callback_data);
void split_ws_string(const char *string, test_parse_func func, void *callback_data)
{
  const char *start = string;
  const char *p = start;
  for (;;) {
    if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\0' ) {
      size_t len = p-start;
      if (len > 0) {
	/* emulate strndup */
	char *str = malloc(1+len);
	if (str) {
	  memcpy(str, start, len);
	  str[len] = '\0';
	  func(str, callback_data);
	  free(str);
	  start = p+1;
	}
      } else {
	start = p+1;
      }
    }
    if (*p == '\0') {
      break;
    }
    p++;
  }  
}


/** Main program. */
int main(const int argc, const char *argv[])
{
  int i;
  void *callback_data = NULL;

  const char *envar = getenv("TEST_IMAGES");
  if (envar) {
    split_ws_string(envar, test_parse, callback_data);
  }

  for (i=1; i<argc; i++) {
    test_parse(argv[i], callback_data);
  }

  return 0;
}
