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



#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#include "config.h"
#include "misc.h"
#include "lf.h"

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

struct _lf
{
  FILE *stream;
  int line_nr;			/* nr complete lines written, curr line is line_nr+1 */
  int indent;
  int line_blank;
  const char *name;
  const char *program;
  lf_file_references references;
  lf_file_type type;
};


lf *
lf_open (char *name,
	 char *real_name,
	 lf_file_references references,
	 lf_file_type type, const char *program)
{
  /* create a file object */
  lf *new_lf = ZALLOC (lf);
  ASSERT (new_lf != NULL);
  new_lf->references = references;
  new_lf->type = type;
  new_lf->name = (real_name == NULL ? name : real_name);
  new_lf->program = program;
  /* attach to stdout if pipe */
  if (!strcmp (name, "-"))
    {
      new_lf->stream = stdout;
    }
  else
    {
      /* create a new file */
      new_lf->stream = fopen (name, "w");
      if (new_lf->stream == NULL)
	{
	  perror (name);
	  exit (1);
	}
    }
  return new_lf;
}


void
lf_close (lf *file)
{
  if (file->stream != stdout)
    {
      if (fclose (file->stream))
	{
	  perror ("lf_close.fclose");
	  exit (1);
	}
      free (file);
    }
}


int
lf_putchr (lf *file, const char chr)
{
  int nr = 0;
  if (chr == '\n')
    {
      file->line_nr += 1;
      file->line_blank = 1;
    }
  else if (file->line_blank)
    {
      int pad;
      for (pad = file->indent; pad > 0; pad--)
	putc (' ', file->stream);
      nr += file->indent;
      file->line_blank = 0;
    }
  putc (chr, file->stream);
  nr += 1;
  return nr;
}

int
lf_write (lf *file, const char *string, int strlen_string)
{
  int nr = 0;
  int i;
  for (i = 0; i < strlen_string; i++)
    nr += lf_putchr (file, string[i]);
  return nr;
}


void
lf_indent_suppress (lf *file)
{
  file->line_blank = 0;
}


int
lf_putstr (lf *file, const char *string)
{
  int nr = 0;
  const char *chp;
  if (string != NULL)
    {
      for (chp = string; *chp != '\0'; chp++)
	{
	  nr += lf_putchr (file, *chp);
	}
    }
  return nr;
}

static int
do_lf_putunsigned (lf *file, unsigned u)
{
  int nr = 0;
  if (u > 0)
    {
      nr += do_lf_putunsigned (file, u / 10);
      nr += lf_putchr (file, (u % 10) + '0');
    }
  return nr;
}


int
lf_putint (lf *file, int decimal)
{
  int nr = 0;
  if (decimal == 0)
    nr += lf_putchr (file, '0');
  else if (decimal < 0)
    {
      nr += lf_putchr (file, '-');
      nr += do_lf_putunsigned (file, -decimal);
    }
  else if (decimal > 0)
    {
      nr += do_lf_putunsigned (file, decimal);
    }
  else
    ASSERT (0);
  return nr;
}


int
lf_printf (lf *file, const char *fmt, ...)
{
  int nr = 0;
  char buf[1024];
  va_list ap;

  va_start (ap, fmt);
  vsprintf (buf, fmt, ap);
  /* FIXME - this is really stuffed but so is vsprintf() on a sun! */
  ASSERT (strlen (buf) < sizeof (buf));
  nr += lf_putstr (file, buf);
  va_end (ap);
  return nr;
}


int
lf_print__line_ref (lf *file, line_ref *line)
{
  return lf_print__external_ref (file, line->line_nr, line->file_name);
}

int
lf_print__external_ref (lf *file, int line_nr, const char *file_name)
{
  int nr = 0;
  switch (file->references)
    {
    case lf_include_references:
      lf_indent_suppress (file);
      nr += lf_putstr (file, "#line ");
      nr += lf_putint (file, line_nr);
      nr += lf_putstr (file, " \"");
      nr += lf_putstr (file, file_name);
      nr += lf_putstr (file, "\"\n");
      break;
    case lf_omit_references:
      nr += lf_putstr (file, "/* ");
      nr += lf_putstr (file, file_name);
      nr += lf_putstr (file, ":");
      nr += lf_putint (file, line_nr);
      nr += lf_putstr (file, "*/\n");
      break;
    }
  return nr;
}

