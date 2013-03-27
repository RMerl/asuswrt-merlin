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


#ifndef HW_INSTANCES_H
#define HW_INSTANCES_H

/* Instances:

   As with IEEE1275, a device can be opened, creating an instance.
   Instances provide more abstract interfaces to the underlying
   hardware.  For example, the instance methods for a disk may include
   code that is able to interpret file systems found on disks.  Such
   methods would there for allow the manipulation of files on the
   disks file system.  The operations would be implemented using the
   basic block I/O model provided by the disk.

   This model includes methods that faciliate the creation of device
   instance and (should a given device support it) standard operations
   on those instances.

   */


struct hw_instance;


typedef void (hw_finish_instance_method)
     (struct hw *hw,
      struct hw_instance *);

extern void set_hw_finish_instance
(struct hw *hw,
 hw_finish_instance_method *method);


/* construct an instance of the hardware */

struct hw_instance *hw_instance_create
(struct hw *hw,
 struct hw_instance *parent,
 const char *path,
 const char *args);

struct hw_instance *hw_instance_interceed
(struct hw_instance *parent,
 const char *path,
 const char *args);

void hw_instance_delete
(struct hw_instance *instance);


/* methods applied to an instance of the hw */

typedef int (hw_instance_read_method)
     (struct hw_instance *instance,
      void *addr,
      unsigned_cell len);

#define hw_instance_read(instance, addr, len) \
((instance)->to_instance_read ((instance), (addr), (len)))

#define set_hw_instance_read(instance, method) \
((instance)->to_instance_read = (method))


typedef int (hw_instance_write_method)
     (struct hw_instance *instance,
      const void *addr,
      unsigned_cell len);

#define hw_instance_write(instance, addr, len) \
((instance)->to_instance_write ((instance), (addr), (len)))

#define set_hw_instance_write(instance, method) \
((instance)->to_instance_write = (method))


typedef int (hw_instance_seek_method)
     (struct hw_instance *instance,
      unsigned_cell pos_hi,
      unsigned_cell pos_lo);

#define hw_instance_seek(instance, pos_hi, pos_lo) \
((instance)->to_instance_seek ((instance), (pos_hi), (pos_lo)));

#define set_hw_instance_seek(instance, method) \
((instance)->to_instance_seek = (method))


int hw_instance_call_method
(struct hw_instance *instance,
 const char *method,
 int n_stack_args,
 unsigned_cell stack_args[/*n_stack_args + 1(NULL)*/],
 int n_stack_returns,
 unsigned_cell stack_returns[/*n_stack_returns + 1(NULL)*/]);



/* the definition of the instance */

#define hw_instance_hw(instance) ((instance)->hw_of_instance + 0)

#define hw_instance_path(instance) ((instance)->path_of_instance + 0)

#define hw_instance_args(instance) ((instance)->args_of_instance)

#define hw_instance_data(instance) ((instance)->data_of_instance)

#define hw_instance_system(instance) (hw_system (hw_instance_hw (instance)))



/* Finally an instance of a hardware device - keep your grubby little
   mits off of these internals! :-) */

struct hw_instance {

  void *data_of_instance;
  char *args_of_instance;
  char *path_of_instance;

  /* the device that owns the instance */
  struct hw *hw_of_instance;
  struct hw_instance *sibling_of_instance;

  /* interposed instance */
  struct hw_instance *parent_of_instance;
  struct hw_instance *child_of_instance;

  /* methods */
  hw_instance_read_method *to_instance_read;
  hw_instance_write_method *to_instance_write;
  hw_instance_seek_method *to_instance_seek;

};

#endif
