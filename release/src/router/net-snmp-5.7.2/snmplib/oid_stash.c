#include <net-snmp/net-snmp-config.h>

#include <string.h>

#include <stdlib.h>
#include <sys/types.h>

#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

netsnmp_feature_child_of(oid_stash_all, libnetsnmp)
netsnmp_feature_child_of(oid_stash, oid_stash_all)
netsnmp_feature_child_of(oid_stash_no_free, oid_stash_all)

#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH

/** @defgroup oid_stash Store and retrieve data referenced by an OID.
    This is essentially a way of storing data associated with a given
    OID.  It stores a bunch of data pointers within a memory tree that
    allows fairly efficient lookups with a heavily populated tree.
    @ingroup library
    @{
*/

/*
 * xxx-rks: when you have some spare time:
 *
 * b) basically, everything currently creates one node per sub-oid,
 *    which is less than optimal. add code to create nodes with the
 *    longest possible OID per node, and split nodes when necessary
 *    during adds.
 *
 * c) If you are feeling really ambitious, also merge split nodes if
 *    possible on a delete.
 *
 * xxx-wes: uh, right, like I *ever* have that much time.
 *
 */

/***************************************************************************
 *
 *
 ***************************************************************************/

/**
 * Create an netsnmp_oid_stash node
 *
 * @param mysize  the size of the child pointer array
 *
 * @return NULL on error, otherwise the newly allocated node
 */
netsnmp_oid_stash_node *
netsnmp_oid_stash_create_sized_node(size_t mysize)
{
    netsnmp_oid_stash_node *ret;
    ret = SNMP_MALLOC_TYPEDEF(netsnmp_oid_stash_node);
    if (!ret)
        return NULL;
    ret->children = (netsnmp_oid_stash_node**) calloc(mysize, sizeof(netsnmp_oid_stash_node *));
    if (!ret->children) {
        free(ret);
        return NULL;
    }
    ret->children_size = mysize;
    return ret;
}

/** Creates a netsnmp_oid_stash_node.
 * Assumes you want the default OID_STASH_CHILDREN_SIZE hash size for the node.
 * @return NULL on error, otherwise the newly allocated node
 */
NETSNMP_INLINE netsnmp_oid_stash_node *
netsnmp_oid_stash_create_node(void)
{
    return netsnmp_oid_stash_create_sized_node(OID_STASH_CHILDREN_SIZE);
}

netsnmp_feature_child_of(oid_stash_add_data, oid_stash_all)
#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH_ADD_DATA
/** adds data to the stash at a given oid.

 * @param root the top of the stash tree
 * @param lookup the oid index to store the data at.
 * @param lookup_len the length of the lookup oid.
 * @param mydata the data to store

 * @return SNMPERR_SUCCESS on success, SNMPERR_GENERR if data is
   already there, SNMPERR_MALLOC on malloc failures or if arguments
   passed in with NULL values.
 */
int
netsnmp_oid_stash_add_data(netsnmp_oid_stash_node **root,
                           const oid * lookup, size_t lookup_len, void *mydata)
{
    netsnmp_oid_stash_node *curnode, *tmpp, *loopp;
    unsigned int    i;

    if (!root || !lookup || lookup_len == 0)
        return SNMPERR_GENERR;

    if (!*root) {
        *root = netsnmp_oid_stash_create_node();
        if (!*root)
            return SNMPERR_MALLOC;
    }
    DEBUGMSGTL(( "oid_stash", "stash_add_data "));
    DEBUGMSGOID(("oid_stash", lookup, lookup_len));
    DEBUGMSG((   "oid_stash", "\n"));
    tmpp = NULL;
    for (curnode = *root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->children_size];
        if (!tmpp) {
            /*
             * no child in array at all 
             */
            tmpp = curnode->children[lookup[i] % curnode->children_size] =
                netsnmp_oid_stash_create_node();
            tmpp->value = lookup[i];
            tmpp->parent = curnode;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->next_sibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                /*
                 * none exists.  Create it 
                 */
                loopp = netsnmp_oid_stash_create_node();
                loopp->value = lookup[i];
                loopp->next_sibling = tmpp;
                loopp->parent = curnode;
                tmpp->prev_sibling = loopp;
                curnode->children[lookup[i] % curnode->children_size] =
                    loopp;
                tmpp = loopp;
            }
            /*
             * tmpp now points to the proper node 
             */
        }
        curnode = tmpp;
    }
    /*
     * tmpp now points to the exact match 
     */
    if (curnode->thedata)
        return SNMPERR_GENERR;
    if (NULL == tmpp)
        return SNMPERR_GENERR;
    tmpp->thedata = mydata;
    return SNMPERR_SUCCESS;
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_STASH_ADD_DATA */

