/*  This file is part of the program psim.

    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
    */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#include "config.h"
#include "misc.h"
#include "lf.h"
#include "table.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

/****************************************************************/

int spreg_lookup_table = 1;
enum {
  nr_of_sprs = 1024,
};

/****************************************************************/


typedef enum {
  spreg_name,
  spreg_reg_nr,
  spreg_readonly,
  spreg_length,
  nr_spreg_fields,
} spreg_fields;

typedef struct _spreg_table_entry spreg_table_entry;
struct _spreg_table_entry {
  char *name;
  int spreg_nr;
  int is_readonly;
  int length;
  table_entry *entry;
  spreg_table_entry *next;
};

typedef struct _spreg_table spreg_table;
struct _spreg_table {
  spreg_table_entry *sprs;
};

static void
spreg_table_insert(spreg_table *table, table_entry *entry)
{
  /* create a new spr entry */
  spreg_table_entry *new_spr = ZALLOC(spreg_table_entry);
  new_spr->next = NULL;
  new_spr->entry = entry;
  new_spr->spreg_nr = atoi(entry->fields[spreg_reg_nr]);
  new_spr->is_readonly = (entry->fields[spreg_readonly]
			  ? atoi(entry->fields[spreg_readonly])
			  : 0);
  new_spr->length = atoi(entry->fields[spreg_length]);
  new_spr->name = (char*)zalloc(strlen(entry->fields[spreg_name]) + 1);
  ASSERT(new_spr->name != NULL);
  {
    int i;
    for (i = 0; entry->fields[spreg_name][i] != '\0'; i++) {
      if (isupper(entry->fields[spreg_name][i]))
	new_spr->name[i] = tolower(entry->fields[spreg_name][i]);
      else
	new_spr->name[i] = entry->fields[spreg_name][i];
    }
  }

  /* insert, by spreg_nr order */
  {
    spreg_table_entry **ptr_to_spreg_entry = &table->sprs;
    spreg_table_entry *spreg_entry = *ptr_to_spreg_entry;
    while (spreg_entry != NULL && spreg_entry->spreg_nr < new_spr->spreg_nr) {
      ptr_to_spreg_entry = &spreg_entry->next;
      spreg_entry = *ptr_to_spreg_entry;
    }
    ASSERT(spreg_entry == NULL || spreg_entry->spreg_nr != new_spr->spreg_nr);
    *ptr_to_spreg_entry = new_spr;
    new_spr->next = spreg_entry;
  }

}


static spreg_table *
spreg_table_load(char *file_name)
{
  table *file = table_open(file_name, nr_spreg_fields, 0);
  spreg_table *table = ZALLOC(spreg_table);

  {
    table_entry *entry;
    while ((entry = table_entry_read(file)) != NULL) {
      spreg_table_insert(table, entry);
    }
  }

  return table;
}


/****************************************************************/

char *spreg_attributes[] = {
  "is_valid",
  "is_readonly",
  "name",
  "index",
  "length",
  0
};

static void
gen_spreg_h(spreg_table *table, lf *file)
{
  spreg_table_entry *entry;
  char **attribute;

  lf_print__gnu_copyleft(file);
  lf_printf(file, "\n");
  lf_printf(file, "#ifndef _SPREG_H_\n");
  lf_printf(file, "#define _SPREG_H_\n");
  lf_printf(file, "\n");
  lf_printf(file, "typedef unsigned_word spreg;\n");
  lf_printf(file, "\n");
  lf_printf(file, "typedef enum {\n");

  for (entry = table->sprs;
       entry != NULL ;
       entry = entry->next) {
    lf_printf(file, "  spr_%s = %d,\n", entry->name, entry->spreg_nr);
  }

  lf_printf(file, "  nr_of_sprs = %d\n", nr_of_sprs);
  lf_printf(file, "} sprs;\n");
  lf_printf(file, "\n");
  for (attribute = spreg_attributes;
       *attribute != NULL;
       attribute++) {
    if (strcmp(*attribute, "name") == 0) {
      lf_print_function_type(file, "const char *", "INLINE_SPREG", " ");
      lf_printf(file, "spr_%s(sprs spr);\n", *attribute);
    }
    else {
      lf_print_function_type(file, "int", "INLINE_SPREG", " ");
      lf_printf(file, "spr_%s(sprs spr);\n", *attribute);
    }
  }
  lf_printf(file, "\n");
  lf_printf(file, "#endif /* _SPREG_H_ */\n");
}


