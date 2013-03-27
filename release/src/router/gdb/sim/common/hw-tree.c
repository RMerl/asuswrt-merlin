/* The common simulator framework for GDB, the GNU Debugger.

   Copyright 2002, 2007 Free Software Foundation, Inc.

   Contributed by Andrew Cagney and Red Hat.

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


#include "hw-main.h"
#include "hw-base.h"
#include "hw-tree.h"

#include "sim-io.h"
#include "sim-assert.h"

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

/* manipulate/lookup device names */

typedef struct _name_specifier {
  
  /* components in the full length name */
  char *path;
  char *property;
  char *value;
  
  /* current device */
  char *family;
  char *name;
  char *unit;
  char *args;
  
  /* previous device */
  char *last_name;
  char *last_family;
  char *last_unit;
  char *last_args;
  
  /* work area */
  char buf[1024];
  
} name_specifier;



/* Given a device specifier, break it up into its main components:
   path (and if present) property name and property value. */

static int
split_device_specifier (struct hw *current,
			const char *device_specifier,
			name_specifier *spec)
{
  char *chp = NULL;
  
  /* expand any leading alias if present */
  if (current != NULL
      && *device_specifier != '\0'
      && *device_specifier != '.'
      && *device_specifier != '/')
    {
      struct hw *aliases = hw_tree_find_device (current, "/aliases");
      char alias[32];
      int len = 0;
      while (device_specifier[len] != '\0'
	     && device_specifier[len] != '/'
	     && device_specifier[len] != ':'
	     && !isspace (device_specifier[len]))
	{
	  alias[len] = device_specifier[len];
	  len++;
	  if (len >= sizeof(alias))
	    hw_abort (NULL, "split_device_specifier: buffer overflow");
	}
      alias[len] = '\0';
      if (aliases != NULL
	  && hw_find_property (aliases, alias))
	{
	  strcpy (spec->buf, hw_find_string_property(aliases, alias));
	  strcat (spec->buf, device_specifier + len);
	}
      else
	{
	  strcpy (spec->buf, device_specifier);
	}
    }
  else
    {
      strcpy(spec->buf, device_specifier);
    }
  
  /* check no overflow */
  if (strlen(spec->buf) >= sizeof(spec->buf))
    hw_abort (NULL, "split_device_specifier: buffer overflow\n");
  
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
  if (*chp != '\0')
    {
      *chp = '\0';
      chp++;
    }
  
  /* and any value */
  while (*chp != '\0' && isspace(*chp))
    chp++;
  spec->value = chp;
  
  /* now go back and chop the property off of the path */
  if (spec->value[0] == '\0')
    {
      spec->property = NULL; /*not a property*/
      spec->value = NULL;
    }
  else if (spec->value[0] == '>'
	   || spec->value[0] == '<')
    {
      /* an interrupt spec */
      spec->property = NULL;
    }
  else {
    chp = strrchr(spec->path, '/');
    if (chp == NULL)
      {
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
  spec->family = NULL;
  spec->unit = NULL;
  spec->args = NULL;
  spec->last_name = NULL;
  spec->last_family = NULL;
  spec->last_unit = NULL;
  spec->last_args = NULL;
  
  return 1;
}


/* given a device specifier break it up into its main components -
   path and property name - assuming that the last `device' is a
   property name. */

static int
split_property_specifier (struct hw *current,
			  const char *property_specifier,
			  name_specifier *spec)
{
  if (split_device_specifier (current, property_specifier, spec))
    {
      if (spec->property == NULL)
	{
	  /* force the last name to be a property name */
	  char *chp = strrchr (spec->path, '/');
	  if (chp == NULL)
	    {
	      spec->property = spec->path;
	      spec->path = strrchr (spec->property, '\0');;
	    }
	  else
	    {
	      *chp = '\0';
	      spec->property = chp + 1;
	    }
	}
      return 1;
    }
  else
    return 0;
}


/* device the next device name and split it up, return 0 when no more
   names to struct hw */

static int
split_device_name (name_specifier *spec)
{
  char *chp;
  /* remember what came before */
  spec->last_name = spec->name;
  spec->last_family = spec->family;
  spec->last_unit = spec->unit;
  spec->last_args = spec->args;
  /* finished? */
  if (spec->path[0] == '\0')
    {
      spec->name = NULL;
      spec->family = NULL;
      spec->unit = NULL;
      spec->args = NULL;
      return 0;
    }
  /* break the current device spec from the path */
  spec->name = spec->path;
  chp = strchr (spec->name, '/');
  if (chp == NULL)
    spec->path = strchr (spec->name, '\0');
  else 
    {
      spec->path = chp+1;
      *chp = '\0';
    }
  /* break out the base */
  if (spec->name[0] == '(')
    {
      chp = strchr(spec->name, ')');
      if (chp == NULL)
	{
	  spec->family = spec->name;
	}
      else
	{
	  *chp = '\0';
	  spec->family = spec->name + 1;
	  spec->name = chp + 1;
	}
    }
  else
    {
      spec->family = spec->name;
    }
  /* now break out the unit */
  chp = strchr(spec->name, '@');
  if (chp == NULL)
    {
      spec->unit = NULL;
      chp = spec->name;
    }
  else
    {
      *chp = '\0';
      chp += 1;
      spec->unit = chp;
    }
  /* finally any args */
  chp = strchr(chp, ':');
  if (chp == NULL)
    spec->args = NULL;
  else
    {
      *chp = '\0';
      spec->args = chp+1;
    }
  return 1;
}


/* device the value, returning the next non-space token */

static char *
split_value (name_specifier *spec)
{
  char *token;
  if (spec->value == NULL)
    return NULL;
  /* skip leading white space */
  while (isspace (spec->value[0]))
    spec->value++;
  if (spec->value[0] == '\0')
    {
      spec->value = NULL;
      return NULL;
    }
  token = spec->value;
  /* find trailing space */
  while (spec->value[0] != '\0' && !isspace (spec->value[0]))
    spec->value++;
  /* chop this value out */
  if (spec->value[0] != '\0')
    {
      spec->value[0] = '\0';
      spec->value++;
    }
  return token;
}



/* traverse the path specified by spec starting at current */

static struct hw *
split_find_device (struct hw *current,
		   name_specifier *spec)
{
  /* strip off (and process) any leading ., .., ./ and / */
  while (1)
    {
      if (strncmp (spec->path, "/", strlen ("/")) == 0)
	{
	  /* cd /... */
	  while (current != NULL && hw_parent (current) != NULL)
	    current = hw_parent (current);
	  spec->path += strlen ("/");
	}
      else if (strncmp (spec->path, "./", strlen ("./")) == 0)
	{
	  /* cd ./... */
	  current = current;
	  spec->path += strlen ("./");
	}
      else if (strncmp (spec->path, "../", strlen ("../")) == 0)
	{
	  /* cd ../... */
	  if (current != NULL && hw_parent (current) != NULL)
	    current = hw_parent (current);
	  spec->path += strlen ("../");
	}
      else if (strcmp (spec->path, ".") == 0)
	{
	  /* cd . */
	  current = current;
	  spec->path += strlen (".");
	}
      else if (strcmp (spec->path, "..") == 0)
	{
	  /* cd .. */
	  if (current != NULL && hw_parent (current) != NULL)
	    current = hw_parent (current);
	  spec->path += strlen ("..");
	}
      else
	break;
    }
  
  /* now go through the path proper */
  
  if (current == NULL)
    {
      split_device_name (spec);
      return NULL;
    }
  
  while (split_device_name (spec))
    {
      struct hw *child;
      for (child = hw_child (current);
	   child != NULL; child = hw_sibling (child))
	{
	  if (strcmp (spec->name, hw_name (child)) == 0)
	    {
	      if (spec->unit == NULL)
		break;
	      else
		{
		  hw_unit phys;
		  hw_unit_decode (current, spec->unit, &phys);
		  if (memcmp (&phys, hw_unit_address (child),
			      sizeof (hw_unit)) == 0)
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


static struct hw *
split_fill_path (struct hw *current,
		 const char *device_specifier,
		 name_specifier *spec)
{
  /* break it up */
  if (!split_device_specifier (current, device_specifier, spec))
    hw_abort (current, "error parsing %s\n", device_specifier);
  
  /* fill our tree with its contents */
  current = split_find_device (current, spec);
  
  /* add any additional devices as needed */
  if (spec->name != NULL)
    {
      do
	{
	  if (current != NULL && !hw_finished_p (current))
	    hw_finish (current);
	  current = hw_create (NULL,
			       current,
			       spec->family,
			       spec->name,
			       spec->unit,
			       spec->args);
	}
      while (split_device_name (spec));
    }
  
  return current;
}


/* <non-white-space> */

static const char *
skip_token(const char *chp)
{
  while (!isspace(*chp) && *chp != '\0')
    chp++;
  while (isspace(*chp) && *chp != '\0')
    chp++;
  return chp;
}


/* count the number of entries */

static int
count_entries (struct hw *current,
	       const char *property_name,
	       const char *property_value,
	       int modulo)
{
  const char *chp = property_value;
  int nr_entries = 0;
  while (*chp != '\0')
    {
      nr_entries += 1;
      chp = skip_token (chp);
    }
  if ((nr_entries % modulo) != 0)
    {
      hw_abort (current, "incorrect number of entries for %s property %s, should be multiple of %d",
		property_name, property_value, modulo);
    }
  return nr_entries / modulo;
}



/* parse: <address> ::= <token> ; device dependant */

static const char *
parse_address (struct hw *current,
	       struct hw *bus,
	       const char *chp,
	       hw_unit *address)
{
  if (hw_unit_decode (bus, chp, address) < 0)
    hw_abort (current, "invalid unit address in %s", chp);
  return skip_token (chp);
}


/* parse: <size> ::= <number> { "," <number> } ; */

static const char *
parse_size (struct hw *current,
	    struct hw *bus,
	    const char *chp,
	    hw_unit *size)
{
  int i;
  int nr;
  const char *curr = chp;
  memset(size, 0, sizeof(*size));
  /* parse the numeric list */
  size->nr_cells = hw_unit_nr_size_cells (bus);
  nr = 0;
  while (1)
    {
      char *next;
      size->cells[nr] = strtoul (curr, &next, 0);
      if (curr == next)
	hw_abort (current, "Problem parsing <size> %s", chp);
      nr += 1;
      if (next[0] != ',')
	break;
      if (nr == size->nr_cells)
	hw_abort (current, "Too many values in <size> %s", chp);
      curr = next + 1;
    }
  ASSERT (nr > 0 && nr <= size->nr_cells);
  /* right align the numbers */
  for (i = 1; i <= size->nr_cells; i++)
    {
      if (i <= nr)
	size->cells[size->nr_cells - i] = size->cells[nr - i];
      else
	size->cells[size->nr_cells - i] = 0;
    }
  return skip_token (chp);
}


/* parse: <reg> ::= { <address> <size> } ; */

static void
parse_reg_property (struct hw *current,
		    const char *property_name,
		    const char *property_value)
{
  int nr_regs;
  int reg_nr;
  reg_property_spec *regs;
  const char *chp;
  
  /* determine the number of reg entries by counting tokens */
  nr_regs = count_entries (current, property_name, property_value, 2);
  
  /* create working space */
  regs = zalloc (nr_regs * sizeof (*regs));
  
  /* fill it in */
  chp = property_value;
  for (reg_nr = 0; reg_nr < nr_regs; reg_nr++)
    {
      chp = parse_address (current, hw_parent(current),
			   chp, &regs[reg_nr].address);
      chp = parse_size (current, hw_parent(current),
			chp, &regs[reg_nr].size);
    }
  
  /* create it */
  hw_add_reg_array_property (current, property_name,
			     regs, nr_regs);
  
  zfree (regs);
}


/* { <child-address> <parent-address> <child-size> }* */

static void
parse_ranges_property (struct hw *current,
		       const char *property_name,
		       const char *property_value)
{
  int nr_ranges;
  int range_nr;
  range_property_spec *ranges;
  const char *chp;
  
  /* determine the number of ranges specified */
  nr_ranges = count_entries (current, property_name, property_value, 3);
  
  /* create a property of that size */
  ranges = zalloc (nr_ranges * sizeof(*ranges));
  
  /* fill it in */
  chp = property_value;
  for (range_nr = 0; range_nr < nr_ranges; range_nr++)
    {
      chp = parse_address (current, current,
			   chp, &ranges[range_nr].child_address);
      chp = parse_address (current, hw_parent(current),
			   chp, &ranges[range_nr].parent_address);
      chp = parse_size (current, current,
			chp, &ranges[range_nr].size);
    }
  
  /* create it */
  hw_add_range_array_property (current, property_name, ranges, nr_ranges);
  
  zfree (ranges);
}


/* <integer> ... */

static void
parse_integer_property (struct hw *current,
			const char *property_name,
			const char *property_value)
{
  int nr_entries;
  unsigned_cell words[1024];
  /* integer or integer array? */
  nr_entries = 0;
  while (1)
    {
      char *end;
      words[nr_entries] = strtoul (property_value, &end, 0);
      if (property_value == end)
	break;
      nr_entries += 1;
      if (nr_entries * sizeof (words[0]) >= sizeof (words))
	hw_abort (current, "buffer overflow");
      property_value = end;
    }
  if (nr_entries == 0)
    hw_abort (current, "error parsing integer property %s (%s)",
	      property_name, property_value);
  else if (nr_entries == 1)
    hw_add_integer_property (current, property_name, words[0]);
  else
    {
      int i;
      for (i = 0; i < nr_entries; i++)
	{
	  H2BE (words[i]);
	}
      /* perhaps integer array property is better */
      hw_add_array_property (current, property_name, words,
			     sizeof(words[0]) * nr_entries);
    }
}


/* <string> ... */

static void
parse_string_property (struct hw *current,
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
  for (chp = property_value; *chp; chp++)
    {
      if (*chp == '"')
	approx_nr_strings++;
    }
  approx_nr_strings = (approx_nr_strings) / 2;
  
  /* create a string buffer for that many (plus a null) */
  strings = (char**) zalloc ((approx_nr_strings + 1) * sizeof(char*));
  
  /* now find all the strings */
  chp = property_value;
  nr_strings = 0;
  while (1)
    {
      
      /* skip leading space */
      while (*chp != '\0' && isspace (*chp))
	chp += 1;
      if (*chp == '\0')
	break;
      
      /* copy it in */
      if (*chp == '"')
	{
	  /* a quoted string - watch for '\' et al. */
	  /* estimate the size and allocate space for it */
	  int pos;
	  chp++;
	  pos = 0;
	  while (chp[pos] != '\0' && chp[pos] != '"')
	    {
	      if (chp[pos] == '\\' && chp[pos+1] != '\0')
		pos += 2;
	      else
		pos += 1;
	    }
	  strings[nr_strings] = zalloc (pos + 1);
	  /* copy the string over */
	  pos = 0;
	  while (*chp != '\0' && *chp != '"')
	    {
	      if (*chp == '\\' && *(chp+1) != '\0') {
		strings[nr_strings][pos] = *(chp+1);
		chp += 2;
		pos++;
	      }
	      else
		{
		  strings[nr_strings][pos] = *chp;
		  chp += 1;
		  pos++;
		}
	    }
	  if (*chp != '\0')
	    chp++;
	  strings[nr_strings][pos] = '\0';
	}
      else
	{
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
	hw_abort (current, "String property %s badly formatted",
		  property_name);
    }
  ASSERT (strings[nr_strings] == NULL); /* from zalloc */
  
  /* install it */
  if (nr_strings == 0)
    hw_add_string_property (current, property_name, "");
  else if (nr_strings == 1)
    hw_add_string_property (current, property_name, strings[0]);
  else
    {
      const char **specs = (const char**) strings; /* stop a bogus error */
      hw_add_string_array_property (current, property_name,
				    specs, nr_strings);
    }
  
  /* flush the created string */
  while (nr_strings > 0)
    {
      nr_strings--;
      zfree (strings[nr_strings]);
    }
  zfree(strings);
}


/* <path-to-ihandle-device> */

#if NOT_YET
static void
parse_ihandle_property (struct hw *current,
			const char *property,
			const char *value)
{
  ihandle_runtime_property_spec ihandle;
  
  /* pass the full path */
  ihandle.full_path = value;
  
  /* save this ready for the ihandle create */
  hw_add_ihandle_runtime_property (current, property,
				   &ihandle);
}
#endif


struct hw *
hw_tree_create (SIM_DESC sd,
		const char *family)
{
  return hw_create (sd, NULL, family, family, NULL, NULL);
}

void
hw_tree_delete (struct hw *me)
{
  /* Need to allow devices to disapear under our feet */
  while (hw_child (me) != NULL)
    {
      hw_tree_delete (hw_child (me));
    }
  hw_delete (me);
}


struct hw *
hw_tree_parse (struct hw *current,
	       const char *fmt,
	       ...)
{
    va_list ap;
    va_start (ap, fmt);
    current = hw_tree_vparse (current, fmt, ap);
    va_end (ap);
    return current;
}
  
struct hw *
hw_tree_vparse (struct hw *current,
		const char *fmt,
		va_list ap)
{
  char device_specifier[1024];
  name_specifier spec;
  
  /* format the path */
  vsprintf (device_specifier, fmt, ap);
  if (strlen (device_specifier) >= sizeof (device_specifier))
    hw_abort (NULL, "device_tree_add_deviced: buffer overflow\n");
  
  /* construct the tree down to the final struct hw */
  current = split_fill_path (current, device_specifier, &spec);
  
  /* is there an interrupt spec */
  if (spec.property == NULL
      && spec.value != NULL)
    {
      char *op = split_value (&spec);
      switch (op[0])
	{
	case '>':
	  {
	    char *my_port_name = split_value (&spec);
	    int my_port;
	    char *dest_port_name = split_value (&spec);
	    int dest_port;
	    name_specifier dest_spec;
	    char *dest_hw_name = split_value (&spec);
	    struct hw *dest;
	    /* find my name */
	    if (!hw_finished_p (current))
	      hw_finish (current);
	    my_port = hw_port_decode (current, my_port_name, output_port);
	    /* find the dest device and port */
	    dest = split_fill_path (current, dest_hw_name, &dest_spec);
	    if (!hw_finished_p (dest))
	      hw_finish (dest);
	    dest_port = hw_port_decode (dest, dest_port_name,
					input_port);
	    /* connect the two */
	    hw_port_attach (current,
			    my_port,
			    dest,
			    dest_port,
			    permenant_object);
	    break;
	  }
	default:
	  hw_abort (current, "unreconised interrupt spec %s\n", spec.value);
	  break;
	}
    }
  
  /* is there a property */
  if (spec.property != NULL)
    {
      if (strcmp (spec.value, "true") == 0)
	hw_add_boolean_property (current, spec.property, 1);
      else if (strcmp (spec.value, "false") == 0)
	hw_add_boolean_property (current, spec.property, 0);
      else
	{
	  const struct hw_property *property;
	  switch (spec.value[0])
	    {
#if NOT_YET
	    case '*':
	      {
		parse_ihandle_property (current, spec.property, spec.value + 1);
		break;
	      }
#endif
	    case '[':
	      {
		unsigned8 words[1024];
		char *curr = spec.value + 1;
		int nr_words = 0;
		while (1)
		  {
		    char *next;
		    words[nr_words] = H2BE_1 (strtoul (curr, &next, 0));
		    if (curr == next)
		      break;
		    curr = next;
		    nr_words += 1;
		  }
		hw_add_array_property (current, spec.property,
				       words, sizeof(words[0]) * nr_words);
		break;
	      }
	    case '"':
	      {
		parse_string_property (current, spec.property, spec.value);
		break;
	      }
	    case '!':
	      {
		spec.value++;
		property = hw_tree_find_property (current, spec.value);
		if (property == NULL)
		  hw_abort (current, "property %s not found\n", spec.value);
		hw_add_duplicate_property (current,
					   spec.property,
					   property);
		break;
	      }
	    default:
	      {
		if (strcmp (spec.property, "reg") == 0
		    || strcmp (spec.property, "assigned-addresses") == 0
		    || strcmp (spec.property, "alternate-reg") == 0)
		  {
		    parse_reg_property (current, spec.property, spec.value);
		  }
		else if (strcmp (spec.property, "ranges") == 0)
		  {
		    parse_ranges_property (current, spec.property, spec.value);
		  }
		else if (isdigit(spec.value[0])
			 || (spec.value[0] == '-' && isdigit(spec.value[1]))
			 || (spec.value[0] == '+' && isdigit(spec.value[1])))
		  {
		    parse_integer_property(current, spec.property, spec.value);
		  }
		else
		  parse_string_property(current, spec.property, spec.value);
		break;
	      }
	    }
	}
    }
  return current;
}


static void
finish_hw_tree (struct hw *me,
		void *data)
{
  if (!hw_finished_p (me))
    hw_finish (me);
}

void
hw_tree_finish (struct hw *root)
{
  hw_tree_traverse (root, finish_hw_tree, NULL, NULL);
}



void
hw_tree_traverse (struct hw *root,
		  hw_tree_traverse_function *prefix,
		  hw_tree_traverse_function *postfix,
		  void *data)
{
  struct hw *child;
  if (prefix != NULL)
    prefix (root, data);
  for (child = hw_child (root);
       child != NULL;
       child = hw_sibling (child))
    {
      hw_tree_traverse (child, prefix, postfix, data);
    }
  if (postfix != NULL)
    postfix (root, data);
}



struct printer {
  hw_tree_print_callback *print;
  void *file;
};

static void
print_address (struct hw *bus,
	       const hw_unit *phys,
	       struct printer *p)
{
  char unit[32];
  hw_unit_encode (bus, phys, unit, sizeof(unit));
  p->print (p->file, " %s", unit);
}

static void
print_size (struct hw *bus,
	    const hw_unit *size,
	    struct printer *p)
{
  int i;
  for (i = 0; i < size->nr_cells; i++)
    if (size->cells[i] != 0)
      break;
  if (i < size->nr_cells) {
    p->print (p->file, " 0x%lx", (unsigned long) size->cells[i]);
    i++;
    for (; i < size->nr_cells; i++)
      p->print (p->file, ",0x%lx", (unsigned long) size->cells[i]);
  }
  else
    p->print (p->file, " 0");
}

static void
print_reg_property (struct hw *me,
		    const struct hw_property *property,
		    struct printer *p)
{
  int reg_nr;
  reg_property_spec reg;
  for (reg_nr = 0;
       hw_find_reg_array_property (me, property->name, reg_nr, &reg);
       reg_nr++) {
    print_address (hw_parent (me), &reg.address, p);
    print_size (me, &reg.size, p);
  }
}

static void
print_ranges_property (struct hw *me,
		       const struct hw_property *property,
		       struct printer *p)
{
  int range_nr;
  range_property_spec range;
  for (range_nr = 0;
       hw_find_range_array_property (me, property->name, range_nr, &range);
       range_nr++)
    {
      print_address (me, &range.child_address, p);
      print_address (hw_parent (me), &range.parent_address, p);
      print_size (me, &range.size, p);
    }
}

static void
print_string (struct hw *me,
	      const char *string,
	      struct printer *p)
{
  p->print (p->file, " \"");
  while (*string != '\0') {
    switch (*string) {
    case '"':
      p->print (p->file, "\\\"");
      break;
    case '\\':
      p->print (p->file, "\\\\");
      break;
    default:
      p->print (p->file, "%c", *string);
      break;
    }
    string++;
  }
  p->print (p->file, "\"");
}

static void
print_string_array_property (struct hw *me,
			     const struct hw_property *property,
			     struct printer *p)
{
  int nr;
  string_property_spec string;
  for (nr = 0;
       hw_find_string_array_property (me, property->name, nr, &string);
       nr++)
    {
      print_string (me, string, p);
    }
}

static void
print_properties (struct hw *me,
		  struct printer *p)
{
  const struct hw_property *property;
  for (property = hw_find_property (me, NULL);
       property != NULL;
       property = hw_next_property (property))
    {
      if (hw_parent (me) == NULL)
	p->print (p->file, "/%s", property->name);
      else
	p->print (p->file, "%s/%s", hw_path (me), property->name);
      if (property->original != NULL)
	{
	  p->print (p->file, " !");
	  p->print (p->file, "%s/%s", 
		     hw_path (property->original->owner),
		     property->original->name);
	}
      else
	{
	  switch (property->type)
	    {
	    case array_property:
	      {
		if ((property->sizeof_array % sizeof (signed_cell)) == 0)
		  {
		    unsigned_cell *w = (unsigned_cell*) property->array;
		    int cell_nr;
		    for (cell_nr = 0;
			 cell_nr < (property->sizeof_array / sizeof (unsigned_cell));
			 cell_nr++)
		      {
			p->print (p->file, " 0x%lx", (unsigned long) BE2H_cell (w[cell_nr]));
		      }
		  }
		else
		  {
		    unsigned8 *w = (unsigned8*)property->array;
		    p->print (p->file, " [");
		    while ((char*)w - (char*)property->array < property->sizeof_array) {
		      p->print (p->file, " 0x%2x", BE2H_1 (*w));
		      w++;
		    }
		  }
		break;
	      }
	    case boolean_property:
	      {
		int b = hw_find_boolean_property(me, property->name);
		p->print (p->file, " %s", b ? "true"  : "false");
		break;
	      }
#if NOT_YET
	    case ihandle_property:
	      {
		if (property->array != NULL)
		  {
		    device_instance *instance = hw_find_ihandle_property (me, property->name);
		    p->print (p->file, " *%s", device_instance_path(instance));
		  }
		else
		  {
		    /* not yet initialized, ask the device for the path */
		    ihandle_runtime_property_spec spec;
		    hw_find_ihandle_runtime_property (me, property->name, &spec);
		    p->print (p->file, " *%s", spec.full_path);
		  }
		break;
	      }
#endif
	    case integer_property:
	      {
		unsigned_word w = hw_find_integer_property (me, property->name);
		p->print (p->file, " 0x%lx", (unsigned long)w);
		break;
	      }
	    case range_array_property:
	      {
		print_ranges_property (me, property, p);
		break;
	      }
	    case reg_array_property:
	      {
		print_reg_property (me, property, p);
		break;
	      }
	    case string_property:
	      {
		const char *s = hw_find_string_property (me, property->name);
		print_string (me, s, p);
		break;
	      }
	    case string_array_property:
	      {
		print_string_array_property (me, property, p);
		break;
	      }
	    }
	}
      p->print (p->file, "\n");
    }
}

static void
print_interrupts (struct hw *me,
                  int my_port,
		  struct hw *dest,
		  int dest_port,
		  void *data)
{
  struct printer *p = data;
  char src[32];
  char dst[32];
  hw_port_encode (me, my_port, src, sizeof(src), output_port);
  hw_port_encode (dest, dest_port, dst, sizeof(dst), input_port);
  p->print (p->file,
	    "%s > %s %s %s\n",
	    hw_path (me),
	    src, dst,
	    hw_path (dest));
}

static void
print_device (struct hw *me,
	      void *data)
{
  struct printer *p = data;
  p->print (p->file, "%s\n", hw_path (me));
  print_properties (me, p);
  hw_port_traverse (me, print_interrupts, data);
}

void
hw_tree_print (struct hw *root,
	       hw_tree_print_callback *print,
	       void *file)
{
  struct printer p;
  p.print = print;
  p.file = file;
  hw_tree_traverse (root,
		    print_device, NULL,
		    &p);
}



#if NOT_YET
device_instance *
tree_instance(struct hw *root,
	      const char *device_specifier)
{
  /* find the device node */
  struct hw *me;
  name_specifier spec;
  if (!split_device_specifier(root, device_specifier, &spec))
    return NULL;
  me = split_find_device(root, &spec);
  if (spec.name != NULL)
    return NULL;
  /* create the instance */
  return device_create_instance(me, device_specifier, spec.last_args);
}
#endif

struct hw *
hw_tree_find_device (struct hw *root,
		     const char *path_to_device)
{
  struct hw *node;
  name_specifier spec;
  
  /* parse the path */
  split_device_specifier (root, path_to_device, &spec);
  if (spec.value != NULL)
    return NULL; /* something wierd */
  
  /* now find it */
  node = split_find_device (root, &spec);
  if (spec.name != NULL)
    return NULL; /* not a leaf */
  
  return node;
}


const struct hw_property *
hw_tree_find_property (struct hw *root,
		       const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier (root, path_to_property, &spec))
    hw_abort (root, "Invalid property path %s", path_to_property);
  root = split_find_device (root, &spec);
  if (spec.name != NULL)
    return NULL; /* not a leaf */
  return hw_find_property (root, spec.property);
}

int
hw_tree_find_boolean_property (struct hw *root,
			       const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier (root, path_to_property, &spec))
    hw_abort (root, "Invalid property path %s", path_to_property);
  root = split_find_device (root, &spec);
  if (spec.name != NULL)
    hw_abort (root, "device \"%s\" not found (property \"%s\")",
	      spec.name, path_to_property);
  return hw_find_boolean_property (root, spec.property);
}

signed_cell
hw_tree_find_integer_property (struct hw *root,
			       const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier (root, path_to_property, &spec))
    hw_abort (root, "Invalid property path %s", path_to_property);
  root = split_find_device (root, &spec);
  if (spec.name != NULL)
    hw_abort (root, "device \"%s\" not found (property \"%s\")",
	      spec.name, path_to_property);
  return hw_find_integer_property (root, spec.property);
}

#if NOT_YET
device_instance *
hw_tree_find_ihandle_property (struct hw *root,
			       const char *path_to_property)
{
  struct hw *root;
  name_specifier spec;
  if (!split_property_specifier (root, path_to_property, &spec))
    hw_abort (root, "Invalid property path %s", path_to_property);
  root = split_find_device (root, &spec);
  if (spec.name != NULL)
    hw_abort (root, "device \"%s\" not found (property \"%s\")",
	      spec.name, path_to_property);
  return hw_find_ihandle_property (root, spec.property);
}
#endif

const char *
hw_tree_find_string_property (struct hw *root,
			      const char *path_to_property)
{
  name_specifier spec;
  if (!split_property_specifier (root, path_to_property, &spec))
    hw_abort (root, "Invalid property path %s", path_to_property);
  root = split_find_device (root, &spec);
  if (spec.name != NULL)
    hw_abort (root, "device \"%s\" not found (property \"%s\")",
	      spec.name, path_to_property);
  return hw_find_string_property (root, spec.property);
}
