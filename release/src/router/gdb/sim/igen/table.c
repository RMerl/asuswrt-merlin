/* The IGEN simulator generator for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney.

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



#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

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

typedef struct _open_table open_table;
struct _open_table
{
  size_t size;
  char *buffer;
  char *pos;
  line_ref pseudo_line;
  line_ref real_line;
  open_table *parent;
  table *root;
};
struct _table
{
  open_table *current;
};


static line_ref *
current_line (open_table * file)
{
  line_ref *entry = ZALLOC (line_ref);
  *entry = file->pseudo_line;
  return entry;
}

static table_entry *
new_table_entry (open_table * file, table_entry_type type)
{
  table_entry *entry;
  entry = ZALLOC (table_entry);
  entry->file = file->root;
  entry->line = current_line (file);
  entry->type = type;
  return entry;
}

static void
set_nr_table_entry_fields (table_entry *entry, int nr_fields)
{
  entry->field = NZALLOC (char *, nr_fields + 1);
  entry->nr_fields = nr_fields;
}


void
table_push (table *root,
	    line_ref *line, table_include *includes, const char *file_name)
{
  FILE *ff;
  open_table *file;
  table_include dummy;
  table_include *include = &dummy;

  /* dummy up a search of this directory */
  dummy.next = includes;
  dummy.dir = "";

  /* create a file descriptor */
  file = ZALLOC (open_table);
  if (file == NULL)
    {
      perror (file_name);
      exit (1);
    }
  file->root = root;
  file->parent = root->current;
  root->current = file;

  while (1)
    {
      /* save the file name */
      char *dup_name =
	NZALLOC (char, strlen (include->dir) + strlen (file_name) + 2);
      if (dup_name == NULL)
	{
	  perror (file_name);
	  exit (1);
	}
      if (include->dir[0] != '\0')
	{
	  strcat (dup_name, include->dir);
	  strcat (dup_name, "/");
	}
      strcat (dup_name, file_name);
      file->real_line.file_name = dup_name;
      file->pseudo_line.file_name = dup_name;
      /* open the file */

      ff = fopen (dup_name, "rb");
      if (ff)
	break;
      /* zfree (dup_name); */
      if (include->next == NULL)
	{
	  if (line != NULL)
	    error (line, "Problem opening file `%s'\n", file_name);
	  perror (file_name);
	  exit (1);
	}
      include = include->next;
    }


  /* determine the size */
  fseek (ff, 0, SEEK_END);
  file->size = ftell (ff);
  fseek (ff, 0, SEEK_SET);

  /* allocate this much memory */
  file->buffer = (char *) zalloc (file->size + 1);
  if (file->buffer == NULL)
    {
      perror (file_name);
      exit (1);
    }
  file->pos = file->buffer;

  /* read it all in */
  if (fread (file->buffer, 1, file->size, ff) < file->size)
    {
      perror (file_name);
      exit (1);
    }
  file->buffer[file->size] = '\0';

  /* set the initial line numbering */
  file->real_line.line_nr = 1;	/* specifies current line */
  file->pseudo_line.line_nr = 1;	/* specifies current line */

  /* done */
  fclose (ff);
}

table *
table_open (const char *file_name)
{
  table *root;

  /* create a file descriptor */
  root = ZALLOC (table);
  if (root == NULL)
    {
      perror (file_name);
      exit (1);
    }

  table_push (root, NULL, NULL, file_name);
  return root;
}

char *
skip_spaces (char *chp)
{
  while (1)
    {
      if (*chp == '\0' || *chp == '\n' || !isspace (*chp))
	return chp;
      chp++;
    }
}


char *
back_spaces (char *start, char *chp)
{
  while (1)
    {
      if (chp <= start || !isspace (chp[-1]))
	return chp;
      chp--;
    }
}

char *
skip_digits (char *chp)
{
  while (1)
    {
      if (*chp == '\0' || *chp == '\n' || !isdigit (*chp))
	return chp;
      chp++;
    }
}

char *
skip_to_separator (char *chp, char *separators)
{
  while (1)
    {
      char *sep = separators;
      while (1)
	{
	  if (*chp == *sep)
	    return chp;
	  if (*sep == '\0')
	    break;
	  sep++;
	}
      chp++;
    }
}

static char *
skip_to_null (char *chp)
{
  return skip_to_separator (chp, "");
}


static char *
skip_to_nl (char *chp)
{
  return skip_to_separator (chp, "\n");
}


static void
next_line (open_table * file)
{
  file->pos = skip_to_nl (file->pos);
  if (*file->pos == '0')
    error (&file->pseudo_line, "Missing <nl> at end of line\n");
  *file->pos = '\0';
  file->pos += 1;
  file->real_line.line_nr += 1;
  file->pseudo_line.line_nr += 1;
}


