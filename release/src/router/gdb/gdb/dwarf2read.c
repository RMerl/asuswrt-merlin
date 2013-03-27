/* DWARF 2 debugging format support for GDB.

   Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
                 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

   Adapted by Gary Funck (gary@intrepid.com), Intrepid Technology,
   Inc.  with support from Florida State University (under contract
   with the Ada Joint Program Office), and Silicon Graphics, Inc.
   Initial contribution by Brent Benson, Harris Computer Systems, Inc.,
   based on Fred Fish's (Cygnus Support) implementation of DWARF 1
   support.

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
#include "bfd.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "objfiles.h"
#include "elf/dwarf2.h"
#include "buildsym.h"
#include "demangle.h"
#include "expression.h"
#include "filenames.h"	/* for DOSish file names */
#include "macrotab.h"
#include "language.h"
#include "complaints.h"
#include "bcache.h"
#include "dwarf2expr.h"
#include "dwarf2loc.h"
#include "cp-support.h"
#include "hashtab.h"
#include "command.h"
#include "gdbcmd.h"

#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_assert.h"
#include <sys/types.h>

/* A note on memory usage for this file.
   
   At the present time, this code reads the debug info sections into
   the objfile's objfile_obstack.  A definite improvement for startup
   time, on platforms which do not emit relocations for debug
   sections, would be to use mmap instead.  The object's complete
   debug information is loaded into memory, partly to simplify
   absolute DIE references.

   Whether using obstacks or mmap, the sections should remain loaded
   until the objfile is released, and pointers into the section data
   can be used for any other data associated to the objfile (symbol
   names, type names, location expressions to name a few).  */

#if 0
/* .debug_info header for a compilation unit
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct comp_unit_header
  {
    unsigned int length;	/* length of the .debug_info
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int abbrev_offset;	/* offset into .debug_abbrev section */
    unsigned char addr_size;	/* byte size of an address -- 4 */
  }
_COMP_UNIT_HEADER;
#define _ACTUAL_COMP_UNIT_HEADER_SIZE 11
#endif

/* .debug_pubnames header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct pubnames_header
  {
    unsigned int length;	/* length of the .debug_pubnames
				   contribution  */
    unsigned char version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned int info_size;	/* byte size of .debug_info section
				   portion */
  }
_PUBNAMES_HEADER;
#define _ACTUAL_PUBNAMES_HEADER_SIZE 13

/* .debug_pubnames header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct aranges_header
  {
    unsigned int length;	/* byte len of the .debug_aranges
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned char addr_size;	/* byte size of an address */
    unsigned char seg_size;	/* byte size of segment descriptor */
  }
_ARANGES_HEADER;
#define _ACTUAL_ARANGES_HEADER_SIZE 12

/* .debug_line statement program prologue
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct statement_prologue
  {
    unsigned int total_length;	/* byte length of the statement
				   information */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int prologue_length;	/* # bytes between prologue &
					   stmt program */
    unsigned char minimum_instruction_length;	/* byte size of
						   smallest instr */
    unsigned char default_is_stmt;	/* initial value of is_stmt
					   register */
    char line_base;
    unsigned char line_range;
    unsigned char opcode_base;	/* number assigned to first special
				   opcode */
    unsigned char *standard_opcode_lengths;
  }
_STATEMENT_PROLOGUE;

static const struct objfile_data *dwarf2_objfile_data_key;

struct dwarf2_per_objfile
{
  /* Sizes of debugging sections.  */
  unsigned int info_size;
  unsigned int abbrev_size;
  unsigned int line_size;
  unsigned int pubnames_size;
  unsigned int aranges_size;
  unsigned int loc_size;
  unsigned int macinfo_size;
  unsigned int str_size;
  unsigned int ranges_size;
  unsigned int frame_size;
  unsigned int eh_frame_size;

  /* Loaded data from the sections.  */
  gdb_byte *info_buffer;
  gdb_byte *abbrev_buffer;
  gdb_byte *line_buffer;
  gdb_byte *str_buffer;
  gdb_byte *macinfo_buffer;
  gdb_byte *ranges_buffer;
  gdb_byte *loc_buffer;

  /* A list of all the compilation units.  This is used to locate
     the target compilation unit of a particular reference.  */
  struct dwarf2_per_cu_data **all_comp_units;

  /* The number of compilation units in ALL_COMP_UNITS.  */
  int n_comp_units;

  /* A chain of compilation units that are currently read in, so that
     they can be freed later.  */
  struct dwarf2_per_cu_data *read_in_chain;

  /* A flag indicating wether this objfile has a section loaded at a
     VMA of 0.  */
  int has_section_at_zero;
};

static struct dwarf2_per_objfile *dwarf2_per_objfile;

static asection *dwarf_info_section;
static asection *dwarf_abbrev_section;
static asection *dwarf_line_section;
static asection *dwarf_pubnames_section;
static asection *dwarf_aranges_section;
static asection *dwarf_loc_section;
static asection *dwarf_macinfo_section;
static asection *dwarf_str_section;
static asection *dwarf_ranges_section;
asection *dwarf_frame_section;
asection *dwarf_eh_frame_section;

/* names of the debugging sections */

#define INFO_SECTION     ".debug_info"
#define ABBREV_SECTION   ".debug_abbrev"
#define LINE_SECTION     ".debug_line"
#define PUBNAMES_SECTION ".debug_pubnames"
#define ARANGES_SECTION  ".debug_aranges"
#define LOC_SECTION      ".debug_loc"
#define MACINFO_SECTION  ".debug_macinfo"
#define STR_SECTION      ".debug_str"
#define RANGES_SECTION   ".debug_ranges"
#define FRAME_SECTION    ".debug_frame"
#define EH_FRAME_SECTION ".eh_frame"

/* local data types */

/* We hold several abbreviation tables in memory at the same time. */
#ifndef ABBREV_HASH_SIZE
#define ABBREV_HASH_SIZE 121
#endif

/* The data in a compilation unit header, after target2host
   translation, looks like this.  */
struct comp_unit_head
{
  unsigned long length;
  short version;
  unsigned int abbrev_offset;
  unsigned char addr_size;
  unsigned char signed_addr_p;

  /* Size of file offsets; either 4 or 8.  */
  unsigned int offset_size;

  /* Size of the length field; either 4 or 12.  */
  unsigned int initial_length_size;

  /* Offset to the first byte of this compilation unit header in the
     .debug_info section, for resolving relative reference dies.  */
  unsigned int offset;

  /* Pointer to this compilation unit header in the .debug_info
     section.  */
  gdb_byte *cu_head_ptr;

  /* Pointer to the first die of this compilation unit.  This will be
     the first byte following the compilation unit header.  */
  gdb_byte *first_die_ptr;

  /* Pointer to the next compilation unit header in the program.  */
  struct comp_unit_head *next;

  /* Base address of this compilation unit.  */
  CORE_ADDR base_address;

  /* Non-zero if base_address has been set.  */
  int base_known;
};

/* Fixed size for the DIE hash table.  */
#ifndef REF_HASH_SIZE
#define REF_HASH_SIZE 1021
#endif

/* Internal state when decoding a particular compilation unit.  */
struct dwarf2_cu
{
  /* The objfile containing this compilation unit.  */
  struct objfile *objfile;

  /* The header of the compilation unit.

     FIXME drow/2003-11-10: Some of the things from the comp_unit_head
     should logically be moved to the dwarf2_cu structure.  */
  struct comp_unit_head header;

  struct function_range *first_fn, *last_fn, *cached_fn;

  /* The language we are debugging.  */
  enum language language;
  const struct language_defn *language_defn;

  const char *producer;

  /* The generic symbol table building routines have separate lists for
     file scope symbols and all all other scopes (local scopes).  So
     we need to select the right one to pass to add_symbol_to_list().
     We do it by keeping a pointer to the correct list in list_in_scope.

     FIXME: The original dwarf code just treated the file scope as the
     first local scope, and all other local scopes as nested local
     scopes, and worked fine.  Check to see if we really need to
     distinguish these in buildsym.c.  */
  struct pending **list_in_scope;

  /* Maintain an array of referenced fundamental types for the current
     compilation unit being read.  For DWARF version 1, we have to construct
     the fundamental types on the fly, since no information about the
     fundamental types is supplied.  Each such fundamental type is created by
     calling a language dependent routine to create the type, and then a
     pointer to that type is then placed in the array at the index specified
     by it's FT_<TYPENAME> value.  The array has a fixed size set by the
     FT_NUM_MEMBERS compile time constant, which is the number of predefined
     fundamental types gdb knows how to construct.  */
  struct type *ftypes[FT_NUM_MEMBERS];	/* Fundamental types */

  /* DWARF abbreviation table associated with this compilation unit.  */
  struct abbrev_info **dwarf2_abbrevs;

  /* Storage for the abbrev table.  */
  struct obstack abbrev_obstack;

  /* Hash table holding all the loaded partial DIEs.  */
  htab_t partial_dies;

  /* Storage for things with the same lifetime as this read-in compilation
     unit, including partial DIEs.  */
  struct obstack comp_unit_obstack;

  /* When multiple dwarf2_cu structures are living in memory, this field
     chains them all together, so that they can be released efficiently.
     We will probably also want a generation counter so that most-recently-used
     compilation units are cached...  */
  struct dwarf2_per_cu_data *read_in_chain;

  /* Backchain to our per_cu entry if the tree has been built.  */
  struct dwarf2_per_cu_data *per_cu;

  /* How many compilation units ago was this CU last referenced?  */
  int last_used;

  /* A hash table of die offsets for following references.  */
  struct die_info *die_ref_table[REF_HASH_SIZE];

  /* Full DIEs if read in.  */
  struct die_info *dies;

  /* A set of pointers to dwarf2_per_cu_data objects for compilation
     units referenced by this one.  Only set during full symbol processing;
     partial symbol tables do not have dependencies.  */
  htab_t dependencies;

  /* Header data from the line table, during full symbol processing.  */
  struct line_header *line_header;

  /* Mark used when releasing cached dies.  */
  unsigned int mark : 1;

  /* This flag will be set if this compilation unit might include
     inter-compilation-unit references.  */
  unsigned int has_form_ref_addr : 1;

  /* This flag will be set if this compilation unit includes any
     DW_TAG_namespace DIEs.  If we know that there are explicit
     DIEs for namespaces, we don't need to try to infer them
     from mangled names.  */
  unsigned int has_namespace_info : 1;
};

/* Persistent data held for a compilation unit, even when not
   processing it.  We put a pointer to this structure in the
   read_symtab_private field of the psymtab.  If we encounter
   inter-compilation-unit references, we also maintain a sorted
   list of all compilation units.  */

struct dwarf2_per_cu_data
{
  /* The start offset and length of this compilation unit.  2**30-1
     bytes should suffice to store the length of any compilation unit
     - if it doesn't, GDB will fall over anyway.  */
  unsigned long offset;
  unsigned long length : 30;

  /* Flag indicating this compilation unit will be read in before
     any of the current compilation units are processed.  */
  unsigned long queued : 1;

  /* This flag will be set if we need to load absolutely all DIEs
     for this compilation unit, instead of just the ones we think
     are interesting.  It gets set if we look for a DIE in the
     hash table and don't find it.  */
  unsigned int load_all_dies : 1;

  /* Set iff currently read in.  */
  struct dwarf2_cu *cu;

  /* If full symbols for this CU have been read in, then this field
     holds a map of DIE offsets to types.  It isn't always possible
     to reconstruct this information later, so we have to preserve
     it.  */
  htab_t type_hash;

  /* The partial symbol table associated with this compilation unit,
     or NULL for partial units (which do not have an associated
     symtab).  */
  struct partial_symtab *psymtab;
};

/* The line number information for a compilation unit (found in the
   .debug_line section) begins with a "statement program header",
   which contains the following information.  */
struct line_header
{
  unsigned int total_length;
  unsigned short version;
  unsigned int header_length;
  unsigned char minimum_instruction_length;
  unsigned char default_is_stmt;
  int line_base;
  unsigned char line_range;
  unsigned char opcode_base;

  /* standard_opcode_lengths[i] is the number of operands for the
     standard opcode whose value is i.  This means that
     standard_opcode_lengths[0] is unused, and the last meaningful
     element is standard_opcode_lengths[opcode_base - 1].  */
  unsigned char *standard_opcode_lengths;

  /* The include_directories table.  NOTE!  These strings are not
     allocated with xmalloc; instead, they are pointers into
     debug_line_buffer.  If you try to free them, `free' will get
     indigestion.  */
  unsigned int num_include_dirs, include_dirs_size;
  char **include_dirs;

  /* The file_names table.  NOTE!  These strings are not allocated
     with xmalloc; instead, they are pointers into debug_line_buffer.
     Don't try to free them directly.  */
  unsigned int num_file_names, file_names_size;
  struct file_entry
  {
    char *name;
    unsigned int dir_index;
    unsigned int mod_time;
    unsigned int length;
    int included_p; /* Non-zero if referenced by the Line Number Program.  */
    struct symtab *symtab; /* The associated symbol table, if any.  */
  } *file_names;

  /* The start and end of the statement program following this
     header.  These point into dwarf2_per_objfile->line_buffer.  */
  gdb_byte *statement_program_start, *statement_program_end;
};

/* When we construct a partial symbol table entry we only
   need this much information. */
struct partial_die_info
  {
    /* Offset of this DIE.  */
    unsigned int offset;

    /* DWARF-2 tag for this DIE.  */
    ENUM_BITFIELD(dwarf_tag) tag : 16;

    /* Language code associated with this DIE.  This is only used
       for the compilation unit DIE.  */
    unsigned int language : 8;

    /* Assorted flags describing the data found in this DIE.  */
    unsigned int has_children : 1;
    unsigned int is_external : 1;
    unsigned int is_declaration : 1;
    unsigned int has_type : 1;
    unsigned int has_specification : 1;
    unsigned int has_stmt_list : 1;
    unsigned int has_pc_info : 1;

    /* Flag set if the SCOPE field of this structure has been
       computed.  */
    unsigned int scope_set : 1;

    /* Flag set if the DIE has a byte_size attribute.  */
    unsigned int has_byte_size : 1;

    /* The name of this DIE.  Normally the value of DW_AT_name, but
       sometimes DW_TAG_MIPS_linkage_name or a string computed in some
       other fashion.  */
    char *name;
    char *dirname;

    /* The scope to prepend to our children.  This is generally
       allocated on the comp_unit_obstack, so will disappear
       when this compilation unit leaves the cache.  */
    char *scope;

    /* The location description associated with this DIE, if any.  */
    struct dwarf_block *locdesc;

    /* If HAS_PC_INFO, the PC range associated with this DIE.  */
    CORE_ADDR lowpc;
    CORE_ADDR highpc;

    /* Pointer into the info_buffer pointing at the target of
       DW_AT_sibling, if any.  */
    gdb_byte *sibling;

    /* If HAS_SPECIFICATION, the offset of the DIE referred to by
       DW_AT_specification (or DW_AT_abstract_origin or
       DW_AT_extension).  */
    unsigned int spec_offset;

    /* If HAS_STMT_LIST, the offset of the Line Number Information data.  */
    unsigned int line_offset;

    /* Pointers to this DIE's parent, first child, and next sibling,
       if any.  */
    struct partial_die_info *die_parent, *die_child, *die_sibling;
  };

/* This data structure holds the information of an abbrev. */
struct abbrev_info
  {
    unsigned int number;	/* number identifying abbrev */
    enum dwarf_tag tag;		/* dwarf tag */
    unsigned short has_children;		/* boolean */
    unsigned short num_attrs;	/* number of attributes */
    struct attr_abbrev *attrs;	/* an array of attribute descriptions */
    struct abbrev_info *next;	/* next in chain */
  };

struct attr_abbrev
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
  };

/* This data structure holds a complete die structure. */
struct die_info
  {
    enum dwarf_tag tag;		/* Tag indicating type of die */
    unsigned int abbrev;	/* Abbrev number */
    unsigned int offset;	/* Offset in .debug_info section */
    unsigned int num_attrs;	/* Number of attributes */
    struct attribute *attrs;	/* An array of attributes */
    struct die_info *next_ref;	/* Next die in ref hash table */

    /* The dies in a compilation unit form an n-ary tree.  PARENT
       points to this die's parent; CHILD points to the first child of
       this node; and all the children of a given node are chained
       together via their SIBLING fields, terminated by a die whose
       tag is zero.  */
    struct die_info *child;	/* Its first child, if any.  */
    struct die_info *sibling;	/* Its next sibling, if any.  */
    struct die_info *parent;	/* Its parent, if any.  */

    struct type *type;		/* Cached type information */
  };

/* Attributes have a name and a value */
struct attribute
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
    union
      {
	char *str;
	struct dwarf_block *blk;
	unsigned long unsnd;
	long int snd;
	CORE_ADDR addr;
      }
    u;
  };

struct function_range
{
  const char *name;
  CORE_ADDR lowpc, highpc;
  int seen_line;
  struct function_range *next;
};

/* Get at parts of an attribute structure */

#define DW_STRING(attr)    ((attr)->u.str)
#define DW_UNSND(attr)     ((attr)->u.unsnd)
#define DW_BLOCK(attr)     ((attr)->u.blk)
#define DW_SND(attr)       ((attr)->u.snd)
#define DW_ADDR(attr)	   ((attr)->u.addr)

/* Blocks are a bunch of untyped bytes. */
struct dwarf_block
  {
    unsigned int size;
    gdb_byte *data;
  };

#ifndef ATTR_ALLOC_CHUNK
#define ATTR_ALLOC_CHUNK 4
#endif

/* Allocate fields for structs, unions and enums in this size.  */
#ifndef DW_FIELD_ALLOC_CHUNK
#define DW_FIELD_ALLOC_CHUNK 4
#endif

/* A zeroed version of a partial die for initialization purposes.  */
static struct partial_die_info zeroed_partial_die;

/* FIXME: We might want to set this from BFD via bfd_arch_bits_per_byte,
   but this would require a corresponding change in unpack_field_as_long
   and friends.  */
static int bits_per_byte = 8;

/* The routines that read and process dies for a C struct or C++ class
   pass lists of data member fields and lists of member function fields
   in an instance of a field_info structure, as defined below.  */
struct field_info
  {
    /* List of data member and baseclasses fields. */
    struct nextfield
      {
	struct nextfield *next;
	int accessibility;
	int virtuality;
	struct field field;
      }
     *fields;

    /* Number of fields.  */
    int nfields;

    /* Number of baseclasses.  */
    int nbaseclasses;

    /* Set if the accesibility of one of the fields is not public.  */
    int non_public_fields;

    /* Member function fields array, entries are allocated in the order they
       are encountered in the object file.  */
    struct nextfnfield
      {
	struct nextfnfield *next;
	struct fn_field fnfield;
      }
     *fnfields;

    /* Member function fieldlist array, contains name of possibly overloaded
       member function, number of overloaded member functions and a pointer
       to the head of the member function field chain.  */
    struct fnfieldlist
      {
	char *name;
	int length;
	struct nextfnfield *head;
      }
     *fnfieldlists;

    /* Number of entries in the fnfieldlists array.  */
    int nfnfields;
  };

/* One item on the queue of compilation units to read in full symbols
   for.  */
struct dwarf2_queue_item
{
  struct dwarf2_per_cu_data *per_cu;
  struct dwarf2_queue_item *next;
};

/* The current queue.  */
static struct dwarf2_queue_item *dwarf2_queue, *dwarf2_queue_tail;

/* Loaded secondary compilation units are kept in memory until they
   have not been referenced for the processing of this many
   compilation units.  Set this to zero to disable caching.  Cache
   sizes of up to at least twenty will improve startup time for
   typical inter-CU-reference binaries, at an obvious memory cost.  */
static int dwarf2_max_cache_age = 5;
static void
show_dwarf2_max_cache_age (struct ui_file *file, int from_tty,
			   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
The upper bound on the age of cached dwarf2 compilation units is %s.\n"),
		    value);
}


/* Various complaints about symbol reading that don't abort the process */

static void
dwarf2_statement_list_fits_in_line_number_section_complaint (void)
{
  complaint (&symfile_complaints,
	     _("statement list doesn't fit in .debug_line section"));
}

static void
dwarf2_debug_line_missing_file_complaint (void)
{
  complaint (&symfile_complaints,
	     _(".debug_line section has line data without a file"));
}

static void
dwarf2_complex_location_expr_complaint (void)
{
  complaint (&symfile_complaints, _("location expression too complex"));
}

static void
dwarf2_const_value_length_mismatch_complaint (const char *arg1, int arg2,
					      int arg3)
{
  complaint (&symfile_complaints,
	     _("const value length mismatch for '%s', got %d, expected %d"), arg1,
	     arg2, arg3);
}

static void
dwarf2_macros_too_long_complaint (void)
{
  complaint (&symfile_complaints,
	     _("macro info runs off end of `.debug_macinfo' section"));
}

static void
dwarf2_macro_malformed_definition_complaint (const char *arg1)
{
  complaint (&symfile_complaints,
	     _("macro debug info contains a malformed macro definition:\n`%s'"),
	     arg1);
}

static void
dwarf2_invalid_attrib_class_complaint (const char *arg1, const char *arg2)
{
  complaint (&symfile_complaints,
	     _("invalid attribute class or form for '%s' in '%s'"), arg1, arg2);
}

/* local function prototypes */

static void dwarf2_locate_sections (bfd *, asection *, void *);

#if 0
static void dwarf2_build_psymtabs_easy (struct objfile *, int);
#endif

static void dwarf2_create_include_psymtab (char *, struct partial_symtab *,
                                           struct objfile *);

static void dwarf2_build_include_psymtabs (struct dwarf2_cu *,
                                           struct partial_die_info *,
                                           struct partial_symtab *);

static void dwarf2_build_psymtabs_hard (struct objfile *, int);

static void scan_partial_symbols (struct partial_die_info *,
				  CORE_ADDR *, CORE_ADDR *,
				  struct dwarf2_cu *);

static void add_partial_symbol (struct partial_die_info *,
				struct dwarf2_cu *);

static int pdi_needs_namespace (enum dwarf_tag tag);

static void add_partial_namespace (struct partial_die_info *pdi,
				   CORE_ADDR *lowpc, CORE_ADDR *highpc,
				   struct dwarf2_cu *cu);

static void add_partial_enumeration (struct partial_die_info *enum_pdi,
				     struct dwarf2_cu *cu);

static gdb_byte *locate_pdi_sibling (struct partial_die_info *orig_pdi,
                                     gdb_byte *info_ptr,
                                     bfd *abfd,
                                     struct dwarf2_cu *cu);

static void dwarf2_psymtab_to_symtab (struct partial_symtab *);

static void psymtab_to_symtab_1 (struct partial_symtab *);

gdb_byte *dwarf2_read_section (struct objfile *, asection *);

static void dwarf2_read_abbrevs (bfd *abfd, struct dwarf2_cu *cu);

static void dwarf2_free_abbrev_table (void *);

static struct abbrev_info *peek_die_abbrev (gdb_byte *, unsigned int *,
					    struct dwarf2_cu *);

static struct abbrev_info *dwarf2_lookup_abbrev (unsigned int,
						 struct dwarf2_cu *);

static struct partial_die_info *load_partial_dies (bfd *, gdb_byte *, int,
						   struct dwarf2_cu *);

static gdb_byte *read_partial_die (struct partial_die_info *,
                                   struct abbrev_info *abbrev, unsigned int,
                                   bfd *, gdb_byte *, struct dwarf2_cu *);

static struct partial_die_info *find_partial_die (unsigned long,
						  struct dwarf2_cu *);

static void fixup_partial_die (struct partial_die_info *,
			       struct dwarf2_cu *);

static gdb_byte *read_full_die (struct die_info **, bfd *, gdb_byte *,
                                struct dwarf2_cu *, int *);

static gdb_byte *read_attribute (struct attribute *, struct attr_abbrev *,
                                 bfd *, gdb_byte *, struct dwarf2_cu *);

static gdb_byte *read_attribute_value (struct attribute *, unsigned,
                                       bfd *, gdb_byte *, struct dwarf2_cu *);

static unsigned int read_1_byte (bfd *, gdb_byte *);

static int read_1_signed_byte (bfd *, gdb_byte *);

static unsigned int read_2_bytes (bfd *, gdb_byte *);

static unsigned int read_4_bytes (bfd *, gdb_byte *);

static unsigned long read_8_bytes (bfd *, gdb_byte *);

static CORE_ADDR read_address (bfd *, gdb_byte *ptr, struct dwarf2_cu *,
			       unsigned int *);

static LONGEST read_initial_length (bfd *, gdb_byte *,
                                    struct comp_unit_head *, unsigned int *);

static LONGEST read_offset (bfd *, gdb_byte *, const struct comp_unit_head *,
                            unsigned int *);

static gdb_byte *read_n_bytes (bfd *, gdb_byte *, unsigned int);

static char *read_string (bfd *, gdb_byte *, unsigned int *);

static char *read_indirect_string (bfd *, gdb_byte *,
                                   const struct comp_unit_head *,
                                   unsigned int *);

static unsigned long read_unsigned_leb128 (bfd *, gdb_byte *, unsigned int *);

static long read_signed_leb128 (bfd *, gdb_byte *, unsigned int *);

static gdb_byte *skip_leb128 (bfd *, gdb_byte *);

static void set_cu_language (unsigned int, struct dwarf2_cu *);

static struct attribute *dwarf2_attr (struct die_info *, unsigned int,
				      struct dwarf2_cu *);

static int dwarf2_flag_true_p (struct die_info *die, unsigned name,
                               struct dwarf2_cu *cu);

static int die_is_declaration (struct die_info *, struct dwarf2_cu *cu);

static struct die_info *die_specification (struct die_info *die,
					   struct dwarf2_cu *);

static void free_line_header (struct line_header *lh);

static void add_file_name (struct line_header *, char *, unsigned int,
                           unsigned int, unsigned int);

static struct line_header *(dwarf_decode_line_header
                            (unsigned int offset,
                             bfd *abfd, struct dwarf2_cu *cu));

static void dwarf_decode_lines (struct line_header *, char *, bfd *,
				struct dwarf2_cu *, struct partial_symtab *);

static void dwarf2_start_subfile (char *, char *, char *);

static struct symbol *new_symbol (struct die_info *, struct type *,
				  struct dwarf2_cu *);

static void dwarf2_const_value (struct attribute *, struct symbol *,
				struct dwarf2_cu *);

static void dwarf2_const_value_data (struct attribute *attr,
				     struct symbol *sym,
				     int bits);

static struct type *die_type (struct die_info *, struct dwarf2_cu *);

static struct type *die_containing_type (struct die_info *,
					 struct dwarf2_cu *);

static struct type *tag_type_to_type (struct die_info *, struct dwarf2_cu *);

static void read_type_die (struct die_info *, struct dwarf2_cu *);

static char *determine_prefix (struct die_info *die, struct dwarf2_cu *);

static char *typename_concat (struct obstack *,
                              const char *prefix, 
                              const char *suffix,
			      struct dwarf2_cu *);

static void read_typedef (struct die_info *, struct dwarf2_cu *);

static void read_base_type (struct die_info *, struct dwarf2_cu *);

static void read_subrange_type (struct die_info *die, struct dwarf2_cu *cu);

static void read_file_scope (struct die_info *, struct dwarf2_cu *);

static void read_func_scope (struct die_info *, struct dwarf2_cu *);

static void read_lexical_block_scope (struct die_info *, struct dwarf2_cu *);

static int dwarf2_get_pc_bounds (struct die_info *,
				 CORE_ADDR *, CORE_ADDR *, struct dwarf2_cu *);

static void get_scope_pc_bounds (struct die_info *,
				 CORE_ADDR *, CORE_ADDR *,
				 struct dwarf2_cu *);

static void dwarf2_add_field (struct field_info *, struct die_info *,
			      struct dwarf2_cu *);

static void dwarf2_attach_fields_to_type (struct field_info *,
					  struct type *, struct dwarf2_cu *);

static void dwarf2_add_member_fn (struct field_info *,
				  struct die_info *, struct type *,
				  struct dwarf2_cu *);

static void dwarf2_attach_fn_fields_to_type (struct field_info *,
					     struct type *, struct dwarf2_cu *);

static void read_structure_type (struct die_info *, struct dwarf2_cu *);

static void process_structure_scope (struct die_info *, struct dwarf2_cu *);

static char *determine_class_name (struct die_info *die, struct dwarf2_cu *cu);

static void read_common_block (struct die_info *, struct dwarf2_cu *);

static void read_namespace (struct die_info *die, struct dwarf2_cu *);

static const char *namespace_name (struct die_info *die,
				   int *is_anonymous, struct dwarf2_cu *);

static void read_enumeration_type (struct die_info *, struct dwarf2_cu *);

static void process_enumeration_scope (struct die_info *, struct dwarf2_cu *);

static struct type *dwarf_base_type (int, int, struct dwarf2_cu *);

static CORE_ADDR decode_locdesc (struct dwarf_block *, struct dwarf2_cu *);

static void read_array_type (struct die_info *, struct dwarf2_cu *);

static enum dwarf_array_dim_ordering read_array_order (struct die_info *, 
						       struct dwarf2_cu *);

static void read_tag_pointer_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_ptr_to_member_type (struct die_info *,
					 struct dwarf2_cu *);

static void read_tag_reference_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_const_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_volatile_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_string_type (struct die_info *, struct dwarf2_cu *);

static void read_subroutine_type (struct die_info *, struct dwarf2_cu *);

static struct die_info *read_comp_unit (gdb_byte *, bfd *, struct dwarf2_cu *);

static struct die_info *read_die_and_children (gdb_byte *info_ptr, bfd *abfd,
					       struct dwarf2_cu *,
					       gdb_byte **new_info_ptr,
					       struct die_info *parent);

static struct die_info *read_die_and_siblings (gdb_byte *info_ptr, bfd *abfd,
					       struct dwarf2_cu *,
					       gdb_byte **new_info_ptr,
					       struct die_info *parent);

static void free_die_list (struct die_info *);

static void process_die (struct die_info *, struct dwarf2_cu *);

static char *dwarf2_linkage_name (struct die_info *, struct dwarf2_cu *);

static char *dwarf2_name (struct die_info *die, struct dwarf2_cu *);

static struct die_info *dwarf2_extension (struct die_info *die,
					  struct dwarf2_cu *);

static char *dwarf_tag_name (unsigned int);

static char *dwarf_attr_name (unsigned int);

static char *dwarf_form_name (unsigned int);

static char *dwarf_stack_op_name (unsigned int);

static char *dwarf_bool_name (unsigned int);

static char *dwarf_type_encoding_name (unsigned int);

#if 0
static char *dwarf_cfi_name (unsigned int);

struct die_info *copy_die (struct die_info *);
#endif

static struct die_info *sibling_die (struct die_info *);

static void dump_die (struct die_info *);

static void dump_die_list (struct die_info *);

static void store_in_ref_table (unsigned int, struct die_info *,
				struct dwarf2_cu *);

static unsigned int dwarf2_get_ref_die_offset (struct attribute *,
					       struct dwarf2_cu *);

static int dwarf2_get_attr_constant_value (struct attribute *, int);

static struct die_info *follow_die_ref (struct die_info *,
					struct attribute *,
					struct dwarf2_cu *);

static struct type *dwarf2_fundamental_type (struct objfile *, int,
					     struct dwarf2_cu *);

/* memory allocation interface */

static struct dwarf_block *dwarf_alloc_block (struct dwarf2_cu *);

static struct abbrev_info *dwarf_alloc_abbrev (struct dwarf2_cu *);

static struct die_info *dwarf_alloc_die (void);

static void initialize_cu_func_list (struct dwarf2_cu *);

static void add_to_cu_func_list (const char *, CORE_ADDR, CORE_ADDR,
				 struct dwarf2_cu *);

static void dwarf_decode_macros (struct line_header *, unsigned int,
                                 char *, bfd *, struct dwarf2_cu *);

static int attr_form_is_block (struct attribute *);

static void dwarf2_symbol_mark_computed (struct attribute *attr,
					 struct symbol *sym,
					 struct dwarf2_cu *cu);

static gdb_byte *skip_one_die (gdb_byte *info_ptr, struct abbrev_info *abbrev,
                               struct dwarf2_cu *cu);

static void free_stack_comp_unit (void *);

static hashval_t partial_die_hash (const void *item);

static int partial_die_eq (const void *item_lhs, const void *item_rhs);

static struct dwarf2_per_cu_data *dwarf2_find_containing_comp_unit
  (unsigned long offset, struct objfile *objfile);

static struct dwarf2_per_cu_data *dwarf2_find_comp_unit
  (unsigned long offset, struct objfile *objfile);

static void free_one_comp_unit (void *);

static void free_cached_comp_units (void *);

static void age_cached_comp_units (void);

static void free_one_cached_comp_unit (void *);

static void set_die_type (struct die_info *, struct type *,
			  struct dwarf2_cu *);

static void reset_die_and_siblings_types (struct die_info *,
					  struct dwarf2_cu *);

static void create_all_comp_units (struct objfile *);

static struct dwarf2_cu *load_full_comp_unit (struct dwarf2_per_cu_data *,
					      struct objfile *);

static void process_full_comp_unit (struct dwarf2_per_cu_data *);

static void dwarf2_add_dependence (struct dwarf2_cu *,
				   struct dwarf2_per_cu_data *);

static void dwarf2_mark (struct dwarf2_cu *);

static void dwarf2_clear_marks (struct dwarf2_per_cu_data *);

static void read_set_type (struct die_info *, struct dwarf2_cu *);


/* Try to locate the sections we need for DWARF 2 debugging
   information and return true if we have enough to do something.  */

int
dwarf2_has_info (struct objfile *objfile)
{
  struct dwarf2_per_objfile *data;

  /* Initialize per-objfile state.  */
  data = obstack_alloc (&objfile->objfile_obstack, sizeof (*data));
  memset (data, 0, sizeof (*data));
  set_objfile_data (objfile, dwarf2_objfile_data_key, data);
  dwarf2_per_objfile = data;

  dwarf_info_section = 0;
  dwarf_abbrev_section = 0;
  dwarf_line_section = 0;
  dwarf_str_section = 0;
  dwarf_macinfo_section = 0;
  dwarf_frame_section = 0;
  dwarf_eh_frame_section = 0;
  dwarf_ranges_section = 0;
  dwarf_loc_section = 0;
  
  bfd_map_over_sections (objfile->obfd, dwarf2_locate_sections, NULL);
  return (dwarf_info_section != NULL && dwarf_abbrev_section != NULL);
}

/* This function is mapped across the sections and remembers the
   offset and size of each of the debugging sections we are interested
   in.  */

static void
dwarf2_locate_sections (bfd *abfd, asection *sectp, void *ignore_ptr)
{
  if (strcmp (sectp->name, INFO_SECTION) == 0)
    {
      dwarf2_per_objfile->info_size = bfd_get_section_size (sectp);
      dwarf_info_section = sectp;
    }
  else if (strcmp (sectp->name, ABBREV_SECTION) == 0)
    {
      dwarf2_per_objfile->abbrev_size = bfd_get_section_size (sectp);
      dwarf_abbrev_section = sectp;
    }
  else if (strcmp (sectp->name, LINE_SECTION) == 0)
    {
      dwarf2_per_objfile->line_size = bfd_get_section_size (sectp);
      dwarf_line_section = sectp;
    }
  else if (strcmp (sectp->name, PUBNAMES_SECTION) == 0)
    {
      dwarf2_per_objfile->pubnames_size = bfd_get_section_size (sectp);
      dwarf_pubnames_section = sectp;
    }
  else if (strcmp (sectp->name, ARANGES_SECTION) == 0)
    {
      dwarf2_per_objfile->aranges_size = bfd_get_section_size (sectp);
      dwarf_aranges_section = sectp;
    }
  else if (strcmp (sectp->name, LOC_SECTION) == 0)
    {
      dwarf2_per_objfile->loc_size = bfd_get_section_size (sectp);
      dwarf_loc_section = sectp;
    }
  else if (strcmp (sectp->name, MACINFO_SECTION) == 0)
    {
      dwarf2_per_objfile->macinfo_size = bfd_get_section_size (sectp);
      dwarf_macinfo_section = sectp;
    }
  else if (strcmp (sectp->name, STR_SECTION) == 0)
    {
      dwarf2_per_objfile->str_size = bfd_get_section_size (sectp);
      dwarf_str_section = sectp;
    }
  else if (strcmp (sectp->name, FRAME_SECTION) == 0)
    {
      dwarf2_per_objfile->frame_size = bfd_get_section_size (sectp);
      dwarf_frame_section = sectp;
    }
  else if (strcmp (sectp->name, EH_FRAME_SECTION) == 0)
    {
      flagword aflag = bfd_get_section_flags (ignore_abfd, sectp);
      if (aflag & SEC_HAS_CONTENTS)
        {
          dwarf2_per_objfile->eh_frame_size = bfd_get_section_size (sectp);
          dwarf_eh_frame_section = sectp;
        }
    }
  else if (strcmp (sectp->name, RANGES_SECTION) == 0)
    {
      dwarf2_per_objfile->ranges_size = bfd_get_section_size (sectp);
      dwarf_ranges_section = sectp;
    }
  
  if ((bfd_get_section_flags (abfd, sectp) & SEC_LOAD)
      && bfd_section_vma (abfd, sectp) == 0)
    dwarf2_per_objfile->has_section_at_zero = 1;
}

/* Build a partial symbol table.  */

