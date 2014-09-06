#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/hardware/memory.h>

#include <net-snmp/net-snmp-features.h>

netsnmp_feature_child_of(hardware_memory, netsnmp_unused)

netsnmp_feature_child_of(memory_get_cache, hardware_memory)

extern NetsnmpCacheLoad netsnmp_mem_arch_load;

netsnmp_memory_info *_mem_head  = NULL;
netsnmp_cache       *_mem_cache = NULL;

void init_hw_mem( void ) {
    oid nsMemory[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 31 };
    _mem_cache = netsnmp_cache_create( 5, netsnmp_mem_arch_load, NULL,
                                          nsMemory, OID_LENGTH(nsMemory));
}



netsnmp_memory_info *netsnmp_memory_get_first( int type ) {
    netsnmp_memory_info *mem;

    for ( mem=_mem_head; mem; mem=mem->next )
        if (mem->type == type)    /* Or treat as bits? */
            return mem;
    return NULL;
}

netsnmp_memory_info *netsnmp_memory_get_next( netsnmp_memory_info *this_ptr, int type ) {
    netsnmp_memory_info *mem;

    if (this_ptr)
        for ( mem=this_ptr->next; mem; mem=mem->next )
            if (mem->type == type)    /* Or treat as bits? */
                return mem;
    return NULL;
}

    /*
     * Work with a list of Memory entries, indexed numerically
     */
netsnmp_memory_info *netsnmp_memory_get_byIdx(  int idx, int create ) {
    netsnmp_memory_info *mem, *mem2;

        /*
         * Find the specified Memory entry
         */
    for ( mem=_mem_head; mem; mem=mem->next ) {
        if ( mem->idx == idx )
            return mem;
    }
    if (!create)
        return NULL;

        /*
         * Create a new memory entry, and insert it into the list....
         */
    mem = SNMP_MALLOC_TYPEDEF( netsnmp_memory_info );
    if (!mem)
        return NULL;
    mem->idx = idx;
        /* ... either as the first (or only) entry....  */
    if ( !_mem_head || _mem_head->idx > idx ) {
        mem->next = _mem_head;
        _mem_head = mem;
        return mem;
    }
        /* ... or in the appropriate position  */
    for ( mem2=_mem_head; mem2; mem2=mem2->next ) {
        if ( !mem2->next || mem2->next->idx > idx ) {
            mem->next  = mem2->next;
            mem2->next = mem;
            return mem;
        }
    }
    SNMP_FREE(mem);
    return NULL;  /* Shouldn't happen! */
}

netsnmp_memory_info *netsnmp_memory_get_next_byIdx( int idx, int type ) {
    netsnmp_memory_info *mem;

    for ( mem=_mem_head; mem; mem=mem->next )
        if (mem->type == type && mem->idx > idx)    /* Or treat as bits? */
            return mem;
    return NULL;
}



#ifndef NETSNMP_FEATURE_REMOVE_MEMORY_GET_CACHE
netsnmp_cache *netsnmp_memory_get_cache( void ) {
    return _mem_cache;
}
#endif /* NETSNMP_FEATURE_REMOVE_MEMORY_GET_CACHE */

int netsnmp_memory_load( void ) {
     return netsnmp_cache_check_and_reload( _mem_cache );
}
