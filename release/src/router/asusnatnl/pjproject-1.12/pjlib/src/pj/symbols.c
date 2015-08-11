/* $Id: symbols.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pjlib.h>

/*
 * addr_resolv.h
 */
PJ_EXPORT_SYMBOL(pj_gethostbyname)

/*
 * array.h
 */
PJ_EXPORT_SYMBOL(pj_array_insert)
PJ_EXPORT_SYMBOL(pj_array_erase)
PJ_EXPORT_SYMBOL(pj_array_find)

/*
 * config.h
 */
PJ_EXPORT_SYMBOL(pj_dump_config)
	
/*
 * errno.h
 */
PJ_EXPORT_SYMBOL(pj_get_os_error)
PJ_EXPORT_SYMBOL(pj_set_os_error)
PJ_EXPORT_SYMBOL(pj_get_netos_error)
PJ_EXPORT_SYMBOL(pj_set_netos_error)
PJ_EXPORT_SYMBOL(pj_strerror)

/*
 * except.h
 */
PJ_EXPORT_SYMBOL(pj_throw_exception_)
PJ_EXPORT_SYMBOL(pj_push_exception_handler_)
PJ_EXPORT_SYMBOL(pj_pop_exception_handler_)
PJ_EXPORT_SYMBOL(pj_setjmp)
PJ_EXPORT_SYMBOL(pj_longjmp)
PJ_EXPORT_SYMBOL(pj_exception_id_alloc)
PJ_EXPORT_SYMBOL(pj_exception_id_free)
PJ_EXPORT_SYMBOL(pj_exception_id_name)


/*
 * fifobuf.h
 */
PJ_EXPORT_SYMBOL(pj_fifobuf_init)
PJ_EXPORT_SYMBOL(pj_fifobuf_max_size)
PJ_EXPORT_SYMBOL(pj_fifobuf_alloc)
PJ_EXPORT_SYMBOL(pj_fifobuf_unalloc)
PJ_EXPORT_SYMBOL(pj_fifobuf_free)

/*
 * guid.h
 */
PJ_EXPORT_SYMBOL(pj_generate_unique_string)
PJ_EXPORT_SYMBOL(pj_create_unique_string)

/*
 * hash.h
 */
PJ_EXPORT_SYMBOL(pj_hash_calc)
PJ_EXPORT_SYMBOL(pj_hash_create)
PJ_EXPORT_SYMBOL(pj_hash_get)
PJ_EXPORT_SYMBOL(pj_hash_set)
PJ_EXPORT_SYMBOL(pj_hash_count)
PJ_EXPORT_SYMBOL(pj_hash_first)
PJ_EXPORT_SYMBOL(pj_hash_next)
PJ_EXPORT_SYMBOL(pj_hash_this)

/*
 * ioqueue.h
 */
PJ_EXPORT_SYMBOL(pj_ioqueue_create)
PJ_EXPORT_SYMBOL(pj_ioqueue_destroy)
PJ_EXPORT_SYMBOL(pj_ioqueue_set_lock)
PJ_EXPORT_SYMBOL(pj_ioqueue_register_sock)
PJ_EXPORT_SYMBOL(pj_ioqueue_unregister)
PJ_EXPORT_SYMBOL(pj_ioqueue_get_user_data)
PJ_EXPORT_SYMBOL(pj_ioqueue_poll)
PJ_EXPORT_SYMBOL(pj_ioqueue_read)
PJ_EXPORT_SYMBOL(pj_ioqueue_recv)
PJ_EXPORT_SYMBOL(pj_ioqueue_recvfrom)
PJ_EXPORT_SYMBOL(pj_ioqueue_write)
PJ_EXPORT_SYMBOL(pj_ioqueue_send)
PJ_EXPORT_SYMBOL(pj_ioqueue_sendto)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
PJ_EXPORT_SYMBOL(pj_ioqueue_accept)
PJ_EXPORT_SYMBOL(pj_ioqueue_connect)
#endif

/*
 * list.h
 */
PJ_EXPORT_SYMBOL(pj_list_insert_before)
PJ_EXPORT_SYMBOL(pj_list_insert_nodes_before)
PJ_EXPORT_SYMBOL(pj_list_insert_after)
PJ_EXPORT_SYMBOL(pj_list_insert_nodes_after)
PJ_EXPORT_SYMBOL(pj_list_merge_first)
PJ_EXPORT_SYMBOL(pj_list_merge_last)
PJ_EXPORT_SYMBOL(pj_list_erase)
PJ_EXPORT_SYMBOL(pj_list_find_node)
PJ_EXPORT_SYMBOL(pj_list_search)


