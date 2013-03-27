/* Helper routines for parsing XML using Expat.

   Copyright (C) 2006, 2007 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */


#ifndef XML_SUPPORT_H
#define XML_SUPPORT_H

#include "gdb_obstack.h"
#include "vec.h"

struct gdb_xml_parser;
struct gdb_xml_element;
struct gdb_xml_attribute;

/* Return an XML document which was compiled into GDB, from
   the given FILENAME, or NULL if the file was not compiled in.  */

const char *fetch_xml_builtin (const char *filename);

/* A to_xfer_partial helper function which reads XML files which were
   compiled into GDB.  The target may call this function from its own
   to_xfer_partial handler, after converting object and annex to the
   appropriate filename.  */

LONGEST xml_builtin_xfer_partial (const char *filename,
				  gdb_byte *readbuf, const gdb_byte *writebuf,
				  ULONGEST offset, LONGEST len);

/* The text of compiled-in XML documents, from xml-builtin.c
   (generated).  */

extern const char *xml_builtin[][2];

/* Return a malloc allocated string with special characters from TEXT
   replaced by entity references.  */

char *xml_escape_text (const char *text);

/* Support for XInclude.  */

/* Callback to fetch a new XML file, based on the provided HREF.  */

typedef char *(*xml_fetch_another) (const char *href, void *baton);

/* Return a new string which is the expansion of TEXT after processing
   <xi:include> tags.  FETCHER will be called (with FETCHER_BATON) to
   retrieve any new files.  DEPTH should be zero on the initial call.

   On failure, this function uses NAME in a warning and returns NULL.
   It may throw an exception, but does not for XML parsing
   problems.  */

char *xml_process_xincludes (const char *name, const char *text,
			     xml_fetch_another fetcher, void *fetcher_baton,
			     int depth);

/* Simplified XML parser infrastructure.  */

/* A name and value pair, used to record parsed attributes.  */

struct gdb_xml_value
{
  const char *name;
  void *value;
};
typedef struct gdb_xml_value gdb_xml_value_s;
DEF_VEC_O(gdb_xml_value_s);

/* The type of an attribute handler.

   PARSER is the current XML parser, which should be used to issue any
   debugging or error messages.  The second argument is the
   corresponding attribute description, so that a single handler can
   be used for multiple attributes; the attribute name is available
   for error messages and the handler data is available for additional
   customization (see gdb_xml_parse_attr_enum).  VALUE is the string
   value of the attribute.

   The returned value should be freeable with xfree, and will be freed
   after the start handler is called.  Errors should be reported by
   calling gdb_xml_error.  */

typedef void *(gdb_xml_attribute_handler) (struct gdb_xml_parser *parser,
					   const struct gdb_xml_attribute *,
					   const char *value);

/* Flags for attributes.  If no flags are specified, the attribute is
   required.  */

enum gdb_xml_attribute_flag
{
  GDB_XML_AF_NONE,
  GDB_XML_AF_OPTIONAL = 1 << 0,  /* The attribute is optional.  */
};

/* An expected attribute and the handler to call when it is
   encountered.  Arrays of struct gdb_xml_attribute are terminated
   by an entry with NAME == NULL.  */

struct gdb_xml_attribute
{
  const char *name;
  int flags;
  gdb_xml_attribute_handler *handler;
  const void *handler_data;
};

/* Flags for elements.  If no flags are specified, the element is
   required exactly once.  */

enum gdb_xml_element_flag
{
  GDB_XML_EF_NONE,
  GDB_XML_EF_OPTIONAL = 1 << 0,  /* The element is optional.  */
  GDB_XML_EF_REPEATABLE = 1 << 1,  /* The element is repeatable.  */
};

/* A handler called at the beginning of an element.

   PARSER is the current XML parser, which should be used to issue any
   debugging or error messages.  ELEMENT is the current element.
   USER_DATA is the opaque pointer supplied when the parser was
   created.  ATTRIBUTES is a vector of the values of any attributes
   attached to this element.

   The start handler will only be called if all the required
   attributes were present and parsed successfully, and elements of
   ATTRIBUTES are guaranteed to be in the same order used in
   ELEMENT->ATTRIBUTES (not the order from the XML file).  Accordingly
   fixed offsets can be used to find any non-optional attributes as
   long as no optional attributes precede them.  */

typedef void (gdb_xml_element_start_handler)
     (struct gdb_xml_parser *parser, const struct gdb_xml_element *element,
      void *user_data, VEC(gdb_xml_value_s) *attributes);

/* A handler called at the end of an element.

   PARSER, ELEMENT, and USER_DATA are as for the start handler.  BODY
   is any accumulated body text inside the element, with leading and
   trailing whitespace removed.  It will never be NULL.  */

typedef void (gdb_xml_element_end_handler)
     (struct gdb_xml_parser *, const struct gdb_xml_element *,
      void *user_data, const char *body_text);

/* An expected element and the handlers to call when it is
   encountered.  Arrays of struct gdb_xml_element are terminated
   by an entry with NAME == NULL.  */

struct gdb_xml_element
{
  const char *name;
  const struct gdb_xml_attribute *attributes;
  const struct gdb_xml_element *children;
  int flags;

  gdb_xml_element_start_handler *start_handler;
  gdb_xml_element_end_handler *end_handler;
};

/* Initialize and return a parser.  Register a cleanup to destroy the
   parser.  */

struct gdb_xml_parser *gdb_xml_create_parser_and_cleanup
  (const char *name, const struct gdb_xml_element *elements,
   void *user_data);

/* Associate DTD_NAME, which must be the name of a compiled-in DTD,
   with PARSER.  */

void gdb_xml_use_dtd (struct gdb_xml_parser *parser, const char *dtd_name);

/* Invoke PARSER on BUFFER.  BUFFER is the data to parse, which
   should be NUL-terminated.

   The return value is 0 for success or -1 for error.  It may throw,
   but only if something unexpected goes wrong during parsing; parse
   errors will be caught, warned about, and reported as failure.  */

int gdb_xml_parse (struct gdb_xml_parser *parser, const char *buffer);

/* Issue a debugging message from one of PARSER's handlers.  */

void gdb_xml_debug (struct gdb_xml_parser *parser, const char *format, ...)
     ATTR_FORMAT (printf, 2, 0);

/* Issue an error message from one of PARSER's handlers, and stop
   parsing.  */

void gdb_xml_error (struct gdb_xml_parser *parser, const char *format, ...)
     ATTR_NORETURN ATTR_FORMAT (printf, 2, 0);

/* Parse an integer attribute into a ULONGEST.  */

extern gdb_xml_attribute_handler gdb_xml_parse_attr_ulongest;

/* Map NAME to VALUE.  A struct gdb_xml_enum * should be saved as the
   value of handler_data when using gdb_xml_parse_attr_enum to parse a
   fixed list of possible strings.  The list is terminated by an entry
   with NAME == NULL.  */

struct gdb_xml_enum
{
  const char *name;
  ULONGEST value;
};

/* A handler_data for yes/no boolean values.  */
extern const struct gdb_xml_enum gdb_xml_enums_boolean[];

extern gdb_xml_attribute_handler gdb_xml_parse_attr_enum;

/* Parse an integer string into a ULONGEST and return it, or call
   gdb_xml_error if it could not be parsed.  */

ULONGEST gdb_xml_parse_ulongest (struct gdb_xml_parser *parser,
				 const char *value);

#endif
