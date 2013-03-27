/* Definitions for targets which report shared library events.

   Copyright (C) 2007
   Free Software Foundation, Inc.

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
#include "objfiles.h"
#include "solist.h"
#include "symtab.h"
#include "symfile.h"
#include "target.h"
#include "vec.h"

#include "gdb_string.h"

DEF_VEC_O(CORE_ADDR);

/* Private data for each loaded library.  */
struct lm_info
{
  /* The library's name.  The name is normally kept in the struct
     so_list; it is only here during XML parsing.  */
  char *name;

  /* The base addresses for each independently relocatable segment of
     this shared library.  */
  VEC(CORE_ADDR) *segment_bases;

  /* The cached offsets for each section of this shared library,
     determined from SEGMENT_BASES.  */
  struct section_offsets *offsets;
};

typedef struct lm_info *lm_info_p;
DEF_VEC_P(lm_info_p);

#if !defined(HAVE_LIBEXPAT)

static VEC(lm_info_p) *
solib_target_parse_libraries (const char *library)
{
  static int have_warned;

  if (!have_warned)
    {
      have_warned = 1;
      warning (_("Can not parse XML library list; XML support was disabled "
		 "at compile time"));
    }

  return NULL;
}

#else /* HAVE_LIBEXPAT */

#include "xml-support.h"

/* Handle the start of a <segment> element.  */

static void
library_list_start_segment (struct gdb_xml_parser *parser,
			    const struct gdb_xml_element *element,
			    void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  VEC(lm_info_p) **list = user_data;
  struct lm_info *last = VEC_last (lm_info_p, *list);
  ULONGEST *address_p = VEC_index (gdb_xml_value_s, attributes, 0)->value;
  CORE_ADDR address = (CORE_ADDR) *address_p;

  VEC_safe_push (CORE_ADDR, last->segment_bases, &address);
}

/* Handle the start of a <library> element.  */

static void
library_list_start_library (struct gdb_xml_parser *parser,
			    const struct gdb_xml_element *element,
			    void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  VEC(lm_info_p) **list = user_data;
  struct lm_info *item = XZALLOC (struct lm_info);
  const char *name = VEC_index (gdb_xml_value_s, attributes, 0)->value;

  item->name = xstrdup (name);
  VEC_safe_push (lm_info_p, *list, item);
}

/* Handle the start of a <library-list> element.  */

static void
library_list_start_list (struct gdb_xml_parser *parser,
			 const struct gdb_xml_element *element,
			 void *user_data, VEC(gdb_xml_value_s) *attributes)
{
  char *version = VEC_index (gdb_xml_value_s, attributes, 0)->value;

  if (strcmp (version, "1.0") != 0)
    gdb_xml_error (parser,
		   _("Library list has unsupported version \"%s\""),
		   version);
}

/* Discard the constructed library list.  */

static void
solib_target_free_library_list (void *p)
{
  VEC(lm_info_p) **result = p;
  struct lm_info *info;
  int ix;

  for (ix = 0; VEC_iterate (lm_info_p, *result, ix, info); ix++)
    {
      xfree (info->name);
      VEC_free (CORE_ADDR, info->segment_bases);
      xfree (info);
    }
  VEC_free (lm_info_p, *result);
  *result = NULL;
}

/* The allowed elements and attributes for an XML library list.
   The root element is a <library-list>.  */

