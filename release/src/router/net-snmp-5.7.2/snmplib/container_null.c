/*
 * container_null.c
 * $Id$
 *
 * see comments in header file.
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#if HAVE_IO_H
#include <io.h>
#endif
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <sys/types.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/types.h>
#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/container.h>
#include <net-snmp/library/container_null.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/snmp_assert.h>

netsnmp_feature_child_of(container_null, container_types)

/** @defgroup null_container null_container
 *  Helps you implement specialized containers.
 *  @ingroup container
 *
 *  This is a simple container that doesn't actually contain anything.
 *  All the methods simply log a debug message and return.
 *  
 *  The original intent for this container is as a wrapper for a specialized
 *  container. Implement the functions you need, create a null_container,
 *  and override the default functions with your specialized versions.
 *
 *  You can use the 'container:null' debug token to see what functions are
 *  being called, to help determine if you need to implement them.
 *
 *  @{
 */

#ifndef NETSNMP_FEATURE_REMOVE_CONTAINER_NULL
/**********************************************************************
 *
 * container
 *
 */
static void *
_null_find(netsnmp_container *container, const void *data)
{
    DEBUGMSGTL(("container:null:find","in\n"));
    return NULL;
}

static void *
_null_find_next(netsnmp_container *container, const void *data)
{
    DEBUGMSGTL(("container:null:find_next","in\n"));
    return NULL;
}

static int
_null_insert(netsnmp_container *container, const void *data)
{
    DEBUGMSGTL(("container:null:insert","in\n"));
    return 0;
}

static int
_null_remove(netsnmp_container *container, const void *data)
{
    DEBUGMSGTL(("container:null:remove","in\n"));
    return 0;
}

static int
_null_free(netsnmp_container *container)
{
    DEBUGMSGTL(("container:null:free","in\n"));
    free(container);
    return 0;
}

static int
_null_init(netsnmp_container *container)
{
    DEBUGMSGTL(("container:null:","in\n"));
    return 0;
}

static size_t
_null_size(netsnmp_container *container)
{
    DEBUGMSGTL(("container:null:size","in\n"));
    return 0;
}

static void
_null_for_each(netsnmp_container *container, netsnmp_container_obj_func *f,
             void *context)
{
    DEBUGMSGTL(("container:null:for_each","in\n"));
}

static netsnmp_void_array *
_null_get_subset(netsnmp_container *container, void *data)
{
    DEBUGMSGTL(("container:null:get_subset","in\n"));
    return NULL;
}

static void
_null_clear(netsnmp_container *container, netsnmp_container_obj_func *f,
                 void *context)
{
    DEBUGMSGTL(("container:null:clear","in\n"));
}

/**********************************************************************
 *
 * factory
 *
 */

netsnmp_container *
netsnmp_container_get_null(void)
{
    /*
     * allocate memory
     */
    netsnmp_container *c;
    DEBUGMSGTL(("container:null:get_null","in\n"));
    c = SNMP_MALLOC_TYPEDEF(netsnmp_container);
    if (NULL==c) {
        snmp_log(LOG_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    c->container_data = NULL;
        
    c->get_size = _null_size;
    c->init = _null_init;
    c->cfree = _null_free;
    c->insert = _null_insert;
    c->remove = _null_remove;
    c->find = _null_find;
    c->find_next = _null_find_next;
    c->get_subset = _null_get_subset;
    c->get_iterator = NULL; /* _null_iterator; */
    c->for_each = _null_for_each;
    c->clear = _null_clear;
       
    return c;
}

netsnmp_factory *
netsnmp_container_get_null_factory(void)
{
    static netsnmp_factory f = { "null",
                                 (netsnmp_factory_produce_f*)
                                 netsnmp_container_get_null};
    
    DEBUGMSGTL(("container:null:get_null_factory","in\n"));
    return &f;
}

void
netsnmp_container_null_init(void)
{
    netsnmp_container_register("null",
                               netsnmp_container_get_null_factory());
}
#else  /* NETSNMP_FEATURE_REMOVE_CONTAINER_NULL */
netsnmp_feature_unused(container_null);
#endif /* NETSNMP_FEATURE_REMOVE_CONTAINER_NULL */
/**  @} */

