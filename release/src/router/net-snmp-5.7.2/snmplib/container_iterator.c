/*
 * $Id$
 *
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

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
#include <net-snmp/library/tools.h>
#include <net-snmp/library/snmp_assert.h>

#include <net-snmp/library/container_iterator.h>

netsnmp_feature_child_of(container_iterator, container_types)

#ifndef NETSNMP_FEATURE_REMOVE_CONTAINER_ITERATOR
/**
 *  Holds iterator information containing functions which should be called
 *  by the iterator_handler to loop over your data set and sort it in a
 *  SNMP specific manner.
 *
 *  The iterator_info typedef can be used instead of directly calling this
 *  struct if you would prefer.
 */
typedef struct iterator_info_s {
   /*
    * netsnmp_conatiner  must be first
    */
   netsnmp_container c;

   /*
    * iterator data
    */
   Netsnmp_Iterator_Loop_Key *get_first;
   Netsnmp_Iterator_Loop_Key *get_next;
   
   Netsnmp_Iterator_Loop_Data *get_data;

   Netsnmp_Iterator_Data *free_user_ctx;
   
   Netsnmp_Iterator_Ctx *init_loop_ctx;
   Netsnmp_Iterator_Ctx *cleanup_loop_ctx;
   Netsnmp_Iterator_Ctx_Dup *save_pos;
   
   Netsnmp_Iterator_Data * release_data;
   Netsnmp_Iterator_Data * insert_data;
   Netsnmp_Iterator_Data * remove_data;

   Netsnmp_Iterator_Op * get_size;
   
   int             sorted;
   
   /** This can be used by client handlers to store any
       information they need */
   void           *user_ctx;
} iterator_info;

/**********************************************************************
 *
 * iterator
 *
 **********************************************************************/
static void *
_iterator_get(iterator_info *ii, const void *key)
{
    int cmp, rc = SNMP_ERR_NOERROR;
    netsnmp_ref_void best = { NULL };
    netsnmp_ref_void tmp = { NULL };
    netsnmp_ref_void loop_ctx = { NULL };

    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_get"));
    
    if(ii->init_loop_ctx)
        ii->init_loop_ctx(ii->user_ctx, &loop_ctx);
    
    rc = ii->get_first(ii->user_ctx, &loop_ctx, &tmp);
    if(SNMP_ERR_NOERROR != rc) {
        if(SNMP_ENDOFMIBVIEW != rc)
            snmp_log(LOG_ERR, "bad rc %d from get_next\n", rc);
    }
    else {
        for( ;
             (NULL != tmp.val) && (SNMP_ERR_NOERROR == rc);
             rc = ii->get_next(ii->user_ctx, &loop_ctx, &tmp) ) {
            
            /*
             * if keys are equal, we are done.
             */
            cmp = ii->c.compare(tmp.val, key);
            if(0 == cmp) {
                best.val = tmp.val;
                if(ii->get_data)
                    ii->get_data(ii->user_ctx, &loop_ctx, &best);
            }
            
            /*
             * if data is sorted and if key is greater,
             * we are done (not found)
             */
            if((cmp > 0) && ii->sorted)
                break;
        } /* end for */
    }
    
    if(ii->cleanup_loop_ctx)
        ii->cleanup_loop_ctx(ii->user_ctx,&loop_ctx);

    return best.val;
}

/**
 *
 * NOTE: the returned data context can be reused, to save from
 *   having to allocate memory repeatedly. However, in this case,
 *   the get_data and get_pos functions must be implemented to
 *   return unique memory that will be saved for later comparisons.
 */