void
dwarf2_build_psymtabs (struct objfile *objfile, int mainline)
{
  /* We definitely need the .debug_info and .debug_abbrev sections */

  dwarf2_per_objfile->info_buffer = dwarf2_read_section (objfile, dwarf_info_section);
  dwarf2_per_objfile->abbrev_buffer = dwarf2_read_section (objfile, dwarf_abbrev_section);

  if (dwarf_line_section)
    dwarf2_per_objfile->line_buffer = dwarf2_read_section (objfile, dwarf_line_section);
  else
    dwarf2_per_objfile->line_buffer = NULL;

  if (dwarf_str_section)
    dwarf2_per_objfile->str_buffer = dwarf2_read_section (objfile, dwarf_str_section);
  else
    dwarf2_per_objfile->str_buffer = NULL;

  if (dwarf_macinfo_section)
    dwarf2_per_objfile->macinfo_buffer = dwarf2_read_section (objfile,
						dwarf_macinfo_section);
  else
    dwarf2_per_objfile->macinfo_buffer = NULL;

  if (dwarf_ranges_section)
    dwarf2_per_objfile->ranges_buffer = dwarf2_read_section (objfile, dwarf_ranges_section);
  else
    dwarf2_per_objfile->ranges_buffer = NULL;

  if (dwarf_loc_section)
    dwarf2_per_objfile->loc_buffer = dwarf2_read_section (objfile, dwarf_loc_section);
  else
    dwarf2_per_objfile->loc_buffer = NULL;

  if (mainline
      || (objfile->global_psymbols.size == 0
	  && objfile->static_psymbols.size == 0))
    {
      init_psymbol_list (objfile, 1024);
    }

#if 0
  if (dwarf_aranges_offset && dwarf_pubnames_offset)
    {
      /* Things are significantly easier if we have .debug_aranges and
         .debug_pubnames sections */

      dwarf2_build_psymtabs_easy (objfile, mainline);
    }
  else
#endif
    /* only test this case for now */
    {
      /* In this case we have to work a bit harder */
      dwarf2_build_psymtabs_hard (objfile, mainline);
    }
}

#if 0
/* Build the partial symbol table from the information in the
   .debug_pubnames and .debug_aranges sections.  */

static void
dwarf2_build_psymtabs_easy (struct objfile *objfile, int mainline)
{
  bfd *abfd = objfile->obfd;
  char *aranges_buffer, *pubnames_buffer;
  char *aranges_ptr, *pubnames_ptr;
  unsigned int entry_length, version, info_offset, info_size;

  pubnames_buffer = dwarf2_read_section (objfile,
					 dwarf_pubnames_section);
  pubnames_ptr = pubnames_buffer;
  while ((pubnames_ptr - pubnames_buffer) < dwarf2_per_objfile->pubnames_size)
    {
      struct comp_unit_head cu_header;
      unsigned int bytes_read;

      entry_length = read_initial_length (abfd, pubnames_ptr, &cu_header,
                                          &bytes_read);
      pubnames_ptr += bytes_read;
      version = read_1_byte (abfd, pubnames_ptr);
      pubnames_ptr += 1;
      info_offset = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
      info_size = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
    }

  aranges_buffer = dwarf2_read_section (objfile,
					dwarf_aranges_section);

}
#endif

/* Read in the comp unit header information from the debug_info at
   info_ptr.  */

static gdb_byte *
read_comp_unit_head (struct comp_unit_head *cu_header,
		     gdb_byte *info_ptr, bfd *abfd)
{
  int signed_addr;
  unsigned int bytes_read;
  cu_header->length = read_initial_length (abfd, info_ptr, cu_header,
                                           &bytes_read);
  info_ptr += bytes_read;
  cu_header->version = read_2_bytes (abfd, info_ptr);
  info_ptr += 2;
  cu_header->abbrev_offset = read_offset (abfd, info_ptr, cu_header,
                                          &bytes_read);
  info_ptr += bytes_read;
  cu_header->addr_size = read_1_byte (abfd, info_ptr);
  info_ptr += 1;
  signed_addr = bfd_get_sign_extend_vma (abfd);
  if (signed_addr < 0)
    internal_error (__FILE__, __LINE__,
		    _("read_comp_unit_head: dwarf from non elf file"));
  cu_header->signed_addr_p = signed_addr;
  return info_ptr;
}

static gdb_byte *
partial_read_comp_unit_head (struct comp_unit_head *header, gdb_byte *info_ptr,
			     bfd *abfd)
{
  gdb_byte *beg_of_comp_unit = info_ptr;

  info_ptr = read_comp_unit_head (header, info_ptr, abfd);

  if (header->version != 2 && header->version != 3)
    error (_("Dwarf Error: wrong version in compilation unit header "
	   "(is %d, should be %d) [in module %s]"), header->version,
	   2, bfd_get_filename (abfd));

  if (header->abbrev_offset >= dwarf2_per_objfile->abbrev_size)
    error (_("Dwarf Error: bad offset (0x%lx) in compilation unit header "
	   "(offset 0x%lx + 6) [in module %s]"),
	   (long) header->abbrev_offset,
	   (long) (beg_of_comp_unit - dwarf2_per_objfile->info_buffer),
	   bfd_get_filename (abfd));

  if (beg_of_comp_unit + header->length + header->initial_length_size
      > dwarf2_per_objfile->info_buffer + dwarf2_per_objfile->info_size)
    error (_("Dwarf Error: bad length (0x%lx) in compilation unit header "
	   "(offset 0x%lx + 0) [in module %s]"),
	   (long) header->length,
	   (long) (beg_of_comp_unit - dwarf2_per_objfile->info_buffer),
	   bfd_get_filename (abfd));

  return info_ptr;
}

/* Allocate a new partial symtab for file named NAME and mark this new
   partial symtab as being an include of PST.  */

static void
dwarf2_create_include_psymtab (char *name, struct partial_symtab *pst,
                               struct objfile *objfile)
{
  struct partial_symtab *subpst = allocate_psymtab (name, objfile);

  subpst->section_offsets = pst->section_offsets;
  subpst->textlow = 0;
  subpst->texthigh = 0;

  subpst->dependencies = (struct partial_symtab **)
    obstack_alloc (&objfile->objfile_obstack,
                   sizeof (struct partial_symtab *));
  subpst->dependencies[0] = pst;
  subpst->number_of_dependencies = 1;

  subpst->globals_offset = 0;
  subpst->n_global_syms = 0;
  subpst->statics_offset = 0;
  subpst->n_static_syms = 0;
  subpst->symtab = NULL;
  subpst->read_symtab = pst->read_symtab;
  subpst->readin = 0;

  /* No private part is necessary for include psymtabs.  This property
     can be used to differentiate between such include psymtabs and
     the regular ones.  */
  subpst->read_symtab_private = NULL;
}

/* Read the Line Number Program data and extract the list of files
   included by the source file represented by PST.  Build an include
   partial symtab for each of these included files.
   
   This procedure assumes that there *is* a Line Number Program in
   the given CU.  Callers should check that PDI->HAS_STMT_LIST is set
   before calling this procedure.  */

static void
dwarf2_build_include_psymtabs (struct dwarf2_cu *cu,
                               struct partial_die_info *pdi,
                               struct partial_symtab *pst)
{
  struct objfile *objfile = cu->objfile;
  bfd *abfd = objfile->obfd;
  struct line_header *lh;

  lh = dwarf_decode_line_header (pdi->line_offset, abfd, cu);
  if (lh == NULL)
    return;  /* No linetable, so no includes.  */

  dwarf_decode_lines (lh, NULL, abfd, cu, pst);

  free_line_header (lh);
}


/* Build the partial symbol table by doing a quick pass through the
   .debug_info and .debug_abbrev sections.  */

static void
dwarf2_build_psymtabs_hard (struct objfile *objfile, int mainline)
{
  /* Instead of reading this into a big buffer, we should probably use
     mmap()  on architectures that support it. (FIXME) */
  bfd *abfd = objfile->obfd;
  gdb_byte *info_ptr;
  gdb_byte *beg_of_comp_unit;
  struct partial_die_info comp_unit_die;
  struct partial_symtab *pst;
  struct cleanup *back_to;
  CORE_ADDR lowpc, highpc, baseaddr;

  info_ptr = dwarf2_per_objfile->info_buffer;

  /* Any cached compilation units will be linked by the per-objfile
     read_in_chain.  Make sure to free them when we're done.  */
  back_to = make_cleanup (free_cached_comp_units, NULL);

  create_all_comp_units (objfile);

  /* Since the objects we're extracting from .debug_info vary in
     length, only the individual functions to extract them (like
     read_comp_unit_head and load_partial_die) can really know whether
     the buffer is large enough to hold another complete object.

     At the moment, they don't actually check that.  If .debug_info
     holds just one extra byte after the last compilation unit's dies,
     then read_comp_unit_head will happily read off the end of the
     buffer.  read_partial_die is similarly casual.  Those functions
     should be fixed.

     For this loop condition, simply checking whether there's any data
     left at all should be sufficient.  */
  while (info_ptr < (dwarf2_per_objfile->info_buffer
		     + dwarf2_per_objfile->info_size))
    {
      struct cleanup *back_to_inner;
      struct dwarf2_cu cu;
      struct abbrev_info *abbrev;
      unsigned int bytes_read;
      struct dwarf2_per_cu_data *this_cu;

      beg_of_comp_unit = info_ptr;

      memset (&cu, 0, sizeof (cu));

      obstack_init (&cu.comp_unit_obstack);

      back_to_inner = make_cleanup (free_stack_comp_unit, &cu);

      cu.objfile = objfile;
      info_ptr = partial_read_comp_unit_head (&cu.header, info_ptr, abfd);

      /* Complete the cu_header */
      cu.header.offset = beg_of_comp_unit - dwarf2_per_objfile->info_buffer;
      cu.header.first_die_ptr = info_ptr;
      cu.header.cu_head_ptr = beg_of_comp_unit;

      cu.list_in_scope = &file_symbols;

      /* Read the abbrevs for this compilation unit into a table */
      dwarf2_read_abbrevs (abfd, &cu);
      make_cleanup (dwarf2_free_abbrev_table, &cu);

      this_cu = dwarf2_find_comp_unit (cu.header.offset, objfile);

      /* Read the compilation unit die */
      abbrev = peek_die_abbrev (info_ptr, &bytes_read, &cu);
      info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
				   abfd, info_ptr, &cu);

      if (comp_unit_die.tag == DW_TAG_partial_unit)
	{
	  info_ptr = (beg_of_comp_unit + cu.header.length
		      + cu.header.initial_length_size);
	  do_cleanups (back_to_inner);
	  continue;
	}

      /* Set the language we're debugging */
      set_cu_language (comp_unit_die.language, &cu);

      /* Allocate a new partial symbol table structure */
      pst = start_psymtab_common (objfile, objfile->section_offsets,
				  comp_unit_die.name ? comp_unit_die.name : "",
				  comp_unit_die.lowpc,
				  objfile->global_psymbols.next,
				  objfile->static_psymbols.next);

      if (comp_unit_die.dirname)
	pst->dirname = xstrdup (comp_unit_die.dirname);

      pst->read_symtab_private = (char *) this_cu;

      baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

      /* Store the function that reads in the rest of the symbol table */
      pst->read_symtab = dwarf2_psymtab_to_symtab;

      /* If this compilation unit was already read in, free the
	 cached copy in order to read it in again.  This is
	 necessary because we skipped some symbols when we first
	 read in the compilation unit (see load_partial_dies).
	 This problem could be avoided, but the benefit is
	 unclear.  */
      if (this_cu->cu != NULL)
	free_one_cached_comp_unit (this_cu->cu);

      cu.per_cu = this_cu;

      /* Note that this is a pointer to our stack frame, being
	 added to a global data structure.  It will be cleaned up
	 in free_stack_comp_unit when we finish with this
	 compilation unit.  */
      this_cu->cu = &cu;

      this_cu->psymtab = pst;

      /* Check if comp unit has_children.
         If so, read the rest of the partial symbols from this comp unit.
         If not, there's no more debug_info for this comp unit. */
      if (comp_unit_die.has_children)
	{
	  struct partial_die_info *first_die;

	  lowpc = ((CORE_ADDR) -1);
	  highpc = ((CORE_ADDR) 0);

	  first_die = load_partial_dies (abfd, info_ptr, 1, &cu);

	  scan_partial_symbols (first_die, &lowpc, &highpc, &cu);

	  /* If we didn't find a lowpc, set it to highpc to avoid
	     complaints from `maint check'.  */
	  if (lowpc == ((CORE_ADDR) -1))
	    lowpc = highpc;

	  /* If the compilation unit didn't have an explicit address range,
	     then use the information extracted from its child dies.  */
	  if (! comp_unit_die.has_pc_info)
	    {
	      comp_unit_die.lowpc = lowpc;
	      comp_unit_die.highpc = highpc;
	    }
	}
      pst->textlow = comp_unit_die.lowpc + baseaddr;
      pst->texthigh = comp_unit_die.highpc + baseaddr;

      pst->n_global_syms = objfile->global_psymbols.next -
	(objfile->global_psymbols.list + pst->globals_offset);
      pst->n_static_syms = objfile->static_psymbols.next -
	(objfile->static_psymbols.list + pst->statics_offset);
      sort_pst_symbols (pst);

      /* If there is already a psymtab or symtab for a file of this
         name, remove it. (If there is a symtab, more drastic things
         also happen.) This happens in VxWorks.  */
      free_named_symtabs (pst->filename);

      info_ptr = beg_of_comp_unit + cu.header.length
                                  + cu.header.initial_length_size;

      if (comp_unit_die.has_stmt_list)
        {
          /* Get the list of files included in the current compilation unit,
             and build a psymtab for each of them.  */
          dwarf2_build_include_psymtabs (&cu, &comp_unit_die, pst);
        }

      do_cleanups (back_to_inner);
    }
  do_cleanups (back_to);
}

/* Load the DIEs for a secondary CU into memory.  */

static void
load_comp_unit (struct dwarf2_per_cu_data *this_cu, struct objfile *objfile)
{
  bfd *abfd = objfile->obfd;
  gdb_byte *info_ptr, *beg_of_comp_unit;
  struct partial_die_info comp_unit_die;
  struct dwarf2_cu *cu;
  struct abbrev_info *abbrev;
  unsigned int bytes_read;
  struct cleanup *back_to;

  info_ptr = dwarf2_per_objfile->info_buffer + this_cu->offset;
  beg_of_comp_unit = info_ptr;

  cu = xmalloc (sizeof (struct dwarf2_cu));
  memset (cu, 0, sizeof (struct dwarf2_cu));

  obstack_init (&cu->comp_unit_obstack);

  cu->objfile = objfile;
  info_ptr = partial_read_comp_unit_head (&cu->header, info_ptr, abfd);

  /* Complete the cu_header.  */
  cu->header.offset = beg_of_comp_unit - dwarf2_per_objfile->info_buffer;
  cu->header.first_die_ptr = info_ptr;
  cu->header.cu_head_ptr = beg_of_comp_unit;

  /* Read the abbrevs for this compilation unit into a table.  */
  dwarf2_read_abbrevs (abfd, cu);
  back_to = make_cleanup (dwarf2_free_abbrev_table, cu);

  /* Read the compilation unit die.  */
  abbrev = peek_die_abbrev (info_ptr, &bytes_read, cu);
  info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
			       abfd, info_ptr, cu);

  /* Set the language we're debugging.  */
  set_cu_language (comp_unit_die.language, cu);

  /* Link this compilation unit into the compilation unit tree.  */
  this_cu->cu = cu;
  cu->per_cu = this_cu;

  /* Check if comp unit has_children.
     If so, read the rest of the partial symbols from this comp unit.
     If not, there's no more debug_info for this comp unit. */
  if (comp_unit_die.has_children)
    load_partial_dies (abfd, info_ptr, 0, cu);

  do_cleanups (back_to);
}

/* Create a list of all compilation units in OBJFILE.  We do this only
   if an inter-comp-unit reference is found; presumably if there is one,
   there will be many, and one will occur early in the .debug_info section.
   So there's no point in building this list incrementally.  */

static void
create_all_comp_units (struct objfile *objfile)
{
  int n_allocated;
  int n_comp_units;
  struct dwarf2_per_cu_data **all_comp_units;
  gdb_byte *info_ptr = dwarf2_per_objfile->info_buffer;

  n_comp_units = 0;
  n_allocated = 10;
  all_comp_units = xmalloc (n_allocated
			    * sizeof (struct dwarf2_per_cu_data *));
  
  while (info_ptr < dwarf2_per_objfile->info_buffer + dwarf2_per_objfile->info_size)
    {
      struct comp_unit_head cu_header;
      gdb_byte *beg_of_comp_unit;
      struct dwarf2_per_cu_data *this_cu;
      unsigned long offset;
      unsigned int bytes_read;

      offset = info_ptr - dwarf2_per_objfile->info_buffer;

      /* Read just enough information to find out where the next
	 compilation unit is.  */
      cu_header.initial_length_size = 0;
      cu_header.length = read_initial_length (objfile->obfd, info_ptr,
					      &cu_header, &bytes_read);

      /* Save the compilation unit for later lookup.  */
      this_cu = obstack_alloc (&objfile->objfile_obstack,
			       sizeof (struct dwarf2_per_cu_data));
      memset (this_cu, 0, sizeof (*this_cu));
      this_cu->offset = offset;
      this_cu->length = cu_header.length + cu_header.initial_length_size;

      if (n_comp_units == n_allocated)
	{
	  n_allocated *= 2;
	  all_comp_units = xrealloc (all_comp_units,
				     n_allocated
				     * sizeof (struct dwarf2_per_cu_data *));
	}
      all_comp_units[n_comp_units++] = this_cu;

      info_ptr = info_ptr + this_cu->length;
    }

  dwarf2_per_objfile->all_comp_units
    = obstack_alloc (&objfile->objfile_obstack,
		     n_comp_units * sizeof (struct dwarf2_per_cu_data *));
  memcpy (dwarf2_per_objfile->all_comp_units, all_comp_units,
	  n_comp_units * sizeof (struct dwarf2_per_cu_data *));
  xfree (all_comp_units);
  dwarf2_per_objfile->n_comp_units = n_comp_units;
}

/* Process all loaded DIEs for compilation unit CU, starting at FIRST_DIE.
   Also set *LOWPC and *HIGHPC to the lowest and highest PC values found
   in CU.  */

static void
scan_partial_symbols (struct partial_die_info *first_die, CORE_ADDR *lowpc,
		      CORE_ADDR *highpc, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  bfd *abfd = objfile->obfd;
  struct partial_die_info *pdi;

  /* Now, march along the PDI's, descending into ones which have
     interesting children but skipping the children of the other ones,
     until we reach the end of the compilation unit.  */

  pdi = first_die;

  while (pdi != NULL)
    {
      fixup_partial_die (pdi, cu);

      /* Anonymous namespaces have no name but have interesting
	 children, so we need to look at them.  Ditto for anonymous
	 enums.  */

      if (pdi->name != NULL || pdi->tag == DW_TAG_namespace
	  || pdi->tag == DW_TAG_enumeration_type)
	{
	  switch (pdi->tag)
	    {
	    case DW_TAG_subprogram:
	      if (pdi->has_pc_info)
		{
		  if (pdi->lowpc < *lowpc)
		    {
		      *lowpc = pdi->lowpc;
		    }
		  if (pdi->highpc > *highpc)
		    {
		      *highpc = pdi->highpc;
		    }
		  if (!pdi->is_declaration)
		    {
		      add_partial_symbol (pdi, cu);
		    }
		}
	      break;
	    case DW_TAG_variable:
	    case DW_TAG_typedef:
	    case DW_TAG_union_type:
	      if (!pdi->is_declaration)
		{
		  add_partial_symbol (pdi, cu);
		}
	      break;
	    case DW_TAG_class_type:
	    case DW_TAG_structure_type:
	      if (!pdi->is_declaration)
		{
		  add_partial_symbol (pdi, cu);
		}
	      break;
	    case DW_TAG_enumeration_type:
	      if (!pdi->is_declaration)
		add_partial_enumeration (pdi, cu);
	      break;
	    case DW_TAG_base_type:
            case DW_TAG_subrange_type:
	      /* File scope base type definitions are added to the partial
	         symbol table.  */
	      add_partial_symbol (pdi, cu);
	      break;
	    case DW_TAG_namespace:
	      add_partial_namespace (pdi, lowpc, highpc, cu);
	      break;
	    default:
	      break;
	    }
	}

      /* If the die has a sibling, skip to the sibling.  */

      pdi = pdi->die_sibling;
    }
}

/* Functions used to compute the fully scoped name of a partial DIE.

   Normally, this is simple.  For C++, the parent DIE's fully scoped
   name is concatenated with "::" and the partial DIE's name.  For
   Java, the same thing occurs except that "." is used instead of "::".
   Enumerators are an exception; they use the scope of their parent
   enumeration type, i.e. the name of the enumeration type is not
   prepended to the enumerator.

   There are two complexities.  One is DW_AT_specification; in this
   case "parent" means the parent of the target of the specification,
   instead of the direct parent of the DIE.  The other is compilers
   which do not emit DW_TAG_namespace; in this case we try to guess
   the fully qualified name of structure types from their members'
   linkage names.  This must be done using the DIE's children rather
   than the children of any DW_AT_specification target.  We only need
   to do this for structures at the top level, i.e. if the target of
   any DW_AT_specification (if any; otherwise the DIE itself) does not
   have a parent.  */

/* Compute the scope prefix associated with PDI's parent, in
   compilation unit CU.  The result will be allocated on CU's
   comp_unit_obstack, or a copy of the already allocated PDI->NAME
   field.  NULL is returned if no prefix is necessary.  */
static char *
partial_die_parent_scope (struct partial_die_info *pdi,
			  struct dwarf2_cu *cu)
{
  char *grandparent_scope;
  struct partial_die_info *parent, *real_pdi;

  /* We need to look at our parent DIE; if we have a DW_AT_specification,
     then this means the parent of the specification DIE.  */

  real_pdi = pdi;
  while (real_pdi->has_specification)
    real_pdi = find_partial_die (real_pdi->spec_offset, cu);

  parent = real_pdi->die_parent;
  if (parent == NULL)
    return NULL;

  if (parent->scope_set)
    return parent->scope;

  fixup_partial_die (parent, cu);

  grandparent_scope = partial_die_parent_scope (parent, cu);

  if (parent->tag == DW_TAG_namespace
      || parent->tag == DW_TAG_structure_type
      || parent->tag == DW_TAG_class_type
      || parent->tag == DW_TAG_union_type)
    {
      if (grandparent_scope == NULL)
	parent->scope = parent->name;
      else
	parent->scope = typename_concat (&cu->comp_unit_obstack, grandparent_scope,
					 parent->name, cu);
    }
  else if (parent->tag == DW_TAG_enumeration_type)
    /* Enumerators should not get the name of the enumeration as a prefix.  */
    parent->scope = grandparent_scope;
  else
    {
      /* FIXME drow/2004-04-01: What should we be doing with
	 function-local names?  For partial symbols, we should probably be
	 ignoring them.  */
      complaint (&symfile_complaints,
		 _("unhandled containing DIE tag %d for DIE at %d"),
		 parent->tag, pdi->offset);
      parent->scope = grandparent_scope;
    }

  parent->scope_set = 1;
  return parent->scope;
}

/* Return the fully scoped name associated with PDI, from compilation unit
   CU.  The result will be allocated with malloc.  */
static char *
partial_die_full_name (struct partial_die_info *pdi,
		       struct dwarf2_cu *cu)
{
  char *parent_scope;

  parent_scope = partial_die_parent_scope (pdi, cu);
  if (parent_scope == NULL)
    return NULL;
  else
    return typename_concat (NULL, parent_scope, pdi->name, cu);
}

static void
add_partial_symbol (struct partial_die_info *pdi, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  CORE_ADDR addr = 0;
  char *actual_name = NULL;
  const char *my_prefix;
  const struct partial_symbol *psym = NULL;
  CORE_ADDR baseaddr;
  int built_actual_name = 0;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  if (pdi_needs_namespace (pdi->tag))
    {
      actual_name = partial_die_full_name (pdi, cu);
      if (actual_name)
	built_actual_name = 1;
    }

  if (actual_name == NULL)
    actual_name = pdi->name;

  switch (pdi->tag)
    {
    case DW_TAG_subprogram:
      if (pdi->is_external)
	{
	  /*prim_record_minimal_symbol (actual_name, pdi->lowpc + baseaddr,
	     mst_text, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_BLOCK,
				      &objfile->global_psymbols,
				      0, pdi->lowpc + baseaddr,
				      cu->language, objfile);
	}
      else
	{
	  /*prim_record_minimal_symbol (actual_name, pdi->lowpc + baseaddr,
	     mst_file_text, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_BLOCK,
				      &objfile->static_psymbols,
				      0, pdi->lowpc + baseaddr,
				      cu->language, objfile);
	}
      break;
    case DW_TAG_variable:
      if (pdi->is_external)
	{
	  /* Global Variable.
	     Don't enter into the minimal symbol tables as there is
	     a minimal symbol table entry from the ELF symbols already.
	     Enter into partial symbol table if it has a location
	     descriptor or a type.
	     If the location descriptor is missing, new_symbol will create
	     a LOC_UNRESOLVED symbol, the address of the variable will then
	     be determined from the minimal symbol table whenever the variable
	     is referenced.
	     The address for the partial symbol table entry is not
	     used by GDB, but it comes in handy for debugging partial symbol
	     table building.  */

	  if (pdi->locdesc)
	    addr = decode_locdesc (pdi->locdesc, cu);
	  if (pdi->locdesc || pdi->has_type)
	    psym = add_psymbol_to_list (actual_name, strlen (actual_name),
					VAR_DOMAIN, LOC_STATIC,
					&objfile->global_psymbols,
					0, addr + baseaddr,
					cu->language, objfile);
	}
      else
	{
	  /* Static Variable. Skip symbols without location descriptors.  */
	  if (pdi->locdesc == NULL)
	    {
	      if (built_actual_name)
		xfree (actual_name);
	      return;
	    }
	  addr = decode_locdesc (pdi->locdesc, cu);
	  /*prim_record_minimal_symbol (actual_name, addr + baseaddr,
	     mst_file_data, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_STATIC,
				      &objfile->static_psymbols,
				      0, addr + baseaddr,
				      cu->language, objfile);
	}
      break;
    case DW_TAG_typedef:
    case DW_TAG_base_type:
    case DW_TAG_subrange_type:
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_TYPEDEF,
			   &objfile->static_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    case DW_TAG_namespace:
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_TYPEDEF,
			   &objfile->global_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
      /* Skip external references.  The DWARF standard says in the section
         about "Structure, Union, and Class Type Entries": "An incomplete
         structure, union or class type is represented by a structure,
         union or class entry that does not have a byte size attribute
         and that has a DW_AT_declaration attribute."  */
      if (!pdi->has_byte_size && pdi->is_declaration)
	{
	  if (built_actual_name)
	    xfree (actual_name);
	  return;
	}

      /* NOTE: carlton/2003-10-07: See comment in new_symbol about
	 static vs. global.  */
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   STRUCT_DOMAIN, LOC_TYPEDEF,
			   (cu->language == language_cplus
			    || cu->language == language_java)
			   ? &objfile->global_psymbols
			   : &objfile->static_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);

      if (cu->language == language_cplus
          || cu->language == language_java
          || cu->language == language_ada)
	{
	  /* For C++ and Java, these implicitly act as typedefs as well. */
	  add_psymbol_to_list (actual_name, strlen (actual_name),
			       VAR_DOMAIN, LOC_TYPEDEF,
			       &objfile->global_psymbols,
			       0, (CORE_ADDR) 0, cu->language, objfile);
	}
      break;
    case DW_TAG_enumerator:
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_CONST,
			   (cu->language == language_cplus
			    || cu->language == language_java)
			   ? &objfile->global_psymbols
			   : &objfile->static_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    default:
      break;
    }

  /* Check to see if we should scan the name for possible namespace
     info.  Only do this if this is C++, if we don't have namespace
     debugging info in the file, if the psym is of an appropriate type
     (otherwise we'll have psym == NULL), and if we actually had a
     mangled name to begin with.  */

  /* FIXME drow/2004-02-22: Why don't we do this for classes, i.e. the
     cases which do not set PSYM above?  */

  if (cu->language == language_cplus
      && cu->has_namespace_info == 0
      && psym != NULL
      && SYMBOL_CPLUS_DEMANGLED_NAME (psym) != NULL)
    cp_check_possible_namespace_symbols (SYMBOL_CPLUS_DEMANGLED_NAME (psym),
					 objfile);

  if (built_actual_name)
    xfree (actual_name);
}

/* Determine whether a die of type TAG living in a C++ class or
   namespace needs to have the name of the scope prepended to the
   name listed in the die.  */

static int
pdi_needs_namespace (enum dwarf_tag tag)
{
  switch (tag)
    {
    case DW_TAG_namespace:
    case DW_TAG_typedef:
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_enumerator:
      return 1;
    default:
      return 0;
    }
}

/* Read a partial die corresponding to a namespace; also, add a symbol
   corresponding to that namespace to the symbol table.  NAMESPACE is
   the name of the enclosing namespace.  */

static void
add_partial_namespace (struct partial_die_info *pdi,
		       CORE_ADDR *lowpc, CORE_ADDR *highpc,
		       struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;

  /* Add a symbol for the namespace.  */

  add_partial_symbol (pdi, cu);

  /* Now scan partial symbols in that namespace.  */

  if (pdi->has_children)
    scan_partial_symbols (pdi->die_child, lowpc, highpc, cu);
}

/* See if we can figure out if the class lives in a namespace.  We do
   this by looking for a member function; its demangled name will
   contain namespace info, if there is any.  */

static void
guess_structure_name (struct partial_die_info *struct_pdi,
		      struct dwarf2_cu *cu)
{
  if ((cu->language == language_cplus
       || cu->language == language_java)
      && cu->has_namespace_info == 0
      && struct_pdi->has_children)
    {
      /* NOTE: carlton/2003-10-07: Getting the info this way changes
	 what template types look like, because the demangler
	 frequently doesn't give the same name as the debug info.  We
	 could fix this by only using the demangled name to get the
	 prefix (but see comment in read_structure_type).  */

      struct partial_die_info *child_pdi = struct_pdi->die_child;
      struct partial_die_info *real_pdi;

      /* If this DIE (this DIE's specification, if any) has a parent, then
	 we should not do this.  We'll prepend the parent's fully qualified
         name when we create the partial symbol.  */

      real_pdi = struct_pdi;
      while (real_pdi->has_specification)
	real_pdi = find_partial_die (real_pdi->spec_offset, cu);

      if (real_pdi->die_parent != NULL)
	return;

      while (child_pdi != NULL)
	{
	  if (child_pdi->tag == DW_TAG_subprogram)
	    {
	      char *actual_class_name
		= language_class_name_from_physname (cu->language_defn,
						     child_pdi->name);
	      if (actual_class_name != NULL)
		{
		  struct_pdi->name
		    = obsavestring (actual_class_name,
				    strlen (actual_class_name),
				    &cu->comp_unit_obstack);
		  xfree (actual_class_name);
		}
	      break;
	    }

	  child_pdi = child_pdi->die_sibling;
	}
    }
}

/* Read a partial die corresponding to an enumeration type.  */

static void
add_partial_enumeration (struct partial_die_info *enum_pdi,
			 struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  bfd *abfd = objfile->obfd;
  struct partial_die_info *pdi;

  if (enum_pdi->name != NULL)
    add_partial_symbol (enum_pdi, cu);

  pdi = enum_pdi->die_child;
  while (pdi)
    {
      if (pdi->tag != DW_TAG_enumerator || pdi->name == NULL)
	complaint (&symfile_complaints, _("malformed enumerator DIE ignored"));
      else
	add_partial_symbol (pdi, cu);
      pdi = pdi->die_sibling;
    }
}

/* Read the initial uleb128 in the die at INFO_PTR in compilation unit CU.
   Return the corresponding abbrev, or NULL if the number is zero (indicating
   an empty DIE).  In either case *BYTES_READ will be set to the length of
   the initial number.  */

static struct abbrev_info *
peek_die_abbrev (gdb_byte *info_ptr, unsigned int *bytes_read,
		 struct dwarf2_cu *cu)
{
  bfd *abfd = cu->objfile->obfd;
  unsigned int abbrev_number;
  struct abbrev_info *abbrev;

  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, bytes_read);

  if (abbrev_number == 0)
    return NULL;

  abbrev = dwarf2_lookup_abbrev (abbrev_number, cu);
  if (!abbrev)
    {
      error (_("Dwarf Error: Could not find abbrev number %d [in module %s]"), abbrev_number,
		      bfd_get_filename (abfd));
    }

  return abbrev;
}

/* Scan the debug information for CU starting at INFO_PTR.  Returns a
   pointer to the end of a series of DIEs, terminated by an empty
   DIE.  Any children of the skipped DIEs will also be skipped.  */

static gdb_byte *
skip_children (gdb_byte *info_ptr, struct dwarf2_cu *cu)
{
  struct abbrev_info *abbrev;
  unsigned int bytes_read;

  while (1)
    {
      abbrev = peek_die_abbrev (info_ptr, &bytes_read, cu);
      if (abbrev == NULL)
	return info_ptr + bytes_read;
      else
	info_ptr = skip_one_die (info_ptr + bytes_read, abbrev, cu);
    }
}

/* Scan the debug information for CU starting at INFO_PTR.  INFO_PTR
   should point just after the initial uleb128 of a DIE, and the
   abbrev corresponding to that skipped uleb128 should be passed in
   ABBREV.  Returns a pointer to this DIE's sibling, skipping any
   children.  */

static gdb_byte *
skip_one_die (gdb_byte *info_ptr, struct abbrev_info *abbrev,
	      struct dwarf2_cu *cu)
{
  unsigned int bytes_read;
  struct attribute attr;
  bfd *abfd = cu->objfile->obfd;
  unsigned int form, i;