/*
 * log.h
 */
PJ_EXPORT_SYMBOL(pj_log_write)
#if PJ_LOG_MAX_LEVEL >= 1
PJ_EXPORT_SYMBOL(pj_log_set_log_func)
PJ_EXPORT_SYMBOL(pj_log_get_log_func)
PJ_EXPORT_SYMBOL(pj_log_set_level)
PJ_EXPORT_SYMBOL(pj_log_get_level)
PJ_EXPORT_SYMBOL(pj_log_set_decor)
PJ_EXPORT_SYMBOL(pj_log_get_decor)
PJ_EXPORT_SYMBOL(pj_log_1)
#endif
#if PJ_LOG_MAX_LEVEL >= 2
PJ_EXPORT_SYMBOL(pj_log_2)
#endif
#if PJ_LOG_MAX_LEVEL >= 3
PJ_EXPORT_SYMBOL(pj_log_3)
#endif
#if PJ_LOG_MAX_LEVEL >= 4
PJ_EXPORT_SYMBOL(pj_log_4)
#endif
#if PJ_LOG_MAX_LEVEL >= 5
PJ_EXPORT_SYMBOL(pj_log_5)
#endif
#if PJ_LOG_MAX_LEVEL >= 6
PJ_EXPORT_SYMBOL(pj_log_6)
#endif

/*
 * os.h
 */
PJ_EXPORT_SYMBOL(pj_init)
PJ_EXPORT_SYMBOL(pj_getpid)
PJ_EXPORT_SYMBOL(pj_thread_register)
PJ_EXPORT_SYMBOL(pj_thread_create)
PJ_EXPORT_SYMBOL(pj_thread_get_name)
PJ_EXPORT_SYMBOL(pj_thread_resume)
PJ_EXPORT_SYMBOL(pj_thread_this)
PJ_EXPORT_SYMBOL(pj_thread_join)
PJ_EXPORT_SYMBOL(pj_thread_destroy)
PJ_EXPORT_SYMBOL(pj_thread_sleep)
#if defined(PJ_OS_HAS_CHECK_STACK) && PJ_OS_HAS_CHECK_STACK != 0
PJ_EXPORT_SYMBOL(pj_thread_check_stack)
PJ_EXPORT_SYMBOL(pj_thread_get_stack_max_usage)
PJ_EXPORT_SYMBOL(pj_thread_get_stack_info)
#endif
PJ_EXPORT_SYMBOL(pj_atomic_create)
PJ_EXPORT_SYMBOL(pj_atomic_destroy)
PJ_EXPORT_SYMBOL(pj_atomic_set)
PJ_EXPORT_SYMBOL(pj_atomic_get)
PJ_EXPORT_SYMBOL(pj_atomic_inc)
PJ_EXPORT_SYMBOL(pj_atomic_dec)
PJ_EXPORT_SYMBOL(pj_thread_local_alloc)
PJ_EXPORT_SYMBOL(pj_thread_local_free)
PJ_EXPORT_SYMBOL(pj_thread_local_set)
PJ_EXPORT_SYMBOL(pj_thread_local_get)
PJ_EXPORT_SYMBOL(pj_enter_critical_section)
PJ_EXPORT_SYMBOL(pj_leave_critical_section)
PJ_EXPORT_SYMBOL(pj_mutex_create)
PJ_EXPORT_SYMBOL(pj_mutex_lock)
PJ_EXPORT_SYMBOL(pj_mutex_unlock)
PJ_EXPORT_SYMBOL(pj_mutex_trylock)
PJ_EXPORT_SYMBOL(pj_mutex_destroy)
#if defined(PJ_DEBUG) && PJ_DEBUG != 0
PJ_EXPORT_SYMBOL(pj_mutex_is_locked)
#endif
#if defined(PJ_HAS_SEMAPHORE) && PJ_HAS_SEMAPHORE != 0
PJ_EXPORT_SYMBOL(pj_sem_create)
PJ_EXPORT_SYMBOL(pj_sem_wait)
PJ_EXPORT_SYMBOL(pj_sem_trywait)
PJ_EXPORT_SYMBOL(pj_sem_post)
PJ_EXPORT_SYMBOL(pj_sem_destroy)
#endif
PJ_EXPORT_SYMBOL(pj_gettimeofday)
PJ_EXPORT_SYMBOL(pj_time_decode)
#if defined(PJ_HAS_HIGH_RES_TIMER) && PJ_HAS_HIGH_RES_TIMER != 0
PJ_EXPORT_SYMBOL(pj_gettickcount)
PJ_EXPORT_SYMBOL(pj_get_timestamp)
PJ_EXPORT_SYMBOL(pj_get_timestamp_freq)
PJ_EXPORT_SYMBOL(pj_elapsed_time)
PJ_EXPORT_SYMBOL(pj_elapsed_usec)
PJ_EXPORT_SYMBOL(pj_elapsed_nanosec)
PJ_EXPORT_SYMBOL(pj_elapsed_cycle)
#endif

	
/*
 * pool.h
 */
