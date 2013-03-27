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

struct _lf {
  FILE *stream;
  int line_nr; /* nr complete lines written, curr line is line_nr+1 */
  int indent;
  int line_blank;
  const char *name;
  const char *program;
  lf_file_references references;
  lf_file_type type;
};


lf *
lf_open(char *name,
	char *real_name,
	lf_file_references references,
	lf_file_type type,
	const char *program)
{
  /* create a file object */
  lf *new_lf = ZALLOC(lf);
  ASSERT(new_lf != NULL);
  new_lf->references = references;
  new_lf->type = type;
  new_lf->name = (real_name == NULL ? name : real_name);
  new_lf->program = program;
  /* attach to stdout if pipe */
  if (!strcmp(name, "-")) {
    new_lf->stream = stdout;
  }
  else {
    /* create a new file */
    new_lf->stream = fopen(name, "w");
    if (new_lf->stream == NULL) {
      perror(name);
      exit(1);
    }
  }
  return new_lf;
}


void
lf_close(lf *file)
{
  if (file->stream != stdout) {
    if (fclose(file->stream)) {
      perror("lf_close.fclose");
      exit(1);
    }
    free(file);
  }
}


int
lf_putchr(lf *file,
	  const char chr)
{
  int nr = 0;
  if (chr == '\n') {
    file->line_nr += 1;
    file->line_blank = 1;
  }
  else if (file->line_blank) {
    int pad;
    for (pad = file->indent; pad > 0; pad--)
      putc(' ', file->stream);
    nr += file->indent;
    file->line_blank = 0;
  }
  putc(chr, file->stream);
  nr += 1;
  return nr;
}

void
lf_indent_suppress(lf *file)
{
  file->line_blank = 0;
}


int
lf_putstr(lf *file,
	  const char *string)
{
  int nr = 0;
  const char *chp;
  if (string != NULL) {
    for (chp = string; *chp != '\0'; chp++) {
      nr += lf_putchr(file, *chp);
    }
  }
  return nr;
}

static int
do_lf_putunsigned(lf *file,
	      unsigned u)
{
  int nr = 0;
  if (u > 0) {
    nr += do_lf_putunsigned(file, u / 10);
    nr += lf_putchr(file, (u % 10) + '0');
  }
  return nr;
}


int
lf_putint(lf *file,
	  int decimal)
{
  int nr = 0;
  if (decimal == 0)
    nr += lf_putchr(file, '0');
  else if (decimal < 0) {
    nr += lf_putchr(file, '-');
    nr += do_lf_putunsigned(file, -decimal);
  }
  else if (decimal > 0) {
    nr += do_lf_putunsigned(file, decimal);
  }
  else
    ASSERT(0);
  return nr;
}


int
lf_printf(lf *file,
	  const char *fmt,
	  ...)
{
  int nr = 0;
  char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  vsprintf(buf, fmt, ap);
  /* FIXME - this is really stuffed but so is vsprintf() on a sun! */
  ASSERT(strlen(buf) > 0 && strlen(buf) < sizeof(buf));
  nr += lf_putstr(file, buf);
  va_end(ap);
  return nr;
}


int
lf_print__c_code(lf *file,
		 const char *code)
{
  int nr = 0;
  const char *chp = code;
  int in_bit_field = 0;
  while (*chp != '\0') {
    if (*chp == '\t')
      chp++;
    if (*chp == '#')
      lf_indent_suppress(file);
    while (*chp != '\0' && *chp != '\n') {
      if (chp[0] == '{' && !isspace(chp[1])) {
	in_bit_field = 1;
	nr += lf_putchr(file, '_');
      }
      else if (in_bit_field && chp[0] == ':') {
	nr += lf_putchr(file, '_');
      }
      else if (in_bit_field && *chp == '}') {
	nr += lf_putchr(file, '_');
	in_bit_field = 0;
      }
      else {
	nr += lf_putchr(file, *chp);
      }
      chp++;
    }
    if (in_bit_field)
      error("bit field paren miss match some where\n");
    if (*chp == '\n') {
      nr += lf_putchr(file, '\n');
      chp++;
    }
  }
  nr += lf_putchr(file, '\n');
  return nr;
}


int
lf_print__external_reference(lf *file,
			     int line_nr,
			     const char *file_name)
{
  int nr = 0;
  switch (file->references) {
  case lf_include_references:
    lf_indent_suppress(file);
    nr += lf_putstr(file, "#line ");
    nr += lf_putint(file, line_nr);
    nr += lf_putstr(file, " \"");
    nr += lf_putstr(file, file_name);
    nr += lf_putstr(file, "\"\n");
    break;
  case lf_omit_references:
    break;
  }
  return nr;
}

