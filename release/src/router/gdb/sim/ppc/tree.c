/*  This file is part of the program psim.

    Copyright (C) 1994-1997, Andrew Cagney <cagney@highland.com.au>

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


#ifndef _PARSE_C_
#define _PARSE_C_

#include <stdio.h>
#include <stdarg.h>

#include "basics.h"

#include "device.h"
#include "tree.h"


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

#include <ctype.h>

#include "libiberty.h"

/* manipulate/lookup device names */

typedef struct _name_specifier {
  /* components in the full length name */
  char *path;
  char *property;
  char *value;
  /* current device */
  char *name;
  char *base;
  char *unit;
  char *args;
  /* previous device */
  char *last_name;
  char *last_base;
  char *last_unit;
  char *last_args;
  /* work area */
  char buf[1024];
} name_specifier;



/* Given a device specifier, break it up into its main components:
   path (and if present) property name and property value. */

STATIC_INLINE_TREE\
(int)
split_device_specifier(device *current,
		       const char *device_specifier,
		       name_specifier *spec)
{
  char *chp = NULL;

  /* expand any leading alias if present */
  if (current != NULL
      && *device_specifier != '\0'
      && *device_specifier != '.'
      && *device_specifier != '/') {
    device *aliases = tree_find_device(current, "/aliases");
    char alias[32];
    int len = 0;
    while (device_specifier[len] != '\0'
	   && device_specifier[len] != '/'
	   && device_specifier[len] != ':'
	   && !isspace(device_specifier[len])) {
      alias[len] = device_specifier[len];
      len++;
      if (len >= sizeof(alias))
	error("split_device_specifier: buffer overflow");
    }
    alias[len] = '\0';
    if (aliases != NULL
	&& device_find_property(aliases, alias)) {
      strcpy(spec->buf, device_find_string_property(aliases, alias));
      strcat(spec->buf, device_specifier + len);
    }
    else {
      strcpy(spec->buf, device_specifier);
    }
  }
  else {
    strcpy(spec->buf, device_specifier);
  }

  /* check no overflow */
  if (strlen(spec->buf) >= sizeof(spec->buf))
    error("split_device_specifier: buffer overflow\n");

  /* strip leading spaces */
  chp = spec->buf;
  while (*chp != '\0' && isspace(*chp))
    chp++;
  if (*chp == '\0')
    return 0;

  /* find the path and terminate it with null */
  spec->path = chp;
  while (*chp != '\0' && !isspace(*chp))
    chp++;
  if (*chp != '\0') {
    *chp = '\0';
    chp++;
  }

  /* and any value */
  while (*chp != '\0' && isspace(*chp))
    chp++;
  spec->value = chp;

  /* now go back and chop the property off of the path */
  if (spec->value[0] == '\0') {
    spec->property = NULL; /*not a property*/
    spec->value = NULL;
  }
  else if (spec->value[0] == '>'
	   || spec->value[0] == '<') {
    /* an interrupt spec */
    spec->property = NULL;
  }
  else {
    chp = strrchr(spec->path, '/');
    if (chp == NULL) {
      spec->property = spec->path;
      spec->path = strchr(spec->property, '\0');
    }
    else {
      *chp = '\0';
      spec->property = chp+1;
    }
  }

  /* and mark the rest as invalid */
  spec->name = NULL;
  spec->base = NULL;
  spec->unit = NULL;
  spec->args = NULL;
  spec->last_name = NULL;
  spec->last_base = NULL;
  spec->last_unit = NULL;
  spec->last_args = NULL;

  return 1;
}


/* given a device specifier break it up into its main components -
   path and property name - assuming that the last `device' is a
   property name. */

STATIC_INLINE_DEVICE\
(int)
split_property_specifier(device *current,
			 const char *property_specifier,
			 name_specifier *spec)
{
  if (split_device_specifier(current, property_specifier, spec)) {
    if (spec->property == NULL) {
      /* force the last name to be a property name */
      char *chp = strrchr(spec->path, '/');
      if (chp == NULL) {
	spec->property = spec->path;
	spec->path = strrchr(spec->property, '\0');;
      }
      else {
	*chp = '\0';
	spec->property = chp+1;
      }
    }
    return 1;
  }
  else
    return 0;
}


