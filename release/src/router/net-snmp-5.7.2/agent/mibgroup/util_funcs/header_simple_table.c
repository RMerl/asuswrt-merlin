#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "header_simple_table.h"

#include <limits.h>

/*******************************************************************-o-******
 * header_simple_table
 *
 * Parameters:
 *	  *vp		 Variable data.
 *	  *name		 Fully instantiated OID name.
 *	  *length	 Length of name.
 *	   exact	 TRUE if an exact match is desired.
 *	  *var_len	 Hook for size of returned data type.
 *	(**write_method) Hook for write method (UNUSED).
 *	   max
 *
 * Returns:
 *	0	If name matches vp->name (accounting for 'exact') and is
 *			not greater in length than 'max'.
 *	1	Otherwise.
 *
 *
 * Compare 'name' to vp->name for the best match or an exact match (if
 *	requested).  Also check that 'name' is not longer than 'max' if
 *	max is greater-than/equal 0.
 * Store a successful match in 'name', and increment the OID instance if
 *	the match was not exact.
 *
 * 'name' and 'length' are undefined upon failure.
 *
 */
int
header_simple_table(struct variable *vp, oid * name, size_t * length,
                    int exact, size_t * var_len,
                    WriteMethod ** write_method, int max)
{
    int             i, rtest;   /* Set to:      -1      If name < vp->name,
                                 *              1       If name > vp->name,
                                 *              0       Otherwise.
                                 */
    oid             newname[MAX_OID_LEN];

    for (i = 0, rtest = 0;
         i < (int) vp->namelen && i < (int) (*length) && !rtest; i++) {
        if (name[i] != vp->name[i]) {
            if (name[i] < vp->name[i])
                rtest = -1;
            else
                rtest = 1;
        }
    }
    if (rtest > 0 ||
        (exact == 1
         && (rtest || (int) *length != (int) (vp->namelen + 1)))) {
        if (var_len)
            *var_len = 0;
        return MATCH_FAILED;
    }

    memset(newname, 0, sizeof(newname));

    if (((int) *length) <= (int) vp->namelen || rtest == -1) {
        memmove(newname, vp->name, (int) vp->namelen * sizeof(oid));
        newname[vp->namelen] = 1;
        *length = vp->namelen + 1;
    } else if (((int) *length) > (int) vp->namelen + 1) {       /* exact case checked earlier */
        *length = vp->namelen + 1;
        memmove(newname, name, (*length) * sizeof(oid));
        if (name[*length - 1] < MAX_SUBID) {
            newname[*length - 1] = name[*length - 1] + 1;
        } else {
            /*
             * Careful not to overflow...
             */
            newname[*length - 1] = name[*length - 1];
        }
    } else {
        *length = vp->namelen + 1;
        memmove(newname, name, (*length) * sizeof(oid));
        if (!exact) {
            if (name[*length - 1] < MAX_SUBID) {
                newname[*length - 1] = name[*length - 1] + 1;
            } else {
                /*
                 * Careful not to overflow...
                 */
                newname[*length - 1] = name[*length - 1];
            }
        } else {
            newname[*length - 1] = name[*length - 1];
        }
    }
    if ((max >= 0 && ((int)newname[*length - 1] > max)) ||
               ( 0 == newname[*length - 1] )) {
        if (var_len)
            *var_len = 0;
        return MATCH_FAILED;
    }

    memmove(name, newname, (*length) * sizeof(oid));
    if (write_method)
        *write_method = (WriteMethod*)0;
    if (var_len)
        *var_len = sizeof(long);        /* default */
    return (MATCH_SUCCEEDED);
}
