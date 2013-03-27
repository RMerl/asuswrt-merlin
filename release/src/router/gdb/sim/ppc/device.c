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


#ifndef _DEVICE_C_
#define _DEVICE_C_

#include <stdio.h>

#include "device_table.h"
#include "cap.h"

#include "events.h"
#include "psim.h"

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

STATIC_INLINE_DEVICE (void) clean_device_properties(device *);

/* property entries */

typedef struct _device_property_entry device_property_entry;
struct _device_property_entry {
  device_property_entry *next;
  device_property *value;
  const void *init_array;
  unsigned sizeof_init_array;
};


/* Interrupt edges */

typedef struct _device_interrupt_edge device_interrupt_edge;
struct _device_interrupt_edge {
  int my_port;
  device *dest;
  int dest_port;
  device_interrupt_edge *next;
  object_disposition disposition;
};

STATIC_INLINE_DEVICE\
(void)
attach_device_interrupt_edge(device_interrupt_edge **list,
			     int my_port,
			     device *dest,
			     int dest_port,
			     object_disposition disposition)
{
  device_interrupt_edge *new_edge = ZALLOC(device_interrupt_edge);
  new_edge->my_port = my_port;
  new_edge->dest = dest;
  new_edge->dest_port = dest_port;
  new_edge->next = *list;
  new_edge->disposition = disposition;
  *list = new_edge;
}

STATIC_INLINE_DEVICE\
(void)
detach_device_interrupt_edge(device *me,
			     device_interrupt_edge **list,
			     int my_port,
			     device *dest,
			     int dest_port)
{
  while (*list != NULL) {
    device_interrupt_edge *old_edge = *list;
    if (old_edge->dest == dest
	&& old_edge->dest_port == dest_port
	&& old_edge->my_port == my_port) {
      if (old_edge->disposition == permenant_object)
	device_error(me, "attempt to delete permenant interrupt");
      *list = old_edge->next;
      zfree(old_edge);
      return;
    }
  }
  device_error(me, "attempt to delete unattached interrupt");
}

STATIC_INLINE_DEVICE\
(void)
clean_device_interrupt_edges(device_interrupt_edge **list)
{
  while (*list != NULL) {
    device_interrupt_edge *old_edge = *list;
    switch (old_edge->disposition) {
    case permenant_object:
      list = &old_edge->next;
      break;
    case tempoary_object:
      *list = old_edge->next;
      zfree(old_edge);
      break;
    }
  }
}


/* A device */

struct _device {

  /* my name is ... */
  const char *name;
  device_unit unit_address;
  const char *path;
  int nr_address_cells;
  int nr_size_cells;

  /* device tree */
  device *parent;
  device *children;
  device *sibling;

  /* its template methods */
  void *data; /* device specific data */
  const device_callbacks *callback;

  /* device properties */
  device_property_entry *properties;

  /* interrupts */
  device_interrupt_edge *interrupt_destinations;

  /* any open instances of this device */
  device_instance *instances;

  /* the internal/external mappings and other global requirements */
  cap *ihandles;
  cap *phandles;
  psim *system;

  /* debugging */
  int trace;
};


/* an instance of a device */
struct _device_instance {
  void *data;
  char *args;
  char *path;
  const device_instance_callbacks *callback;
  /* the root instance */
  device *owner;
  device_instance *next;
  /* interposed instance */
  device_instance *parent;
  device_instance *child;
};



/* creation */

STATIC_INLINE_DEVICE\
(const char *)
device_full_name(device *leaf,
                 char *buf,
                 unsigned sizeof_buf)
{
  /* get a buffer */
  char full_name[1024];
  if (buf == (char*)0) {
    buf = full_name;
    sizeof_buf = sizeof(full_name);
  }

  /* construct a name */
  if (leaf->parent == NULL) {
    if (sizeof_buf < 1)
      error("device_full_name: buffer overflow");
    *buf = '\0';
  }
  else {
    char unit[1024];
    device_full_name(leaf->parent, buf, sizeof_buf);
    if (leaf->parent != NULL
        && device_encode_unit(leaf->parent,
                              &leaf->unit_address,
                              unit+1,
                              sizeof(unit)-1) > 0)
      unit[0] = '@';
    else
      unit[0] = '\0';
    if (strlen(buf) + strlen("/") + strlen(leaf->name) + strlen(unit)
        >= sizeof_buf)
      error("device_full_name: buffer overflow");
    strcat(buf, "/");
    strcat(buf, leaf->name);
    strcat (buf, unit);
  }
  
  /* return it usefully */
  if (buf == full_name)
    buf = (char *) strdup(full_name);
  return buf;
}

STATIC_INLINE_DEVICE\
(device *)
device_create_from(const char *name,
		   const device_unit *unit_address,
		   void *data,
		   const device_callbacks *callbacks,
		   device *parent)
{
  device *new_device = ZALLOC(device);

  /* insert it into the device tree */
  new_device->parent = parent;
  new_device->children = NULL;
  if (parent != NULL) {
    device **sibling = &parent->children;
    while ((*sibling) != NULL)
      sibling = &(*sibling)->sibling;
    *sibling = new_device;
  }

  /* give it a name */
  new_device->name = (char *) strdup(name);
  new_device->unit_address = *unit_address;
  new_device->path = device_full_name(new_device, NULL, 0);

  /* its template */
  new_device->data = data;
  new_device->callback = callbacks;

  /* its properties - already null */
  /* interrupts - already null */

  /* mappings - if needed */
  if (parent == NULL) {
    new_device->ihandles = cap_create(name);
    new_device->phandles = cap_create(name);
  }
  else {
    new_device->ihandles = device_root(parent)->ihandles;
    new_device->phandles = device_root(parent)->phandles;
  }

  cap_add(new_device->phandles, new_device);
  return new_device;
}



INLINE_DEVICE\
(device *)
device_create(device *parent,
	      const char *base,
	      const char *name,
	      const char *unit_address,
	      const char *args)
{
  const device_descriptor *const *table;
  for (table = device_table; *table != NULL; table++) {
    const device_descriptor *descr;
    for (descr = *table; descr->name != NULL; descr++) {
      if (strcmp(base, descr->name) == 0) {
	device_unit address = { 0 };
	void *data = NULL;
	if (parent != NULL)
	  if (device_decode_unit(parent, unit_address, &address) < 0)
	    device_error(parent, "invalid address %s for device %s",
			 unit_address, name);
	if (descr->creator != NULL)
	  data = descr->creator(name, &address, args);
	return device_create_from(name, &address, data,
				  descr->callbacks, parent);
      }
    }
  }
  device_error(parent, "attempt to attach unknown device %s", name);
  return NULL;
}



