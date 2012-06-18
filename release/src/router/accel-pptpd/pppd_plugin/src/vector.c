/* vector.c ..... store a vector of PPTP_CALL information and search it
 *                efficiently.
 *                C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: vector.c,v 1.3 2003/06/17 10:12:55 reink Exp $
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pptp_ctrl.h"
#include "vector.h"
/* #define VECTOR_DEBUG */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct vector_item {
    int key;
    PPTP_CALL *call;
};

struct vector_struct {
    struct vector_item *item;
    int size;
    int alloc;
#ifdef VECTOR_DEBUG
    int key_max;
#endif
};

static struct vector_item *binary_search(VECTOR *v, int key);

/*** vector_create ************************************************************/
VECTOR *vector_create()
{
    const int INITIAL_SIZE = 4;

    VECTOR *v = malloc(sizeof(*v));
    if (v == NULL) return v;

    v->size = 0;
    v->alloc = INITIAL_SIZE;
    v->item = malloc(sizeof(*(v->item)) * (v->alloc));
#ifdef VECTOR_DEBUG
    v->key_max = -1;
#endif
    if (v->item == NULL) { free(v); return NULL; }
    else return v;
}

/*** vector_destroy ***********************************************************/
void vector_destroy(VECTOR *v)
{
    free(v->item);
#ifdef VECTOR_DEBUG
    v->item = NULL;
#endif
    free(v);
}

/*** vector_size **************************************************************/
int vector_size(VECTOR *v)
{
    assert(v != NULL);
    return v->size;
}

/*** vector_insert*************************************************************
 * nice thing about file descriptors is that we are assured by POSIX 
 * that they are monotonically increasing.
 */
int vector_insert(VECTOR *v, int key, PPTP_CALL * call)
{
    int i;
    assert(v != NULL && call != NULL);
    assert(!vector_contains(v, key));
#ifdef VECTOR_DEBUG
    assert(v->key_max < key);
#endif
    if (!(v->size < v->alloc)) {
        void *tmp = realloc(v->item, sizeof(*(v->item)) * 2 * v->alloc);
        if (tmp != NULL) {
            v->alloc *= 2;
            v->item = tmp;
        } else return FALSE; /* failed to alloc memory. */
    }
    assert(v->size < v->alloc);
    /* for safety, we make this work in the general case;
     * but this is optimized for adding call to the end of the vector.
     */
    for(i = v->size - 1; i >= 0; i--)
        if (v->item[i].key < key)
            break;
    /* insert after item i */
    memmove(&v->item[i + 2], &v->item[i + 1],
            (v->size - i - 1) * sizeof(*(v->item)));
    v->item[i + 1].key  = key;
    v->item[i + 1].call = call;
    v->size++;
#ifdef VECTOR_DEBUG
    if (v->key_max < key) /* ie, always. */
        v->key_max = key;
#endif
    return TRUE;
}

/*** vector_remove ************************************************************/
int  vector_remove(VECTOR *v, int key)
{
    struct vector_item *tmp;
    assert(v != NULL);
    if ((tmp =binary_search(v,key)) == NULL) return FALSE;
    assert(tmp >= v->item && tmp < v->item + v->size);
    memmove(tmp, tmp + 1, (v->size - (v->item - tmp) - 1) * sizeof(*(v->item)));
    v->size--;
    return TRUE;
}

/*** vector_search ************************************************************/
int  vector_search(VECTOR *v, int key, PPTP_CALL **call)
{
    struct vector_item *tmp;
    assert(v != NULL);
    tmp = binary_search(v, key);
    if (tmp ==NULL) return FALSE;
    *call = tmp->call;
    return TRUE;
}

/*** vector_contains **********************************************************/
int  vector_contains(VECTOR *v, int key)
{
    assert(v != NULL);
    return (binary_search(v, key) != NULL);
}

/*** vector_item **************************************************************/
static struct vector_item *binary_search(VECTOR *v, int key)
{
    int l,r,x;
    l = 0;
    r = v->size - 1;
    while (r >= l) {
        x = (l + r)/2;
        if (key <  v->item[x].key) r = x - 1; else l = x + 1;
        if (key == v->item[x].key) return &(v->item[x]);
    }
    return NULL;
}

/*** vector_scan ***************************************************************
 * Hmm.  Let's be fancy and use a binary search for the first
 * unused key, taking advantage of the list is stored sorted; ie
 * we can look at pointers and keys at two different locations, 
 * and if (ptr1 - ptr2) = (key1 - key2) then all the slots
 * between ptr1 and ptr2 are filled.  Note that ptr1-ptr2 should
 * never be greater than key1-key2 (no duplicate keys!)... we
 * check for this.
 */
int vector_scan(VECTOR *v, int lo, int hi, int *key)
{
    int l,r,x;
    assert(v != NULL);
    assert(key != NULL);
    if ((v->size<1) || (lo < v->item[0].key)) { *key = lo; return TRUE; }
    /* our array bounds */
    l = 0;  r = v->size - 1;
    while (r > l) {
        /* check for a free spot right after l */
        if (v->item[l].key + 1 < v->item[l + 1].key) { /* found it! */
            *key = v->item[l].key + 1;
            return TRUE;
        }
        /* no dice. Let's see if the free spot is before or after the midpoint */
        x = (l + r)/2;
        /* Okay, we have right (r), left (l) and the probe (x). */
        assert(x - l <= v->item[x].key - v->item[l].key);
        assert(r - x <= v->item[r].key - v->item[x].key);
        if (x - l < v->item[x].key - v->item[l].key)
            /* room between l and x */
            r = x;
        else /* no room between l and x */
            if (r - x < v->item[r].key - v->item[x].key)
                /* room between x and r */
                l = x;
            else /* no room between x and r, either */
                break; /* game over, man. */
    }
    /* no room found in already allocated space.  Check to see if
     * there's free space above allocated entries. */
    if (v->item[v->size - 1].key < hi) {
        *key = v->item[v->size - 1].key + 1;
        return TRUE;
    }
    /* outta luck */
    return FALSE;
}

/*** vector_get_Nth ***********************************************************/
PPTP_CALL * vector_get_Nth(VECTOR *v, int n)
{
    assert(v != NULL);
    assert(0 <= n && n < vector_size(v));
    return v->item[n].call;
}
