/*
 * agent_index.c
 *
 * Maintain a registry of index allocations
 *      (Primarily required for AgentX support,
 *       but it could be more widely useable).
 */


#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <signal.h>
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>
#include <net-snmp/agent/agent_index.h>

#include "snmpd.h"
#include "mibgroup/struct.h"
#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_iterator.h>
#include "mib_module_includes.h"

#ifdef USING_AGENTX_SUBAGENT_MODULE
#include "agentx/subagent.h"
#include "agentx/client.h"
#endif

netsnmp_feature_child_of(agent_index_all, libnetsnmpagent)

netsnmp_feature_child_of(remove_index, agent_index_all)

        /*
         * Initial support for index allocation
         */

struct snmp_index {
    netsnmp_variable_list *varbind;     /* or pointer to var_list ? */
    int             allocated;
    netsnmp_session *session;
    struct snmp_index *next_oid;
    struct snmp_index *prev_oid;
    struct snmp_index *next_idx;
}              *snmp_index_head = NULL;

extern netsnmp_session *main_session;

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.  
 */

char           *
register_string_index(oid * name, size_t name_len, char *cp)
{
    netsnmp_variable_list varbind, *res;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_OCTET_STR;
    snmp_set_var_objid(&varbind, name, name_len);
    if (cp != ANY_STRING_INDEX) {
        snmp_set_var_value(&varbind, (u_char *) cp, strlen(cp));
        res = register_index(&varbind, ALLOCATE_THIS_INDEX, main_session);
    } else {
        res = register_index(&varbind, ALLOCATE_ANY_INDEX, main_session);
    }

    if (res == NULL) {
        return NULL;
    } else {
        char *rv = (char *)malloc(res->val_len + 1);
        if (rv) {
            memcpy(rv, res->val.string, res->val_len);
            rv[res->val_len] = 0;
        }
        free(res);
        return rv;
    }
}

int
register_int_index(oid * name, size_t name_len, int val)
{
    netsnmp_variable_list varbind, *res;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_INTEGER;
    snmp_set_var_objid(&varbind, name, name_len);
    varbind.val.string = varbind.buf;
    if (val != ANY_INTEGER_INDEX) {
        varbind.val_len = sizeof(long);
        *varbind.val.integer = val;
        res = register_index(&varbind, ALLOCATE_THIS_INDEX, main_session);
    } else {
        res = register_index(&varbind, ALLOCATE_ANY_INDEX, main_session);
    }

    if (res == NULL) {
        return -1;
    } else {
        int             rv = *(res->val.integer);
        free(res);
        return rv;
    }
}

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.  
 */

netsnmp_variable_list *
register_oid_index(oid * name, size_t name_len,
                   oid * value, size_t value_len)
{
    netsnmp_variable_list varbind;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_OBJECT_ID;
    snmp_set_var_objid(&varbind, name, name_len);
    if (value != ANY_OID_INDEX) {
        snmp_set_var_value(&varbind, (u_char *) value,
                           value_len * sizeof(oid));
        return register_index(&varbind, ALLOCATE_THIS_INDEX, main_session);
    } else {
        return register_index(&varbind, ALLOCATE_ANY_INDEX, main_session);
    }
}

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.  
 */

netsnmp_variable_list *
register_index(netsnmp_variable_list * varbind, int flags,
               netsnmp_session * ss)
{
    netsnmp_variable_list *rv = NULL;
    struct snmp_index *new_index, *idxptr, *idxptr2;
    struct snmp_index *prev_oid_ptr, *prev_idx_ptr;
    int             res, res2, i;

    DEBUGMSGTL(("register_index", "register "));
    DEBUGMSGVAR(("register_index", varbind));
    DEBUGMSG(("register_index", "for session %8p\n", ss));

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(TESTING)
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) == SUB_AGENT) {
        return (agentx_register_index(ss, varbind, flags));
    }