INLINE_DEVICE\
(void)
device_usage(int verbose)
{
  const device_descriptor *const *table;
  if (verbose == 1) {
    int pos = 0;
    for (table = device_table; *table != NULL; table++) {
      const device_descriptor *descr;
      for (descr = *table; descr->name != NULL; descr++) {
	pos += strlen(descr->name) + 2;
	if (pos > 75) {
	  pos = strlen(descr->name) + 2;
	  printf_filtered("\n");
	}
	printf_filtered("  %s", descr->name);
      }
      printf_filtered("\n");
    }
  }
  if (verbose > 1) {
    for (table = device_table; *table != NULL; table++) {
      const device_descriptor *descr;
      for (descr = *table; descr->name != NULL; descr++) {
	printf_filtered("  %s:\n", descr->name);
	/* interrupt ports */
	if (descr->callbacks->interrupt.ports != NULL) {
	  const device_interrupt_port_descriptor *ports =
	    descr->callbacks->interrupt.ports;
	  printf_filtered("    interrupt ports:");
	  while (ports->name != NULL) {
	    printf_filtered(" %s", ports->name);
	    ports++;
	  }
	  printf_filtered("\n");
	}
	/* general info */
	if (descr->callbacks->usage != NULL)
	  descr->callbacks->usage(verbose);
      }
    }
  }
}
 




/* Device node: */

INLINE_DEVICE\
(device *)
device_parent(device *me)
{
  return me->parent;
}

INLINE_DEVICE\
(device *)
device_root(device *me)
{
  ASSERT(me != NULL);
  while (me->parent != NULL)
    me = me->parent;
  return me;
}

INLINE_DEVICE\
(device *)
device_sibling(device *me)
{
  return me->sibling;
}

INLINE_DEVICE\
(device *)
device_child(device *me)
{
  return me->children;
}

INLINE_DEVICE\
(const char *)
device_name(device *me)
{
  return me->name;
}

INLINE_DEVICE\
(const char *)
device_path(device *me)
{
  return me->path;
}

INLINE_DEVICE\
(void *)
device_data(device *me)
{
  return me->data;
}

INLINE_DEVICE\
(psim *)
device_system(device *me)
{
  return me->system;
}

INLINE_DEVICE\
(const device_unit *)
device_unit_address(device *me)
{
  return &me->unit_address;
}


INLINE_DEVICE\
(int)
device_address_to_attach_address(device *me,
				 const device_unit *address,
				 int *attach_space,
				 unsigned_word *attach_address,
				 device *client)
{
  if (me->callback->convert.address_to_attach_address == NULL)
    device_error(me, "no convert.address_to_attach_address method");
  return me->callback->convert.address_to_attach_address(me, address, attach_space, attach_address, client);
}


INLINE_DEVICE\
(int)
device_size_to_attach_size(device *me,
			   const device_unit *size,
			   unsigned *nr_bytes,
			   device *client)
{
  if (me->callback->convert.size_to_attach_size == NULL)
    device_error(me, "no convert.size_to_attach_size method");
  return me->callback->convert.size_to_attach_size(me, size, nr_bytes, client);
}


INLINE_DEVICE\
(int)
device_decode_unit(device *bus,
		   const char *unit,
		   device_unit *address)
{
  if (bus->callback->convert.decode_unit == NULL)
    device_error(bus, "no convert.decode_unit method");
  return bus->callback->convert.decode_unit(bus, unit, address);
}


INLINE_DEVICE\
(int)
device_encode_unit(device *bus,
		   const device_unit *unit_address,
		   char *buf,
		   int sizeof_buf)
{
  if (bus->callback->convert.encode_unit == NULL)
    device_error(bus, "no convert.encode_unit method");
  return bus->callback->convert.encode_unit(bus, unit_address, buf, sizeof_buf);
}

INLINE_DEVICE\
(unsigned)
device_nr_address_cells(device *me)
{
  if (me->nr_address_cells == 0) {
    if (device_find_property(me, "#address-cells") != NULL)
      me->nr_address_cells = device_find_integer_property(me, "#address-cells");
    else
      me->nr_address_cells = 2;
  }
  return me->nr_address_cells;
}

INLINE_DEVICE\
(unsigned)
device_nr_size_cells(device *me)
{
  if (me->nr_size_cells == 0) {
    if (device_find_property(me, "#size-cells") != NULL)
      me->nr_size_cells = device_find_integer_property(me, "#size-cells");
    else
      me->nr_size_cells = 1;
  }
  return me->nr_size_cells;
}



/* device-instance: */

INLINE_DEVICE\
(device_instance *)
device_create_instance_from(device *me,
			    device_instance *parent,
			    void *data,
			    const char *path,
			    const char *args,
			    const device_instance_callbacks *callbacks)
{
  device_instance *instance = ZALLOC(device_instance);
  if ((me == NULL) == (parent == NULL))
    device_error(me, "can't have both parent instance and parent device");
  /*instance->unit*/
  /* link this instance into the devices list */
  if (me != NULL) {
    ASSERT(parent == NULL);
    instance->owner = me;
    instance->parent = NULL;
    /* link this instance into the front of the devices instance list */
    instance->next = me->instances;
    me->instances = instance;
  }
  if (parent != NULL) {
    device_instance **previous;
    ASSERT(parent->child == NULL);
    parent->child = instance;
    ASSERT(me == NULL);
    instance->owner = parent->owner;
    instance->parent = parent;
    /* in the devices instance list replace the parent instance with
       this one */
    instance->next = parent->next;
    /* replace parent with this new node */
    previous = &instance->owner->instances;
    while (*previous != parent) {
      ASSERT(*previous != NULL);
      previous = &(*previous)->next;
    }
    *previous = instance;
  }
  instance->data = data;
  instance->args = (args == NULL ? NULL : (char *) strdup(args));
  instance->path = (path == NULL ? NULL : (char *) strdup(path));
  instance->callback = callbacks;
  cap_add(instance->owner->ihandles, instance);
  return instance;
}


