/* Routines for handling XML memory maps provided by target.

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

#include "defs.h"
#include "memory-map.h"
#include "gdb_assert.h"
#include "exceptions.h"

#include "gdb_string.h"

#if !defined(HAVE_LIBEXPAT)

VEC(mem_region_s) *
parse_memory_map (const char *memory_map)
{
  static int have_warned;

  if (!have_warned)
    {
      have_warned = 1;
      warning (_("Can not parse XML memory map; XML support was disabled "
		 "at compile time"));
    }

  return NULL;
}

#else /* HAVE_LIBEXPAT */

#include "xml-support.h"

/* Internal parsing data passed to all XML callbacks.  */
struct memory_map_parsing_data
  {
    VEC(mem_region_s) **memory_map;
    char property_name[32];
  };

/* Handle the start of a <memory> element.  */

static void
memory_map_start_memory (struct gdb_xml_parser *parser,
			 const struct gdb_xml_element *element,
			 void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  struct memory_map_parsing_data *data = user_data;
  struct mem_region *r = VEC_safe_push (mem_region_s, *data->memory_map, NULL);
  ULONGEST *start_p, *length_p, *type_p;

  start_p = VEC_index (gdb_xml_value_s, attributes, 0)->value;
  length_p = VEC_index (gdb_xml_value_s, attributes, 1)->value;
  type_p = VEC_index (gdb_xml_value_s, attributes, 2)->value;

  mem_region_init (r);
  r->lo = *start_p;
  r->hi = r->lo + *length_p;
  r->attrib.mode = *type_p;
  r->attrib.blocksize = -1;
}

/* Handle the end of a <memory> element.  Verify that any necessary
   children were present.  */

static void
memory_map_end_memory (struct gdb_xml_parser *parser,
		       const struct gdb_xml_element *element,
		       void *user_data, const char *body_text)
{
  struct memory_map_parsing_data *data = user_data;
  struct mem_region *r = VEC_last (mem_region_s, *data->memory_map);

  if (r->attrib.mode == MEM_FLASH && r->attrib.blocksize == -1)
    gdb_xml_error (parser, _("Flash block size is not set"));
}

/* Handle the start of a <property> element by saving the name
   attribute for later.  */

static void
memory_map_start_property (struct gdb_xml_parser *parser,
			   const struct gdb_xml_element *element,
			   void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  struct memory_map_parsing_data *data = user_data;
  char *name;

  name = VEC_index (gdb_xml_value_s, attributes, 0)->value;
  snprintf (data->property_name, sizeof (data->property_name), "%s", name);
}

/* Handle the end of a <property> element and its value.  */

static void
memory_map_end_property (struct gdb_xml_parser *parser,
			 const struct gdb_xml_element *element,
			 void *user_data, const char *body_text)
{
  struct memory_map_parsing_data *data = user_data;
  char *name = data->property_name;

  if (strcmp (name, "blocksize") == 0)
    {
      struct mem_region *r = VEC_last (mem_region_s, *data->memory_map);

      r->attrib.blocksize = gdb_xml_parse_ulongest (parser, body_text);
    }
  else
    gdb_xml_debug (parser, _("Unknown property \"%s\""), name);
}

/* Discard the constructed memory map (if an error occurs).  */

static void
clear_result (void *p)
{
  VEC(mem_region_s) **result = p;
  VEC_free (mem_region_s, *result);
  *result = NULL;
}

/* The allowed elements and attributes for an XML memory map.  */

const struct gdb_xml_attribute property_attributes[] = {
  { "name", GDB_XML_AF_NONE, NULL, NULL },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element memory_children[] = {
  { "property", property_attributes, NULL,
    GDB_XML_EF_REPEATABLE | GDB_XML_EF_OPTIONAL,
    memory_map_start_property, memory_map_end_property },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

const struct gdb_xml_enum memory_type_enum[] = {
  { "ram", MEM_RW },
  { "rom", MEM_RO },
  { "flash", MEM_FLASH },
  { NULL, 0 }
};

const struct gdb_xml_attribute memory_attributes[] = {
  { "start", GDB_XML_AF_NONE, gdb_xml_parse_attr_ulongest, NULL },
  { "length", GDB_XML_AF_NONE, gdb_xml_parse_attr_ulongest, NULL },
  { "type", GDB_XML_AF_NONE, gdb_xml_parse_attr_enum, &memory_type_enum },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element memory_map_children[] = {
  { "memory", memory_attributes, memory_children, GDB_XML_EF_REPEATABLE,
    memory_map_start_memory, memory_map_end_memory },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

const struct gdb_xml_element memory_map_elements[] = {
  { "memory-map", NULL, memory_map_children, GDB_XML_EF_NONE,
    NULL, NULL },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

VEC(mem_region_s) *
parse_memory_map (const char *memory_map)
{
  struct gdb_xml_parser *parser;
  VEC(mem_region_s) *result = NULL;
  struct cleanup *before_deleting_result, *back_to;
  struct memory_map_parsing_data data = { NULL };

  back_to = make_cleanup (null_cleanup, NULL);
  parser = gdb_xml_create_parser_and_cleanup (_("target memory map"),
					      memory_map_elements, &data);

  /* Note: 'clear_result' will zero 'result'.  */
  before_deleting_result = make_cleanup (clear_result, &result);
  data.memory_map = &result;

  if (gdb_xml_parse (parser, memory_map) == 0)
    /* Parsed successfully, don't need to delete the result.  */
    discard_cleanups (before_deleting_result);

  do_cleanups (back_to);
  return result;
}

#endif /* HAVE_LIBEXPAT */
