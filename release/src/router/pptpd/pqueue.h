#ifndef PQUEUE_H
#define PQUEUE_H

#include <time.h>
#include <sys/time.h>

/* wait this many seconds for missing packets before forgetting about them */
#define DEFAULT_PACKET_TIMEOUT 0.3
extern int packet_timeout_usecs;

/* assume packet is bad/spoofed if it's more than this many seqs ahead */
#define MISSING_WINDOW 300

/* Packet queue structure: linked list of packets received out-of-order */
typedef struct pqueue {
  struct pqueue *next;
  struct pqueue *prev;
  int seq;
  struct timeval expires;
  unsigned char *packet;
  int packlen;
  int capacity;
} pqueue_t;

int       pqueue_add  (int seq, unsigned char *packet, int packlen);
int       pqueue_del  (pqueue_t *point);
pqueue_t *pqueue_head ();
int       pqueue_expiry_time (pqueue_t *entry);

#endif /* PQUEUE_H */