INLINE_DEVICE\
(device_instance *)
device_create_instance(device *me,
		       const char *path,
		       const char *args)
{
  /* create the instance */
  if (me->callback->instance_create == NULL)
    device_error(me, "no instance_create method");
  return me->callback->instance_create(me, path, args);
}


STATIC_INLINE_DEVICE\
(void)
clean_device_instances(device *me)
{
  device_instance **instance = &me->instances;
  while (*instance != NULL) {
    device_instance *old_instance = *instance;
    device_instance_delete(old_instance);
    instance = &me->instances;
  }
}


INLINE_DEVICE\
(void)
device_instance_delete(device_instance *instance)
{
  device *me = instance->owner;
  if (instance->callback->delete == NULL)
    device_error(me, "no delete method");
  instance->callback->delete(instance);
  if (instance->args != NULL)
    zfree(instance->args);
  if (instance->path != NULL)
    zfree(instance->path);
  if (instance->child == NULL) {
    /* only remove leaf nodes */
    device_instance **curr = &me->instances;
    while (*curr != instance) {
      ASSERT(*curr != NULL);
      curr = &(*curr)->next;
    }
    *curr = instance->next;
  }
  else {
    /* check it isn't in the instance list */
    device_instance *curr = me->instances;
    while (curr != NULL) {
      ASSERT(curr != instance);
      curr = curr->next;
    }
    /* unlink the child */
    ASSERT(instance->child->parent == instance);
    instance->child->parent = NULL;
  }
  cap_remove(me->ihandles, instance);
  zfree(instance);
}

INLINE_DEVICE\
(int)
device_instance_read(device_instance *instance,
		     void *addr,
		     unsigned_word len)
{
  device *me = instance->owner;
  if (instance->callback->read == NULL)
    device_error(me, "no read method");
  return instance->callback->read(instance, addr, len);
}

INLINE_DEVICE\
(int)
device_instance_write(device_instance *instance,
		      const void *addr,
		      unsigned_word len)
{
  device *me = instance->owner;
  if (instance->callback->write == NULL)
    device_error(me, "no write method");
  return instance->callback->write(instance, addr, len);
}

INLINE_DEVICE\
(int)
device_instance_seek(device_instance *instance,
		     unsigned_word pos_hi,
		     unsigned_word pos_lo)
{
  device *me = instance->owner;
  if (instance->callback->seek == NULL)
    device_error(me, "no seek method");
  return instance->callback->seek(instance, pos_hi, pos_lo);
}

INLINE_DEVICE\
(int)
device_instance_call_method(device_instance *instance,
			    const char *method_name,
			    int n_stack_args,
			    unsigned_cell stack_args[/*n_stack_args*/],	
			    int n_stack_returns,
			    unsigned_cell stack_returns[/*n_stack_args*/])
{
  device *me = instance->owner;
  const device_instance_methods *method = instance->callback->methods;
  if (method == NULL) {
    device_error(me, "no methods (want %s)", method_name);
  }
  while (method->name != NULL) {
    if (strcmp(method->name, method_name) == 0) {
      return method->method(instance,
			    n_stack_args, stack_args,
			    n_stack_returns, stack_returns);
    }
    method++;
  }
  device_error(me, "no %s method", method_name);
  return 0;
}


INLINE_DEVICE\
(device *)
device_instance_device(device_instance *instance)
{
  return instance->owner;
}

INLINE_DEVICE\
(const char *)
device_instance_path(device_instance *instance)
{
  return instance->path;
}

INLINE_DEVICE\
(void *)
device_instance_data(device_instance *instance)
{
  return instance->data;
}



/* Device Properties: */

STATIC_INLINE_DEVICE\
(device_property_entry *)
find_property_entry(device *me,
		     const char *property)
{
  device_property_entry *entry;
  ASSERT(property != NULL);
  entry = me->properties;
  while (entry != NULL) {
    if (strcmp(entry->value->name, property) == 0)
      return entry;
    entry = entry->next;
  }
  return NULL;
}

STATIC_INLINE_DEVICE\
(void)
device_add_property(device *me,
		    const char *property,
		    device_property_type type,
		    const void *init_array,
		    unsigned sizeof_init_array,
		    const void *array,
		    unsigned sizeof_array,
		    const device_property *original,
		    object_disposition disposition)
{
  device_property_entry *new_entry = NULL;
  device_property *new_value = NULL;

  /* find the list end */
  device_property_entry **insertion_point = &me->properties;
  while (*insertion_point != NULL) {
    if (strcmp((*insertion_point)->value->name, property) == 0)
      return;
    insertion_point = &(*insertion_point)->next;
  }

  /* create a new value */
  new_value = ZALLOC(device_property);
  new_value->name = (char *) strdup(property);
  new_value->type = type;
  if (sizeof_array > 0) {
    void *new_array = zalloc(sizeof_array);
    memcpy(new_array, array, sizeof_array);
    new_value->array = new_array;
    new_value->sizeof_array = sizeof_array;
  }
  new_value->owner = me;
  new_value->original = original;
  new_value->disposition = disposition;

  /* insert the value into the list */
  new_entry = ZALLOC(device_property_entry);
  *insertion_point = new_entry;
  if (sizeof_init_array > 0) {
    void *new_init_array = zalloc(sizeof_init_array);
    memcpy(new_init_array, init_array, sizeof_init_array);
    new_entry->init_array = new_init_array;
    new_entry->sizeof_init_array = sizeof_init_array;
  }
  new_entry->value = new_value;
}


/* local - not available externally */
STATIC_INLINE_DEVICE\
(void)
device_set_property(device *me,
		    const char *property,
		    device_property_type type,
		    const void *array,
		    int sizeof_array)
{
  /* find the property */
  device_property_entry *entry = find_property_entry(me, property);
  if (entry != NULL) {
    /* existing property - update it */
    void *new_array = 0;
    device_property *value = entry->value;
    /* check the type matches */
    if (value->type != type)
      device_error(me, "conflict between type of new and old value for property %s", property);
    /* replace its value */
    if (value->array != NULL)
      zfree((void*)value->array);
    new_array = (sizeof_array > 0
		 ? zalloc(sizeof_array)
		 : (void*)0);
    value->array = new_array;
    value->sizeof_array = sizeof_array;
    if (sizeof_array > 0)
      memcpy(new_array, array, sizeof_array);
    return;
  }
  else {
    /* new property - create it */
    device_add_property(me, property, type,
			NULL, 0, array, sizeof_array,
			NULL, tempoary_object);
  }
}


