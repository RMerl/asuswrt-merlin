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

#include "sim-io.h"
#include "sim-assert.h"

struct hw_instance_data {
  hw_finish_instance_method *to_finish;
  struct hw_instance *instances;
};

static hw_finish_instance_method abort_hw_finish_instance;

void
create_hw_instance_data (struct hw *me)
{
  me->instances_of_hw = HW_ZALLOC (me, struct hw_instance_data);
  set_hw_finish_instance (me, abort_hw_finish_instance);
}

void
delete_hw_instance_data (struct hw *me)
{
  /* NOP */
}


static void
abort_hw_finish_instance (struct hw *hw,
			  struct hw_instance *instance)
{
  hw_abort (hw, "no instance finish method");
}

void
set_hw_finish_instance (struct hw *me,
			hw_finish_instance_method *finish)
{
  me->instances_of_hw->to_finish = finish;
}


#if 0
void
clean_hw_instances (struct hw *me)
{
  struct hw_instance **instance = &me->instances;
  while (*instance != NULL)
    {
      struct hw_instance *old_instance = *instance;
      hw_instance_delete (old_instance);
      instance = &me->instances;
    }
}
#endif


void
hw_instance_delete (struct hw_instance *instance)
{
#if 1
  hw_abort (hw_instance_hw (instance), "not implemented");
#else
  struct hw *me = hw_instance_hw (instance);
  if (instance->to_instance_delete == NULL)
    hw_abort (me, "no delete method");
  instance->method->delete(instance);
  if (instance->args != NULL)
    zfree (instance->args);
  if (instance->path != NULL)
    zfree (instance->path);
  if (instance->child == NULL)
    {
      /* only remove leaf nodes */
      struct hw_instance **curr = &me->instances;
      while (*curr != instance)
	{
	  ASSERT (*curr != NULL);
	  curr = &(*curr)->next;
	}
      *curr = instance->next;
    }
  else
    {
      /* check it isn't in the instance list */
      struct hw_instance *curr = me->instances;
      while (curr != NULL)
	{
	  ASSERT(curr != instance);
	  curr = curr->next;
	}
      /* unlink the child */
      ASSERT (instance->child->parent == instance);
      instance->child->parent = NULL;
    }
  cap_remove (me->ihandles, instance);
  zfree (instance);
#endif
}


static int
panic_hw_instance_read (struct hw_instance *instance,
			void *addr,
			unsigned_word len)
{
  hw_abort (hw_instance_hw (instance), "no read method");
  return -1;
}



static int
panic_hw_instance_write (struct hw_instance *instance,
			 const void *addr,
			 unsigned_word len)
{
  hw_abort (hw_instance_hw (instance), "no write method");
  return -1;
}


static int
panic_hw_instance_seek (struct hw_instance *instance,
			unsigned_word pos_hi,
			unsigned_word pos_lo)
{
  hw_abort (hw_instance_hw (instance), "no seek method");
  return -1;
}


int
hw_instance_call_method (struct hw_instance *instance,
			 const char *method_name,
			 int n_stack_args,
			 unsigned_cell stack_args[/*n_stack_args*/],	
			 int n_stack_returns,
			 unsigned_cell stack_returns[/*n_stack_args*/])
{
#if 1
  hw_abort (hw_instance_hw (instance), "not implemented");
  return -1;
#else
  struct hw *me = instance->owner;
  const hw_instance_methods *method = instance->method->methods;
  if (method == NULL)
    {
      hw_abort (me, "no methods (want %s)", method_name);
    }
  while (method->name != NULL)
    {
      if (strcmp(method->name, method_name) == 0)
	{
	  return method->method (instance,
				 n_stack_args, stack_args,
				 n_stack_returns, stack_returns);
	}
      method++;
    }
  hw_abort (me, "no %s method", method_name);
  return 0;
#endif
}


#define set_hw_instance_read(instance, method)\
((instance)->to_instance_read = (method))

#define set_hw_instance_write(instance, method)\
((instance)->to_instance_write = (method))

#define set_hw_instance_seek(instance, method)\
((instance)->to_instance_seek = (method))


#if 0
static void
set_hw_instance_finish (struct hw *me,
			hw_instance_finish_method *method)
{
  if (me->instances_of_hw == NULL)
    me->instances_of_hw = HW_ZALLOC (me, struct hw_instance_data);
  me->instances_of_hw->to_finish = method;
}
#endif


struct hw_instance *
hw_instance_create (struct hw *me,
		    struct hw_instance *parent,
		    const char *path,
		    const char *args)
{
  struct hw_instance *instance = ZALLOC (struct hw_instance);
  /*instance->unit*/
  /* link this instance into the devices list */
  instance->hw_of_instance = me;
  instance->parent_of_instance = NULL;
  /* link this instance into the front of the devices instance list */
  instance->sibling_of_instance = me->instances_of_hw->instances;
  me->instances_of_hw->instances = instance;
  if (parent != NULL)
    {
      ASSERT (parent->child_of_instance == NULL);
      parent->child_of_instance = instance;
      instance->parent_of_instance = parent;
    }
  instance->args_of_instance = hw_strdup (me, args);
  instance->path_of_instance = hw_strdup (me, path);
  set_hw_instance_read (instance, panic_hw_instance_read);
  set_hw_instance_write (instance, panic_hw_instance_write);
  set_hw_instance_seek (instance, panic_hw_instance_seek);
  hw_handle_add_ihandle (me, instance);
  me->instances_of_hw->to_finish (me, instance);
  return instance;
}


struct hw_instance *
hw_instance_interceed (struct hw_instance *parent,
		       const char *path,
		       const char *args)
{
#if 1
  return NULL;
#else
  struct hw_instance *instance = ZALLOC (struct hw_instance);
  /*instance->unit*/
  /* link this instance into the devices list */
  if (me != NULL)
    {
      ASSERT (parent == NULL);
      instance->hw_of_instance = me;
      instance->parent_of_instance = NULL;
      /* link this instance into the front of the devices instance list */
      instance->sibling_of_instance = me->instances_of_hw->instances;
      me->instances_of_hw->instances = instance;
    }
  if (parent != NULL)
    {
      struct hw_instance **previous;
      ASSERT (parent->child_of_instance == NULL);
      parent->child_of_instance = instance;
      instance->owner = parent->owner;
      instance->parent_of_instance = parent;
      /* in the devices instance list replace the parent instance with
	 this one */
      instance->next = parent->next;
      /* replace parent with this new node */
      previous = &instance->owner->instances;
      while (*previous != parent)
	{
	  ASSERT (*previous != NULL);
	  previous = &(*previous)->next;
	}
      *previous = instance;
    }
  instance->data = data;
  instance->args = (args == NULL ? NULL : (char *) strdup(args));
  instance->path = (path == NULL ? NULL : (char *) strdup(path));
  cap_add (instance->owner->ihandles, instance);
  return instance;
#endif
}