static void *
_iterator_get_next(iterator_info *ii, const void *key)
{
    int cmp, rc = SNMP_ERR_NOERROR;
    netsnmp_ref_void best_val = { NULL };
    netsnmp_ref_void best_ctx = { NULL };
    netsnmp_ref_void tmp = { NULL };
    netsnmp_ref_void loop_ctx = { NULL };

    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_get_next"));
    
    /*
     * initialize loop context
     */
    if(ii->init_loop_ctx)
        ii->init_loop_ctx(ii->user_ctx, &loop_ctx);
    
    /*
     * get first item
     */
    rc = ii->get_first(ii->user_ctx, &loop_ctx, &tmp);
    if(SNMP_ERR_NOERROR == rc) {
        /*
         * special case: if key is null, find the first item.
         * this is each if the container is sorted, since we're
         * already done!  Otherwise, get the next item for the
         * first comparison in the loop below.
         */
        if (NULL == key) {
            if(ii->get_data)
                ii->save_pos(ii->user_ctx, &loop_ctx, &best_ctx, 1);
            best_val.val = tmp.val;
            if(ii->sorted)
                tmp.val = NULL; /* so we skip for loop */
            else
                rc = ii->get_next(ii->user_ctx, &loop_ctx, &tmp);
        }
        /*
         * loop over remaining items
         */
        for( ;
             (NULL != tmp.val) && (rc == SNMP_ERR_NOERROR);
             rc = ii->get_next(ii->user_ctx, &loop_ctx, &tmp) ) {
            
            /*
             * if we have a key, this is a get-next, and we need to compare
             * the key to the tmp value to see if the tmp value is greater
             * than the key, but less than any previous match.
             *
             * if there is no key, this is a get-first, and we need to
             * compare the best value agains the tmp value to see if the
             * tmp value is lesser than the best match.
             */
            if(key) /* get next */
                cmp = ii->c.compare(tmp.val, key);
            else { /* get first */
                /*
                 * best value and tmp value should never be equal,
                 * otherwise we'd be comparing a pointer to itself.
                 * (see note on context reuse in comments above function.
                 */
                if(best_val.val == tmp.val) {
                    snmp_log(LOG_ERR,"illegal reuse of data context in "
                             "container_iterator\n");
                    rc = SNMP_ERR_GENERR;
                    break;
                }
                cmp = ii->c.compare(best_val.val, tmp.val);
            }
            if(cmp > 0) {
                /*
                 * if we don't have a key (get-first) or a current best match,
                 * then the comparison above is all we need to know that
                 * tmp is the best match. otherwise, compare against the
                 * current best match.
                 */
                if((NULL == key) || (NULL == best_val.val) ||
                   ((cmp=ii->c.compare(tmp.val, best_val.val)) < 0) ) {
                    DEBUGMSGT(("container_iterator:results"," best match\n"));
                    best_val.val = tmp.val;
                    if(ii->get_data)
                        ii->save_pos(ii->user_ctx, &loop_ctx, &best_ctx, 1);
                }
            }
            else if((cmp == 0) && ii->sorted && key) {
                /*
                 * if keys are equal and container is sorted, then we know
                 * the next key will be the one we want.
                 * NOTE: if no vars, treat as generr, since we
                 *    went past the end of the container when we know
                 *    the next item is the one we want. (IGN-A)
                 */
                rc = ii->get_next(ii->user_ctx, &loop_ctx, &tmp);
                if(SNMP_ERR_NOERROR == rc) {
                    best_val.val = tmp.val;
                    if(ii->get_data)
                        ii->save_pos(ii->user_ctx, &loop_ctx, &best_ctx, 1);
                }
                else if(SNMP_ENDOFMIBVIEW == rc)
                    rc = SNMPERR_GENERR; /* not found */
                break;
            }
            
        } /* end for */
    }

    /*
     * no vars is ok, except as noted above (IGN-A)
     */
    if(SNMP_ENDOFMIBVIEW == rc)
        rc = SNMP_ERR_NOERROR;
            
    /*
     * get data, iff necessary
     * clear return value iff errors
     */
    if(SNMP_ERR_NOERROR == rc) {
        if(ii->get_data && best_val.val) {
            rc = ii->get_data(ii->user_ctx, &best_ctx, &best_val);
            if(SNMP_ERR_NOERROR != rc) {
                snmp_log(LOG_ERR, "bad rc %d from get_data\n", rc);
                best_val.val = NULL;
            }
        }
    }
    else if(SNMP_ENDOFMIBVIEW != rc) {
        snmp_log(LOG_ERR, "bad rc %d from get_next\n", rc);
        best_val.val = NULL;
    }

    /*
     * if we have a saved loop ctx, clean it up
     */
    if((best_ctx.val != NULL) && (best_ctx.val != loop_ctx.val) &&
       (ii->cleanup_loop_ctx))
        ii->cleanup_loop_ctx(ii->user_ctx,&best_ctx);

    /*
     * clean up loop ctx
     */
    if(ii->cleanup_loop_ctx)
        ii->cleanup_loop_ctx(ii->user_ctx,&loop_ctx);

    DEBUGMSGT(("container_iterator:results"," returning %p\n", best_val.val));
    return best_val.val;
}

/**********************************************************************
 *
 * container
 *
 **********************************************************************/
static void
_iterator_free(iterator_info *ii)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_free"));
    
    if(NULL == ii)
        return;
    
    if(ii->user_ctx)
        ii->free_user_ctx(ii->user_ctx,ii->user_ctx);
    
    free(ii);
}

static void *
_iterator_find(iterator_info *ii, const void *data)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_find"));
    
    if((NULL == ii) || (NULL == data))
        return NULL;

    return _iterator_get(ii, data);
}

static void *
_iterator_find_next(iterator_info *ii, const void *data)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_find_next"));
    
    if(NULL == ii)
        return NULL;

    return _iterator_get_next(ii, data);
}

static int
_iterator_insert(iterator_info *ii, const void *data)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_insert"));
    
    if(NULL == ii)
        return -1;

    if(NULL == ii->insert_data)
        return -1;

    return ii->insert_data(ii->user_ctx, data);
}

static int
_iterator_remove(iterator_info *ii, const void *data)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_remove"));
    
    if(NULL == ii)
        return -1;

    if(NULL == ii->remove_data)
        return -1;

    return ii->remove_data(ii->user_ctx, data);
}

static int
_iterator_release(iterator_info *ii, const void *data)
{
    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_release"));
    
    if(NULL == ii)
        return -1;

    if(NULL == ii->release_data)
        return -1;

    return ii->release_data(ii->user_ctx, data);
}