STATIC_INLINE_DEVICE\
(void)
clean_device_properties(device *me)
{
  device_property_entry **delete_point = &me->properties;
  while (*delete_point != NULL) {
    device_property_entry *current = *delete_point;
    switch (current->value->disposition) {
    case permenant_object:
      /* zap the current value, will be initialized later */
      ASSERT(current->init_array != NULL);
      if (current->value->array != NULL) {
	zfree((void*)current->value->array);
	current->value->array = NULL;
      }
      delete_point = &(*delete_point)->next;
      break;
    case tempoary_object:
      /* zap the actual property, was created during simulation run */
      ASSERT(current->init_array == NULL);
      *delete_point = current->next;
      if (current->value->array != NULL)
	zfree((void*)current->value->array);
      zfree(current->value);
      zfree(current);
      break;
    }
  }
}


INLINE_DEVICE\
(void)
device_init_static_properties(device *me,
			      void *data)
{
  device_property_entry *property;
  for (property = me->properties;
       property != NULL;
       property = property->next) {
    ASSERT(property->init_array != NULL);
    ASSERT(property->value->array == NULL);
    ASSERT(property->value->disposition == permenant_object);
    switch (property->value->type) {
    case array_property:
    case boolean_property:
    case range_array_property:
    case reg_array_property:
    case string_property:
    case string_array_property:
    case integer_property:
      /* delete the property, and replace it with the original */
      device_set_property(me, property->value->name,
			  property->value->type,
			  property->init_array,
			  property->sizeof_init_array);
      break;
    case ihandle_property:
      break;
    }
  }
}


INLINE_DEVICE\
(void)
device_init_runtime_properties(device *me,
			       void *data)
{
  device_property_entry *property;
  for (property = me->properties;
       property != NULL;
       property = property->next) {
    switch (property->value->disposition) {
    case permenant_object:
      switch (property->value->type) {
      case ihandle_property:
	{
	  device_instance *ihandle;
	  ihandle_runtime_property_spec spec;
	  ASSERT(property->init_array != NULL);
	  ASSERT(property->value->array == NULL);
	  device_find_ihandle_runtime_property(me, property->value->name, &spec);
	  ihandle = tree_instance(me, spec.full_path);
	  device_set_ihandle_property(me, property->value->name, ihandle);
	  break;
	}
      case array_property:
      case boolean_property:
      case range_array_property:
      case integer_property:
      case reg_array_property:
      case string_property:
      case string_array_property:
	ASSERT(property->init_array != NULL);
	ASSERT(property->value->array != NULL);
	break;
      }
      break;
    case tempoary_object:
      ASSERT(property->init_array == NULL);
      ASSERT(property->value->array != NULL);
      break;
    }
  }
}


INLINE_DEVICE\
(const device_property *)
device_next_property(const device_property *property)
{
  /* find the property in the list */
  device *owner = property->owner;
  device_property_entry *entry = owner->properties;
  while (entry != NULL && entry->value != property)
    entry = entry->next;
  /* now return the following property */
  ASSERT(entry != NULL); /* must be a member! */
  if (entry->next != NULL)
    return entry->next->value;
  else
    return NULL;
}


INLINE_DEVICE\
(const device_property *)
device_find_property(device *me,
		     const char *property)
{
  if (me == NULL) {
    return NULL;
  }
  else if (property == NULL || strcmp(property, "") == 0) {
    if (me->properties == NULL)
      return NULL;
    else
      return me->properties->value;
  }
  else {
    device_property_entry *entry = find_property_entry(me, property);
    if (entry != NULL)
      return entry->value;
  }
  return NULL;
}


INLINE_DEVICE\
(void)
device_add_array_property(device *me,
                          const char *property,
                          const void *array,
                          int sizeof_array)
{
  device_add_property(me, property, array_property,
                      array, sizeof_array, array, sizeof_array,
                      NULL, permenant_object);
}

INLINE_DEVICE\
(void)
device_set_array_property(device *me,
			  const char *property,
			  const void *array,
			  int sizeof_array)
{
  device_set_property(me, property, array_property, array, sizeof_array);
}

INLINE_DEVICE\
(const device_property *)
device_find_array_property(device *me,
			   const char *property)
{
  const device_property *node;
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != array_property)
    device_error(me, "property %s not found or of wrong type", property);
  return node;
}


INLINE_DEVICE\
(void)
device_add_boolean_property(device *me,
                            const char *property,
                            int boolean)
{
  signed32 new_boolean = (boolean ? -1 : 0);
  device_add_property(me, property, boolean_property,
                      &new_boolean, sizeof(new_boolean),
                      &new_boolean, sizeof(new_boolean),
                      NULL, permenant_object);
}

INLINE_DEVICE\
(int)
device_find_boolean_property(device *me,
			     const char *property)
{
  const device_property *node;
  unsigned_cell boolean;
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != boolean_property)
    device_error(me, "property %s not found or of wrong type", property);
  ASSERT(sizeof(boolean) == node->sizeof_array);
  memcpy(&boolean, node->array, sizeof(boolean));
  return boolean;
}


