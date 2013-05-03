/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "dbutil.h"
#include "queue.h"

void initqueue(struct Queue* queue) {

	queue->head = NULL;
	queue->tail = NULL;
	queue->count = 0;
}

int isempty(struct Queue* queue) {

	return (queue->head == NULL);
}
	
void* dequeue(struct Queue* queue) {

	void* ret;
	struct Link* oldhead;
	dropbear_assert(!isempty(queue));
	
	ret = queue->head->item;
	oldhead = queue->head;
	
	if (oldhead->link != NULL) {
		queue->head = oldhead->link;
	} else {
		queue->head = NULL;
		queue->tail = NULL;
		TRACE(("empty queue dequeing"))
	}

	m_free(oldhead);
	queue->count--;
	return ret;
}

void *examine(struct Queue* queue) {

	dropbear_assert(!isempty(queue));
	return queue->head->item;
}

void enqueue(struct Queue* queue, void* item) {

	struct Link* newlink;

	newlink = (struct Link*)m_malloc(sizeof(struct Link));

	newlink->item = item;
	newlink->link = NULL;

	if (queue->tail != NULL) {
		queue->tail->link = newlink;
	}
	queue->tail = newlink;

	if (queue->head == NULL) {
		queue->head = newlink;
	}
	queue->count++;
}