static void
gen_spreg_c(spreg_table *table, lf *file)
{
  spreg_table_entry *entry;
  char **attribute;
  int spreg_nr;

  lf_print__gnu_copyleft(file);
  lf_printf(file, "\n");
  lf_printf(file, "#ifndef _SPREG_C_\n");
  lf_printf(file, "#define _SPREG_C_\n");
  lf_printf(file, "\n");
  lf_printf(file, "#include \"basics.h\"\n");
  lf_printf(file, "#include \"spreg.h\"\n");

  lf_printf(file, "\n");
  lf_printf(file, "typedef struct _spreg_info {\n");
  lf_printf(file, "  char *name;\n");
  lf_printf(file, "  int is_valid;\n");
  lf_printf(file, "  int length;\n");
  lf_printf(file, "  int is_readonly;\n");
  lf_printf(file, "  int index;\n");
  lf_printf(file, "} spreg_info;\n");
  lf_printf(file, "\n");
  lf_printf(file, "static spreg_info spr_info[nr_of_sprs+1] = {\n");
  entry = table->sprs;
  for (spreg_nr = 0; spreg_nr < nr_of_sprs+1; spreg_nr++) {
    if (entry == NULL || spreg_nr < entry->spreg_nr)
      lf_printf(file, "  { 0, 0, 0, 0, %d},\n", spreg_nr);
    else {
      lf_printf(file, "  { \"%s\", %d, %d, %d, spr_%s /*%d*/ },\n",
		entry->name, 1, entry->length, entry->is_readonly,
		entry->name, entry->spreg_nr);
      entry = entry->next;
    }
  }
  lf_printf(file, "};\n");

  for (attribute = spreg_attributes;
       *attribute != NULL;
       attribute++) {
    lf_printf(file, "\n");
    if (strcmp(*attribute, "name") == 0) {
      lf_print_function_type(file, "const char *", "INLINE_SPREG", "\n");
    }
    else {
      lf_print_function_type(file, "int", "INLINE_SPREG", "\n");
    }
    lf_printf(file, "spr_%s(sprs spr)\n", *attribute);
    lf_printf(file, "{\n");
    if (spreg_lookup_table
	|| strcmp(*attribute, "name") == 0
	|| strcmp(*attribute, "index") == 0)
      lf_printf(file, "  return spr_info[spr].%s;\n",
		*attribute);
    else {
      spreg_table_entry *entry;
      lf_printf(file, "  switch (spr) {\n");
      for (entry = table->sprs; entry != NULL; entry = entry->next) {
	lf_printf(file, "  case %d:\n", entry->spreg_nr);
	if (strcmp(*attribute, "is_valid") == 0)
	  lf_printf(file, "    return 1;\n");
	else if (strcmp(*attribute, "is_readonly") == 0)
	  lf_printf(file, "    return %d;\n", entry->is_readonly);
	else if (strcmp(*attribute, "length") == 0)
	  lf_printf(file, "    return %d;\n", entry->length);
	else
	  ASSERT(0);
      }
      lf_printf(file, "  default:\n");
      lf_printf(file, "    return 0;\n");
      lf_printf(file, "  }\n");
    }
    lf_printf(file, "}\n");
  }

  lf_printf(file, "\n");
  lf_printf(file, "#endif /* _SPREG_C_ */\n");
}



/****************************************************************/


int
main(int argc,
     char **argv,
     char **envp)
{
  lf_file_references file_references = lf_include_references;
  spreg_table *sprs = NULL;
  char *real_file_name = NULL;
  int is_header = 0;
  int ch;

  if (argc <= 1) {
    printf("Usage: dgen ...\n");
    printf("-s  Use switch instead of table\n");
    printf("-n <file-name>  Use this as cpp line numbering name\n");
    printf("-h  Output header file\n");
    printf("-p <spreg-file>  Output spreg.h(P) or spreg.c(p)\n");
    printf("-L  Suppress cpp line numbering in output files\n");
  }


  while ((ch = getopt(argc, argv, "hLsn:r:p:")) != -1) {
    fprintf(stderr, "\t-%c %s\n", ch, ( optarg ? optarg : ""));
    switch(ch) {
    case 's':
      spreg_lookup_table = 0;
      break;
    case 'r':
      sprs = spreg_table_load(optarg);
      break;
    case 'n':
      real_file_name = strdup(optarg);
      break;
    case 'L':
      file_references = lf_omit_references;
      break;
    case 'h':
      is_header = 1;
      break;
    case 'p':
      {
	lf *file = lf_open(optarg, real_file_name, file_references,
			   (is_header ? lf_is_h : lf_is_c),
			   argv[0]);
	if (is_header)
	  gen_spreg_h(sprs, file);
	else
	  gen_spreg_c(sprs, file);
	lf_close(file);
	is_header = 0;
      }
      real_file_name = NULL;
      break;
    default:
      error("unknown option\n");
    }
  }
  return 0;
}
