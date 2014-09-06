/*
 * header complex:  More complex storage and data sorting for mib modules 
 */

#include <net-snmp/net-snmp-config.h>

#include <sys/types.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "header_complex.h"

#include <net-snmp/net-snmp-features.h>

netsnmp_feature_child_of(header_complex_all, libnetsnmpmibs)

netsnmp_feature_child_of(header_complex_free_all, header_complex_all)
netsnmp_feature_child_of(header_complex_find_entry, header_complex_all)

int
header_complex_generate_varoid(netsnmp_variable_list * var)
{
    int             i;

    if (var->name == NULL) {
        /*
         * assume cached value is correct 
         */
        switch (var->type) {
        case ASN_INTEGER:
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
            var->name_length = 1;
            var->name = (oid *) malloc(sizeof(oid));
            if (var->name == NULL)
                return SNMPERR_GENERR;
            var->name[0] = *(var->val.integer);
            break;

        case ASN_PRIV_IMPLIED_OBJECT_ID:
            var->name_length = var->val_len / sizeof(oid);
            var->name = (oid *) malloc(sizeof(oid) * (var->name_length));
            if (var->name == NULL)
                return SNMPERR_GENERR;

            for (i = 0; i < (int) var->name_length; i++)
                var->name[i] = var->val.objid[i];
            break;

        case ASN_OBJECT_ID:
            var->name_length = var->val_len / sizeof(oid) + 1;
            var->name = (oid *) malloc(sizeof(oid) * (var->name_length));
            if (var->name == NULL)
                return SNMPERR_GENERR;

            var->name[0] = var->name_length - 1;
            for (i = 0; i < (int) var->name_length - 1; i++)
                var->name[i + 1] = var->val.objid[i];
            break;

        case ASN_PRIV_IMPLIED_OCTET_STR:
            var->name_length = var->val_len;
            var->name = (oid *) malloc(sizeof(oid) * (var->name_length));
            if (var->name == NULL)
                return SNMPERR_GENERR;

            for (i = 0; i < (int) var->val_len; i++)
                var->name[i] = (oid) var->val.string[i];
            break;

        case ASN_OPAQUE:
        case ASN_OCTET_STR:
            var->name_length = var->val_len + 1;
            var->name = (oid *) malloc(sizeof(oid) * (var->name_length));
            if (var->name == NULL)
                return SNMPERR_GENERR;

            var->name[0] = (oid) var->val_len;
            for (i = 0; i < (int) var->val_len; i++)
                var->name[i + 1] = (oid) var->val.string[i];
            break;

        default:
            DEBUGMSGTL(("header_complex_generate_varoid",
                        "invalid asn type: %d\n", var->type));
            return SNMPERR_GENERR;
        }
    }
    if (var->name_length > MAX_OID_LEN) {
        DEBUGMSGTL(("header_complex_generate_varoid",
                    "Something terribly wrong, namelen = %d\n",
                    (int)var->name_length));
        return SNMPERR_GENERR;
    }

    return SNMPERR_SUCCESS;
}

/*
 * header_complex_parse_oid(): parses an index to the usmTable to
 * break it down into a engineID component and a name component.
 * The results are stored in the data pointer, as a varbindlist:
 * 
 * 
 * returns 1 if an error is encountered, or 0 if successful.
 */
