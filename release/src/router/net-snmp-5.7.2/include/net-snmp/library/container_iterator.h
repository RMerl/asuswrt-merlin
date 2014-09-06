/**
 * @file container_iterator.h
 * @addtogroup container_iterator
 * @{
 */
#ifndef _CONTAINER_ITERATOR_HANDLER_H_
#define _CONTAINER_ITERATOR_HANDLER_H_

#include "container.h"

#ifdef __cplusplus
extern          "C" {
#endif

    typedef int (Netsnmp_Iterator_Loop_Key) (void *iterator_ctx,
                                             netsnmp_ref_void* loop_ctx,
                                             netsnmp_ref_void* key);
    typedef int (Netsnmp_Iterator_Loop_Data)(void *iterator_ctx,
                                             netsnmp_ref_void* loop_ctx,
                                             netsnmp_ref_void* data);
    typedef int (Netsnmp_Iterator_Ctx) (void *iterator_ctx,
                                         netsnmp_ref_void* loop_ctx);
    typedef int (Netsnmp_Iterator_Ctx_Dup) (void *iterator_ctx,
                                            netsnmp_ref_void* loop_ctx,
                                            netsnmp_ref_void* dup_ctx,
                                            int reuse);
    typedef int (Netsnmp_Iterator_Op) (void *iterator_ctx);
    typedef int (Netsnmp_Iterator_Data) (void *iterator_ctx,
                                            const void *data);    

    void netsnmp_container_iterator_init(void);

    netsnmp_container* netsnmp_container_iterator_get(
        void *iterator_user_ctx,
        netsnmp_container_compare * compare,
        Netsnmp_Iterator_Loop_Key * get_first,
        Netsnmp_Iterator_Loop_Key * get_next,
        Netsnmp_Iterator_Loop_Data * get_data,
        Netsnmp_Iterator_Ctx_Dup * save_pos, /* iff returning static data */
        Netsnmp_Iterator_Ctx * init_loop_ctx,
        Netsnmp_Iterator_Ctx * cleanup_loop_ctx,
        Netsnmp_Iterator_Data * free_user_ctx,
        int sorted);
    
    /*
     * set up optional callbacks/
     * NOTE: even though the first parameter is a generic netsnmp_container,
     *       this function should only be called for a container created
     *       by netsnmp_container_iterator_get.
     */
    void
    netsnmp_container_iterator_set_data_cb(netsnmp_container *c,
                                           Netsnmp_Iterator_Data * insert_data,
                                           Netsnmp_Iterator_Data * remove_data,
                                           Netsnmp_Iterator_Op * get_size);

#ifdef __cplusplus
}
#endif

#endif                          /* _CONTAINER_ITERATOR_HANDLER_H_ */
/** @} */
