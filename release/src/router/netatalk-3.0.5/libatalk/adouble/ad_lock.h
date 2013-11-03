#ifndef LIBATALK_ADOUBLE_AD_PRIVATE_H
#define LIBATALK_ADOUBLE_AD_PRIVATE_H 1

#include <atalk/adouble.h>

/* this is so that we can keep lists of fds referencing the same file
 * around. that way, we can honor locks created by the same process
 * with the same file. */

#define adf_lock_init(a) do {   \
        (a)->adf_lockmax = 0;   \
        (a)->adf_lockcount = 0; \
        (a)->adf_lock = NULL;   \
    } while (0)

#define adf_lock_free(a) do {                                 \
        int i;                                                \
        if (!(a)->adf_lock)                                   \
            break;                                            \
        for (i = 0; i < (a)->adf_lockcount; i++) {            \
            adf_lock_t *lock = (a)->adf_lock + i;             \
            if (--(*lock->refcount) < 1)                      \
                free(lock->refcount);                         \
        }                                                     \
        free((a)->adf_lock);                                  \
        adf_lock_init(a);                                     \
    } while (0)

#endif /* libatalk/adouble/ad_private.h */