int
header_complex_parse_oid(oid * oidIndex, size_t oidLen,
                         netsnmp_variable_list * data)
{
    netsnmp_variable_list *var = data;
    int             i, itmp;

    while (var && oidLen > 0) {
        switch (var->type) {
        case ASN_INTEGER:
        case ASN_COUNTER:
        case ASN_GAUGE:
        case ASN_TIMETICKS:
            var->val.integer = (long *) calloc(1, sizeof(long));
            if (var->val.string == NULL)
                return SNMPERR_GENERR;

            *var->val.integer = (long) *oidIndex++;
            var->val_len = sizeof(long);
            oidLen--;
            DEBUGMSGTL(("header_complex_parse_oid",
                        "Parsed int(%d): %ld\n", var->type,
                        *var->val.integer));
            break;

        case ASN_OBJECT_ID:
        case ASN_PRIV_IMPLIED_OBJECT_ID:
            if (var->type == ASN_PRIV_IMPLIED_OBJECT_ID) {
                itmp = oidLen;
            } else {
                itmp = (long) *oidIndex++;
                oidLen--;
                if (itmp > (int) oidLen)
                    return SNMPERR_GENERR;
            }

            if (itmp == 0)
                break;          /* zero length strings shouldn't malloc */

            var->val_len = itmp * sizeof(oid);
            var->val.objid = (oid *) calloc(1, var->val_len);
            if (var->val.objid == NULL)
                return SNMPERR_GENERR;

            for (i = 0; i < itmp; i++)
                var->val.objid[i] = (u_char) * oidIndex++;
            oidLen -= itmp;

            DEBUGMSGTL(("header_complex_parse_oid", "Parsed oid: "));
            DEBUGMSGOID(("header_complex_parse_oid", var->val.objid,
                         var->val_len / sizeof(oid)));
            DEBUGMSG(("header_complex_parse_oid", "\n"));
            break;

        case ASN_OPAQUE:
        case ASN_OCTET_STR:
        case ASN_PRIV_IMPLIED_OCTET_STR:
            if (var->type == ASN_PRIV_IMPLIED_OCTET_STR) {
                itmp = oidLen;
            } else {
                itmp = (long) *oidIndex++;
                oidLen--;
                if (itmp > (int) oidLen)
                    return SNMPERR_GENERR;
            }

            if (itmp == 0)
                break;          /* zero length strings shouldn't malloc */

            /*
             * malloc by size+1 to allow a null to be appended. 
             */
            var->val_len = itmp;
            var->val.string = (u_char *) calloc(1, itmp + 1);
            if (var->val.string == NULL)
                return SNMPERR_GENERR;

            for (i = 0; i < itmp; i++)
                var->val.string[i] = (u_char) * oidIndex++;
            var->val.string[itmp] = '\0';
            oidLen -= itmp;

            DEBUGMSGTL(("header_complex_parse_oid",
                        "Parsed str(%d): %s\n", var->type,
                        var->val.string));
            break;

        default:
            DEBUGMSGTL(("header_complex_parse_oid",
                        "invalid asn type: %d\n", var->type));
            return SNMPERR_GENERR;
        }
        var = var->next_variable;
    }
    if (var != NULL || oidLen > 0)
        return SNMPERR_GENERR;
    return SNMPERR_SUCCESS;
}


void
header_complex_generate_oid(oid * name, /* out */
                            size_t * length,    /* out */
                            oid * prefix,
                            size_t prefix_len,
                            netsnmp_variable_list * data)
{

    oid            *oidptr;
    netsnmp_variable_list *var;

    if (prefix) {
        memcpy(name, prefix, prefix_len * sizeof(oid));
        oidptr = (name + (prefix_len));
        *length = prefix_len;
    } else {
        oidptr = name;
        *length = 0;
    }

    for (var = data; var != NULL; var = var->next_variable) {
        header_complex_generate_varoid(var);
        memcpy(oidptr, var->name, sizeof(oid) * var->name_length);
        oidptr = oidptr + var->name_length;
        *length += var->name_length;
    }

    DEBUGMSGTL(("header_complex_generate_oid", "generated: "));
    DEBUGMSGOID(("header_complex_generate_oid", name, *length));
    DEBUGMSG(("header_complex_generate_oid", "\n"));
}

/*
 * finds the data in "datalist" stored at "index" 
 */
void           *
header_complex_get(struct header_complex_index *datalist,
                   netsnmp_variable_list * index)
{
    oid             searchfor[MAX_OID_LEN];
    size_t          searchfor_len;

    header_complex_generate_oid(searchfor,      /* out */
                                &searchfor_len, /* out */
                                NULL, 0, index);
    return header_complex_get_from_oid(datalist, searchfor, searchfor_len);
}

void           *
header_complex_get_from_oid(struct header_complex_index *datalist,
                            oid * searchfor, size_t searchfor_len)
{
    struct header_complex_index *nptr;
    for (nptr = datalist; nptr != NULL; nptr = nptr->next) {
        if (netsnmp_oid_equals(searchfor, searchfor_len,
                             nptr->name, nptr->namelen) == 0)
            return nptr->data;
    }
    return NULL;
}


void           *
header_complex(struct header_complex_index *datalist,
               struct variable *vp,
               oid * name,
               size_t * length,
               int exact, size_t * var_len, WriteMethod ** write_method)
{

    struct header_complex_index *nptr, *found = NULL;
    oid             indexOid[MAX_OID_LEN];
    size_t          len;
    int             result;

    /*
     * set up some nice defaults for the user 
     */
    if (write_method)
        *write_method = NULL;
    if (var_len)
        *var_len = sizeof(long);

    for (nptr = datalist; nptr != NULL && found == NULL; nptr = nptr->next) {
        if (vp) {
            memcpy(indexOid, vp->name, vp->namelen * sizeof(oid));
            memcpy(indexOid + vp->namelen, nptr->name,
                   nptr->namelen * sizeof(oid));
            len = vp->namelen + nptr->namelen;
        } else {
            memcpy(indexOid, nptr->name, nptr->namelen * sizeof(oid));
            len = nptr->namelen;
        }
        result = snmp_oid_compare(name, *length, indexOid, len);
        DEBUGMSGTL(("header_complex", "Checking: "));
        DEBUGMSGOID(("header_complex", indexOid, len));
        DEBUGMSG(("header_complex", "\n"));

        if (exact) {
            if (result == 0) {
                found = nptr;
            }
        } else {
            if (result == 0) {
                /*
                 * found an exact match.  Need the next one for !exact 
                 */
                if (nptr->next)
                    found = nptr->next;
            } else if (result == -1) {
                found = nptr;
            }
        }
    }
    if (found) {
        if (vp) {
            memcpy(name, vp->name, vp->namelen * sizeof(oid));
            memcpy(name + vp->namelen, found->name,
                   found->namelen * sizeof(oid));
            *length = vp->namelen + found->namelen;
        } else {
            memcpy(name, found->name, found->namelen * sizeof(oid));
            *length = found->namelen;
        }
        return found->data;
    }

    return NULL;
}