INLINE_DEVICE\
(void)
device_add_ihandle_runtime_property(device *me,
				    const char *property,
				    const ihandle_runtime_property_spec *ihandle)
{
  /* enter the full path as the init array */
  device_add_property(me, property, ihandle_property,
		      ihandle->full_path, strlen(ihandle->full_path) + 1,
		      NULL, 0,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(void)
device_find_ihandle_runtime_property(device *me,
				     const char *property,
				     ihandle_runtime_property_spec *ihandle)
{
  device_property_entry *entry = find_property_entry(me, property);
  TRACE(trace_devices,
	("device_find_ihandle_runtime_property(me=0x%lx, property=%s)\n",
	 (long)me, property));
  if (entry == NULL
      || entry->value->type != ihandle_property
      || entry->value->disposition != permenant_object)
    device_error(me, "property %s not found or of wrong type", property);
  ASSERT(entry->init_array != NULL);
  /* the full path */
  ihandle->full_path = entry->init_array;
}



INLINE_DEVICE\
(void)
device_set_ihandle_property(device *me,
			    const char *property,
			    device_instance *ihandle)
{
  unsigned_cell cells;
  cells = H2BE_cell(device_instance_to_external(ihandle));
  device_set_property(me, property, ihandle_property,
		      &cells, sizeof(cells));
		      
}

INLINE_DEVICE\
(device_instance *)
device_find_ihandle_property(device *me,
			     const char *property)
{
  const device_property *node;
  unsigned_cell ihandle;
  device_instance *instance;

  node = device_find_property(me, property);
  if (node == NULL || node->type != ihandle_property)
    device_error(me, "property %s not found or of wrong type", property);
  if (node->array == NULL)
    device_error(me, "runtime property %s not yet initialized", property);

  ASSERT(sizeof(ihandle) == node->sizeof_array);
  memcpy(&ihandle, node->array, sizeof(ihandle));
  instance = external_to_device_instance(me, BE2H_cell(ihandle));
  ASSERT(instance != NULL);
  return instance;
}


INLINE_DEVICE\
(void)
device_add_integer_property(device *me,
			    const char *property,
			    signed_cell integer)
{
  H2BE(integer);
  device_add_property(me, property, integer_property,
                      &integer, sizeof(integer),
                      &integer, sizeof(integer),
                      NULL, permenant_object);
}

INLINE_DEVICE\
(signed_cell)
device_find_integer_property(device *me,
			     const char *property)
{
  const device_property *node;
  signed_cell integer;
  TRACE(trace_devices,
	("device_find_integer(me=0x%lx, property=%s)\n",
	 (long)me, property));
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != integer_property)
    device_error(me, "property %s not found or of wrong type", property);
  ASSERT(sizeof(integer) == node->sizeof_array);
  memcpy(&integer, node->array, sizeof(integer));
  return BE2H_cell(integer);
}

INLINE_DEVICE\
(int)
device_find_integer_array_property(device *me,
				   const char *property,
				   unsigned index,
				   signed_cell *integer)
{
  const device_property *node;
  int sizeof_integer = sizeof(*integer);
  signed_cell *cell;
  TRACE(trace_devices,
	("device_find_integer(me=0x%lx, property=%s)\n",
	 (long)me, property));

  /* check things sane */
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || (node->type != integer_property
	  && node->type != array_property))
    device_error(me, "property %s not found or of wrong type", property);
  if ((node->sizeof_array % sizeof_integer) != 0)
    device_error(me, "property %s contains an incomplete number of cells", property);
  if (node->sizeof_array <= sizeof_integer * index)
    return 0;

  /* Find and convert the value */
  cell = ((signed_cell*)node->array) + index;
  *integer = BE2H_cell(*cell);

  return node->sizeof_array / sizeof_integer;
}


STATIC_INLINE_DEVICE\
(unsigned_cell *)
unit_address_to_cells(const device_unit *unit,
		      unsigned_cell *cell,
		      int nr_cells)
{
  int i;
  ASSERT(nr_cells == unit->nr_cells);
  for (i = 0; i < unit->nr_cells; i++) {
    *cell = H2BE_cell(unit->cells[i]);
    cell += 1;
  }
  return cell;
}


STATIC_INLINE_DEVICE\
(const unsigned_cell *)
cells_to_unit_address(const unsigned_cell *cell,
		      device_unit *unit,
		      int nr_cells)
{
  int i;
  memset(unit, 0, sizeof(*unit));
  unit->nr_cells = nr_cells;
  for (i = 0; i < unit->nr_cells; i++) {
    unit->cells[i] = BE2H_cell(*cell);
    cell += 1;
  }
  return cell;
}


STATIC_INLINE_DEVICE\
(unsigned)
nr_range_property_cells(device *me,
			int nr_ranges)
{
  return ((device_nr_address_cells(me)
	   + device_nr_address_cells(device_parent(me))
	   + device_nr_size_cells(me))
	  ) * nr_ranges;
}

INLINE_DEVICE\
(void)
device_add_range_array_property(device *me,
				const char *property,
				const range_property_spec *ranges,
				unsigned nr_ranges)
{
  unsigned sizeof_cells = (nr_range_property_cells(me, nr_ranges)
			   * sizeof(unsigned_cell));
  unsigned_cell *cells = zalloc(sizeof_cells);
  unsigned_cell *cell;
  int i;

  /* copy the property elements over */
  cell = cells;
  for (i = 0; i < nr_ranges; i++) {
    const range_property_spec *range = &ranges[i];
    /* copy the child address */
    cell = unit_address_to_cells(&range->child_address, cell,
				 device_nr_address_cells(me));
    /* copy the parent address */
    cell = unit_address_to_cells(&range->parent_address, cell, 
				 device_nr_address_cells(device_parent(me)));
    /* copy the size */
    cell = unit_address_to_cells(&range->size, cell, 
				 device_nr_size_cells(me));
  }
  ASSERT(cell == &cells[nr_range_property_cells(me, nr_ranges)]);

  /* add it */
  device_add_property(me, property, range_array_property,
		      cells, sizeof_cells,
		      cells, sizeof_cells,
		      NULL, permenant_object);

  zfree(cells);
}

INLINE_DEVICE\
(int)
device_find_range_array_property(device *me,
				 const char *property,
				 unsigned index,
				 range_property_spec *range)
{
  const device_property *node;
  unsigned sizeof_entry = (nr_range_property_cells(me, 1)
			   * sizeof(unsigned_cell));
  const unsigned_cell *cells;

  /* locate the property */
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != range_array_property)
    device_error(me, "property %s not found or of wrong type", property);

  /* aligned ? */
  if ((node->sizeof_array % sizeof_entry) != 0)
    device_error(me, "property %s contains an incomplete number of entries",
		 property);

  /* within bounds? */
  if (node->sizeof_array < sizeof_entry * (index + 1))
    return 0;

  /* find the range of interest */
  cells = (unsigned_cell*)((char*)node->array + sizeof_entry * index);

  /* copy the child address out - converting as we go */
  cells = cells_to_unit_address(cells, &range->child_address,
				device_nr_address_cells(me));

  /* copy the parent address out - converting as we go */
  cells = cells_to_unit_address(cells, &range->parent_address,
				device_nr_address_cells(device_parent(me)));

  /* copy the size - converting as we go */
  cells = cells_to_unit_address(cells, &range->size,
				device_nr_size_cells(me));

  return node->sizeof_array / sizeof_entry;
}