/** returns a node associated with a given OID.
 * @param root the top of the stash tree
 * @param lookup the oid to look up a node for.
 * @param lookup_len the length of the lookup oid
 */
netsnmp_oid_stash_node *
netsnmp_oid_stash_get_node(netsnmp_oid_stash_node *root,
                           const oid * lookup, size_t lookup_len)
{
    netsnmp_oid_stash_node *curnode, *tmpp, *loopp;
    unsigned int    i;

    if (!root)
        return NULL;
    tmpp = NULL;
    for (curnode = root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->children_size];
        if (!tmpp) {
            return NULL;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->next_sibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                return NULL;
            }
        }
        curnode = tmpp;
    }
    return tmpp;
}

/** returns the next node associated with a given OID. INCOMPLETE.
    This is equivelent to a GETNEXT operation.
 * @internal
 * @param root the top of the stash tree
 * @param lookup the oid to look up a node for.
 * @param lookup_len the length of the lookup oid
 */
netsnmp_feature_child_of(oid_stash_iterate, oid_stash_all)
#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH_ITERATE
netsnmp_oid_stash_node *
netsnmp_oid_stash_getnext_node(netsnmp_oid_stash_node *root,
                               oid * lookup, size_t lookup_len)
{
    netsnmp_oid_stash_node *curnode, *tmpp, *loopp;
    unsigned int    i, j, bigger_than = 0, do_bigger = 0;

    if (!root)
        return NULL;
    tmpp = NULL;

    /* get closest matching node */
    for (curnode = root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->children_size];
        if (!tmpp) {
            break;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->next_sibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                break;
            }
        }
        curnode = tmpp;
    }

    /* find the *next* node lexographically greater */
    if (!curnode)
        return NULL; /* ack! */

    if (i+1 < lookup_len) {
        bigger_than = lookup[i+1];
        do_bigger = 1;
    }

    do {
        /* check the children first */
        tmpp = NULL;
        /* next child must be (next) greater than our next search node */
        /* XXX: should start this loop at best_nums[i]%... and wrap */
        for(j = 0; j < curnode->children_size; j++) {
            for (loopp = curnode->children[j];
                 loopp; loopp = loopp->next_sibling) {
                if ((!do_bigger || loopp->value > bigger_than) &&
                    (!tmpp || tmpp->value > loopp->value)) {
                    tmpp = loopp;
                    /* XXX: can do better and include min_nums[i] */
                    if (tmpp->value <= curnode->children_size-1) {
                        /* best we can do. */
                        goto done_this_loop;
                    }
                }
            }
        }

      done_this_loop:
        if (tmpp && tmpp->thedata)
            /* found a node with data.  Go with it. */
            return tmpp;

        if (tmpp) {
            /* found a child node without data, maybe find a grandchild? */
            do_bigger = 0;
            curnode = tmpp;
        } else {
            /* no respectable children (the bums), we'll have to go up.
               But to do so, they must be better than our current best_num + 1.
            */
            do_bigger = 1;
            bigger_than = curnode->value;
            curnode = curnode->parent;
        }
    } while (curnode);

    /* fell off the top */
    return NULL;
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_STASH_ITERATE */

netsnmp_feature_child_of(oid_stash_get_data, oid_stash_all)
#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH_GET_DATA
/** returns a data pointer associated with a given OID.

    This is equivelent to netsnmp_oid_stash_get_node, but returns only
    the data not the entire node.

 * @param root the top of the stash
 * @param lookup the oid to search for
 * @param lookup_len the length of the search oid.
 */
