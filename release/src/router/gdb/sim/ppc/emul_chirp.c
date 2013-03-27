/*  This file is part of the program psim.

    Copyright 1994, 1995, 1996, 1997, 2003 Andrew Cagney

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


#ifndef _EMUL_CHIRP_C_
#define _EMUL_CHIRP_C_

/* Note: this module is called via a table.  There is no benefit in
   making it inline */

#include "emul_generic.h"
#include "emul_chirp.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef STATIC_INLINE_EMUL_CHIRP
#define STATIC_INLINE_EMUL_CHIRP STATIC_INLINE
#endif


/* EMULATION


   OpenFirmware - IEEE Standard for Boot (Initialization
   Configuration) Firmware.


   DESCRIPTION


   BUGS

   
   This code assumes that the memory node has #address-cells and
   #size-cells set to one.  For future implementations, this may not
   be the case.

   */




/* Descriptor of the open boot services being emulated */

typedef int (chirp_handler)
     (os_emul_data *data,
      cpu *processor,
      unsigned_word cia);

typedef struct _chirp_services {
  const char *name;
  chirp_handler *handler;
} chirp_services;


/* The OpenBoot emulation is, at any time either waiting for a client
   request or waiting on a client callback */
typedef enum {
  serving,
  emulating,
  faulting,
} chirp_emul_state;

struct _os_emul_data {
  chirp_emul_state state;
  unsigned_word return_address;
  unsigned_word arguments;
  unsigned_word n_args;
  unsigned_word n_returns;
  chirp_services *service;
  device *root;
  chirp_services *services;
  /* configuration */
  unsigned_word memory_size;
  unsigned_word real_base;
  unsigned_word real_size;
  unsigned_word virt_base;
  unsigned_word virt_size;
  int real_mode;
  int little_endian;
  int floating_point_available;
  int interrupt_prefix;
  unsigned_word load_base;
  /* hash table */
  unsigned_word nr_page_table_entry_groups;
  unsigned_word htab_offset;
  unsigned_word htab_ra;
  unsigned_word htab_va;
  unsigned_word sizeof_htab;
  /* virtual address of htab */
  unsigned_word stack_offset;
  unsigned_word stack_ra;
  unsigned_word stack_va;
  unsigned_word sizeof_stack;
  /* addresses of emulation instructions virtual/real */
  unsigned_word code_offset;
  unsigned_word code_va;
  unsigned_word code_ra;
  unsigned_word sizeof_code;
  unsigned_word code_client_va;
  unsigned_word code_client_ra;
  unsigned_word code_callback_va;
  unsigned_word code_callback_ra;
  unsigned_word code_loop_va;
  unsigned_word code_loop_ra;
};


/* returns the name of the corresponding Ihandle */
static const char *
ihandle_name(device_instance *ihandle)
{
  if (ihandle == NULL)
    return "";
  else
    return device_name(device_instance_device(ihandle));
}



/* Read/write the argument list making certain that all values are
   converted to/from host byte order.

   In the below only n_args+n_returns is read/written */

static int
chirp_read_t2h_args(void *args,
		    int sizeof_args,
		    int n_args,
		    int n_returns,
		    os_emul_data *data,
		    cpu *processor,
		    unsigned_word cia)
{
  unsigned_cell *words;
  int i;
  /* check against the number of arguments specified by the client
     program */
  if ((n_args >= 0 && data->n_args != n_args)
      || (n_returns >= 0 && data->n_returns != n_returns)) {
    TRACE(trace_os_emul, ("%s - invalid nr of args - n_args=%ld, n_returns=%ld\n",
			  data->service->name,
			  (long)data->n_args,
			  (long)data->n_returns));
    return -1;
  }
  /* check that there is enough space */
  if (sizeof(unsigned_cell) * (data->n_args + data->n_returns) > sizeof_args)
    return -1;
  /* bring in the data */
  memset(args, 0, sizeof_args);
  emul_read_buffer(args, data->arguments + 3 * sizeof(unsigned_cell),
		   sizeof(unsigned_cell) * (data->n_args + data->n_returns),
		   processor, cia);
  /* convert all words to host format */
  words = args;
  for (i = 0; i < (sizeof_args / sizeof(unsigned_cell)); i++)
    words[i] = T2H_cell(words[i]);
  return 0;
}

static void
chirp_write_h2t_args(void *args,
		     int sizeof_args,
		     os_emul_data *data,
		     cpu *processor,
		     unsigned_word cia)
{
  int i;
  unsigned_cell *words;
  /* convert to target everything */
  words = args;
  for (i = 0; i < (sizeof_args / sizeof(unsigned_cell)); i++)
    words[i] = H2T_cell(words[i]);
  /* bring in the data */
  emul_write_buffer(args, data->arguments + 3 * sizeof(unsigned_cell),
		    sizeof(unsigned_cell) * (data->n_args + data->n_returns),
		    processor, cia);
}


/* OpenBoot emulation functions */

/* client interface */

static int
chirp_emul_test(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  struct test_args {
    /*in*/
    unsigned_cell name; /*string*/
    /*out*/
    unsigned_cell missing;
  } args;
  char name[32];
  chirp_services *service = NULL;
  /* read in the arguments */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  emul_read_string(name, args.name, sizeof(name),
		   processor, cia);
  TRACE(trace_os_emul, ("test - in - name=`%s'\n", name));
  /* see if we know about the service */
  service = data->services;
  while (service->name != NULL && strcmp(service->name, name) != 0) {
    service++;
  }
  if (service->name == NULL)
    args.missing = -1;
  else
    args.missing = 0;
  /* write the arguments back out */
  TRACE(trace_os_emul, ("test - out - missing=%ld\n",
			(long)args.missing));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}


/* Device tree */