struct header_complex_index *
header_complex_maybe_add_data(struct header_complex_index **thedata,
                              netsnmp_variable_list * var, void *data,
                              int dont_allow_duplicates)
{
    oid             newoid[MAX_OID_LEN];
    size_t          newoid_len;
    struct header_complex_index *ret;

    if (thedata == NULL || var == NULL || data == NULL)
        return NULL;

    header_complex_generate_oid(newoid, &newoid_len, NULL, 0, var);
    ret =
        header_complex_maybe_add_data_by_oid(thedata, newoid, newoid_len, data,
                                             dont_allow_duplicates);
    /*
     * free the variable list, but not the enclosed data!  it's not ours! 
     */
    snmp_free_varbind(var);
    return (ret);
}

struct header_complex_index *
header_complex_add_data(struct header_complex_index **thedata,
                        netsnmp_variable_list * var, void *data)
{
    return
        header_complex_maybe_add_data(thedata, var, data, 0);
}


struct header_complex_index *
_header_complex_add_between(struct header_complex_index **thedata,
                            struct header_complex_index *hciptrp,
                            struct header_complex_index *hciptrn,
                            oid * newoid, size_t newoid_len, void *data)
{
    struct header_complex_index *ourself;

    /*
     * nptr should now point to the spot that we need to add ourselves
     * in front of, and pptr should be our new 'prev'. 
     */

    /*
     * create ourselves 
     */
    ourself = (struct header_complex_index *)
        SNMP_MALLOC_STRUCT(header_complex_index);
    if (ourself == NULL)
        return NULL;

    /*
     * change our pointers 
     */
    ourself->prev = hciptrp;
    ourself->next = hciptrn;

    if (ourself->next)
        ourself->next->prev = ourself;

    if (ourself->prev)
        ourself->prev->next = ourself;

    ourself->data = data;
    ourself->name = snmp_duplicate_objid(newoid, newoid_len);
    ourself->namelen = newoid_len;

    /*
     * rewind to the head of the list and return it (since the new head
     * could be us, we need to notify the above routine who the head now is. 
     */
    for (hciptrp = ourself; hciptrp->prev != NULL;
         hciptrp = hciptrp->prev);

    *thedata = hciptrp;
    DEBUGMSGTL(("header_complex_add_data", "adding something...\n"));

    return hciptrp;
}


struct header_complex_index *
header_complex_maybe_add_data_by_oid(struct header_complex_index **thedata,
                                     oid * newoid, size_t newoid_len, void *data,
                                     int dont_allow_duplicates)
{
    struct header_complex_index *hciptrn, *hciptrp;
    int rc;

    if (thedata == NULL || newoid == NULL || data == NULL)
        return NULL;

    for (hciptrn = *thedata, hciptrp = NULL;
         hciptrn != NULL; hciptrp = hciptrn, hciptrn = hciptrn->next) {
        /*
         * XXX: check for == and error (overlapping table entries) 
         * 8/2005 rks Ok, I added duplicate entry check, but only log
         *            warning and continue, because it seems that nobody
         *            that calls this fucntion does error checking!.
         */
        rc = snmp_oid_compare(hciptrn->name, hciptrn->namelen,
                              newoid, newoid_len);
        if (rc > 0)
            break;
        else if (0 == rc) {
            snmp_log(LOG_WARNING, "header_complex_add_data_by_oid with "
                     "duplicate index.\n");
            if (dont_allow_duplicates)
                return NULL;
        }
    }

    return _header_complex_add_between(thedata, hciptrp, hciptrn,
                                       newoid, newoid_len, data);
}

struct header_complex_index *
header_complex_add_data_by_oid(struct header_complex_index **thedata,
                               oid * newoid, size_t newoid_len, void *data)
{
    return header_complex_maybe_add_data_by_oid(thedata, newoid, newoid_len,
                                                data, 0);
}