STATIC_INLINE_DEVICE\
(unsigned)
nr_reg_property_cells(device *me,
		      int nr_regs)
{
  return (device_nr_address_cells(device_parent(me))
	  + device_nr_size_cells(device_parent(me))
	  ) * nr_regs;
}

INLINE_DEVICE\
(void)
device_add_reg_array_property(device *me,
			      const char *property,
			      const reg_property_spec *regs,
			      unsigned nr_regs)
{
  unsigned sizeof_cells = (nr_reg_property_cells(me, nr_regs)
			   * sizeof(unsigned_cell));
  unsigned_cell *cells = zalloc(sizeof_cells);
  unsigned_cell *cell;
  int i;

  /* copy the property elements over */
  cell = cells;
  for (i = 0; i < nr_regs; i++) {
    const reg_property_spec *reg = &regs[i];
    /* copy the address */
    cell = unit_address_to_cells(&reg->address, cell,
				 device_nr_address_cells(device_parent(me)));
    /* copy the size */
    cell = unit_address_to_cells(&reg->size, cell,
				 device_nr_size_cells(device_parent(me)));
  }
  ASSERT(cell == &cells[nr_reg_property_cells(me, nr_regs)]);

  /* add it */
  device_add_property(me, property, reg_array_property,
		      cells, sizeof_cells,
		      cells, sizeof_cells,
		      NULL, permenant_object);

  zfree(cells);
}

INLINE_DEVICE\
(int)
device_find_reg_array_property(device *me,
			       const char *property,
			       unsigned index,
			       reg_property_spec *reg)
{
  const device_property *node;
  unsigned sizeof_entry = (nr_reg_property_cells(me, 1)
			   * sizeof(unsigned_cell));
  const unsigned_cell *cells;

  /* locate the property */
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != reg_array_property)
    device_error(me, "property %s not found or of wrong type", property);

  /* aligned ? */
  if ((node->sizeof_array % sizeof_entry) != 0)
    device_error(me, "property %s contains an incomplete number of entries",
		 property);

  /* within bounds? */
  if (node->sizeof_array < sizeof_entry * (index + 1))
    return 0;

  /* find the range of interest */
  cells = (unsigned_cell*)((char*)node->array + sizeof_entry * index);

  /* copy the address out - converting as we go */
  cells = cells_to_unit_address(cells, &reg->address,
				device_nr_address_cells(device_parent(me)));

  /* copy the size out - converting as we go */
  cells = cells_to_unit_address(cells, &reg->size,
				device_nr_size_cells(device_parent(me)));

  return node->sizeof_array / sizeof_entry;
}


INLINE_DEVICE\
(void)
device_add_string_property(device *me,
                           const char *property,
                           const char *string)
{
  device_add_property(me, property, string_property,
                      string, strlen(string) + 1,
                      string, strlen(string) + 1,
                      NULL, permenant_object);
}

INLINE_DEVICE\
(const char *)
device_find_string_property(device *me,
			    const char *property)
{
  const device_property *node;
  const char *string;
  node = device_find_property(me, property);
  if (node == (device_property*)0
      || node->type != string_property)
    device_error(me, "property %s not found or of wrong type", property);
  string = node->array;
  ASSERT(strlen(string) + 1 == node->sizeof_array);
  return string;
}

INLINE_DEVICE\
(void)
device_add_string_array_property(device *me,
				 const char *property,
				 const string_property_spec *strings,
				 unsigned nr_strings)
{
  int sizeof_array;
  int string_nr;
  char *array;
  char *chp;
  if (nr_strings == 0)
    device_error(me, "property %s must be non-null", property);
  /* total up the size of the needed array */
  for (sizeof_array = 0, string_nr = 0;
       string_nr < nr_strings;
       string_nr ++) {
    sizeof_array += strlen(strings[string_nr]) + 1;
  }
  /* create the array */
  array = (char*)zalloc(sizeof_array);
  chp = array;
  for (string_nr = 0;
       string_nr < nr_strings;
       string_nr++) {
    strcpy(chp, strings[string_nr]);
    chp += strlen(chp) + 1;
  }
  ASSERT(chp == array + sizeof_array);
  /* now enter it */
  device_add_property(me, property, string_array_property,
		      array, sizeof_array,
		      array, sizeof_array,
		      NULL, permenant_object);
}

INLINE_DEVICE\
(int)
device_find_string_array_property(device *me,
				  const char *property,
				  unsigned index,
				  string_property_spec *string)
{
  const device_property *node;
  node = device_find_property(me, property);
  if (node == (device_property*)0)
    device_error(me, "property %s not found", property);
  switch (node->type) {
  default:
    device_error(me, "property %s of wrong type", property);
    break;
  case string_property:
    if (index == 0) {
      *string = node->array;
      ASSERT(strlen(*string) + 1 == node->sizeof_array);
      return 1;
    }
    break;
  case array_property:
    if (node->sizeof_array == 0
	|| ((char*)node->array)[node->sizeof_array - 1] != '\0')
      device_error(me, "property %s invalid for string array", property);
    /* FALL THROUGH */
  case string_array_property:
    ASSERT(node->sizeof_array > 0);
    ASSERT(((char*)node->array)[node->sizeof_array - 1] == '\0');
    {
      const char *chp = node->array;
      int nr_entries = 0;
      /* count the number of strings, keeping an eye out for the one
         we're looking for */
      *string = chp;
      do {
	if (*chp == '\0') {
	  /* next string */
	  nr_entries++;
	  chp++;
	  if (nr_entries == index)
	    *string = chp;
	}
	else {
	  chp++;
	}
      } while (chp < (char*)node->array + node->sizeof_array);
      if (index < nr_entries)
	return nr_entries;
      else {
	*string = NULL;
	return 0;
      }
    }
    break;
  }
  return 0;
}