  for (i = 0; i < abbrev->num_attrs; i++)
    {
      /* The only abbrev we care about is DW_AT_sibling.  */
      if (abbrev->attrs[i].name == DW_AT_sibling)
	{
	  read_attribute (&attr, &abbrev->attrs[i],
			  abfd, info_ptr, cu);
	  if (attr.form == DW_FORM_ref_addr)
	    complaint (&symfile_complaints, _("ignoring absolute DW_AT_sibling"));
	  else
	    return dwarf2_per_objfile->info_buffer
	      + dwarf2_get_ref_die_offset (&attr, cu);
	}

      /* If it isn't DW_AT_sibling, skip this attribute.  */
      form = abbrev->attrs[i].form;
    skip_attribute:
      switch (form)
	{
	case DW_FORM_addr:
	case DW_FORM_ref_addr:
	  info_ptr += cu->header.addr_size;
	  break;
	case DW_FORM_data1:
	case DW_FORM_ref1:
	case DW_FORM_flag:
	  info_ptr += 1;
	  break;
	case DW_FORM_data2:
	case DW_FORM_ref2:
	  info_ptr += 2;
	  break;
	case DW_FORM_data4:
	case DW_FORM_ref4:
	  info_ptr += 4;
	  break;
	case DW_FORM_data8:
	case DW_FORM_ref8:
	  info_ptr += 8;
	  break;
	case DW_FORM_string:
	  read_string (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  break;
	case DW_FORM_strp:
	  info_ptr += cu->header.offset_size;
	  break;
	case DW_FORM_block:
	  info_ptr += read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  break;
	case DW_FORM_block1:
	  info_ptr += 1 + read_1_byte (abfd, info_ptr);
	  break;
	case DW_FORM_block2:
	  info_ptr += 2 + read_2_bytes (abfd, info_ptr);
	  break;
	case DW_FORM_block4:
	  info_ptr += 4 + read_4_bytes (abfd, info_ptr);
	  break;
	case DW_FORM_sdata:
	case DW_FORM_udata:
	case DW_FORM_ref_udata:
	  info_ptr = skip_leb128 (abfd, info_ptr);
	  break;
	case DW_FORM_indirect:
	  form = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  /* We need to continue parsing from here, so just go back to
	     the top.  */
	  goto skip_attribute;

	default:
	  error (_("Dwarf Error: Cannot handle %s in DWARF reader [in module %s]"),
		 dwarf_form_name (form),
		 bfd_get_filename (abfd));
	}
    }

  if (abbrev->has_children)
    return skip_children (info_ptr, cu);
  else
    return info_ptr;
}

/* Locate ORIG_PDI's sibling; INFO_PTR should point to the start of
   the next DIE after ORIG_PDI.  */

static gdb_byte *
locate_pdi_sibling (struct partial_die_info *orig_pdi, gdb_byte *info_ptr,
		    bfd *abfd, struct dwarf2_cu *cu)
{
  /* Do we know the sibling already?  */

  if (orig_pdi->sibling)
    return orig_pdi->sibling;

  /* Are there any children to deal with?  */

  if (!orig_pdi->has_children)
    return info_ptr;

  /* Skip the children the long way.  */

  return skip_children (info_ptr, cu);
}

/* Expand this partial symbol table into a full symbol table.  */

static void
dwarf2_psymtab_to_symtab (struct partial_symtab *pst)
{
  /* FIXME: This is barely more than a stub.  */
  if (pst != NULL)
    {
      if (pst->readin)
	{
	  warning (_("bug: psymtab for %s is already read in."), pst->filename);
	}
      else
	{
	  if (info_verbose)
	    {
	      printf_filtered (_("Reading in symbols for %s..."), pst->filename);
	      gdb_flush (gdb_stdout);
	    }

	  /* Restore our global data.  */
	  dwarf2_per_objfile = objfile_data (pst->objfile,
					     dwarf2_objfile_data_key);

	  psymtab_to_symtab_1 (pst);

	  /* Finish up the debug error message.  */
	  if (info_verbose)
	    printf_filtered (_("done.\n"));
	}
    }
}

/* Add PER_CU to the queue.  */

static void
queue_comp_unit (struct dwarf2_per_cu_data *per_cu)
{
  struct dwarf2_queue_item *item;

  per_cu->queued = 1;
  item = xmalloc (sizeof (*item));
  item->per_cu = per_cu;
  item->next = NULL;

  if (dwarf2_queue == NULL)
    dwarf2_queue = item;
  else
    dwarf2_queue_tail->next = item;

  dwarf2_queue_tail = item;
}

/* Process the queue.  */

static void
process_queue (struct objfile *objfile)
{
  struct dwarf2_queue_item *item, *next_item;

  /* Initially, there is just one item on the queue.  Load its DIEs,
     and the DIEs of any other compilation units it requires,
     transitively.  */

  for (item = dwarf2_queue; item != NULL; item = item->next)
    {
      /* Read in this compilation unit.  This may add new items to
	 the end of the queue.  */
      load_full_comp_unit (item->per_cu, objfile);

      item->per_cu->cu->read_in_chain = dwarf2_per_objfile->read_in_chain;
      dwarf2_per_objfile->read_in_chain = item->per_cu;

      /* If this compilation unit has already had full symbols created,
	 reset the TYPE fields in each DIE.  */
      if (item->per_cu->type_hash)
	reset_die_and_siblings_types (item->per_cu->cu->dies,
				      item->per_cu->cu);
    }

  /* Now everything left on the queue needs to be read in.  Process
     them, one at a time, removing from the queue as we finish.  */
  for (item = dwarf2_queue; item != NULL; dwarf2_queue = item = next_item)
    {
      if (item->per_cu->psymtab && !item->per_cu->psymtab->readin)
	process_full_comp_unit (item->per_cu);

      item->per_cu->queued = 0;
      next_item = item->next;
      xfree (item);
    }

  dwarf2_queue_tail = NULL;
}

/* Free all allocated queue entries.  This function only releases anything if
   an error was thrown; if the queue was processed then it would have been
   freed as we went along.  */

static void
dwarf2_release_queue (void *dummy)
{
  struct dwarf2_queue_item *item, *last;

  item = dwarf2_queue;
  while (item)
    {
      /* Anything still marked queued is likely to be in an
	 inconsistent state, so discard it.  */
      if (item->per_cu->queued)
	{
	  if (item->per_cu->cu != NULL)
	    free_one_cached_comp_unit (item->per_cu->cu);
	  item->per_cu->queued = 0;
	}

      last = item;
      item = item->next;
      xfree (last);
    }

  dwarf2_queue = dwarf2_queue_tail = NULL;
}

/* Read in full symbols for PST, and anything it depends on.  */

static void
psymtab_to_symtab_1 (struct partial_symtab *pst)
{
  struct dwarf2_per_cu_data *per_cu;
  struct cleanup *back_to;
  int i;

  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
        /* Inform about additional files that need to be read in.  */
        if (info_verbose)
          {
	    /* FIXME: i18n: Need to make this a single string.  */
            fputs_filtered (" ", gdb_stdout);
            wrap_here ("");
            fputs_filtered ("and ", gdb_stdout);
            wrap_here ("");
            printf_filtered ("%s...", pst->dependencies[i]->filename);
            wrap_here ("");     /* Flush output */
            gdb_flush (gdb_stdout);
          }
        psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  per_cu = (struct dwarf2_per_cu_data *) pst->read_symtab_private;

  if (per_cu == NULL)
    {
      /* It's an include file, no symbols to read for it.
         Everything is in the parent symtab.  */
      pst->readin = 1;
      return;
    }

  back_to = make_cleanup (dwarf2_release_queue, NULL);

  queue_comp_unit (per_cu);

  process_queue (pst->objfile);

  /* Age the cache, releasing compilation units that have not
     been used recently.  */
  age_cached_comp_units ();

  do_cleanups (back_to);
}

/* Load the DIEs associated with PST and PER_CU into memory.  */

static struct dwarf2_cu *
load_full_comp_unit (struct dwarf2_per_cu_data *per_cu, struct objfile *objfile)
{
  bfd *abfd = objfile->obfd;
  struct dwarf2_cu *cu;
  unsigned long offset;
  gdb_byte *info_ptr;
  struct cleanup *back_to, *free_cu_cleanup;
  struct attribute *attr;
  CORE_ADDR baseaddr;

  /* Set local variables from the partial symbol table info.  */
  offset = per_cu->offset;

  info_ptr = dwarf2_per_objfile->info_buffer + offset;

  cu = xmalloc (sizeof (struct dwarf2_cu));
  memset (cu, 0, sizeof (struct dwarf2_cu));

  /* If an error occurs while loading, release our storage.  */
  free_cu_cleanup = make_cleanup (free_one_comp_unit, cu);

  cu->objfile = objfile;

  /* read in the comp_unit header  */
  info_ptr = read_comp_unit_head (&cu->header, info_ptr, abfd);

  /* Read the abbrevs for this compilation unit  */
  dwarf2_read_abbrevs (abfd, cu);
  back_to = make_cleanup (dwarf2_free_abbrev_table, cu);

  cu->header.offset = offset;

  cu->per_cu = per_cu;
  per_cu->cu = cu;

  /* We use this obstack for block values in dwarf_alloc_block.  */
  obstack_init (&cu->comp_unit_obstack);

  cu->dies = read_comp_unit (info_ptr, abfd, cu);

  /* We try not to read any attributes in this function, because not
     all objfiles needed for references have been loaded yet, and symbol
     table processing isn't initialized.  But we have to set the CU language,
     or we won't be able to build types correctly.  */
  attr = dwarf2_attr (cu->dies, DW_AT_language, cu);
  if (attr)
    set_cu_language (DW_UNSND (attr), cu);
  else
    set_cu_language (language_minimal, cu);

  do_cleanups (back_to);

  /* We've successfully allocated this compilation unit.  Let our caller
     clean it up when finished with it.  */
  discard_cleanups (free_cu_cleanup);

  return cu;
}

/* Generate full symbol information for PST and CU, whose DIEs have
   already been loaded into memory.  */

static void
process_full_comp_unit (struct dwarf2_per_cu_data *per_cu)
{
  struct partial_symtab *pst = per_cu->psymtab;
  struct dwarf2_cu *cu = per_cu->cu;
  struct objfile *objfile = pst->objfile;
  bfd *abfd = objfile->obfd;
  CORE_ADDR lowpc, highpc;
  struct symtab *symtab;
  struct cleanup *back_to;
  struct attribute *attr;
  CORE_ADDR baseaddr;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  /* We're in the global namespace.  */
  processing_current_prefix = "";

  buildsym_init ();
  back_to = make_cleanup (really_free_pendings, NULL);

  cu->list_in_scope = &file_symbols;

  /* Find the base address of the compilation unit for range lists and
     location lists.  It will normally be specified by DW_AT_low_pc.
     In DWARF-3 draft 4, the base address could be overridden by
     DW_AT_entry_pc.  It's been removed, but GCC still uses this for
     compilation units with discontinuous ranges.  */

  cu->header.base_known = 0;
  cu->header.base_address = 0;

  attr = dwarf2_attr (cu->dies, DW_AT_entry_pc, cu);
  if (attr)
    {
      cu->header.base_address = DW_ADDR (attr);
      cu->header.base_known = 1;
    }
  else
    {
      attr = dwarf2_attr (cu->dies, DW_AT_low_pc, cu);
      if (attr)
	{
	  cu->header.base_address = DW_ADDR (attr);
	  cu->header.base_known = 1;
	}
    }

  /* Do line number decoding in read_file_scope () */
  process_die (cu->dies, cu);

  /* Some compilers don't define a DW_AT_high_pc attribute for the
     compilation unit.  If the DW_AT_high_pc is missing, synthesize
     it, by scanning the DIE's below the compilation unit.  */
  get_scope_pc_bounds (cu->dies, &lowpc, &highpc, cu);

  symtab = end_symtab (highpc + baseaddr, objfile, SECT_OFF_TEXT (objfile));

  /* Set symtab language to language from DW_AT_language.
     If the compilation is from a C file generated by language preprocessors,
     do not set the language if it was already deduced by start_subfile.  */
  if (symtab != NULL
      && !(cu->language == language_c && symtab->language != language_c))
    {
      symtab->language = cu->language;
    }
  pst->symtab = symtab;
  pst->readin = 1;

  do_cleanups (back_to);
}

/* Process a die and its children.  */

static void
process_die (struct die_info *die, struct dwarf2_cu *cu)
{
  switch (die->tag)
    {
    case DW_TAG_padding:
      break;
    case DW_TAG_compile_unit:
      read_file_scope (die, cu);
      break;
    case DW_TAG_subprogram:
      read_subroutine_type (die, cu);
      read_func_scope (die, cu);
      break;
    case DW_TAG_inlined_subroutine:
      /* FIXME:  These are ignored for now.
         They could be used to set breakpoints on all inlined instances
         of a function and make GDB `next' properly over inlined functions.  */
      break;
    case DW_TAG_lexical_block:
    case DW_TAG_try_block:
    case DW_TAG_catch_block:
      read_lexical_block_scope (die, cu);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_type (die, cu);
      process_structure_scope (die, cu);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration_type (die, cu);
      process_enumeration_scope (die, cu);
      break;

    /* FIXME drow/2004-03-14: These initialize die->type, but do not create
       a symbol or process any children.  Therefore it doesn't do anything
       that won't be done on-demand by read_type_die.  */
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, cu);
      break;
    case DW_TAG_set_type:
      read_set_type (die, cu);
      break;
    case DW_TAG_array_type:
      read_array_type (die, cu);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, cu);
      break;
    case DW_TAG_ptr_to_member_type:
      read_tag_ptr_to_member_type (die, cu);
      break;
    case DW_TAG_reference_type:
      read_tag_reference_type (die, cu);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, cu);
      break;
    /* END FIXME */

    case DW_TAG_base_type:
      read_base_type (die, cu);
      /* Add a typedef symbol for the type definition, if it has a
	 DW_AT_name.  */
      new_symbol (die, die->type, cu);
      break;
    case DW_TAG_subrange_type:
      read_subrange_type (die, cu);
      /* Add a typedef symbol for the type definition, if it has a
         DW_AT_name.  */
      new_symbol (die, die->type, cu);
      break;
    case DW_TAG_common_block:
      read_common_block (die, cu);
      break;
    case DW_TAG_common_inclusion:
      break;
    case DW_TAG_namespace:
      processing_has_namespace_info = 1;
      read_namespace (die, cu);
      break;
    case DW_TAG_imported_declaration:
    case DW_TAG_imported_module:
      /* FIXME: carlton/2002-10-16: Eventually, we should use the
	 information contained in these.  DW_TAG_imported_declaration
	 dies shouldn't have children; DW_TAG_imported_module dies
	 shouldn't in the C++ case, but conceivably could in the
	 Fortran case, so we'll have to replace this gdb_assert if
	 Fortran compilers start generating that info.  */
      processing_has_namespace_info = 1;
      gdb_assert (die->child == NULL);
      break;
    default:
      new_symbol (die, NULL, cu);
      break;
    }
}

static void
initialize_cu_func_list (struct dwarf2_cu *cu)
{
  cu->first_fn = cu->last_fn = cu->cached_fn = NULL;
}

static void
free_cu_line_header (void *arg)
{
  struct dwarf2_cu *cu = arg;

  free_line_header (cu->line_header);
  cu->line_header = NULL;
}

static void
read_file_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;
  struct cleanup *back_to = make_cleanup (null_cleanup, 0);
  CORE_ADDR lowpc = ((CORE_ADDR) -1);
  CORE_ADDR highpc = ((CORE_ADDR) 0);
  struct attribute *attr;
  char *name = NULL;
  char *comp_dir = NULL;
  struct die_info *child_die;
  bfd *abfd = objfile->obfd;
  struct line_header *line_header = 0;
  CORE_ADDR baseaddr;
  
  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  get_scope_pc_bounds (die, &lowpc, &highpc, cu);

  /* If we didn't find a lowpc, set it to highpc to avoid complaints
     from finish_block.  */
  if (lowpc == ((CORE_ADDR) -1))
    lowpc = highpc;
  lowpc += baseaddr;
  highpc += baseaddr;

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr)
    {
      name = DW_STRING (attr);
    }

  attr = dwarf2_attr (die, DW_AT_comp_dir, cu);
  if (attr)
    comp_dir = DW_STRING (attr);
  else if (name != NULL && IS_ABSOLUTE_PATH (name))
    {
      comp_dir = ldirname (name);
      if (comp_dir != NULL)
	make_cleanup (xfree, comp_dir);
    }
  if (comp_dir != NULL)
    {
      /* Irix 6.2 native cc prepends <machine>.: to the compilation
	 directory, get rid of it.  */
      char *cp = strchr (comp_dir, ':');

      if (cp && cp != comp_dir && cp[-1] == '.' && cp[1] == '/')
	comp_dir = cp + 1;
    }

  if (name == NULL)
    name = "<unknown>";

  attr = dwarf2_attr (die, DW_AT_language, cu);
  if (attr)
    {
      set_cu_language (DW_UNSND (attr), cu);
    }

  attr = dwarf2_attr (die, DW_AT_producer, cu);
  if (attr) 
    cu->producer = DW_STRING (attr);

  /* We assume that we're processing GCC output. */
  processing_gcc_compilation = 2;

  /* The compilation unit may be in a different language or objfile,
     zero out all remembered fundamental types.  */
  memset (cu->ftypes, 0, FT_NUM_MEMBERS * sizeof (struct type *));

  start_symtab (name, comp_dir, lowpc);
  record_debugformat ("DWARF 2");
  record_producer (cu->producer);

  initialize_cu_func_list (cu);

  /* Decode line number information if present.  We do this before
     processing child DIEs, so that the line header table is available
     for DW_AT_decl_file.  */
  attr = dwarf2_attr (die, DW_AT_stmt_list, cu);
  if (attr)
    {
      unsigned int line_offset = DW_UNSND (attr);
      line_header = dwarf_decode_line_header (line_offset, abfd, cu);
      if (line_header)
        {
          cu->line_header = line_header;
          make_cleanup (free_cu_line_header, cu);
          dwarf_decode_lines (line_header, comp_dir, abfd, cu, NULL);
        }
    }

  /* Process all dies in compilation unit.  */
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  /* Decode macro information, if present.  Dwarf 2 macro information
     refers to information in the line number info statement program
     header, so we can only read it if we've read the header
     successfully.  */
  attr = dwarf2_attr (die, DW_AT_macro_info, cu);
  if (attr && line_header)
    {
      unsigned int macro_offset = DW_UNSND (attr);
      dwarf_decode_macros (line_header, macro_offset,
                           comp_dir, abfd, cu);
    }
  do_cleanups (back_to);
}

static void
add_to_cu_func_list (const char *name, CORE_ADDR lowpc, CORE_ADDR highpc,
		     struct dwarf2_cu *cu)
{
  struct function_range *thisfn;

  thisfn = (struct function_range *)
    obstack_alloc (&cu->comp_unit_obstack, sizeof (struct function_range));
  thisfn->name = name;
  thisfn->lowpc = lowpc;
  thisfn->highpc = highpc;
  thisfn->seen_line = 0;
  thisfn->next = NULL;

  if (cu->last_fn == NULL)
      cu->first_fn = thisfn;
  else
      cu->last_fn->next = thisfn;

  cu->last_fn = thisfn;
}

static void
read_func_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct context_stack *new;
  CORE_ADDR lowpc;
  CORE_ADDR highpc;
  struct die_info *child_die;
  struct attribute *attr;
  char *name;
  const char *previous_prefix = processing_current_prefix;
  struct cleanup *back_to = NULL;
  CORE_ADDR baseaddr;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  name = dwarf2_linkage_name (die, cu);

  /* Ignore functions with missing or empty names and functions with
     missing or invalid low and high pc attributes.  */
  if (name == NULL || !dwarf2_get_pc_bounds (die, &lowpc, &highpc, cu))
    return;

  if (cu->language == language_cplus
      || cu->language == language_java)
    {
      struct die_info *spec_die = die_specification (die, cu);

      /* NOTE: carlton/2004-01-23: We have to be careful in the
         presence of DW_AT_specification.  For example, with GCC 3.4,
         given the code

           namespace N {
             void foo() {
               // Definition of N::foo.
             }
           }

         then we'll have a tree of DIEs like this:

         1: DW_TAG_compile_unit
           2: DW_TAG_namespace        // N
             3: DW_TAG_subprogram     // declaration of N::foo
           4: DW_TAG_subprogram       // definition of N::foo
                DW_AT_specification   // refers to die #3

         Thus, when processing die #4, we have to pretend that we're
         in the context of its DW_AT_specification, namely the contex
         of die #3.  */
	
      if (spec_die != NULL)
	{
	  char *specification_prefix = determine_prefix (spec_die, cu);
	  processing_current_prefix = specification_prefix;
	  back_to = make_cleanup (xfree, specification_prefix);
	}
    }

  lowpc += baseaddr;
  highpc += baseaddr;

  /* Record the function range for dwarf_decode_lines.  */
  add_to_cu_func_list (name, lowpc, highpc, cu);

  new = push_context (0, lowpc);
  new->name = new_symbol (die, die->type, cu);

  /* If there is a location expression for DW_AT_frame_base, record
     it.  */
  attr = dwarf2_attr (die, DW_AT_frame_base, cu);
  if (attr)
    /* FIXME: cagney/2004-01-26: The DW_AT_frame_base's location
       expression is being recorded directly in the function's symbol
       and not in a separate frame-base object.  I guess this hack is
       to avoid adding some sort of frame-base adjunct/annex to the
       function's symbol :-(.  The problem with doing this is that it
       results in a function symbol with a location expression that
       has nothing to do with the location of the function, ouch!  The
       relationship should be: a function's symbol has-a frame base; a
       frame-base has-a location expression.  */
    dwarf2_symbol_mark_computed (attr, new->name, cu);

  cu->list_in_scope = &local_symbols;

  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  new = pop_context ();
  /* Make a block for the local symbols within.  */
  finish_block (new->name, &local_symbols, new->old_blocks,
		lowpc, highpc, objfile);
  
  /* In C++, we can have functions nested inside functions (e.g., when
     a function declares a class that has methods).  This means that
     when we finish processing a function scope, we may need to go
     back to building a containing block's symbol lists.  */
  local_symbols = new->locals;
  param_symbols = new->params;

  /* If we've finished processing a top-level function, subsequent
     symbols go in the file symbol list.  */
  if (outermost_context_p ())
    cu->list_in_scope = &file_symbols;

  processing_current_prefix = previous_prefix;
  if (back_to != NULL)
    do_cleanups (back_to);
}

/* Process all the DIES contained within a lexical block scope.  Start
   a new scope, process the dies, and then close the scope.  */

static void
read_lexical_block_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct context_stack *new;
  CORE_ADDR lowpc, highpc;
  struct die_info *child_die;
  CORE_ADDR baseaddr;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  /* Ignore blocks with missing or invalid low and high pc attributes.  */
  /* ??? Perhaps consider discontiguous blocks defined by DW_AT_ranges
     as multiple lexical blocks?  Handling children in a sane way would
     be nasty.  Might be easier to properly extend generic blocks to 
     describe ranges.  */
  if (!dwarf2_get_pc_bounds (die, &lowpc, &highpc, cu))
    return;
  lowpc += baseaddr;
  highpc += baseaddr;

  push_context (0, lowpc);
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }
  new = pop_context ();

  if (local_symbols != NULL)
    {
      finish_block (0, &local_symbols, new->old_blocks, new->start_addr,
		    highpc, objfile);
    }
  local_symbols = new->locals;
}

/* Get low and high pc attributes from a die.  Return 1 if the attributes
   are present and valid, otherwise, return 0.  Return -1 if the range is
   discontinuous, i.e. derived from DW_AT_ranges information.  */
static int
dwarf2_get_pc_bounds (struct die_info *die, CORE_ADDR *lowpc,
		      CORE_ADDR *highpc, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;
  struct attribute *attr;
  bfd *obfd = objfile->obfd;
  CORE_ADDR low = 0;
  CORE_ADDR high = 0;
  int ret = 0;

  attr = dwarf2_attr (die, DW_AT_high_pc, cu);
  if (attr)
    {
      high = DW_ADDR (attr);
      attr = dwarf2_attr (die, DW_AT_low_pc, cu);
      if (attr)
	low = DW_ADDR (attr);
      else
	/* Found high w/o low attribute.  */
	return 0;

      /* Found consecutive range of addresses.  */
      ret = 1;
    }
  else
    {
      attr = dwarf2_attr (die, DW_AT_ranges, cu);
      if (attr != NULL)
	{
	  unsigned int addr_size = cu_header->addr_size;
	  CORE_ADDR mask = ~(~(CORE_ADDR)1 << (addr_size * 8 - 1));
	  /* Value of the DW_AT_ranges attribute is the offset in the
	     .debug_ranges section.  */
	  unsigned int offset = DW_UNSND (attr);
	  /* Base address selection entry.  */
	  CORE_ADDR base;
	  int found_base;
	  unsigned int dummy;
	  gdb_byte *buffer;
	  CORE_ADDR marker;
	  int low_set;
 
	  found_base = cu_header->base_known;
	  base = cu_header->base_address;

	  if (offset >= dwarf2_per_objfile->ranges_size)
	    {
	      complaint (&symfile_complaints,
	                 _("Offset %d out of bounds for DW_AT_ranges attribute"),
			 offset);
	      return 0;
	    }
	  buffer = dwarf2_per_objfile->ranges_buffer + offset;

	  /* Read in the largest possible address.  */
	  marker = read_address (obfd, buffer, cu, &dummy);
	  if ((marker & mask) == mask)
	    {
	      /* If we found the largest possible address, then
		 read the base address.  */
	      base = read_address (obfd, buffer + addr_size, cu, &dummy);
	      buffer += 2 * addr_size;
	      offset += 2 * addr_size;
	      found_base = 1;
	    }

	  low_set = 0;

	  while (1)
	    {
	      CORE_ADDR range_beginning, range_end;

	      range_beginning = read_address (obfd, buffer, cu, &dummy);
	      buffer += addr_size;
	      range_end = read_address (obfd, buffer, cu, &dummy);
	      buffer += addr_size;
	      offset += 2 * addr_size;

	      /* An end of list marker is a pair of zero addresses.  */
	      if (range_beginning == 0 && range_end == 0)
		/* Found the end of list entry.  */
		break;

	      /* Each base address selection entry is a pair of 2 values.
		 The first is the largest possible address, the second is
		 the base address.  Check for a base address here.  */
	      if ((range_beginning & mask) == mask)
		{
		  /* If we found the largest possible address, then
		     read the base address.  */
		  base = read_address (obfd, buffer + addr_size, cu, &dummy);
		  found_base = 1;
		  continue;
		}

	      if (!found_base)
		{
		  /* We have no valid base address for the ranges
		     data.  */
		  complaint (&symfile_complaints,
			     _("Invalid .debug_ranges data (no base address)"));
		  return 0;
		}

	      range_beginning += base;
	      range_end += base;

	      /* FIXME: This is recording everything as a low-high
		 segment of consecutive addresses.  We should have a
		 data structure for discontiguous block ranges
		 instead.  */
	      if (! low_set)
		{
		  low = range_beginning;
		  high = range_end;
		  low_set = 1;
		}
	      else
		{
		  if (range_beginning < low)
		    low = range_beginning;
		  if (range_end > high)
		    high = range_end;
		}
	    }

	  if (! low_set)
	    /* If the first entry is an end-of-list marker, the range
	       describes an empty scope, i.e. no instructions.  */
	    return 0;

	  ret = -1;
	}
    }

  if (high < low)
    return 0;

  /* When using the GNU linker, .gnu.linkonce. sections are used to
     eliminate duplicate copies of functions and vtables and such.
     The linker will arbitrarily choose one and discard the others.
     The AT_*_pc values for such functions refer to local labels in
     these sections.  If the section from that file was discarded, the
     labels are not in the output, so the relocs get a value of 0.
     If this is a discarded function, mark the pc bounds as invalid,
     so that GDB will ignore it.  */
  if (low == 0 && !dwarf2_per_objfile->has_section_at_zero)
    return 0;

  *lowpc = low;
  *highpc = high;
  return ret;
}

/* Get the low and high pc's represented by the scope DIE, and store
   them in *LOWPC and *HIGHPC.  If the correct values can't be
   determined, set *LOWPC to -1 and *HIGHPC to 0.  */

static void
get_scope_pc_bounds (struct die_info *die,
		     CORE_ADDR *lowpc, CORE_ADDR *highpc,
		     struct dwarf2_cu *cu)
{
  CORE_ADDR best_low = (CORE_ADDR) -1;
  CORE_ADDR best_high = (CORE_ADDR) 0;
  CORE_ADDR current_low, current_high;

  if (dwarf2_get_pc_bounds (die, &current_low, &current_high, cu))
    {
      best_low = current_low;
      best_high = current_high;
    }
  else
    {
      struct die_info *child = die->child;

      while (child && child->tag)
	{
	  switch (child->tag) {
	  case DW_TAG_subprogram:
	    if (dwarf2_get_pc_bounds (child, &current_low, &current_high, cu))
	      {
		best_low = min (best_low, current_low);
		best_high = max (best_high, current_high);
	      }
	    break;
	  case DW_TAG_namespace:
	    /* FIXME: carlton/2004-01-16: Should we do this for
	       DW_TAG_class_type/DW_TAG_structure_type, too?  I think
	       that current GCC's always emit the DIEs corresponding
	       to definitions of methods of classes as children of a
	       DW_TAG_compile_unit or DW_TAG_namespace (as opposed to
	       the DIEs giving the declarations, which could be
	       anywhere).  But I don't see any reason why the
	       standards says that they have to be there.  */
	    get_scope_pc_bounds (child, &current_low, &current_high, cu);

	    if (current_low != ((CORE_ADDR) -1))
	      {
		best_low = min (best_low, current_low);
		best_high = max (best_high, current_high);
	      }
	    break;
	  default:
	    /* Ignore. */
	    break;
	  }

	  child = sibling_die (child);
	}
    }

  *lowpc = best_low;
  *highpc = best_high;
}

/* Add an aggregate field to the field list.  */

static void
dwarf2_add_field (struct field_info *fip, struct die_info *die,
		  struct dwarf2_cu *cu)
{ 
  struct objfile *objfile = cu->objfile;
  struct nextfield *new_field;
  struct attribute *attr;
  struct field *fp;
  char *fieldname = "";

  /* Allocate a new field list entry and link it in.  */
  new_field = (struct nextfield *) xmalloc (sizeof (struct nextfield));
  make_cleanup (xfree, new_field);
  memset (new_field, 0, sizeof (struct nextfield));
  new_field->next = fip->fields;
  fip->fields = new_field;
  fip->nfields++;

  /* Handle accessibility and virtuality of field.
     The default accessibility for members is public, the default
     accessibility for inheritance is private.  */
  if (die->tag != DW_TAG_inheritance)
    new_field->accessibility = DW_ACCESS_public;
  else
    new_field->accessibility = DW_ACCESS_private;
  new_field->virtuality = DW_VIRTUALITY_none;

  attr = dwarf2_attr (die, DW_AT_accessibility, cu);
  if (attr)
    new_field->accessibility = DW_UNSND (attr);
  if (new_field->accessibility != DW_ACCESS_public)
    fip->non_public_fields = 1;
  attr = dwarf2_attr (die, DW_AT_virtuality, cu);
  if (attr)
    new_field->virtuality = DW_UNSND (attr);

  fp = &new_field->field;

  if (die->tag == DW_TAG_member && ! die_is_declaration (die, cu))
    {
      /* Data member other than a C++ static data member.  */
      
      /* Get type of field.  */
      fp->type = die_type (die, cu);

      FIELD_STATIC_KIND (*fp) = 0;

      /* Get bit size of field (zero if none).  */
      attr = dwarf2_attr (die, DW_AT_bit_size, cu);
      if (attr)
	{
	  FIELD_BITSIZE (*fp) = DW_UNSND (attr);
	}
      else
	{
	  FIELD_BITSIZE (*fp) = 0;
	}

      /* Get bit offset of field.  */
      attr = dwarf2_attr (die, DW_AT_data_member_location, cu);
      if (attr)
	{
	  FIELD_BITPOS (*fp) =
	    decode_locdesc (DW_BLOCK (attr), cu) * bits_per_byte;
	}
      else
	FIELD_BITPOS (*fp) = 0;
      attr = dwarf2_attr (die, DW_AT_bit_offset, cu);
      if (attr)
	{
	  if (BITS_BIG_ENDIAN)
	    {
	      /* For big endian bits, the DW_AT_bit_offset gives the
	         additional bit offset from the MSB of the containing
	         anonymous object to the MSB of the field.  We don't
	         have to do anything special since we don't need to
	         know the size of the anonymous object.  */
	      FIELD_BITPOS (*fp) += DW_UNSND (attr);
	    }
	  else
	    {
	      /* For little endian bits, compute the bit offset to the
	         MSB of the anonymous object, subtract off the number of
	         bits from the MSB of the field to the MSB of the
	         object, and then subtract off the number of bits of
	         the field itself.  The result is the bit offset of
	         the LSB of the field.  */
	      int anonymous_size;
	      int bit_offset = DW_UNSND (attr);

	      attr = dwarf2_attr (die, DW_AT_byte_size, cu);
	      if (attr)
		{
		  /* The size of the anonymous object containing
		     the bit field is explicit, so use the
		     indicated size (in bytes).  */
		  anonymous_size = DW_UNSND (attr);
		}
	      else
		{
		  /* The size of the anonymous object containing
		     the bit field must be inferred from the type
		     attribute of the data member containing the
		     bit field.  */
		  anonymous_size = TYPE_LENGTH (fp->type);
		}
	      FIELD_BITPOS (*fp) += anonymous_size * bits_per_byte
		- bit_offset - FIELD_BITSIZE (*fp);
	    }
	}

      /* Get name of field.  */
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	fieldname = DW_STRING (attr);

      /* The name is already allocated along with this objfile, so we don't
	 need to duplicate it for the type.  */
      fp->name = fieldname;

      /* Change accessibility for artificial fields (e.g. virtual table
         pointer or virtual base class pointer) to private.  */
      if (dwarf2_attr (die, DW_AT_artificial, cu))
	{
	  new_field->accessibility = DW_ACCESS_private;
	  fip->non_public_fields = 1;
	}
    }
  else if (die->tag == DW_TAG_member || die->tag == DW_TAG_variable)
    {
      /* C++ static member.  */

      /* NOTE: carlton/2002-11-05: It should be a DW_TAG_member that
	 is a declaration, but all versions of G++ as of this writing
	 (so through at least 3.2.1) incorrectly generate
	 DW_TAG_variable tags.  */
      
      char *physname;

      /* Get name of field.  */
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	fieldname = DW_STRING (attr);
      else
	return;

      /* Get physical name.  */
      physname = dwarf2_linkage_name (die, cu);

      /* The name is already allocated along with this objfile, so we don't
	 need to duplicate it for the type.  */
      SET_FIELD_PHYSNAME (*fp, physname ? physname : "");
      FIELD_TYPE (*fp) = die_type (die, cu);
      FIELD_NAME (*fp) = fieldname;
    }
  else if (die->tag == DW_TAG_inheritance)
    {
      /* C++ base class field.  */
      attr = dwarf2_attr (die, DW_AT_data_member_location, cu);
      if (attr)
	FIELD_BITPOS (*fp) = (decode_locdesc (DW_BLOCK (attr), cu)
			      * bits_per_byte);
      FIELD_BITSIZE (*fp) = 0;
      FIELD_STATIC_KIND (*fp) = 0;
      FIELD_TYPE (*fp) = die_type (die, cu);
      FIELD_NAME (*fp) = type_name_no_tag (fp->type);
      fip->nbaseclasses++;
    }
}

/* Create the vector of fields, and attach it to the type.  */

static void
dwarf2_attach_fields_to_type (struct field_info *fip, struct type *type,
			      struct dwarf2_cu *cu)
{
  int nfields = fip->nfields;

  /* Record the field count, allocate space for the array of fields,
     and create blank accessibility bitfields if necessary.  */
  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nfields);
  memset (TYPE_FIELDS (type), 0, sizeof (struct field) * nfields);

  if (fip->non_public_fields)
    {
      ALLOCATE_CPLUS_STRUCT_TYPE (type);

      TYPE_FIELD_PRIVATE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PRIVATE_BITS (type), nfields);

      TYPE_FIELD_PROTECTED_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PROTECTED_BITS (type), nfields);

      TYPE_FIELD_IGNORE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_IGNORE_BITS (type), nfields);
    }

  /* If the type has baseclasses, allocate and clear a bit vector for
     TYPE_FIELD_VIRTUAL_BITS.  */
  if (fip->nbaseclasses)
    {
      int num_bytes = B_BYTES (fip->nbaseclasses);
      unsigned char *pointer;

      ALLOCATE_CPLUS_STRUCT_TYPE (type);
      pointer = TYPE_ALLOC (type, num_bytes);
      TYPE_FIELD_VIRTUAL_BITS (type) = pointer;
      B_CLRALL (TYPE_FIELD_VIRTUAL_BITS (type), fip->nbaseclasses);
      TYPE_N_BASECLASSES (type) = fip->nbaseclasses;
    }

  /* Copy the saved-up fields into the field vector.  Start from the head
     of the list, adding to the tail of the field array, so that they end
     up in the same order in the array in which they were added to the list.  */
  while (nfields-- > 0)
    {
      TYPE_FIELD (type, nfields) = fip->fields->field;
      switch (fip->fields->accessibility)
	{
	case DW_ACCESS_private:
	  SET_TYPE_FIELD_PRIVATE (type, nfields);
	  break;

	case DW_ACCESS_protected:
	  SET_TYPE_FIELD_PROTECTED (type, nfields);
	  break;

	case DW_ACCESS_public:
	  break;

	default:
	  /* Unknown accessibility.  Complain and treat it as public.  */
	  {
	    complaint (&symfile_complaints, _("unsupported accessibility %d"),
		       fip->fields->accessibility);
	  }
	  break;
	}
      if (nfields < fip->nbaseclasses)
	{
	  switch (fip->fields->virtuality)
	    {
	    case DW_VIRTUALITY_virtual:
	    case DW_VIRTUALITY_pure_virtual:
	      SET_TYPE_FIELD_VIRTUAL (type, nfields);
	      break;
	    }
	}
      fip->fields = fip->fields->next;
    }
}

/* Add a member function to the proper fieldlist.  */