static int
chirp_emul_peer(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  struct peer_args {
    /*in*/
    unsigned_cell phandle;
    /*out*/
    unsigned_cell sibling_phandle;
  } args;
  device *phandle;
  device *sibling_phandle = NULL;
  /* read in the arguments */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  TRACE(trace_os_emul, ("peer - in - phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle))));
  /* find the peer */
  if (args.phandle == 0) {
    sibling_phandle = data->root;
    args.sibling_phandle = device_to_external(sibling_phandle);
  }
  else if (phandle == NULL) {
    sibling_phandle = NULL;
    args.sibling_phandle = -1;
  }
  else {
    sibling_phandle = device_sibling(phandle);
    if (sibling_phandle == NULL)
      args.sibling_phandle = 0;
    else
      args.sibling_phandle = device_to_external(sibling_phandle);
  }
  /* write the arguments back out */
  TRACE(trace_os_emul, ("peer - out - sibling_phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.sibling_phandle,
			(unsigned long)sibling_phandle,
			(sibling_phandle == NULL ? "" : device_name(sibling_phandle))));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_child(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  struct child_args {
    /*in*/
    unsigned_cell phandle;
    /*out*/
    unsigned_cell child_phandle;
  } args;
  device *phandle;
  device *child_phandle;
  /* read the arguments in */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  TRACE(trace_os_emul, ("child - in - phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle))));
  /* find a child */
  if (args.phandle == 0
      || phandle == NULL) {
    child_phandle = NULL;
    args.child_phandle = -1;
  }
  else {
    child_phandle = device_child(phandle);
    if (child_phandle == NULL)
      args.child_phandle = 0;
    else
      args.child_phandle = device_to_external(child_phandle);
  }
  /* write the result out */
  TRACE(trace_os_emul, ("child - out - child_phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.child_phandle,
			(unsigned long)child_phandle,
			(child_phandle == NULL ? "" : device_name(child_phandle))));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_parent(os_emul_data *data,
		  cpu *processor,
		  unsigned_word cia)
{
  struct parent_args {
    /*in*/
    unsigned_cell phandle;
    /*out*/
    unsigned_cell parent_phandle;
  } args;
  device *phandle;
  device *parent_phandle;
  /* read the args in */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  TRACE(trace_os_emul, ("parent - in - phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle))));
  /* find a parent */
  if (args.phandle == 0
      || phandle == NULL) {
    parent_phandle = NULL;
    args.parent_phandle = -1;
  }
  else {
    parent_phandle = device_parent(phandle);
    if (parent_phandle == NULL)
      args.parent_phandle = 0;
    else
      args.parent_phandle = device_to_external(parent_phandle);
  }
  /* return the result */
  TRACE(trace_os_emul, ("parent - out - parent_phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.parent_phandle,
			(unsigned long)parent_phandle,
			(parent_phandle == NULL ? "" : device_name(parent_phandle))));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_instance_to_package(os_emul_data *data,
			       cpu *processor,
			       unsigned_word cia)
{
  struct instance_to_package_args {
    /*in*/
    unsigned_cell ihandle;
    /*out*/
    unsigned_cell phandle;
  } args;
  device_instance *ihandle;
  device *phandle = NULL;
  /* read the args in */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("instance-to-package - in - ihandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle)));
  /* find the corresponding phandle */
  if (ihandle == NULL) {
    phandle = NULL;
    args.phandle = -1;
  }
  else {
    phandle = device_instance_device(ihandle);
    args.phandle = device_to_external(phandle);
  }
  /* return the result */
  TRACE(trace_os_emul, ("instance-to-package - out - phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle))));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_getproplen(os_emul_data *data,
		      cpu *processor,
		      unsigned_word cia)
{
  struct getproplen_args {
    /*in*/
    unsigned_cell phandle;
    unsigned_cell name;
    /*out*/
    unsigned_cell proplen;
  } args;
  char name[32];
  device *phandle;
  /* read the args in */
  if (chirp_read_t2h_args(&args, sizeof(args), 2, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  emul_read_string(name,
		   args.name,
		   sizeof(name),
		   processor, cia);
  TRACE(trace_os_emul, ("getproplen - in - phandle=0x%lx(0x%lx`%s') name=`%s'\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle)),
			name));
  /* find our prop and get its length */
  if (args.phandle == 0
      || phandle == NULL) {
    args.proplen = -1;
  }
  else {
    const device_property *prop = device_find_property(phandle, name);
    if (prop == (device_property*)0) {
      args.proplen = -1;
    }
    else {
      args.proplen = prop->sizeof_array;
    }
  }
  /* return the result */
  TRACE(trace_os_emul, ("getproplen - out - proplen=%ld\n",
			(unsigned long)args.proplen));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_getprop(os_emul_data *data,
		   cpu *processor,
		   unsigned_word cia)
{
  struct getprop_args {
    /*in*/
    unsigned_cell phandle;
    unsigned_cell name;
    unsigned_cell buf;
    unsigned_cell buflen;
    /*out*/
    unsigned_cell size;
  } args;
  char name[32];
  device *phandle;
  /* read in the args, the return is optional */
  if (chirp_read_t2h_args(&args, sizeof(args), 4, -1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  emul_read_string(name,
		   args.name,
		   sizeof(name),
		   processor, cia);
  TRACE(trace_os_emul, ("getprop - in - phandle=0x%lx(0x%lx`%s') name=`%s' buf=0x%lx buflen=%ld\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle)),
			name,
			(unsigned long)args.buf,
			(unsigned long)args.buflen));
  /* get the property */
  if (args.phandle == 0
      || phandle == NULL) {
    args.size = -1;
  }
  else {
    const device_property *prop = device_find_property(phandle, name);
    if (prop == NULL) {
      args.size = -1;
    }
    else {
      int size = args.buflen;
      if (size > prop->sizeof_array)
	size = prop->sizeof_array;
      emul_write_buffer(prop->array, args.buf,
			size,
			processor, cia);
      args.size = size;
      switch (prop->type) {
      case string_property:
	TRACE(trace_os_emul, ("getprop - string `%s'\n",
			      device_find_string_property(phandle, name)));
	break;
      case ihandle_property:
	TRACE(trace_os_emul, ("getprop - ihandle=0x%lx(0x%lx`%s')\n",
			      BE2H_cell(*(unsigned_cell*)prop->array),
			      (unsigned long)device_find_ihandle_property(phandle, name),
			      ihandle_name(device_find_ihandle_property(phandle, name))));
	break;
      default:
	break;
      }
    }
  }
  /* write back the result */
  if (data->n_returns == 0)
    TRACE(trace_os_emul, ("getprop - out - size=%ld (not returned)\n",
			  (unsigned long)args.size));
  else {
    TRACE(trace_os_emul, ("getprop - out - size=%ld\n",
			  (unsigned long)args.size));
    chirp_write_h2t_args(&args,
			 sizeof(args),
			 data,
			 processor, cia);
  }
  return 0;
}

static int
chirp_emul_nextprop(os_emul_data *data,
		    cpu *processor,
		    unsigned_word cia)
{
  struct nextprop_args {
    /*in*/
    unsigned_cell phandle;
    unsigned_cell previous;
    unsigned_cell buf;
    /*out*/
    unsigned_cell flag;
  } args;
  char previous[32];
  device *phandle;
  /* read in the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  if (args.previous != 0)
    emul_read_string(previous,
		     args.previous,
		     sizeof(previous),
		     processor, cia);
  else
    /* If previous is NULL, make it look like the empty string.  The
       next property after the empty string is the first property.  */
    strcpy (previous, "");
  TRACE(trace_os_emul, ("nextprop - in - phandle=0x%lx(0x%lx`%s') previous=`%s' buf=0x%lx\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle)),
			previous,
			(unsigned long)args.buf));
  /* find the next property */
  if (args.phandle == 0
      || phandle == NULL) {
    args.flag = -1;
  }
  else {
    const device_property *prev_prop = device_find_property(phandle, previous);
    if (prev_prop == NULL) {
      if (strcmp (previous, "") == 0)
	args.flag = 0; /* No properties */
      else
	args.flag = -1; /* name invalid */
    }
    else {
      const device_property *next_prop;
      if (strcmp (previous, "") == 0) {
	next_prop = prev_prop;	/* The first property.  */
      }
      else {
	next_prop = device_next_property(prev_prop);
      }
      if (next_prop == NULL) {
	args.flag = 0; /* last property */
      }
      else {
	emul_write_buffer(next_prop->name, args.buf, strlen(next_prop->name),
			  processor, cia);
	TRACE(trace_os_emul, ("nextprop - name=`%s'\n", next_prop->name));
	args.flag = 1; /* worked ok */
      }
    }
  }
  /* write back the result */
  TRACE(trace_os_emul, ("nextprop - out - flag=%ld\n",
			(unsigned long)args.flag));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

#if 0
static int
chirp_emul_setprop(os_emul_data *data,
		   cpu *processor,
		   unsigned_word cia)
{
  error("chirp: setprop method not implemented\n");
  return 0;
}
#endif

static int
chirp_emul_canon(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  struct canon_args {
    /*in*/
    unsigned_cell device_specifier;
    unsigned_cell buf;
    unsigned_cell buflen;
    /*out*/
    unsigned_cell length;
  } args;
  char device_specifier[1024];
  device *phandle;
  const char *path;
  int length;
  /* read in the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  emul_read_string(device_specifier,
		   args.device_specifier,
		   sizeof(device_specifier),
		   processor, cia);
  TRACE(trace_os_emul, ("canon - in - device_specifier=`%s' buf=0x%lx buflen=%lx\n",
			device_specifier,
			(unsigned long)args.buf,
			(unsigned long)args.buflen));
  /* canon the name */
  phandle = tree_find_device(data->root, device_specifier);
  if (phandle == NULL) {
    length = -1;
    path = "";
    args.length = -1;
  }
  else {
    path = device_path(phandle);
    length = strlen(path);
    if (length >= args.buflen)
      length = args.buflen - 1;
    emul_write_buffer(path, args.buf, length,
		      processor, cia);
    args.length = length;
  }
  /* write back the result */
  TRACE(trace_os_emul, ("canon - out - length=%ld buf=`%s'\n",
			(unsigned long)args.length,
			path));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_finddevice(os_emul_data *data,
		      cpu *processor,
		      unsigned_word cia)
{
  struct finddevice_args {
    /*in*/
    unsigned_cell device_specifier;
    /*out*/
    unsigned_cell phandle;
  } args;
  char device_specifier[1024];
  device *phandle;
  /* get the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  emul_read_string(device_specifier,
		   args.device_specifier,
		   sizeof(device_specifier),
		   processor, cia);
  TRACE(trace_os_emul, ("finddevice - in - device_specifier=`%s'\n",
			device_specifier));
  /* find the device */
  phandle = tree_find_device(data->root, device_specifier);
  if (phandle == NULL)
    args.phandle = -1;
  else
    args.phandle = device_to_external(phandle);
  /* return its phandle */
  TRACE(trace_os_emul, ("finddevice - out - phandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle))));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_instance_to_path(os_emul_data *data,
			    cpu *processor,
			    unsigned_word cia)
{
  struct instance_to_path_args {
    /*in*/
    unsigned_cell ihandle;
    unsigned_cell buf;
    unsigned_cell buflen;
    /*out*/
    unsigned_cell length;
  } args;
  device_instance *ihandle;
  const char *path;
  int length;
  /* get the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("instance-to-path - in - ihandle=0x%lx(0x%lx`%s') buf=0x%lx buflen=%ld\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle),
			(unsigned long)args.buf,
			(unsigned long)args.buflen));
  /* get the devices name */
  if (ihandle == NULL) {
    args.length = -1;
    path = "(null)";
  }
  else {
    path = device_instance_path(ihandle);
    length = strlen(path);
    if (length >= args.buflen)
      length = args.buflen - 1;
    emul_write_buffer(path, args.buf, length,
		      processor, cia);
    args.length = length;
  }
  /* return its phandle */
  TRACE(trace_os_emul, ("instance-to-path - out - length=%ld buf=`%s')\n",
			(unsigned long)args.length,
			path));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_package_to_path(os_emul_data *data,
			   cpu *processor,
			   unsigned_word cia)
{
  struct package_to_path_args {
    /*in*/
    unsigned_cell phandle;
    unsigned_cell buf;
    unsigned_cell buflen;
    /*out*/
    unsigned_cell length;
  } args;
  device *phandle;
  const char *path;
  /* get the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  phandle = external_to_device(data->root, args.phandle);
  TRACE(trace_os_emul, ("package-to-path - in - phandle=0x%lx(0x%lx`%s') buf=0x%lx buflen=%ld\n",
			(unsigned long)args.phandle,
			(unsigned long)phandle,
			(phandle == NULL ? "" : device_name(phandle)),
			(unsigned long)args.buf,
			(unsigned long)args.buflen));
  /* get the devices name */
  if (phandle == NULL) {
    args.length = -1;
    path = "(null)";
  }
  else {
    int length;
    path = device_path(phandle);
    length = strlen(path);
    if (length >= args.buflen)
      length = args.buflen - 1;
    emul_write_buffer(path, args.buf, length,
		      processor, cia);
    args.length = length;
  }
  /* return its phandle */
  TRACE(trace_os_emul, ("package-to-path - out - length=%ld buf=`%s')\n",
			(unsigned long)args.length,
			path));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_call_method(os_emul_data *data,
		       cpu *processor,
		       unsigned_word cia)
{
  struct call_method_args {
    /*in*/
    unsigned_cell method;
    unsigned_cell ihandle;
    /*in/out*/
    unsigned_cell stack[13]; /*6in + 6out + catch */
  } args;
  char method[32];
  device_instance *ihandle;
  /* some useful info about our mini stack */
  int n_stack_args;
  int n_stack_returns;
  int stack_catch_result;
  int stack_returns;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args), -1, -1, data, processor, cia))
    return -1;
  emul_read_string(method,
		   args.method,
		   sizeof(method),
		   processor, cia);
  ihandle = external_to_device_instance(data->root, args.ihandle);
  n_stack_args = data->n_args - 2;
  n_stack_returns = data->n_returns - 1;
  stack_catch_result = n_stack_args;
  stack_returns = stack_catch_result + 1;
  TRACE(trace_os_emul, ("call-method - in - n_args=%ld n_returns=%ld method=`%s' ihandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)data->n_args,
			(unsigned long)data->n_returns,
			method,
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle)));
  /* see if we can emulate this method */
  if (ihandle == NULL) {
    /* OpenFirmware doesn't define this error */
    error("chirp: invalid ihandle passed to call-method method");
  }
  else {
    args.stack[stack_catch_result] =
      device_instance_call_method(ihandle,
				  method,
				  n_stack_args,
				  &args.stack[0],
				  n_stack_returns,
				  &args.stack[stack_returns]);
  }
  /* finished */
  TRACE(trace_os_emul, ("call-method - out - catch-result=%ld\n",
			(unsigned long)args.stack[stack_catch_result]));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}


/* Device I/O */

static int
chirp_emul_open(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  struct open_args {
    /*in*/
    unsigned_cell device_specifier;
    /*out*/
    unsigned_cell ihandle;
  } args;
  char device_specifier[1024];
  device_instance *ihandle;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  emul_read_string(device_specifier,
		   args.device_specifier,
		   sizeof(device_specifier),
		   processor, cia);
  TRACE(trace_os_emul, ("open - in - device_specifier=`%s'\n",
			device_specifier));
  /* open the device */
  ihandle = tree_instance(data->root, device_specifier);
  if (ihandle == NULL)
    args.ihandle = -1;
  else
    args.ihandle = device_instance_to_external(ihandle);
  /* return the ihandle result */
  TRACE(trace_os_emul, ("open - out - ihandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle)));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_close(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  struct close_args {
    /*in*/
    unsigned_cell ihandle;
    /*out*/
  } args;
  device_instance *ihandle;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 0, data, processor, cia))
    return -1;
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("close - in - ihandle=0x%lx(0x%lx`%s')\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle)));
  /* close the device */
  if (ihandle == NULL) {
    /* OpenFirmware doesn't define this error */
    error("chirp: invalid ihandle passed to close method");
  }
  else {
    device_instance_delete(ihandle);
  }
  /* return the ihandle result */
  TRACE(trace_os_emul, ("close - out\n"));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_read(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  struct read_args {
    /*in*/
    unsigned_cell ihandle;
    unsigned_cell addr;
    unsigned_cell len;
    /*out*/
    unsigned_cell actual;
  } args;
  char buf[1024];
  device_instance *ihandle;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("read - in - ihandle=0x%lx(0x%lx`%s') addr=0x%lx len=%ld\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle),
			(unsigned long)args.addr,
			(unsigned long)args.len));
  if (ihandle == NULL) {
    /* OpenFirmware doesn't define this error */
    error("chirp: invalid ihandle passed to read method");
  }
  else {
    /* do the reads */
    int actual = 0;
    while (actual < args.len) {
      int remaining = args.len - actual;
      int to_read = (remaining <= sizeof(buf) ? remaining : sizeof(buf));
      int nr_read = device_instance_read(ihandle, buf, to_read);
      if (nr_read < 0) {
	actual = nr_read; /* the error */
	break;
      }
      else if (nr_read == 0) {
	break;
      }
      emul_write_buffer(buf,
			args.addr + actual,
			nr_read,
			processor, cia);
      actual += nr_read;
    }
    if (actual >= 0) {
      args.actual = actual;
      if (actual < sizeof(buf))
	buf[actual] = '\0';
      else
	buf[sizeof(buf) - 1] = '\0';
    }
    else {
      switch (actual) {
      case sim_io_eof:
	args.actual = 0;
	break;
      case sim_io_not_ready:
	ASSERT(sim_io_not_ready == -2);
	args.actual = sim_io_not_ready;
	break;
      default:
	error("Bad error value %ld", (long)actual);
	break;
      }
    }
  }
  /* return the result */
  TRACE(trace_os_emul, ("read - out - actual=%ld `%s'\n",
			(long)args.actual,
			((args.actual > 0 && args.actual < sizeof(buf)) ? buf : "")
			));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_write(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  struct write_args {
    /*in*/
    unsigned_cell ihandle;
    unsigned_cell addr;
    unsigned_cell len;
    /*out*/
    unsigned_cell actual;
  } args;
  char buf[1024];
  device_instance *ihandle;
  int actual;
  /* get the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  actual = args.len;
  if (actual >= sizeof(buf))
    actual = sizeof(buf) - 1;
  emul_read_buffer(buf,
		   args.addr,
		   actual,
		   processor, cia);
  buf[actual] = '\0';
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("write - in - ihandle=0x%lx(0x%lx`%s') `%s' (%ld)\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle),
			buf, (long)actual));
  if (ihandle == NULL) {
    /* OpenFirmware doesn't define this error */
    error("chirp: invalid ihandle passed to write method");
  }
  else {
    /* write it out */
    actual = device_instance_write(ihandle, buf, actual);
    if (actual < 0)
      args.actual = 0;
    else
      args.actual = actual;
  }
  /* return the result */
  TRACE(trace_os_emul, ("write - out - actual=%ld\n",
			(long)args.actual));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_seek(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  struct seek_args {
    /*in*/
    unsigned_cell ihandle;
    unsigned_cell pos_hi;
    unsigned_cell pos_lo;
    /*out*/
    unsigned_cell status;
  } args;
  int status;
  device_instance *ihandle;
  /* get the args */
  if (chirp_read_t2h_args(&args, sizeof(args), 3, 1, data, processor, cia))
    return -1;
  ihandle = external_to_device_instance(data->root, args.ihandle);
  TRACE(trace_os_emul, ("seek - in - ihandle=0x%lx(0x%lx`%s') pos.hi=0x%lx pos.lo=0x%lx\n",
			(unsigned long)args.ihandle,
			(unsigned long)ihandle,
			ihandle_name(ihandle),
			args.pos_hi, args.pos_lo));
  if (ihandle == NULL) {
    /* OpenFirmware doesn't define this error */
    error("chirp: invalid ihandle passed to seek method");
  }
  else {
    /* seek it out */
    status = device_instance_seek(ihandle, args.pos_hi, args.pos_lo);
    args.status = status;
  }
  /* return the result */
  TRACE(trace_os_emul, ("seek - out - status=%ld\n",
			(long)args.status));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}


/* memory */

static int
chirp_emul_claim(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  /* NOTE: the client interface claim routine is *very* different to
     the "claim" method described in IEEE-1275 appendix A.  The latter
     uses real addresses while this uses virtual (effective)
     addresses. */
  struct claim_args {
    /* in */
    unsigned_cell virt;
    unsigned_cell size;
    unsigned_cell align;
    /* out */
    unsigned_cell baseaddr;
  } args;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args),
			  3 /*n_args*/, 1 /*n_returns*/,
			  data, processor, cia))
    return -1;
  TRACE(trace_os_emul, ("claim - in - virt=0x%lx size=%ld align=%d\n",
			(unsigned long)args.virt,
			(long int)args.size,
			(int)args.align));
  /* use the memory device to allocate (real) memory at the requested
     address */
  {
    device_instance *memory = tree_find_ihandle_property(data->root, "/chosen/memory");
    unsigned_cell mem_in[3];
    unsigned_cell mem_out[1];
    mem_in[0] = args.align; /*top-of-stack*/
    mem_in[1] = args.size;
    mem_in[2] = args.virt;
    if (device_instance_call_method(memory, "claim",
				    3, mem_in, 1, mem_out) < 0)
      error("chirp: claim failed to allocate memory virt=0x%lx size=%ld align=%d",
	    (unsigned long)args.virt,
	    (long int)args.size,
	    (int)args.align);
    args.baseaddr = mem_out[0];
  }
  /* if using virtual addresses, create a 1-1 map of this address space */
  if (!data->real_mode) {
    error("chirp: claim method does not support virtual mode");
  }
  /* return the base address */
  TRACE(trace_os_emul, ("claim - out - baseaddr=0x%lx\n",
			(unsigned long)args.baseaddr));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}

static int
chirp_emul_release(os_emul_data *data,
		   cpu *processor,
		   unsigned_word cia)
{
  /* NOTE: the client interface release routine is *very* different to
     the "claim" method described in IEEE-1275 appendix A.  The latter
     uses real addresses while this uses virtual (effective)
     addresses. */
  struct claim_args {
    /* in */
    unsigned_cell virt;
    unsigned_cell size;
    /* out */
  } args;
  /* read the args */
  if (chirp_read_t2h_args(&args, sizeof(args),
			  2 /*n_args*/, 0 /*n_returns*/,
			  data, processor, cia))
    return -1;
  TRACE(trace_os_emul, ("release - in - virt=0x%lx size=%ld\n",
			(unsigned long)args.virt,
			(long int)args.size));
  /* use the memory device to release (real) memory at the requested
     address */
  {
    device_instance *memory = tree_find_ihandle_property(data->root, "/chosen/memory");
    unsigned_cell mem_in[2];
    mem_in[0] = args.size;
    mem_in[1] = args.virt;
    if (device_instance_call_method(memory, "release",
				    2, mem_in, 0, NULL) < 0)
      error("chirp: claim failed to release memory virt=0x%lx size=%ld",
	    (unsigned long)args.virt,
	    (long int)args.size);
  }
  /* if using virtual addresses, remove the 1-1 map of this address space */
  if (!data->real_mode) {
    error("chirp: release method does not support virtual mode");
  }
  /* return the base address */
  TRACE(trace_os_emul, ("release - out\n"));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}


/* Control transfer */

static int
chirp_emul_boot(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  /* unlike OpenFirmware this one can take an argument */
  struct boot_args {
    /*in*/
    unsigned_cell bootspec;
    /*out*/
  } args;
  char bootspec[1024];
  /* read in the arguments */
  if (chirp_read_t2h_args(&args, sizeof(args), -1, 0, data, processor, cia))
    cpu_halt(processor, cia, was_exited, -1);
  if (args.bootspec != 0)
    emul_read_string(bootspec, args.bootspec, sizeof(bootspec),
		     processor, cia);
  else
    strcpy(bootspec, "(null)");
  TRACE(trace_os_emul, ("boot - in bootspec=`%s'\n", bootspec));
  /* just report this and exit */
  printf_filtered("chrp: boot %s called, exiting.\n", bootspec);
  cpu_halt(processor, cia, was_exited, 0);
  return 0;
}

static int
chirp_emul_enter(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  error("chirp: enter method not implemented\n");
  return 0;
}

static int
chirp_emul_exit(os_emul_data *data,
		cpu *processor,
		unsigned_word cia)
{
  /* unlike OpenBoot this one can take an argument */
  struct exit_args {
    /*in*/
    signed_cell status;
    /*out*/
  } args;
  if (chirp_read_t2h_args(&args, sizeof(args), -1, 0, data, processor, cia))
    cpu_halt(processor, cia, was_exited, -1);
  cpu_halt(processor, cia, was_exited, args.status);
  return 0;
}

static int
chirp_emul_chain(os_emul_data *data,
		 cpu *processor,
		 unsigned_word cia)
{
  error("chirp: chain method not implemented\n");
  return 0;
}


/* user interface */

static int
chirp_emul_interpret(os_emul_data *data,
		     cpu *processor,
		     unsigned_word cia)
{
  error("chirp: interpret method not implemented\n");
  return 0;
}

static int
chirp_emul_set_callback(os_emul_data *data,
			cpu *processor,
			unsigned_word cia)
{
  error("chirp: set_callback method not implemented\n");
  return 0;
}

static int
chirp_emul_set_symbol_lookup(os_emul_data *data,
			     cpu *processor,
			     unsigned_word cia)
{
  error("chirp: set_symbol_lookup method not implemented\n");
  return 0;
}


/* Time */

static int
chirp_emul_milliseconds(os_emul_data *data,
			cpu *processor,
			unsigned_word cia)
{
  struct test_args {
    /*in*/
    /*out*/
    unsigned_cell ms;
  } args;
  unsigned64 time;
  /* read in the arguments */
  if (chirp_read_t2h_args(&args, sizeof(args), 1, 1, data, processor, cia))
    return -1;
  /* make up a number */
  time = event_queue_time(psim_event_queue(cpu_system(processor))) / 1000000;
  args.ms = time;
  /* write the arguments back out */
  TRACE(trace_os_emul, ("milliseconds - out - ms=%ld\n",
			(unsigned long)args.ms));
  chirp_write_h2t_args(&args,
		       sizeof(args),
		       data,
		       processor, cia);
  return 0;
}




static chirp_services services[] = {

  /* client interface */
  { "test", chirp_emul_test },

  /* device tree */
  { "peer", chirp_emul_peer },
  { "child", chirp_emul_child },
  { "parent", chirp_emul_parent },
  { "instance-to-package", chirp_emul_instance_to_package },
  { "getproplen", chirp_emul_getproplen },
  { "getprop", chirp_emul_getprop },
  { "nextprop", chirp_emul_nextprop },
  /* { "setprop", chirp_emul_setprop }, */
  { "canon", chirp_emul_canon },
  { "finddevice", chirp_emul_finddevice },
  { "instance-to-path", chirp_emul_instance_to_path },
  { "package-to-path", chirp_emul_package_to_path },
  { "call-method", chirp_emul_call_method },

  /* device I/O */
  { "open", chirp_emul_open },
  { "close", chirp_emul_close },
  { "read", chirp_emul_read },
  { "write", chirp_emul_write },
  { "seek", chirp_emul_seek },
  { "write", chirp_emul_write },

  /* memory */
  { "claim", chirp_emul_claim },
  { "release", chirp_emul_release },

  /* control transfer */
  { "boot", chirp_emul_boot },
  { "enter", chirp_emul_enter },
  { "exit", chirp_emul_exit },
  { "chain", chirp_emul_chain },

  /* user interface */
  { "interpret", chirp_emul_interpret },
  { "set_callback", chirp_emul_set_callback },
  { "set_symbol_lookup", chirp_emul_set_symbol_lookup },

  /* time */
  { "milliseconds", chirp_emul_milliseconds },

  { 0, /* sentinal */ },
};


/* main handlers */

/* Any starting address greater than this is assumed to be an Chirp
   rather than VEA */

#ifndef CHIRP_START_ADDRESS
#define CHIRP_START_ADDRESS 0x80000000
#endif
#ifndef CHIRP_LOAD_BASE
#define CHIRP_LOAD_BASE -1
#endif


typedef struct _chirp_note_desc {
  signed32 real_mode;
  signed32 real_base;
  signed32 real_size;
  signed32 virt_base;
  signed32 virt_size;
  signed32 load_base;
} chirp_note_desc;

typedef enum {
  note_missing,
  note_found,
  note_correct,
} note_found_status;
typedef struct _chirp_note {
  chirp_note_desc desc;
  note_found_status found;
} chirp_note;

typedef struct _chirp_note_head {
  unsigned32 namesz;
  unsigned32 descsz;
  unsigned32 type;
} chirp_note_head;

static void
map_over_chirp_note(bfd *image,
		    asection *sect,
		    PTR obj)
{
  chirp_note *note = (chirp_note*)obj;
  if (strcmp(sect->name, ".note") == 0) {
    chirp_note_head head;
    char name[16];
    /* check the head */
    if (!bfd_get_section_contents(image, sect,
				  &head, 0, sizeof(head)))
      return;
    head.namesz = bfd_get_32(image, (void*)&head.namesz);
    head.descsz = bfd_get_32(image, (void*)&head.descsz);
    head.type = bfd_get_32(image, (void*)&head.type);
    if (head.type != 0x1275)
      return;
    /* check the name field */
    if (head.namesz > sizeof(name)) {
      error("chirp: note name too long (%d > %d)\n", (int)head.namesz, sizeof(name));
    }
    if (!bfd_get_section_contents(image, sect,
				  name, sizeof(head), head.namesz)) {
      error("chirp: note name unreadable\n");
    }
    if (strcmp(name, "PowerPC") != 0) {
      printf_filtered("chirp: note name (%s) not `PowerPC'\n", name);
    }
    /* check the size */
    if (head.descsz == sizeof(note->desc) - sizeof(signed32)) {
      sim_io_printf_filtered("chirp: note descriptor missing load-base\n");
    }
    else if (head.descsz != sizeof(note->desc)) {
      sim_io_printf_filtered("chirp: note descriptor of wrong size\n");
      note->found = note_found;
      return;
    }
    note->found = note_correct;
    /* get the contents */
    if (!bfd_get_section_contents(image, sect,
				  &note->desc, /* page align start */
				  ((sizeof(head) + head.namesz) + 3) & ~3,
				  head.descsz)) {
      error("chirp: note descriptor unreadable\n");
    }
    note->desc.real_mode = bfd_get_32(image, (void*)&note->desc.real_mode);
    note->desc.real_base = bfd_get_32(image, (void*)&note->desc.real_base);
    note->desc.real_size = bfd_get_32(image, (void*)&note->desc.real_size);
    note->desc.virt_base = bfd_get_32(image, (void*)&note->desc.virt_base);
    note->desc.virt_size = bfd_get_32(image, (void*)&note->desc.virt_size);
    if (head.descsz == sizeof(note->desc))
      note->desc.load_base = bfd_get_32(image, (void*)&note->desc.load_base);
    else
      note->desc.load_base = (signed32)-1;
  }
}


static os_emul_data *
emul_chirp_create(device *root,
		  bfd *image,
		  const char *name)
{
  os_emul_data *chirp;
  device *node;
  chirp_note note;
  int i;

  /* Sanity check that this really is the chosen emulation */
  if (name == NULL && image == NULL)
    return NULL;
  if (name != NULL
      && strcmp(name, "ob") != 0
      && strcmp(name, "ieee1274") != 0
      && strcmp(name, "chrp") != 0
      && strcmp(name, "chirp") != 0
      && strcmp(name, "openboot") != 0)
    return NULL;

  /* look for an elf note section, enter its values into the device tree */
  memset(&note, 0, sizeof(note));
  if (image != NULL)
    bfd_map_over_sections(image, map_over_chirp_note, &note);
  if (name == NULL && image != NULL && note.found == note_missing)
    return NULL;

  /* Assume that it is a chirp emulation */

  chirp = ZALLOC(os_emul_data);
  chirp->root = root;
  chirp->services = services;

  /* the root node */
  tree_parse(root, "/name \"gpl,clayton");

  /* default options */
  emul_add_tree_options(root, image, "chirp", "oea",
			0 /*oea-interrupt-prefix*/);

  /* hardware */
  emul_add_tree_hardware(root);

  /* basic information */
  chirp->memory_size
    = tree_find_integer_property(root, "/openprom/options/oea-memory-size");
  chirp->little_endian
    = tree_find_boolean_property(root, "/options/little-endian?");
  chirp->floating_point_available
    = tree_find_boolean_property(root, "/openprom/options/floating-point?");
  chirp->interrupt_prefix =
    tree_find_integer_property(root, "/openprom/options/oea-interrupt-prefix");


  /* Perform an interum layout of the openboot firmware in memory */


  /* a page for firmware calls */
  chirp->sizeof_code = 4096;
  chirp->code_offset = 0x4000; /* possible space for interrupt table */

  /* the stack */
  chirp->sizeof_stack = 32 * 1024;
  chirp->stack_offset = chirp->code_offset + chirp->sizeof_code;

  /* the hash table */
  if (!note.desc.real_mode) {
    chirp->nr_page_table_entry_groups = (chirp->memory_size < 0x800000
					 ? 1024 /* min allowed */
					 : (chirp->memory_size / 4096 / 2));
    chirp->sizeof_htab = chirp->nr_page_table_entry_groups * 64;
  }
  chirp->htab_offset = chirp->stack_offset + chirp->sizeof_stack;

  /* the actual amount of space needed */
  chirp->real_size = chirp->htab_offset + chirp->sizeof_htab;


  /* now go through and see if it fits in what is available */


  /* resolve real-mode? */
  if (note.found == note_correct)
    chirp->real_mode = note.desc.real_mode;
  else if (tree_find_property(root, "/options/real-mode?") != NULL)
    chirp->real_mode = tree_find_boolean_property(root, "/options/real-mode?");
  else
    chirp->real_mode = 0;
  if (tree_find_property(root, "/options/real-mode?") != NULL) {
    if (!chirp->real_mode
	!= !tree_find_boolean_property(root, "/options/real-mode?"))
      error("chirp: /options/real-mode? conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/real-mode? %s",
	       chirp->real_mode ? "true" : "false");

  /* resolve real-base */
  if (note.found == note_correct
      && note.desc.real_base != (signed32)-1)
    chirp->real_base = note.desc.real_base;
  else if (tree_find_property(root, "/options/real-base") != NULL)
    chirp->real_base = tree_find_integer_property(root, "/options/real-base");
  else
    chirp->real_base = chirp->memory_size - chirp->real_size;
  if (tree_find_property(root, "/options/real-base") != NULL) {
    if (chirp->real_base != tree_find_integer_property(root, "/options/real-base"))
      error("chirp: /options/real-base conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/real-base 0x%lx",
	       (unsigned long)chirp->real_base);

  /* resolve real-size */
  if (note.found == note_correct
      && note.desc.real_size != (signed32)-1
      && note.desc.real_size != 0
      && chirp->real_size > note.desc.real_size)
    error("chirp: insufficient physical memory for firmware\n");
  if (tree_find_property(root, "/options/real-size") != NULL) {
    if (chirp->real_size > tree_find_integer_property(root, "/options/real-size"))
      error("chirp: /options/real-size conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/real-size 0x%lx",
	       (unsigned long)chirp->real_size);

  /* resolve virt-base */
  if (chirp->real_mode)
    chirp->virt_base = chirp->real_base;
  else if (note.found == note_correct && note.desc.virt_base != -1)
    chirp->virt_base = note.desc.virt_base;
  else if (tree_find_property(root, "/options/virt-base") != NULL)
    chirp->virt_base = tree_find_integer_property(root, "/options/virt-base");
  else
    chirp->virt_base = CHIRP_START_ADDRESS;
  if (tree_find_property(root, "/options/virt-base") != NULL) {
    unsigned_word virt_base = tree_find_integer_property(root, "/options/virt-base");
    if (virt_base != -1 && chirp->virt_base != virt_base)
      error("chirp: /options/virt-base conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/virt-base 0x%lx",
	       chirp->real_mode ? -1 : (unsigned long)chirp->virt_base);

  /* resolve virt-size */
  chirp->virt_size = chirp->real_size;
  if (note.found == note_correct
     && note.desc.virt_size != (signed32)-1
      && note.desc.virt_size != 0
      && !chirp->real_mode
      && chirp->virt_size > note.desc.virt_size)
    error("chirp: insufficent virtual memory for firmware\n");
  if (tree_find_property(root, "/options/virt-size") != NULL) {
    if (chirp->virt_size > tree_find_integer_property(root, "/options/virt-size"))
      error("chirp: /options/virt-size conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/virt-size 0x%lx",
	       chirp->real_mode ? -1 : (unsigned long)chirp->virt_size);

  /* resolve load-base */
  if (note.found == note_correct
      && note.desc.load_base != (signed32)-1)
    chirp->load_base = note.desc.load_base;
  else if (tree_find_property(root, "/options/load-base") != NULL)
    chirp->load_base = tree_find_integer_property(root, "/options/load-base");
  else
    chirp->load_base = CHIRP_LOAD_BASE;
  if (tree_find_property(root, "/options/load-base") != NULL) {
    if (chirp->load_base != tree_find_integer_property(root, "/options/load-base"))
      error("chirp: /options/load-base conflicts with note section\n");
  }
  else
    tree_parse(root, "/options/load-base 0x%lx",
	       (unsigned long)chirp->load_base);

  /* now adjust the preliminary firmware addresses to final values */
  chirp->code_ra = chirp->code_offset + chirp->real_base;
  chirp->stack_ra = chirp->stack_offset + chirp->real_base;
  chirp->htab_ra = chirp->htab_offset + chirp->real_base;

  /* the virtual addresses.  In real mode these are real addresses. */

  chirp->code_va = chirp->code_offset + chirp->virt_base;
  chirp->stack_va = chirp->stack_offset + chirp->virt_base;
  chirp->htab_va = chirp->htab_offset + chirp->virt_base;

  chirp->code_client_va = chirp->code_va;
  chirp->code_client_ra = chirp->code_ra;

  chirp->code_callback_va = chirp->code_client_va + 16;
  chirp->code_callback_ra = chirp->code_client_ra + 16;

  chirp->code_loop_va = chirp->code_callback_va + 16;
  chirp->code_loop_ra = chirp->code_callback_ra + 16;

  /* initialization */

  tree_parse(root, "/openprom/init");
  tree_parse(root, "/openprom/init/register");
  tree_parse(root, "/openprom/init/register/0.pc 0x%lx",
	     (unsigned long)bfd_get_start_address(image));
  tree_parse(root, "/openprom/init/register/pc 0x%lx",
	     (unsigned long)chirp->code_loop_va);
  tree_parse(root, "/openprom/init/register/msr 0x%x",
	     (msr_machine_check_enable
	      | (chirp->real_mode
		 ? 0
		 : (msr_instruction_relocate
		    | msr_data_relocate))
	      | (chirp->little_endian
		 ? (msr_little_endian_mode
		    | msr_interrupt_little_endian_mode)
		 : 0)
	      | (chirp->floating_point_available
		 ? msr_floating_point_available
		 : 0)
	      | (chirp->interrupt_prefix
		 ? msr_interrupt_prefix
		 : 0)
	      ));
  tree_parse(root, "/openprom/init/register/sdr1 0x%lx",
	     (unsigned long)(chirp->htab_ra
			     | MASK32(16, 22)
			     | ((chirp->sizeof_htab - 1) >> 16)));
  /* make certain that the segment registers map straight through */
  for (i = 0; i < 16; i++) {
    tree_parse(root, "/openprom/init/register/sr%d 0x%lx",
	       i, (unsigned long)i);
  }

  /* establish an initial state for all processors */


  /* the client interface address */
  tree_parse(root, "/openprom/init/register/r5 0x%lx",
	     (unsigned long)chirp->code_client_va);
  /* a stack */
  tree_parse(root, "/openprom/init/register/sp 0x%lx",
	     (unsigned long)(chirp->stack_va + chirp->sizeof_stack - 16));
  /* in chrp mode any arguments end up being concatinated */
  tree_parse(root, "/openprom/init/stack/stack-type chirp");


  /* client interface - emul-call followed by return instruction */


  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)chirp->code_client_ra);
  tree_parse(node, "./psim,description \"client-interface instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)chirp->code_client_ra);
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_call_instruction);

  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)(chirp->code_client_ra + 4));
  tree_parse(node, "./psim,description \"client-interface return instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)(chirp->code_client_ra + 4));
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_blr_instruction);


  /* return address for client callbacks - an emul-call instruction
     that is again followed by a return instruction */


  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)chirp->code_callback_ra);
  tree_parse(node, "./psim,description \"client-callback instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)chirp->code_callback_ra);
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_call_instruction);

  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)(chirp->code_callback_ra + 4));
  tree_parse(node, "./psim,description \"client-callback return instruction");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)(chirp->code_callback_ra + 4));
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_blr_instruction);

  /* loop to keep other processors busy */

  node = tree_parse(root, "/openprom/init/data@0x%lx",
		    (unsigned long)chirp->code_loop_ra);
  tree_parse(node, "./psim,description \"processor busy loop");
  tree_parse(node, "./real-address 0x%lx",
	     (unsigned long)chirp->code_loop_ra);
  tree_parse(node, "./data 0x%lx",
	     (unsigned long)emul_loop_instruction);

  /* hash table */

  /* create a hash table */

  if (!chirp->real_mode) {
    node = tree_parse(root, "/openprom/init/htab@0x%lx",
		      (unsigned long)chirp->htab_ra);
    tree_parse(node, "./claim 0");
    tree_parse(node, "./real-address 0x%lx",
	       (unsigned long)chirp->htab_ra);
    tree_parse(node, "./nr-bytes 0x%lx",
	       (unsigned long)chirp->sizeof_htab);
  }

  /* map in the stack */

  if (!chirp->real_mode) {
    node = tree_parse(root, "/openprom/init/htab/pte@0x%lx",
		      (unsigned long)chirp->stack_ra);
    tree_parse(node, "./psim,description \"map in the stack");
    tree_parse(node, "./claim 1");
    tree_parse(node, "./virtual-address 0x%lx",
	       (unsigned long)chirp->stack_va);
    tree_parse(node, "./real-address 0x%lx",
	       (unsigned long)chirp->stack_ra);
    tree_parse(node, "./nr-bytes 0x%lx",
	       (unsigned long)chirp->sizeof_stack);
    tree_parse(node, "./wimg %d", 0x7);
    tree_parse(node, "./pp %d", 0x2);
  }

  /* map in the chrp openboot callback code */

  if (!chirp->real_mode) {
    node = tree_parse(root, "/openprom/init/htab/pte@0x%lx",
		      (unsigned long)chirp->code_ra);
    tree_parse(node, "./psim,description \"map in chrp openboot callback code");
    tree_parse(node, "./claim 1");
    tree_parse(node, "./virtual-address 0x%lx",
	       (unsigned long)chirp->code_va);
    tree_parse(node, "./real-address 0x%lx",
	       (unsigned long)chirp->code_ra);
    tree_parse(node, "./nr-bytes 0x%lx",
	       (unsigned long)chirp->sizeof_code);
    tree_parse(node, "./wimg %d", 0x7);
    tree_parse(node, "./pp %d", 0x2);
  }

  /* map in the program to run */

  if (chirp->real_mode) {
    node = tree_parse(node, "/openprom/init/load-binary");
    tree_parse(node, "./psim,description \"load the binary");
    tree_parse(node, "./file-name %s", bfd_get_filename(image));
    tree_parse(node, "./claim 1");
  }
  else {
    node = tree_parse(root, "/openprom/init/htab/pte@0x%lx",
		      (unsigned long)chirp->load_base);
    tree_parse(node, "./psim,description \"load & map the binary");
    tree_parse(node, "./claim 1");
    tree_parse(node, "./file-name \"%s", bfd_get_filename(image));
    tree_parse(node, "./wimg %d", 0x7);
    tree_parse(node, "./pp %d", 0x2);
  }

  /* map in the interrupt vectors */

  if (!chirp->real_mode) {
    node = tree_parse(root, "/openprom/init/htab/pte@0x0");
    tree_parse(node, "./psim,description \"map in interrupt vectors");
    tree_parse(node, "./virtual-address 0x0");
    tree_parse(node, "./real-address 0x0");
    tree_parse(node, "./nr-bytes 0x3000");
    tree_parse(node, "./wimg %d", 0x7);
    tree_parse(node, "./pp %d", 0x2);
  }

  return chirp;
}

static void
emul_chirp_init(os_emul_data *emul_data,
		int nr_cpus)
{
  emul_data->state = serving;
}

static int
emul_chirp_instruction_call(cpu *processor,
			    unsigned_word cia,
			    unsigned_word ra,
			    os_emul_data *emul_data)
{
  unsigned_word service_name_addr;
  unsigned_word result;
  char service_buf[32];
  char *service_name;
  chirp_services *service;

  switch (emul_data->state) {

  case serving:
    /* we are waiting on an OpenBoot request from the client program
       via the client interface */
    if (cia != emul_data->code_client_va)
      return 0;
    emul_data->return_address = LR;
    emul_data->arguments = cpu_registers(processor)->gpr[3];
    /* try to determine what to do */
    service_name_addr = emul_read_word(cpu_registers(processor)->gpr[3],
				       processor, cia);
    service_name = emul_read_string(service_buf, service_name_addr,
				    sizeof(service_buf), processor, cia);
    emul_data->n_args = emul_read_word(emul_data->arguments + sizeof(unsigned_cell),
				       processor, cia);
    emul_data->n_returns = emul_read_word(emul_data->arguments + 2 * sizeof(unsigned_cell),
					  processor, cia);
    /* verify what was passed */
    if (service_name_addr == 0
	|| service_name == NULL) {
      error("OpenFirmware called with invalid (NULL) service name from 0x%lx with args 0x%lx\n",
	    (unsigned long)emul_data->return_address,
	    (unsigned long)emul_data->arguments);
    }
    if (emul_data->n_args > 6) { /* See iee1275 requirements on nr returns */
      error("OpenFirmware service %s called from 0x%lx with args 0x%lx, too many args (%d)\n",
	    (unsigned long)emul_data->return_address,
	    (unsigned long)emul_data->arguments,
	    emul_data->n_returns);
    }
    if (emul_data->n_returns > 6) {
      error("OpenFirmware service %s called from 0x%lx with args 0x%lx,  with too many returns (%d)\n",
	    (unsigned long)emul_data->return_address,
	    (unsigned long)emul_data->arguments,
	    emul_data->n_args);
    }
    /* look it up */
    TRACE(trace_os_emul, ("%s called from 0x%lx with args 0x%lx\n",
			  service_name,
			  (unsigned long)emul_data->return_address,
			  (unsigned long)emul_data->arguments));
    service = services;
    while (service->name != NULL && strcmp(service->name, service_name) != 0)
      service++;
    /* found or not? */
    if (service->name == NULL) {
      error("OpenBoot service `%s' not found\n", service_name);
      TRACE(trace_os_emul, ("%s not found\n", service_name));
      cpu_registers(processor)->gpr[3] = -1;
    }
    else {
      emul_data->service = service;
      /* call upon it */
      result = service->handler(emul_data, processor, cia);
      if (result != 0)
	TRACE(trace_os_emul, ("%s aborted with %ld\n", service_name, (long)result));
      cpu_registers(processor)->gpr[3] = result;
    }
    break;

  default:
    error("emul_chirp_instruction_call() unknown internal state\n");
    result = -1;
    break;

  }

  /* return to caller - instruction following this is a function return */
  return 1;
}

const os_emul emul_chirp = {
  "chirp",
  emul_chirp_create,
  emul_chirp_init,
  NULL, /*system_call*/
  emul_chirp_instruction_call,
  0 /*data*/
};

#endif
