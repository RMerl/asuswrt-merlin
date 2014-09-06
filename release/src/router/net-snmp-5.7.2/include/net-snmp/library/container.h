/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_CONTAINER_H
#define NETSNMP_CONTAINER_H

/*
 * $Id$
 *
 * WARNING: This is a recently created file, and all of it's contents are
 *          subject to change at any time.
 *
 * A basic container template. A generic way for code to store and
 * retrieve data. Allows for interchangable storage algorithms.
 */
#ifndef NET_SNMP_CONFIG_H
#error "Please include <net-snmp/net-snmp-config.h> before this file"
#endif

#include <stdlib.h> /* free() */
#include <net-snmp/types.h>
#include <net-snmp/library/factory.h>
#include <net-snmp/library/snmp_logging.h>
#include <net-snmp/library/tools.h>

#ifdef  __cplusplus
extern "C" {
#endif

    /*************************************************************************
     *
     * function pointer definitions
     *
     *************************************************************************/
    struct netsnmp_iterator_s; /** forward declare */
    struct netsnmp_container_s; /** forward declare */

    /*
     * function for performing an operation on a container which
     * returns (maybe the same) container.
     */
    typedef struct netsnmp_container_s* (netsnmp_container_mod_op)
        (struct netsnmp_container_s *, void *context, u_int flags);

    /*
     * function for setting an option on a container
     */
    typedef int (netsnmp_container_option)(struct netsnmp_container_s *,
                                           int set, u_int flags);

    /*
     * function returning an int for an operation on a container
     */
    typedef int (netsnmp_container_rc)(struct netsnmp_container_s *);

    /*
     * function returning an iterator for a container
     */
    typedef struct netsnmp_iterator_s * (netsnmp_container_it)
        (struct netsnmp_container_s *);

    /*
     * function returning a size_t for an operation on a container
     */
    typedef size_t (netsnmp_container_size)(struct netsnmp_container_s *);

    /*
     * function returning an int for an operation on an object and
     * a container
     */
    typedef int (netsnmp_container_op)(struct netsnmp_container_s *,
                                       const void *data);

    /*
     * function returning an oject for an operation on an object and a
     * container
     */
    typedef void * (netsnmp_container_rtn)(struct netsnmp_container_s *,
                                           const void *data);

    /*
     * function with no return which acts on an object
     */
    typedef void (netsnmp_container_obj_func)(void *data, void *context);

    /*
     * function with no return which calls a function on an object
     */
    typedef void (netsnmp_container_func)(struct netsnmp_container_s *,
                                          netsnmp_container_obj_func *,
                                          void *context);

    /*
     * function returning an array of objects for an operation on an
     * ojbect and a container
     */
    typedef netsnmp_void_array * (netsnmp_container_set)
        (struct netsnmp_container_s *, void *data);

    /*
     * function returning an int for a comparison between two objects
     */
    typedef int (netsnmp_container_compare)(const void *lhs,
                                            const void *rhs);

    /*************************************************************************
     *
     * Basic container
     *
     *************************************************************************/
    typedef struct netsnmp_container_s {
       
       /*
        * pointer for container implementation
        */
       void *         container_data;

       /*
        * returns the number of items in a container
        */
       netsnmp_container_size  *get_size;
       
       /*
        * initialize a container
        */
       netsnmp_container_rc    *init;

       /*
        * release memory used by a container.
        *
        * Note: if your data structures contained allocated
        * memory, you are responsible for releasing that
        * memory before calling this function!
        */
       netsnmp_container_rc    *cfree;

       /*
        * add an entry to the container
        */
       netsnmp_container_op    *insert;

       /*
        * remove an entry from the container
        */
       netsnmp_container_op    *remove;

       /*
        * release memory for an entry from the container
        */
       netsnmp_container_op    *release; /* NOTE: deprecated. Use free_item */
       netsnmp_container_obj_func *free_item;

       /*
        * find the entry in the container with the same key
        *
        * Note: do not change the key!  If you need to
        * change a key, remove the entry, change the key,
        * and the re-add the entry.
        */
       netsnmp_container_rtn   *find;

       /*
        * find the entry in the container with the next highest key
        *
        * If the key is NULL, return the first item in the container.
        */
       netsnmp_container_rtn   *find_next;

       /*
        * find all entries in the container which match the partial key
        * returns allocated memory (netsnmp_void_array). User is responsible
        * for releasing this memory (free(array->array), free(array)).
        * DO NOT FREE ELEMENTS OF THE ARRAY, because they are the same pointers
        * stored in the container.
        */
       netsnmp_container_set            *get_subset;

       /*
        * function to return an iterator for the container
        */
       netsnmp_container_it           *get_iterator;

       /*
        * function to call another function for each object in the container
        */
       netsnmp_container_func         *for_each;

       /*
        * specialized version of for_each used to optimize cleanup.
        * clear the container, optionally calling a function for each item.
        */
       netsnmp_container_func         *clear;

       /*
        * OPTIONAL function to filter inserts to the container
        *  (intended for a secondary container, which only wants
        *   a sub-set of the objects in the primary/parent container)
        * Returns:
        *   1 : filter matched (don't insert)
        *   0 : no match (insert)
        */
       netsnmp_container_op    *insert_filter;

        /*
         * OPTIONAL function to duplicate a container. Defaults to a shallow
         * copy. Only the specified container is copied (i.e. sub-containers
         * not included).
         */
        netsnmp_container_mod_op *duplicate;


       /*
        * function to compare two object stored in the container.
        *
        * Returns:
        *
        *   -1  LHS < RHS
        *    0  LHS = RHS
        *    1  LHS > RHS
        */
       netsnmp_container_compare        *compare;

       /*
        * same as compare, but RHS will be a partial key
        */
       netsnmp_container_compare        *ncompare;

       /*
        * function to set container options
        */
       netsnmp_container_option         *options;

       /*
        * unique name for finding a particular container in a list
        */
       char *container_name;

       /*
        * sort count, for iterators to track (insert/delete
        * bumps counter, invalidates iterator)
        */
       u_long                          sync;

       /*
        * flags
        */
       u_int                           flags;

       /*
        * containers can contain other containers (additional indexes)
        */
       struct netsnmp_container_s *next, *prev;

    } netsnmp_container;

    /*
     * initialize/free a container of container factories. used by
     * netsnmp_container_find* functions.
     */
    NETSNMP_IMPORT
    void netsnmp_container_init_list(void);
    NETSNMP_IMPORT
    void netsnmp_container_free_list(void);

    /*
     * register a new container factory
     */
    int netsnmp_container_register_with_compare(const char* name,
                                                netsnmp_factory *f,
                                                netsnmp_container_compare *c);
    int netsnmp_container_register(const char* name, netsnmp_factory *f);

    /*
     * search for and create a container from a list of types or a
     * specific type.
     */
    NETSNMP_IMPORT
    netsnmp_container * netsnmp_container_find(const char *type_list);
    netsnmp_container * netsnmp_container_get(const char *type);

    /*
     * utility routines
     */
    NETSNMP_IMPORT
    void netsnmp_container_add_index(netsnmp_container *primary,
                                     netsnmp_container *new_index);


    netsnmp_factory *netsnmp_container_get_factory(const char *type);

    /*
     * common comparison routines
     */
    /** first data element is a 'netsnmp_index' */
    NETSNMP_IMPORT
    int netsnmp_compare_netsnmp_index(const void *lhs, const void *rhs);
    NETSNMP_IMPORT
    int netsnmp_ncompare_netsnmp_index(const void *lhs, const void *rhs);

    /** first data element is a 'char *' */
    int netsnmp_compare_cstring(const void * lhs, const void * rhs);
    int netsnmp_ncompare_cstring(const void * lhs, const void * rhs);

    /** useful for octet strings */
    int netsnmp_compare_mem(const char * lhs, size_t lhs_len,
                            const char * rhs, size_t rhs_len);

    /** no structure, just 'char *' pointers */
    int netsnmp_compare_direct_cstring(const void * lhs, const void * rhs);

    int netsnmp_compare_long(const void * lhs, const void * rhs);
    int netsnmp_compare_ulong(const void * lhs, const void * rhs);
    int netsnmp_compare_int32(const void * lhs, const void * rhs);
    int netsnmp_compare_uint32(const void * lhs, const void * rhs);

    /** for_each callback to call free on data item */
    NETSNMP_IMPORT
    void  netsnmp_container_simple_free(void *data, void *context);

/*
 * container optionflags
 */
#define CONTAINER_KEY_ALLOW_DUPLICATES             0x00000001
#define CONTAINER_KEY_UNSORTED                     0x00000002

#define CONTAINER_SET_OPTIONS(x,o,rc)  do {                             \
        if (NULL==(x)->options)                                         \
            rc = -1;                                                    \
        else {                                                          \
            rc = (x)->options(x, 1, o);                                 \
            if (rc != -1 )                                              \
                (x)->flags |= o;                                        \
        }                                                               \
    } while(0)

