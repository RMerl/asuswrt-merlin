#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util.h" // for log()
#include "pqueue.h"

#ifdef DEBUG_PQUEUE
#define DEBUG_ON 1
#else
#define DEBUG_ON 0
#endif

#define DEBUG_CMD(_a) if (DEBUG_ON) { _a }


#define MIN_CAPACITY 128 /* min allocated buffer for a packet */

static int pqueue_alloc (int seq, unsigned char *packet, int packlen, pqueue_t **new);

int packet_timeout_usecs = DEFAULT_PACKET_TIMEOUT * 1000000;


static pqueue_t *pq_head = NULL, *pq_tail = NULL;

/* contains a list of free queue elements.*/
static pqueue_t *pq_freelist_head = NULL;



static int pqueue_alloc(int seq, unsigned char *packet, int packlen, pqueue_t **new) {

  pqueue_t *newent;

  DEBUG_CMD(log("seq=%d, packlen=%d", seq, packlen););

  /* search the freelist for one that has sufficient space  */
  if (pq_freelist_head) {

    for (newent = pq_freelist_head; newent; newent = newent->next) {

      if (newent->capacity >= packlen) {

 	/* unlink from freelist */
	if (pq_freelist_head == newent) 
	  pq_freelist_head = newent->next;

	if (newent->prev) 
	  newent->prev->next = newent->next;

	if (newent->next) 
	  newent->next->prev = newent->prev;

	if (pq_freelist_head) 
	  pq_freelist_head->prev = NULL;

	break;
      }	/* end if capacity >= packlen */
    } /* end for */
	
    /* nothing found? Take first and reallocate it */
    if (NULL == newent) {

      newent = pq_freelist_head;
      pq_freelist_head = pq_freelist_head->next;

      if (pq_freelist_head) 
	pq_freelist_head->prev = NULL;

      DEBUG_CMD(log("realloc capacity %d to %d",newent->capacity, packlen););

      newent->packet = (unsigned char *)realloc(newent->packet, packlen);

      if (!newent->packet) {
	warn("error reallocating packet: %s", strerror(errno));
	return -1;
      }
      newent->capacity = packlen;
    }
    
    DEBUG_CMD(log("Recycle entry from freelist. Capacity: %d", newent->capacity););

  } else {

    /* allocate a new one */
    newent = (pqueue_t *)malloc( sizeof(pqueue_t) );
    if (!newent) {
      warn("error allocating newent: %s", strerror(errno));
      return -1;
    }
    newent->capacity = 0;

    DEBUG_CMD(log("Alloc new queue entry"););
  }

  if ( ! newent->capacity  ) {
    /* a new queue entry was allocated. Allocate the packet buffer */
    int size = packlen < MIN_CAPACITY ? MIN_CAPACITY : packlen;
    /* Allocate at least MIN_CAPACITY */
    DEBUG_CMD(log("allocating for packet size %d", size););
    newent->packet = (unsigned char *)malloc(size);
    if (!newent->packet) {
      warn("error allocating packet: %s", strerror(errno));
      return -1;
    }
    newent->capacity = size;
  } /* endif ! capacity */
  assert( newent->capacity >= packlen );
  /* store the contents into the buffer */
  memcpy(newent->packet, packet, packlen);

  newent->next = newent->prev = NULL;
  newent->seq     = seq;
  newent->packlen = packlen;

  gettimeofday(&newent->expires, NULL);
  newent->expires.tv_usec += packet_timeout_usecs;
  newent->expires.tv_sec  += (newent->expires.tv_usec / 1000000);
  newent->expires.tv_usec %= 1000000;

  *new = newent;
  return 0;
}



int pqueue_add (int seq, unsigned char *packet, int packlen) {
  pqueue_t *newent, *point;

  /* get a new entry */
  if ( 0 != pqueue_alloc(seq, packet, packlen, &newent) ) {
    return -1;
  }

  for (point = pq_head; point != NULL; point = point->next) {
    if (point->seq == seq) {
      // queue already contains this packet
      warn("discarding duplicate packet %d", seq);
      return -1;
    }
    if (point->seq > seq) {
      // gone too far: point->seq > seq and point->prev->seq < seq
      if (point->prev) {
	// insert between point->prev and point
	DEBUG_CMD(log("adding %d between %d and %d", 
		      seq, point->prev->seq, point->seq););

	point->prev->next = newent;
      } else {
	// insert at head of queue, before point
	DEBUG_CMD(log("adding %d before %d", seq, point->seq););
	pq_head = newent;
      }
      newent->prev = point->prev; // will be NULL, at head of queue
      newent->next = point;
      point->prev = newent;
      return 0;
    }
  }

  /* We didn't find anywhere to insert the packet,
   * so there are no packets in the queue with higher sequences than this one,
   * so all the packets in the queue have lower sequences,
   * so this packet belongs at the end of the queue (which might be empty)
   */
  
  if (pq_head == NULL) {
    DEBUG_CMD(log("adding %d to empty queue", seq););
    pq_head = newent;
  } else {
    DEBUG_CMD(log("adding %d as tail, after %d", seq, pq_tail->seq););
    pq_tail->next = newent;
  }
  newent->prev = pq_tail;
  pq_tail = newent;

  return 0;
}



int pqueue_del (pqueue_t *point) {

  DEBUG_CMD(log("Move seq %d to freelist", point->seq););

  /* unlink from pq */
  if (pq_head == point) pq_head = point->next;
  if (pq_tail == point) pq_tail = point->prev;
  if (point->prev) point->prev->next = point->next;
  if (point->next) point->next->prev = point->prev;

  /* add point to the freelist */
  point->next = pq_freelist_head;
  point->prev = NULL;

  if (point->next)
    point->next->prev = point;
  pq_freelist_head = point;

  DEBUG_CMD(
    int pq_count = 0;
    int pq_freelist_count = 0;
    pqueue_t *point;
    for ( point = pq_head; point ; point = point->next) {
      ++pq_count;
    }

    for ( point = pq_freelist_head; point ; point = point->next) {
      ++pq_freelist_count;
    }
    log("queue length is %d, freelist length is %d", pq_count, pq_freelist_count);
    );

  return 0;
}



pqueue_t *pqueue_head () {
  return pq_head;
}



int pqueue_expiry_time (pqueue_t *entry) {
  struct timeval tv;
  int expiry_time;

  gettimeofday(&tv, NULL);
  expiry_time  = (entry->expires.tv_sec  - tv.tv_sec) * 1000000;
  expiry_time += (entry->expires.tv_usec - tv.tv_usec);
  return expiry_time;
}
