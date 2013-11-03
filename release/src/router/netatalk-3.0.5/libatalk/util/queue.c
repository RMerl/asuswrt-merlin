/*
  Copyright (c) 2010 Frank Lahm <franklahm@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
*/

/*!
 * @file
 * Netatalk utility functions: queue
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <atalk/queue.h>

static qnode_t *alloc_init_node(void *data)
{
    qnode_t *node;
    if ((node = malloc(sizeof(qnode_t))) == NULL)
        return NULL;
    node->data = data;

    return node;
}

/********************************************************************************
 * Interface
 *******************************************************************************/

q_t *queue_init(void)
{
    q_t *queue;

    if ((queue = alloc_init_node(NULL)) == NULL)
        return NULL;

    queue->prev = queue->next = queue;
    return queue;
}

/* Insert at tail */
qnode_t *enqueue(q_t *q, void *data)
{
    qnode_t *node;

    if ((node = alloc_init_node(data)) == NULL)
        return NULL;

    /* insert at tail */
    node->next = q;
    node->prev = q->prev;
    q->prev->next = node;
    q->prev = node;

    return node;
}

/* Insert at head */
qnode_t *prequeue(q_t *q, void *data)
{
    qnode_t *node;

    if ((node = alloc_init_node(data)) == NULL)
        return NULL;

    /* insert at head */
    q->next->prev = node;
    node->next = q->next;
    node->prev = q;
    q->next = node;

    return node;
}

/* Take from head */
void *dequeue(q_t *q)
{
    qnode_t *node;
    void *data;

    if (q == NULL || q->next == q)
        return NULL;

    /* take first node from head */
    node = q->next;
    data = node->data;
    q->next = node->next;
    node->next->prev = node->prev;
    free(node);

    return data;    
}

void queue_destroy(q_t *q, void (*callback)(void *))
{
    void *p;

    while ((p = dequeue(q)) != NULL)
        callback(p);

    free(q);
    q = NULL;
}

