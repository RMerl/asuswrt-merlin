/**
 * memory debugging
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "err.h"
#include "util.h"

#ifdef DEBUG_MEM
typedef struct debugnode_t {
    char *file;
    int line;
    int size;
    void *ptr;
    struct debugnode_t *next;
} DEBUGNODE;

DEBUGNODE _debug_memlist = { NULL, 0, 0, NULL, NULL };

void debug_free(char *file, int line, void *ptr);
void *debug_malloc(char *file, int line, size_t size);
void *debug_realloc(char *file, int line, void *ptr, size_t size);
void *debug_calloc(char *file, int line, size_t count, size_t size);
char *debug_strdup(char *file, int line, const char *str);
void debug_dump(void);


void *_debug_alloc_nolock(char *file, int line, size_t size);
DEBUGNODE *_debug_find_ptr(void *ptr);
void _debug_register_ptr(char *file, int line, void *ptr, size_t size);

/**
 * find a ptr in the node list, assuming the list is already
 * locked.
 */
DEBUGNODE *_debug_find_ptr(void *ptr) {
    DEBUGNODE *prev, *current;

    prev = &_debug_memlist;
    current = prev->next;

    while(current) {
        if(current->ptr == ptr)
            return current;
        prev = current;
        current = current->next;
    }

    return NULL;
}

void *_debug_alloc_nolock(char *file, int line, size_t size) {
    void *ptr;

    ptr = malloc(size);
    if(!ptr)
        DPRINTF(E_FATAL,L_MISC,"Malloc failed in _debug_alloc\n");

    _debug_register_ptr(file, line, ptr, size);
    return ptr;
}

void _debug_register_ptr(char *file, int line, void *ptr, size_t size) {
    DEBUGNODE *pnew;

    pnew = (DEBUGNODE *)malloc(sizeof(DEBUGNODE));
    if(!pnew)
        DPRINTF(E_FATAL,L_MISC,"Malloc failed in _debug_alloc\n");

    pnew->file = strdup(file);
    pnew->line = line;
    pnew->size = size;
    pnew->ptr = ptr;

    pnew->next = _debug_memlist.next;
    _debug_memlist.next = pnew;
}

void debug_register(char *file, int line, void *ptr, size_t size) {
    util_mutex_lock(l_memdebug);
    _debug_register_ptr(file, line, ptr, size);
    util_mutex_unlock(l_memdebug);
}

void *_debug_alloc(char *file, int line, int size) {
    void *ptr;

    util_mutex_lock(l_memdebug);
    ptr = _debug_alloc_nolock(file, line, size);
    util_mutex_unlock(l_memdebug);

    return ptr;
}

void debug_dump(void) {
    DEBUGNODE *current;
    uint32_t size = 0;
    int blocks = 0;

    if(!_debug_memlist.next) {
        DPRINTF(E_LOG,L_MISC,"No leaked memory!\n");
        return;
    }

    current = _debug_memlist.next;
    while(current) {
        size += current->size;
        blocks++;
        current = current->next;
    }

    DPRINTF(E_WARN,L_MISC,"Leaked %d bytes in %d blocks\n",size, blocks);
    current = _debug_memlist.next;
    while(current) {
        DPRINTF(E_WARN,L_MISC,"%d bytes: %s, line %d\n", current->size,
                current->file, current->line);
        util_hexdump(current->ptr,current->size);
        current = current->next;
    }

}

void debug_free(char *file, int line, void *ptr) {
    DEBUGNODE *prev, *current;

    util_mutex_lock(l_memdebug);
    prev = &_debug_memlist;
    current = prev->next;

    while(current) {
        if(current->ptr == ptr) {
            /* found it */
            break;
        }
        prev = current;
        current = current->next;
    }

    if(!current) {
        util_mutex_unlock(l_memdebug);
        DPRINTF(E_FATAL,L_MISC,"Attempt to free an unallocated ptr: %s, %d\n",
                file, line);
    }

    prev->next = current->next;
    util_mutex_unlock(l_memdebug);

    if(current) {
        if(current->file)
            free(current->file);
        if(current->ptr)
            free(current->ptr);
        free(current);
    }
    return;
}

void *debug_malloc(char *file, int line, size_t size) {
    void *ptr;

    ptr = _debug_alloc(file,line,size);
    return ptr;
}

void *debug_realloc(char *file, int line, void *ptr, size_t size) {
    void *new_ptr;
    DEBUGNODE *old_node;
    int copy_size;

    util_mutex_lock(l_memdebug);
    old_node = _debug_find_ptr(ptr);
    if(!old_node) {
        util_mutex_unlock(l_memdebug);
        DPRINTF(E_FATAL,L_MISC,"Attempt to realloc invalid ptr: %s, %d\n",
                file, line);
    }

    new_ptr = _debug_alloc_nolock(file, line, size);
    util_mutex_unlock(l_memdebug);

    copy_size = old_node->size;
    if(size < copy_size)
        copy_size=size;

    memcpy(new_ptr,old_node->ptr,copy_size);
    debug_free(file, line, old_node->ptr);

    return new_ptr;
}

void *debug_calloc(char *file, int line, size_t count, size_t size) {
    void *ptr;

    ptr = debug_malloc(file, line, count * size);
    memset(ptr,0,count *size);
    return ptr;
}

char *debug_strdup(char *file, int line, const char *str) {
    void *ptr;
    int size;

    size = strlen(str);
    ptr = debug_malloc(file, line, size + 1);
    strcpy(ptr,str);

    return ptr;
}

#endif