static void
dwarf2_add_member_fn (struct field_info *fip, struct die_info *die,
		      struct type *type, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct attribute *attr;
  struct fnfieldlist *flp;
  int i;
  struct fn_field *fnp;
  char *fieldname;
  char *physname;
  struct nextfnfield *new_fnfield;

  /* Get name of member function.  */
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    fieldname = DW_STRING (attr);
  else
    return;

  /* Get the mangled name.  */
  physname = dwarf2_linkage_name (die, cu);

  /* Look up member function name in fieldlist.  */
  for (i = 0; i < fip->nfnfields; i++)
    {
      if (strcmp (fip->fnfieldlists[i].name, fieldname) == 0)
	break;
    }

  /* Create new list element if necessary.  */
  if (i < fip->nfnfields)
    flp = &fip->fnfieldlists[i];
  else
    {
      if ((fip->nfnfields % DW_FIELD_ALLOC_CHUNK) == 0)
	{
	  fip->fnfieldlists = (struct fnfieldlist *)
	    xrealloc (fip->fnfieldlists,
		      (fip->nfnfields + DW_FIELD_ALLOC_CHUNK)
		      * sizeof (struct fnfieldlist));
	  if (fip->nfnfields == 0)
	    make_cleanup (free_current_contents, &fip->fnfieldlists);
	}
      flp = &fip->fnfieldlists[fip->nfnfields];
      flp->name = fieldname;
      flp->length = 0;
      flp->head = NULL;
      fip->nfnfields++;
    }

  /* Create a new member function field and chain it to the field list
     entry. */
  new_fnfield = (struct nextfnfield *) xmalloc (sizeof (struct nextfnfield));
  make_cleanup (xfree, new_fnfield);
  memset (new_fnfield, 0, sizeof (struct nextfnfield));
  new_fnfield->next = flp->head;
  flp->head = new_fnfield;
  flp->length++;

  /* Fill in the member function field info.  */
  fnp = &new_fnfield->fnfield;
  /* The name is already allocated along with this objfile, so we don't
     need to duplicate it for the type.  */
  fnp->physname = physname ? physname : "";
  fnp->type = alloc_type (objfile);
  if (die->type && TYPE_CODE (die->type) == TYPE_CODE_FUNC)
    {
      int nparams = TYPE_NFIELDS (die->type);

      /* TYPE is the domain of this method, and DIE->TYPE is the type
	   of the method itself (TYPE_CODE_METHOD).  */
      smash_to_method_type (fnp->type, type,
			    TYPE_TARGET_TYPE (die->type),
			    TYPE_FIELDS (die->type),
			    TYPE_NFIELDS (die->type),
			    TYPE_VARARGS (die->type));

      /* Handle static member functions.
         Dwarf2 has no clean way to discern C++ static and non-static
         member functions. G++ helps GDB by marking the first
         parameter for non-static member functions (which is the
         this pointer) as artificial. We obtain this information
         from read_subroutine_type via TYPE_FIELD_ARTIFICIAL.  */
      if (nparams == 0 || TYPE_FIELD_ARTIFICIAL (die->type, 0) == 0)
	fnp->voffset = VOFFSET_STATIC;
    }
  else
    complaint (&symfile_complaints, _("member function type missing for '%s'"),
	       physname);

  /* Get fcontext from DW_AT_containing_type if present.  */
  if (dwarf2_attr (die, DW_AT_containing_type, cu) != NULL)
    fnp->fcontext = die_containing_type (die, cu);

  /* dwarf2 doesn't have stubbed physical names, so the setting of is_const
     and is_volatile is irrelevant, as it is needed by gdb_mangle_name only.  */

  /* Get accessibility.  */
  attr = dwarf2_attr (die, DW_AT_accessibility, cu);
  if (attr)
    {
      switch (DW_UNSND (attr))
	{
	case DW_ACCESS_private:
	  fnp->is_private = 1;
	  break;
	case DW_ACCESS_protected:
	  fnp->is_protected = 1;
	  break;
	}
    }

  /* Check for artificial methods.  */
  attr = dwarf2_attr (die, DW_AT_artificial, cu);
  if (attr && DW_UNSND (attr) != 0)
    fnp->is_artificial = 1;

  /* Get index in virtual function table if it is a virtual member function.  */
  attr = dwarf2_attr (die, DW_AT_vtable_elem_location, cu);
  if (attr)
    {
      /* Support the .debug_loc offsets */
      if (attr_form_is_block (attr))
        {
          fnp->voffset = decode_locdesc (DW_BLOCK (attr), cu) + 2;
        }
      else if (attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
        {
	  dwarf2_complex_location_expr_complaint ();
        }
      else
        {
	  dwarf2_invalid_attrib_class_complaint ("DW_AT_vtable_elem_location",
						 fieldname);
        }
   }
}

/* Create the vector of member function fields, and attach it to the type.  */

static void
dwarf2_attach_fn_fields_to_type (struct field_info *fip, struct type *type,
				 struct dwarf2_cu *cu)
{
  struct fnfieldlist *flp;
  int total_length = 0;
  int i;

  ALLOCATE_CPLUS_STRUCT_TYPE (type);
  TYPE_FN_FIELDLISTS (type) = (struct fn_fieldlist *)
    TYPE_ALLOC (type, sizeof (struct fn_fieldlist) * fip->nfnfields);

  for (i = 0, flp = fip->fnfieldlists; i < fip->nfnfields; i++, flp++)
    {
      struct nextfnfield *nfp = flp->head;
      struct fn_fieldlist *fn_flp = &TYPE_FN_FIELDLIST (type, i);
      int k;

      TYPE_FN_FIELDLIST_NAME (type, i) = flp->name;
      TYPE_FN_FIELDLIST_LENGTH (type, i) = flp->length;
      fn_flp->fn_fields = (struct fn_field *)
	TYPE_ALLOC (type, sizeof (struct fn_field) * flp->length);
      for (k = flp->length; (k--, nfp); nfp = nfp->next)
	fn_flp->fn_fields[k] = nfp->fnfield;

      total_length += flp->length;
    }

  TYPE_NFN_FIELDS (type) = fip->nfnfields;
  TYPE_NFN_FIELDS_TOTAL (type) = total_length;
}

/* Returns non-zero if NAME is the name of a vtable member in CU's
   language, zero otherwise.  */
static int
is_vtable_name (const char *name, struct dwarf2_cu *cu)
{
  static const char vptr[] = "_vptr";
  static const char vtable[] = "vtable";

  /* Look for the C++ and Java forms of the vtable.  */
  if ((cu->language == language_java
       && strncmp (name, vtable, sizeof (vtable) - 1) == 0)
       || (strncmp (name, vptr, sizeof (vptr) - 1) == 0
       && is_cplus_marker (name[sizeof (vptr) - 1])))
    return 1;

  return 0;
}

/* GCC outputs unnamed structures that are really pointers to member
   functions, with the ABI-specified layout.  If DIE (from CU) describes
   such a structure, set its type, and return nonzero.  Otherwise return
   zero.

   GCC shouldn't do this; it should just output pointer to member DIEs.
   This is GCC PR debug/28767.  */

static int
quirk_gcc_member_function_pointer (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct die_info *pfn_die, *delta_die;
  struct attribute *pfn_name, *delta_name;
  struct type *pfn_type, *domain_type;

  /* Check for a structure with no name and two children.  */
  if (die->tag != DW_TAG_structure_type
      || dwarf2_attr (die, DW_AT_name, cu) != NULL
      || die->child == NULL
      || die->child->sibling == NULL
      || (die->child->sibling->sibling != NULL
	  && die->child->sibling->sibling->tag != DW_TAG_padding))
    return 0;

  /* Check for __pfn and __delta members.  */
  pfn_die = die->child;
  pfn_name = dwarf2_attr (pfn_die, DW_AT_name, cu);
  if (pfn_die->tag != DW_TAG_member
      || pfn_name == NULL
      || DW_STRING (pfn_name) == NULL
      || strcmp ("__pfn", DW_STRING (pfn_name)) != 0)
    return 0;

  delta_die = pfn_die->sibling;
  delta_name = dwarf2_attr (delta_die, DW_AT_name, cu);
  if (delta_die->tag != DW_TAG_member
      || delta_name == NULL
      || DW_STRING (delta_name) == NULL
      || strcmp ("__delta", DW_STRING (delta_name)) != 0)
    return 0;

  /* Find the type of the method.  */
  pfn_type = die_type (pfn_die, cu);
  if (pfn_type == NULL
      || TYPE_CODE (pfn_type) != TYPE_CODE_PTR
      || TYPE_CODE (TYPE_TARGET_TYPE (pfn_type)) != TYPE_CODE_FUNC)
    return 0;

  /* Look for the "this" argument.  */
  pfn_type = TYPE_TARGET_TYPE (pfn_type);
  if (TYPE_NFIELDS (pfn_type) == 0
      || TYPE_CODE (TYPE_FIELD_TYPE (pfn_type, 0)) != TYPE_CODE_PTR)
    return 0;

  domain_type = TYPE_TARGET_TYPE (TYPE_FIELD_TYPE (pfn_type, 0));
  type = alloc_type (objfile);
  smash_to_method_type (type, domain_type, TYPE_TARGET_TYPE (pfn_type),
			TYPE_FIELDS (pfn_type), TYPE_NFIELDS (pfn_type),
			TYPE_VARARGS (pfn_type));
  type = lookup_methodptr_type (type);
  set_die_type (die, type, cu);

  return 1;
}

/* Called when we find the DIE that starts a structure or union scope
   (definition) to process all dies that define the members of the
   structure or union.

   NOTE: we need to call struct_type regardless of whether or not the
   DIE has an at_name attribute, since it might be an anonymous
   structure or union.  This gets the type entered into our set of
   user defined types.

   However, if the structure is incomplete (an opaque struct/union)
   then suppress creating a symbol table entry for it since gdb only
   wants to find the one with the complete definition.  Note that if
   it is complete, we just call new_symbol, which does it's own
   checking about whether the struct/union is anonymous or not (and
   suppresses creating a symbol table entry itself).  */

static void
read_structure_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;
  const char *previous_prefix = processing_current_prefix;
  struct cleanup *back_to = NULL;

  if (die->type)
    return;

  if (quirk_gcc_member_function_pointer (die, cu))
    return;

  type = alloc_type (objfile);
  INIT_CPLUS_SPECIFIC (type);
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      if (cu->language == language_cplus
	  || cu->language == language_java)
	{
	  char *new_prefix = determine_class_name (die, cu);
	  TYPE_TAG_NAME (type) = obsavestring (new_prefix,
					       strlen (new_prefix),
					       &objfile->objfile_obstack);
	  back_to = make_cleanup (xfree, new_prefix);
	  processing_current_prefix = new_prefix;
	}
      else
	{
	  /* The name is already allocated along with this objfile, so
	     we don't need to duplicate it for the type.  */
	  TYPE_TAG_NAME (type) = DW_STRING (attr);
	}
    }

  if (die->tag == DW_TAG_structure_type)
    {
      TYPE_CODE (type) = TYPE_CODE_STRUCT;
    }
  else if (die->tag == DW_TAG_union_type)
    {
      TYPE_CODE (type) = TYPE_CODE_UNION;
    }
  else
    {
      /* FIXME: TYPE_CODE_CLASS is currently defined to TYPE_CODE_STRUCT
         in gdbtypes.h.  */
      TYPE_CODE (type) = TYPE_CODE_CLASS;
    }

  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = 0;
    }

  TYPE_FLAGS (type) |= TYPE_FLAG_STUB_SUPPORTED;
  if (die_is_declaration (die, cu))
    TYPE_FLAGS (type) |= TYPE_FLAG_STUB;

  /* We need to add the type field to the die immediately so we don't
     infinitely recurse when dealing with pointers to the structure
     type within the structure itself. */
  set_die_type (die, type, cu);

  if (die->child != NULL && ! die_is_declaration (die, cu))
    {
      struct field_info fi;
      struct die_info *child_die;
      struct cleanup *back_to = make_cleanup (null_cleanup, NULL);

      memset (&fi, 0, sizeof (struct field_info));

      child_die = die->child;

      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_member
	      || child_die->tag == DW_TAG_variable)
	    {
	      /* NOTE: carlton/2002-11-05: A C++ static data member
		 should be a DW_TAG_member that is a declaration, but
		 all versions of G++ as of this writing (so through at
		 least 3.2.1) incorrectly generate DW_TAG_variable
		 tags for them instead.  */
	      dwarf2_add_field (&fi, child_die, cu);
	    }
	  else if (child_die->tag == DW_TAG_subprogram)
	    {
	      /* C++ member function. */
	      read_type_die (child_die, cu);
	      dwarf2_add_member_fn (&fi, child_die, type, cu);
	    }
	  else if (child_die->tag == DW_TAG_inheritance)
	    {
	      /* C++ base class field.  */
	      dwarf2_add_field (&fi, child_die, cu);
	    }
	  child_die = sibling_die (child_die);
	}

      /* Attach fields and member functions to the type.  */
      if (fi.nfields)
	dwarf2_attach_fields_to_type (&fi, type, cu);
      if (fi.nfnfields)
	{
	  dwarf2_attach_fn_fields_to_type (&fi, type, cu);

	  /* Get the type which refers to the base class (possibly this
	     class itself) which contains the vtable pointer for the current
	     class from the DW_AT_containing_type attribute.  */

	  if (dwarf2_attr (die, DW_AT_containing_type, cu) != NULL)
	    {
	      struct type *t = die_containing_type (die, cu);

	      TYPE_VPTR_BASETYPE (type) = t;
	      if (type == t)
		{
		  int i;

		  /* Our own class provides vtbl ptr.  */
		  for (i = TYPE_NFIELDS (t) - 1;
		       i >= TYPE_N_BASECLASSES (t);
		       --i)
		    {
		      char *fieldname = TYPE_FIELD_NAME (t, i);

                      if (is_vtable_name (fieldname, cu))
			{
			  TYPE_VPTR_FIELDNO (type) = i;
			  break;
			}
		    }

		  /* Complain if virtual function table field not found.  */
		  if (i < TYPE_N_BASECLASSES (t))
		    complaint (&symfile_complaints,
			       _("virtual function table pointer not found when defining class '%s'"),
			       TYPE_TAG_NAME (type) ? TYPE_TAG_NAME (type) :
			       "");
		}
	      else
		{
		  TYPE_VPTR_FIELDNO (type) = TYPE_VPTR_FIELDNO (t);
		}
	    }
	  else if (cu->producer
		   && strncmp (cu->producer,
			       "IBM(R) XL C/C++ Advanced Edition", 32) == 0)
	    {
	      /* The IBM XLC compiler does not provide direct indication
	         of the containing type, but the vtable pointer is
	         always named __vfp.  */

	      int i;

	      for (i = TYPE_NFIELDS (type) - 1;
		   i >= TYPE_N_BASECLASSES (type);
		   --i)
		{
		  if (strcmp (TYPE_FIELD_NAME (type, i), "__vfp") == 0)
		    {
		      TYPE_VPTR_FIELDNO (type) = i;
		      TYPE_VPTR_BASETYPE (type) = type;
		      break;
		    }
		}
	    }
	}

      do_cleanups (back_to);
    }

  processing_current_prefix = previous_prefix;
  if (back_to != NULL)
    do_cleanups (back_to);
}

static void
process_structure_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  const char *previous_prefix = processing_current_prefix;
  struct die_info *child_die = die->child;

  if (TYPE_TAG_NAME (die->type) != NULL)
    processing_current_prefix = TYPE_TAG_NAME (die->type);

  /* NOTE: carlton/2004-03-16: GCC 3.4 (or at least one of its
     snapshots) has been known to create a die giving a declaration
     for a class that has, as a child, a die giving a definition for a
     nested class.  So we have to process our children even if the
     current die is a declaration.  Normally, of course, a declaration
     won't have any children at all.  */

  while (child_die != NULL && child_die->tag)
    {
      if (child_die->tag == DW_TAG_member
	  || child_die->tag == DW_TAG_variable
	  || child_die->tag == DW_TAG_inheritance)
	{
	  /* Do nothing.  */
	}
      else
	process_die (child_die, cu);

      child_die = sibling_die (child_die);
    }

  /* Do not consider external references.  According to the DWARF standard,
     these DIEs are identified by the fact that they have no byte_size
     attribute, and a declaration attribute.  */
  if (dwarf2_attr (die, DW_AT_byte_size, cu) != NULL
      || !die_is_declaration (die, cu))
    new_symbol (die, die->type, cu);

  processing_current_prefix = previous_prefix;
}

/* Given a DW_AT_enumeration_type die, set its type.  We do not
   complete the type's fields yet, or create any symbols.  */

static void
read_enumeration_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;

  if (die->type)
    return;

  type = alloc_type (objfile);

  TYPE_CODE (type) = TYPE_CODE_ENUM;
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      char *name = DW_STRING (attr);

      if (processing_has_namespace_info)
	{
	  TYPE_TAG_NAME (type) = typename_concat (&objfile->objfile_obstack,
						  processing_current_prefix,
						  name, cu);
	}
      else
	{
	  /* The name is already allocated along with this objfile, so
	     we don't need to duplicate it for the type.  */
	  TYPE_TAG_NAME (type) = name;
	}
    }

  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = 0;
    }

  set_die_type (die, type, cu);
}

/* Determine the name of the type represented by DIE, which should be
   a named C++ or Java compound type.  Return the name in question; the caller
   is responsible for xfree()'ing it.  */

static char *
determine_class_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct cleanup *back_to = NULL;
  struct die_info *spec_die = die_specification (die, cu);
  char *new_prefix = NULL;

  /* If this is the definition of a class that is declared by another
     die, then processing_current_prefix may not be accurate; see
     read_func_scope for a similar example.  */
  if (spec_die != NULL)
    {
      char *specification_prefix = determine_prefix (spec_die, cu);
      processing_current_prefix = specification_prefix;
      back_to = make_cleanup (xfree, specification_prefix);
    }

  /* If we don't have namespace debug info, guess the name by trying
     to demangle the names of members, just like we did in
     guess_structure_name.  */
  if (!processing_has_namespace_info)
    {
      struct die_info *child;

      for (child = die->child;
	   child != NULL && child->tag != 0;
	   child = sibling_die (child))
	{
	  if (child->tag == DW_TAG_subprogram)
	    {
	      new_prefix 
		= language_class_name_from_physname (cu->language_defn,
						     dwarf2_linkage_name
						     (child, cu));

	      if (new_prefix != NULL)
		break;
	    }
	}
    }

  if (new_prefix == NULL)
    {
      const char *name = dwarf2_name (die, cu);
      new_prefix = typename_concat (NULL, processing_current_prefix,
				    name ? name : "<<anonymous>>", 
				    cu);
    }

  if (back_to != NULL)
    do_cleanups (back_to);

  return new_prefix;
}

/* Given a pointer to a die which begins an enumeration, process all
   the dies that define the members of the enumeration, and create the
   symbol for the enumeration type.

   NOTE: We reverse the order of the element list.  */

static void
process_enumeration_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct die_info *child_die;
  struct field *fields;
  struct attribute *attr;
  struct symbol *sym;
  int num_fields;
  int unsigned_enum = 1;

  num_fields = 0;
  fields = NULL;
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag != DW_TAG_enumerator)
	    {
	      process_die (child_die, cu);
	    }
	  else
	    {
	      attr = dwarf2_attr (child_die, DW_AT_name, cu);
	      if (attr)
		{
		  sym = new_symbol (child_die, die->type, cu);
		  if (SYMBOL_VALUE (sym) < 0)
		    unsigned_enum = 0;

		  if ((num_fields % DW_FIELD_ALLOC_CHUNK) == 0)
		    {
		      fields = (struct field *)
			xrealloc (fields,
				  (num_fields + DW_FIELD_ALLOC_CHUNK)
				  * sizeof (struct field));
		    }

		  FIELD_NAME (fields[num_fields]) = DEPRECATED_SYMBOL_NAME (sym);
		  FIELD_TYPE (fields[num_fields]) = NULL;
		  FIELD_BITPOS (fields[num_fields]) = SYMBOL_VALUE (sym);
		  FIELD_BITSIZE (fields[num_fields]) = 0;
		  FIELD_STATIC_KIND (fields[num_fields]) = 0;

		  num_fields++;
		}
	    }

	  child_die = sibling_die (child_die);
	}

      if (num_fields)
	{
	  TYPE_NFIELDS (die->type) = num_fields;
	  TYPE_FIELDS (die->type) = (struct field *)
	    TYPE_ALLOC (die->type, sizeof (struct field) * num_fields);
	  memcpy (TYPE_FIELDS (die->type), fields,
		  sizeof (struct field) * num_fields);
	  xfree (fields);
	}
      if (unsigned_enum)
	TYPE_FLAGS (die->type) |= TYPE_FLAG_UNSIGNED;
    }

  new_symbol (die, die->type, cu);
}

/* Extract all information from a DW_TAG_array_type DIE and put it in
   the DIE's type field.  For now, this only handles one dimensional
   arrays.  */

static void
read_array_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct die_info *child_die;
  struct type *type = NULL;
  struct type *element_type, *range_type, *index_type;
  struct type **range_types = NULL;
  struct attribute *attr;
  int ndim = 0;
  struct cleanup *back_to;

  /* Return if we've already decoded this type. */
  if (die->type)
    {
      return;
    }

  element_type = die_type (die, cu);

  /* Irix 6.2 native cc creates array types without children for
     arrays with unspecified length.  */
  if (die->child == NULL)
    {
      index_type = dwarf2_fundamental_type (objfile, FT_INTEGER, cu);
      range_type = create_range_type (NULL, index_type, 0, -1);
      set_die_type (die, create_array_type (NULL, element_type, range_type),
		    cu);
      return;
    }

  back_to = make_cleanup (null_cleanup, NULL);
  child_die = die->child;
  while (child_die && child_die->tag)
    {
      if (child_die->tag == DW_TAG_subrange_type)
	{
          read_subrange_type (child_die, cu);

          if (child_die->type != NULL)
            {
	      /* The range type was succesfully read. Save it for
                 the array type creation.  */
              if ((ndim % DW_FIELD_ALLOC_CHUNK) == 0)
                {
                  range_types = (struct type **)
                    xrealloc (range_types, (ndim + DW_FIELD_ALLOC_CHUNK)
                              * sizeof (struct type *));
                  if (ndim == 0)
                    make_cleanup (free_current_contents, &range_types);
	        }
	      range_types[ndim++] = child_die->type;
            }
	}
      child_die = sibling_die (child_die);
    }

  /* Dwarf2 dimensions are output from left to right, create the
     necessary array types in backwards order.  */

  type = element_type;

  if (read_array_order (die, cu) == DW_ORD_col_major)
    {
      int i = 0;
      while (i < ndim)
	type = create_array_type (NULL, type, range_types[i++]);
    }
  else
    {
      while (ndim-- > 0)
	type = create_array_type (NULL, type, range_types[ndim]);
    }

  /* Understand Dwarf2 support for vector types (like they occur on
     the PowerPC w/ AltiVec).  Gcc just adds another attribute to the
     array type.  This is not part of the Dwarf2/3 standard yet, but a
     custom vendor extension.  The main difference between a regular
     array and the vector variant is that vectors are passed by value
     to functions.  */
  attr = dwarf2_attr (die, DW_AT_GNU_vector, cu);
  if (attr)
    make_vector_type (type);

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    TYPE_NAME (type) = DW_STRING (attr);
  
  do_cleanups (back_to);

  /* Install the type in the die. */
  set_die_type (die, type, cu);
}

static enum dwarf_array_dim_ordering
read_array_order (struct die_info *die, struct dwarf2_cu *cu) 
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_ordering, cu);

  if (attr) return DW_SND (attr);

  /*
    GNU F77 is a special case, as at 08/2004 array type info is the
    opposite order to the dwarf2 specification, but data is still 
    laid out as per normal fortran.

    FIXME: dsl/2004-8-20: If G77 is ever fixed, this will also need 
    version checking.
  */

  if (cu->language == language_fortran &&
      cu->producer && strstr (cu->producer, "GNU F77"))
    {
      return DW_ORD_row_major;
    }

  switch (cu->language_defn->la_array_ordering) 
    {
    case array_column_major:
      return DW_ORD_col_major;
    case array_row_major:
    default:
      return DW_ORD_row_major;
    };
}

/* Extract all information from a DW_TAG_set_type DIE and put it in
   the DIE's type field. */

static void
read_set_type (struct die_info *die, struct dwarf2_cu *cu)
{
  if (die->type == NULL)
    die->type = create_set_type ((struct type *) NULL, die_type (die, cu));
}

/* First cut: install each common block member as a global variable.  */

static void
read_common_block (struct die_info *die, struct dwarf2_cu *cu)
{
  struct die_info *child_die;
  struct attribute *attr;
  struct symbol *sym;
  CORE_ADDR base = (CORE_ADDR) 0;

  attr = dwarf2_attr (die, DW_AT_location, cu);
  if (attr)
    {
      /* Support the .debug_loc offsets */
      if (attr_form_is_block (attr))
        {
          base = decode_locdesc (DW_BLOCK (attr), cu);
        }
      else if (attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
        {
	  dwarf2_complex_location_expr_complaint ();
        }
      else
        {
	  dwarf2_invalid_attrib_class_complaint ("DW_AT_location",
						 "common block member");
        }
    }
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  sym = new_symbol (child_die, NULL, cu);
	  attr = dwarf2_attr (child_die, DW_AT_data_member_location, cu);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) =
		base + decode_locdesc (DW_BLOCK (attr), cu);
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  child_die = sibling_die (child_die);
	}
    }
}

/* Read a C++ namespace.  */

static void
read_namespace (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  const char *previous_prefix = processing_current_prefix;
  const char *name;
  int is_anonymous;
  struct die_info *current_die;
  struct cleanup *back_to = make_cleanup (null_cleanup, 0);

  name = namespace_name (die, &is_anonymous, cu);

  /* Now build the name of the current namespace.  */

  if (previous_prefix[0] == '\0')
    {
      processing_current_prefix = name;
    }
  else
    {
      char *temp_name = typename_concat (NULL, previous_prefix, name, cu);
      make_cleanup (xfree, temp_name);
      processing_current_prefix = temp_name;
    }

  /* Add a symbol associated to this if we haven't seen the namespace
     before.  Also, add a using directive if it's an anonymous
     namespace.  */

  if (dwarf2_extension (die, cu) == NULL)
    {
      struct type *type;

      /* FIXME: carlton/2003-06-27: Once GDB is more const-correct,
	 this cast will hopefully become unnecessary.  */
      type = init_type (TYPE_CODE_NAMESPACE, 0, 0,
			(char *) processing_current_prefix,
			objfile);
      TYPE_TAG_NAME (type) = TYPE_NAME (type);

      new_symbol (die, type, cu);
      set_die_type (die, type, cu);

      if (is_anonymous)
	cp_add_using_directive (processing_current_prefix,
				strlen (previous_prefix),
				strlen (processing_current_prefix));
    }

  if (die->child != NULL)
    {
      struct die_info *child_die = die->child;
      
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  processing_current_prefix = previous_prefix;
  do_cleanups (back_to);
}

/* Return the name of the namespace represented by DIE.  Set
   *IS_ANONYMOUS to tell whether or not the namespace is an anonymous
   namespace.  */

static const char *
namespace_name (struct die_info *die, int *is_anonymous, struct dwarf2_cu *cu)
{
  struct die_info *current_die;
  const char *name = NULL;

  /* Loop through the extensions until we find a name.  */

  for (current_die = die;
       current_die != NULL;
       current_die = dwarf2_extension (die, cu))
    {
      name = dwarf2_name (current_die, cu);
      if (name != NULL)
	break;
    }

  /* Is it an anonymous namespace?  */

  *is_anonymous = (name == NULL);
  if (*is_anonymous)
    name = "(anonymous namespace)";

  return name;
}

/* Extract all information from a DW_TAG_pointer_type DIE and add to
   the user defined type vector.  */

static void
read_tag_pointer_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  struct type *type;
  struct attribute *attr_byte_size;
  struct attribute *attr_address_class;
  int byte_size, addr_class;

  if (die->type)
    {
      return;
    }

  type = lookup_pointer_type (die_type (die, cu));

  attr_byte_size = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr_byte_size)
    byte_size = DW_UNSND (attr_byte_size);
  else
    byte_size = cu_header->addr_size;

  attr_address_class = dwarf2_attr (die, DW_AT_address_class, cu);
  if (attr_address_class)
    addr_class = DW_UNSND (attr_address_class);
  else
    addr_class = DW_ADDR_none;

  /* If the pointer size or address class is different than the
     default, create a type variant marked as such and set the
     length accordingly.  */
  if (TYPE_LENGTH (type) != byte_size || addr_class != DW_ADDR_none)
    {
      if (gdbarch_address_class_type_flags_p (current_gdbarch))
	{
	  int type_flags;

	  type_flags = gdbarch_address_class_type_flags
			 (current_gdbarch, byte_size, addr_class);
	  gdb_assert ((type_flags & ~TYPE_FLAG_ADDRESS_CLASS_ALL) == 0);
	  type = make_type_with_address_space (type, type_flags);
	}
      else if (TYPE_LENGTH (type) != byte_size)
	{
	  complaint (&symfile_complaints, _("invalid pointer size %d"), byte_size);
	}
      else {
	/* Should we also complain about unhandled address classes?  */
      }
    }

  TYPE_LENGTH (type) = byte_size;
  set_die_type (die, type, cu);
}

/* Extract all information from a DW_TAG_ptr_to_member_type DIE and add to
   the user defined type vector.  */

static void
read_tag_ptr_to_member_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct type *to_type;
  struct type *domain;

  if (die->type)
    {
      return;
    }

  to_type = die_type (die, cu);
  domain = die_containing_type (die, cu);

  if (TYPE_CODE (check_typedef (to_type)) == TYPE_CODE_METHOD)
    type = lookup_methodptr_type (to_type);
  else
    type = lookup_memberptr_type (to_type, domain);

  set_die_type (die, type, cu);
}

/* Extract all information from a DW_TAG_reference_type DIE and add to
   the user defined type vector.  */

static void
read_tag_reference_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  struct type *type;
  struct attribute *attr;

  if (die->type)
    {
      return;
    }

  type = lookup_reference_type (die_type (die, cu));
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH (type) = cu_header->addr_size;
    }
  set_die_type (die, type, cu);
}

static void
read_tag_const_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;

  if (die->type)
    {
      return;
    }

  base_type = die_type (die, cu);
  set_die_type (die, make_cv_type (1, TYPE_VOLATILE (base_type), base_type, 0),
		cu);
}

static void
read_tag_volatile_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;

  if (die->type)
    {
      return;
    }

  base_type = die_type (die, cu);
  set_die_type (die, make_cv_type (TYPE_CONST (base_type), 1, base_type, 0),
		cu);
}

/* Extract all information from a DW_TAG_string_type DIE and add to
   the user defined type vector.  It isn't really a user defined type,
   but it behaves like one, with other DIE's using an AT_user_def_type
   attribute to reference it.  */

static void
read_tag_string_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type, *range_type, *index_type, *char_type;
  struct attribute *attr;
  unsigned int length;

  if (die->type)
    {
      return;
    }

  attr = dwarf2_attr (die, DW_AT_string_length, cu);
  if (attr)
    {
      length = DW_UNSND (attr);
    }
  else
    {
      /* check for the DW_AT_byte_size attribute */
      attr = dwarf2_attr (die, DW_AT_byte_size, cu);
      if (attr)
        {
          length = DW_UNSND (attr);
        }
      else
        {
          length = 1;
        }
    }
  index_type = dwarf2_fundamental_type (objfile, FT_INTEGER, cu);
  range_type = create_range_type (NULL, index_type, 1, length);
  if (cu->language == language_fortran)
    {
      /* Need to create a unique string type for bounds
         information */
      type = create_string_type (0, range_type);
    }
  else
    {
      char_type = dwarf2_fundamental_type (objfile, FT_CHAR, cu);
      type = create_string_type (char_type, range_type);
    }
  set_die_type (die, type, cu);
}

/* Handle DIES due to C code like:

   struct foo
   {
   int (*funcp)(int a, long l);
   int b;
   };

   ('funcp' generates a DW_TAG_subroutine_type DIE)
 */

static void
read_subroutine_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type;		/* Type that this function returns */
  struct type *ftype;		/* Function that returns above type */
  struct attribute *attr;

  /* Decode the type that this subroutine returns */
  if (die->type)
    {
      return;
    }
  type = die_type (die, cu);
  ftype = make_function_type (type, (struct type **) 0);

  /* All functions in C++ and Java have prototypes.  */
  attr = dwarf2_attr (die, DW_AT_prototyped, cu);
  if ((attr && (DW_UNSND (attr) != 0))
      || cu->language == language_cplus
      || cu->language == language_java)
    TYPE_FLAGS (ftype) |= TYPE_FLAG_PROTOTYPED;

  if (die->child != NULL)
    {
      struct die_info *child_die;
      int nparams = 0;
      int iparams = 0;

      /* Count the number of parameters.
         FIXME: GDB currently ignores vararg functions, but knows about
         vararg member functions.  */
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_formal_parameter)
	    nparams++;
	  else if (child_die->tag == DW_TAG_unspecified_parameters)
	    TYPE_FLAGS (ftype) |= TYPE_FLAG_VARARGS;
	  child_die = sibling_die (child_die);
	}

      /* Allocate storage for parameters and fill them in.  */
      TYPE_NFIELDS (ftype) = nparams;
      TYPE_FIELDS (ftype) = (struct field *)
	TYPE_ZALLOC (ftype, nparams * sizeof (struct field));

      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_formal_parameter)
	    {
	      /* Dwarf2 has no clean way to discern C++ static and non-static
	         member functions. G++ helps GDB by marking the first
	         parameter for non-static member functions (which is the
	         this pointer) as artificial. We pass this information
	         to dwarf2_add_member_fn via TYPE_FIELD_ARTIFICIAL.  */
	      attr = dwarf2_attr (child_die, DW_AT_artificial, cu);
	      if (attr)
		TYPE_FIELD_ARTIFICIAL (ftype, iparams) = DW_UNSND (attr);
	      else
		TYPE_FIELD_ARTIFICIAL (ftype, iparams) = 0;
	      TYPE_FIELD_TYPE (ftype, iparams) = die_type (child_die, cu);
	      iparams++;
	    }
	  child_die = sibling_die (child_die);
	}
    }

  set_die_type (die, ftype, cu);
}

static void
read_typedef (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct attribute *attr;
  char *name = NULL;

  if (!die->type)
    {
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	{
	  name = DW_STRING (attr);
	}
      set_die_type (die, init_type (TYPE_CODE_TYPEDEF, 0,
				    TYPE_FLAG_TARGET_STUB, name, objfile),
		    cu);
      TYPE_TARGET_TYPE (die->type) = die_type (die, cu);
    }
}

/* Find a representation of a given base type and install
   it in the TYPE field of the die.  */

static void
read_base_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;
  int encoding = 0, size = 0;

  /* If we've already decoded this die, this is a no-op. */
  if (die->type)
    {
      return;
    }

  attr = dwarf2_attr (die, DW_AT_encoding, cu);
  if (attr)
    {
      encoding = DW_UNSND (attr);
    }
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      size = DW_UNSND (attr);
    }
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      enum type_code code = TYPE_CODE_INT;
      int type_flags = 0;

      switch (encoding)
	{
	case DW_ATE_address:
	  /* Turn DW_ATE_address into a void * pointer.  */
	  code = TYPE_CODE_PTR;
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	case DW_ATE_boolean:
	  code = TYPE_CODE_BOOL;
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	case DW_ATE_complex_float:
	  code = TYPE_CODE_COMPLEX;
	  break;
	case DW_ATE_float:
	  code = TYPE_CODE_FLT;
	  break;
	case DW_ATE_signed:
	  break;
	case DW_ATE_unsigned:
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	case DW_ATE_signed_char:
	  if (cu->language == language_m2)
	    code = TYPE_CODE_CHAR;
	  break;
 	case DW_ATE_unsigned_char:
	  if (cu->language == language_m2)
	    code = TYPE_CODE_CHAR;
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	default:
	  complaint (&symfile_complaints, _("unsupported DW_AT_encoding: '%s'"),
		     dwarf_type_encoding_name (encoding));
	  break;
	}
      type = init_type (code, size, type_flags, DW_STRING (attr), objfile);
      if (encoding == DW_ATE_address)
	TYPE_TARGET_TYPE (type) = dwarf2_fundamental_type (objfile, FT_VOID,
							   cu);
      else if (encoding == DW_ATE_complex_float)
	{
	  if (size == 32)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_EXT_PREC_FLOAT, cu);
	  else if (size == 16)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_DBL_PREC_FLOAT, cu);
	  else if (size == 8)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_FLOAT, cu);
	}
    }
  else
    {
      type = dwarf_base_type (encoding, size, cu);
    }
  set_die_type (die, type, cu);
}

/* Read the given DW_AT_subrange DIE.  */

static void
read_subrange_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;
  struct type *range_type;
  struct attribute *attr;
  int low = 0;
  int high = -1;
  
  /* If we have already decoded this die, then nothing more to do.  */
  if (die->type)
    return;

  base_type = die_type (die, cu);
  if (TYPE_CODE (base_type) == TYPE_CODE_VOID)
    {
      complaint (&symfile_complaints,
                _("DW_AT_type missing from DW_TAG_subrange_type"));
      base_type
	= dwarf_base_type (DW_ATE_signed,
			   gdbarch_addr_bit (current_gdbarch) / 8, cu);
    }

  if (cu->language == language_fortran)
    { 
      /* FORTRAN implies a lower bound of 1, if not given.  */
      low = 1;
    }

  /* FIXME: For variable sized arrays either of these could be
     a variable rather than a constant value.  We'll allow it,
     but we don't know how to handle it.  */
  attr = dwarf2_attr (die, DW_AT_lower_bound, cu);
  if (attr)
    low = dwarf2_get_attr_constant_value (attr, 0);

  attr = dwarf2_attr (die, DW_AT_upper_bound, cu);
  if (attr)
    {       
      if (attr->form == DW_FORM_block1)
        {
          /* GCC encodes arrays with unspecified or dynamic length
             with a DW_FORM_block1 attribute.
             FIXME: GDB does not yet know how to handle dynamic
             arrays properly, treat them as arrays with unspecified
             length for now.

             FIXME: jimb/2003-09-22: GDB does not really know
             how to handle arrays of unspecified length
             either; we just represent them as zero-length
             arrays.  Choose an appropriate upper bound given
             the lower bound we've computed above.  */
          high = low - 1;
        }
      else
        high = dwarf2_get_attr_constant_value (attr, 1);
    }

  range_type = create_range_type (NULL, base_type, low, high);

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    TYPE_NAME (range_type) = DW_STRING (attr);
  
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    TYPE_LENGTH (range_type) = DW_UNSND (attr);

  set_die_type (die, range_type, cu);
}
  
static void
read_unspecified_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type;
  struct attribute *attr;

  if (die->type)
    return;

  /* For now, we only support the C meaning of an unspecified type: void.  */

  attr = dwarf2_attr (die, DW_AT_name, cu);
  type = init_type (TYPE_CODE_VOID, 0, 0, attr ? DW_STRING (attr) : "",
		    cu->objfile);

  set_die_type (die, type, cu);
}

/* Read a whole compilation unit into a linked list of dies.  */

static struct die_info *
read_comp_unit (gdb_byte *info_ptr, bfd *abfd, struct dwarf2_cu *cu)
{
  return read_die_and_children (info_ptr, abfd, cu, &info_ptr, NULL);
}

/* Read a single die and all its descendents.  Set the die's sibling
   field to NULL; set other fields in the die correctly, and set all
   of the descendents' fields correctly.  Set *NEW_INFO_PTR to the
   location of the info_ptr after reading all of those dies.  PARENT
   is the parent of the die in question.  */

static struct die_info *
read_die_and_children (gdb_byte *info_ptr, bfd *abfd,
		       struct dwarf2_cu *cu,
		       gdb_byte **new_info_ptr,
		       struct die_info *parent)
{
  struct die_info *die;
  gdb_byte *cur_ptr;
  int has_children;

  cur_ptr = read_full_die (&die, abfd, info_ptr, cu, &has_children);
  store_in_ref_table (die->offset, die, cu);

  if (has_children)
    {
      die->child = read_die_and_siblings (cur_ptr, abfd, cu,
					  new_info_ptr, die);
    }
  else
    {
      die->child = NULL;
      *new_info_ptr = cur_ptr;
    }

  die->sibling = NULL;
  die->parent = parent;
  return die;
}

/* Read a die, all of its descendents, and all of its siblings; set
   all of the fields of all of the dies correctly.  Arguments are as
   in read_die_and_children.  */

static struct die_info *
read_die_and_siblings (gdb_byte *info_ptr, bfd *abfd,
		       struct dwarf2_cu *cu,
		       gdb_byte **new_info_ptr,
		       struct die_info *parent)
{
  struct die_info *first_die, *last_sibling;
  gdb_byte *cur_ptr;

  cur_ptr = info_ptr;
  first_die = last_sibling = NULL;