/* device the next device name and split it up, return 0 when no more
   names to device */

STATIC_INLINE_TREE\
(int)
split_device_name(name_specifier *spec)
{
  char *chp;
  /* remember what came before */
  spec->last_name = spec->name;
  spec->last_base = spec->base;
  spec->last_unit = spec->unit;
  spec->last_args = spec->args;
  /* finished? */
  if (spec->path[0] == '\0') {
    spec->name = NULL;
    spec->base = NULL;
    spec->unit = NULL;
    spec->args = NULL;
    return 0;
  }
  /* break the current device spec from the path */
  spec->name = spec->path;
  chp = strchr(spec->name, '/');
  if (chp == NULL)
    spec->path = strchr(spec->name, '\0');
  else {
    spec->path = chp+1;
    *chp = '\0';
  }
  /* break out the base */
  if (spec->name[0] == '(') {
    chp = strchr(spec->name, ')');
    if (chp == NULL) {
      spec->base = spec->name;
    }
    else {
      *chp = '\0';
      spec->base = spec->name + 1;
      spec->name = chp + 1;
    }
  }
  else {
    spec->base = spec->name;
  }
  /* now break out the unit */
  chp = strchr(spec->name, '@');
  if (chp == NULL) {
    spec->unit = NULL;
    chp = spec->name;
  }
  else {
    *chp = '\0';
    chp += 1;
    spec->unit = chp;
  }
  /* finally any args */
  chp = strchr(chp, ':');
  if (chp == NULL)
    spec->args = NULL;
  else {
    *chp = '\0';
    spec->args = chp+1;
  }
  return 1;
}


/* device the value, returning the next non-space token */

STATIC_INLINE_TREE\
(char *)
split_value(name_specifier *spec)
{
  char *token;
  if (spec->value == NULL)
    return NULL;
  /* skip leading white space */
  while (isspace(spec->value[0]))
    spec->value++;
  if (spec->value[0] == '\0') {
    spec->value = NULL;
    return NULL;
  }
  token = spec->value;
  /* find trailing space */
  while (spec->value[0] != '\0' && !isspace(spec->value[0]))
    spec->value++;
  /* chop this value out */
  if (spec->value[0] != '\0') {
    spec->value[0] = '\0';
    spec->value++;
  }
  return token;
}



/* traverse the path specified by spec starting at current */

STATIC_INLINE_TREE\
(device *)
split_find_device(device *current,
		  name_specifier *spec)
{
  /* strip off (and process) any leading ., .., ./ and / */
  while (1) {
    if (strncmp(spec->path, "/", strlen("/")) == 0) {
      /* cd /... */
      while (current != NULL && device_parent(current) != NULL)
	current = device_parent(current);
      spec->path += strlen("/");
    }
    else if (strncmp(spec->path, "./", strlen("./")) == 0) {
      /* cd ./... */
      current = current;
      spec->path += strlen("./");
    }
    else if (strncmp(spec->path, "../", strlen("../")) == 0) {
      /* cd ../... */
      if (current != NULL && device_parent(current) != NULL)
	current = device_parent(current);
      spec->path += strlen("../");
    }
    else if (strcmp(spec->path, ".") == 0) {
      /* cd . */
      current = current;
      spec->path += strlen(".");
    }
    else if (strcmp(spec->path, "..") == 0) {
      /* cd . */
      if (current != NULL && device_parent(current) != NULL)
	current = device_parent(current);
      spec->path += strlen("..");
    }
    else
      break;
  }

  /* now go through the path proper */

  if (current == NULL) {
    split_device_name(spec);
    return NULL;
  }

  while (split_device_name(spec)) {
    device *child;
    for (child = device_child(current);
	 child != NULL; child = device_sibling(child)) {
      if (strcmp(spec->name, device_name(child)) == 0) {
	if (spec->unit == NULL)
	  break;
	else {
	  device_unit phys;
	  device_decode_unit(current, spec->unit, &phys);
	  if (memcmp(&phys, device_unit_address(child),
		     sizeof(device_unit)) == 0)
	    break;
	}
      }
    }
    if (child == NULL)
      return current; /* search failed */
    current = child;
  }

  return current;
}


