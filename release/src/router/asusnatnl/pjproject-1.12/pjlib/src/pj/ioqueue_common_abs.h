/* $Id */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/* ioqueue_common_abs.h
 *
 * This file contains private declarations for abstracting various 
 * event polling/dispatching mechanisms (e.g. select, poll, epoll) 
 * to the ioqueue. 
 */

#include <pj/list.h>

/*
 * The select ioqueue relies on socket functions (pj_sock_xxx()) to return
 * the correct error code.
 */
#if PJ_RETURN_OS_ERROR(100) != PJ_STATUS_FROM_OS(100)
#   error "Proper error reporting must be enabled for ioqueue to work!"
#endif


struct generic_operation
{
    PJ_DECL_LIST_MEMBER(struct generic_operation);
    pj_ioqueue_operation_e  op;
};

struct read_operation
{
    PJ_DECL_LIST_MEMBER(struct read_operation);
    pj_ioqueue_operation_e  op;

    void		   *buf;
    pj_size_t		    size;
    unsigned                flags;
    pj_sockaddr_t	   *rmt_addr;
    int			   *rmt_addrlen;
};

struct write_operation
{
    PJ_DECL_LIST_MEMBER(struct write_operation);
    pj_ioqueue_operation_e  op;

    char		   *buf;
    pj_size_t		    size;
    pj_ssize_t              written;
    unsigned                flags;
    pj_sockaddr_in	    rmt_addr;
    int			    rmt_addrlen;
};

struct accept_operation
{
    PJ_DECL_LIST_MEMBER(struct accept_operation);
    pj_ioqueue_operation_e  op;

    pj_sock_t              *accept_fd;
    pj_sockaddr_t	   *local_addr;
    pj_sockaddr_t	   *rmt_addr;
    int			   *addrlen;
};

union operation_key
{
    struct generic_operation generic;
    struct read_operation    read;
    struct write_operation   write;
#if PJ_HAS_TCP
    struct accept_operation  accept;
#endif
};

#if PJ_IOQUEUE_HAS_SAFE_UNREG
#   define UNREG_FIELDS			\
	unsigned	    ref_count;	\
	pj_bool_t	    closing;	\
	pj_time_val	    free_time;	\
	
#else
#   define UNREG_FIELDS
#endif

#define DECLARE_COMMON_KEY                          \
    PJ_DECL_LIST_MEMBER(struct pj_ioqueue_key_t);   \
    pj_ioqueue_t           *ioqueue;                \
    pj_mutex_t             *mutex;                  \
    pj_bool_t		    inside_callback;	    \
    pj_bool_t		    destroy_requested;	    \
    pj_bool_t		    allow_concurrent;	    \
    pj_sock_t		    fd;                     \
    int                     fd_type;                \
    void		   *user_data;              \
    pj_ioqueue_callback	    cb;                     \
    int                     connecting;             \
    struct read_operation   read_list;              \
    struct write_operation  write_list;             \
    struct accept_operation accept_list;	    \
    UNREG_FIELDS


#define DECLARE_COMMON_IOQUEUE                      \
    pj_lock_t          *lock;                       \
    pj_bool_t           auto_delete_lock;	    \
    pj_bool_t		default_concurrency;


enum ioqueue_event_type
{
    NO_EVENT,
    READABLE_EVENT,
    WRITEABLE_EVENT,
    EXCEPTION_EVENT,
};

static void ioqueue_add_to_set( pj_ioqueue_t *ioqueue,
                                pj_ioqueue_key_t *key,
                                enum ioqueue_event_type event_type );
static void ioqueue_remove_from_set( pj_ioqueue_t *ioqueue,
                                     pj_ioqueue_key_t *key, 
                                     enum ioqueue_event_type event_type);