  while (1)
    {
      struct die_info *die
	= read_die_and_children (cur_ptr, abfd, cu, &cur_ptr, parent);

      if (!first_die)
	{
	  first_die = die;
	}
      else
	{
	  last_sibling->sibling = die;
	}

      if (die->tag == 0)
	{
	  *new_info_ptr = cur_ptr;
	  return first_die;
	}
      else
	{
	  last_sibling = die;
	}
    }
}

/* Free a linked list of dies.  */

static void
free_die_list (struct die_info *dies)
{
  struct die_info *die, *next;

  die = dies;
  while (die)
    {
      if (die->child != NULL)
	free_die_list (die->child);
      next = die->sibling;
      xfree (die->attrs);
      xfree (die);
      die = next;
    }
}

/* Read the contents of the section at OFFSET and of size SIZE from the
   object file specified by OBJFILE into the objfile_obstack and return it.  */

gdb_byte *
dwarf2_read_section (struct objfile *objfile, asection *sectp)
{
  bfd *abfd = objfile->obfd;
  gdb_byte *buf, *retbuf;
  bfd_size_type size = bfd_get_section_size (sectp);

  if (size == 0)
    return NULL;

  buf = obstack_alloc (&objfile->objfile_obstack, size);
  retbuf = symfile_relocate_debug_section (abfd, sectp, buf);
  if (retbuf != NULL)
    return retbuf;

  if (bfd_seek (abfd, sectp->filepos, SEEK_SET) != 0
      || bfd_bread (buf, size, abfd) != size)
    error (_("Dwarf Error: Can't read DWARF data from '%s'"),
	   bfd_get_filename (abfd));

  return buf;
}

/* In DWARF version 2, the description of the debugging information is
   stored in a separate .debug_abbrev section.  Before we read any
   dies from a section we read in all abbreviations and install them
   in a hash table.  This function also sets flags in CU describing
   the data found in the abbrev table.  */

static void
dwarf2_read_abbrevs (bfd *abfd, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  gdb_byte *abbrev_ptr;
  struct abbrev_info *cur_abbrev;
  unsigned int abbrev_number, bytes_read, abbrev_name;
  unsigned int abbrev_form, hash_number;
  struct attr_abbrev *cur_attrs;
  unsigned int allocated_attrs;

  /* Initialize dwarf2 abbrevs */
  obstack_init (&cu->abbrev_obstack);
  cu->dwarf2_abbrevs = obstack_alloc (&cu->abbrev_obstack,
				      (ABBREV_HASH_SIZE
				       * sizeof (struct abbrev_info *)));
  memset (cu->dwarf2_abbrevs, 0,
          ABBREV_HASH_SIZE * sizeof (struct abbrev_info *));

  abbrev_ptr = dwarf2_per_objfile->abbrev_buffer + cu_header->abbrev_offset;
  abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
  abbrev_ptr += bytes_read;

  allocated_attrs = ATTR_ALLOC_CHUNK;
  cur_attrs = xmalloc (allocated_attrs * sizeof (struct attr_abbrev));
  
  /* loop until we reach an abbrev number of 0 */
  while (abbrev_number)
    {
      cur_abbrev = dwarf_alloc_abbrev (cu);

      /* read in abbrev header */
      cur_abbrev->number = abbrev_number;
      cur_abbrev->tag = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      cur_abbrev->has_children = read_1_byte (abfd, abbrev_ptr);
      abbrev_ptr += 1;

      if (cur_abbrev->tag == DW_TAG_namespace)
	cu->has_namespace_info = 1;

      /* now read in declarations */
      abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      while (abbrev_name)
	{
	  if (cur_abbrev->num_attrs == allocated_attrs)
	    {
	      allocated_attrs += ATTR_ALLOC_CHUNK;
	      cur_attrs
		= xrealloc (cur_attrs, (allocated_attrs
					* sizeof (struct attr_abbrev)));
	    }

	  /* Record whether this compilation unit might have
	     inter-compilation-unit references.  If we don't know what form
	     this attribute will have, then it might potentially be a
	     DW_FORM_ref_addr, so we conservatively expect inter-CU
	     references.  */

	  if (abbrev_form == DW_FORM_ref_addr
	      || abbrev_form == DW_FORM_indirect)
	    cu->has_form_ref_addr = 1;

	  cur_attrs[cur_abbrev->num_attrs].name = abbrev_name;
	  cur_attrs[cur_abbrev->num_attrs++].form = abbrev_form;
	  abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	}

      cur_abbrev->attrs = obstack_alloc (&cu->abbrev_obstack,
					 (cur_abbrev->num_attrs
					  * sizeof (struct attr_abbrev)));
      memcpy (cur_abbrev->attrs, cur_attrs,
	      cur_abbrev->num_attrs * sizeof (struct attr_abbrev));

      hash_number = abbrev_number % ABBREV_HASH_SIZE;
      cur_abbrev->next = cu->dwarf2_abbrevs[hash_number];
      cu->dwarf2_abbrevs[hash_number] = cur_abbrev;

      /* Get next abbreviation.
         Under Irix6 the abbreviations for a compilation unit are not
         always properly terminated with an abbrev number of 0.
         Exit loop if we encounter an abbreviation which we have
         already read (which means we are about to read the abbreviations
         for the next compile unit) or if the end of the abbreviation
         table is reached.  */
      if ((unsigned int) (abbrev_ptr - dwarf2_per_objfile->abbrev_buffer)
	  >= dwarf2_per_objfile->abbrev_size)
	break;
      abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      if (dwarf2_lookup_abbrev (abbrev_number, cu) != NULL)
	break;
    }

  xfree (cur_attrs);
}

/* Release the memory used by the abbrev table for a compilation unit.  */

static void
dwarf2_free_abbrev_table (void *ptr_to_cu)
{
  struct dwarf2_cu *cu = ptr_to_cu;

  obstack_free (&cu->abbrev_obstack, NULL);
  cu->dwarf2_abbrevs = NULL;
}

/* Lookup an abbrev_info structure in the abbrev hash table.  */

static struct abbrev_info *
dwarf2_lookup_abbrev (unsigned int number, struct dwarf2_cu *cu)
{
  unsigned int hash_number;
  struct abbrev_info *abbrev;

  hash_number = number % ABBREV_HASH_SIZE;
  abbrev = cu->dwarf2_abbrevs[hash_number];

  while (abbrev)
    {
      if (abbrev->number == number)
	return abbrev;
      else
	abbrev = abbrev->next;
    }
  return NULL;
}

/* Returns nonzero if TAG represents a type that we might generate a partial
   symbol for.  */

static int
is_type_tag_for_partial (int tag)
{
  switch (tag)
    {
#if 0
    /* Some types that would be reasonable to generate partial symbols for,
       that we don't at present.  */
    case DW_TAG_array_type:
    case DW_TAG_file_type:
    case DW_TAG_ptr_to_member_type:
    case DW_TAG_set_type:
    case DW_TAG_string_type:
    case DW_TAG_subroutine_type:
#endif
    case DW_TAG_base_type:
    case DW_TAG_class_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_structure_type:
    case DW_TAG_subrange_type:
    case DW_TAG_typedef:
    case DW_TAG_union_type:
      return 1;
    default:
      return 0;
    }
}

/* Load all DIEs that are interesting for partial symbols into memory.  */

static struct partial_die_info *
load_partial_dies (bfd *abfd, gdb_byte *info_ptr, int building_psymtab,
		   struct dwarf2_cu *cu)
{
  struct partial_die_info *part_die;
  struct partial_die_info *parent_die, *last_die, *first_die = NULL;
  struct abbrev_info *abbrev;
  unsigned int bytes_read;
  unsigned int load_all = 0;

  int nesting_level = 1;

  parent_die = NULL;
  last_die = NULL;

  if (cu->per_cu && cu->per_cu->load_all_dies)
    load_all = 1;

  cu->partial_dies
    = htab_create_alloc_ex (cu->header.length / 12,
			    partial_die_hash,
			    partial_die_eq,
			    NULL,
			    &cu->comp_unit_obstack,
			    hashtab_obstack_allocate,
			    dummy_obstack_deallocate);

  part_die = obstack_alloc (&cu->comp_unit_obstack,
			    sizeof (struct partial_die_info));

  while (1)
    {
      abbrev = peek_die_abbrev (info_ptr, &bytes_read, cu);

      /* A NULL abbrev means the end of a series of children.  */
      if (abbrev == NULL)
	{
	  if (--nesting_level == 0)
	    {
	      /* PART_DIE was probably the last thing allocated on the
		 comp_unit_obstack, so we could call obstack_free
		 here.  We don't do that because the waste is small,
		 and will be cleaned up when we're done with this
		 compilation unit.  This way, we're also more robust
		 against other users of the comp_unit_obstack.  */
	      return first_die;
	    }
	  info_ptr += bytes_read;
	  last_die = parent_die;
	  parent_die = parent_die->die_parent;
	  continue;
	}

      /* Check whether this DIE is interesting enough to save.  Normally
	 we would not be interested in members here, but there may be
	 later variables referencing them via DW_AT_specification (for
	 static members).  */
      if (!load_all
	  && !is_type_tag_for_partial (abbrev->tag)
	  && abbrev->tag != DW_TAG_enumerator
	  && abbrev->tag != DW_TAG_subprogram
	  && abbrev->tag != DW_TAG_variable
	  && abbrev->tag != DW_TAG_namespace
	  && abbrev->tag != DW_TAG_member)
	{
	  /* Otherwise we skip to the next sibling, if any.  */
	  info_ptr = skip_one_die (info_ptr + bytes_read, abbrev, cu);
	  continue;
	}

      info_ptr = read_partial_die (part_die, abbrev, bytes_read,
				   abfd, info_ptr, cu);

      /* This two-pass algorithm for processing partial symbols has a
	 high cost in cache pressure.  Thus, handle some simple cases
	 here which cover the majority of C partial symbols.  DIEs
	 which neither have specification tags in them, nor could have
	 specification tags elsewhere pointing at them, can simply be
	 processed and discarded.

	 This segment is also optional; scan_partial_symbols and
	 add_partial_symbol will handle these DIEs if we chain
	 them in normally.  When compilers which do not emit large
	 quantities of duplicate debug information are more common,
	 this code can probably be removed.  */

      /* Any complete simple types at the top level (pretty much all
	 of them, for a language without namespaces), can be processed
	 directly.  */
      if (parent_die == NULL
	  && part_die->has_specification == 0
	  && part_die->is_declaration == 0
	  && (part_die->tag == DW_TAG_typedef
	      || part_die->tag == DW_TAG_base_type
	      || part_die->tag == DW_TAG_subrange_type))
	{
	  if (building_psymtab && part_die->name != NULL)
	    add_psymbol_to_list (part_die->name, strlen (part_die->name),
				 VAR_DOMAIN, LOC_TYPEDEF,
				 &cu->objfile->static_psymbols,
				 0, (CORE_ADDR) 0, cu->language, cu->objfile);
	  info_ptr = locate_pdi_sibling (part_die, info_ptr, abfd, cu);
	  continue;
	}

      /* If we're at the second level, and we're an enumerator, and
	 our parent has no specification (meaning possibly lives in a
	 namespace elsewhere), then we can add the partial symbol now
	 instead of queueing it.  */
      if (part_die->tag == DW_TAG_enumerator
	  && parent_die != NULL
	  && parent_die->die_parent == NULL
	  && parent_die->tag == DW_TAG_enumeration_type
	  && parent_die->has_specification == 0)
	{
	  if (part_die->name == NULL)
	    complaint (&symfile_complaints, _("malformed enumerator DIE ignored"));
	  else if (building_psymtab)
	    add_psymbol_to_list (part_die->name, strlen (part_die->name),
				 VAR_DOMAIN, LOC_CONST,
				 (cu->language == language_cplus
				  || cu->language == language_java)
				 ? &cu->objfile->global_psymbols
				 : &cu->objfile->static_psymbols,
				 0, (CORE_ADDR) 0, cu->language, cu->objfile);

	  info_ptr = locate_pdi_sibling (part_die, info_ptr, abfd, cu);
	  continue;
	}

      /* We'll save this DIE so link it in.  */
      part_die->die_parent = parent_die;
      part_die->die_sibling = NULL;
      part_die->die_child = NULL;

      if (last_die && last_die == parent_die)
	last_die->die_child = part_die;
      else if (last_die)
	last_die->die_sibling = part_die;

      last_die = part_die;

      if (first_die == NULL)
	first_die = part_die;

      /* Maybe add the DIE to the hash table.  Not all DIEs that we
	 find interesting need to be in the hash table, because we
	 also have the parent/sibling/child chains; only those that we
	 might refer to by offset later during partial symbol reading.

	 For now this means things that might have be the target of a
	 DW_AT_specification, DW_AT_abstract_origin, or
	 DW_AT_extension.  DW_AT_extension will refer only to
	 namespaces; DW_AT_abstract_origin refers to functions (and
	 many things under the function DIE, but we do not recurse
	 into function DIEs during partial symbol reading) and
	 possibly variables as well; DW_AT_specification refers to
	 declarations.  Declarations ought to have the DW_AT_declaration
	 flag.  It happens that GCC forgets to put it in sometimes, but
	 only for functions, not for types.

	 Adding more things than necessary to the hash table is harmless
	 except for the performance cost.  Adding too few will result in
	 wasted time in find_partial_die, when we reread the compilation
	 unit with load_all_dies set.  */

      if (load_all
	  || abbrev->tag == DW_TAG_subprogram
	  || abbrev->tag == DW_TAG_variable
	  || abbrev->tag == DW_TAG_namespace
	  || part_die->is_declaration)
	{
	  void **slot;

	  slot = htab_find_slot_with_hash (cu->partial_dies, part_die,
					   part_die->offset, INSERT);
	  *slot = part_die;
	}

      part_die = obstack_alloc (&cu->comp_unit_obstack,
				sizeof (struct partial_die_info));

      /* For some DIEs we want to follow their children (if any).  For C
         we have no reason to follow the children of structures; for other
	 languages we have to, both so that we can get at method physnames
	 to infer fully qualified class names, and for DW_AT_specification.  */
      if (last_die->has_children
	  && (load_all
	      || last_die->tag == DW_TAG_namespace
	      || last_die->tag == DW_TAG_enumeration_type
	      || (cu->language != language_c
		  && (last_die->tag == DW_TAG_class_type
		      || last_die->tag == DW_TAG_structure_type
		      || last_die->tag == DW_TAG_union_type))))
	{
	  nesting_level++;
	  parent_die = last_die;
	  continue;
	}

      /* Otherwise we skip to the next sibling, if any.  */
      info_ptr = locate_pdi_sibling (last_die, info_ptr, abfd, cu);

      /* Back to the top, do it again.  */
    }
}

/* Read a minimal amount of information into the minimal die structure.  */

static gdb_byte *
read_partial_die (struct partial_die_info *part_die,
		  struct abbrev_info *abbrev,
		  unsigned int abbrev_len, bfd *abfd,
		  gdb_byte *info_ptr, struct dwarf2_cu *cu)
{
  unsigned int bytes_read, i;
  struct attribute attr;
  int has_low_pc_attr = 0;
  int has_high_pc_attr = 0;

  memset (part_die, 0, sizeof (struct partial_die_info));

  part_die->offset = info_ptr - dwarf2_per_objfile->info_buffer;

  info_ptr += abbrev_len;

  if (abbrev == NULL)
    return info_ptr;

  part_die->tag = abbrev->tag;
  part_die->has_children = abbrev->has_children;

  for (i = 0; i < abbrev->num_attrs; ++i)
    {
      info_ptr = read_attribute (&attr, &abbrev->attrs[i], abfd, info_ptr, cu);

      /* Store the data if it is of an attribute we want to keep in a
         partial symbol table.  */
      switch (attr.name)
	{
	case DW_AT_name:

	  /* Prefer DW_AT_MIPS_linkage_name over DW_AT_name.  */
	  if (part_die->name == NULL)
	    part_die->name = DW_STRING (&attr);
	  break;
	case DW_AT_comp_dir:
	  if (part_die->dirname == NULL)
	    part_die->dirname = DW_STRING (&attr);
	  break;
	case DW_AT_MIPS_linkage_name:
	  part_die->name = DW_STRING (&attr);
	  break;
	case DW_AT_low_pc:
	  has_low_pc_attr = 1;
	  part_die->lowpc = DW_ADDR (&attr);
	  break;
	case DW_AT_high_pc:
	  has_high_pc_attr = 1;
	  part_die->highpc = DW_ADDR (&attr);
	  break;
	case DW_AT_location:
          /* Support the .debug_loc offsets */
          if (attr_form_is_block (&attr))
            {
	       part_die->locdesc = DW_BLOCK (&attr);
            }
          else if (attr.form == DW_FORM_data4 || attr.form == DW_FORM_data8)
            {
	      dwarf2_complex_location_expr_complaint ();
            }
          else
            {
	      dwarf2_invalid_attrib_class_complaint ("DW_AT_location",
						     "partial symbol information");
            }
	  break;
	case DW_AT_language:
	  part_die->language = DW_UNSND (&attr);
	  break;
	case DW_AT_external:
	  part_die->is_external = DW_UNSND (&attr);
	  break;
	case DW_AT_declaration:
	  part_die->is_declaration = DW_UNSND (&attr);
	  break;
	case DW_AT_type:
	  part_die->has_type = 1;
	  break;
	case DW_AT_abstract_origin:
	case DW_AT_specification:
	case DW_AT_extension:
	  part_die->has_specification = 1;
	  part_die->spec_offset = dwarf2_get_ref_die_offset (&attr, cu);
	  break;
	case DW_AT_sibling:
	  /* Ignore absolute siblings, they might point outside of
	     the current compile unit.  */
	  if (attr.form == DW_FORM_ref_addr)
	    complaint (&symfile_complaints, _("ignoring absolute DW_AT_sibling"));
	  else
	    part_die->sibling = dwarf2_per_objfile->info_buffer
	      + dwarf2_get_ref_die_offset (&attr, cu);
	  break;
        case DW_AT_stmt_list:
          part_die->has_stmt_list = 1;
          part_die->line_offset = DW_UNSND (&attr);
          break;
        case DW_AT_byte_size:
          part_die->has_byte_size = 1;
          break;
	default:
	  break;
	}
    }

  /* When using the GNU linker, .gnu.linkonce. sections are used to
     eliminate duplicate copies of functions and vtables and such.
     The linker will arbitrarily choose one and discard the others.
     The AT_*_pc values for such functions refer to local labels in
     these sections.  If the section from that file was discarded, the
     labels are not in the output, so the relocs get a value of 0.
     If this is a discarded function, mark the pc bounds as invalid,
     so that GDB will ignore it.  */
  if (has_low_pc_attr && has_high_pc_attr
      && part_die->lowpc < part_die->highpc
      && (part_die->lowpc != 0
	  || dwarf2_per_objfile->has_section_at_zero))
    part_die->has_pc_info = 1;
  return info_ptr;
}

/* Find a cached partial DIE at OFFSET in CU.  */

static struct partial_die_info *
find_partial_die_in_comp_unit (unsigned long offset, struct dwarf2_cu *cu)
{
  struct partial_die_info *lookup_die = NULL;
  struct partial_die_info part_die;

  part_die.offset = offset;
  lookup_die = htab_find_with_hash (cu->partial_dies, &part_die, offset);

  return lookup_die;
}

/* Find a partial DIE at OFFSET, which may or may not be in CU.  */

static struct partial_die_info *
find_partial_die (unsigned long offset, struct dwarf2_cu *cu)
{
  struct dwarf2_per_cu_data *per_cu = NULL;
  struct partial_die_info *pd = NULL;

  if (offset >= cu->header.offset
      && offset < cu->header.offset + cu->header.length)
    {
      pd = find_partial_die_in_comp_unit (offset, cu);
      if (pd != NULL)
	return pd;
    }

  per_cu = dwarf2_find_containing_comp_unit (offset, cu->objfile);

  if (per_cu->cu == NULL)
    {
      load_comp_unit (per_cu, cu->objfile);
      per_cu->cu->read_in_chain = dwarf2_per_objfile->read_in_chain;
      dwarf2_per_objfile->read_in_chain = per_cu;
    }

  per_cu->cu->last_used = 0;
  pd = find_partial_die_in_comp_unit (offset, per_cu->cu);

  if (pd == NULL && per_cu->load_all_dies == 0)
    {
      struct cleanup *back_to;
      struct partial_die_info comp_unit_die;
      struct abbrev_info *abbrev;
      unsigned int bytes_read;
      char *info_ptr;

      per_cu->load_all_dies = 1;

      /* Re-read the DIEs.  */
      back_to = make_cleanup (null_cleanup, 0);
      if (per_cu->cu->dwarf2_abbrevs == NULL)
	{
	  dwarf2_read_abbrevs (per_cu->cu->objfile->obfd, per_cu->cu);
	  back_to = make_cleanup (dwarf2_free_abbrev_table, per_cu->cu);
	}
      info_ptr = per_cu->cu->header.first_die_ptr;
      abbrev = peek_die_abbrev (info_ptr, &bytes_read, per_cu->cu);
      info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
				   per_cu->cu->objfile->obfd, info_ptr,
				   per_cu->cu);
      if (comp_unit_die.has_children)
	load_partial_dies (per_cu->cu->objfile->obfd, info_ptr, 0, per_cu->cu);
      do_cleanups (back_to);

      pd = find_partial_die_in_comp_unit (offset, per_cu->cu);
    }

  if (pd == NULL)
    internal_error (__FILE__, __LINE__,
		    _("could not find partial DIE 0x%lx in cache [from module %s]\n"),
		    offset, bfd_get_filename (cu->objfile->obfd));
  return pd;
}

/* Adjust PART_DIE before generating a symbol for it.  This function
   may set the is_external flag or change the DIE's name.  */

static void
fixup_partial_die (struct partial_die_info *part_die,
		   struct dwarf2_cu *cu)
{
  /* If we found a reference attribute and the DIE has no name, try
     to find a name in the referred to DIE.  */

  if (part_die->name == NULL && part_die->has_specification)
    {
      struct partial_die_info *spec_die;

      spec_die = find_partial_die (part_die->spec_offset, cu);

      fixup_partial_die (spec_die, cu);

      if (spec_die->name)
	{
	  part_die->name = spec_die->name;

	  /* Copy DW_AT_external attribute if it is set.  */
	  if (spec_die->is_external)
	    part_die->is_external = spec_die->is_external;
	}
    }

  /* Set default names for some unnamed DIEs.  */
  if (part_die->name == NULL && (part_die->tag == DW_TAG_structure_type
				 || part_die->tag == DW_TAG_class_type))
    part_die->name = "(anonymous class)";

  if (part_die->name == NULL && part_die->tag == DW_TAG_namespace)
    part_die->name = "(anonymous namespace)";

  if (part_die->tag == DW_TAG_structure_type
      || part_die->tag == DW_TAG_class_type
      || part_die->tag == DW_TAG_union_type)
    guess_structure_name (part_die, cu);
}

/* Read the die from the .debug_info section buffer.  Set DIEP to
   point to a newly allocated die with its information, except for its
   child, sibling, and parent fields.  Set HAS_CHILDREN to tell
   whether the die has children or not.  */

static gdb_byte *
read_full_die (struct die_info **diep, bfd *abfd, gdb_byte *info_ptr,
	       struct dwarf2_cu *cu, int *has_children)
{
  unsigned int abbrev_number, bytes_read, i, offset;
  struct abbrev_info *abbrev;
  struct die_info *die;

  offset = info_ptr - dwarf2_per_objfile->info_buffer;
  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
  info_ptr += bytes_read;
  if (!abbrev_number)
    {
      die = dwarf_alloc_die ();
      die->tag = 0;
      die->abbrev = abbrev_number;
      die->type = NULL;
      *diep = die;
      *has_children = 0;
      return info_ptr;
    }

  abbrev = dwarf2_lookup_abbrev (abbrev_number, cu);
  if (!abbrev)
    {
      error (_("Dwarf Error: could not find abbrev number %d [in module %s]"),
	     abbrev_number,
	     bfd_get_filename (abfd));
    }
  die = dwarf_alloc_die ();
  die->offset = offset;
  die->tag = abbrev->tag;
  die->abbrev = abbrev_number;
  die->type = NULL;

  die->num_attrs = abbrev->num_attrs;
  die->attrs = (struct attribute *)
    xmalloc (die->num_attrs * sizeof (struct attribute));

  for (i = 0; i < abbrev->num_attrs; ++i)
    {
      info_ptr = read_attribute (&die->attrs[i], &abbrev->attrs[i],
				 abfd, info_ptr, cu);

      /* If this attribute is an absolute reference to a different
	 compilation unit, make sure that compilation unit is loaded
	 also.  */
      if (die->attrs[i].form == DW_FORM_ref_addr
	  && (DW_ADDR (&die->attrs[i]) < cu->header.offset
	      || (DW_ADDR (&die->attrs[i])
		  >= cu->header.offset + cu->header.length)))
	{
	  struct dwarf2_per_cu_data *per_cu;
	  per_cu = dwarf2_find_containing_comp_unit (DW_ADDR (&die->attrs[i]),
						     cu->objfile);

	  /* Mark the dependence relation so that we don't flush PER_CU
	     too early.  */
	  dwarf2_add_dependence (cu, per_cu);

	  /* If it's already on the queue, we have nothing to do.  */
	  if (per_cu->queued)
	    continue;

	  /* If the compilation unit is already loaded, just mark it as
	     used.  */
	  if (per_cu->cu != NULL)
	    {
	      per_cu->cu->last_used = 0;
	      continue;
	    }

	  /* Add it to the queue.  */
	  queue_comp_unit (per_cu);
       }
    }

  *diep = die;
  *has_children = abbrev->has_children;
  return info_ptr;
}

/* Read an attribute value described by an attribute form.  */

static gdb_byte *
read_attribute_value (struct attribute *attr, unsigned form,
		      bfd *abfd, gdb_byte *info_ptr,
		      struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  unsigned int bytes_read;
  struct dwarf_block *blk;

  attr->form = form;
  switch (form)
    {
    case DW_FORM_addr:
    case DW_FORM_ref_addr:
      DW_ADDR (attr) = read_address (abfd, info_ptr, cu, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_block2:
      blk = dwarf_alloc_block (cu);
      blk->size = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block4:
      blk = dwarf_alloc_block (cu);
      blk->size = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data2:
      DW_UNSND (attr) = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_data4:
      DW_UNSND (attr) = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_data8:
      DW_UNSND (attr) = read_8_bytes (abfd, info_ptr);
      info_ptr += 8;
      break;
    case DW_FORM_string:
      DW_STRING (attr) = read_string (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_strp:
      DW_STRING (attr) = read_indirect_string (abfd, info_ptr, cu_header,
					       &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_block:
      blk = dwarf_alloc_block (cu);
      blk->size = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block1:
      blk = dwarf_alloc_block (cu);
      blk->size = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data1:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_flag:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_sdata:
      DW_SND (attr) = read_signed_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_udata:
      DW_UNSND (attr) = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_ref1:
      DW_ADDR (attr) = cu->header.offset + read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_ref2:
      DW_ADDR (attr) = cu->header.offset + read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_ref4:
      DW_ADDR (attr) = cu->header.offset + read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_ref8:
      DW_ADDR (attr) = cu->header.offset + read_8_bytes (abfd, info_ptr);
      info_ptr += 8;
      break;
    case DW_FORM_ref_udata:
      DW_ADDR (attr) = (cu->header.offset
			+ read_unsigned_leb128 (abfd, info_ptr, &bytes_read));
      info_ptr += bytes_read;
      break;
    case DW_FORM_indirect:
      form = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      info_ptr = read_attribute_value (attr, form, abfd, info_ptr, cu);
      break;
    default:
      error (_("Dwarf Error: Cannot handle %s in DWARF reader [in module %s]"),
	     dwarf_form_name (form),
	     bfd_get_filename (abfd));
    }
  return info_ptr;
}

/* Read an attribute described by an abbreviated attribute.  */

static gdb_byte *
read_attribute (struct attribute *attr, struct attr_abbrev *abbrev,
		bfd *abfd, gdb_byte *info_ptr, struct dwarf2_cu *cu)
{
  attr->name = abbrev->name;
  return read_attribute_value (attr, abbrev->form, abfd, info_ptr, cu);
}

/* read dwarf information from a buffer */

static unsigned int
read_1_byte (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_8 (abfd, buf);
}

static int
read_1_signed_byte (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_signed_8 (abfd, buf);
}

static unsigned int
read_2_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_16 (abfd, buf);
}

static int
read_2_signed_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_signed_16 (abfd, buf);
}

static unsigned int
read_4_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_32 (abfd, buf);
}

static int
read_4_signed_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_signed_32 (abfd, buf);
}

static unsigned long
read_8_bytes (bfd *abfd, gdb_byte *buf)
{
  return bfd_get_64 (abfd, buf);
}

static CORE_ADDR
read_address (bfd *abfd, gdb_byte *buf, struct dwarf2_cu *cu,
	      unsigned int *bytes_read)
{
  struct comp_unit_head *cu_header = &cu->header;
  CORE_ADDR retval = 0;

  if (cu_header->signed_addr_p)
    {
      switch (cu_header->addr_size)
	{
	case 2:
	  retval = bfd_get_signed_16 (abfd, buf);
	  break;
	case 4:
	  retval = bfd_get_signed_32 (abfd, buf);
	  break;
	case 8:
	  retval = bfd_get_signed_64 (abfd, buf);
	  break;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("read_address: bad switch, signed [in module %s]"),
			  bfd_get_filename (abfd));
	}
    }
  else
    {
      switch (cu_header->addr_size)
	{
	case 2:
	  retval = bfd_get_16 (abfd, buf);
	  break;
	case 4:
	  retval = bfd_get_32 (abfd, buf);
	  break;
	case 8:
	  retval = bfd_get_64 (abfd, buf);
	  break;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("read_address: bad switch, unsigned [in module %s]"),
			  bfd_get_filename (abfd));
	}
    }

  *bytes_read = cu_header->addr_size;
  return retval;
}

/* Read the initial length from a section.  The (draft) DWARF 3
   specification allows the initial length to take up either 4 bytes
   or 12 bytes.  If the first 4 bytes are 0xffffffff, then the next 8
   bytes describe the length and all offsets will be 8 bytes in length
   instead of 4.

   An older, non-standard 64-bit format is also handled by this
   function.  The older format in question stores the initial length
   as an 8-byte quantity without an escape value.  Lengths greater
   than 2^32 aren't very common which means that the initial 4 bytes
   is almost always zero.  Since a length value of zero doesn't make
   sense for the 32-bit format, this initial zero can be considered to
   be an escape value which indicates the presence of the older 64-bit
   format.  As written, the code can't detect (old format) lengths
   greater than 4GB.  If it becomes necessary to handle lengths
   somewhat larger than 4GB, we could allow other small values (such
   as the non-sensical values of 1, 2, and 3) to also be used as
   escape values indicating the presence of the old format.

   The value returned via bytes_read should be used to increment the
   relevant pointer after calling read_initial_length().
   
   As a side effect, this function sets the fields initial_length_size
   and offset_size in cu_header to the values appropriate for the
   length field.  (The format of the initial length field determines
   the width of file offsets to be fetched later with read_offset().)
   
   [ Note:  read_initial_length() and read_offset() are based on the
     document entitled "DWARF Debugging Information Format", revision
     3, draft 8, dated November 19, 2001.  This document was obtained
     from:

	http://reality.sgiweb.org/davea/dwarf3-draft8-011125.pdf
     
     This document is only a draft and is subject to change.  (So beware.)

     Details regarding the older, non-standard 64-bit format were
     determined empirically by examining 64-bit ELF files produced by
     the SGI toolchain on an IRIX 6.5 machine.

     - Kevin, July 16, 2002
   ] */

static LONGEST
read_initial_length (bfd *abfd, gdb_byte *buf, struct comp_unit_head *cu_header,
                     unsigned int *bytes_read)
{
  LONGEST length = bfd_get_32 (abfd, buf);

  if (length == 0xffffffff)
    {
      length = bfd_get_64 (abfd, buf + 4);
      *bytes_read = 12;
    }
  else if (length == 0)
    {
      /* Handle the (non-standard) 64-bit DWARF2 format used by IRIX.  */
      length = bfd_get_64 (abfd, buf);
      *bytes_read = 8;
    }
  else
    {
      *bytes_read = 4;
    }

  if (cu_header)
    {
      gdb_assert (cu_header->initial_length_size == 0
		  || cu_header->initial_length_size == 4
		  || cu_header->initial_length_size == 8
		  || cu_header->initial_length_size == 12);

      if (cu_header->initial_length_size != 0
	  && cu_header->initial_length_size != *bytes_read)
	complaint (&symfile_complaints,
		   _("intermixed 32-bit and 64-bit DWARF sections"));

      cu_header->initial_length_size = *bytes_read;
      cu_header->offset_size = (*bytes_read == 4) ? 4 : 8;
    }

  return length;
}

/* Read an offset from the data stream.  The size of the offset is
   given by cu_header->offset_size.  */

static LONGEST
read_offset (bfd *abfd, gdb_byte *buf, const struct comp_unit_head *cu_header,
             unsigned int *bytes_read)
{
  LONGEST retval = 0;

  switch (cu_header->offset_size)
    {
    case 4:
      retval = bfd_get_32 (abfd, buf);
      *bytes_read = 4;
      break;
    case 8:
      retval = bfd_get_64 (abfd, buf);
      *bytes_read = 8;
      break;
    default:
      internal_error (__FILE__, __LINE__,
		      _("read_offset: bad switch [in module %s]"),
		      bfd_get_filename (abfd));
    }

  return retval;
}

static gdb_byte *
read_n_bytes (bfd *abfd, gdb_byte *buf, unsigned int size)
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the buffer, otherwise we have to copy the data to a buffer
     allocated on the temporary obstack.  */
  gdb_assert (HOST_CHAR_BIT == 8);
  return buf;
}

static char *
read_string (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the string, otherwise we have to copy the string to a buffer
     allocated on the temporary obstack.  */
  gdb_assert (HOST_CHAR_BIT == 8);
  if (*buf == '\0')
    {
      *bytes_read_ptr = 1;
      return NULL;
    }
  *bytes_read_ptr = strlen ((char *) buf) + 1;
  return (char *) buf;
}

static char *
read_indirect_string (bfd *abfd, gdb_byte *buf,
		      const struct comp_unit_head *cu_header,
		      unsigned int *bytes_read_ptr)
{
  LONGEST str_offset = read_offset (abfd, buf, cu_header,
				    bytes_read_ptr);

  if (dwarf2_per_objfile->str_buffer == NULL)
    {
      error (_("DW_FORM_strp used without .debug_str section [in module %s]"),
		      bfd_get_filename (abfd));
      return NULL;
    }
  if (str_offset >= dwarf2_per_objfile->str_size)
    {
      error (_("DW_FORM_strp pointing outside of .debug_str section [in module %s]"),
		      bfd_get_filename (abfd));
      return NULL;
    }
  gdb_assert (HOST_CHAR_BIT == 8);
  if (dwarf2_per_objfile->str_buffer[str_offset] == '\0')
    return NULL;
  return (char *) (dwarf2_per_objfile->str_buffer + str_offset);
}

static unsigned long
read_unsigned_leb128 (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  unsigned long result;
  unsigned int num_read;
  int i, shift;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, buf);
      buf++;
      num_read++;
      result |= ((unsigned long)(byte & 127) << shift);
      if ((byte & 128) == 0)
	{
	  break;
	}
      shift += 7;
    }
  *bytes_read_ptr = num_read;
  return result;
}

static long
read_signed_leb128 (bfd *abfd, gdb_byte *buf, unsigned int *bytes_read_ptr)
{
  long result;
  int i, shift, num_read;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, buf);
      buf++;
      num_read++;
      result |= ((long)(byte & 127) << shift);
      shift += 7;
      if ((byte & 128) == 0)
	{
	  break;
	}
    }
  if ((shift < 8 * sizeof (result)) && (byte & 0x40))
    result |= -(((long)1) << shift);
  *bytes_read_ptr = num_read;
  return result;
}

/* Return a pointer to just past the end of an LEB128 number in BUF.  */

static gdb_byte *
skip_leb128 (bfd *abfd, gdb_byte *buf)
{
  int byte;

  while (1)
    {
      byte = bfd_get_8 (abfd, buf);
      buf++;
      if ((byte & 128) == 0)
	return buf;
    }
}

static void
set_cu_language (unsigned int lang, struct dwarf2_cu *cu)
{
  switch (lang)
    {
    case DW_LANG_C89:
    case DW_LANG_C:
      cu->language = language_c;
      break;
    case DW_LANG_C_plus_plus:
      cu->language = language_cplus;
      break;
    case DW_LANG_Fortran77:
    case DW_LANG_Fortran90:
    case DW_LANG_Fortran95:
      cu->language = language_fortran;
      break;
    case DW_LANG_Mips_Assembler:
      cu->language = language_asm;
      break;
    case DW_LANG_Java:
      cu->language = language_java;
      break;
    case DW_LANG_Ada83:
    case DW_LANG_Ada95:
      cu->language = language_ada;
      break;
    case DW_LANG_Modula2:
      cu->language = language_m2;
      break;
    case DW_LANG_Pascal83:
      cu->language = language_pascal;
      break;
    case DW_LANG_Cobol74:
    case DW_LANG_Cobol85:
    default:
      cu->language = language_minimal;
      break;
    }
  cu->language_defn = language_def (cu->language);
}

/* Return the named attribute or NULL if not there.  */

static struct attribute *
dwarf2_attr (struct die_info *die, unsigned int name, struct dwarf2_cu *cu)
{
  unsigned int i;
  struct attribute *spec = NULL;

  for (i = 0; i < die->num_attrs; ++i)
    {
      if (die->attrs[i].name == name)
	return &die->attrs[i];
      if (die->attrs[i].name == DW_AT_specification
	  || die->attrs[i].name == DW_AT_abstract_origin)
	spec = &die->attrs[i];
    }

  if (spec)
    return dwarf2_attr (follow_die_ref (die, spec, cu), name, cu);

  return NULL;
}

