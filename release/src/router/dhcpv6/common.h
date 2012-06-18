/*	$KAME: common.h,v 1.42 2005/09/16 11:30:13 suz Exp $	*/
/*
 * Copyright (C) 1998 and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __KAME__
#define IN6_IFF_INVALID (IN6_IFF_ANYCAST|IN6_IFF_TENTATIVE|\
		IN6_IFF_DUPLICATED|IN6_IFF_DETACHED)
#else
#define IN6_IFF_INVALID (0)
#endif

#ifdef HAVE_ANSI_FUNC
#define FNAME __func__
#elif defined (HAVE_GCC_FUNCTION)
#define FNAME __FUNCTION__
#else
#define FNAME ""
#endif

/* XXX: bsdi4 does not have TAILQ_EMPTY */
#ifndef TAILQ_EMPTY
#define	TAILQ_EMPTY(head) ((head)->tqh_first == NULL)
#endif

/* and linux *_FIRST and *_NEXT */
#ifndef LIST_EMPTY
#define	LIST_EMPTY(head)	((head)->lh_first == NULL)
#endif
#ifndef LIST_FIRST
#define	LIST_FIRST(head)	((head)->lh_first)
#endif
#ifndef LIST_NEXT
#define	LIST_NEXT(elm, field)	((elm)->field.le_next)
#endif
#ifndef LIST_FOREACH
#define	LIST_FOREACH(var, head, field)					\
	for ((var) = LIST_FIRST((head));				\
	    (var);							\
	    (var) = LIST_NEXT((var), field))
#endif
#ifndef TAILQ_FIRST
#define	TAILQ_FIRST(head)	((head)->tqh_first)
#endif
#ifndef TAILQ_LAST
#define	TAILQ_LAST(head, headname)					\
	(*(((struct headname *)((head)->tqh_last))->tqh_last))
#endif
#ifndef TAILQ_PREV
#define	TAILQ_PREV(elm, headname, field)				\
	(*(((struct headname *)((elm)->field.tqe_prev))->tqh_last))
#endif
#ifndef TAILQ_NEXT
#define	TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#endif
#ifndef TAILQ_FOREACH
#define	TAILQ_FOREACH(var, head, field)					\
	for ((var) = TAILQ_FIRST((head));				\
	    (var);							\
	    (var) = TAILQ_NEXT((var), field))
#endif
#ifdef HAVE_TAILQ_FOREACH_REVERSE_OLD
#undef TAILQ_FOREACH_REVERSE
#endif
#ifndef TAILQ_FOREACH_REVERSE
#define	TAILQ_FOREACH_REVERSE(var, head, headname, field)		\
	for ((var) = TAILQ_LAST((head), headname);			\
	    (var);							\
	    (var) = TAILQ_PREV((var), headname, field))
#endif


#ifndef SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

/* s*_len stuff */
static __inline u_int8_t
sysdep_sa_len (const struct sockaddr *sa)
{
#ifndef HAVE_SA_LEN
  switch (sa->sa_family)
    {
    case AF_INET:
      return sizeof (struct sockaddr_in);
    case AF_INET6:
      return sizeof (struct sockaddr_in6);
    }
  return sizeof (struct sockaddr_in);
#else
  return sa->sa_len;
#endif
}

extern int foreground;
extern int debug_thresh;
extern int duid_type;
extern char *device;

#include "debug.h"

/* search option for dhcp6_find_listval() */
#define MATCHLIST_PREFIXLEN 0x1

/* common.c */
typedef enum { IFADDRCONF_ADD, IFADDRCONF_REMOVE } ifaddrconf_cmd_t;
extern int dhcp6_copy_list __P((struct dhcp6_list *, struct dhcp6_list *));
extern void dhcp6_move_list __P((struct dhcp6_list *, struct dhcp6_list *));
extern void dhcp6_clear_list __P((struct dhcp6_list *));
extern void dhcp6_clear_listval __P((struct dhcp6_listval *));
extern struct dhcp6_listval *dhcp6_find_listval __P((struct dhcp6_list *,
    dhcp6_listval_type_t, void *, int));
extern struct dhcp6_listval *dhcp6_add_listval __P((struct dhcp6_list *,
    dhcp6_listval_type_t, void *, struct dhcp6_list *));
