/* vector.h ..... store a vector of PPTP_CALL information and search it
 *                efficiently.
 *                C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * $Id: vector.h,v 1.1.1.1 2000/12/23 08:19:51 scott Exp $
 */

#ifndef INC_VECTOR_H
#define INC_VECTOR_H

#include "pptp_ctrl.h" /* for definition of PPTP_CALL */

typedef struct vector_struct VECTOR;

VECTOR *vector_create();
void vector_destroy(VECTOR *v);

int vector_size(VECTOR *v);

/* vector_insert and vector_search return TRUE on success, FALSE on failure. */
int  vector_insert(VECTOR *v, int key, PPTP_CALL * call);
int  vector_remove(VECTOR *v, int key);
int  vector_search(VECTOR *v, int key, PPTP_CALL ** call);
/* vector_contains returns FALSE if not found, TRUE if found. */
int  vector_contains(VECTOR *v, int key);
/* find first unused key. Returns TRUE on success, FALSE if no. */
int  vector_scan(VECTOR *v, int lo, int hi, int *key);
/* get a specific PPTP_CALL ... useful only when iterating. */
PPTP_CALL * vector_get_Nth(VECTOR *v, int n);

#endif /* INC_VECTOR_H */