/* Return non-zero iff the attribute NAME is defined for the given DIE,
   and holds a non-zero value.  This function should only be used for
   DW_FORM_flag attributes.  */

static int
dwarf2_flag_true_p (struct die_info *die, unsigned name, struct dwarf2_cu *cu)
{
  struct attribute *attr = dwarf2_attr (die, name, cu);

  return (attr && DW_UNSND (attr));
}

static int
die_is_declaration (struct die_info *die, struct dwarf2_cu *cu)
{
  /* A DIE is a declaration if it has a DW_AT_declaration attribute
     which value is non-zero.  However, we have to be careful with
     DIEs having a DW_AT_specification attribute, because dwarf2_attr()
     (via dwarf2_flag_true_p) follows this attribute.  So we may
     end up accidently finding a declaration attribute that belongs
     to a different DIE referenced by the specification attribute,
     even though the given DIE does not have a declaration attribute.  */
  return (dwarf2_flag_true_p (die, DW_AT_declaration, cu)
	  && dwarf2_attr (die, DW_AT_specification, cu) == NULL);
}

/* Return the die giving the specification for DIE, if there is
   one.  */

static struct die_info *
die_specification (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *spec_attr = dwarf2_attr (die, DW_AT_specification, cu);

  if (spec_attr == NULL)
    return NULL;
  else
    return follow_die_ref (die, spec_attr, cu);
}

/* Free the line_header structure *LH, and any arrays and strings it
   refers to.  */
static void
free_line_header (struct line_header *lh)
{
  if (lh->standard_opcode_lengths)
    xfree (lh->standard_opcode_lengths);

  /* Remember that all the lh->file_names[i].name pointers are
     pointers into debug_line_buffer, and don't need to be freed.  */
  if (lh->file_names)
    xfree (lh->file_names);

  /* Similarly for the include directory names.  */
  if (lh->include_dirs)
    xfree (lh->include_dirs);

  xfree (lh);
}


/* Add an entry to LH's include directory table.  */
static void
add_include_dir (struct line_header *lh, char *include_dir)
{
  /* Grow the array if necessary.  */
  if (lh->include_dirs_size == 0)
    {
      lh->include_dirs_size = 1; /* for testing */
      lh->include_dirs = xmalloc (lh->include_dirs_size
                                  * sizeof (*lh->include_dirs));
    }
  else if (lh->num_include_dirs >= lh->include_dirs_size)
    {
      lh->include_dirs_size *= 2;
      lh->include_dirs = xrealloc (lh->include_dirs,
                                   (lh->include_dirs_size
                                    * sizeof (*lh->include_dirs)));
    }

  lh->include_dirs[lh->num_include_dirs++] = include_dir;
}
 

/* Add an entry to LH's file name table.  */
static void
add_file_name (struct line_header *lh,
               char *name,
               unsigned int dir_index,
               unsigned int mod_time,
               unsigned int length)
{
  struct file_entry *fe;

  /* Grow the array if necessary.  */
  if (lh->file_names_size == 0)
    {
      lh->file_names_size = 1; /* for testing */
      lh->file_names = xmalloc (lh->file_names_size
                                * sizeof (*lh->file_names));
    }
  else if (lh->num_file_names >= lh->file_names_size)
    {
      lh->file_names_size *= 2;
      lh->file_names = xrealloc (lh->file_names,
                                 (lh->file_names_size
                                  * sizeof (*lh->file_names)));
    }

  fe = &lh->file_names[lh->num_file_names++];
  fe->name = name;
  fe->dir_index = dir_index;
  fe->mod_time = mod_time;
  fe->length = length;
  fe->included_p = 0;
  fe->symtab = NULL;
}
 

/* Read the statement program header starting at OFFSET in
   .debug_line, according to the endianness of ABFD.  Return a pointer
   to a struct line_header, allocated using xmalloc.

   NOTE: the strings in the include directory and file name tables of
   the returned object point into debug_line_buffer, and must not be
   freed.  */
static struct line_header *
dwarf_decode_line_header (unsigned int offset, bfd *abfd,
			  struct dwarf2_cu *cu)
{
  struct cleanup *back_to;
  struct line_header *lh;
  gdb_byte *line_ptr;
  unsigned int bytes_read;
  int i;
  char *cur_dir, *cur_file;

  if (dwarf2_per_objfile->line_buffer == NULL)
    {
      complaint (&symfile_complaints, _("missing .debug_line section"));
      return 0;
    }

  /* Make sure that at least there's room for the total_length field.
     That could be 12 bytes long, but we're just going to fudge that.  */
  if (offset + 4 >= dwarf2_per_objfile->line_size)
    {
      dwarf2_statement_list_fits_in_line_number_section_complaint ();
      return 0;
    }

  lh = xmalloc (sizeof (*lh));
  memset (lh, 0, sizeof (*lh));
  back_to = make_cleanup ((make_cleanup_ftype *) free_line_header,
                          (void *) lh);

  line_ptr = dwarf2_per_objfile->line_buffer + offset;

  /* Read in the header.  */
  lh->total_length = 
    read_initial_length (abfd, line_ptr, &cu->header, &bytes_read);
  line_ptr += bytes_read;
  if (line_ptr + lh->total_length > (dwarf2_per_objfile->line_buffer
				     + dwarf2_per_objfile->line_size))
    {
      dwarf2_statement_list_fits_in_line_number_section_complaint ();
      return 0;
    }
  lh->statement_program_end = line_ptr + lh->total_length;
  lh->version = read_2_bytes (abfd, line_ptr);
  line_ptr += 2;
  lh->header_length = read_offset (abfd, line_ptr, &cu->header, &bytes_read);
  line_ptr += bytes_read;
  lh->minimum_instruction_length = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->default_is_stmt = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->line_base = read_1_signed_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->line_range = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->opcode_base = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->standard_opcode_lengths
    = xmalloc (lh->opcode_base * sizeof (lh->standard_opcode_lengths[0]));

  lh->standard_opcode_lengths[0] = 1;  /* This should never be used anyway.  */
  for (i = 1; i < lh->opcode_base; ++i)
    {
      lh->standard_opcode_lengths[i] = read_1_byte (abfd, line_ptr);
      line_ptr += 1;
    }

  /* Read directory table.  */
  while ((cur_dir = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      line_ptr += bytes_read;
      add_include_dir (lh, cur_dir);
    }
  line_ptr += bytes_read;

  /* Read file name table.  */
  while ((cur_file = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      unsigned int dir_index, mod_time, length;

      line_ptr += bytes_read;
      dir_index = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      mod_time = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      length = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;

      add_file_name (lh, cur_file, dir_index, mod_time, length);
    }
  line_ptr += bytes_read;
  lh->statement_program_start = line_ptr; 

  if (line_ptr > (dwarf2_per_objfile->line_buffer
		  + dwarf2_per_objfile->line_size))
    complaint (&symfile_complaints,
	       _("line number info header doesn't fit in `.debug_line' section"));

  discard_cleanups (back_to);
  return lh;
}

/* This function exists to work around a bug in certain compilers
   (particularly GCC 2.95), in which the first line number marker of a
   function does not show up until after the prologue, right before
   the second line number marker.  This function shifts ADDRESS down
   to the beginning of the function if necessary, and is called on
   addresses passed to record_line.  */

static CORE_ADDR
check_cu_functions (CORE_ADDR address, struct dwarf2_cu *cu)
{
  struct function_range *fn;

  /* Find the function_range containing address.  */
  if (!cu->first_fn)
    return address;

  if (!cu->cached_fn)
    cu->cached_fn = cu->first_fn;

  fn = cu->cached_fn;
  while (fn)
    if (fn->lowpc <= address && fn->highpc > address)
      goto found;
    else
      fn = fn->next;

  fn = cu->first_fn;
  while (fn && fn != cu->cached_fn)
    if (fn->lowpc <= address && fn->highpc > address)
      goto found;
    else
      fn = fn->next;

  return address;

 found:
  if (fn->seen_line)
    return address;
  if (address != fn->lowpc)
    complaint (&symfile_complaints,
	       _("misplaced first line number at 0x%lx for '%s'"),
	       (unsigned long) address, fn->name);
  fn->seen_line = 1;
  return fn->lowpc;
}

/* Decode the Line Number Program (LNP) for the given line_header
   structure and CU.  The actual information extracted and the type
   of structures created from the LNP depends on the value of PST.

   1. If PST is NULL, then this procedure uses the data from the program
      to create all necessary symbol tables, and their linetables.
      The compilation directory of the file is passed in COMP_DIR,
      and must not be NULL.
   
   2. If PST is not NULL, this procedure reads the program to determine
      the list of files included by the unit represented by PST, and
      builds all the associated partial symbol tables.  In this case,
      the value of COMP_DIR is ignored, and can thus be NULL (the COMP_DIR
      is not used to compute the full name of the symtab, and therefore
      omitting it when building the partial symtab does not introduce
      the potential for inconsistency - a partial symtab and its associated
      symbtab having a different fullname -).  */

static void
dwarf_decode_lines (struct line_header *lh, char *comp_dir, bfd *abfd,
		    struct dwarf2_cu *cu, struct partial_symtab *pst)
{
  gdb_byte *line_ptr, *extended_end;
  gdb_byte *line_end;
  unsigned int bytes_read, extended_len;
  unsigned char op_code, extended_op, adj_opcode;
  CORE_ADDR baseaddr;
  struct objfile *objfile = cu->objfile;
  const int decode_for_pst_p = (pst != NULL);
  struct subfile *last_subfile = NULL, *first_subfile = current_subfile;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  line_ptr = lh->statement_program_start;
  line_end = lh->statement_program_end;

  /* Read the statement sequences until there's nothing left.  */
  while (line_ptr < line_end)
    {
      /* state machine registers  */
      CORE_ADDR address = 0;
      unsigned int file = 1;
      unsigned int line = 1;
      unsigned int column = 0;
      int is_stmt = lh->default_is_stmt;
      int basic_block = 0;
      int end_sequence = 0;

      if (!decode_for_pst_p && lh->num_file_names >= file)
	{
          /* Start a subfile for the current file of the state machine.  */
	  /* lh->include_dirs and lh->file_names are 0-based, but the
	     directory and file name numbers in the statement program
	     are 1-based.  */
          struct file_entry *fe = &lh->file_names[file - 1];
          char *dir = NULL;

          if (fe->dir_index)
            dir = lh->include_dirs[fe->dir_index - 1];

	  dwarf2_start_subfile (fe->name, dir, comp_dir);
	}

      /* Decode the table.  */
      while (!end_sequence)
	{
	  op_code = read_1_byte (abfd, line_ptr);
	  line_ptr += 1;

	  if (op_code >= lh->opcode_base)
	    {		
	      /* Special operand.  */
	      adj_opcode = op_code - lh->opcode_base;
	      address += (adj_opcode / lh->line_range)
		* lh->minimum_instruction_length;
	      line += lh->line_base + (adj_opcode % lh->line_range);
	      if (lh->num_file_names < file)
		dwarf2_debug_line_missing_file_complaint ();
	      else
		{
		  lh->file_names[file - 1].included_p = 1;
		  if (!decode_for_pst_p)
                    {
                      if (last_subfile != current_subfile)
                        {
                          if (last_subfile)
                            record_line (last_subfile, 0, address);
                          last_subfile = current_subfile;
                        }
		      /* Append row to matrix using current values.  */
		      record_line (current_subfile, line, 
				   check_cu_functions (address, cu));
		    }
		}
	      basic_block = 1;
	    }
	  else switch (op_code)
	    {
	    case DW_LNS_extended_op:
	      extended_len = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      extended_end = line_ptr + extended_len;
	      extended_op = read_1_byte (abfd, line_ptr);
	      line_ptr += 1;
	      switch (extended_op)
		{
		case DW_LNE_end_sequence:
		  end_sequence = 1;

		  if (lh->num_file_names < file)
		    dwarf2_debug_line_missing_file_complaint ();
		  else
		    {
		      lh->file_names[file - 1].included_p = 1;
		      if (!decode_for_pst_p)
			record_line (current_subfile, 0, address);
		    }
		  break;
		case DW_LNE_set_address:
		  address = read_address (abfd, line_ptr, cu, &bytes_read);
		  line_ptr += bytes_read;
		  address += baseaddr;
		  break;
		case DW_LNE_define_file:
                  {
                    char *cur_file;
                    unsigned int dir_index, mod_time, length;
                    
                    cur_file = read_string (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    dir_index =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    mod_time =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    length =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    add_file_name (lh, cur_file, dir_index, mod_time, length);
                  }
		  break;
		default:
		  complaint (&symfile_complaints,
			     _("mangled .debug_line section"));
		  return;
		}
	      /* Make sure that we parsed the extended op correctly.  If e.g.
		 we expected a different address size than the producer used,
		 we may have read the wrong number of bytes.  */
	      if (line_ptr != extended_end)
		{
		  complaint (&symfile_complaints,
			     _("mangled .debug_line section"));
		  return;
		}
	      break;
	    case DW_LNS_copy:
	      if (lh->num_file_names < file)
		dwarf2_debug_line_missing_file_complaint ();
	      else
		{
		  lh->file_names[file - 1].included_p = 1;
		  if (!decode_for_pst_p)
                    {
                      if (last_subfile != current_subfile)
                        {
                          if (last_subfile)
                            record_line (last_subfile, 0, address);
                          last_subfile = current_subfile;
                        }
                      record_line (current_subfile, line, 
                                   check_cu_functions (address, cu));
                    }
		}
	      basic_block = 0;
	      break;
	    case DW_LNS_advance_pc:
	      address += lh->minimum_instruction_length
		* read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_advance_line:
	      line += read_signed_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_set_file:
              {
                /* The arrays lh->include_dirs and lh->file_names are
                   0-based, but the directory and file name numbers in
                   the statement program are 1-based.  */
                struct file_entry *fe;
                char *dir = NULL;

                file = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                line_ptr += bytes_read;
                if (lh->num_file_names < file)
                  dwarf2_debug_line_missing_file_complaint ();
                else
                  {
                    fe = &lh->file_names[file - 1];
                    if (fe->dir_index)
                      dir = lh->include_dirs[fe->dir_index - 1];
                    if (!decode_for_pst_p)
                      {
                        last_subfile = current_subfile;
                        dwarf2_start_subfile (fe->name, dir, comp_dir);
                      }
                  }
              }
	      break;
	    case DW_LNS_set_column:
	      column = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_negate_stmt:
	      is_stmt = (!is_stmt);
	      break;
	    case DW_LNS_set_basic_block:
	      basic_block = 1;
	      break;
	    /* Add to the address register of the state machine the
	       address increment value corresponding to special opcode
	       255.  I.e., this value is scaled by the minimum
	       instruction length since special opcode 255 would have
	       scaled the the increment.  */
	    case DW_LNS_const_add_pc:
	      address += (lh->minimum_instruction_length
			  * ((255 - lh->opcode_base) / lh->line_range));
	      break;
	    case DW_LNS_fixed_advance_pc:
	      address += read_2_bytes (abfd, line_ptr);
	      line_ptr += 2;
	      break;
	    default:
	      {
		/* Unknown standard opcode, ignore it.  */
		int i;

		for (i = 0; i < lh->standard_opcode_lengths[op_code]; i++)
		  {
		    (void) read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		    line_ptr += bytes_read;
		  }
	      }
	    }
	}
    }

  if (decode_for_pst_p)
    {
      int file_index;

      /* Now that we're done scanning the Line Header Program, we can
         create the psymtab of each included file.  */
      for (file_index = 0; file_index < lh->num_file_names; file_index++)
        if (lh->file_names[file_index].included_p == 1)
          {
            const struct file_entry fe = lh->file_names [file_index];
            char *include_name = fe.name;
            char *dir_name = NULL;
            char *pst_filename = pst->filename;

            if (fe.dir_index)
              dir_name = lh->include_dirs[fe.dir_index - 1];

            if (!IS_ABSOLUTE_PATH (include_name) && dir_name != NULL)
              {
                include_name = concat (dir_name, SLASH_STRING,
				       include_name, (char *)NULL);
                make_cleanup (xfree, include_name);
              }

            if (!IS_ABSOLUTE_PATH (pst_filename) && pst->dirname != NULL)
              {
                pst_filename = concat (pst->dirname, SLASH_STRING,
				       pst_filename, (char *)NULL);
                make_cleanup (xfree, pst_filename);
              }

            if (strcmp (include_name, pst_filename) != 0)
              dwarf2_create_include_psymtab (include_name, pst, objfile);
          }
    }
  else
    {
      /* Make sure a symtab is created for every file, even files
	 which contain only variables (i.e. no code with associated
	 line numbers).  */

      int i;
      struct file_entry *fe;

      for (i = 0; i < lh->num_file_names; i++)
	{
	  char *dir = NULL;
	  fe = &lh->file_names[i];
	  if (fe->dir_index)
	    dir = lh->include_dirs[fe->dir_index - 1];
	  dwarf2_start_subfile (fe->name, dir, comp_dir);

	  /* Skip the main file; we don't need it, and it must be
	     allocated last, so that it will show up before the
	     non-primary symtabs in the objfile's symtab list.  */
	  if (current_subfile == first_subfile)
	    continue;

	  if (current_subfile->symtab == NULL)
	    current_subfile->symtab = allocate_symtab (current_subfile->name,
						       cu->objfile);
	  fe->symtab = current_subfile->symtab;
	}
    }
}

/* Start a subfile for DWARF.  FILENAME is the name of the file and
   DIRNAME the name of the source directory which contains FILENAME
   or NULL if not known.  COMP_DIR is the compilation directory for the
   linetable's compilation unit or NULL if not known.
   This routine tries to keep line numbers from identical absolute and
   relative file names in a common subfile.

   Using the `list' example from the GDB testsuite, which resides in
   /srcdir and compiling it with Irix6.2 cc in /compdir using a filename
   of /srcdir/list0.c yields the following debugging information for list0.c:

   DW_AT_name:          /srcdir/list0.c
   DW_AT_comp_dir:              /compdir
   files.files[0].name: list0.h
   files.files[0].dir:  /srcdir
   files.files[1].name: list0.c
   files.files[1].dir:  /srcdir

   The line number information for list0.c has to end up in a single
   subfile, so that `break /srcdir/list0.c:1' works as expected.
   start_subfile will ensure that this happens provided that we pass the
   concatenation of files.files[1].dir and files.files[1].name as the
   subfile's name.  */

static void
dwarf2_start_subfile (char *filename, char *dirname, char *comp_dir)
{
  char *fullname;

  /* While reading the DIEs, we call start_symtab(DW_AT_name, DW_AT_comp_dir).
     `start_symtab' will always pass the contents of DW_AT_comp_dir as
     second argument to start_subfile.  To be consistent, we do the
     same here.  In order not to lose the line information directory,
     we concatenate it to the filename when it makes sense.
     Note that the Dwarf3 standard says (speaking of filenames in line
     information): ``The directory index is ignored for file names
     that represent full path names''.  Thus ignoring dirname in the
     `else' branch below isn't an issue.  */

  if (!IS_ABSOLUTE_PATH (filename) && dirname != NULL)
    fullname = concat (dirname, SLASH_STRING, filename, (char *)NULL);
  else
    fullname = filename;

  start_subfile (fullname, comp_dir);

  if (fullname != filename)
    xfree (fullname);
}

static void
var_decode_location (struct attribute *attr, struct symbol *sym,
		     struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;

  /* NOTE drow/2003-01-30: There used to be a comment and some special
     code here to turn a symbol with DW_AT_external and a
     SYMBOL_VALUE_ADDRESS of 0 into a LOC_UNRESOLVED symbol.  This was
     necessary for platforms (maybe Alpha, certainly PowerPC GNU/Linux
     with some versions of binutils) where shared libraries could have
     relocations against symbols in their debug information - the
     minimal symbol would have the right address, but the debug info
     would not.  It's no longer necessary, because we will explicitly
     apply relocations when we read in the debug information now.  */

  /* A DW_AT_location attribute with no contents indicates that a
     variable has been optimized away.  */
  if (attr_form_is_block (attr) && DW_BLOCK (attr)->size == 0)
    {
      SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
      return;
    }

  /* Handle one degenerate form of location expression specially, to
     preserve GDB's previous behavior when section offsets are
     specified.  If this is just a DW_OP_addr then mark this symbol
     as LOC_STATIC.  */

  if (attr_form_is_block (attr)
      && DW_BLOCK (attr)->size == 1 + cu_header->addr_size
      && DW_BLOCK (attr)->data[0] == DW_OP_addr)
    {
      unsigned int dummy;

      SYMBOL_VALUE_ADDRESS (sym) =
	read_address (objfile->obfd, DW_BLOCK (attr)->data + 1, cu, &dummy);
      fixup_symbol_section (sym, objfile);
      SYMBOL_VALUE_ADDRESS (sym) += ANOFFSET (objfile->section_offsets,
					      SYMBOL_SECTION (sym));
      SYMBOL_CLASS (sym) = LOC_STATIC;
      return;
    }

  /* NOTE drow/2002-01-30: It might be worthwhile to have a static
     expression evaluator, and use LOC_COMPUTED only when necessary
     (i.e. when the value of a register or memory location is
     referenced, or a thread-local block, etc.).  Then again, it might
     not be worthwhile.  I'm assuming that it isn't unless performance
     or memory numbers show me otherwise.  */

  dwarf2_symbol_mark_computed (attr, sym, cu);
  SYMBOL_CLASS (sym) = LOC_COMPUTED;
}

/* Given a pointer to a DWARF information entry, figure out if we need
   to make a symbol table entry for it, and if so, create a new entry
   and return a pointer to it.
   If TYPE is NULL, determine symbol type from the die, otherwise
   used the passed type.  */

static struct symbol *
new_symbol (struct die_info *die, struct type *type, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct symbol *sym = NULL;
  char *name;
  struct attribute *attr = NULL;
  struct attribute *attr2 = NULL;
  CORE_ADDR baseaddr;

  baseaddr = ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

  if (die->tag != DW_TAG_namespace)
    name = dwarf2_linkage_name (die, cu);
  else
    name = TYPE_NAME (type);

  if (name)
    {
      sym = (struct symbol *) obstack_alloc (&objfile->objfile_obstack,
					     sizeof (struct symbol));
      OBJSTAT (objfile, n_syms++);
      memset (sym, 0, sizeof (struct symbol));

      /* Cache this symbol's name and the name's demangled form (if any).  */
      SYMBOL_LANGUAGE (sym) = cu->language;
      SYMBOL_SET_NAMES (sym, name, strlen (name), objfile);

      /* Default assumptions.
         Use the passed type or decode it from the die.  */
      SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
      SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
      if (type != NULL)
	SYMBOL_TYPE (sym) = type;
      else
	SYMBOL_TYPE (sym) = die_type (die, cu);
      attr = dwarf2_attr (die, DW_AT_decl_line, cu);
      if (attr)
	{
	  SYMBOL_LINE (sym) = DW_UNSND (attr);
	}

      attr = dwarf2_attr (die, DW_AT_decl_file, cu);
      if (attr)
	{
	  int file_index = DW_UNSND (attr);
	  if (cu->line_header == NULL
	      || file_index > cu->line_header->num_file_names)
	    complaint (&symfile_complaints,
		       _("file index out of range"));
	  else if (file_index > 0)
	    {
	      struct file_entry *fe;
	      fe = &cu->line_header->file_names[file_index - 1];
	      SYMBOL_SYMTAB (sym) = fe->symtab;
	    }
	}

      switch (die->tag)
	{
	case DW_TAG_label:
	  attr = dwarf2_attr (die, DW_AT_low_pc, cu);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) = DW_ADDR (attr) + baseaddr;
	    }
	  SYMBOL_CLASS (sym) = LOC_LABEL;
	  break;
	case DW_TAG_subprogram:
	  /* SYMBOL_BLOCK_VALUE (sym) will be filled in later by
	     finish_block.  */
	  SYMBOL_CLASS (sym) = LOC_BLOCK;
	  attr2 = dwarf2_attr (die, DW_AT_external, cu);
	  if (attr2 && (DW_UNSND (attr2) != 0))
	    {
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  else
	    {
	      add_symbol_to_list (sym, cu->list_in_scope);
	    }
	  break;
	case DW_TAG_variable:
	  /* Compilation with minimal debug info may result in variables
	     with missing type entries. Change the misleading `void' type
	     to something sensible.  */
	  if (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_VOID)
	    SYMBOL_TYPE (sym)
	      = builtin_type (current_gdbarch)->nodebug_data_symbol;

	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0))
		add_symbol_to_list (sym, &global_symbols);
	      else
		add_symbol_to_list (sym, cu->list_in_scope);
	      break;
	    }
	  attr = dwarf2_attr (die, DW_AT_location, cu);
	  if (attr)
	    {
	      var_decode_location (attr, sym, cu);
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0))
		add_symbol_to_list (sym, &global_symbols);
	      else
		add_symbol_to_list (sym, cu->list_in_scope);
	    }
	  else
	    {
	      /* We do not know the address of this symbol.
	         If it is an external symbol and we have type information
	         for it, enter the symbol as a LOC_UNRESOLVED symbol.
	         The address of the variable will then be determined from
	         the minimal symbol table whenever the variable is
	         referenced.  */
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0)
		  && dwarf2_attr (die, DW_AT_type, cu) != NULL)
		{
		  SYMBOL_CLASS (sym) = LOC_UNRESOLVED;
		  add_symbol_to_list (sym, &global_symbols);
		}
	    }
	  break;
	case DW_TAG_formal_parameter:
	  attr = dwarf2_attr (die, DW_AT_location, cu);
	  if (attr)
	    {
	      var_decode_location (attr, sym, cu);
	      /* FIXME drow/2003-07-31: Is LOC_COMPUTED_ARG necessary?  */
	      if (SYMBOL_CLASS (sym) == LOC_COMPUTED)
		SYMBOL_CLASS (sym) = LOC_COMPUTED_ARG;
	    }
	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	    }
	  add_symbol_to_list (sym, cu->list_in_scope);
	  break;
	case DW_TAG_unspecified_parameters:
	  /* From varargs functions; gdb doesn't seem to have any
	     interest in this information, so just ignore it for now.
	     (FIXME?) */
	  break;
	case DW_TAG_class_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
	case DW_TAG_set_type:
	case DW_TAG_enumeration_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = STRUCT_DOMAIN;

	  /* Make sure that the symbol includes appropriate enclosing
	     classes/namespaces in its name.  These are calculated in
	     read_structure_type, and the correct name is saved in
	     the type.  */

	  if (cu->language == language_cplus
	      || cu->language == language_java)
	    {
	      struct type *type = SYMBOL_TYPE (sym);
	      
	      if (TYPE_TAG_NAME (type) != NULL)
		{
		  /* FIXME: carlton/2003-11-10: Should this use
		     SYMBOL_SET_NAMES instead?  (The same problem also
		     arises further down in this function.)  */
		  /* The type's name is already allocated along with
		     this objfile, so we don't need to duplicate it
		     for the symbol.  */
		  SYMBOL_LINKAGE_NAME (sym) = TYPE_TAG_NAME (type);
		}
	    }

	  {
	    /* NOTE: carlton/2003-11-10: C++ and Java class symbols shouldn't
	       really ever be static objects: otherwise, if you try
	       to, say, break of a class's method and you're in a file
	       which doesn't mention that class, it won't work unless
	       the check for all static symbols in lookup_symbol_aux
	       saves you.  See the OtherFileClass tests in
	       gdb.c++/namespace.exp.  */

	    struct pending **list_to_add;

	    list_to_add = (cu->list_in_scope == &file_symbols
			   && (cu->language == language_cplus
			       || cu->language == language_java)
			   ? &global_symbols : cu->list_in_scope);
	  
	    add_symbol_to_list (sym, list_to_add);

	    /* The semantics of C++ state that "struct foo { ... }" also
	       defines a typedef for "foo".  A Java class declaration also
	       defines a typedef for the class.  Synthesize a typedef symbol
	       so that "ptype foo" works as expected.  */
	    if (cu->language == language_cplus
		|| cu->language == language_java
		|| cu->language == language_ada)
	      {
		struct symbol *typedef_sym = (struct symbol *)
		  obstack_alloc (&objfile->objfile_obstack,
				 sizeof (struct symbol));
		*typedef_sym = *sym;
		SYMBOL_DOMAIN (typedef_sym) = VAR_DOMAIN;
		/* The symbol's name is already allocated along with
		   this objfile, so we don't need to duplicate it for
		   the type.  */
		if (TYPE_NAME (SYMBOL_TYPE (sym)) == 0)
		  TYPE_NAME (SYMBOL_TYPE (sym)) = SYMBOL_SEARCH_NAME (sym);
		add_symbol_to_list (typedef_sym, list_to_add);
	      }
	  }
	  break;
	case DW_TAG_typedef:
	  if (processing_has_namespace_info
	      && processing_current_prefix[0] != '\0')
	    {
	      SYMBOL_LINKAGE_NAME (sym) = typename_concat (&objfile->objfile_obstack,
							   processing_current_prefix,
							   name, cu);
	    }
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
	  add_symbol_to_list (sym, cu->list_in_scope);
	  break;
	case DW_TAG_base_type:
        case DW_TAG_subrange_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
	  add_symbol_to_list (sym, cu->list_in_scope);
	  break;
	case DW_TAG_enumerator:
	  if (processing_has_namespace_info
	      && processing_current_prefix[0] != '\0')
	    {
	      SYMBOL_LINKAGE_NAME (sym) = typename_concat (&objfile->objfile_obstack,
							   processing_current_prefix,
							   name, cu);
	    }
	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	    }
	  {
	    /* NOTE: carlton/2003-11-10: See comment above in the
	       DW_TAG_class_type, etc. block.  */

	    struct pending **list_to_add;

	    list_to_add = (cu->list_in_scope == &file_symbols
			   && (cu->language == language_cplus
			       || cu->language == language_java)
			   ? &global_symbols : cu->list_in_scope);
	  
	    add_symbol_to_list (sym, list_to_add);
	  }
	  break;
	case DW_TAG_namespace:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  add_symbol_to_list (sym, &global_symbols);
	  break;
	default:
	  /* Not a tag we recognize.  Hopefully we aren't processing
	     trash data, but since we must specifically ignore things
	     we don't recognize, there is nothing else we should do at
	     this point. */
	  complaint (&symfile_complaints, _("unsupported tag: '%s'"),
		     dwarf_tag_name (die->tag));
	  break;
	}
    }
  return (sym);
}

/* Copy constant value from an attribute to a symbol.  */

static void
dwarf2_const_value (struct attribute *attr, struct symbol *sym,
		    struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;
  struct dwarf_block *blk;

  switch (attr->form)
    {
    case DW_FORM_addr:
      if (TYPE_LENGTH (SYMBOL_TYPE (sym)) != cu_header->addr_size)
	dwarf2_const_value_length_mismatch_complaint (DEPRECATED_SYMBOL_NAME (sym),
						      cu_header->addr_size,
						      TYPE_LENGTH (SYMBOL_TYPE
								   (sym)));
      SYMBOL_VALUE_BYTES (sym) = 
	obstack_alloc (&objfile->objfile_obstack, cu_header->addr_size);
      /* NOTE: cagney/2003-05-09: In-lined store_address call with
         it's body - store_unsigned_integer.  */
      store_unsigned_integer (SYMBOL_VALUE_BYTES (sym), cu_header->addr_size,
			      DW_ADDR (attr));
      SYMBOL_CLASS (sym) = LOC_CONST_BYTES;
      break;
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
      blk = DW_BLOCK (attr);
      if (TYPE_LENGTH (SYMBOL_TYPE (sym)) != blk->size)
	dwarf2_const_value_length_mismatch_complaint (DEPRECATED_SYMBOL_NAME (sym),
						      blk->size,
						      TYPE_LENGTH (SYMBOL_TYPE
								   (sym)));
      SYMBOL_VALUE_BYTES (sym) =
	obstack_alloc (&objfile->objfile_obstack, blk->size);
      memcpy (SYMBOL_VALUE_BYTES (sym), blk->data, blk->size);
      SYMBOL_CLASS (sym) = LOC_CONST_BYTES;
      break;

      /* The DW_AT_const_value attributes are supposed to carry the
	 symbol's value "represented as it would be on the target
	 architecture."  By the time we get here, it's already been
	 converted to host endianness, so we just need to sign- or
	 zero-extend it as appropriate.  */
    case DW_FORM_data1:
      dwarf2_const_value_data (attr, sym, 8);
      break;
    case DW_FORM_data2:
      dwarf2_const_value_data (attr, sym, 16);
      break;
    case DW_FORM_data4:
      dwarf2_const_value_data (attr, sym, 32);
      break;
    case DW_FORM_data8:
      dwarf2_const_value_data (attr, sym, 64);
      break;

    case DW_FORM_sdata:
      SYMBOL_VALUE (sym) = DW_SND (attr);
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;

    case DW_FORM_udata:
      SYMBOL_VALUE (sym) = DW_UNSND (attr);
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;

    default:
      complaint (&symfile_complaints,
		 _("unsupported const value attribute form: '%s'"),
		 dwarf_form_name (attr->form));
      SYMBOL_VALUE (sym) = 0;
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;
    }
}


/* Given an attr with a DW_FORM_dataN value in host byte order, sign-
   or zero-extend it as appropriate for the symbol's type.  */
static void
dwarf2_const_value_data (struct attribute *attr,
			 struct symbol *sym,
			 int bits)
{
  LONGEST l = DW_UNSND (attr);

  if (bits < sizeof (l) * 8)
    {
      if (TYPE_UNSIGNED (SYMBOL_TYPE (sym)))
	l &= ((LONGEST) 1 << bits) - 1;
      else
	l = (l << (sizeof (l) * 8 - bits)) >> (sizeof (l) * 8 - bits);
    }

  SYMBOL_VALUE (sym) = l;
  SYMBOL_CLASS (sym) = LOC_CONST;
}


/* Return the type of the die in question using its DW_AT_type attribute.  */

static struct type *
die_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type;
  struct attribute *type_attr;
  struct die_info *type_die;

  type_attr = dwarf2_attr (die, DW_AT_type, cu);
  if (!type_attr)
    {
      /* A missing DW_AT_type represents a void type.  */
      return dwarf2_fundamental_type (cu->objfile, FT_VOID, cu);
    }
  else
    type_die = follow_die_ref (die, type_attr, cu);

  type = tag_type_to_type (type_die, cu);
  if (!type)
    {
      dump_die (type_die);
      error (_("Dwarf Error: Problem turning type die at offset into gdb type [in module %s]"),
		      cu->objfile->name);
    }
  return type;
}

/* Return the containing type of the die in question using its
   DW_AT_containing_type attribute.  */

static struct type *
die_containing_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type = NULL;
  struct attribute *type_attr;
  struct die_info *type_die = NULL;

  type_attr = dwarf2_attr (die, DW_AT_containing_type, cu);
  if (type_attr)
    {
      type_die = follow_die_ref (die, type_attr, cu);
      type = tag_type_to_type (type_die, cu);
    }
  if (!type)
    {
      if (type_die)
	dump_die (type_die);
      error (_("Dwarf Error: Problem turning containing type into gdb type [in module %s]"), 
		      cu->objfile->name);
    }
  return type;
}

static struct type *
tag_type_to_type (struct die_info *die, struct dwarf2_cu *cu)
{
  if (die->type)
    {
      return die->type;
    }
  else
    {
      read_type_die (die, cu);
      if (!die->type)
	{
	  dump_die (die);
	  error (_("Dwarf Error: Cannot find type of die [in module %s]"), 
			  cu->objfile->name);
	}
      return die->type;
    }
}

static void
read_type_die (struct die_info *die, struct dwarf2_cu *cu)
{
  char *prefix = determine_prefix (die, cu);
  const char *old_prefix = processing_current_prefix;
  struct cleanup *back_to = make_cleanup (xfree, prefix);
  processing_current_prefix = prefix;
  
  switch (die->tag)
    {
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_type (die, cu);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration_type (die, cu);
      break;
    case DW_TAG_subprogram:
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, cu);
      break;
    case DW_TAG_array_type:
      read_array_type (die, cu);
      break;
    case DW_TAG_set_type:
      read_set_type (die, cu);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, cu);
      break;
    case DW_TAG_ptr_to_member_type:
      read_tag_ptr_to_member_type (die, cu);
      break;
    case DW_TAG_reference_type:
      read_tag_reference_type (die, cu);
      break;
    case DW_TAG_const_type:
      read_tag_const_type (die, cu);
      break;
    case DW_TAG_volatile_type:
      read_tag_volatile_type (die, cu);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, cu);
      break;
    case DW_TAG_typedef:
      read_typedef (die, cu);
      break;
    case DW_TAG_subrange_type:
      read_subrange_type (die, cu);
      break;
    case DW_TAG_base_type:
      read_base_type (die, cu);
      break;
    case DW_TAG_unspecified_type:
      read_unspecified_type (die, cu);
      break;
    default:
      complaint (&symfile_complaints, _("unexpected tag in read_type_die: '%s'"),
		 dwarf_tag_name (die->tag));
      break;
    }

  processing_current_prefix = old_prefix;
  do_cleanups (back_to);
}

/* Return the name of the namespace/class that DIE is defined within,
   or "" if we can't tell.  The caller should xfree the result.  */

/* NOTE: carlton/2004-01-23: See read_func_scope (and the comment
   therein) for an example of how to use this function to deal with
   DW_AT_specification.  */