extern table_entry *
table_read (table *root)
{
  open_table *file = root->current;
  table_entry *entry = NULL;
  while (1)
    {

      /* end-of-file? */
      while (*file->pos == '\0')
	{
	  if (file->parent != NULL)
	    {
	      file = file->parent;
	      root->current = file;
	    }
	  else
	    return NULL;
	}

      /* code_block? */
      if (*file->pos == '{')
	{
	  char *chp;
	  next_line (file);	/* discard leading brace */
	  entry = new_table_entry (file, table_code_entry);
	  chp = file->pos;
	  /* determine how many lines are involved - look for <nl> "}" */
	  {
	    int nr_lines = 0;
	    while (*file->pos != '}')
	      {
		next_line (file);
		nr_lines++;
	      }
	    set_nr_table_entry_fields (entry, nr_lines);
	  }
	  /* now enter each line */
	  {
	    int line_nr;
	    for (line_nr = 0; line_nr < entry->nr_fields; line_nr++)
	      {
		if (strncmp (chp, "  ", 2) == 0)
		  entry->field[line_nr] = chp + 2;
		else
		  entry->field[line_nr] = chp;
		chp = skip_to_null (chp) + 1;
	      }
	    /* skip trailing brace */
	    ASSERT (*file->pos == '}');
	    next_line (file);
	  }
	  break;
	}

      /* tab block? */
      if (*file->pos == '\t')
	{
	  char *chp = file->pos;
	  entry = new_table_entry (file, table_code_entry);
	  /* determine how many lines are involved - look for <nl> !<tab> */
	  {
	    int nr_lines = 0;
	    int nr_blank_lines = 0;
	    while (1)
	      {
		if (*file->pos == '\t')
		  {
		    nr_lines = nr_lines + nr_blank_lines + 1;
		    nr_blank_lines = 0;
		    next_line (file);
		  }
		else
		  {
		    file->pos = skip_spaces (file->pos);
		    if (*file->pos != '\n')
		      break;
		    nr_blank_lines++;
		    next_line (file);
		  }
	      }
	    set_nr_table_entry_fields (entry, nr_lines);
	  }
	  /* now enter each line */
	  {
	    int line_nr;
	    for (line_nr = 0; line_nr < entry->nr_fields; line_nr++)
	      {
		if (*chp == '\t')
		  entry->field[line_nr] = chp + 1;
		else
		  entry->field[line_nr] = "";	/* blank */
		chp = skip_to_null (chp) + 1;
	      }
	  }
	  break;
	}

      /* cpp directive? */
      if (file->pos[0] == '#')
	{
	  char *chp = skip_spaces (file->pos + 1);

	  /* cpp line-nr directive - # <line-nr> "<file>" */
	  if (isdigit (*chp)
	      && *skip_digits (chp) == ' '
	      && *skip_spaces (skip_digits (chp)) == '"')
	    {
	      int line_nr;
	      char *file_name;
	      file->pos = chp;
	      /* parse the number */
	      line_nr = atoi (file->pos) - 1;
	      /* skip to the file name */
	      while (file->pos[0] != '0'
		     && file->pos[0] != '"' && file->pos[0] != '\0')
		file->pos++;
	      if (file->pos[0] != '"')
		error (&file->real_line,
		       "Missing opening quote in cpp directive\n");
	      /* parse the file name */
	      file->pos++;
	      file_name = file->pos;
	      while (file->pos[0] != '"' && file->pos[0] != '\0')
		file->pos++;
	      if (file->pos[0] != '"')
		error (&file->real_line,
		       "Missing closing quote in cpp directive\n");
	      file->pos[0] = '\0';
	      file->pos++;
	      file->pos = skip_to_nl (file->pos);
	      if (file->pos[0] != '\n')
		error (&file->real_line,
		       "Missing newline in cpp directive\n");
	      file->pseudo_line.file_name = file_name;
	      file->pseudo_line.line_nr = line_nr;
	      next_line (file);
	      continue;
	    }

	  /* #define and #undef - not implemented yet */

	  /* Old style # comment */
	  next_line (file);
	  continue;
	}

      /* blank line or end-of-file? */
      file->pos = skip_spaces (file->pos);
      if (*file->pos == '\0')
	error (&file->pseudo_line, "Missing <nl> at end of file\n");
      if (*file->pos == '\n')
	{
	  next_line (file);
	  continue;
	}

      /* comment - leading // or # - skip */
      if ((file->pos[0] == '/' && file->pos[1] == '/')
	  || (file->pos[0] == '#'))
	{
	  next_line (file);
	  continue;
	}

      /* colon field */
      {
	char *chp = file->pos;
	entry = new_table_entry (file, table_colon_entry);
	next_line (file);
	/* figure out how many fields */
	{
	  int nr_fields = 1;
	  char *tmpch = chp;
	  while (1)
	    {
	      tmpch = skip_to_separator (tmpch, "\\:");
	      if (*tmpch == '\\')
		{
		  /* eat the escaped character */
		  char *cp = tmpch;
		  while (cp[1] != '\0')
		    {
		      cp[0] = cp[1];
		      cp++;
		    }
		  cp[0] = '\0';
		  tmpch++;
		}
	      else if (*tmpch != ':')
		break;
	      else
		{
		  *tmpch = '\0';
		  tmpch++;
		  nr_fields++;
		}
	    }
	  set_nr_table_entry_fields (entry, nr_fields);
	}
	/* now parse them */
	{
	  int field_nr;
	  for (field_nr = 0; field_nr < entry->nr_fields; field_nr++)
	    {
	      chp = skip_spaces (chp);
	      entry->field[field_nr] = chp;
	      chp = skip_to_null (chp);
	      *back_spaces (entry->field[field_nr], chp) = '\0';
	      chp++;
	    }
	}
	break;
      }

    }

  ASSERT (entry == NULL || entry->field[entry->nr_fields] == NULL);
  return entry;
}

