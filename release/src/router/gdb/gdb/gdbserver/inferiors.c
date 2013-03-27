/* Inferior process information for the remote server for GDB.
   Copyright (C) 2002, 2005, 2007 Free Software Foundation, Inc.

   Contributed by MontaVista Software.

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

#include <stdlib.h>

#include "server.h"

struct thread_info
{
  struct inferior_list_entry entry;
  void *target_data;
  void *regcache_data;
  unsigned int gdb_id;
};

struct inferior_list all_threads;
struct inferior_list all_dlls;
int dlls_changed;

struct thread_info *current_inferior;

#define get_thread(inf) ((struct thread_info *)(inf))
#define get_dll(inf) ((struct dll_info *)(inf))

void
add_inferior_to_list (struct inferior_list *list,
		      struct inferior_list_entry *new_inferior)
{
  new_inferior->next = NULL;
  if (list->tail != NULL)
    list->tail->next = new_inferior;
  else
    list->head = new_inferior;
  list->tail = new_inferior;
}

void
for_each_inferior (struct inferior_list *list,
		   void (*action) (struct inferior_list_entry *))
{
  struct inferior_list_entry *cur = list->head, *next;

  while (cur != NULL)
    {
      next = cur->next;
      (*action) (cur);
      cur = next;
    }
}

/* When debugging a single-threaded program, the threads list (such as
   it is) is indexed by PID.  When debugging a multi-threaded program,
   we index by TID.  This ugly routine replaces the
   first-debugged-thread's PID with its TID.  */

void
change_inferior_id (struct inferior_list *list,
		    unsigned long new_id)
{
  if (list->head != list->tail)
    error ("tried to change thread ID after multiple threads are created");

  list->head->id = new_id;
}

void
remove_inferior (struct inferior_list *list,
		 struct inferior_list_entry *entry)
{
  struct inferior_list_entry **cur;

  if (list->head == entry)
    {
      list->head = entry->next;
      if (list->tail == entry)
	list->tail = list->head;
      return;
    }

  cur = &list->head;
  while (*cur && (*cur)->next != entry)
    cur = &(*cur)->next;

  if (*cur == NULL)
    return;

  (*cur)->next = entry->next;

  if (list->tail == entry)
    list->tail = *cur;
}

void
add_thread (unsigned long thread_id, void *target_data, unsigned int gdb_id)
{
  struct thread_info *new_thread = malloc (sizeof (*new_thread));

  memset (new_thread, 0, sizeof (*new_thread));

  new_thread->entry.id = thread_id;

  add_inferior_to_list (&all_threads, & new_thread->entry);

  if (current_inferior == NULL)
    current_inferior = new_thread;

  new_thread->target_data = target_data;
  set_inferior_regcache_data (new_thread, new_register_cache ());
  new_thread->gdb_id = gdb_id;
}

unsigned int
thread_id_to_gdb_id (unsigned long thread_id)
{
  struct inferior_list_entry *inf = all_threads.head;

  while (inf != NULL)
    {
      struct thread_info *thread = get_thread (inf);
      if (inf->id == thread_id)
	return thread->gdb_id;
      inf = inf->next;
    }

  return 0;
}

unsigned int
thread_to_gdb_id (struct thread_info *thread)
{
  return thread->gdb_id;
}

struct thread_info *
gdb_id_to_thread (unsigned int gdb_id)
{
  struct inferior_list_entry *inf = all_threads.head;

  while (inf != NULL)
    {
      struct thread_info *thread = get_thread (inf);
      if (thread->gdb_id == gdb_id)
	return thread;
      inf = inf->next;
    }

  return NULL;
}

unsigned long
gdb_id_to_thread_id (unsigned int gdb_id)
{
  struct thread_info *thread = gdb_id_to_thread (gdb_id);

  return thread ? thread->entry.id : 0;
}

static void
free_one_thread (struct inferior_list_entry *inf)
{
  struct thread_info *thread = get_thread (inf);
  free_register_cache (inferior_regcache_data (thread));
  free (thread);
}

void
remove_thread (struct thread_info *thread)
{
  remove_inferior (&all_threads, (struct inferior_list_entry *) thread);
  free_one_thread (&thread->entry);
}

struct inferior_list_entry *
find_inferior (struct inferior_list *list,
	       int (*func) (struct inferior_list_entry *, void *), void *arg)
{
  struct inferior_list_entry *inf = list->head;

  while (inf != NULL)
    {
      if ((*func) (inf, arg))
	return inf;
      inf = inf->next;
    }

  return NULL;
}

struct inferior_list_entry *
find_inferior_id (struct inferior_list *list, unsigned long id)
{
  struct inferior_list_entry *inf = list->head;

  while (inf != NULL)
    {
      if (inf->id == id)
	return inf;
      inf = inf->next;
    }

  return NULL;
}

void *
inferior_target_data (struct thread_info *inferior)
{
  return inferior->target_data;
}

void
set_inferior_target_data (struct thread_info *inferior, void *data)
{
  inferior->target_data = data;
}

void *
inferior_regcache_data (struct thread_info *inferior)
{
  return inferior->regcache_data;
}

void
set_inferior_regcache_data (struct thread_info *inferior, void *data)
{
  inferior->regcache_data = data;
}

static void
free_one_dll (struct inferior_list_entry *inf)
{
  struct dll_info *dll = get_dll (inf);
  if (dll->name != NULL)
    free (dll->name);
  free (dll);
}

/* Find a DLL with the same name and/or base address.  A NULL name in
   the key is ignored; so is an all-ones base address.  */

static int
match_dll (struct inferior_list_entry *inf, void *arg)
{
  struct dll_info *iter = (void *) inf;
  struct dll_info *key = arg;

  if (key->base_addr != ~(CORE_ADDR) 0
      && iter->base_addr == key->base_addr)
    return 1;
  else if (key->name != NULL
	   && iter->name != NULL
	   && strcmp (key->name, iter->name) == 0)
    return 1;

  return 0;
}

/* Record a newly loaded DLL at BASE_ADDR.  */

void
loaded_dll (const char *name, CORE_ADDR base_addr)
{
  struct dll_info *new_dll = malloc (sizeof (*new_dll));
  memset (new_dll, 0, sizeof (*new_dll));

  new_dll->entry.id = -1;

  new_dll->name = strdup (name);
  new_dll->base_addr = base_addr;

  add_inferior_to_list (&all_dlls, &new_dll->entry);
  dlls_changed = 1;
}

/* Record that the DLL with NAME and BASE_ADDR has been unloaded.  */

void
unloaded_dll (const char *name, CORE_ADDR base_addr)
{
  struct dll_info *dll;
  struct dll_info key_dll;

  /* Be careful not to put the key DLL in any list.  */
  key_dll.name = (char *) name;
  key_dll.base_addr = base_addr;

  dll = (void *) find_inferior (&all_dlls, match_dll, &key_dll);
  remove_inferior (&all_dlls, &dll->entry);
  free_one_dll (&dll->entry);
  dlls_changed = 1;
}

#define clear_list(LIST) \
  do { (LIST)->head = (LIST)->tail = NULL; } while (0)

void
clear_inferiors (void)
{
  for_each_inferior (&all_threads, free_one_thread);
  for_each_inferior (&all_dlls, free_one_dll);

  clear_list (&all_threads);
  clear_list (&all_dlls);
}