int
lf_print__internal_reference(lf *file)
{
  int nr = 0;
  nr += lf_print__external_reference(file, file->line_nr+2, file->name);
  /* line_nr == last_line, want to number from next */
  return nr;
}

void
lf_indent(lf *file, int delta)
{
  file->indent += delta;
}


int
lf_print__gnu_copyleft(lf *file)
{
  int nr = 0;
  switch (file->type) {
  case lf_is_c:
  case lf_is_h:
    nr += lf_printf(file, "\n\
/*  This file is part of the program psim.\n\
\n\
    Copyright (C) 1994-1995, Andrew Cagney <cagney@highland.com.au>\n\
\n\
    This program is free software; you can redistribute it and/or modify\n\
    it under the terms of the GNU General Public License as published by\n\
    the Free Software Foundation; either version 2 of the License, or\n\
    (at your option) any later version.\n\
\n\
    This program is distributed in the hope that it will be useful,\n\
    but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
    GNU General Public License for more details.\n\
 \n\
    You should have received a copy of the GNU General Public License\n\
    along with this program; if not, write to the Free Software\n\
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.\n\
 \n\
    --\n\
\n\
    This file was generated by the program %s */\n\
", filter_filename(file->program));
    break;
  default:
    ASSERT(0);
    break;
  }
  return nr;
}


int
lf_putbin(lf *file, int decimal, int width)
{
  int nr = 0;
  int bit;
  ASSERT(width > 0);
  for (bit = 1 << (width-1); bit != 0; bit >>= 1) {
    if (decimal & bit)
      nr += lf_putchr(file, '1');
    else
      nr += lf_putchr(file, '0');
  }
  return nr;
}

int
lf_print__this_file_is_empty(lf *file)
{
  int nr = 0;
  switch (file->type) {
  case lf_is_c:
  case lf_is_h:
    nr += lf_printf(file,
		    "/* This generated file (%s) is intentionally left blank */\n",
		    file->name);
    break;
  default:
    ASSERT(0);
  }
  return nr;
}

int
lf_print__ucase_filename(lf *file)
{
  int nr = 0;
  const char *chp = file->name;
  while (*chp != '\0') {
    char ch = *chp;
    if (islower(ch)) {
      nr += lf_putchr(file, toupper(ch));
    }
    else if (ch == '.')
      nr += lf_putchr(file, '_');
    else
      nr += lf_putchr(file, ch);
    chp++;
  }
  return nr;
}

int
lf_print__file_start(lf *file)
{
  int nr = 0;
  switch (file->type) {
  case lf_is_h:
  case lf_is_c:
    nr += lf_print__gnu_copyleft(file);
    nr += lf_printf(file, "\n");
    nr += lf_printf(file, "#ifndef _");
    nr += lf_print__ucase_filename(file);
    nr += lf_printf(file, "_\n");
    nr += lf_printf(file, "#define _");
    nr += lf_print__ucase_filename(file);
    nr += lf_printf(file, "_\n");
    nr += lf_printf(file, "\n");
    break;
  default:
    ASSERT(0);
  }
  return nr;
}


int
lf_print__file_finish(lf *file)
{
  int nr = 0;
  switch (file->type) {
  case lf_is_h:
  case lf_is_c:
    nr += lf_printf(file, "\n");
    nr += lf_printf(file, "#endif /* _");
    nr += lf_print__ucase_filename(file);
    nr += lf_printf(file, "_*/\n");
    break;
  default:
    ASSERT(0);
  }
  return nr;
}


int
lf_print_function_type(lf *file,
		       const char *type,
		       const char *prefix,
		       const char *trailing_space)
{
  int nr = 0;
  nr += lf_printf(file, "%s\\\n(%s)", prefix, type);
  if (trailing_space != NULL)
    nr += lf_printf(file, "%s", trailing_space);
#if 0
  const char *type_pointer = strrchr(type, '*');
  int type_pointer_offset = (type_pointer != NULL
			     ? type_pointer - type
			     : 0);
  if (type_pointer == NULL) {
    lf_printf(file, "%s %s", type, prefix);
  }
  else {
    char *munged_type = (char*)zalloc(strlen(type)
				      + strlen(prefix)
				      + strlen(" * ")
				      + 1);
    strcpy(munged_type, type);
    munged_type[type_pointer_offset] = '\0';
    if (type_pointer_offset > 0 && type[type_pointer_offset-1] != ' ')
      strcat(munged_type, " ");
    strcat(munged_type, prefix);
    strcat(munged_type, " ");
    strcat(munged_type, type + type_pointer_offset);
    lf_printf(file, "%s", munged_type);
    free(munged_type);
  }
  if (trailing_space != NULL && type_pointer_offset < strlen(type) - 1)
    lf_printf(file, trailing_space);
#endif
  return nr;
}