extern void
table_print_code (lf *file, table_entry *entry)
{
  int field_nr;
  int nr = 0;
  for (field_nr = 0; field_nr < entry->nr_fields; field_nr++)
    {
      char *chp = entry->field[field_nr];
      int in_bit_field = 0;
      if (*chp == '#')
	lf_indent_suppress (file);
      while (*chp != '\0')
	{
	  if (chp[0] == '{' && !isspace (chp[1]) && chp[1] != '\0')
	    {
	      in_bit_field = 1;
	      nr += lf_putchr (file, '_');
	    }
	  else if (in_bit_field && chp[0] == ':')
	    {
	      nr += lf_putchr (file, '_');
	    }
	  else if (in_bit_field && *chp == '}')
	    {
	      nr += lf_putchr (file, '_');
	      in_bit_field = 0;
	    }
	  else
	    {
	      nr += lf_putchr (file, *chp);
	    }
	  chp++;
	}
      if (in_bit_field)
	{
	  line_ref line = *entry->line;
	  line.line_nr += field_nr;
	  error (&line, "Bit field brace miss match\n");
	}
      nr += lf_putchr (file, '\n');
    }
}



void
dump_line_ref (lf *file, char *prefix, const line_ref *line, char *suffix)
{
  lf_printf (file, "%s(line_ref*) 0x%lx", prefix, (long) line);
  if (line != NULL)
    {
      lf_indent (file, +1);
      lf_printf (file, "\n(line_nr %d)", line->line_nr);
      lf_printf (file, "\n(file_name %s)", line->file_name);
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


static const char *
table_entry_type_to_str (table_entry_type type)
{
  switch (type)
    {
    case table_code_entry:
      return "code-entry";
    case table_colon_entry:
      return "colon-entry";
    }
  return "*invalid*";
}

void
dump_table_entry (lf *file,
		  char *prefix, const table_entry *entry, char *suffix)
{
  lf_printf (file, "%s(table_entry*) 0x%lx", prefix, (long) entry);
  if (entry != NULL)
    {
      int field;
      lf_indent (file, +1);
      dump_line_ref (file, "\n(line ", entry->line, ")");
      lf_printf (file, "\n(type %s)", table_entry_type_to_str (entry->type));
      lf_printf (file, "\n(nr_fields %d)", entry->nr_fields);
      lf_printf (file, "\n(fields");
      lf_indent (file, +1);
      for (field = 0; field < entry->nr_fields; field++)
	lf_printf (file, "\n\"%s\"", entry->field[field]);
      lf_indent (file, -1);
      lf_printf (file, ")");
      lf_indent (file, -1);
    }
  lf_printf (file, "%s", suffix);
}


#ifdef MAIN
int
main (int argc, char **argv)
{
  table *t;
  table_entry *entry;
  lf *l;
  int line_nr;

  if (argc != 2)
    {
      printf ("Usage: table <file>\n");
      exit (1);
    }

  t = table_open (argv[1]);
  l = lf_open ("-", "stdout", lf_omit_references, lf_is_text, "tmp-table");

  line_nr = 0;
  do
    {
      char line[10];
      entry = table_read (t);
      line_nr++;
      sprintf (line, "(%d ", line_nr);
      dump_table_entry (l, line, entry, ")\n");
    }
  while (entry != NULL);

  return 0;
}
#endif