STATIC_INLINE_TREE\
(device *)
split_fill_path(device *current,
		const char *device_specifier,
		name_specifier *spec)
{
  /* break it up */
  if (!split_device_specifier(current, device_specifier, spec))
    device_error(current, "error parsing %s\n", device_specifier);

  /* fill our tree with its contents */
  current = split_find_device(current, spec);

  /* add any additional devices as needed */
  if (spec->name != NULL) {
    do {
      current = device_create(current, spec->base, spec->name,
			      spec->unit, spec->args);
    } while (split_device_name(spec));
  }

  return current;
}


INLINE_TREE\
(void)
tree_init(device *root,
	  psim *system)
{
  TRACE(trace_device_tree, ("tree_init(root=0x%lx, system=0x%lx)\n",
			    (long)root,
			    (long)system));
  /* remove the old, rebuild the new */
  tree_traverse(root, device_clean, NULL, system);
  tree_traverse(root, device_init_static_properties, NULL, system);
  tree_traverse(root, device_init_address, NULL, system);
  tree_traverse(root, device_init_runtime_properties, NULL, system);
  tree_traverse(root, device_init_data, NULL, system);
}



/* <non-white-space> */

STATIC_INLINE_TREE\
(const char *)
skip_token(const char *chp)
{
  while (!isspace(*chp) && *chp != '\0')
    chp++;
  while (isspace(*chp) && *chp != '\0')
    chp++;
  return chp;
}


/* count the number of entries */

STATIC_INLINE_TREE\
(int)
count_entries(device *current,
	      const char *property_name,
	      const char *property_value,
	      int modulo)
{
  const char *chp = property_value;
  int nr_entries = 0;
  while (*chp != '\0') {
    nr_entries += 1;
    chp = skip_token(chp);
  }
  if ((nr_entries % modulo) != 0) {
    device_error(current, "incorrect number of entries for %s property %s, should be multiple of %d",
		 property_name, property_value, modulo);
  }
  return nr_entries / modulo;
}



/* parse: <address> ::= <token> ; device dependant */

STATIC_INLINE_TREE\
(const char *)
parse_address(device *current,
	      device *bus,
	      const char *chp,
	      device_unit *address)
{
  ASSERT(device_nr_address_cells(bus) > 0);
  if (device_decode_unit(bus, chp, address) < 0)
    device_error(current, "invalid unit address in %s", chp);
  return skip_token(chp);
}


/* parse: <size> ::= <number> { "," <number> } ; */

STATIC_INLINE_TREE\
(const char *)
parse_size(device *current,
	   device *bus,
	   const char *chp,
	   device_unit *size)
{
  int i;
  int nr;
  const char *curr = chp;
  memset(size, 0, sizeof(*size));
  /* parse the numeric list */
  size->nr_cells = device_nr_size_cells(bus);
  nr = 0;
  ASSERT(size->nr_cells > 0);
  while (1) {
    char *next;
    size->cells[nr] = strtoul(curr, &next, 0);
    if (curr == next)
      device_error(current, "Problem parsing <size> %s", chp);
    nr += 1;
    if (next[0] != ',')
      break;
    if (nr == size->nr_cells)
      device_error(current, "Too many values in <size> %s", chp);
    curr = next + 1;
  }
  ASSERT(nr > 0 && nr <= size->nr_cells);
  /* right align the numbers */
  for (i = 1; i <= size->nr_cells; i++) {
    if (i <= nr)
      size->cells[size->nr_cells - i] = size->cells[nr - i];
    else
      size->cells[size->nr_cells - i] = 0;
  }
  return skip_token(chp);
}


/* parse: <reg> ::= { <address> <size> } ; */

STATIC_INLINE_TREE\
(void)
parse_reg_property(device *current,
		   const char *property_name,
		   const char *property_value)
{
  int nr_regs;
  int reg_nr;
  reg_property_spec *regs;
  const char *chp;
  device *bus = device_parent(current);

  /* determine the number of reg entries by counting tokens */
  nr_regs = count_entries(current, property_name, property_value,
			  1 + (device_nr_size_cells(bus) > 0));

  /* create working space */
  regs = zalloc(nr_regs * sizeof(*regs));

  /* fill it in */
  chp = property_value;
  for (reg_nr = 0; reg_nr < nr_regs; reg_nr++) {
    chp = parse_address(current, bus, chp, &regs[reg_nr].address);
    if (device_nr_size_cells(bus) > 0)
      chp = parse_size(current, bus, chp, &regs[reg_nr].size);
    else
      memset(&regs[reg_nr].size, 0, sizeof (&regs[reg_nr].size));
  }

  /* create it */
  device_add_reg_array_property(current, property_name,
				regs, nr_regs);

  zfree(regs);
}