extern int dhcp6_vbuf_copy __P((struct dhcp6_vbuf *, struct dhcp6_vbuf *));
extern void dhcp6_vbuf_free __P((struct dhcp6_vbuf *));
extern int dhcp6_vbuf_cmp __P((struct dhcp6_vbuf *, struct dhcp6_vbuf *));
extern struct dhcp6_event *dhcp6_create_event __P((struct dhcp6_if *, int));
extern void dhcp6_remove_event __P((struct dhcp6_event *));
extern void dhcp6_remove_evdata __P((struct dhcp6_event *));
extern struct authparam *new_authparam __P((int, int, int));
extern struct authparam *copy_authparam __P((struct authparam *));
extern int dhcp6_auth_replaycheck __P((int, u_int64_t, u_int64_t));
extern int getifaddr __P((struct in6_addr *, char *, struct in6_addr *,
			  int, int, int));
extern int getifidfromaddr __P((struct in6_addr *, unsigned int *));
extern int transmit_sa __P((int, struct sockaddr *, char *, size_t));
extern long random_between __P((long, long));
extern int prefix6_mask __P((struct in6_addr *, int));
extern int sa6_plen2mask __P((struct sockaddr_in6 *, int));
extern char *addr2str __P((struct sockaddr *));
extern char *in6addr2str __P((struct in6_addr *, int));
extern int in6_addrscopebyif __P((struct in6_addr *, char *));
extern int in6_scope __P((struct in6_addr *));
extern void setloglevel __P((int));
extern int get_duid __P((char *, struct duid *));
extern void dhcp6_init_options __P((struct dhcp6_optinfo *));
extern void dhcp6_clear_options __P((struct dhcp6_optinfo *));
extern int dhcp6_copy_options __P((struct dhcp6_optinfo *,
				   struct dhcp6_optinfo *));
extern int dhcp6_get_options __P((struct dhcp6opt *, struct dhcp6opt *,
				  struct dhcp6_optinfo *));
extern int dhcp6_set_options __P((int, struct dhcp6opt *, struct dhcp6opt *,
				  struct dhcp6_optinfo *));
extern void dhcp6_set_timeoparam __P((struct dhcp6_event *));
extern void dhcp6_reset_timer __P((struct dhcp6_event *));
extern char *dhcp6optstr __P((int));
extern char *dhcp6msgstr __P((int));
extern char *dhcp6_stcodestr __P((u_int16_t));
extern char *duidstr __P((struct duid *));
extern char *dhcp6_event_statestr __P((struct dhcp6_event *));
extern int get_rdvalue __P((int, void *, size_t));
extern int duidcpy __P((struct duid *, struct duid *));
extern int duidcmp __P((struct duid *, struct duid *));
extern void duidfree __P((struct duid *));
extern int ifaddrconf __P((ifaddrconf_cmd_t, char *, struct sockaddr_in6 *,
			   int, int, int));
extern int safefile __P((const char *));

/* missing */
#ifndef HAVE_STRLCAT
extern size_t strlcat __P((char *, const char *, size_t));
#endif
#ifndef HAVE_STRLCPY
extern size_t strlcpy __P((char *, const char *, size_t));
#endif

/*
 * compat hacks in case libc and kernel get out of sync:
 *
 * glibc 2.4 and uClibc 0.9.29 introduce IPV6_RECVPKTINFO etc. and change IPV6_PKTINFO
 * This is only supported in Linux kernel >= 2.6.14
 *
 * This is only an approximation because the kernel version that libc was compiled against
 * could be older or newer than the one being run.  But this should not be a problem --
 * we just keep using the old kernel interface.
 *
 * these are placed here because they're needed in all of socket.c, recv.c and send.c
 */
#ifdef __linux__
#  if defined IPV6_RECVHOPLIMIT || defined IPV6_RECVPKTINFO
#    include <linux/version.h>
#    if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
#      if defined IPV6_RECVHOPLIMIT && defined IPV6_2292HOPLIMIT
#        undef IPV6_RECVHOPLIMIT
#        define IPV6_RECVHOPLIMIT IPV6_2292HOPLIMIT
#      endif
#      if defined IPV6_RECVPKTINFO && defined IPV6_2292PKTINFO
#        undef IPV6_RECVPKTINFO
#        undef IPV6_PKTINFO
#        define IPV6_RECVPKTINFO IPV6_2292PKTINFO
#        define IPV6_PKTINFO IPV6_2292PKTINFO
#      endif
#    endif
#  endif
#endif