/*
 * extracts an entry from the storage space (removing it from future
 * accesses) and returns the data stored there
 * 
 * Modifies "thetop" pointer as needed (and if present) if were
 * extracting the first node.
 */

void           *
header_complex_extract_entry(struct header_complex_index **thetop,
                             struct header_complex_index *thespot)
{
    struct header_complex_index *hciptrp, *hciptrn;
    void           *retdata;

    if (thespot == NULL) {
        DEBUGMSGTL(("header_complex_extract_entry",
                    "Null pointer asked to be extracted\n"));
        return NULL;
    }

    retdata = thespot->data;

    hciptrp = thespot->prev;
    hciptrn = thespot->next;

    if (hciptrp)
        hciptrp->next = hciptrn;
    else if (thetop)
        *thetop = hciptrn;

    if (hciptrn)
        hciptrn->prev = hciptrp;

    if (thespot->name)
        free(thespot->name);

    free(thespot);
    return retdata;
}

/*
 * wipe out a single entry 
 */
void
header_complex_free_entry(struct header_complex_index *theentry,
                          HeaderComplexCleaner * cleaner)
{
    void           *data;
    data = header_complex_extract_entry(NULL, theentry);
    (*cleaner) (data);
}

/*
 * completely wipe out all entries in our data store 
 */
#ifndef NETSNMP_FEATURE_REMOVE_HEADER_COMPLEX_FREE_ALL
void
header_complex_free_all(struct header_complex_index *thestuff,
                        HeaderComplexCleaner * cleaner)
{
    struct header_complex_index *hciptr, *hciptrn;

    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptrn) {
        hciptrn = hciptr->next; /* need to extract this before deleting it */
        header_complex_free_entry(hciptr, cleaner);
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_HEADER_COMPLEX_FREE_ALL */

#ifndef NETSNMP_FEATURE_REMOVE_HEADER_COMPLEX_FIND_ENTRY
struct header_complex_index *
header_complex_find_entry(struct header_complex_index *thestuff,
                          void *theentry)
{
    struct header_complex_index *hciptr;

    for (hciptr = thestuff; hciptr != NULL && hciptr->data != theentry;
         hciptr = hciptr->next);
    return hciptr;
}
#endif /* NETSNMP_FEATURE_REMOVE_HEADER_COMPLEX_FIND_ENTRY */

#ifdef TESTING

void
header_complex_dump(struct header_complex_index *thestuff)
{
    struct header_complex_index *hciptr;
    oid             oidsave[MAX_OID_LEN];
    size_t          len;

    for (hciptr = thestuff; hciptr != NULL; hciptr = hciptr->next) {
        DEBUGMSGTL(("header_complex_dump", "var:  "));
        header_complex_generate_oid(oidsave, &len, NULL, 0, hciptr->);
        DEBUGMSGOID(("header_complex_dump", oidsave, len));
        DEBUGMSG(("header_complex_dump", "\n"));
    }
}

main()
{
    oid             oidsave[MAX_OID_LEN];
    int             len = MAX_OID_LEN, len2;
    netsnmp_variable_list *vars;
    long            ltmp = 4242, ltmp2 = 88, ltmp3 = 1, ltmp4 = 4200;
    oid             ourprefix[] = { 1, 2, 3, 4 };
    oid             testparse[] = { 4, 116, 101, 115, 116, 4200 };
    int             ret;

    char           *string = "wes", *string2 = "dawn", *string3 = "test";

    struct header_complex_index *thestorage = NULL;

    debug_register_tokens("header_complex");
    snmp_set_do_debugging(1);

    vars = NULL;
    len2 = sizeof(ltmp);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_INTEGER, (char *) &ltmp,
                              len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    vars = NULL;
    len2 = strlen(string);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, string, len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    vars = NULL;
    len2 = sizeof(ltmp2);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_INTEGER, (char *) &ltmp2,
                              len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    vars = NULL;
    len2 = strlen(string2);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, string2,
                              len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    vars = NULL;
    len2 = sizeof(ltmp3);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_INTEGER, (char *) &ltmp3,
                              len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    vars = NULL;
    len2 = strlen(string3);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, string3,
                              len2);
    len2 = sizeof(ltmp4);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_INTEGER, (char *) &ltmp4,
                              len2);
    header_complex_add_data(&thestorage, vars, ourprefix);

    header_complex_dump(thestorage);

    vars = NULL;
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_OCTET_STR, NULL, 0);
    snmp_varlist_add_variable(&vars, NULL, 0, ASN_INTEGER, NULL, 0);
    ret =
        header_complex_parse_oid(testparse,
                                 sizeof(testparse) / sizeof(oid), vars);
    DEBUGMSGTL(("header_complex_test", "parse returned %d...\n", ret));

}
#endif