/* { <child-address> <parent-address> <child-size> }* */

STATIC_INLINE_TREE\
(void)
parse_ranges_property(device *current,
		      const char *property_name,
		      const char *property_value)
{
  int nr_ranges;
  int range_nr;
  range_property_spec *ranges;
  const char *chp;
  
  /* determine the number of ranges specified */
  nr_ranges = count_entries(current, property_name, property_value, 3);

  /* create a property of that size */
  ranges = zalloc(nr_ranges * sizeof(*ranges));

  /* fill it in */
  chp = property_value;
  for (range_nr = 0; range_nr < nr_ranges; range_nr++) {
    chp = parse_address(current, current,
			chp, &ranges[range_nr].child_address);
    chp = parse_address(current, device_parent(current),
			chp, &ranges[range_nr].parent_address);
    chp = parse_size(current, current,
		     chp, &ranges[range_nr].size);
  }

  /* create it */
  device_add_range_array_property(current, property_name, ranges, nr_ranges);

  zfree(ranges);
}


/* <integer> ... */

STATIC_INLINE_TREE\
(void)
parse_integer_property(device *current,
		       const char *property_name,
		       const char *property_value)
{
  int nr_entries;
  unsigned_cell words[1024];
  /* integer or integer array? */
  nr_entries = 0;
  while (1) {
    char *end;
    words[nr_entries] = strtoul(property_value, &end, 0);
    if (property_value == end)
      break;
    nr_entries += 1;
    if (nr_entries * sizeof(words[0]) >= sizeof(words))
      device_error(current, "buffer overflow");
    property_value = end;
  }
  if (nr_entries == 0)
    device_error(current, "error parsing integer property %s (%s)",
                 property_name, property_value);
  else if (nr_entries == 1)
    device_add_integer_property(current, property_name, words[0]);
  else {
    int i;
    for (i = 0; i < nr_entries; i++) {
      H2BE(words[i]);
    }
    /* perhaps integer array property is better */
    device_add_array_property(current, property_name, words,
                              sizeof(words[0]) * nr_entries);
  }
}

/* PROPERTY_VALUE is a raw property value.  Quote it as required by
   parse_string_property.  It is the caller's responsibility to free
   the memory returned.  */

EXTERN_TREE\
(char *)
tree_quote_property(const char *property_value)
{
  char *p;
  char *ret;
  const char *chp;
  int quotees;

  /* Count characters needing quotes in PROPERTY_VALUE.  */
  quotees = 0;
  for (chp = property_value; *chp; ++chp)
    if (*chp == '\\' || *chp == '"')
      ++quotees;
  
  ret = (char *) xmalloc (strlen (property_value) 
			  + 2 /* quotes */
			  + quotees
			  + 1 /* terminator */);

  p = ret;
  /* Add the opening quote.  */
  *p++ = '"';
  /* Copy the value.  */
  for (chp = property_value; *chp; ++chp)
    if (*chp == '\\' || *chp == '"')
      {
	/* Quote this character.  */ 
	*p++ = '\\';
	*p++ = *chp;
      }
    else
      *p++ = *chp;
  /* Add the closing quote.  */
  *p++ = '"';
  /* Terminate the string.  */
  *p++ = '\0';

  return ret;
}

/* <string> ... */