#endif
    /*
     * Look for the requested OID entry 
     */
    prev_oid_ptr = NULL;
    prev_idx_ptr = NULL;
    res = 1;
    res2 = 1;
    for (idxptr = snmp_index_head; idxptr != NULL;
         prev_oid_ptr = idxptr, idxptr = idxptr->next_oid) {
        if ((res = snmp_oid_compare(varbind->name, varbind->name_length,
                                    idxptr->varbind->name,
                                    idxptr->varbind->name_length)) <= 0)
            break;
    }

    /*
     * Found the OID - now look at the registered indices 
     */
    if (res == 0 && idxptr) {
        if (varbind->type != idxptr->varbind->type)
            return NULL;        /* wrong type */

        /*
         * If we've been asked for an arbitrary new value,
         *      then find the end of the list.
         * If we've been asked for any arbitrary value,
         *      then look for an unused entry, and use that.
         *      If there aren't any, continue as for new.
         * Otherwise, locate the given value in the (sorted)
         *      list of already allocated values
         */
        if (flags & ALLOCATE_ANY_INDEX) {
            for (idxptr2 = idxptr; idxptr2 != NULL;
                 prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {

                if (flags == ALLOCATE_ANY_INDEX && !(idxptr2->allocated)) {
                    if ((rv =
                         snmp_clone_varbind(idxptr2->varbind)) != NULL) {
                        idxptr2->session = ss;
                        idxptr2->allocated = 1;
                    }
                    return rv;
                }
            }
        } else {
            for (idxptr2 = idxptr; idxptr2 != NULL;
                 prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {
                switch (varbind->type) {
                case ASN_INTEGER:
                    res2 =
                        (*varbind->val.integer -
                         *idxptr2->varbind->val.integer);
                    break;
                case ASN_OCTET_STR:
                    i = SNMP_MIN(varbind->val_len,
                                 idxptr2->varbind->val_len);
                    res2 =
                        memcmp(varbind->val.string,
                               idxptr2->varbind->val.string, i);
                    break;
                case ASN_OBJECT_ID:
                    res2 =
                        snmp_oid_compare(varbind->val.objid,
                                         varbind->val_len / sizeof(oid),
                                         idxptr2->varbind->val.objid,
                                         idxptr2->varbind->val_len /
                                         sizeof(oid));
                    break;
                default:
                    return NULL;        /* wrong type */
                }
                if (res2 <= 0)
                    break;
            }
            if (res2 == 0) {
                if (idxptr2->allocated) {
                    /*
                     * No good: the index is in use.  
                     */
                    return NULL;
                } else {
                    /*
                     * Okay, it's unallocated, we can just claim ownership
                     * here.  
                     */
                    if ((rv =
                         snmp_clone_varbind(idxptr2->varbind)) != NULL) {
                        idxptr2->session = ss;
                        idxptr2->allocated = 1;
                    }
                    return rv;
                }
            }
        }
    }

    /*
     * OK - we've now located where the new entry needs to
     *      be fitted into the index registry tree          
     * To recap:
     *      'prev_oid_ptr' points to the head of the OID index
     *          list prior to this one.  If this is null, then
     *          it means that this is the first OID in the list.
     *      'idxptr' points either to the head of this OID list,
     *          or the next OID (if this is a new OID request)
     *          These can be distinguished by the value of 'res'.
     *
     *      'prev_idx_ptr' points to the index entry that sorts
     *          immediately prior to the requested value (if any).
     *          If an arbitrary value is required, then this will
     *          point to the last allocated index.
     *          If this pointer is null, then either this is a new
     *          OID request, or the requested value is the first
     *          in the list.
     *      'idxptr2' points to the next sorted index (if any)
     *          but is not actually needed any more.
     *
     *  Clear?  Good!
     *      I hope you've been paying attention.
     *          There'll be a test later :-)
     */

    /*
     *      We proceed by creating the new entry
     *         (by copying the entry provided)
     */
    new_index = (struct snmp_index *) calloc(1, sizeof(struct snmp_index));
    if (new_index == NULL)
        return NULL;

    if (NULL == snmp_varlist_add_variable(&new_index->varbind,
                                          varbind->name,
                                          varbind->name_length,
                                          varbind->type,
                                          varbind->val.string,
                                          varbind->val_len)) {
        /*
         * if (snmp_clone_var( varbind, new_index->varbind ) != 0 ) 
         */
        free(new_index);
        return NULL;
    }
    new_index->session = ss;
    new_index->allocated = 1;

    if (varbind->type == ASN_OCTET_STR && flags == ALLOCATE_THIS_INDEX)
        new_index->varbind->val.string[new_index->varbind->val_len] = 0;

    /*
     * If we've been given a value, then we can use that, but
     *    otherwise, we need to create a new value for this entry.
     * Note that ANY_INDEX and NEW_INDEX are both covered by this
     *   test (since NEW_INDEX & ANY_INDEX = ANY_INDEX, remember?)
     */
    if (flags & ALLOCATE_ANY_INDEX) {
        if (prev_idx_ptr) {
            if (snmp_clone_var(prev_idx_ptr->varbind, new_index->varbind)
                != 0) {
                free(new_index);
                return NULL;
            }
        } else
            new_index->varbind->val.string = new_index->varbind->buf;

        switch (varbind->type) {
        case ASN_INTEGER:
            if (prev_idx_ptr) {
                (*new_index->varbind->val.integer)++;
            } else
                *(new_index->varbind->val.integer) = 1;
            new_index->varbind->val_len = sizeof(long);
            break;
        case ASN_OCTET_STR:
            if (prev_idx_ptr) {
                i = new_index->varbind->val_len - 1;
                while (new_index->varbind->buf[i] == 'z') {
                    new_index->varbind->buf[i] = 'a';
                    i--;
                    if (i < 0) {
                        i = new_index->varbind->val_len;
                        new_index->varbind->buf[i] = 'a';
                        new_index->varbind->buf[i + 1] = 0;
                    }
                }
                new_index->varbind->buf[i]++;
            } else
                strcpy((char *) new_index->varbind->buf, "aaaa");
            new_index->varbind->val_len =
                strlen((char *) new_index->varbind->buf);
            break;
        case ASN_OBJECT_ID:
            if (prev_idx_ptr) {
                i = prev_idx_ptr->varbind->val_len / sizeof(oid) - 1;
                while (new_index->varbind->val.objid[i] == 255) {
                    new_index->varbind->val.objid[i] = 1;
                    i--;
                    if (i == 0 && new_index->varbind->val.objid[0] == 2) {
                        new_index->varbind->val.objid[0] = 1;
                        i = new_index->varbind->val_len / sizeof(oid);
                        new_index->varbind->val.objid[i] = 0;
                        new_index->varbind->val_len += sizeof(oid);
                    }
                }
                new_index->varbind->val.objid[i]++;
            } else {
                /*
                 * If the requested OID name is small enough,
                 * *   append another OID (1) and use this as the
                 * *   default starting value for new indexes.
                 */
                if ((varbind->name_length + 1) * sizeof(oid) <= 40) {
                    for (i = 0; i < (int) varbind->name_length; i++)
                        new_index->varbind->val.objid[i] =
                            varbind->name[i];
                    new_index->varbind->val.objid[varbind->name_length] =
                        1;
                    new_index->varbind->val_len =
                        (varbind->name_length + 1) * sizeof(oid);
                } else {
                    /*
                     * Otherwise use '.1.1.1.1...' 
                     */
                    i = 40 / sizeof(oid);
                    if (i > 4)
                        i = 4;
                    new_index->varbind->val_len = i * (sizeof(oid));
                    for (i--; i >= 0; i--)
                        new_index->varbind->val.objid[i] = 1;
                }
            }
            break;
        default:
            snmp_free_var(new_index->varbind);
            free(new_index);
            return NULL;        /* Index type not supported */
        }
    }

    /*
     * Try to duplicate the new varbind for return.  
     */

    if ((rv = snmp_clone_varbind(new_index->varbind)) == NULL) {
        snmp_free_var(new_index->varbind);
        free(new_index);
        return NULL;
    }

    /*
     * Right - we've set up the new entry.
     * All that remains is to link it into the tree.
     * There are a number of possible cases here,
     *   so watch carefully.
     */
    if (prev_idx_ptr) {
        new_index->next_idx = prev_idx_ptr->next_idx;
        new_index->next_oid = prev_idx_ptr->next_oid;
        prev_idx_ptr->next_idx = new_index;
    } else {
        if (res == 0 && idxptr) {
            new_index->next_idx = idxptr;
            new_index->next_oid = idxptr->next_oid;
        } else {
            new_index->next_idx = NULL;
            new_index->next_oid = idxptr;
        }

        if (prev_oid_ptr) {
            while (prev_oid_ptr) {
                prev_oid_ptr->next_oid = new_index;
                prev_oid_ptr = prev_oid_ptr->next_idx;
            }
        } else
            snmp_index_head = new_index;
    }
    return rv;
}

        /*
         * Release an allocated index,
         *   to allow it to be used elsewhere
         */
netsnmp_feature_child_of(release_index,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_RELEASE_INDEX
int
release_index(netsnmp_variable_list * varbind)
{
    return (unregister_index(varbind, TRUE, NULL));
}
#endif /* NETSNMP_FEATURE_REMOVE_RELEASE_INDEX */

#ifndef NETSNMP_FEATURE_REMOVE_REMOVE_INDEX
        /*
         * Completely remove an allocated index,
         *   due to errors in the registration process.
         */
int
remove_index(netsnmp_variable_list * varbind, netsnmp_session * ss)
{
    return (unregister_index(varbind, FALSE, ss));
}
#endif /* NETSNMP_FEATURE_REMOVE_REMOVE_INDEX */

void
unregister_index_by_session(netsnmp_session * ss)
{
    struct snmp_index *idxptr, *idxptr2;
    for (idxptr = snmp_index_head; idxptr != NULL;
         idxptr = idxptr->next_oid)
        for (idxptr2 = idxptr; idxptr2 != NULL;
             idxptr2 = idxptr2->next_idx)
            if (idxptr2->session == ss) {
                idxptr2->allocated = 0;
                idxptr2->session = NULL;
            }
}


int
unregister_index(netsnmp_variable_list * varbind, int remember,
                 netsnmp_session * ss)
{
    struct snmp_index *idxptr, *idxptr2;
    struct snmp_index *prev_oid_ptr, *prev_idx_ptr;
    int             res, res2, i;

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(TESTING)
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) == SUB_AGENT) {
        return (agentx_unregister_index(ss, varbind));
    }
#endif
    /*
     * Look for the requested OID entry 
     */
    prev_oid_ptr = NULL;
    prev_idx_ptr = NULL;
    res = 1;
    res2 = 1;
    for (idxptr = snmp_index_head; idxptr != NULL;
         prev_oid_ptr = idxptr, idxptr = idxptr->next_oid) {
        if ((res = snmp_oid_compare(varbind->name, varbind->name_length,
                                    idxptr->varbind->name,
                                    idxptr->varbind->name_length)) <= 0)
            break;
    }

    if (res != 0)
        return INDEX_ERR_NOT_ALLOCATED;
    if (varbind->type != idxptr->varbind->type)
        return INDEX_ERR_WRONG_TYPE;

    for (idxptr2 = idxptr; idxptr2 != NULL;
         prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {
        i = SNMP_MIN(varbind->val_len, idxptr2->varbind->val_len);
        res2 =
            memcmp(varbind->val.string, idxptr2->varbind->val.string, i);
        if (res2 <= 0)
            break;
    }
    if (res2 != 0 || (res2 == 0 && !idxptr2->allocated)) {
        return INDEX_ERR_NOT_ALLOCATED;
    }
    if (ss != idxptr2->session)
        return INDEX_ERR_WRONG_SESSION;

    /*
     *  If this is a "normal" index unregistration,
     *      mark the index entry as unused, but leave
     *      it in situ.  This allows differentiation
     *      between ANY_INDEX and NEW_INDEX
     */
    if (remember) {
        idxptr2->allocated = 0; /* Unused index */
        idxptr2->session = NULL;
        return SNMP_ERR_NOERROR;
    }
    /*
     *  If this is a failed attempt to register a
     *      number of indexes, the successful ones
     *      must be removed completely.
     */
    if (prev_idx_ptr) {
        prev_idx_ptr->next_idx = idxptr2->next_idx;
    } else if (prev_oid_ptr) {
        if (idxptr2->next_idx)  /* Use p_idx_ptr as a temp variable */
            prev_idx_ptr = idxptr2->next_idx;
        else
            prev_idx_ptr = idxptr2->next_oid;
        while (prev_oid_ptr) {
            prev_oid_ptr->next_oid = prev_idx_ptr;
            prev_oid_ptr = prev_oid_ptr->next_idx;
        }
    } else {
        if (idxptr2->next_idx)
            snmp_index_head = idxptr2->next_idx;
        else
            snmp_index_head = idxptr2->next_oid;
    }
    snmp_free_var(idxptr2->varbind);
    free(idxptr2);
    return SNMP_ERR_NOERROR;
}

netsnmp_feature_child_of(unregister_indexes,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_UNREGISTER_INDEXES
int
unregister_string_index(oid * name, size_t name_len, char *cp)
{
    netsnmp_variable_list varbind;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_OCTET_STR;
    snmp_set_var_objid(&varbind, name, name_len);
    snmp_set_var_value(&varbind, (u_char *) cp, strlen(cp));
    return (unregister_index(&varbind, FALSE, main_session));
}

int
unregister_int_index(oid * name, size_t name_len, int val)
{
    netsnmp_variable_list varbind;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_INTEGER;
    snmp_set_var_objid(&varbind, name, name_len);
    varbind.val.string = varbind.buf;
    varbind.val_len = sizeof(long);
    *varbind.val.integer = val;
    return (unregister_index(&varbind, FALSE, main_session));
}

int
unregister_oid_index(oid * name, size_t name_len,
                     oid * value, size_t value_len)
{
    netsnmp_variable_list varbind;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    varbind.type = ASN_OBJECT_ID;
    snmp_set_var_objid(&varbind, name, name_len);
    snmp_set_var_value(&varbind, (u_char *) value,
                       value_len * sizeof(oid));
    return (unregister_index(&varbind, FALSE, main_session));
}
#endif /* NETSNMP_FEATURE_REMOVE_UNREGISTER_INDEXES */

void
dump_idx_registry(void)
{
    struct snmp_index *idxptr, *idxptr2;
    u_char         *sbuf = NULL, *ebuf = NULL;
    size_t          sbuf_len = 0, sout_len = 0, ebuf_len = 0, eout_len = 0;

    if (snmp_index_head != NULL) {
        printf("\nIndex Allocations:\n");
    }

    for (idxptr = snmp_index_head; idxptr != NULL;
         idxptr = idxptr->next_oid) {
        sout_len = 0;
        if (sprint_realloc_objid(&sbuf, &sbuf_len, &sout_len, 1,
                                 idxptr->varbind->name,
                                 idxptr->varbind->name_length)) {
            printf("%s indexes:\n", sbuf);
        } else {
            printf("%s [TRUNCATED] indexes:\n", sbuf);
        }

        for (idxptr2 = idxptr; idxptr2 != NULL;
             idxptr2 = idxptr2->next_idx) {
            switch (idxptr2->varbind->type) {
            case ASN_INTEGER:
                printf("    %ld for session %8p, allocated %d\n",
                       *idxptr2->varbind->val.integer, idxptr2->session,
                       idxptr2->allocated);
                break;
            case ASN_OCTET_STR:
                printf("    \"%s\" for session %8p, allocated %d\n",
                       idxptr2->varbind->val.string, idxptr2->session,
                       idxptr2->allocated);
                break;
            case ASN_OBJECT_ID:
                eout_len = 0;
                if (sprint_realloc_objid(&ebuf, &ebuf_len, &eout_len, 1,
                                         idxptr2->varbind->val.objid,
                                         idxptr2->varbind->val_len /
                                         sizeof(oid))) {
                    printf("    %s for session %8p, allocated %d\n", ebuf,
                           idxptr2->session, idxptr2->allocated);
                } else {
                    printf
                        ("    %s [TRUNCATED] for sess %8p, allocated %d\n",
                         ebuf, idxptr2->session, idxptr2->allocated);
                }
                break;
            default:
                printf("unsupported type (%d/0x%02x)\n",
                       idxptr2->varbind->type, idxptr2->varbind->type);
            }
        }
    }

    if (sbuf != NULL) {
        free(sbuf);
    }
    if (ebuf != NULL) {
        free(ebuf);
    }
}

netsnmp_feature_child_of(count_indexes, netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_UNUSED
unsigned long
count_indexes(oid * name, size_t namelen, int include_unallocated)
{
    struct snmp_index *i = NULL, *j = NULL;
    unsigned long   n = 0;

    for (i = snmp_index_head; i != NULL; i = i->next_oid) {
        if (netsnmp_oid_equals(name, namelen,
                             i->varbind->name,
                             i->varbind->name_length) == 0) {
            for (j = i; j != NULL; j = j->next_idx) {
                if (j->allocated || include_unallocated) {
                    n++;
                }
            }
        }
    }
    return n;
}
#endif /* NETSNMP_FEATURE_REMOVE_UNUSED */

#ifdef TESTING
netsnmp_variable_list varbind;
netsnmp_session main_sess, *main_session = &main_sess;

void
test_string_register(int n, char *cp)
{
    varbind->name[4] = n;
    if (register_string_index(varbind->name, varbind.name_length, cp) ==
        NULL)
        printf("allocating %s failed\n", cp);
}

void
test_int_register(int n, int val)
{
    varbind->name[4] = n;
    if (register_int_index(varbind->name, varbind.name_length, val) == -1)
        printf("allocating %d/%d failed\n", n, val);
}

void
test_oid_register(int n, int subid)
{
    netsnmp_variable_list *res;

    varbind->name[4] = n;
    if (subid != -1) {
        varbind->val.objid[5] = subid;
        res = register_oid_index(varbind->name, varbind.name_length,
                                 varbind->val.objid,
                                 varbind->val_len / sizeof(oid));
    } else
        res =
            register_oid_index(varbind->name, varbind.name_length, NULL,
                               0);

    if (res == NULL)
        printf("allocating %d/%d failed\n", n, subid);
}

void
main(int argc, char argv[])
{
    oid             name[] = { 1, 2, 3, 4, 0 };
    int             i;

    memset(&varbind, 0, sizeof(netsnmp_variable_list));
    snmp_set_var_objid(&varbind, name, 5);
    varbind->type = ASN_OCTET_STR;
    /*
     * Test index structure linking:
     *      a) sorted by OID
     */
    test_string_register(20, "empty OID");
    test_string_register(10, "first OID");
    test_string_register(40, "last OID");
    test_string_register(30, "middle OID");

    /*
     *      b) sorted by index value
     */
    test_string_register(25, "eee: empty IDX");
    test_string_register(25, "aaa: first IDX");
    test_string_register(25, "zzz: last IDX");
    test_string_register(25, "mmm: middle IDX");
    printf("This next one should fail....\n");
    test_string_register(25, "eee: empty IDX"); /* duplicate */
    printf("done\n");

    /*
     *      c) test initial index linking
     */
    test_string_register(5, "eee: empty initial IDX");
    test_string_register(5, "aaa: replace initial IDX");

    /*
     *      Did it all work?
     */
    dump_idx_registry();
    unregister_index_by_session(main_session);
    /*
     *  Now test index allocation
     *      a) integer values
     */
    test_int_register(110, -1); /* empty */
    test_int_register(110, -1); /* append */
    test_int_register(110, 10); /* append exact */
    printf("This next one should fail....\n");
    test_int_register(110, 10); /* exact duplicate */
    printf("done\n");
    test_int_register(110, -1); /* append */
    test_int_register(110, 5);  /* insert exact */

    /*
     *      b) string values
     */
    test_string_register(120, NULL);    /* empty */
    test_string_register(120, NULL);    /* append */
    test_string_register(120, "aaaz");
    test_string_register(120, NULL);    /* minor rollover */
    test_string_register(120, "zzzz");
    test_string_register(120, NULL);    /* major rollover */

    /*
     *      c) OID values
     */

    test_oid_register(130, -1); /* empty */
    test_oid_register(130, -1); /* append */

    varbind->val_len = varbind.name_length * sizeof(oid);
    memcpy(varbind->buf, varbind.name, varbind.val_len);
    varbind->val.objid = (oid *) varbind.buf;
    varbind->val_len += sizeof(oid);

    test_oid_register(130, 255);        /* append exact */
    test_oid_register(130, -1); /* minor rollover */
    test_oid_register(130, 100);        /* insert exact */
    printf("This next one should fail....\n");
    test_oid_register(130, 100);        /* exact duplicate */
    printf("done\n");

    varbind->val.objid = (oid *) varbind.buf;
    for (i = 0; i < 6; i++)
        varbind->val.objid[i] = 255;
    varbind->val.objid[0] = 1;
    test_oid_register(130, 255);        /* set up rollover  */
    test_oid_register(130, -1); /* medium rollover */

    for (i = 0; i < 6; i++)
        varbind->val.objid[i] = 255;
    varbind->val.objid[0] = 2;
    test_oid_register(130, 255);        /* set up rollover  */
    test_oid_register(130, -1); /* major rollover */

    /*
     *      Did it all work?
     */
    dump_idx_registry();

    /*
     *      Test the various "invalid" requests
     *      (unsupported types, mis-matched types, etc)
     */
    printf("The rest of these should fail....\n");
    test_oid_register(110, -1);
    test_oid_register(110, 100);
    test_oid_register(120, -1);
    test_oid_register(120, 100);
    test_string_register(110, NULL);
    test_string_register(110, "aaaa");
    test_string_register(130, NULL);
    test_string_register(130, "aaaa");
    test_int_register(120, -1);
    test_int_register(120, 1);
    test_int_register(130, -1);
    test_int_register(130, 1);
    printf("done - this dump should be the same as before\n");
    dump_idx_registry();
}
#endif