PJ_EXPORT_SYMBOL(pj_pool_create)
PJ_EXPORT_SYMBOL(pj_pool_release)
PJ_EXPORT_SYMBOL(pj_pool_getobjname)
PJ_EXPORT_SYMBOL(pj_pool_reset)
PJ_EXPORT_SYMBOL(pj_pool_get_capacity)
PJ_EXPORT_SYMBOL(pj_pool_get_used_size)
PJ_EXPORT_SYMBOL(pj_pool_alloc)
PJ_EXPORT_SYMBOL(pj_pool_calloc)
PJ_EXPORT_SYMBOL(pj_pool_factory_default_policy)
PJ_EXPORT_SYMBOL(pj_pool_create_int)
PJ_EXPORT_SYMBOL(pj_pool_init_int)
PJ_EXPORT_SYMBOL(pj_pool_destroy_int)
PJ_EXPORT_SYMBOL(pj_caching_pool_init)
PJ_EXPORT_SYMBOL(pj_caching_pool_destroy)

/*
 * rand.h
 */
PJ_EXPORT_SYMBOL(pj_rand)
PJ_EXPORT_SYMBOL(pj_srand)

/*
 * rbtree.h
 */
PJ_EXPORT_SYMBOL(pj_rbtree_init)
PJ_EXPORT_SYMBOL(pj_rbtree_first)
PJ_EXPORT_SYMBOL(pj_rbtree_last)
PJ_EXPORT_SYMBOL(pj_rbtree_next)
PJ_EXPORT_SYMBOL(pj_rbtree_prev)
PJ_EXPORT_SYMBOL(pj_rbtree_insert)
PJ_EXPORT_SYMBOL(pj_rbtree_find)
PJ_EXPORT_SYMBOL(pj_rbtree_erase)
PJ_EXPORT_SYMBOL(pj_rbtree_max_height)
PJ_EXPORT_SYMBOL(pj_rbtree_min_height)

/*
 * sock.h
 */