const struct gdb_xml_attribute segment_attributes[] = {
  { "address", GDB_XML_AF_NONE, gdb_xml_parse_attr_ulongest, NULL },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element library_children[] = {
  { "segment", segment_attributes, NULL, GDB_XML_EF_REPEATABLE,
    library_list_start_segment, NULL },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

const struct gdb_xml_attribute library_attributes[] = {
  { "name", GDB_XML_AF_NONE, NULL, NULL },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element library_list_children[] = {
  { "library", library_attributes, library_children,
    GDB_XML_EF_REPEATABLE | GDB_XML_EF_OPTIONAL,
    library_list_start_library, NULL },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

const struct gdb_xml_attribute library_list_attributes[] = {
  { "version", GDB_XML_AF_NONE, NULL, NULL },
  { NULL, GDB_XML_AF_NONE, NULL, NULL }
};

const struct gdb_xml_element library_list_elements[] = {
  { "library-list", library_list_attributes, library_list_children,
    GDB_XML_EF_NONE, library_list_start_list, NULL },
  { NULL, NULL, NULL, GDB_XML_EF_NONE, NULL, NULL }
};

static VEC(lm_info_p) *
solib_target_parse_libraries (const char *library)
{
  struct gdb_xml_parser *parser;
  VEC(lm_info_p) *result = NULL;
  struct cleanup *before_deleting_result, *back_to;

  back_to = make_cleanup (null_cleanup, NULL);
  parser = gdb_xml_create_parser_and_cleanup (_("target library list"),
					      library_list_elements, &result);
  gdb_xml_use_dtd (parser, "library-list.dtd");

  before_deleting_result = make_cleanup (solib_target_free_library_list,
					 &result);

  if (gdb_xml_parse (parser, library) == 0)
    /* Parsed successfully, don't need to delete the result.  */
    discard_cleanups (before_deleting_result);

  do_cleanups (back_to);
  return result;
}
#endif

static struct so_list *
solib_target_current_sos (void)
{
  struct so_list *new_solib, *start = NULL, *last = NULL;
  const char *library_document;
  VEC(lm_info_p) *library_list;
  struct lm_info *info;
  int ix;

  /* Fetch the list of shared libraries.  */
  library_document = target_read_stralloc (&current_target,
					   TARGET_OBJECT_LIBRARIES,
					   NULL);
  if (library_document == NULL)
    return NULL;

  /* Parse the list.  */
  library_list = solib_target_parse_libraries (library_document);
  if (library_list == NULL)
    return NULL;

  /* Build a struct so_list for each entry on the list.  */
  for (ix = 0; VEC_iterate (lm_info_p, library_list, ix, info); ix++)
    {
      new_solib = XZALLOC (struct so_list);
      strncpy (new_solib->so_name, info->name, SO_NAME_MAX_PATH_SIZE - 1);
      new_solib->so_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
      strncpy (new_solib->so_original_name, info->name,
	       SO_NAME_MAX_PATH_SIZE - 1);
      new_solib->so_original_name[SO_NAME_MAX_PATH_SIZE - 1] = '\0';
      new_solib->lm_info = info;

      /* We no longer need this copy of the name.  */
      xfree (info->name);
      info->name = NULL;

      /* Add it to the list.  */
      if (!start)
	last = start = new_solib;
      else
	{
	  last->next = new_solib;
	  last = new_solib;
	}
    }

  /* Free the library list, but not its members.  */
  VEC_free (lm_info_p, library_list);

  return start;
}

static void
solib_target_special_symbol_handling (void)
{
  /* Nothing needed.  */
}

static void
solib_target_solib_create_inferior_hook (void)
{
  /* Nothing needed.  */
}

static void
solib_target_clear_solib (void)
{
  /* Nothing needed.  */
}

static void
solib_target_free_so (struct so_list *so)
{
  gdb_assert (so->lm_info->name == NULL);
  xfree (so->lm_info->offsets);
  VEC_free (CORE_ADDR, so->lm_info->segment_bases);
  xfree (so->lm_info);
}

static void
solib_target_relocate_section_addresses (struct so_list *so,
					 struct section_table *sec)
{
  int flags = bfd_get_section_flags (sec->bfd, sec->the_bfd_section);
  CORE_ADDR offset;

  /* Build the offset table only once per object file.  We can not do
     it any earlier, since we need to open the file first.  */
  if (so->lm_info->offsets == NULL)
    {
      struct symfile_segment_data *data;
      int num_sections = bfd_count_sections (so->abfd);

      so->lm_info->offsets = xzalloc (SIZEOF_N_SECTION_OFFSETS (num_sections));

      data = get_symfile_segment_data (so->abfd);
      if (data == NULL)
	warning (_("Could not relocate shared library \"%s\": no segments"),
		 so->so_name);
      else
	{
	  ULONGEST orig_delta;
	  int i;
	  int num_bases = VEC_length (CORE_ADDR, so->lm_info->segment_bases);
	  CORE_ADDR *segment_bases = VEC_address (CORE_ADDR,
						  so->lm_info->segment_bases);

	  if (!symfile_map_offsets_to_segments (so->abfd, data,
						so->lm_info->offsets,
						num_bases, segment_bases))
	    warning (_("Could not relocate shared library \"%s\": bad offsets"),
		     so->so_name);

	  /* Find the range of addresses to report for this library in
	     "info sharedlibrary".  Report any consecutive segments
	     which were relocated as a single unit.  */
	  gdb_assert (num_bases > 0);
	  orig_delta = segment_bases[0] - data->segment_bases[0];

	  for (i = 1; i < data->num_segments; i++)
	    {
	      /* If we have run out of offsets, assume all remaining segments
		 have the same offset.  */
	      if (i >= num_bases)
		continue;

	      /* If this segment does not have the same offset, do not include
		 it in the library's range.  */
	      if (segment_bases[i] - data->segment_bases[i] != orig_delta)
		break;
	    }

	  so->addr_low = segment_bases[0];
	  so->addr_high = (data->segment_bases[i - 1]
			   + data->segment_sizes[i - 1]
			   + orig_delta);
	  gdb_assert (so->addr_low <= so->addr_high);

	  free_symfile_segment_data (data);
	}
    }

  offset = so->lm_info->offsets->offsets[sec->the_bfd_section->index];
  sec->addr += offset;
  sec->endaddr += offset;
}

static int
solib_target_open_symbol_file_object (void *from_ttyp)
{
  /* We can't locate the main symbol file based on the target's
     knowledge; the user has to specify it.  */
  return 0;
}

static int
solib_target_in_dynsym_resolve_code (CORE_ADDR pc)
{
  /* We don't have a range of addresses for the dynamic linker; there
     may not be one in the program's address space.  So only report
     PLT entries (which may be import stubs).  */
  return in_plt_section (pc, NULL);
}

static struct target_so_ops solib_target_so_ops;

extern initialize_file_ftype _initialize_solib_target; /* -Wmissing-prototypes */

void
_initialize_solib_target (void)
{
  solib_target_so_ops.relocate_section_addresses
    = solib_target_relocate_section_addresses;
  solib_target_so_ops.free_so = solib_target_free_so;
  solib_target_so_ops.clear_solib = solib_target_clear_solib;
  solib_target_so_ops.solib_create_inferior_hook
    = solib_target_solib_create_inferior_hook;
  solib_target_so_ops.special_symbol_handling
    = solib_target_special_symbol_handling;
  solib_target_so_ops.current_sos = solib_target_current_sos;
  solib_target_so_ops.open_symbol_file_object
    = solib_target_open_symbol_file_object;
  solib_target_so_ops.in_dynsym_resolve_code
    = solib_target_in_dynsym_resolve_code;

  current_target_so_ops = &solib_target_so_ops;
}