STATIC_INLINE_TREE\
(void)
parse_string_property(device *current,
		      const char *property_name,
		      const char *property_value)
{
  char **strings;
  const char *chp;
  int nr_strings;
  int approx_nr_strings;

  /* get an estimate as to the number of strings by counting double
     quotes */
  approx_nr_strings = 2;
  for (chp = property_value; *chp; chp++) {
    if (*chp == '"')
      approx_nr_strings++;
  }
  approx_nr_strings = (approx_nr_strings) / 2;

  /* create a string buffer for that many (plus a null) */
  strings = (char**)zalloc((approx_nr_strings + 1) * sizeof(char*));

  /* now find all the strings */
  chp = property_value;
  nr_strings = 0;
  while (1) {

    /* skip leading space */
    while (*chp != '\0' && isspace(*chp))
      chp += 1;
    if (*chp == '\0')
      break;

    /* copy it in */
    if (*chp == '"') {
      /* a quoted string - watch for '\' et.al. */
      /* estimate the size and allocate space for it */
      int pos;
      chp++;
      pos = 0;
      while (chp[pos] != '\0' && chp[pos] != '"') {
	if (chp[pos] == '\\' && chp[pos+1] != '\0')
	  pos += 2;
	else
	  pos += 1;
      }
      strings[nr_strings] = zalloc(pos + 1);
      /* copy the string over */
      pos = 0;
      while (*chp != '\0' && *chp != '"') {
	if (*chp == '\\' && *(chp+1) != '\0') {
	  strings[nr_strings][pos] = *(chp+1);
	  chp += 2;
	  pos++;
	}
	else {
	  strings[nr_strings][pos] = *chp;
	  chp += 1;
	  pos++;
	}
      }
      if (*chp != '\0')
	chp++;
      strings[nr_strings][pos] = '\0';
    }
    else {
      /* copy over a single unquoted token */
      int len = 0;
      while (chp[len] != '\0' && !isspace(chp[len]))
	len++;
      strings[nr_strings] = zalloc(len + 1);
      strncpy(strings[nr_strings], chp, len);
      strings[nr_strings][len] = '\0';
      chp += len;
    }
    nr_strings++;
    if (nr_strings > approx_nr_strings)
      device_error(current, "String property %s badly formatted",
		   property_name);
  }
  ASSERT(strings[nr_strings] == NULL); /* from zalloc */

  /* install it */
  if (nr_strings == 0)
    device_add_string_property(current, property_name, "");
  else if (nr_strings == 1)
    device_add_string_property(current, property_name, strings[0]);
  else {
    const char **specs = (const char**)strings; /* stop a bogus error */
    device_add_string_array_property(current, property_name,
				     specs, nr_strings);
  }

  /* flush the created string */
  while (nr_strings > 0) {
    nr_strings--;
    zfree(strings[nr_strings]);
  }
  zfree(strings);
}


/* <path-to-ihandle-device> */

STATIC_INLINE_TREE\
(void)
parse_ihandle_property(device *current,
		       const char *property,
		       const char *value)
{
  ihandle_runtime_property_spec ihandle;

  /* pass the full path */
  ihandle.full_path = value;

  /* save this ready for the ihandle create */
  device_add_ihandle_runtime_property(current, property,
				      &ihandle);
}



