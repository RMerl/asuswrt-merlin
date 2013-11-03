/*
   Copyright (c) 2010 Frank Lahm
 
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef ATALK_QUEUE_H
#define ATALK_QUEUE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

typedef struct qnode {
    struct qnode *prev;
    struct qnode *next;
    void *data;
} qnode_t;

typedef qnode_t q_t;

extern q_t *queue_init(void);
extern void queue_destroy(q_t *q, void (*callback)(void *));
#define queue_free(q) queue_destroy((q), free)
extern qnode_t *enqueue(q_t *q, void *data);
extern qnode_t *prequeue(q_t *q, void *data);
extern void *dequeue(q_t *q);

#endif  /* ATALK_QUEUE_H */
