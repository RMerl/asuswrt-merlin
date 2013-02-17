/* 
 * Copyright (C) 1995, 1997-1999, 2002 Jeffrey A. Uphoff
 * Major rewrite by Olaf Kirch, Dec. 1996.
 *
 * NSM for Linux.
 */

#include <netinet/in.h>

/*
 * Primary information structure.
 */
struct notify_list {
  mon			mon;	/* Big honkin' NSM structure. */
  struct in_addr	addr;	/* IP address for callback. */
  unsigned short	port;	/* port number for callback */
  short int		times;	/* Counter used for various things. */
  int			state;	/* For storing notified state for callbacks. */
  char			*dns_name; /* used for matching incoming
				    * NOTIFY requests */
  struct notify_list	*next;	/* Linked list forward pointer. */
  struct notify_list	*prev;	/* Linked list backward pointer. */
  u_int32_t		xid;	/* XID of MS_NOTIFY RPC call */
  time_t		when;	/* notify: timeout for re-xmit */
};

typedef struct notify_list notify_list;

/*
 * Global Variables
 */
extern notify_list *	rtnl;	/* Run-time notify list */
extern notify_list *	notify;	/* Pending RPC calls */

/*
 * List-handling functions
 */
extern notify_list *	nlist_new(char *, char *, int);
extern void		nlist_insert(notify_list **, notify_list *);
extern void		nlist_remove(notify_list **, notify_list *);
extern void		nlist_insert_timer(notify_list **, notify_list *);
extern notify_list *	nlist_clone(notify_list *);
extern void		nlist_free(notify_list **, notify_list *);
extern void		nlist_kill(notify_list **);
extern notify_list *	nlist_gethost(notify_list *, char *, int);

/* 
 * List-handling macros.
 * THESE INHERIT INFORMATION FROM PREVIOUSLY-DEFINED MACROS.
 * (So don't change their order unless you study them first!)
 */
#define NL_NEXT(L)	((L)->next)
#define NL_FIRST	NL_NEXT
#define NL_PREV(L)	((L)->prev)
#define NL_DATA(L)	((L)->mon)
#define NL_ADDR(L)	((L)->addr)
#define NL_STATE(L)	((L)->state)
#define NL_TIMES(L)	((L)->times)
#define NL_MON_ID(L)	(NL_DATA((L)).mon_id)
#define NL_PRIV(L)	(NL_DATA((L)).priv)
#define NL_MON_NAME(L)	(NL_MON_ID((L)).mon_name)
#define NL_MY_ID(L)	(NL_MON_ID((L)).my_id)
#define NL_MY_NAME(L)	(NL_MY_ID((L)).my_name)
#define NL_MY_PROC(L)	(NL_MY_ID((L)).my_proc)
#define NL_MY_PROG(L)	(NL_MY_ID((L)).my_prog)
#define NL_MY_VERS(L)	(NL_MY_ID((L)).my_vers)
#define NL_WHEN(L)	((L)->when)
