#include "lib_types.h"
#include "lib_string.h"

#define CHAR_BIT 8
#define MAXSTACK (sizeof(int) * CHAR_BIT)

static void qsexchange(void *a, void *b, size_t size) 
{
    size_t i;

    /******************
     *  exchange a,b  *
     ******************/

    for (i = sizeof(int); i <= size; i += sizeof(int)) {
        int t = *((int *)a);
        *((int *)a++) = *((int *)b);
        *((int *)b++) = t;
	}
    for (i = i - sizeof(int) + 1; i <= size; i++) {
        char t = *((char *)a);
        *((char *)a++) = *((char *)b);
        *((char *)b++) = t;
	}
}

void qsort(void *base, size_t nmemb, size_t size,
	   int (*compar)(const void *, const void *)) 
{
    void *lbStack[MAXSTACK], *ubStack[MAXSTACK];
    int sp;
    unsigned int offset;

    /********************
     *  ANSI-C qsort()  *
     ********************/

    lbStack[0] = (char *)base;
    ubStack[0] = (char *)base + (nmemb-1)*size;
    for (sp = 0; sp >= 0; sp--) {
        char *lb, *ub, *m;
        char *P, *i, *j;

        lb = lbStack[sp];
        ub = ubStack[sp];

        while (lb < ub) {

            /* select pivot and exchange with 1st element */
            offset = (ub - lb) >> 1;
            P = lb + offset - offset % size;
            qsexchange (lb, P, size);

            /* partition into two segments */
            i = lb + size;
            j = ub;
            while (1) {
                while (i < j && compar(lb, i) > 0) i += size;
                while (j >= i && compar(j, lb) > 0) j -= size;
                if (i >= j) break;
                qsexchange (i, j, size);
                j -= size;
                i += size;
		}

            /* pivot belongs in A[j] */
            qsexchange (lb, j, size);
            m = j;

            /* keep processing smallest segment, and stack largest */
            if (m - lb <= ub - m) {
                if (m + size < ub) {
                    lbStack[sp] = m + size;
                    ubStack[sp++] = ub;
		    }
                ub = m - size;
		} 
	    else {
                if (m - size > lb) {
                    lbStack[sp] = lb; 
                    ubStack[sp++] = m - size;
		    }
                lb = m + size;
		}
	    }
	}
}