void           *
netsnmp_oid_stash_get_data(netsnmp_oid_stash_node *root,
                           const oid * lookup, size_t lookup_len)
{
    netsnmp_oid_stash_node *ret;
    ret = netsnmp_oid_stash_get_node(root, lookup, lookup_len);
    if (ret)
        return ret->thedata;
    return NULL;
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_STASH_GET_DATA */

netsnmp_feature_child_of(oid_stash_store_all, oid_stash_all)
#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH_STORE_ALL
/** a wrapper around netsnmp_oid_stash_store for use with a snmp_alarm.
 * when calling snmp_alarm, you can list this as a callback.  The
 * clientarg should be a pointer to a netsnmp_oid_stash_save_info
 * pointer.  It can also be called directly, of course.  The last
 * argument (clientarg) is the only one that is used.  The rest are
 * ignored by the function.
 * @param majorID
 * @param minorID
 * @param serverarg
 * @param clientarg A pointer to a netsnmp_oid_stash_save_info structure.
 */
int
netsnmp_oid_stash_store_all(int majorID, int minorID,
                            void *serverarg, void *clientarg) {
    oid oidbase[MAX_OID_LEN];
    netsnmp_oid_stash_save_info *sinfo;
    
    if (!clientarg)
        return SNMP_ERR_NOERROR;
    
    sinfo = (netsnmp_oid_stash_save_info *) clientarg;
    netsnmp_oid_stash_store(*(sinfo->root), sinfo->token, sinfo->dumpfn,
                            oidbase,0);
    return SNMP_ERR_NOERROR;
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_STASH_STORE_ALL */

/** stores data in a starsh tree to peristent storage.

    This function can be called to save all data in a stash tree to
    Net-SNMP's percent storage.  Make sure you register a parsing
    function with the read_config system to re-incorperate your saved
    data into future trees.

    @param root the top of the stash to store.
    @param tokenname the file token name to save in (passing "snmpd" will
    save things into snmpd.conf).
    @param dumpfn A function which can dump the data stored at a particular
    node into a char buffer.
    @param curoid must be a pointer to a OID array of length MAX_OID_LEN.
    @param curoid_len must be 0 for the top level call.
*/
void
netsnmp_oid_stash_store(netsnmp_oid_stash_node *root,
                        const char *tokenname, NetSNMPStashDump *dumpfn,
                        oid *curoid, size_t curoid_len) {

    char buf[SNMP_MAXBUF];
    netsnmp_oid_stash_node *tmpp;
    char *cp;
    char *appname = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID, 
                                          NETSNMP_DS_LIB_APPTYPE);
    int i;
    
    if (!tokenname || !root || !curoid || !dumpfn)
        return;

    for (i = 0; i < (int)root->children_size; i++) {
        if (root->children[i]) {
            for (tmpp = root->children[i]; tmpp; tmpp = tmpp->next_sibling) {
                curoid[curoid_len] = tmpp->value;
                if (tmpp->thedata) {
                    snprintf(buf, sizeof(buf), "%s ", tokenname);
                    cp = read_config_save_objid(buf+strlen(buf), curoid,
                                                curoid_len+1);
                    *cp++ = ' ';
                    *cp = '\0';
                    if ((*dumpfn)(cp, sizeof(buf) - strlen(buf),
                                  tmpp->thedata, tmpp))
                        read_config_store(appname, buf);
                }
                netsnmp_oid_stash_store(tmpp, tokenname, dumpfn,
                                        curoid, curoid_len+1);
            }
        }
    }
}

/** For debugging: dump the netsnmp_oid_stash tree to stdout
    @param root The top of the tree
    @param prefix a character string prefix printed to the beginning of each line.
*/
void 
oid_stash_dump(netsnmp_oid_stash_node *root, char *prefix)
{
    char            myprefix[MAX_OID_LEN * 4];
    netsnmp_oid_stash_node *tmpp;
    int             prefix_len = strlen(prefix) + 1;    /* actually it's +2 */
    unsigned int    i;

    memset(myprefix, ' ', MAX_OID_LEN * 4);
    myprefix[prefix_len] = '\0';

    for (i = 0; i < root->children_size; i++) {
        if (root->children[i]) {
            for (tmpp = root->children[i]; tmpp; tmpp = tmpp->next_sibling) {
                printf("%s%" NETSNMP_PRIo "d@%d: %s\n", prefix, tmpp->value, i,
                       (tmpp->thedata) ? "DATA" : "");
                oid_stash_dump(tmpp, myprefix);
            }
        }
    }
}

/** Frees the contents of a netsnmp_oid_stash tree.
    @param root the top of the tree (or branch to be freed)
    @param freefn The function to be called on each data (void *)
    pointer.  If left NULL the system free() function will be called
*/
void
netsnmp_oid_stash_free(netsnmp_oid_stash_node **root,
                       NetSNMPStashFreeNode *freefn) {

    netsnmp_oid_stash_node *curnode, *tmpp;
    unsigned int    i;

    if (!root || !*root)
        return;

    /* loop through all our children and free each node */
    for (i = 0; i < (*root)->children_size; i++) {
        if ((*root)->children[i]) {
            for(tmpp = (*root)->children[i]; tmpp; tmpp = curnode) {
                if (tmpp->thedata) {
                    if (freefn)
                        (*freefn)(tmpp->thedata);
                    else
                        free(tmpp->thedata);
                }
                curnode = tmpp->next_sibling;
                netsnmp_oid_stash_free(&tmpp, freefn);
            }
        }
    }
    free((*root)->children);
    free (*root);
    *root = NULL;
}

#else /* NETSNMP_FEATURE_REMOVE_OID_STASH */
netsnmp_feature_unused(oid_stash);
#endif/* NETSNMP_FEATURE_REMOVE_OID_STASH */

#ifndef NETSNMP_FEATURE_REMOVE_OID_STASH_NO_FREE
void
netsnmp_oid_stash_no_free(void *bogus)
{
    /* noop */
}
#endif /* NETSNMP_FEATURE_REMOVE_OID_STASH_NO_FREE */

/** @} */