#define CONTAINER_CHECK_OPTION(x,o,rc)    do {                          \
        rc = x->flags & 0;                                              \
    } while(0)


    /*
     * useful macros (x = container; k = key; c = user context)
     */
#define CONTAINER_FIRST(x)          (x)->find_next(x,NULL)
#define CONTAINER_FIND(x,k)         (x)->find(x,k)
#define CONTAINER_NEXT(x,k)         (x)->find_next(x,k)
/*
 * GET_SUBSET returns allocated memory (netsnmp_void_array). User is responsible
 * for releasing this memory (free(array->array), free(array)).
 * DO NOT FREE ELEMENTS OF THE ARRAY, because they are the same pointers
 * stored in the container.
 */
#define CONTAINER_GET_SUBSET(x,k)   (x)->get_subset(x,k)
#define CONTAINER_SIZE(x)           (x)->get_size(x)
#define CONTAINER_ITERATOR(x)       (x)->get_iterator(x)
#define CONTAINER_COMPARE(x,l,r)    (x)->compare(l,r)
#define CONTAINER_FOR_EACH(x,f,c)   (x)->for_each(x,f,c)

    /*
     * if you are getting multiple definitions of these three
     * inline functions, you most likely have optimizations turned off.
     * Either turn them back on, or define NETSNMP_NO_INLINE
     */
    /*
     * insert k into all containers
     */
    NETSNMP_IMPORT
    int CONTAINER_INSERT(netsnmp_container *x, const void *k);

    /*
     * remove k from all containers
     */
    NETSNMP_IMPORT
    int CONTAINER_REMOVE(netsnmp_container *x, const void *k);

    /*
     * duplicate container
     */
    NETSNMP_IMPORT
    netsnmp_container *CONTAINER_DUP(netsnmp_container *x, void *ctx,
                                     u_int flags);

    /*
     * clear all containers. When clearing the *first* container, and
     * *only* the first container, call the function f for each item.
     * After calling this function, all containers should be empty.
     */
    NETSNMP_IMPORT
    void CONTAINER_CLEAR(netsnmp_container *x, netsnmp_container_obj_func *f,
                        void *c);

    /*
     * clear all containers. When clearing the *first* container, and
     * *only* the first container, call the free_item function for each item.
     * After calling this function, all containers should be empty.
     */
    NETSNMP_IMPORT
    void CONTAINER_FREE_ALL(netsnmp_container *x, void *c);

    /*
     * free all containers
     */
    NETSNMP_IMPORT
    int CONTAINER_FREE(netsnmp_container *x);

    NETSNMP_IMPORT
    netsnmp_container *SUBCONTAINER_FIND(netsnmp_container *x,
                                         const char* name);

    /*
     * INTERNAL utility routines for container implementations
     */
    void netsnmp_init_container(netsnmp_container         *c,
                                netsnmp_container_rc      *init,
                                netsnmp_container_rc      *cfree,
                                netsnmp_container_size    *size,
                                netsnmp_container_compare *cmp,
                                netsnmp_container_op      *ins,
                                netsnmp_container_op      *rem,
                                netsnmp_container_rtn     *fnd);
    /** Duplicate container meta-data. */
    int netsnmp_container_data_dup(netsnmp_container *dup,
                                   netsnmp_container *c);

    
    /*************************************************************************
     *
     * container iterator
     *
     *************************************************************************/
    /*
     * function returning an int for an operation on an iterator
     */
    typedef int (netsnmp_iterator_rc)(struct netsnmp_iterator_s *);

    /*
     * function returning an oject for an operation on an iterator
     */
    typedef void * (netsnmp_iterator_rtn)(struct netsnmp_iterator_s *);


    /*
     * iterator structure
     */
    typedef struct netsnmp_iterator_s {

       netsnmp_container              *container;

        /*
         * sync from container when iterator created. used to invalidate
         * the iterator when the container changes.
         */
       u_long                          sync;

        /*
         * reset iterator position to beginning of container.
         */
       netsnmp_iterator_rc           *reset;

        /*
         * release iterator and memory it uses
         */
       netsnmp_iterator_rc           *release;

        /*
         * first, last and current DO NOT advance the iterator
         */
       netsnmp_iterator_rtn          *first;
       netsnmp_iterator_rtn          *curr;
       netsnmp_iterator_rtn          *last;

       netsnmp_iterator_rtn          *next;

        /*
         * remove will remove the item at the current position, then back up
         * the iterator to the previous item. That way next will move to the
         * item (the one that replaced the removed item.
         */
       netsnmp_iterator_rc           *remove;

    } netsnmp_iterator;


#define ITERATOR_FIRST(x)  x->first(x)
#define ITERATOR_NEXT(x)   x->next(x)
#define ITERATOR_LAST(x)   x->last(x)
#define ITERATOR_REMOVE(x) x->remove(x)
#define ITERATOR_RELEASE(x) do { x->release(x); x = NULL; } while(0)
    
#ifdef  __cplusplus
}
#endif

#endif /** NETSNMP_CONTAINER_H */
