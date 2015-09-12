/* Priority queue functions.
   Copyright (C) 2003 Yasuhiro Ohara

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published
by the Free Software Foundation; either version 2, or (at your
option) any later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include <zebra.h>

#include "memory.h"
#include "pqueue.h"

/* priority queue using heap sort */

/* pqueue->cmp() controls the order of sorting (i.e, ascending or
   descending). If you want the left node to move upper of the heap
   binary tree, make cmp() to return less than 0.  for example, if cmp
   (10, 20) returns -1, the sorting is ascending order. if cmp (10,
   20) returns 1, the sorting is descending order. if cmp (10, 20)
   returns 0, this library does not do sorting (which will not be what
   you want).  To be brief, if the contents of cmp_func (left, right)
   is left - right, dequeue () returns the smallest node.  Otherwise
   (if the contents is right - left), dequeue () returns the largest
   node.  */

#define DATA_SIZE (sizeof (void *))
#define PARENT_OF(x) ((x - 1) / 2)
#define LEFT_OF(x)  (2 * x + 1)
#define RIGHT_OF(x) (2 * x + 2)
#define HAVE_CHILD(x,q) (x < (q)->size / 2)

void
trickle_up (int index, struct pqueue *queue)
{
  void *tmp;

  /* Save current node as tmp node.  */
  tmp = queue->array[index];

  /* Continue until the node reaches top or the place where the parent
     node should be upper than the tmp node.  */
  while (index > 0 &&
         (*queue->cmp) (tmp, queue->array[PARENT_OF (index)]) < 0)
    {
      /* actually trickle up */
      queue->array[index] = queue->array[PARENT_OF (index)];
      if (queue->update != NULL)
	(*queue->update) (queue->array[index], index);
      index = PARENT_OF (index);
    }

  /* Restore the tmp node to appropriate place.  */
  queue->array[index] = tmp;
  if (queue->update != NULL)
    (*queue->update) (tmp, index);
}

void
trickle_down (int index, struct pqueue *queue)
{
  void *tmp;
  int which;

  /* Save current node as tmp node.  */
  tmp = queue->array[index];

  /* Continue until the node have at least one (left) child.  */
  while (HAVE_CHILD (index, queue))
    {
      /* If right child exists, and if the right child is more proper
         to be moved upper.  */
      if (RIGHT_OF (index) < queue->size &&
          (*queue->cmp) (queue->array[LEFT_OF (index)],
                         queue->array[RIGHT_OF (index)]) > 0)
        which = RIGHT_OF (index);
      else
        which = LEFT_OF (index);

      /* If the tmp node should be upper than the child, break.  */
      if ((*queue->cmp) (queue->array[which], tmp) > 0)
        break;

      /* Actually trickle down the tmp node.  */
      queue->array[index] = queue->array[which];
       if (queue->update != NULL)
	 (*queue->update) (queue->array[index], index);
      index = which;
    }

  /* Restore the tmp node to appropriate place.  */
  queue->array[index] = tmp;
  if (queue->update != NULL)
    (*queue->update) (tmp, index);
}

struct pqueue *
pqueue_create (void)
{
  struct pqueue *queue;

  queue = XCALLOC (MTYPE_PQUEUE, sizeof (struct pqueue));

  queue->array = XCALLOC (MTYPE_PQUEUE_DATA, 
                          DATA_SIZE * PQUEUE_INIT_ARRAYSIZE);
  queue->array_size = PQUEUE_INIT_ARRAYSIZE;

  /* By default we want nothing to happen when a node changes. */
  queue->update = NULL;
  return queue;
}

void
pqueue_delete (struct pqueue *queue)
{
  XFREE (MTYPE_PQUEUE_DATA, queue->array);
  XFREE (MTYPE_PQUEUE, queue);
}

static int
pqueue_expand (struct pqueue *queue)
{
  void **newarray;

  newarray = XCALLOC (MTYPE_PQUEUE_DATA, queue->array_size * DATA_SIZE * 2);
  if (newarray == NULL)
    return 0;

  memcpy (newarray, queue->array, queue->array_size * DATA_SIZE);

  XFREE (MTYPE_PQUEUE_DATA, queue->array);
  queue->array = newarray;
  queue->array_size *= 2;

  return 1;
}

void
pqueue_enqueue (void *data, struct pqueue *queue)
{
  if (queue->size + 2 >= queue->array_size && ! pqueue_expand (queue))
    return;

  queue->array[queue->size] = data;
  if (queue->update != NULL)
    (*queue->update) (data, queue->size);
  trickle_up (queue->size, queue);
  queue->size ++;
}

void *
pqueue_dequeue (struct pqueue *queue)
{
  void *data = queue->array[0];
  queue->array[0] =  queue->array[--queue->size];
  trickle_down (0, queue);
  return data;
}

void
pqueue_remove_at (int index, struct pqueue *queue)
{
  queue->array[index] = queue->array[--queue->size];

  if (index > 0
      && (*queue->cmp) (queue->array[index],
                        queue->array[PARENT_OF(index)]) < 0)
    {
      trickle_up (index, queue);
    }
  else
    {
      trickle_down (index, queue);
    }
}