INLINE_DEVICE\
(void)
device_add_duplicate_property(device *me,
			      const char *property,
			      const device_property *original)
{
  device_property_entry *master;
  TRACE(trace_devices,
	("device_add_duplicate_property(me=0x%lx, property=%s, ...)\n",
	 (long)me, property));
  if (original->disposition != permenant_object)
    device_error(me, "Can only duplicate permenant objects");
  /* find the original's master */
  master = original->owner->properties;
  while (master->value != original) {
    master = master->next;
    ASSERT(master != NULL);
  }
  /* now duplicate it */
  device_add_property(me, property,
		      original->type,
		      master->init_array, master->sizeof_init_array,
		      original->array, original->sizeof_array,
		      original, permenant_object);
}



/* Device Hardware: */

INLINE_DEVICE\
(unsigned)
device_io_read_buffer(device *me,
		      void *dest,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      cpu *processor,
		      unsigned_word cia)
{
  if (me->callback->io.read_buffer == NULL)
    device_error(me, "no io.read_buffer method");
  return me->callback->io.read_buffer(me, dest, space,
				      addr, nr_bytes,
				      processor, cia);
}

INLINE_DEVICE\
(unsigned)
device_io_write_buffer(device *me,
		       const void *source,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes,
		       cpu *processor,
		       unsigned_word cia)
{
  if (me->callback->io.write_buffer == NULL)
    device_error(me, "no io.write_buffer method");
  return me->callback->io.write_buffer(me, source, space,
				       addr, nr_bytes,
				       processor, cia);
}

INLINE_DEVICE\
(unsigned)
device_dma_read_buffer(device *me,
		       void *dest,
		       int space,
		       unsigned_word addr,
		       unsigned nr_bytes)
{
  if (me->callback->dma.read_buffer == NULL)
    device_error(me, "no dma.read_buffer method");
  return me->callback->dma.read_buffer(me, dest, space,
				       addr, nr_bytes);
}

INLINE_DEVICE\
(unsigned)
device_dma_write_buffer(device *me,
			const void *source,
			int space,
			unsigned_word addr,
			unsigned nr_bytes,
			int violate_read_only_section)
{
  if (me->callback->dma.write_buffer == NULL)
    device_error(me, "no dma.write_buffer method");
  return me->callback->dma.write_buffer(me, source, space,
					addr, nr_bytes,
					violate_read_only_section);
}

INLINE_DEVICE\
(void)
device_attach_address(device *me,
		      attach_type attach,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *client) /*callback/default*/
{
  if (me->callback->address.attach == NULL)
    device_error(me, "no address.attach method");
  me->callback->address.attach(me, attach, space,
			       addr, nr_bytes, access, client);
}

INLINE_DEVICE\
(void)
device_detach_address(device *me,
		      attach_type attach,
		      int space,
		      unsigned_word addr,
		      unsigned nr_bytes,
		      access_type access,
		      device *client) /*callback/default*/
{
  if (me->callback->address.detach == NULL)
    device_error(me, "no address.detach method");
  me->callback->address.detach(me, attach, space,
			       addr, nr_bytes, access, client);
}



/* Interrupts: */

INLINE_DEVICE(void)
device_interrupt_event(device *me,
		       int my_port,
		       int level,
		       cpu *processor,
		       unsigned_word cia)
{
  int found_an_edge = 0;
  device_interrupt_edge *edge;
  /* device's interrupt lines directly connected */
  for (edge = me->interrupt_destinations;
       edge != NULL;
       edge = edge->next) {
    if (edge->my_port == my_port) {
      if (edge->dest->callback->interrupt.event == NULL)
	device_error(me, "no interrupt method");
      edge->dest->callback->interrupt.event(edge->dest,
					    edge->dest_port,
					    me,
					    my_port,
					    level,
					    processor, cia);
      found_an_edge = 1;
    }
  }
  if (!found_an_edge) {
    device_error(me, "No interrupt edge for port %d", my_port);
  }
}

INLINE_DEVICE\
(void)
device_interrupt_attach(device *me,
			int my_port,
			device *dest,
			int dest_port,
			object_disposition disposition)
{
  attach_device_interrupt_edge(&me->interrupt_destinations,
			       my_port,
			       dest,
			       dest_port,
			       disposition);
}

INLINE_DEVICE\
(void)
device_interrupt_detach(device *me,
			int my_port,
			device *dest,
			int dest_port)
{
  detach_device_interrupt_edge(me,
			       &me->interrupt_destinations,
			       my_port,
			       dest,
			       dest_port);
}

INLINE_DEVICE\
(void)
device_interrupt_traverse(device *me,
			  device_interrupt_traverse_function *handler,
			  void *data)
{
  device_interrupt_edge *interrupt_edge;
  for (interrupt_edge = me->interrupt_destinations;
       interrupt_edge != NULL;
       interrupt_edge = interrupt_edge->next) {
    handler(me, interrupt_edge->my_port,
	    interrupt_edge->dest, interrupt_edge->dest_port,
	    data);
  }
}

INLINE_DEVICE\
(int)
device_interrupt_decode(device *me,
			const char *port_name,
			port_direction direction)
{
  if (port_name == NULL || port_name[0] == '\0')
    return 0;
  if (isdigit(port_name[0])) {
    return strtoul(port_name, NULL, 0);
  }
  else {
    const device_interrupt_port_descriptor *ports =
      me->callback->interrupt.ports;
    if (ports != NULL) {
      while (ports->name != NULL) {
	if (ports->direction == bidirect_port
	    || ports->direction == direction) {
	  if (ports->nr_ports > 0) {
	    int len = strlen(ports->name);
	    if (strncmp(port_name, ports->name, len) == 0) {
	      if (port_name[len] == '\0')
		return ports->number;
	      else if(isdigit(port_name[len])) {
		int port = ports->number + strtoul(&port_name[len], NULL, 0);
		if (port >= ports->number + ports->nr_ports)
		  device_error(me, "Interrupt port %s out of range",
			       port_name);
		return port;
	      }
	    }
	  }
	  else if (strcmp(port_name, ports->name) == 0)
	    return ports->number;
	}
	ports++;
      }
    }
  }
  device_error(me, "Unreconized interrupt port %s", port_name);
  return 0;
}