static char *
determine_prefix (struct die_info *die, struct dwarf2_cu *cu)
{
  struct die_info *parent;

  if (cu->language != language_cplus
      && cu->language != language_java)
    return NULL;

  parent = die->parent;

  if (parent == NULL)
    {
      return xstrdup ("");
    }
  else
    {
      switch (parent->tag) {
      case DW_TAG_namespace:
	{
	  /* FIXME: carlton/2004-03-05: Should I follow extension dies
	     before doing this check?  */
	  if (parent->type != NULL && TYPE_TAG_NAME (parent->type) != NULL)
	    {
	      return xstrdup (TYPE_TAG_NAME (parent->type));
	    }
	  else
	    {
	      int dummy;
	      char *parent_prefix = determine_prefix (parent, cu);
	      char *retval = typename_concat (NULL, parent_prefix,
					      namespace_name (parent, &dummy,
							      cu),
					      cu);
	      xfree (parent_prefix);
	      return retval;
	    }
	}
	break;
      case DW_TAG_class_type:
      case DW_TAG_structure_type:
	{
	  if (parent->type != NULL && TYPE_TAG_NAME (parent->type) != NULL)
	    {
	      return xstrdup (TYPE_TAG_NAME (parent->type));
	    }
	  else
	    {
	      const char *old_prefix = processing_current_prefix;
	      char *new_prefix = determine_prefix (parent, cu);
	      char *retval;

	      processing_current_prefix = new_prefix;
	      retval = determine_class_name (parent, cu);
	      processing_current_prefix = old_prefix;

	      xfree (new_prefix);
	      return retval;
	    }
	}
      default:
	return determine_prefix (parent, cu);
      }
    }
}

/* Return a newly-allocated string formed by concatenating PREFIX and
   SUFFIX with appropriate separator.  If PREFIX or SUFFIX is NULL or empty, then
   simply copy the SUFFIX or PREFIX, respectively.  If OBS is non-null,
   perform an obconcat, otherwise allocate storage for the result.  The CU argument
   is used to determine the language and hence, the appropriate separator.  */

#define MAX_SEP_LEN 2  /* sizeof ("::")  */

static char *
typename_concat (struct obstack *obs, const char *prefix, const char *suffix, 
		 struct dwarf2_cu *cu)
{
  char *sep;

  if (suffix == NULL || suffix[0] == '\0' || prefix == NULL || prefix[0] == '\0')
    sep = "";
  else if (cu->language == language_java)
    sep = ".";
  else
    sep = "::";

  if (obs == NULL)
    {
      char *retval = xmalloc (strlen (prefix) + MAX_SEP_LEN + strlen (suffix) + 1);
      retval[0] = '\0';
      
      if (prefix)
	{
	  strcpy (retval, prefix);
	  strcat (retval, sep);
	}
      if (suffix)
	strcat (retval, suffix);
      
      return retval;
    }
  else
    {
      /* We have an obstack.  */
      return obconcat (obs, prefix, sep, suffix);
    }
}

static struct type *
dwarf_base_type (int encoding, int size, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;

  /* FIXME - this should not produce a new (struct type *)
     every time.  It should cache base types.  */
  struct type *type;
  switch (encoding)
    {
    case DW_ATE_address:
      type = dwarf2_fundamental_type (objfile, FT_VOID, cu);
      return type;
    case DW_ATE_boolean:
      type = dwarf2_fundamental_type (objfile, FT_BOOLEAN, cu);
      return type;
    case DW_ATE_complex_float:
      if (size == 16)
	{
	  type = dwarf2_fundamental_type (objfile, FT_DBL_PREC_COMPLEX, cu);
	}
      else
	{
	  type = dwarf2_fundamental_type (objfile, FT_COMPLEX, cu);
	}
      return type;
    case DW_ATE_float:
      if (size == 8)
	{
	  type = dwarf2_fundamental_type (objfile, FT_DBL_PREC_FLOAT, cu);
	}
      else
	{
	  type = dwarf2_fundamental_type (objfile, FT_FLOAT, cu);
	}
      return type;
    case DW_ATE_signed:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_CHAR, cu);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_SHORT, cu);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_INTEGER, cu);
	  break;
	}
      return type;
    case DW_ATE_signed_char:
      type = dwarf2_fundamental_type (objfile, FT_SIGNED_CHAR, cu);
      return type;
    case DW_ATE_unsigned:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_CHAR, cu);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_SHORT, cu);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_INTEGER, cu);
	  break;
	}
      return type;
    case DW_ATE_unsigned_char:
      type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_CHAR, cu);
      return type;
    default:
      type = dwarf2_fundamental_type (objfile, FT_SIGNED_INTEGER, cu);
      return type;
    }
}

#if 0
struct die_info *
copy_die (struct die_info *old_die)
{
  struct die_info *new_die;
  int i, num_attrs;

  new_die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (new_die, 0, sizeof (struct die_info));

  new_die->tag = old_die->tag;
  new_die->has_children = old_die->has_children;
  new_die->abbrev = old_die->abbrev;
  new_die->offset = old_die->offset;
  new_die->type = NULL;

  num_attrs = old_die->num_attrs;
  new_die->num_attrs = num_attrs;
  new_die->attrs = (struct attribute *)
    xmalloc (num_attrs * sizeof (struct attribute));

  for (i = 0; i < old_die->num_attrs; ++i)
    {
      new_die->attrs[i].name = old_die->attrs[i].name;
      new_die->attrs[i].form = old_die->attrs[i].form;
      new_die->attrs[i].u.addr = old_die->attrs[i].u.addr;
    }

  new_die->next = NULL;
  return new_die;
}
#endif

/* Return sibling of die, NULL if no sibling.  */

static struct die_info *
sibling_die (struct die_info *die)
{
  return die->sibling;
}

/* Get linkage name of a die, return NULL if not found.  */

static char *
dwarf2_linkage_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_MIPS_linkage_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  return NULL;
}

/* Get name of a die, return NULL if not found.  */

static char *
dwarf2_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  return NULL;
}

/* Return the die that this die in an extension of, or NULL if there
   is none.  */

static struct die_info *
dwarf2_extension (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_extension, cu);
  if (attr == NULL)
    return NULL;

  return follow_die_ref (die, attr, cu);
}

/* Convert a DIE tag into its string name.  */

static char *
dwarf_tag_name (unsigned tag)
{
  switch (tag)
    {
    case DW_TAG_padding:
      return "DW_TAG_padding";
    case DW_TAG_array_type:
      return "DW_TAG_array_type";
    case DW_TAG_class_type:
      return "DW_TAG_class_type";
    case DW_TAG_entry_point:
      return "DW_TAG_entry_point";
    case DW_TAG_enumeration_type:
      return "DW_TAG_enumeration_type";
    case DW_TAG_formal_parameter:
      return "DW_TAG_formal_parameter";
    case DW_TAG_imported_declaration:
      return "DW_TAG_imported_declaration";
    case DW_TAG_label:
      return "DW_TAG_label";
    case DW_TAG_lexical_block:
      return "DW_TAG_lexical_block";
    case DW_TAG_member:
      return "DW_TAG_member";
    case DW_TAG_pointer_type:
      return "DW_TAG_pointer_type";
    case DW_TAG_reference_type:
      return "DW_TAG_reference_type";
    case DW_TAG_compile_unit:
      return "DW_TAG_compile_unit";
    case DW_TAG_string_type:
      return "DW_TAG_string_type";
    case DW_TAG_structure_type:
      return "DW_TAG_structure_type";
    case DW_TAG_subroutine_type:
      return "DW_TAG_subroutine_type";
    case DW_TAG_typedef:
      return "DW_TAG_typedef";
    case DW_TAG_union_type:
      return "DW_TAG_union_type";
    case DW_TAG_unspecified_parameters:
      return "DW_TAG_unspecified_parameters";
    case DW_TAG_variant:
      return "DW_TAG_variant";
    case DW_TAG_common_block:
      return "DW_TAG_common_block";
    case DW_TAG_common_inclusion:
      return "DW_TAG_common_inclusion";
    case DW_TAG_inheritance:
      return "DW_TAG_inheritance";
    case DW_TAG_inlined_subroutine:
      return "DW_TAG_inlined_subroutine";
    case DW_TAG_module:
      return "DW_TAG_module";
    case DW_TAG_ptr_to_member_type:
      return "DW_TAG_ptr_to_member_type";
    case DW_TAG_set_type:
      return "DW_TAG_set_type";
    case DW_TAG_subrange_type:
      return "DW_TAG_subrange_type";
    case DW_TAG_with_stmt:
      return "DW_TAG_with_stmt";
    case DW_TAG_access_declaration:
      return "DW_TAG_access_declaration";
    case DW_TAG_base_type:
      return "DW_TAG_base_type";
    case DW_TAG_catch_block:
      return "DW_TAG_catch_block";
    case DW_TAG_const_type:
      return "DW_TAG_const_type";
    case DW_TAG_constant:
      return "DW_TAG_constant";
    case DW_TAG_enumerator:
      return "DW_TAG_enumerator";
    case DW_TAG_file_type:
      return "DW_TAG_file_type";
    case DW_TAG_friend:
      return "DW_TAG_friend";
    case DW_TAG_namelist:
      return "DW_TAG_namelist";
    case DW_TAG_namelist_item:
      return "DW_TAG_namelist_item";
    case DW_TAG_packed_type:
      return "DW_TAG_packed_type";
    case DW_TAG_subprogram:
      return "DW_TAG_subprogram";
    case DW_TAG_template_type_param:
      return "DW_TAG_template_type_param";
    case DW_TAG_template_value_param:
      return "DW_TAG_template_value_param";
    case DW_TAG_thrown_type:
      return "DW_TAG_thrown_type";
    case DW_TAG_try_block:
      return "DW_TAG_try_block";
    case DW_TAG_variant_part:
      return "DW_TAG_variant_part";
    case DW_TAG_variable:
      return "DW_TAG_variable";
    case DW_TAG_volatile_type:
      return "DW_TAG_volatile_type";
    case DW_TAG_dwarf_procedure:
      return "DW_TAG_dwarf_procedure";
    case DW_TAG_restrict_type:
      return "DW_TAG_restrict_type";
    case DW_TAG_interface_type:
      return "DW_TAG_interface_type";
    case DW_TAG_namespace:
      return "DW_TAG_namespace";
    case DW_TAG_imported_module:
      return "DW_TAG_imported_module";
    case DW_TAG_unspecified_type:
      return "DW_TAG_unspecified_type";
    case DW_TAG_partial_unit:
      return "DW_TAG_partial_unit";
    case DW_TAG_imported_unit:
      return "DW_TAG_imported_unit";
    case DW_TAG_condition:
      return "DW_TAG_condition";
    case DW_TAG_shared_type:
      return "DW_TAG_shared_type";
    case DW_TAG_MIPS_loop:
      return "DW_TAG_MIPS_loop";
    case DW_TAG_HP_array_descriptor:
      return "DW_TAG_HP_array_descriptor";
    case DW_TAG_format_label:
      return "DW_TAG_format_label";
    case DW_TAG_function_template:
      return "DW_TAG_function_template";
    case DW_TAG_class_template:
      return "DW_TAG_class_template";
    case DW_TAG_GNU_BINCL:
      return "DW_TAG_GNU_BINCL";
    case DW_TAG_GNU_EINCL:
      return "DW_TAG_GNU_EINCL";
    case DW_TAG_upc_shared_type:
      return "DW_TAG_upc_shared_type";
    case DW_TAG_upc_strict_type:
      return "DW_TAG_upc_strict_type";
    case DW_TAG_upc_relaxed_type:
      return "DW_TAG_upc_relaxed_type";
    case DW_TAG_PGI_kanji_type:
      return "DW_TAG_PGI_kanji_type";
    case DW_TAG_PGI_interface_block:
      return "DW_TAG_PGI_interface_block";
    default:
      return "DW_TAG_<unknown>";
    }
}

/* Convert a DWARF attribute code into its string name.  */

static char *
dwarf_attr_name (unsigned attr)
{
  switch (attr)
    {
    case DW_AT_sibling:
      return "DW_AT_sibling";
    case DW_AT_location:
      return "DW_AT_location";
    case DW_AT_name:
      return "DW_AT_name";
    case DW_AT_ordering:
      return "DW_AT_ordering";
    case DW_AT_subscr_data:
      return "DW_AT_subscr_data";
    case DW_AT_byte_size:
      return "DW_AT_byte_size";
    case DW_AT_bit_offset:
      return "DW_AT_bit_offset";
    case DW_AT_bit_size:
      return "DW_AT_bit_size";
    case DW_AT_element_list:
      return "DW_AT_element_list";
    case DW_AT_stmt_list:
      return "DW_AT_stmt_list";
    case DW_AT_low_pc:
      return "DW_AT_low_pc";
    case DW_AT_high_pc:
      return "DW_AT_high_pc";
    case DW_AT_language:
      return "DW_AT_language";
    case DW_AT_member:
      return "DW_AT_member";
    case DW_AT_discr:
      return "DW_AT_discr";
    case DW_AT_discr_value:
      return "DW_AT_discr_value";
    case DW_AT_visibility:
      return "DW_AT_visibility";
    case DW_AT_import:
      return "DW_AT_import";
    case DW_AT_string_length:
      return "DW_AT_string_length";
    case DW_AT_common_reference:
      return "DW_AT_common_reference";
    case DW_AT_comp_dir:
      return "DW_AT_comp_dir";
    case DW_AT_const_value:
      return "DW_AT_const_value";
    case DW_AT_containing_type:
      return "DW_AT_containing_type";
    case DW_AT_default_value:
      return "DW_AT_default_value";
    case DW_AT_inline:
      return "DW_AT_inline";
    case DW_AT_is_optional:
      return "DW_AT_is_optional";
    case DW_AT_lower_bound:
      return "DW_AT_lower_bound";
    case DW_AT_producer:
      return "DW_AT_producer";
    case DW_AT_prototyped:
      return "DW_AT_prototyped";
    case DW_AT_return_addr:
      return "DW_AT_return_addr";
    case DW_AT_start_scope:
      return "DW_AT_start_scope";
    case DW_AT_stride_size:
      return "DW_AT_stride_size";
    case DW_AT_upper_bound:
      return "DW_AT_upper_bound";
    case DW_AT_abstract_origin:
      return "DW_AT_abstract_origin";
    case DW_AT_accessibility:
      return "DW_AT_accessibility";
    case DW_AT_address_class:
      return "DW_AT_address_class";
    case DW_AT_artificial:
      return "DW_AT_artificial";
    case DW_AT_base_types:
      return "DW_AT_base_types";
    case DW_AT_calling_convention:
      return "DW_AT_calling_convention";
    case DW_AT_count:
      return "DW_AT_count";
    case DW_AT_data_member_location:
      return "DW_AT_data_member_location";
    case DW_AT_decl_column:
      return "DW_AT_decl_column";
    case DW_AT_decl_file:
      return "DW_AT_decl_file";
    case DW_AT_decl_line:
      return "DW_AT_decl_line";
    case DW_AT_declaration:
      return "DW_AT_declaration";
    case DW_AT_discr_list:
      return "DW_AT_discr_list";
    case DW_AT_encoding:
      return "DW_AT_encoding";
    case DW_AT_external:
      return "DW_AT_external";
    case DW_AT_frame_base:
      return "DW_AT_frame_base";
    case DW_AT_friend:
      return "DW_AT_friend";
    case DW_AT_identifier_case:
      return "DW_AT_identifier_case";
    case DW_AT_macro_info:
      return "DW_AT_macro_info";
    case DW_AT_namelist_items:
      return "DW_AT_namelist_items";
    case DW_AT_priority:
      return "DW_AT_priority";
    case DW_AT_segment:
      return "DW_AT_segment";
    case DW_AT_specification:
      return "DW_AT_specification";
    case DW_AT_static_link:
      return "DW_AT_static_link";
    case DW_AT_type:
      return "DW_AT_type";
    case DW_AT_use_location:
      return "DW_AT_use_location";
    case DW_AT_variable_parameter:
      return "DW_AT_variable_parameter";
    case DW_AT_virtuality:
      return "DW_AT_virtuality";
    case DW_AT_vtable_elem_location:
      return "DW_AT_vtable_elem_location";
    /* DWARF 3 values.  */
    case DW_AT_allocated:
      return "DW_AT_allocated";
    case DW_AT_associated:
      return "DW_AT_associated";
    case DW_AT_data_location:
      return "DW_AT_data_location";
    case DW_AT_stride:
      return "DW_AT_stride";
    case DW_AT_entry_pc:
      return "DW_AT_entry_pc";
    case DW_AT_use_UTF8:
      return "DW_AT_use_UTF8";
    case DW_AT_extension:
      return "DW_AT_extension";
    case DW_AT_ranges:
      return "DW_AT_ranges";
    case DW_AT_trampoline:
      return "DW_AT_trampoline";
    case DW_AT_call_column:
      return "DW_AT_call_column";
    case DW_AT_call_file:
      return "DW_AT_call_file";
    case DW_AT_call_line:
      return "DW_AT_call_line";
    case DW_AT_description:
      return "DW_AT_description";
    case DW_AT_binary_scale:
      return "DW_AT_binary_scale";
    case DW_AT_decimal_scale:
      return "DW_AT_decimal_scale";
    case DW_AT_small:
      return "DW_AT_small";
    case DW_AT_decimal_sign:
      return "DW_AT_decimal_sign";
    case DW_AT_digit_count:
      return "DW_AT_digit_count";
    case DW_AT_picture_string:
      return "DW_AT_picture_string";
    case DW_AT_mutable:
      return "DW_AT_mutable";
    case DW_AT_threads_scaled:
      return "DW_AT_threads_scaled";
    case DW_AT_explicit:
      return "DW_AT_explicit";
    case DW_AT_object_pointer:
      return "DW_AT_object_pointer";
    case DW_AT_endianity:
      return "DW_AT_endianity";
    case DW_AT_elemental:
      return "DW_AT_elemental";
    case DW_AT_pure:
      return "DW_AT_pure";
    case DW_AT_recursive:
      return "DW_AT_recursive";
#ifdef MIPS
    /* SGI/MIPS extensions.  */
    case DW_AT_MIPS_fde:
      return "DW_AT_MIPS_fde";
    case DW_AT_MIPS_loop_begin:
      return "DW_AT_MIPS_loop_begin";
    case DW_AT_MIPS_tail_loop_begin:
      return "DW_AT_MIPS_tail_loop_begin";
    case DW_AT_MIPS_epilog_begin:
      return "DW_AT_MIPS_epilog_begin";
    case DW_AT_MIPS_loop_unroll_factor:
      return "DW_AT_MIPS_loop_unroll_factor";
    case DW_AT_MIPS_software_pipeline_depth:
      return "DW_AT_MIPS_software_pipeline_depth";
    case DW_AT_MIPS_linkage_name:
      return "DW_AT_MIPS_linkage_name";
    case DW_AT_MIPS_stride:
      return "DW_AT_MIPS_stride";
    case DW_AT_MIPS_abstract_name:
      return "DW_AT_MIPS_abstract_name";
    case DW_AT_MIPS_clone_origin:
      return "DW_AT_MIPS_clone_origin";
    case DW_AT_MIPS_has_inlines:
      return "DW_AT_MIPS_has_inlines";
#endif
    /* HP extensions.  */
    case DW_AT_HP_block_index:
      return "DW_AT_HP_block_index";
    case DW_AT_HP_unmodifiable:
      return "DW_AT_HP_unmodifiable";
    case DW_AT_HP_actuals_stmt_list:
      return "DW_AT_HP_actuals_stmt_list";
    case DW_AT_HP_proc_per_section:
      return "DW_AT_HP_proc_per_section";
    case DW_AT_HP_raw_data_ptr:
      return "DW_AT_HP_raw_data_ptr";
    case DW_AT_HP_pass_by_reference:
      return "DW_AT_HP_pass_by_reference";
    case DW_AT_HP_opt_level:
      return "DW_AT_HP_opt_level";
    case DW_AT_HP_prof_version_id:
      return "DW_AT_HP_prof_version_id";
    case DW_AT_HP_opt_flags:
      return "DW_AT_HP_opt_flags";
    case DW_AT_HP_cold_region_low_pc:
      return "DW_AT_HP_cold_region_low_pc";
    case DW_AT_HP_cold_region_high_pc:
      return "DW_AT_HP_cold_region_high_pc";
    case DW_AT_HP_all_variables_modifiable:
      return "DW_AT_HP_all_variables_modifiable";
    case DW_AT_HP_linkage_name:
      return "DW_AT_HP_linkage_name";
    case DW_AT_HP_prof_flags:
      return "DW_AT_HP_prof_flags";
    /* GNU extensions.  */
    case DW_AT_sf_names:
      return "DW_AT_sf_names";
    case DW_AT_src_info:
      return "DW_AT_src_info";
    case DW_AT_mac_info:
      return "DW_AT_mac_info";
    case DW_AT_src_coords:
      return "DW_AT_src_coords";
    case DW_AT_body_begin:
      return "DW_AT_body_begin";
    case DW_AT_body_end:
      return "DW_AT_body_end";
    case DW_AT_GNU_vector:
      return "DW_AT_GNU_vector";
    /* VMS extensions.  */
    case DW_AT_VMS_rtnbeg_pd_address:
      return "DW_AT_VMS_rtnbeg_pd_address";
    /* UPC extension.  */
    case DW_AT_upc_threads_scaled:
      return "DW_AT_upc_threads_scaled";
    /* PGI (STMicroelectronics) extensions.  */
    case DW_AT_PGI_lbase:
      return "DW_AT_PGI_lbase";
    case DW_AT_PGI_soffset:
      return "DW_AT_PGI_soffset";
    case DW_AT_PGI_lstride:
      return "DW_AT_PGI_lstride";
    default:
      return "DW_AT_<unknown>";
    }
}

/* Convert a DWARF value form code into its string name.  */

static char *
dwarf_form_name (unsigned form)
{
  switch (form)
    {
    case DW_FORM_addr:
      return "DW_FORM_addr";
    case DW_FORM_block2:
      return "DW_FORM_block2";
    case DW_FORM_block4:
      return "DW_FORM_block4";
    case DW_FORM_data2:
      return "DW_FORM_data2";
    case DW_FORM_data4:
      return "DW_FORM_data4";
    case DW_FORM_data8:
      return "DW_FORM_data8";
    case DW_FORM_string:
      return "DW_FORM_string";
    case DW_FORM_block:
      return "DW_FORM_block";
    case DW_FORM_block1:
      return "DW_FORM_block1";
    case DW_FORM_data1:
      return "DW_FORM_data1";
    case DW_FORM_flag:
      return "DW_FORM_flag";
    case DW_FORM_sdata:
      return "DW_FORM_sdata";
    case DW_FORM_strp:
      return "DW_FORM_strp";
    case DW_FORM_udata:
      return "DW_FORM_udata";
    case DW_FORM_ref_addr:
      return "DW_FORM_ref_addr";
    case DW_FORM_ref1:
      return "DW_FORM_ref1";
    case DW_FORM_ref2:
      return "DW_FORM_ref2";
    case DW_FORM_ref4:
      return "DW_FORM_ref4";
    case DW_FORM_ref8:
      return "DW_FORM_ref8";
    case DW_FORM_ref_udata:
      return "DW_FORM_ref_udata";
    case DW_FORM_indirect:
      return "DW_FORM_indirect";
    default:
      return "DW_FORM_<unknown>";
    }
}

/* Convert a DWARF stack opcode into its string name.  */

static char *
dwarf_stack_op_name (unsigned op)
{
  switch (op)
    {
    case DW_OP_addr:
      return "DW_OP_addr";
    case DW_OP_deref:
      return "DW_OP_deref";
    case DW_OP_const1u:
      return "DW_OP_const1u";
    case DW_OP_const1s:
      return "DW_OP_const1s";
    case DW_OP_const2u:
      return "DW_OP_const2u";
    case DW_OP_const2s:
      return "DW_OP_const2s";
    case DW_OP_const4u:
      return "DW_OP_const4u";
    case DW_OP_const4s:
      return "DW_OP_const4s";
    case DW_OP_const8u:
      return "DW_OP_const8u";
    case DW_OP_const8s:
      return "DW_OP_const8s";
    case DW_OP_constu:
      return "DW_OP_constu";
    case DW_OP_consts:
      return "DW_OP_consts";
    case DW_OP_dup:
      return "DW_OP_dup";
    case DW_OP_drop:
      return "DW_OP_drop";
    case DW_OP_over:
      return "DW_OP_over";
    case DW_OP_pick:
      return "DW_OP_pick";
    case DW_OP_swap:
      return "DW_OP_swap";
    case DW_OP_rot:
      return "DW_OP_rot";
    case DW_OP_xderef:
      return "DW_OP_xderef";
    case DW_OP_abs:
      return "DW_OP_abs";
    case DW_OP_and:
      return "DW_OP_and";
    case DW_OP_div:
      return "DW_OP_div";
    case DW_OP_minus:
      return "DW_OP_minus";
    case DW_OP_mod:
      return "DW_OP_mod";
    case DW_OP_mul:
      return "DW_OP_mul";
    case DW_OP_neg:
      return "DW_OP_neg";
    case DW_OP_not:
      return "DW_OP_not";
    case DW_OP_or:
      return "DW_OP_or";
    case DW_OP_plus:
      return "DW_OP_plus";
    case DW_OP_plus_uconst:
      return "DW_OP_plus_uconst";
    case DW_OP_shl:
      return "DW_OP_shl";
    case DW_OP_shr:
      return "DW_OP_shr";
    case DW_OP_shra:
      return "DW_OP_shra";
    case DW_OP_xor:
      return "DW_OP_xor";
    case DW_OP_bra:
      return "DW_OP_bra";
    case DW_OP_eq:
      return "DW_OP_eq";
    case DW_OP_ge:
      return "DW_OP_ge";
    case DW_OP_gt:
      return "DW_OP_gt";
    case DW_OP_le:
      return "DW_OP_le";
    case DW_OP_lt:
      return "DW_OP_lt";
    case DW_OP_ne:
      return "DW_OP_ne";
    case DW_OP_skip:
      return "DW_OP_skip";
    case DW_OP_lit0:
      return "DW_OP_lit0";
    case DW_OP_lit1:
      return "DW_OP_lit1";
    case DW_OP_lit2:
      return "DW_OP_lit2";
    case DW_OP_lit3:
      return "DW_OP_lit3";
    case DW_OP_lit4:
      return "DW_OP_lit4";
    case DW_OP_lit5:
      return "DW_OP_lit5";
    case DW_OP_lit6:
      return "DW_OP_lit6";
    case DW_OP_lit7:
      return "DW_OP_lit7";
    case DW_OP_lit8:
      return "DW_OP_lit8";
    case DW_OP_lit9:
      return "DW_OP_lit9";
    case DW_OP_lit10:
      return "DW_OP_lit10";
    case DW_OP_lit11:
      return "DW_OP_lit11";
    case DW_OP_lit12:
      return "DW_OP_lit12";
    case DW_OP_lit13:
      return "DW_OP_lit13";
    case DW_OP_lit14:
      return "DW_OP_lit14";
    case DW_OP_lit15:
      return "DW_OP_lit15";
    case DW_OP_lit16:
      return "DW_OP_lit16";
    case DW_OP_lit17:
      return "DW_OP_lit17";
    case DW_OP_lit18:
      return "DW_OP_lit18";
    case DW_OP_lit19:
      return "DW_OP_lit19";
    case DW_OP_lit20:
      return "DW_OP_lit20";
    case DW_OP_lit21:
      return "DW_OP_lit21";
    case DW_OP_lit22:
      return "DW_OP_lit22";
    case DW_OP_lit23:
      return "DW_OP_lit23";
    case DW_OP_lit24:
      return "DW_OP_lit24";
    case DW_OP_lit25:
      return "DW_OP_lit25";
    case DW_OP_lit26:
      return "DW_OP_lit26";
    case DW_OP_lit27:
      return "DW_OP_lit27";
    case DW_OP_lit28:
      return "DW_OP_lit28";
    case DW_OP_lit29:
      return "DW_OP_lit29";
    case DW_OP_lit30:
      return "DW_OP_lit30";
    case DW_OP_lit31:
      return "DW_OP_lit31";
    case DW_OP_reg0:
      return "DW_OP_reg0";
    case DW_OP_reg1:
      return "DW_OP_reg1";
    case DW_OP_reg2:
      return "DW_OP_reg2";
    case DW_OP_reg3:
      return "DW_OP_reg3";
    case DW_OP_reg4:
      return "DW_OP_reg4";
    case DW_OP_reg5:
      return "DW_OP_reg5";
    case DW_OP_reg6:
      return "DW_OP_reg6";
    case DW_OP_reg7:
      return "DW_OP_reg7";
    case DW_OP_reg8:
      return "DW_OP_reg8";
    case DW_OP_reg9:
      return "DW_OP_reg9";
    case DW_OP_reg10:
      return "DW_OP_reg10";
    case DW_OP_reg11:
      return "DW_OP_reg11";
    case DW_OP_reg12:
      return "DW_OP_reg12";
    case DW_OP_reg13:
      return "DW_OP_reg13";
    case DW_OP_reg14:
      return "DW_OP_reg14";
    case DW_OP_reg15:
      return "DW_OP_reg15";
    case DW_OP_reg16:
      return "DW_OP_reg16";
    case DW_OP_reg17:
      return "DW_OP_reg17";
    case DW_OP_reg18:
      return "DW_OP_reg18";
    case DW_OP_reg19:
      return "DW_OP_reg19";
    case DW_OP_reg20:
      return "DW_OP_reg20";
    case DW_OP_reg21:
      return "DW_OP_reg21";
    case DW_OP_reg22:
      return "DW_OP_reg22";
    case DW_OP_reg23:
      return "DW_OP_reg23";
    case DW_OP_reg24:
      return "DW_OP_reg24";
    case DW_OP_reg25:
      return "DW_OP_reg25";
    case DW_OP_reg26:
      return "DW_OP_reg26";
    case DW_OP_reg27:
      return "DW_OP_reg27";
    case DW_OP_reg28:
      return "DW_OP_reg28";
    case DW_OP_reg29:
      return "DW_OP_reg29";
    case DW_OP_reg30:
      return "DW_OP_reg30";
    case DW_OP_reg31:
      return "DW_OP_reg31";
    case DW_OP_breg0:
      return "DW_OP_breg0";
    case DW_OP_breg1:
      return "DW_OP_breg1";
    case DW_OP_breg2:
      return "DW_OP_breg2";
    case DW_OP_breg3:
      return "DW_OP_breg3";
    case DW_OP_breg4:
      return "DW_OP_breg4";
    case DW_OP_breg5:
      return "DW_OP_breg5";
    case DW_OP_breg6:
      return "DW_OP_breg6";
    case DW_OP_breg7:
      return "DW_OP_breg7";
    case DW_OP_breg8:
      return "DW_OP_breg8";
    case DW_OP_breg9:
      return "DW_OP_breg9";
    case DW_OP_breg10:
      return "DW_OP_breg10";
    case DW_OP_breg11:
      return "DW_OP_breg11";
    case DW_OP_breg12:
      return "DW_OP_breg12";
    case DW_OP_breg13:
      return "DW_OP_breg13";
    case DW_OP_breg14:
      return "DW_OP_breg14";
    case DW_OP_breg15:
      return "DW_OP_breg15";
    case DW_OP_breg16:
      return "DW_OP_breg16";
    case DW_OP_breg17:
      return "DW_OP_breg17";
    case DW_OP_breg18:
      return "DW_OP_breg18";
    case DW_OP_breg19:
      return "DW_OP_breg19";
    case DW_OP_breg20:
      return "DW_OP_breg20";
    case DW_OP_breg21:
      return "DW_OP_breg21";
    case DW_OP_breg22:
      return "DW_OP_breg22";
    case DW_OP_breg23:
      return "DW_OP_breg23";
    case DW_OP_breg24:
      return "DW_OP_breg24";
    case DW_OP_breg25:
      return "DW_OP_breg25";
    case DW_OP_breg26:
      return "DW_OP_breg26";
    case DW_OP_breg27:
      return "DW_OP_breg27";
    case DW_OP_breg28:
      return "DW_OP_breg28";
    case DW_OP_breg29:
      return "DW_OP_breg29";
    case DW_OP_breg30:
      return "DW_OP_breg30";
    case DW_OP_breg31:
      return "DW_OP_breg31";
    case DW_OP_regx:
      return "DW_OP_regx";
    case DW_OP_fbreg:
      return "DW_OP_fbreg";
    case DW_OP_bregx:
      return "DW_OP_bregx";
    case DW_OP_piece:
      return "DW_OP_piece";
    case DW_OP_deref_size:
      return "DW_OP_deref_size";
    case DW_OP_xderef_size:
      return "DW_OP_xderef_size";
    case DW_OP_nop:
      return "DW_OP_nop";
    /* DWARF 3 extensions.  */
    case DW_OP_push_object_address:
      return "DW_OP_push_object_address";
    case DW_OP_call2:
      return "DW_OP_call2";
    case DW_OP_call4:
      return "DW_OP_call4";
    case DW_OP_call_ref:
      return "DW_OP_call_ref";
    /* GNU extensions.  */
    case DW_OP_form_tls_address:
      return "DW_OP_form_tls_address";
    case DW_OP_call_frame_cfa:
      return "DW_OP_call_frame_cfa";
    case DW_OP_bit_piece:
      return "DW_OP_bit_piece";
    case DW_OP_GNU_push_tls_address:
      return "DW_OP_GNU_push_tls_address";
    case DW_OP_GNU_uninit:
      return "DW_OP_GNU_uninit";
    /* HP extensions. */ 
    case DW_OP_HP_is_value:
      return "DW_OP_HP_is_value";
    case DW_OP_HP_fltconst4:
      return "DW_OP_HP_fltconst4";
    case DW_OP_HP_fltconst8:
      return "DW_OP_HP_fltconst8";
    case DW_OP_HP_mod_range:
      return "DW_OP_HP_mod_range";
    case DW_OP_HP_unmod_range:
      return "DW_OP_HP_unmod_range";
    case DW_OP_HP_tls:
      return "DW_OP_HP_tls";
    default:
      return "OP_<unknown>";
    }
}

static char *
dwarf_bool_name (unsigned mybool)
{
  if (mybool)
    return "TRUE";
  else
    return "FALSE";
}

/* Convert a DWARF type code into its string name.  */

static char *
dwarf_type_encoding_name (unsigned enc)
{
  switch (enc)
    {
    case DW_ATE_void:
      return "DW_ATE_void";
    case DW_ATE_address:
      return "DW_ATE_address";
    case DW_ATE_boolean:
      return "DW_ATE_boolean";
    case DW_ATE_complex_float:
      return "DW_ATE_complex_float";
    case DW_ATE_float:
      return "DW_ATE_float";
    case DW_ATE_signed:
      return "DW_ATE_signed";
    case DW_ATE_signed_char:
      return "DW_ATE_signed_char";
    case DW_ATE_unsigned:
      return "DW_ATE_unsigned";
    case DW_ATE_unsigned_char:
      return "DW_ATE_unsigned_char";
    /* DWARF 3.  */
    case DW_ATE_imaginary_float:
      return "DW_ATE_imaginary_float";
    case DW_ATE_packed_decimal:
      return "DW_ATE_packed_decimal";
    case DW_ATE_numeric_string:
      return "DW_ATE_numeric_string";
    case DW_ATE_edited:
      return "DW_ATE_edited";
    case DW_ATE_signed_fixed:
      return "DW_ATE_signed_fixed";
    case DW_ATE_unsigned_fixed:
      return "DW_ATE_unsigned_fixed";
    case DW_ATE_decimal_float:
      return "DW_ATE_decimal_float";
    /* HP extensions.  */
    case DW_ATE_HP_float80:
      return "DW_ATE_HP_float80";
    case DW_ATE_HP_complex_float80:
      return "DW_ATE_HP_complex_float80";
    case DW_ATE_HP_float128:
      return "DW_ATE_HP_float128";
    case DW_ATE_HP_complex_float128:
      return "DW_ATE_HP_complex_float128";
    case DW_ATE_HP_floathpintel:
      return "DW_ATE_HP_floathpintel";
    case DW_ATE_HP_imaginary_float80:
      return "DW_ATE_HP_imaginary_float80";
    case DW_ATE_HP_imaginary_float128:
      return "DW_ATE_HP_imaginary_float128";
    default:
      return "DW_ATE_<unknown>";
    }
}

/* Convert a DWARF call frame info operation to its string name. */

#if 0
static char *
dwarf_cfi_name (unsigned cfi_opc)
{
  switch (cfi_opc)
    {
    case DW_CFA_advance_loc:
      return "DW_CFA_advance_loc";
    case DW_CFA_offset:
      return "DW_CFA_offset";
    case DW_CFA_restore:
      return "DW_CFA_restore";
    case DW_CFA_nop:
      return "DW_CFA_nop";
    case DW_CFA_set_loc:
      return "DW_CFA_set_loc";
    case DW_CFA_advance_loc1:
      return "DW_CFA_advance_loc1";
    case DW_CFA_advance_loc2:
      return "DW_CFA_advance_loc2";
    case DW_CFA_advance_loc4:
      return "DW_CFA_advance_loc4";
    case DW_CFA_offset_extended:
      return "DW_CFA_offset_extended";
    case DW_CFA_restore_extended:
      return "DW_CFA_restore_extended";
    case DW_CFA_undefined:
      return "DW_CFA_undefined";
    case DW_CFA_same_value:
      return "DW_CFA_same_value";
    case DW_CFA_register:
      return "DW_CFA_register";
    case DW_CFA_remember_state:
      return "DW_CFA_remember_state";
    case DW_CFA_restore_state:
      return "DW_CFA_restore_state";
    case DW_CFA_def_cfa:
      return "DW_CFA_def_cfa";
    case DW_CFA_def_cfa_register:
      return "DW_CFA_def_cfa_register";
    case DW_CFA_def_cfa_offset:
      return "DW_CFA_def_cfa_offset";
    /* DWARF 3.  */
    case DW_CFA_def_cfa_expression:
      return "DW_CFA_def_cfa_expression";
    case DW_CFA_expression:
      return "DW_CFA_expression";
    case DW_CFA_offset_extended_sf:
      return "DW_CFA_offset_extended_sf";
    case DW_CFA_def_cfa_sf:
      return "DW_CFA_def_cfa_sf";
    case DW_CFA_def_cfa_offset_sf:
      return "DW_CFA_def_cfa_offset_sf";
    case DW_CFA_val_offset:
      return "DW_CFA_val_offset";
    case DW_CFA_val_offset_sf:
      return "DW_CFA_val_offset_sf";
    case DW_CFA_val_expression:
      return "DW_CFA_val_expression";
    /* SGI/MIPS specific.  */
    case DW_CFA_MIPS_advance_loc8:
      return "DW_CFA_MIPS_advance_loc8";
    /* GNU extensions.  */
    case DW_CFA_GNU_window_save:
      return "DW_CFA_GNU_window_save";
    case DW_CFA_GNU_args_size:
      return "DW_CFA_GNU_args_size";
    case DW_CFA_GNU_negative_offset_extended:
      return "DW_CFA_GNU_negative_offset_extended";
    default:
      return "DW_CFA_<unknown>";
    }
}
#endif