EXTERN_TREE\
(device *)
tree_parse(device *current,
	   const char *fmt,
	   ...)
{
  char device_specifier[1024];
  name_specifier spec;

  /* format the path */
  {
    va_list ap;
    va_start(ap, fmt);
    vsprintf(device_specifier, fmt, ap);
    va_end(ap);
    if (strlen(device_specifier) >= sizeof(device_specifier))
      error("device_tree_add_deviced: buffer overflow\n");
  }

  /* construct the tree down to the final device */
  current = split_fill_path(current, device_specifier, &spec);

  /* is there an interrupt spec */
  if (spec.property == NULL
      && spec.value != NULL) {
    char *op = split_value(&spec);
    switch (op[0]) {
    case '>':
      {
	char *my_port_name = split_value(&spec);
	int my_port;
	char *dest_port_name = split_value(&spec);
	int dest_port;
	name_specifier dest_spec;
	char *dest_device_name = split_value(&spec);
	device *dest;
	/* find my name */
	my_port = device_interrupt_decode(current, my_port_name,
					  output_port);
	/* find the dest device and port */
	dest = split_fill_path(current, dest_device_name, &dest_spec);
	dest_port = device_interrupt_decode(dest, dest_port_name,
					    input_port);
	/* connect the two */
	device_interrupt_attach(current,
				my_port,
				dest,
				dest_port,
				permenant_object);
      }
      break;
    default:
      device_error(current, "unreconised interrupt spec %s\n", spec.value);
      break;
    }
  }

  /* is there a property */
  if (spec.property != NULL) {
    if (strcmp(spec.value, "true") == 0)
      device_add_boolean_property(current, spec.property, 1);
    else if (strcmp(spec.value, "false") == 0)
      device_add_boolean_property(current, spec.property, 0);
    else {
      const device_property *property;
      switch (spec.value[0]) {
      case '*':
	parse_ihandle_property(current, spec.property, spec.value + 1);
	break;
      case '[':
	{
	  unsigned8 words[1024];
	  char *curr = spec.value + 1;
	  int nr_words = 0;
	  while (1) {
	    char *next;
	    words[nr_words] = H2BE_1(strtoul(curr, &next, 0));
	    if (curr == next)
	      break;
	    curr = next;
	    nr_words += 1;
	  }
	  device_add_array_property(current, spec.property,
				    words, sizeof(words[0]) * nr_words);
	}
	break;
      case '"':
	parse_string_property(current, spec.property, spec.value);
	break;
      case '!':
	spec.value++;
	property = tree_find_property(current, spec.value);
	if (property == NULL)
	  device_error(current, "property %s not found\n", spec.value);
	device_add_duplicate_property(current,
				      spec.property,
				      property);
	break;
      default:
	if (strcmp(spec.property, "reg") == 0
	    || strcmp(spec.property, "assigned-addresses") == 0
	    || strcmp(spec.property, "alternate-reg") == 0){
	  parse_reg_property(current, spec.property, spec.value);
	}
	else if (strcmp(spec.property, "ranges") == 0) {
	  parse_ranges_property(current, spec.property, spec.value);
	}
	else if (isdigit(spec.value[0])
		 || (spec.value[0] == '-' && isdigit(spec.value[1]))
		 || (spec.value[0] == '+' && isdigit(spec.value[1]))) {
	  parse_integer_property(current, spec.property, spec.value);
	}
	else
	  parse_string_property(current, spec.property, spec.value);
	break;
      }
    }
  }
  return current;
}


INLINE_TREE\
(void)
tree_traverse(device *root,
	      tree_traverse_function *prefix,
	      tree_traverse_function *postfix,
	      void *data)
{
  device *child;
  if (prefix != NULL)
    prefix(root, data);
  for (child = device_child(root);
       child != NULL;
       child = device_sibling(child)) {
    tree_traverse(child, prefix, postfix, data);
  }
  if (postfix != NULL)
    postfix(root, data);
}


STATIC_INLINE_TREE\
(void)
print_address(device *bus,
	      const device_unit *phys)
{
  char unit[32];
  device_encode_unit(bus, phys, unit, sizeof(unit));
  printf_filtered(" %s", unit);
}

STATIC_INLINE_TREE\
(void)
print_size(device *bus,
	   const device_unit *size)
{
  int i;
  for (i = 0; i < size->nr_cells; i++)
    if (size->cells[i] != 0)
      break;
  if (i < size->nr_cells) {
    printf_filtered(" 0x%lx", (unsigned long)size->cells[i]);
    i++;
    for (; i < size->nr_cells; i++)
      printf_filtered(",0x%lx", (unsigned long)size->cells[i]);
  }
  else
    printf_filtered(" 0");
}

STATIC_INLINE_TREE\
(void)
print_reg_property(device *me,
		   const device_property *property)
{
  int reg_nr;
  reg_property_spec reg;
  for (reg_nr = 0;
       device_find_reg_array_property(me, property->name, reg_nr, &reg);
       reg_nr++) {
    print_address(device_parent(me), &reg.address);
    print_size(me, &reg.size);
  }
}

STATIC_INLINE_TREE\
(void)
print_ranges_property(device *me,
		      const device_property *property)
{
  int range_nr;
  range_property_spec range;
  for (range_nr = 0;
       device_find_range_array_property(me, property->name, range_nr, &range);
       range_nr++) {
    print_address(me, &range.child_address);
    print_address(device_parent(me), &range.parent_address);
    print_size(me, &range.size);
  }
}