int
lf_print__internal_ref (lf *file)
{
  int nr = 0;
  nr += lf_print__external_ref (file, file->line_nr + 2, file->name);
  /* line_nr == last_line, want to number from next */
  return nr;
}

void
lf_indent (lf *file, int delta)
{
  file->indent += delta;
}


int
lf_print__gnu_copyleft (lf *file)
{
  int nr = 0;
  switch (file->type)
    {
    case lf_is_c:
    case lf_is_h:
      nr += lf_printf (file, "\
/* This file is part of GDB.\n\
\n\
   Copyright 2002, 2007 Free Software Foundation, Inc.\n\
\n\
   This program is free software; you can redistribute it and/or modify\n\
   it under the terms of the GNU General Public License as published by\n\
   the Free Software Foundation; either version 3 of the License, or\n\
   (at your option) any later version.\n\
\n\
   This program is distributed in the hope that it will be useful,\n\
   but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
   GNU General Public License for more details.\n\
\n\
   You should have received a copy of the GNU General Public License\n\
   along with this program.  If not, see <http://www.gnu.org/licenses/>.\n\
\n\
   --\n\
\n\
   This file was generated by the program %s */\n\
", filter_filename (file->program));
      break;
    default:
      ASSERT (0);
      break;
    }
  return nr;
}


int
lf_putbin (lf *file, int decimal, int width)
{
  int nr = 0;
  int bit;
  ASSERT (width > 0);
  for (bit = 1 << (width - 1); bit != 0; bit >>= 1)
    {
      if (decimal & bit)
	nr += lf_putchr (file, '1');
      else
	nr += lf_putchr (file, '0');
    }
  return nr;
}

int
lf_print__this_file_is_empty (lf *file, const char *reason)
{
  int nr = 0;
  switch (file->type)
    {
    case lf_is_c:
    case lf_is_h:
      nr += lf_printf (file,
		       "/* This generated file (%s) is intentionally left blank",
		       file->name);
      if (reason != NULL)
	nr += lf_printf (file, " - %s", reason);
      nr += lf_printf (file, " */\n");
      break;
    default:
      ERROR ("Bad switch");
    }
  return nr;
}

int
lf_print__ucase_filename (lf *file)
{
  int nr = 0;
  const char *chp = file->name;
  while (*chp != '\0')
    {
      char ch = *chp;
      if (islower (ch))
	{
	  nr += lf_putchr (file, toupper (ch));
	}
      else if (ch == '.')
	nr += lf_putchr (file, '_');
      else
	nr += lf_putchr (file, ch);
      chp++;
    }
  return nr;
}

int
lf_print__file_start (lf *file)
{
  int nr = 0;
  switch (file->type)
    {
    case lf_is_h:
    case lf_is_c:
      nr += lf_print__gnu_copyleft (file);
      nr += lf_printf (file, "\n");
      nr += lf_printf (file, "#ifndef ");
      nr += lf_print__ucase_filename (file);
      nr += lf_printf (file, "\n");
      nr += lf_printf (file, "#define ");
      nr += lf_print__ucase_filename (file);
      nr += lf_printf (file, "\n");
      nr += lf_printf (file, "\n");
      break;
    default:
      ASSERT (0);
    }
  return nr;
}


int
lf_print__file_finish (lf *file)
{
  int nr = 0;
  switch (file->type)
    {
    case lf_is_h:
    case lf_is_c:
      nr += lf_printf (file, "\n");
      nr += lf_printf (file, "#endif /* _");
      nr += lf_print__ucase_filename (file);
      nr += lf_printf (file, "_*/\n");
      break;
    default:
      ASSERT (0);
    }
  return nr;
}


int
lf_print__function_type (lf *file,
			 const char *type,
			 const char *prefix, const char *trailing_space)
{
  int nr = 0;
  nr += lf_printf (file, "%s\\\n(%s)", prefix, type);
  if (trailing_space != NULL)
    nr += lf_printf (file, "%s", trailing_space);
  return nr;
}

int
lf_print__function_type_function (lf *file,
				  print_function * print_type,
				  const char *prefix,
				  const char *trailing_space)
{
  int nr = 0;
  nr += lf_printf (file, "%s\\\n(", prefix);
  nr += print_type (file);
  nr += lf_printf (file, ")");
  if (trailing_space != NULL)
    nr += lf_printf (file, "%s", trailing_space);
  return nr;
}