INLINE_DEVICE\
(int)
device_interrupt_encode(device *me,
			int port_number,
			char *buf,
			int sizeof_buf,
			port_direction direction)
{
  const device_interrupt_port_descriptor *ports = NULL;
  ports = me->callback->interrupt.ports;
  if (ports != NULL) {
    while (ports->name != NULL) {
      if (ports->direction == bidirect_port
	  || ports->direction == direction) {
	if (ports->nr_ports > 0) {
	  if (port_number >= ports->number
	      && port_number < ports->number + ports->nr_ports) {
	    strcpy(buf, ports->name);
	    sprintf(buf + strlen(buf), "%d", port_number - ports->number);
	    if (strlen(buf) >= sizeof_buf)
	      error("device_interrupt_encode: buffer overflow");
	    return strlen(buf);
	  }
	}
	else {
	  if (ports->number == port_number) {
	    if (strlen(ports->name) >= sizeof_buf)
	      error("device_interrupt_encode: buffer overflow");
	    strcpy(buf, ports->name);
	    return strlen(buf);
	  }
	}
      }
      ports++;
    }
  }
  sprintf(buf, "%d", port_number);
  if (strlen(buf) >= sizeof_buf)
    error("device_interrupt_encode: buffer overflow");
  return strlen(buf);
}



/* IOCTL: */

EXTERN_DEVICE\
(int)
device_ioctl(device *me,
	     cpu *processor,
	     unsigned_word cia,
	     device_ioctl_request request,
	     ...)
{
  int status;
  va_list ap;
  va_start(ap, request);
  if (me->callback->ioctl == NULL)
    device_error(me, "no ioctl method");
  status = me->callback->ioctl(me, processor, cia, request, ap);
  va_end(ap);
  return status;
}
      


/* I/O */

EXTERN_DEVICE\
(void volatile)
device_error(device *me,
	     const char *fmt,
	     ...)
{
  char message[1024];
  va_list ap;
  /* format the message */
  va_start(ap, fmt);
  vsprintf(message, fmt, ap);
  va_end(ap);
  /* sanity check */
  if (strlen(message) >= sizeof(message))
    error("device_error: buffer overflow");
  if (me == NULL)
    error("device: %s", message);
  else if (me->path != NULL && me->path[0] != '\0')
    error("%s: %s", me->path, message);
  else if (me->name != NULL && me->name[0] != '\0')
    error("%s: %s", me->name, message);
  else
    error("device: %s", message);
  while(1);
}

INLINE_DEVICE\
(int)
device_trace(device *me)
{
  return me->trace;
}


/* External representation */

INLINE_DEVICE\
(device *)
external_to_device(device *tree_member,
		   unsigned_cell phandle)
{
  device *me = cap_internal(tree_member->phandles, phandle);
  return me;
}

INLINE_DEVICE\
(unsigned_cell)
device_to_external(device *me)
{
  unsigned_cell phandle = cap_external(me->phandles, me);
  return phandle;
}

INLINE_DEVICE\
(device_instance *)
external_to_device_instance(device *tree_member,
			    unsigned_cell ihandle)
{
  device_instance *instance = cap_internal(tree_member->ihandles, ihandle);
  return instance;
}

INLINE_DEVICE\
(unsigned_cell)
device_instance_to_external(device_instance *instance)
{
  unsigned_cell ihandle = cap_external(instance->owner->ihandles, instance);
  return ihandle;
}


/* Map onto the event functions */

INLINE_DEVICE\
(event_entry_tag)
device_event_queue_schedule(device *me,
			    signed64 delta_time,
			    device_event_handler *handler,
			    void *data)
{
  return event_queue_schedule(psim_event_queue(me->system),
			      delta_time,
			      handler,
			      data);
}

INLINE_DEVICE\
(void)
device_event_queue_deschedule(device *me,
			      event_entry_tag event_to_remove)
{
  event_queue_deschedule(psim_event_queue(me->system),
			 event_to_remove);
}

INLINE_DEVICE\
(signed64)
device_event_queue_time(device *me)
{
  return event_queue_time(psim_event_queue(me->system));
}


/* Initialization: */


INLINE_DEVICE\
(void)
device_clean(device *me,
	     void *data)
{
  psim *system;
  system = (psim*)data;
  TRACE(trace_device_init, ("device_clean - initializing %s", me->path));
  clean_device_interrupt_edges(&me->interrupt_destinations);
  clean_device_instances(me);
  clean_device_properties(me);
}

/* Device initialization: */

INLINE_DEVICE\
(void)
device_init_address(device *me,
		    void *data)
{
  psim *system = (psim*)data;
  int nr_address_cells;
  int nr_size_cells;
  TRACE(trace_device_init, ("device_init_address - initializing %s", me->path));

  /* ensure the cap database is valid */
  if (me->parent == NULL) {
    cap_init(me->ihandles);
    cap_init(me->phandles);
  }

  /* some basics */
  me->system = system; /* misc things not known until now */
  me->trace = (device_find_property(me, "trace")
	       ? device_find_integer_property(me, "trace")
	       : 0);

  /* Ensure that the first address found in the reg property matches
     anything that was specified as part of the devices name */
  if (device_find_property(me, "reg") != NULL) {
    reg_property_spec unit;
    device_find_reg_array_property(me, "reg", 0, &unit);
    if (memcmp(device_unit_address(me), &unit.address, sizeof(unit.address))
	!= 0)
      device_error(me, "Unit address as specified by the reg property in conflict with the value previously specified in the devices path");
  }

  /* ensure that the devices #address/size-cells is consistent */
  nr_address_cells = device_nr_address_cells(me);
  if (device_find_property(me, "#address-cells") != NULL
      && (nr_address_cells
	  != device_find_integer_property(me, "#address-cells")))
    device_error(me, "#address-cells property used before defined");
  nr_size_cells = device_nr_size_cells(me);
  if (device_find_property(me, "#size-cells") != NULL
      && (nr_size_cells
	  != device_find_integer_property(me, "#size-cells")))
    device_error(me, "#size-cells property used before defined");

  /* now init it */
  if (me->callback->init.address != NULL)
    me->callback->init.address(me);
}

INLINE_DEVICE\
(void)
device_init_data(device *me,
		    void *data)
{
  TRACE(trace_device_init, ("device_init_data - initializing %s", me->path));
  if (me->callback->init.data != NULL)
    me->callback->init.data(me);
}

#endif /* _DEVICE_C_ */