static void
dump_die (struct die_info *die)
{
  unsigned int i;

  fprintf_unfiltered (gdb_stderr, "Die: %s (abbrev = %d, offset = %d)\n",
	   dwarf_tag_name (die->tag), die->abbrev, die->offset);
  fprintf_unfiltered (gdb_stderr, "\thas children: %s\n",
	   dwarf_bool_name (die->child != NULL));

  fprintf_unfiltered (gdb_stderr, "\tattributes:\n");
  for (i = 0; i < die->num_attrs; ++i)
    {
      fprintf_unfiltered (gdb_stderr, "\t\t%s (%s) ",
	       dwarf_attr_name (die->attrs[i].name),
	       dwarf_form_name (die->attrs[i].form));
      switch (die->attrs[i].form)
	{
	case DW_FORM_ref_addr:
	case DW_FORM_addr:
	  fprintf_unfiltered (gdb_stderr, "address: ");
	  deprecated_print_address_numeric (DW_ADDR (&die->attrs[i]), 1, gdb_stderr);
	  break;
	case DW_FORM_block2:
	case DW_FORM_block4:
	case DW_FORM_block:
	case DW_FORM_block1:
	  fprintf_unfiltered (gdb_stderr, "block: size %d", DW_BLOCK (&die->attrs[i])->size);
	  break;
	case DW_FORM_ref1:
	case DW_FORM_ref2:
	case DW_FORM_ref4:
	  fprintf_unfiltered (gdb_stderr, "constant ref: %ld (adjusted)",
			      (long) (DW_ADDR (&die->attrs[i])));
	  break;
	case DW_FORM_data1:
	case DW_FORM_data2:
	case DW_FORM_data4:
	case DW_FORM_data8:
	case DW_FORM_udata:
	case DW_FORM_sdata:
	  fprintf_unfiltered (gdb_stderr, "constant: %ld", DW_UNSND (&die->attrs[i]));
	  break;
	case DW_FORM_string:
	case DW_FORM_strp:
	  fprintf_unfiltered (gdb_stderr, "string: \"%s\"",
		   DW_STRING (&die->attrs[i])
		   ? DW_STRING (&die->attrs[i]) : "");
	  break;
	case DW_FORM_flag:
	  if (DW_UNSND (&die->attrs[i]))
	    fprintf_unfiltered (gdb_stderr, "flag: TRUE");
	  else
	    fprintf_unfiltered (gdb_stderr, "flag: FALSE");
	  break;
	case DW_FORM_indirect:
	  /* the reader will have reduced the indirect form to
	     the "base form" so this form should not occur */
	  fprintf_unfiltered (gdb_stderr, "unexpected attribute form: DW_FORM_indirect");
	  break;
	default:
	  fprintf_unfiltered (gdb_stderr, "unsupported attribute form: %d.",
		   die->attrs[i].form);
	}
      fprintf_unfiltered (gdb_stderr, "\n");
    }
}

static void
dump_die_list (struct die_info *die)
{
  while (die)
    {
      dump_die (die);
      if (die->child != NULL)
	dump_die_list (die->child);
      if (die->sibling != NULL)
	dump_die_list (die->sibling);
    }
}

static void
store_in_ref_table (unsigned int offset, struct die_info *die,
		    struct dwarf2_cu *cu)
{
  int h;
  struct die_info *old;

  h = (offset % REF_HASH_SIZE);
  old = cu->die_ref_table[h];
  die->next_ref = old;
  cu->die_ref_table[h] = die;
}

static unsigned int
dwarf2_get_ref_die_offset (struct attribute *attr, struct dwarf2_cu *cu)
{
  unsigned int result = 0;

  switch (attr->form)
    {
    case DW_FORM_ref_addr:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      result = DW_ADDR (attr);
      break;
    default:
      complaint (&symfile_complaints,
		 _("unsupported die ref attribute form: '%s'"),
		 dwarf_form_name (attr->form));
    }
  return result;
}

/* Return the constant value held by the given attribute.  Return -1
   if the value held by the attribute is not constant.  */

static int
dwarf2_get_attr_constant_value (struct attribute *attr, int default_value)
{
  if (attr->form == DW_FORM_sdata)
    return DW_SND (attr);
  else if (attr->form == DW_FORM_udata
           || attr->form == DW_FORM_data1
           || attr->form == DW_FORM_data2
           || attr->form == DW_FORM_data4
           || attr->form == DW_FORM_data8)
    return DW_UNSND (attr);
  else
    {
      complaint (&symfile_complaints, _("Attribute value is not a constant (%s)"),
                 dwarf_form_name (attr->form));
      return default_value;
    }
}

static struct die_info *
follow_die_ref (struct die_info *src_die, struct attribute *attr,
		struct dwarf2_cu *cu)
{
  struct die_info *die;
  unsigned int offset;
  int h;
  struct die_info temp_die;
  struct dwarf2_cu *target_cu;

  offset = dwarf2_get_ref_die_offset (attr, cu);

  if (DW_ADDR (attr) < cu->header.offset
      || DW_ADDR (attr) >= cu->header.offset + cu->header.length)
    {
      struct dwarf2_per_cu_data *per_cu;
      per_cu = dwarf2_find_containing_comp_unit (DW_ADDR (attr),
						 cu->objfile);
      target_cu = per_cu->cu;
    }
  else
    target_cu = cu;

  h = (offset % REF_HASH_SIZE);
  die = target_cu->die_ref_table[h];
  while (die)
    {
      if (die->offset == offset)
	return die;
      die = die->next_ref;
    }

  error (_("Dwarf Error: Cannot find DIE at 0x%lx referenced from DIE "
	 "at 0x%lx [in module %s]"),
	 (long) src_die->offset, (long) offset, cu->objfile->name);

  return NULL;
}

static struct type *
dwarf2_fundamental_type (struct objfile *objfile, int typeid,
			 struct dwarf2_cu *cu)
{
  if (typeid < 0 || typeid >= FT_NUM_MEMBERS)
    {
      error (_("Dwarf Error: internal error - invalid fundamental type id %d [in module %s]"),
	     typeid, objfile->name);
    }

  /* Look for this particular type in the fundamental type vector.  If
     one is not found, create and install one appropriate for the
     current language and the current target machine. */

  if (cu->ftypes[typeid] == NULL)
    {
      cu->ftypes[typeid] = cu->language_defn->la_fund_type (objfile, typeid);
    }

  return (cu->ftypes[typeid]);
}

/* Decode simple location descriptions.
   Given a pointer to a dwarf block that defines a location, compute
   the location and return the value.

   NOTE drow/2003-11-18: This function is called in two situations
   now: for the address of static or global variables (partial symbols
   only) and for offsets into structures which are expected to be
   (more or less) constant.  The partial symbol case should go away,
   and only the constant case should remain.  That will let this
   function complain more accurately.  A few special modes are allowed
   without complaint for global variables (for instance, global
   register values and thread-local values).

   A location description containing no operations indicates that the
   object is optimized out.  The return value is 0 for that case.
   FIXME drow/2003-11-16: No callers check for this case any more; soon all
   callers will only want a very basic result and this can become a
   complaint.

   Note that stack[0] is unused except as a default error return.
   Note that stack overflow is not yet handled.  */

static CORE_ADDR
decode_locdesc (struct dwarf_block *blk, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;
  int i;
  int size = blk->size;
  gdb_byte *data = blk->data;
  CORE_ADDR stack[64];
  int stacki;
  unsigned int bytes_read, unsnd;
  gdb_byte op;

  i = 0;
  stacki = 0;
  stack[stacki] = 0;

  while (i < size)
    {
      op = data[i++];
      switch (op)
	{
	case DW_OP_lit0:
	case DW_OP_lit1:
	case DW_OP_lit2:
	case DW_OP_lit3:
	case DW_OP_lit4:
	case DW_OP_lit5:
	case DW_OP_lit6:
	case DW_OP_lit7:
	case DW_OP_lit8:
	case DW_OP_lit9:
	case DW_OP_lit10:
	case DW_OP_lit11:
	case DW_OP_lit12:
	case DW_OP_lit13:
	case DW_OP_lit14:
	case DW_OP_lit15:
	case DW_OP_lit16:
	case DW_OP_lit17:
	case DW_OP_lit18:
	case DW_OP_lit19:
	case DW_OP_lit20:
	case DW_OP_lit21:
	case DW_OP_lit22:
	case DW_OP_lit23:
	case DW_OP_lit24:
	case DW_OP_lit25:
	case DW_OP_lit26:
	case DW_OP_lit27:
	case DW_OP_lit28:
	case DW_OP_lit29:
	case DW_OP_lit30:
	case DW_OP_lit31:
	  stack[++stacki] = op - DW_OP_lit0;
	  break;

	case DW_OP_reg0:
	case DW_OP_reg1:
	case DW_OP_reg2:
	case DW_OP_reg3:
	case DW_OP_reg4:
	case DW_OP_reg5:
	case DW_OP_reg6:
	case DW_OP_reg7:
	case DW_OP_reg8:
	case DW_OP_reg9:
	case DW_OP_reg10:
	case DW_OP_reg11:
	case DW_OP_reg12:
	case DW_OP_reg13:
	case DW_OP_reg14:
	case DW_OP_reg15:
	case DW_OP_reg16:
	case DW_OP_reg17:
	case DW_OP_reg18:
	case DW_OP_reg19:
	case DW_OP_reg20:
	case DW_OP_reg21:
	case DW_OP_reg22:
	case DW_OP_reg23:
	case DW_OP_reg24:
	case DW_OP_reg25:
	case DW_OP_reg26:
	case DW_OP_reg27:
	case DW_OP_reg28:
	case DW_OP_reg29:
	case DW_OP_reg30:
	case DW_OP_reg31:
	  stack[++stacki] = op - DW_OP_reg0;
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

	case DW_OP_regx:
	  unsnd = read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  stack[++stacki] = unsnd;
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

	case DW_OP_addr:
	  stack[++stacki] = read_address (objfile->obfd, &data[i],
					  cu, &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_const1u:
	  stack[++stacki] = read_1_byte (objfile->obfd, &data[i]);
	  i += 1;
	  break;

	case DW_OP_const1s:
	  stack[++stacki] = read_1_signed_byte (objfile->obfd, &data[i]);
	  i += 1;
	  break;

	case DW_OP_const2u:
	  stack[++stacki] = read_2_bytes (objfile->obfd, &data[i]);
	  i += 2;
	  break;

	case DW_OP_const2s:
	  stack[++stacki] = read_2_signed_bytes (objfile->obfd, &data[i]);
	  i += 2;
	  break;

	case DW_OP_const4u:
	  stack[++stacki] = read_4_bytes (objfile->obfd, &data[i]);
	  i += 4;
	  break;

	case DW_OP_const4s:
	  stack[++stacki] = read_4_signed_bytes (objfile->obfd, &data[i]);
	  i += 4;
	  break;

	case DW_OP_constu:
	  stack[++stacki] = read_unsigned_leb128 (NULL, (data + i),
						  &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_consts:
	  stack[++stacki] = read_signed_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_dup:
	  stack[stacki + 1] = stack[stacki];
	  stacki++;
	  break;

	case DW_OP_plus:
	  stack[stacki - 1] += stack[stacki];
	  stacki--;
	  break;

	case DW_OP_plus_uconst:
	  stack[stacki] += read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_minus:
	  stack[stacki - 1] -= stack[stacki];
	  stacki--;
	  break;

	case DW_OP_deref:
	  /* If we're not the last op, then we definitely can't encode
	     this using GDB's address_class enum.  This is valid for partial
	     global symbols, although the variable's address will be bogus
	     in the psymtab.  */
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

        case DW_OP_GNU_push_tls_address:
	  /* The top of the stack has the offset from the beginning
	     of the thread control block at which the variable is located.  */
	  /* Nothing should follow this operator, so the top of stack would
	     be returned.  */
	  /* This is valid for partial global symbols, but the variable's
	     address will be bogus in the psymtab.  */
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
          break;

	case DW_OP_GNU_uninit:
	  break;

	default:
	  complaint (&symfile_complaints, _("unsupported stack op: '%s'"),
		     dwarf_stack_op_name (op));
	  return (stack[stacki]);
	}
    }
  return (stack[stacki]);
}

/* memory allocation interface */

static struct dwarf_block *
dwarf_alloc_block (struct dwarf2_cu *cu)
{
  struct dwarf_block *blk;

  blk = (struct dwarf_block *)
    obstack_alloc (&cu->comp_unit_obstack, sizeof (struct dwarf_block));
  return (blk);
}

static struct abbrev_info *
dwarf_alloc_abbrev (struct dwarf2_cu *cu)
{
  struct abbrev_info *abbrev;

  abbrev = (struct abbrev_info *)
    obstack_alloc (&cu->abbrev_obstack, sizeof (struct abbrev_info));
  memset (abbrev, 0, sizeof (struct abbrev_info));
  return (abbrev);
}

static struct die_info *
dwarf_alloc_die (void)
{
  struct die_info *die;

  die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (die, 0, sizeof (struct die_info));
  return (die);
}


/* Macro support.  */


/* Return the full name of file number I in *LH's file name table.
   Use COMP_DIR as the name of the current directory of the
   compilation.  The result is allocated using xmalloc; the caller is
   responsible for freeing it.  */
static char *
file_full_name (int file, struct line_header *lh, const char *comp_dir)
{
  /* Is the file number a valid index into the line header's file name
     table?  Remember that file numbers start with one, not zero.  */
  if (1 <= file && file <= lh->num_file_names)
    {
      struct file_entry *fe = &lh->file_names[file - 1];
  
      if (IS_ABSOLUTE_PATH (fe->name))
        return xstrdup (fe->name);
      else
        {
          const char *dir;
          int dir_len;
          char *full_name;

          if (fe->dir_index)
            dir = lh->include_dirs[fe->dir_index - 1];
          else
            dir = comp_dir;

          if (dir)
            {
              dir_len = strlen (dir);
              full_name = xmalloc (dir_len + 1 + strlen (fe->name) + 1);
              strcpy (full_name, dir);
              full_name[dir_len] = '/';
              strcpy (full_name + dir_len + 1, fe->name);
              return full_name;
            }
          else
            return xstrdup (fe->name);
        }
    }
  else
    {
      /* The compiler produced a bogus file number.  We can at least
         record the macro definitions made in the file, even if we
         won't be able to find the file by name.  */
      char fake_name[80];
      sprintf (fake_name, "<bad macro file number %d>", file);

      complaint (&symfile_complaints, 
                 _("bad file number in macro information (%d)"),
                 file);

      return xstrdup (fake_name);
    }
}


static struct macro_source_file *
macro_start_file (int file, int line,
                  struct macro_source_file *current_file,
                  const char *comp_dir,
                  struct line_header *lh, struct objfile *objfile)
{
  /* The full name of this source file.  */
  char *full_name = file_full_name (file, lh, comp_dir);

  /* We don't create a macro table for this compilation unit
     at all until we actually get a filename.  */
  if (! pending_macros)
    pending_macros = new_macro_table (&objfile->objfile_obstack,
                                      objfile->macro_cache);

  if (! current_file)
    /* If we have no current file, then this must be the start_file
       directive for the compilation unit's main source file.  */
    current_file = macro_set_main (pending_macros, full_name);
  else
    current_file = macro_include (current_file, line, full_name);

  xfree (full_name);
              
  return current_file;
}


/* Copy the LEN characters at BUF to a xmalloc'ed block of memory,
   followed by a null byte.  */
static char *
copy_string (const char *buf, int len)
{
  char *s = xmalloc (len + 1);
  memcpy (s, buf, len);
  s[len] = '\0';

  return s;
}


static const char *
consume_improper_spaces (const char *p, const char *body)
{
  if (*p == ' ')
    {
      complaint (&symfile_complaints,
		 _("macro definition contains spaces in formal argument list:\n`%s'"),
		 body);

      while (*p == ' ')
        p++;
    }

  return p;
}


static void
parse_macro_definition (struct macro_source_file *file, int line,
                        const char *body)
{
  const char *p;

  /* The body string takes one of two forms.  For object-like macro
     definitions, it should be:

        <macro name> " " <definition>

     For function-like macro definitions, it should be:

        <macro name> "() " <definition>
     or
        <macro name> "(" <arg name> ( "," <arg name> ) * ") " <definition>

     Spaces may appear only where explicitly indicated, and in the
     <definition>.

     The Dwarf 2 spec says that an object-like macro's name is always
     followed by a space, but versions of GCC around March 2002 omit
     the space when the macro's definition is the empty string. 

     The Dwarf 2 spec says that there should be no spaces between the
     formal arguments in a function-like macro's formal argument list,
     but versions of GCC around March 2002 include spaces after the
     commas.  */


  /* Find the extent of the macro name.  The macro name is terminated
     by either a space or null character (for an object-like macro) or
     an opening paren (for a function-like macro).  */
  for (p = body; *p; p++)
    if (*p == ' ' || *p == '(')
      break;

  if (*p == ' ' || *p == '\0')
    {
      /* It's an object-like macro.  */
      int name_len = p - body;
      char *name = copy_string (body, name_len);
      const char *replacement;

      if (*p == ' ')
        replacement = body + name_len + 1;
      else
        {
	  dwarf2_macro_malformed_definition_complaint (body);
          replacement = body + name_len;
        }
      
      macro_define_object (file, line, name, replacement);

      xfree (name);
    }
  else if (*p == '(')
    {
      /* It's a function-like macro.  */
      char *name = copy_string (body, p - body);
      int argc = 0;
      int argv_size = 1;
      char **argv = xmalloc (argv_size * sizeof (*argv));

      p++;

      p = consume_improper_spaces (p, body);

      /* Parse the formal argument list.  */
      while (*p && *p != ')')
        {
          /* Find the extent of the current argument name.  */
          const char *arg_start = p;

          while (*p && *p != ',' && *p != ')' && *p != ' ')
            p++;

          if (! *p || p == arg_start)
	    dwarf2_macro_malformed_definition_complaint (body);
          else
            {
              /* Make sure argv has room for the new argument.  */
              if (argc >= argv_size)
                {
                  argv_size *= 2;
                  argv = xrealloc (argv, argv_size * sizeof (*argv));
                }

              argv[argc++] = copy_string (arg_start, p - arg_start);
            }

          p = consume_improper_spaces (p, body);

          /* Consume the comma, if present.  */
          if (*p == ',')
            {
              p++;

              p = consume_improper_spaces (p, body);
            }
        }

      if (*p == ')')
        {
          p++;

          if (*p == ' ')
            /* Perfectly formed definition, no complaints.  */
            macro_define_function (file, line, name,
                                   argc, (const char **) argv, 
                                   p + 1);
          else if (*p == '\0')
            {
              /* Complain, but do define it.  */
	      dwarf2_macro_malformed_definition_complaint (body);
              macro_define_function (file, line, name,
                                     argc, (const char **) argv, 
                                     p);
            }
          else
            /* Just complain.  */
	    dwarf2_macro_malformed_definition_complaint (body);
        }
      else
        /* Just complain.  */
	dwarf2_macro_malformed_definition_complaint (body);

      xfree (name);
      {
        int i;

        for (i = 0; i < argc; i++)
          xfree (argv[i]);
      }
      xfree (argv);
    }
  else
    dwarf2_macro_malformed_definition_complaint (body);
}


static void
dwarf_decode_macros (struct line_header *lh, unsigned int offset,
                     char *comp_dir, bfd *abfd,
                     struct dwarf2_cu *cu)
{
  gdb_byte *mac_ptr, *mac_end;
  struct macro_source_file *current_file = 0;

  if (dwarf2_per_objfile->macinfo_buffer == NULL)
    {
      complaint (&symfile_complaints, _("missing .debug_macinfo section"));
      return;
    }

  mac_ptr = dwarf2_per_objfile->macinfo_buffer + offset;
  mac_end = dwarf2_per_objfile->macinfo_buffer
    + dwarf2_per_objfile->macinfo_size;

  for (;;)
    {
      enum dwarf_macinfo_record_type macinfo_type;

      /* Do we at least have room for a macinfo type byte?  */
      if (mac_ptr >= mac_end)
        {
	  dwarf2_macros_too_long_complaint ();
          return;
        }

      macinfo_type = read_1_byte (abfd, mac_ptr);
      mac_ptr++;

      switch (macinfo_type)
        {
          /* A zero macinfo type indicates the end of the macro
             information.  */
        case 0:
          return;

        case DW_MACINFO_define:
        case DW_MACINFO_undef:
          {
            unsigned int bytes_read;
            int line;
            char *body;

            line = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            body = read_string (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            if (! current_file)
	      complaint (&symfile_complaints,
			 _("debug info gives macro %s outside of any file: %s"),
			 macinfo_type ==
			 DW_MACINFO_define ? "definition" : macinfo_type ==
			 DW_MACINFO_undef ? "undefinition" :
			 "something-or-other", body);
            else
              {
                if (macinfo_type == DW_MACINFO_define)
                  parse_macro_definition (current_file, line, body);
                else if (macinfo_type == DW_MACINFO_undef)
                  macro_undef (current_file, line, body);
              }
          }
          break;

        case DW_MACINFO_start_file:
          {
            unsigned int bytes_read;
            int line, file;

            line = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            file = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            current_file = macro_start_file (file, line,
                                             current_file, comp_dir,
                                             lh, cu->objfile);
          }
          break;

        case DW_MACINFO_end_file:
          if (! current_file)
	    complaint (&symfile_complaints,
		       _("macro debug info has an unmatched `close_file' directive"));
          else
            {
              current_file = current_file->included_by;
              if (! current_file)
                {
                  enum dwarf_macinfo_record_type next_type;

                  /* GCC circa March 2002 doesn't produce the zero
                     type byte marking the end of the compilation
                     unit.  Complain if it's not there, but exit no
                     matter what.  */

                  /* Do we at least have room for a macinfo type byte?  */
                  if (mac_ptr >= mac_end)
                    {
		      dwarf2_macros_too_long_complaint ();
                      return;
                    }

                  /* We don't increment mac_ptr here, so this is just
                     a look-ahead.  */
                  next_type = read_1_byte (abfd, mac_ptr);
                  if (next_type != 0)
		    complaint (&symfile_complaints,
			       _("no terminating 0-type entry for macros in `.debug_macinfo' section"));

                  return;
                }
            }
          break;

        case DW_MACINFO_vendor_ext:
          {
            unsigned int bytes_read;
            int constant;
            char *string;

            constant = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            string = read_string (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            /* We don't recognize any vendor extensions.  */
          }
          break;
        }
    }
}

/* Check if the attribute's form is a DW_FORM_block*
   if so return true else false. */
static int
attr_form_is_block (struct attribute *attr)
{
  return (attr == NULL ? 0 :
      attr->form == DW_FORM_block1
      || attr->form == DW_FORM_block2
      || attr->form == DW_FORM_block4
      || attr->form == DW_FORM_block);
}

static void
dwarf2_symbol_mark_computed (struct attribute *attr, struct symbol *sym,
			     struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;

  /* Save the master objfile, so that we can report and look up the
     correct file containing this variable.  */
  if (objfile->separate_debug_objfile_backlink)
    objfile = objfile->separate_debug_objfile_backlink;

  if ((attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
      /* ".debug_loc" may not exist at all, or the offset may be outside
	 the section.  If so, fall through to the complaint in the
	 other branch.  */
      && DW_UNSND (attr) < dwarf2_per_objfile->loc_size)
    {
      struct dwarf2_loclist_baton *baton;

      baton = obstack_alloc (&cu->objfile->objfile_obstack,
			     sizeof (struct dwarf2_loclist_baton));
      baton->objfile = objfile;

      /* We don't know how long the location list is, but make sure we
	 don't run off the edge of the section.  */
      baton->size = dwarf2_per_objfile->loc_size - DW_UNSND (attr);
      baton->data = dwarf2_per_objfile->loc_buffer + DW_UNSND (attr);
      baton->base_address = cu->header.base_address;
      if (cu->header.base_known == 0)
	complaint (&symfile_complaints,
		   _("Location list used without specifying the CU base address."));

      SYMBOL_OPS (sym) = &dwarf2_loclist_funcs;
      SYMBOL_LOCATION_BATON (sym) = baton;
    }
  else
    {
      struct dwarf2_locexpr_baton *baton;

      baton = obstack_alloc (&cu->objfile->objfile_obstack,
			     sizeof (struct dwarf2_locexpr_baton));
      baton->objfile = objfile;

      if (attr_form_is_block (attr))
	{
	  /* Note that we're just copying the block's data pointer
	     here, not the actual data.  We're still pointing into the
	     info_buffer for SYM's objfile; right now we never release
	     that buffer, but when we do clean up properly this may
	     need to change.  */
	  baton->size = DW_BLOCK (attr)->size;
	  baton->data = DW_BLOCK (attr)->data;
	}
      else
	{
	  dwarf2_invalid_attrib_class_complaint ("location description",
						 SYMBOL_NATURAL_NAME (sym));
	  baton->size = 0;
	  baton->data = NULL;
	}
      
      SYMBOL_OPS (sym) = &dwarf2_locexpr_funcs;
      SYMBOL_LOCATION_BATON (sym) = baton;
    }
}

/* Locate the compilation unit from CU's objfile which contains the
   DIE at OFFSET.  Raises an error on failure.  */

static struct dwarf2_per_cu_data *
dwarf2_find_containing_comp_unit (unsigned long offset,
				  struct objfile *objfile)
{
  struct dwarf2_per_cu_data *this_cu;
  int low, high;

  low = 0;
  high = dwarf2_per_objfile->n_comp_units - 1;
  while (high > low)
    {
      int mid = low + (high - low) / 2;
      if (dwarf2_per_objfile->all_comp_units[mid]->offset >= offset)
	high = mid;
      else
	low = mid + 1;
    }
  gdb_assert (low == high);
  if (dwarf2_per_objfile->all_comp_units[low]->offset > offset)
    {
      if (low == 0)
	error (_("Dwarf Error: could not find partial DIE containing "
	       "offset 0x%lx [in module %s]"),
	       (long) offset, bfd_get_filename (objfile->obfd));

      gdb_assert (dwarf2_per_objfile->all_comp_units[low-1]->offset <= offset);
      return dwarf2_per_objfile->all_comp_units[low-1];
    }
  else
    {
      this_cu = dwarf2_per_objfile->all_comp_units[low];
      if (low == dwarf2_per_objfile->n_comp_units - 1
	  && offset >= this_cu->offset + this_cu->length)
	error (_("invalid dwarf2 offset %ld"), offset);
      gdb_assert (offset < this_cu->offset + this_cu->length);
      return this_cu;
    }
}

/* Locate the compilation unit from OBJFILE which is located at exactly
   OFFSET.  Raises an error on failure.  */

static struct dwarf2_per_cu_data *
dwarf2_find_comp_unit (unsigned long offset, struct objfile *objfile)
{
  struct dwarf2_per_cu_data *this_cu;
  this_cu = dwarf2_find_containing_comp_unit (offset, objfile);
  if (this_cu->offset != offset)
    error (_("no compilation unit with offset %ld."), offset);
  return this_cu;
}

/* Release one cached compilation unit, CU.  We unlink it from the tree
   of compilation units, but we don't remove it from the read_in_chain;
   the caller is responsible for that.  */

static void
free_one_comp_unit (void *data)
{
  struct dwarf2_cu *cu = data;

  if (cu->per_cu != NULL)
    cu->per_cu->cu = NULL;
  cu->per_cu = NULL;

  obstack_free (&cu->comp_unit_obstack, NULL);
  if (cu->dies)
    free_die_list (cu->dies);

  xfree (cu);
}

/* This cleanup function is passed the address of a dwarf2_cu on the stack
   when we're finished with it.  We can't free the pointer itself, but be
   sure to unlink it from the cache.  Also release any associated storage
   and perform cache maintenance.

   Only used during partial symbol parsing.  */

static void
free_stack_comp_unit (void *data)
{
  struct dwarf2_cu *cu = data;

  obstack_free (&cu->comp_unit_obstack, NULL);
  cu->partial_dies = NULL;

  if (cu->per_cu != NULL)
    {
      /* This compilation unit is on the stack in our caller, so we
	 should not xfree it.  Just unlink it.  */
      cu->per_cu->cu = NULL;
      cu->per_cu = NULL;

      /* If we had a per-cu pointer, then we may have other compilation
	 units loaded, so age them now.  */
      age_cached_comp_units ();
    }
}

/* Free all cached compilation units.  */

static void
free_cached_comp_units (void *data)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      free_one_comp_unit (per_cu->cu);
      *last_chain = next_cu;

      per_cu = next_cu;
    }
}

/* Increase the age counter on each cached compilation unit, and free
   any that are too old.  */

static void
age_cached_comp_units (void)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  dwarf2_clear_marks (dwarf2_per_objfile->read_in_chain);
  per_cu = dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      per_cu->cu->last_used ++;
      if (per_cu->cu->last_used <= dwarf2_max_cache_age)
	dwarf2_mark (per_cu->cu);
      per_cu = per_cu->cu->read_in_chain;
    }

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      if (!per_cu->cu->mark)
	{
	  free_one_comp_unit (per_cu->cu);
	  *last_chain = next_cu;
	}
      else
	last_chain = &per_cu->cu->read_in_chain;

      per_cu = next_cu;
    }
}

/* Remove a single compilation unit from the cache.  */

static void
free_one_cached_comp_unit (void *target_cu)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      if (per_cu->cu == target_cu)
	{
	  free_one_comp_unit (per_cu->cu);
	  *last_chain = next_cu;
	  break;
	}
      else
	last_chain = &per_cu->cu->read_in_chain;

      per_cu = next_cu;
    }
}

/* A pair of DIE offset and GDB type pointer.  We store these
   in a hash table separate from the DIEs, and preserve them
   when the DIEs are flushed out of cache.  */

struct dwarf2_offset_and_type
{
  unsigned int offset;
  struct type *type;
};

/* Hash function for a dwarf2_offset_and_type.  */

static hashval_t
offset_and_type_hash (const void *item)
{
  const struct dwarf2_offset_and_type *ofs = item;
  return ofs->offset;
}

/* Equality function for a dwarf2_offset_and_type.  */

static int
offset_and_type_eq (const void *item_lhs, const void *item_rhs)
{
  const struct dwarf2_offset_and_type *ofs_lhs = item_lhs;
  const struct dwarf2_offset_and_type *ofs_rhs = item_rhs;
  return ofs_lhs->offset == ofs_rhs->offset;
}

/* Set the type associated with DIE to TYPE.  Save it in CU's hash
   table if necessary.  */

static void
set_die_type (struct die_info *die, struct type *type, struct dwarf2_cu *cu)
{
  struct dwarf2_offset_and_type **slot, ofs;

  die->type = type;

  if (cu->per_cu == NULL)
    return;

  if (cu->per_cu->type_hash == NULL)
    cu->per_cu->type_hash
      = htab_create_alloc_ex (cu->header.length / 24,
			      offset_and_type_hash,
			      offset_and_type_eq,
			      NULL,
			      &cu->objfile->objfile_obstack,
			      hashtab_obstack_allocate,
			      dummy_obstack_deallocate);

  ofs.offset = die->offset;
  ofs.type = type;
  slot = (struct dwarf2_offset_and_type **)
    htab_find_slot_with_hash (cu->per_cu->type_hash, &ofs, ofs.offset, INSERT);
  *slot = obstack_alloc (&cu->objfile->objfile_obstack, sizeof (**slot));
  **slot = ofs;
}

/* Find the type for DIE in TYPE_HASH, or return NULL if DIE does not
   have a saved type.  */

static struct type *
get_die_type (struct die_info *die, htab_t type_hash)
{
  struct dwarf2_offset_and_type *slot, ofs;

  ofs.offset = die->offset;
  slot = htab_find_with_hash (type_hash, &ofs, ofs.offset);
  if (slot)
    return slot->type;
  else
    return NULL;
}

/* Restore the types of the DIE tree starting at START_DIE from the hash
   table saved in CU.  */

static void
reset_die_and_siblings_types (struct die_info *start_die, struct dwarf2_cu *cu)
{
  struct die_info *die;

  if (cu->per_cu->type_hash == NULL)
    return;

  for (die = start_die; die != NULL; die = die->sibling)
    {
      die->type = get_die_type (die, cu->per_cu->type_hash);
      if (die->child != NULL)
	reset_die_and_siblings_types (die->child, cu);
    }
}

/* Set the mark field in CU and in every other compilation unit in the
   cache that we must keep because we are keeping CU.  */

/* Add a dependence relationship from CU to REF_PER_CU.  */

static void
dwarf2_add_dependence (struct dwarf2_cu *cu,
		       struct dwarf2_per_cu_data *ref_per_cu)
{
  void **slot;

  if (cu->dependencies == NULL)
    cu->dependencies
      = htab_create_alloc_ex (5, htab_hash_pointer, htab_eq_pointer,
			      NULL, &cu->comp_unit_obstack,
			      hashtab_obstack_allocate,
			      dummy_obstack_deallocate);

  slot = htab_find_slot (cu->dependencies, ref_per_cu, INSERT);
  if (*slot == NULL)
    *slot = ref_per_cu;
}

/* Set the mark field in CU and in every other compilation unit in the
   cache that we must keep because we are keeping CU.  */

static int
dwarf2_mark_helper (void **slot, void *data)
{
  struct dwarf2_per_cu_data *per_cu;

  per_cu = (struct dwarf2_per_cu_data *) *slot;
  if (per_cu->cu->mark)
    return 1;
  per_cu->cu->mark = 1;

  if (per_cu->cu->dependencies != NULL)
    htab_traverse (per_cu->cu->dependencies, dwarf2_mark_helper, NULL);

  return 1;
}

static void
dwarf2_mark (struct dwarf2_cu *cu)
{
  if (cu->mark)
    return;
  cu->mark = 1;
  if (cu->dependencies != NULL)
    htab_traverse (cu->dependencies, dwarf2_mark_helper, NULL);
}

static void
dwarf2_clear_marks (struct dwarf2_per_cu_data *per_cu)
{
  while (per_cu)
    {
      per_cu->cu->mark = 0;
      per_cu = per_cu->cu->read_in_chain;
    }
}

/* Trivial hash function for partial_die_info: the hash value of a DIE
   is its offset in .debug_info for this objfile.  */

static hashval_t
partial_die_hash (const void *item)
{
  const struct partial_die_info *part_die = item;
  return part_die->offset;
}

/* Trivial comparison function for partial_die_info structures: two DIEs
   are equal if they have the same offset.  */

static int
partial_die_eq (const void *item_lhs, const void *item_rhs)
{
  const struct partial_die_info *part_die_lhs = item_lhs;
  const struct partial_die_info *part_die_rhs = item_rhs;
  return part_die_lhs->offset == part_die_rhs->offset;
}

static struct cmd_list_element *set_dwarf2_cmdlist;
static struct cmd_list_element *show_dwarf2_cmdlist;

static void
set_dwarf2_cmd (char *args, int from_tty)
{
  help_list (set_dwarf2_cmdlist, "maintenance set dwarf2 ", -1, gdb_stdout);
}

static void
show_dwarf2_cmd (char *args, int from_tty)
{ 
  cmd_show_list (show_dwarf2_cmdlist, from_tty, "");
}

void _initialize_dwarf2_read (void);

void
_initialize_dwarf2_read (void)
{
  dwarf2_objfile_data_key = register_objfile_data ();

  add_prefix_cmd ("dwarf2", class_maintenance, set_dwarf2_cmd, _("\
Set DWARF 2 specific variables.\n\
Configure DWARF 2 variables such as the cache size"),
                  &set_dwarf2_cmdlist, "maintenance set dwarf2 ",
                  0/*allow-unknown*/, &maintenance_set_cmdlist);

  add_prefix_cmd ("dwarf2", class_maintenance, show_dwarf2_cmd, _("\
Show DWARF 2 specific variables\n\
Show DWARF 2 variables such as the cache size"),
                  &show_dwarf2_cmdlist, "maintenance show dwarf2 ",
                  0/*allow-unknown*/, &maintenance_show_cmdlist);

  add_setshow_zinteger_cmd ("max-cache-age", class_obscure,
			    &dwarf2_max_cache_age, _("\
Set the upper bound on the age of cached dwarf2 compilation units."), _("\
Show the upper bound on the age of cached dwarf2 compilation units."), _("\
A higher limit means that cached compilation units will be stored\n\
in memory longer, and more total memory will be used.  Zero disables\n\
caching, which can slow down startup."),
			    NULL,
			    show_dwarf2_max_cache_age,
			    &set_dwarf2_cmdlist,
			    &show_dwarf2_cmdlist);
}