PJ_EXPORT_SYMBOL(PJ_AF_UNIX)
PJ_EXPORT_SYMBOL(PJ_AF_INET)
PJ_EXPORT_SYMBOL(PJ_AF_INET6)
PJ_EXPORT_SYMBOL(PJ_AF_PACKET)
PJ_EXPORT_SYMBOL(PJ_AF_IRDA)
PJ_EXPORT_SYMBOL(PJ_SOCK_STREAM)
PJ_EXPORT_SYMBOL(PJ_SOCK_DGRAM)
PJ_EXPORT_SYMBOL(PJ_SOCK_RAW)
PJ_EXPORT_SYMBOL(PJ_SOCK_RDM)
PJ_EXPORT_SYMBOL(PJ_SOL_SOCKET)
PJ_EXPORT_SYMBOL(PJ_SOL_IP)
PJ_EXPORT_SYMBOL(PJ_SOL_TCP)
PJ_EXPORT_SYMBOL(PJ_SOL_UDP)
PJ_EXPORT_SYMBOL(PJ_SOL_IPV6)
PJ_EXPORT_SYMBOL(pj_ntohs)
PJ_EXPORT_SYMBOL(pj_htons)
PJ_EXPORT_SYMBOL(pj_ntohl)
PJ_EXPORT_SYMBOL(pj_htonl)
PJ_EXPORT_SYMBOL(pj_inet_ntoa)
PJ_EXPORT_SYMBOL(pj_inet_aton)
PJ_EXPORT_SYMBOL(pj_inet_addr)
PJ_EXPORT_SYMBOL(pj_sockaddr_in_set_str_addr)
PJ_EXPORT_SYMBOL(pj_sockaddr_in_init)
PJ_EXPORT_SYMBOL(pj_gethostname)
PJ_EXPORT_SYMBOL(pj_gethostaddr)
PJ_EXPORT_SYMBOL(pj_sock_socket)
PJ_EXPORT_SYMBOL(pj_sock_close)
PJ_EXPORT_SYMBOL(pj_sock_bind)
PJ_EXPORT_SYMBOL(pj_sock_bind_in)
#if defined(PJ_HAS_TCP) && PJ_HAS_TCP != 0
PJ_EXPORT_SYMBOL(pj_sock_listen)
PJ_EXPORT_SYMBOL(pj_sock_accept)
PJ_EXPORT_SYMBOL(pj_sock_shutdown)
#endif
PJ_EXPORT_SYMBOL(pj_sock_connect)
PJ_EXPORT_SYMBOL(pj_sock_getpeername)
PJ_EXPORT_SYMBOL(pj_sock_getsockname)
PJ_EXPORT_SYMBOL(pj_sock_getsockopt)
PJ_EXPORT_SYMBOL(pj_sock_setsockopt)
PJ_EXPORT_SYMBOL(pj_sock_recv)
PJ_EXPORT_SYMBOL(pj_sock_recvfrom)
PJ_EXPORT_SYMBOL(pj_sock_send)
PJ_EXPORT_SYMBOL(pj_sock_sendto)

/*
 * sock_select.h
 */
PJ_EXPORT_SYMBOL(PJ_FD_ZERO)
PJ_EXPORT_SYMBOL(PJ_FD_SET)
PJ_EXPORT_SYMBOL(PJ_FD_CLR)
PJ_EXPORT_SYMBOL(PJ_FD_ISSET)
PJ_EXPORT_SYMBOL(pj_sock_select)

/*
 * string.h
 */
PJ_EXPORT_SYMBOL(pj_str)
PJ_EXPORT_SYMBOL(pj_strassign)
PJ_EXPORT_SYMBOL(pj_strcpy)
PJ_EXPORT_SYMBOL(pj_strcpy2)
PJ_EXPORT_SYMBOL(pj_strdup)
PJ_EXPORT_SYMBOL(pj_strdup_with_null)
PJ_EXPORT_SYMBOL(pj_strdup2)
PJ_EXPORT_SYMBOL(pj_strdup3)
PJ_EXPORT_SYMBOL(pj_strcmp)
PJ_EXPORT_SYMBOL(pj_strcmp2)
PJ_EXPORT_SYMBOL(pj_strncmp)
PJ_EXPORT_SYMBOL(pj_strncmp2)
PJ_EXPORT_SYMBOL(pj_stricmp)
PJ_EXPORT_SYMBOL(pj_stricmp2)
PJ_EXPORT_SYMBOL(pj_strnicmp)
PJ_EXPORT_SYMBOL(pj_strnicmp2)
PJ_EXPORT_SYMBOL(pj_strcat)
PJ_EXPORT_SYMBOL(pj_strltrim)
PJ_EXPORT_SYMBOL(pj_strrtrim)
PJ_EXPORT_SYMBOL(pj_strtrim)
PJ_EXPORT_SYMBOL(pj_create_random_string)
PJ_EXPORT_SYMBOL(pj_strtoul)
PJ_EXPORT_SYMBOL(pj_utoa)
PJ_EXPORT_SYMBOL(pj_utoa_pad)

/*
 * timer.h
 */
PJ_EXPORT_SYMBOL(pj_timer_heap_mem_size)
PJ_EXPORT_SYMBOL(pj_timer_heap_create)
PJ_EXPORT_SYMBOL(pj_timer_entry_init)
PJ_EXPORT_SYMBOL(pj_timer_heap_schedule)
PJ_EXPORT_SYMBOL(pj_timer_heap_cancel)
PJ_EXPORT_SYMBOL(pj_timer_heap_count)
PJ_EXPORT_SYMBOL(pj_timer_heap_earliest_time)
PJ_EXPORT_SYMBOL(pj_timer_heap_poll)

/*
 * types.h
 */
PJ_EXPORT_SYMBOL(pj_time_val_normalize)