STATIC_INLINE_TREE\
(void)
print_string(const char *string)
{
  printf_filtered(" \"");
  while (*string != '\0') {
    switch (*string) {
    case '"':
      printf_filtered("\\\"");
      break;
    case '\\':
      printf_filtered("\\\\");
      break;
    default:
      printf_filtered("%c", *string);
      break;
    }
    string++;
  }
  printf_filtered("\"");
}

STATIC_INLINE_TREE\
(void)
print_string_array_property(device *me,
			    const device_property *property)
{
  int nr;
  string_property_spec string;
  for (nr = 0;
       device_find_string_array_property(me, property->name, nr, &string);
       nr++) {
    print_string(string);
  }
}

STATIC_INLINE_TREE\
(void)
print_properties(device *me)
{
  const device_property *property;
  for (property = device_find_property(me, NULL);
       property != NULL;
       property = device_next_property(property)) {
    printf_filtered("%s/%s", device_path(me), property->name);
    if (property->original != NULL) {
      printf_filtered(" !");
      printf_filtered("%s/%s", 
		      device_path(property->original->owner),
		      property->original->name);
    }
    else {
      switch (property->type) {
      case array_property:
	if ((property->sizeof_array % sizeof(signed_cell)) == 0) {
	  unsigned_cell *w = (unsigned_cell*)property->array;
	  int cell_nr;
	  for (cell_nr = 0;
	       cell_nr < (property->sizeof_array / sizeof(unsigned_cell));
	       cell_nr++) {
	    printf_filtered(" 0x%lx", (unsigned long)BE2H_cell(w[cell_nr]));
	  }
	}
	else {
	  unsigned8 *w = (unsigned8*)property->array;
	  printf_filtered(" [");
	  while ((char*)w - (char*)property->array < property->sizeof_array) {
	    printf_filtered(" 0x%2x", BE2H_1(*w));
	    w++;
	  }
	}
	break;
      case boolean_property:
	{
	  int b = device_find_boolean_property(me, property->name);
	  printf_filtered(" %s", b ? "true"  : "false");
	}
	break;
      case ihandle_property:
	{
	  if (property->array != NULL) {
	    device_instance *instance = device_find_ihandle_property(me, property->name);
	    printf_filtered(" *%s", device_instance_path(instance));
	  }
	  else {
	    /* not yet initialized, ask the device for the path */
	    ihandle_runtime_property_spec spec;
	    device_find_ihandle_runtime_property(me, property->name, &spec);
	    printf_filtered(" *%s", spec.full_path);
	  }
	}
	break;
      case integer_property:
	{
	  unsigned_word w = device_find_integer_property(me, property->name);
	  printf_filtered(" 0x%lx", (unsigned long)w);
	}
	break;
      case range_array_property:
	print_ranges_property(me, property);
	break;
      case reg_array_property:
	print_reg_property(me, property);
	break;
      case string_property:
	{
	  const char *s = device_find_string_property(me, property->name);
	  print_string(s);
	}
	break;
      case string_array_property:
	print_string_array_property(me, property);
	break;
      }
    }
    printf_filtered("\n");
  }
}

STATIC_INLINE_TREE\
(void)
print_interrupts(device *me,
		 int my_port,
		 device *dest,
		 int dest_port,
		 void *ignore_or_null)
{
  char src[32];
  char dst[32];
  device_interrupt_encode(me, my_port, src, sizeof(src), output_port);
  device_interrupt_encode(dest, dest_port, dst, sizeof(dst), input_port);
  printf_filtered("%s > %s %s %s\n",
		  device_path(me),
		  src, dst,
		  device_path(dest));
}

STATIC_INLINE_TREE\
(void)
print_device(device *me,
	     void *ignore_or_null)
{
  printf_filtered("%s\n", device_path(me));
  print_properties(me);
  device_interrupt_traverse(me, print_interrupts, NULL);
}

INLINE_TREE\
(void)
tree_print(device *root)
{
  tree_traverse(root,
		print_device, NULL,
		NULL);
}


