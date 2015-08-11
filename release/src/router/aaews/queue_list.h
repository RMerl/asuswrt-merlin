#ifndef _QUEUE_LIST_H
#define _QUEUE_LIST_H

typedef struct items 
{
	void* data ;
	struct items *next; // points to next element
} ITEM;

typedef struct queue {
	int size;
	ITEM *front, *rear;
} QUEUE;

void 	initQueue(QUEUE *q) ;
int 	queueIsEmpty(QUEUE *q); 
int 	queueLength(QUEUE *q);
void 	pushQueue(QUEUE *q, void* y);
void* 	popQueue(QUEUE *q);
#endif
