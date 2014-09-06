#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "header_generic.h"

/*
 * header_generic(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 *
 */

/*******************************************************************-o-******
 * generic_header
 *
 * Parameters:
 *	  *vp	   (I)     Pointer to variable entry that points here.
 *	  *name	   (I/O)   Input name requested, output name found.
 *	  *length  (I/O)   Length of input and output oid's.
 *	   exact   (I)     TRUE if an exact match was requested.
 *	  *var_len (O)     Length of variable or 0 if function returned.
 *	(**write_method)   Hook to name a write method (UNUSED).
 *
 * Returns:
 *	MATCH_SUCCEEDED	If vp->name matches name (accounting for exact bit).
 *	MATCH_FAILED	Otherwise,
 *
 *
 * Check whether variable (vp) matches name.
 */
int
header_generic(struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{
    oid             newname[MAX_OID_LEN];
    int             result;

    DEBUGMSGTL(("util_funcs", "header_generic: "));
    DEBUGMSGOID(("util_funcs", name, *length));
    DEBUGMSG(("util_funcs", " exact=%d\n", exact));

    memcpy((char *) newname, (char *) vp->name,
           (int) vp->namelen * sizeof(oid));
    newname[vp->namelen] = 0;
    result = snmp_oid_compare(name, *length, newname, vp->namelen + 1);
    DEBUGMSGTL(("util_funcs", "  result: %d\n", result));
    if ((exact && (result != 0)) || (!exact && (result >= 0)))
        return (MATCH_FAILED);
    memcpy((char *) name, (char *) newname,
           ((int) vp->namelen + 1) * sizeof(oid));
    *length = vp->namelen + 1;

    *write_method = (WriteMethod*)0;
    *var_len = sizeof(long);    /* default to 'long' results */
    return (MATCH_SUCCEEDED);
}