INLINE_TREE\
(void)
tree_usage(int verbose)
{
  if (verbose == 1) {
    printf_filtered("\n");
    printf_filtered("A device/property specifier has the form:\n");
    printf_filtered("\n");
    printf_filtered("  /path/to/a/device [ property-value ]\n");
    printf_filtered("\n");
    printf_filtered("and a possible device is\n");
    printf_filtered("\n");
  }
  if (verbose > 1) {
    printf_filtered("\n");
    printf_filtered("A device/property specifier (<spec>) has the format:\n");
    printf_filtered("\n");
    printf_filtered("  <spec> ::= <path> [ <value> ] ;\n");
    printf_filtered("  <path> ::= { <prefix> } { <node> \"/\" } <node> ;\n");
    printf_filtered("  <prefix> ::= ( | \"/\" | \"../\" | \"./\" ) ;\n");
    printf_filtered("  <node> ::= <name> [ \"@\" <unit> ] [ \":\" <args> ] ;\n");
    printf_filtered("  <unit> ::= <number> { \",\" <number> } ;\n");
    printf_filtered("\n");
    printf_filtered("Where:\n");
    printf_filtered("\n");
    printf_filtered("  <name>  is the name of a device (list below)\n");
    printf_filtered("  <unit>  is the unit-address relative to the parent bus\n");
    printf_filtered("  <args>  additional arguments used when creating the device\n");
    printf_filtered("  <value> ::= ( <number> # integer property\n");
    printf_filtered("              | \"[\" { <number> } # array property (byte)\n");
    printf_filtered("              | \"{\" { <number> } # array property (cell)\n");
    printf_filtered("              | [ \"true\" | \"false\" ] # boolean property\n");
    printf_filtered("              | \"*\" <path> # ihandle property\n");
    printf_filtered("              | \"!\" <path> # copy property\n");
    printf_filtered("              | \">\" [ <number> ] <path> # attach interrupt\n");
    printf_filtered("              | \"<\" <path> # attach child interrupt\n");
    printf_filtered("              | \"\\\"\" <text> # string property\n");
    printf_filtered("              | <text> # string property\n");
    printf_filtered("              ) ;\n");
    printf_filtered("\n");
    printf_filtered("And the following are valid device names:\n");
    printf_filtered("\n");
  }
}



INLINE_TREE\
(device_instance *)
tree_instance(device *root,
	      const char *device_specifier)
{
  /* find the device node */
  device *me;
  name_specifier spec;
  if (!split_device_specifier(root, device_specifier, &spec))
    return NULL;
  me = split_find_device(root, &spec);
  if (spec.name != NULL)
    return NULL;
  /* create the instance */
  return device_create_instance(me, device_specifier, spec.last_args);
}


INLINE_TREE\
(device *)
tree_find_device(device *root,
		 const char *path_to_device)
{
  device *node;
  name_specifier spec;

  /* parse the path */
  split_device_specifier(root, path_to_device, &spec);
  if (spec.value != NULL)
    return NULL; /* something wierd */

  /* now find it */
  node = split_find_device(root, &spec);
  if (spec.name != NULL)
    return NULL; /* not a leaf */

  return node;
}


INLINE_TREE\
(const device_property *)
tree_find_property(device *root,
		   const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier(root, path_to_property, &spec))
    device_error(root, "Invalid property path %s", path_to_property);
  root = split_find_device(root, &spec);
  return device_find_property(root, spec.property);
}

INLINE_TREE\
(int)
tree_find_boolean_property(device *root,
			   const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier(root, path_to_property, &spec))
    device_error(root, "Invalid property path %s", path_to_property);
  root = split_find_device(root, &spec);
  return device_find_boolean_property(root, spec.property);
}

INLINE_TREE\
(signed_cell)
tree_find_integer_property(device *root,
			   const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier(root, path_to_property, &spec))
    device_error(root, "Invalid property path %s", path_to_property);
  root = split_find_device(root, &spec);
  return device_find_integer_property(root, spec.property);
}

INLINE_TREE\
(device_instance *)
tree_find_ihandle_property(device *root,
			   const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier(root, path_to_property, &spec))
    device_error(root, "Invalid property path %s", path_to_property);
  root = split_find_device(root, &spec);
  return device_find_ihandle_property(root, spec.property);
}

INLINE_TREE\
(const char *)
tree_find_string_property(device *root,
			  const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier(root, path_to_property, &spec))
    device_error(root, "Invalid property path %s", path_to_property);
  root = split_find_device(root, &spec);
  return device_find_string_property(root, spec.property);
}


#endif /* _PARSE_C_ */
