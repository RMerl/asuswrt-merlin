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

#ifndef _ZEBRA_PQUEUE_H
#define _ZEBRA_PQUEUE_H

struct pqueue
{
  void **array;
  int array_size;
  int size;

  int (*cmp) (void *, void *);
};

#define PQUEUE_INIT_ARRAYSIZE  32

struct pqueue *pqueue_create ();
void pqueue_delete (struct pqueue *queue);

void pqueue_enqueue (void *data, struct pqueue *queue);
void *pqueue_dequeue (struct pqueue *queue);

#endif /* _ZEBRA_PQUEUE_H */