static size_t
_iterator_size(iterator_info *ii)
{
    size_t count = 0;
    netsnmp_ref_void loop_ctx = { NULL };
    netsnmp_ref_void tmp = { NULL };

    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_size"));
    
    if(NULL == ii)
        return -1;

    if(NULL != ii->get_size)
        return ii->get_size(ii->user_ctx);

    /*
     * no get_size. loop and count ourselves
     */
    if(ii->init_loop_ctx)
        ii->init_loop_ctx(ii->user_ctx, &loop_ctx);
    
    for( ii->get_first(ii->user_ctx, &loop_ctx, &tmp);
         NULL != tmp.val;
         ii->get_next(ii->user_ctx, &loop_ctx, &tmp) )
        ++count;

    if(ii->cleanup_loop_ctx)
        ii->cleanup_loop_ctx(ii->user_ctx,&loop_ctx);

    return count;
}

static void
_iterator_for_each(iterator_info *ii, netsnmp_container_obj_func *f,
                   void *ctx)
{
    netsnmp_ref_void loop_ctx = { NULL };
    netsnmp_ref_void tmp = { NULL };

    DEBUGMSGT(("container_iterator",">%s\n", "_iterator_foreach"));
    
    if(NULL == ii)
        return;

    if(ii->init_loop_ctx)
        ii->init_loop_ctx(ii->user_ctx, &loop_ctx);
    
    for( ii->get_first(ii->user_ctx, &loop_ctx, &tmp);
         NULL != tmp.val;
         ii->get_next(ii->user_ctx, &loop_ctx, &tmp) )
        (*f) (tmp.val, ctx);

    if(ii->cleanup_loop_ctx)
        ii->cleanup_loop_ctx(ii->user_ctx,&loop_ctx);
}

static void
_iterator_clear(netsnmp_container *container, netsnmp_container_obj_func *f,
                 void *context)
{
    snmp_log(LOG_WARNING,"clear is meaningless for iterator container.\n");
}

/**********************************************************************
 *
 */
netsnmp_container*
netsnmp_container_iterator_get(void *iterator_user_ctx,
                               netsnmp_container_compare * compare,
                               Netsnmp_Iterator_Loop_Key * get_first,
                               Netsnmp_Iterator_Loop_Key * get_next,
                               Netsnmp_Iterator_Loop_Data * get_data,
                               Netsnmp_Iterator_Ctx_Dup * save_pos,
                               Netsnmp_Iterator_Ctx * init_loop_ctx,
                               Netsnmp_Iterator_Ctx * cleanup_loop_ctx,
                               Netsnmp_Iterator_Data * free_user_ctx,
                               int sorted)
{
    iterator_info *ii;

    /*
     * sanity checks
     */
    if(get_data && ! save_pos) {
        snmp_log(LOG_ERR, "save_pos required with get_data\n");
        return NULL;
    }

    /*
     * allocate memory
     */
    ii = SNMP_MALLOC_TYPEDEF(iterator_info);
    if (NULL==ii) {
        snmp_log(LOG_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    /*
     * init container structure with iterator functions
     */
    ii->c.cfree = (netsnmp_container_rc*)_iterator_free;
    ii->c.compare = compare;
    ii->c.get_size = (netsnmp_container_size*)_iterator_size;
    ii->c.init = NULL;
    ii->c.insert = (netsnmp_container_op*)_iterator_insert;
    ii->c.remove = (netsnmp_container_op*)_iterator_remove;
    ii->c.release = (netsnmp_container_op*)_iterator_release;
    ii->c.find = (netsnmp_container_rtn*)_iterator_find;
    ii->c.find_next = (netsnmp_container_rtn*)_iterator_find_next;
    ii->c.get_subset = NULL;
    ii->c.get_iterator = NULL;
    ii->c.for_each = (netsnmp_container_func*)_iterator_for_each;
    ii->c.clear = _iterator_clear;

    /*
     * init iterator structure with user functions
     */
    ii->get_first = get_first;
    ii->get_next = get_next;
    ii->get_data = get_data;
    ii->save_pos = save_pos;
    ii->init_loop_ctx = init_loop_ctx;
    ii->cleanup_loop_ctx = cleanup_loop_ctx;
    ii->free_user_ctx = free_user_ctx;
    ii->sorted = sorted;

    ii->user_ctx = iterator_user_ctx;

    return (netsnmp_container*)ii;
}

void
netsnmp_container_iterator_set_data_cb(netsnmp_container *c,
                                       Netsnmp_Iterator_Data * insert_data,
                                       Netsnmp_Iterator_Data * remove_data,
                                       Netsnmp_Iterator_Op * get_size)
{
    iterator_info *ii = (iterator_info *)c;
    if(NULL == ii)
        return;
    
    ii->insert_data = insert_data;
    ii->remove_data = remove_data;
    ii->get_size = get_size;
}
#else  /* NETSNMP_FEATURE_REMOVE_CONTAINER_ITERATOR */
netsnmp_feature_unused(container_iterator);
#endif /* NETSNMP_FEATURE_REMOVE_CONTAINER_ITERATOR */
