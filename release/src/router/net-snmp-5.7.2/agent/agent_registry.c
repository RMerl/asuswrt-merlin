/*
 * agent_registry.c
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/** @defgroup agent_registry Registry of MIB subtrees, modules, sessions, etc
 *     Maintain a registry of MIB subtrees, together with related information
 *     regarding MIB modules, sessions, etc
 *   @ingroup agent
 *
 * @{
 */

#define IN_SNMP_VARS_C

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
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_callbacks.h>

#include "snmpd.h"
#include "mibgroup/struct.h"
#include <net-snmp/agent/old_api.h>
#include <net-snmp/agent/null.h>
#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_iterator.h>
#include <net-snmp/agent/agent_registry.h>
#include "mib_module_includes.h"

#ifdef USING_AGENTX_SUBAGENT_MODULE
#include "agentx/subagent.h"
#include "agentx/client.h"
#endif

netsnmp_feature_child_of(agent_registry_all, libnetsnmpagent)

netsnmp_feature_child_of(unregister_mib_table_row, agent_registry_all)

/** @defgroup agent_lookup_cache Lookup cache, storing the registered OIDs.
 *     Maintain the cache used for locating sub-trees and OIDs.
 *   @ingroup agent_registry
 *
 * @{
 */

/**  Lookup cache - default size.*/
#define SUBTREE_DEFAULT_CACHE_SIZE 8
/**  Lookup cache - max acceptable size.*/
#define SUBTREE_MAX_CACHE_SIZE     32
int lookup_cache_size = 0; /*enabled later after registrations are loaded */

typedef struct lookup_cache_s {
   netsnmp_subtree *next;
   netsnmp_subtree *previous;
} lookup_cache;

typedef struct lookup_cache_context_s {
   char *context;
   struct lookup_cache_context_s *next;
   int thecachecount;
   int currentpos;
   lookup_cache cache[SUBTREE_MAX_CACHE_SIZE];
} lookup_cache_context;

static lookup_cache_context *thecontextcache = NULL;

/** Set the lookup cache size for optimized agent registration performance.
 * Note that it is only used by master agent - sub-agent doesn't need the cache.
 * The rough guide is that the cache size should be equal to the maximum
 * number of simultaneous managers you expect to talk to the agent (M) times 80%
 * (or so, he says randomly) the average number (N) of varbinds you
 * expect to receive in a given request for a manager.  ie, M times N.
 * Bigger does NOT necessarily mean better.  Certainly 16 should be an
 * upper limit.  32 is the hard coded limit.
 *
 * @param newsize set to the maximum size of a cache for a given
 * context.  Set to 0 to completely disable caching, or to -1 to set
 * to the default cache size (8), or to a number of your chosing.  The
 */
void
netsnmp_set_lookup_cache_size(int newsize) {
    if (newsize < 0)
        lookup_cache_size = SUBTREE_DEFAULT_CACHE_SIZE;
    else if (newsize < SUBTREE_MAX_CACHE_SIZE)
        lookup_cache_size = newsize;
    else
        lookup_cache_size = SUBTREE_MAX_CACHE_SIZE;
}

/** Retrieves the current value of the lookup cache size
 *  Should be called from master agent only - sub-agent doesn't need the cache.
 *
 *  @return the current lookup cache size
 */
int
netsnmp_get_lookup_cache_size(void) {
    return lookup_cache_size;
}

/** Returns lookup cache entry for the context of given name.
 *
 *  @param context Name of the context. Name is case sensitive.
 *
 *  @return the lookup cache context
 */
NETSNMP_STATIC_INLINE lookup_cache_context *
get_context_lookup_cache(const char *context) {
    lookup_cache_context *ptr;
    if (!context)
        context = "";

    for(ptr = thecontextcache; ptr; ptr = ptr->next) {
        if (strcmp(ptr->context, context) == 0)
            break;
    }
    if (!ptr) {
        if (netsnmp_subtree_find_first(context)) {
            ptr = SNMP_MALLOC_TYPEDEF(lookup_cache_context);
            ptr->next = thecontextcache;
            ptr->context = strdup(context);
            thecontextcache = ptr;
        } else {
            return NULL;
        }
    }
    return ptr;
}

/** Adds an entry to the Lookup Cache under specified context name.
 *
 *  @param context  Name of the context. Name is case sensitive.
 *
 *  @param next     Next subtree item.
 *
 *  @param previous Previous subtree item.
 */
NETSNMP_STATIC_INLINE void
lookup_cache_add(const char *context,
                 netsnmp_subtree *next, netsnmp_subtree *previous) {
    lookup_cache_context *cptr;

    if ((cptr = get_context_lookup_cache(context)) == NULL)
        return;

    if (cptr->thecachecount < lookup_cache_size)
        cptr->thecachecount++;

    cptr->cache[cptr->currentpos].next = next;
    cptr->cache[cptr->currentpos].previous = previous;

    if (++cptr->currentpos >= lookup_cache_size)
        cptr->currentpos = 0;
}

/** @private
 *  Replaces next and previous pointer in given Lookup Cache.
 *
 *  @param ptr      Lookup Cache pointer.
 *
 *  @param next     Next subtree item.
 *
 *  @param previous Previous subtree item.
 */
NETSNMP_STATIC_INLINE void
lookup_cache_replace(lookup_cache *ptr,
                     netsnmp_subtree *next, netsnmp_subtree *previous) {

    ptr->next = next;
    ptr->previous = previous;
}

/** Finds an entry in the Lookup Cache.
 *
 *  @param context  Case sensitive name of the context.
 *
 *  @param name     The OID we're searching for.
 *
 *  @param name_len Number of sub-ids (single integers) in the OID.
 *
 *  @param retcmp   Value set to snmp_oid_compare() call result.
 *                  The value, if set, is always nonnegative.
 *
 *  @return gives Lookup Cache entry, or NULL if not found.
 *
 *  @see snmp_oid_compare()
 */
NETSNMP_STATIC_INLINE lookup_cache *
lookup_cache_find(const char *context, const oid *name, size_t name_len,
                  int *retcmp) {
    lookup_cache_context *cptr;
    lookup_cache *ret = NULL;
    int cmp;
    int i;

    if ((cptr = get_context_lookup_cache(context)) == NULL)
        return NULL;

    for(i = 0; i < cptr->thecachecount && i < lookup_cache_size; i++) {
        if (cptr->cache[i].previous->start_a)
            cmp = snmp_oid_compare(name, name_len,
                                   cptr->cache[i].previous->start_a,
                                   cptr->cache[i].previous->start_len);
        else
            cmp = 1;
        if (cmp >= 0) {
            *retcmp = cmp;
            ret = &(cptr->cache[i]);
        }
    }
    return ret;
}

/** @private
 *  Clears cache count and position in Lookup Cache.
 */
NETSNMP_STATIC_INLINE void
invalidate_lookup_cache(const char *context) {
    lookup_cache_context *cptr;
    if ((cptr = get_context_lookup_cache(context)) != NULL) {
        cptr->thecachecount = 0;
        cptr->currentpos = 0;
    }
}

void
clear_lookup_cache(void) {

    lookup_cache_context *ptr = NULL, *next = NULL;

    ptr = thecontextcache;
    while (ptr) {
	next = ptr->next;
	SNMP_FREE(ptr->context);
	SNMP_FREE(ptr);
	ptr = next;
    }
    thecontextcache = NULL; /* !!! */
}

/**  @} */
/* End of Lookup cache code */

/** @defgroup agent_context_cache Context cache, storing the OIDs under their contexts.
 *     Maintain the cache used for locating sub-trees registered under different contexts.
 *   @ingroup agent_registry
 *
 * @{
 */
subtree_context_cache *context_subtrees = NULL;

/** Returns the top element of context subtrees cache.
 *  Use it if you wish to sweep through the cache elements.
 *  Note that the return may be NULL (cache may be empty).
 *
 *  @return pointer to topmost context subtree cache element.
 */
subtree_context_cache *
get_top_context_cache(void)
{
    return context_subtrees;
}

/** Finds the first subtree registered under given context.
 *
 *  @param context_name Text name of the context we're searching for.
 *
 *  @return pointer to the first subtree element, or NULL if not found.
 */
netsnmp_subtree *
netsnmp_subtree_find_first(const char *context_name)
{
    subtree_context_cache *ptr;

    if (!context_name) {
        context_name = "";
    }

    DEBUGMSGTL(("subtree", "looking for subtree for context: \"%s\"\n", 
		context_name));
    for (ptr = context_subtrees; ptr != NULL; ptr = ptr->next) {
        if (ptr->context_name != NULL && 
	    strcmp(ptr->context_name, context_name) == 0) {
            DEBUGMSGTL(("subtree", "found one for: \"%s\"\n", context_name));
            return ptr->first_subtree;
        }
    }
    DEBUGMSGTL(("subtree", "didn't find a subtree for context: \"%s\"\n", 
		context_name));
    return NULL;
}

/** Adds the subtree to Context Cache under given context name.
 *
 *  @param context_name Text name of the context we're adding.
 *
 *  @param new_tree The subtree to be added.
 *
 *  @return copy of the new_tree pointer, or NULL if cannot add.
 */
netsnmp_subtree *
add_subtree(netsnmp_subtree *new_tree, const char *context_name)
{
    subtree_context_cache *ptr = SNMP_MALLOC_TYPEDEF(subtree_context_cache);
    
    if (!context_name) {
        context_name = "";
    }

    if (!ptr) {
        return NULL;
    }
    
    DEBUGMSGTL(("subtree", "adding subtree for context: \"%s\"\n",	
		context_name));

    ptr->next = context_subtrees;
    ptr->first_subtree = new_tree;
    ptr->context_name = strdup(context_name);
    context_subtrees = ptr;

    return ptr->first_subtree;
}

void
netsnmp_remove_subtree(netsnmp_subtree *tree)
{
    subtree_context_cache *ptr;

    if (!tree->prev) {
        for (ptr = context_subtrees; ptr; ptr = ptr->next)
            if (ptr->first_subtree == tree)
                break;
        netsnmp_assert(ptr);
        if (ptr)
            ptr->first_subtree = tree->next;
    } else
        tree->prev->next = tree->next;

    if (tree->next)
        tree->next->prev = tree->prev;
}

/** Replaces first subtree registered under given context name.
 *  Overwrites a subtree pointer in Context Cache for the context name.
 *  The previous subtree pointer is lost. If there's no subtree
 *  under the supplied name, then a new cache item is created.
 *
 *  @param new_tree     The new subtree to be set.
 *
 *  @param context_name Text name of the context we're replacing.
 *                      It is case sensitive.
 *
 * @return copy of the new_tree pointer, or NULL on error.
 */
netsnmp_subtree *
netsnmp_subtree_replace_first(netsnmp_subtree *new_tree, 
			      const char *context_name)
{
    subtree_context_cache *ptr;
    if (!context_name) {
        context_name = "";
    }
    for (ptr = context_subtrees; ptr != NULL; ptr = ptr->next) {
        if (ptr->context_name != NULL &&
	    strcmp(ptr->context_name, context_name) == 0) {
            ptr->first_subtree = new_tree;
            return ptr->first_subtree;
        }
    }
    return add_subtree(new_tree, context_name);
}


void clear_subtree (netsnmp_subtree *sub);

/** Completely clears both the Context cache and the Lookup cache.
 */
void
clear_context(void) {

    subtree_context_cache *ptr = NULL, *next = NULL;
    netsnmp_subtree *t, *u;

    DEBUGMSGTL(("agent_registry", "clear context\n"));

    ptr = get_top_context_cache(); 
    while (ptr) {
	next = ptr->next;

	for (t = ptr->first_subtree; t; t = u) {
            u = t->next;
	    clear_subtree(t);
	}

        free(NETSNMP_REMOVE_CONST(char*, ptr->context_name));
        SNMP_FREE(ptr);

	ptr = next;
    }
    context_subtrees = NULL; /* !!! */
    clear_lookup_cache();
}

/**  @} */
/* End of Context cache code */

/** @defgroup agent_mib_subtree Maintaining MIB subtrees.
 *     Maintaining MIB nodes and subtrees.
 *   @ingroup agent_registry
 *
 * @{
 */

static void register_mib_detach_node(netsnmp_subtree *s);

/** Frees single subtree item.
 *  Deallocated memory for given netsnmp_subtree item, including
 *  Handle Registration structure stored inside this item.
 *  After calling this function, the pointer is invalid
 *  and should be set to NULL.
 *
 *  @param a The subtree item to dispose.
 */
void
netsnmp_subtree_free(netsnmp_subtree *a)
{
  if (a != NULL) {
    if (a->variables != NULL && netsnmp_oid_equals(a->name_a, a->namelen, 
					     a->start_a, a->start_len) == 0) {
      SNMP_FREE(a->variables);
    }
    SNMP_FREE(a->name_a);
    a->namelen = 0;
    SNMP_FREE(a->start_a);
    a->start_len = 0;
    SNMP_FREE(a->end_a);
    a->end_len = 0;
    SNMP_FREE(a->label_a);
    netsnmp_handler_registration_free(a->reginfo);
    a->reginfo = NULL;
    SNMP_FREE(a);
  }
}

/** Creates deep copy of a subtree item.
 *  Duplicates all properties stored in the structure, including
 *  Handle Registration structure stored inside the item.
 *
 *  @param a The subtree item to copy.
 *
 *  @return deep copy of the subtree item, or NULL on error.
 */
netsnmp_subtree *
netsnmp_subtree_deepcopy(netsnmp_subtree *a)
{
  netsnmp_subtree *b = (netsnmp_subtree *)calloc(1, sizeof(netsnmp_subtree));

  if (b != NULL) {
    memcpy(b, a, sizeof(netsnmp_subtree));
    b->name_a  = snmp_duplicate_objid(a->name_a,  a->namelen);
    b->start_a = snmp_duplicate_objid(a->start_a, a->start_len);
    b->end_a   = snmp_duplicate_objid(a->end_a,   a->end_len);
    b->label_a = strdup(a->label_a);
    
    if (b->name_a == NULL || b->start_a == NULL || 
	b->end_a  == NULL || b->label_a == NULL) {
      netsnmp_subtree_free(b);
      return NULL;
    }

    if (a->variables != NULL) {
      b->variables = (struct variable *)malloc(a->variables_len * 
					       a->variables_width);
      if (b->variables != NULL) {
	memcpy(b->variables, a->variables,a->variables_len*a->variables_width);
      } else {
	netsnmp_subtree_free(b);
	return NULL;
      }
    }

    if (a->reginfo != NULL) {
      b->reginfo = netsnmp_handler_registration_dup(a->reginfo);
      if (b->reginfo == NULL) {
	netsnmp_subtree_free(b);
	return NULL;
      }
    }
  }
  return b;
}

/** @private
 *  Replaces next subtree pointer in given subtree.
 */
NETSNMP_INLINE void
netsnmp_subtree_change_next(netsnmp_subtree *ptr, netsnmp_subtree *thenext)
{
    ptr->next = thenext;
    if (thenext)
        netsnmp_oid_compare_ll(ptr->start_a,
                               ptr->start_len,
                               thenext->start_a,
                               thenext->start_len,
                               &thenext->oid_off);
}

/** @private
 *  Replaces previous subtree pointer in given subtree.
 */
NETSNMP_INLINE void
netsnmp_subtree_change_prev(netsnmp_subtree *ptr, netsnmp_subtree *theprev)
{
    ptr->prev = theprev;
    if (theprev)
        netsnmp_oid_compare_ll(theprev->start_a,
                               theprev->start_len,
                               ptr->start_a,
                               ptr->start_len,
                               &ptr->oid_off);
}

netsnmp_feature_child_of(netsnmp_subtree_compare,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_NETSNMP_SUBTREE_COMPARE
/** Compares OIDs of given subtrees.
 *
 *  @param ap,bp Pointers to the subtrees to be compared.
 *
 *  @return OIDs lexicographical comparison result.
 *
 *  @see snmp_oid_compare()
 */
int
netsnmp_subtree_compare(const netsnmp_subtree *ap, const netsnmp_subtree *bp)
{
    return snmp_oid_compare(ap->name_a, ap->namelen, bp->name_a, bp->namelen);
}
#endif /* NETSNMP_FEATURE_REMOVE_NETSNMP_SUBTREE_COMPARE */

/** Joins the given subtree with the current tree.
 *  Trees are joined and the one supplied as parameter is freed.
 *
 *  @param root The subtree to be merged with current subtree.
 *              Do not use the pointer after joining - it may be invalid.
 */
void
netsnmp_subtree_join(netsnmp_subtree *root)
{
    netsnmp_subtree *s, *tmp, *c, *d;

    while (root != NULL) {
        s = root->next;
        while (s != NULL && root->reginfo == s->reginfo) {
            tmp = s->next;
            DEBUGMSGTL(("subtree", "root start "));
            DEBUGMSGOID(("subtree", root->start_a, root->start_len));
            DEBUGMSG(("subtree", " (original end "));
            DEBUGMSGOID(("subtree", root->end_a, root->end_len));
            DEBUGMSG(("subtree", ")\n"));
            DEBUGMSGTL(("subtree", "  JOINING to "));
            DEBUGMSGOID(("subtree", s->start_a, s->start_len));

	    SNMP_FREE(root->end_a);
	    root->end_a   = s->end_a;
            root->end_len = s->end_len;
	    s->end_a      = NULL;

            for (c = root; c != NULL; c = c->children) {
                netsnmp_subtree_change_next(c, s->next);
            }
            for (c = s; c != NULL; c = c->children) {
                netsnmp_subtree_change_prev(c, root);
            }
            DEBUGMSG(("subtree", " so new end "));
            DEBUGMSGOID(("subtree", root->end_a, root->end_len));
            DEBUGMSG(("subtree", "\n"));
            /*
             * Probably need to free children too?  
             */
            for (c = s->children; c != NULL; c = d) {
                d = c->children;
                netsnmp_subtree_free(c);
            }
            netsnmp_subtree_free(s);
            s = tmp;
        }
        root = root->next;
    }
}


/** Split the subtree into two at the specified point.
 *  Subtrees of the given OID and separated and formed into the
 *  returned subtree.
 *
 *  @param current The element at which splitting is started.
 *
 *  @param name The OID we'd like to split.
 *
 *  @param name_len Length of the OID.
 *
 *  @return head of the new (second) subtree.
 */
netsnmp_subtree *
netsnmp_subtree_split(netsnmp_subtree *current, oid name[], int name_len)
{
    struct variable *vp = NULL;
    netsnmp_subtree *new_sub, *ptr;
    int i = 0, rc = 0, rc2 = 0;
    size_t common_len = 0;
    char *cp;
    oid *tmp_a, *tmp_b;

    if (snmp_oid_compare(name, name_len, current->end_a, current->end_len)>0) {
	/* Split comes after the end of this subtree */
        return NULL;
    }

    new_sub = netsnmp_subtree_deepcopy(current);
    if (new_sub == NULL) {
        return NULL;
    }

    /*  Set up the point of division.  */
    tmp_a = snmp_duplicate_objid(name, name_len);
    if (tmp_a == NULL) {
	netsnmp_subtree_free(new_sub);
	return NULL;
    }
    tmp_b = snmp_duplicate_objid(name, name_len);
    if (tmp_b == NULL) {
	netsnmp_subtree_free(new_sub);
	SNMP_FREE(tmp_a);
	return NULL;
    }

    SNMP_FREE(current->end_a);
    current->end_a = tmp_a;
    current->end_len = name_len;
    if (new_sub->start_a != NULL) {
	SNMP_FREE(new_sub->start_a);
    }
    new_sub->start_a = tmp_b;
    new_sub->start_len = name_len;

    /*  Split the variables between the two new subtrees.  */
    i = current->variables_len;
    current->variables_len = 0;

    for (vp = current->variables; i > 0; i--) {
	/*  Note that the variable "name" field omits the prefix common to the
	    whole registration, hence the strange comparison here.  */

	rc = snmp_oid_compare(vp->name, vp->namelen,
			      name     + current->namelen, 
			      name_len - current->namelen);

        if (name_len - current->namelen > vp->namelen) {
            common_len = vp->namelen;
        } else {
            common_len = name_len - current->namelen;
        }

        rc2 = snmp_oid_compare(vp->name, common_len,
                               name + current->namelen, common_len);

        if (rc >= 0) {
            break;  /* All following variables belong to the second subtree */
	}

        current->variables_len++;
        if (rc2 < 0) {
            new_sub->variables_len--;
            cp = (char *) new_sub->variables;
            new_sub->variables = (struct variable *)(cp + 
						     new_sub->variables_width);
        }
        vp = (struct variable *) ((char *) vp + current->variables_width);
    }

    /* Delegated trees should retain their variables regardless */
    if (current->variables_len > 0 &&
        IS_DELEGATED((u_char) current->variables[0].type)) {
        new_sub->variables_len = 1;
        new_sub->variables = current->variables;
    }

    /* Propogate this split down through any children */
    if (current->children) {
        new_sub->children = netsnmp_subtree_split(current->children, 
						  name, name_len);
    }

    /* Retain the correct linking of the list */
    for (ptr = current; ptr != NULL; ptr = ptr->children) {
        netsnmp_subtree_change_next(ptr, new_sub);
    }
    for (ptr = new_sub; ptr != NULL; ptr = ptr->children) {
        netsnmp_subtree_change_prev(ptr, current);
    }
    for (ptr = new_sub->next; ptr != NULL; ptr=ptr->children) {
        netsnmp_subtree_change_prev(ptr, new_sub);
    }

    return new_sub;
}

/** Loads the subtree under given context name.
 *
 *  @param new_sub The subtree to be loaded into current subtree.
 *
 *  @param context_name Text name of the context we're searching for.
 *
 *  @return gives MIB_REGISTERED_OK on success, error code otherwise.
 */
int
netsnmp_subtree_load(netsnmp_subtree *new_sub, const char *context_name)
{
    netsnmp_subtree *tree1, *tree2;
    netsnmp_subtree *prev, *next;

    if (new_sub == NULL) {
        return MIB_REGISTERED_OK;       /* Degenerate case */
    }

    if (!netsnmp_subtree_find_first(context_name)) {
        static int inloop = 0;
        if (!inloop) {
            oid ccitt[1]           = { 0 };
            oid iso[1]             = { 1 };
            oid joint_ccitt_iso[1] = { 2 };
            inloop = 1;
            netsnmp_register_null_context(snmp_duplicate_objid(ccitt, 1), 1,
                                          context_name);
            netsnmp_register_null_context(snmp_duplicate_objid(iso, 1), 1,
                                          context_name);
            netsnmp_register_null_context(snmp_duplicate_objid(joint_ccitt_iso, 1),
                                          1, context_name);
            inloop = 0;
        }
    }

    /*  Find the subtree that contains the start of the new subtree (if
	any)...*/

    tree1 = netsnmp_subtree_find(new_sub->start_a, new_sub->start_len, 
				 NULL, context_name);

    /*  ... and the subtree that follows the new one (NULL implies this is the
	final region covered).  */

    if (tree1 == NULL) {
	tree2 = netsnmp_subtree_find_next(new_sub->start_a, new_sub->start_len,
					  NULL, context_name);
    } else {
	tree2 = tree1->next;
    }

    /*  Handle new subtrees that start in virgin territory.  */

    if (tree1 == NULL) {
        netsnmp_subtree *new2 = NULL;
	/*  Is there any overlap with later subtrees?  */
	if (tree2 && snmp_oid_compare(new_sub->end_a, new_sub->end_len,
				      tree2->start_a, tree2->start_len) > 0) {
	    new2 = netsnmp_subtree_split(new_sub, 
					 tree2->start_a, tree2->start_len);
	}

	/*  Link the new subtree (less any overlapping region) with the list of
	    existing registrations.  */

	if (tree2) {
            netsnmp_subtree_change_prev(new_sub, tree2->prev);
            netsnmp_subtree_change_prev(tree2, new_sub);
	} else {
            netsnmp_subtree_change_prev(new_sub,
                                        netsnmp_subtree_find_prev(new_sub->start_a,
                                                                  new_sub->start_len, NULL, context_name));

	    if (new_sub->prev) {
                netsnmp_subtree_change_next(new_sub->prev, new_sub);
	    } else {
		netsnmp_subtree_replace_first(new_sub, context_name);
	    }

            netsnmp_subtree_change_next(new_sub, tree2);

	    /* If there was any overlap, recurse to merge in the overlapping
	       region (including anything that may follow the overlap).  */
	    if (new2) {
		return netsnmp_subtree_load(new2, context_name);
	    }
	}
    } else {
	/*  If the new subtree starts *within* an existing registration
	    (rather than at the same point as it), then split the existing
	    subtree at this point.  */

	if (netsnmp_oid_equals(new_sub->start_a, new_sub->start_len, 
			     tree1->start_a,   tree1->start_len) != 0) {
	    tree1 = netsnmp_subtree_split(tree1, new_sub->start_a, 
					  new_sub->start_len);
	}

        if (tree1 == NULL) {
            return MIB_REGISTRATION_FAILED;
	}

	/*  Now consider the end of this existing subtree:
	    
	    If it matches the new subtree precisely,
	            simply merge the new one into the list of children

	    If it includes the whole of the new subtree,
		    split it at the appropriate point, and merge again
     
	    If the new subtree extends beyond this existing region,
	            split it, and recurse to merge the two parts.  */

	switch (snmp_oid_compare(new_sub->end_a, new_sub->end_len,
                                 tree1->end_a, tree1->end_len)) {

	case -1:
	    /*  Existing subtree contains new one.  */
	    netsnmp_subtree_split(tree1, new_sub->end_a, new_sub->end_len);
	    /* Fall Through */

	case  0:
	    /*  The two trees match precisely.  */

	    /*  Note: This is the only point where the original registration
	        OID ("name") is used.  */

	    prev = NULL;
	    next = tree1;
	
	    while (next && next->namelen > new_sub->namelen) {
		prev = next;
		next = next->children;
	    }

	    while (next && next->namelen == new_sub->namelen &&
		   next->priority < new_sub->priority ) {
		prev = next;
		next = next->children;
	    }
	
	    if (next && (next->namelen  == new_sub->namelen) &&
		(next->priority == new_sub->priority)) {
                if (new_sub->namelen != 1) {    /* ignore root OID dups */
                    size_t          out_len = 0;
                    size_t          buf_len = 0;
                    char           *buf = NULL;
                    int             buf_overflow = 0;

                    netsnmp_sprint_realloc_objid((u_char **) &buf, &buf_len, &out_len,
                                                 1, &buf_overflow,
                                                 new_sub->start_a,
                                                 new_sub->start_len);
                    snmp_log(LOG_ERR,
                             "duplicate registration: MIB modules %s and %s (oid %s%s).\n",
                             next->label_a, new_sub->label_a,
                             buf ? buf : "",
                             buf_overflow ? " [TRUNCATED]" : "");
                    free(buf);
                }
		return MIB_DUPLICATE_REGISTRATION;
	    }

	    if (prev) {
		prev->children    = new_sub;
		new_sub->children = next;
                netsnmp_subtree_change_prev(new_sub, prev->prev);
                netsnmp_subtree_change_next(new_sub, prev->next);
	    } else {
		new_sub->children = next;
                netsnmp_subtree_change_prev(new_sub, next->prev);
                netsnmp_subtree_change_next(new_sub, next->next);
	
		for (next = new_sub->next; next != NULL;next = next->children){
                    netsnmp_subtree_change_prev(next, new_sub);
		}

		for (prev = new_sub->prev; prev != NULL;prev = prev->children){
                    netsnmp_subtree_change_next(prev, new_sub);
		}
	    }
	    break;

	case  1:
	    /*  New subtree contains the existing one.  */
            {
                netsnmp_subtree *new2 =
                    netsnmp_subtree_split(new_sub, tree1->end_a,tree1->end_len);
                int res = netsnmp_subtree_load(new_sub, context_name);
                if (res != MIB_REGISTERED_OK) {
                    netsnmp_remove_subtree(new2);
                    netsnmp_subtree_free(new2);
                    return res;
                }
                return netsnmp_subtree_load(new2, context_name);
            }
        }
    }
    return 0;
}

/** Free the given subtree and all its children.
 *
 *  @param sub Subtree branch to be cleared and freed.
 *             After the call, this pointer is invalid
 *             and should be set to NULL.
 */
void
clear_subtree (netsnmp_subtree *sub) {

    netsnmp_subtree *c;
    
    if (sub == NULL)
	return;

    for(c = sub; c;) {
        sub = c;
        c = c->children;
        netsnmp_subtree_free(sub);
    }

}

netsnmp_subtree *
netsnmp_subtree_find_prev(const oid *name, size_t len, netsnmp_subtree *subtree,
			  const char *context_name)
{
    lookup_cache *lookup_cache = NULL;
    netsnmp_subtree *myptr = NULL, *previous = NULL;
    int cmp = 1;
    size_t ll_off = 0;

    if (subtree) {
        myptr = subtree;
    } else {
	/* look through everything */
        if (lookup_cache_size) {
            lookup_cache = lookup_cache_find(context_name, name, len, &cmp);
            if (lookup_cache) {
                myptr = lookup_cache->next;
                previous = lookup_cache->previous;
            }
            if (!myptr)
                myptr = netsnmp_subtree_find_first(context_name);
        } else {
            myptr = netsnmp_subtree_find_first(context_name);
        }
    }

    /*
     * this optimization causes a segfault on sf cf alpha-linux1.
     * ifdef out until someone figures out why and fixes it. xxx-rks 20051117
     */
#ifndef __alpha
#define WTEST_OPTIMIZATION 1
#endif
#ifdef WTEST_OPTIMIZATION
    DEBUGMSGTL(("wtest","oid in: "));
    DEBUGMSGOID(("wtest", name, len));
    DEBUGMSG(("wtest","\n"));
#endif
    for (; myptr != NULL; previous = myptr, myptr = myptr->next) {
#ifdef WTEST_OPTIMIZATION
        /* Compare the incoming oid with the linked list.  If we have
           results of previous compares, its faster to make sure the
           length we differed in the last check is greater than the
           length between this pointer and the last then we don't need
           to actually perform a comparison */
        DEBUGMSGTL(("wtest","oid cmp: "));
        DEBUGMSGOID(("wtest", myptr->start_a, myptr->start_len));
        DEBUGMSG(("wtest","  --- off = %lu, in off = %lu test = %d\n",
                  (unsigned long)myptr->oid_off, (unsigned long)ll_off,
                  !(ll_off && myptr->oid_off &&
                    myptr->oid_off > ll_off)));
        if (!(ll_off && myptr->oid_off && myptr->oid_off > ll_off) &&
            netsnmp_oid_compare_ll(name, len,
                                   myptr->start_a, myptr->start_len,
                                   &ll_off) < 0) {
#else
        if (snmp_oid_compare(name, len, myptr->start_a, myptr->start_len) < 0) {
#endif
            if (lookup_cache_size && previous && cmp) {
                if (lookup_cache) {
                    lookup_cache_replace(lookup_cache, myptr, previous);
                } else {
                    lookup_cache_add(context_name, myptr, previous);
                }
            }
            return previous;
        }
    }
    return previous;
}

netsnmp_subtree *
netsnmp_subtree_find_next(const oid *name, size_t len,
			  netsnmp_subtree *subtree, const char *context_name)
{
    netsnmp_subtree *myptr = NULL;

    myptr = netsnmp_subtree_find_prev(name, len, subtree, context_name);

    if (myptr != NULL) {
        myptr = myptr->next;
        while (myptr != NULL && (myptr->variables == NULL || 
				 myptr->variables_len == 0)) {
            myptr = myptr->next;
        }
        return myptr;
    } else if (subtree != NULL && snmp_oid_compare(name, len, 
				   subtree->start_a, subtree->start_len) < 0) {
        return subtree;
    } else {
        return NULL;
    }
}

netsnmp_subtree *
netsnmp_subtree_find(const oid *name, size_t len, netsnmp_subtree *subtree, 
		     const char *context_name)
{
    netsnmp_subtree *myptr;

    myptr = netsnmp_subtree_find_prev(name, len, subtree, context_name);
    if (myptr && myptr->end_a &&
        snmp_oid_compare(name, len, myptr->end_a, myptr->end_len)<0) {
        return myptr;
    }

    return NULL;
}

/**  @} */
/* End of Subtrees maintaining code */

/** @defgroup agent_mib_registering Registering and unregistering MIB subtrees.
 *     Adding and removing MIB nodes to the database under their contexts.
 *   @ingroup agent_registry
 *
 * @{
 */


/** Registers a MIB handler.
 *
 *  @param moduleName
 *  @param var
 *  @param varsize
 *  @param numvars
 *  @param  mibloc
 *  @param mibloclen
 *  @param priority
 *  @param range_subid
 *  @param range_ubound
 *  @param  ss
 *  @param context
 *  @param timeout
 *  @param flags
 *  @param reginfo Registration handler structure.
 *                 In a case of failure, it will be freed.
 *  @param perform_callback
 *
 *  @return gives MIB_REGISTERED_OK or MIB_* error code.
 *
 *  @see netsnmp_register_handler()
 *  @see register_agentx_list()
 *  @see netsnmp_handler_registration_free()
 */
int
netsnmp_register_mib(const char *moduleName,
                     struct variable *var,
                     size_t varsize,
                     size_t numvars,
                     oid * mibloc,
                     size_t mibloclen,
                     int priority,
                     int range_subid,
                     oid range_ubound,
                     netsnmp_session * ss,
                     const char *context,
                     int timeout,
                     int flags,
                     netsnmp_handler_registration *reginfo,
                     int perform_callback)
{
    netsnmp_subtree *subtree, *sub2;
    int             res;
    struct register_parameters reg_parms;
    int old_lookup_cache_val = netsnmp_get_lookup_cache_size();

    if (moduleName == NULL ||
        mibloc     == NULL) {
        /* Shouldn't happen ??? */
        netsnmp_handler_registration_free(reginfo);
        return MIB_REGISTRATION_FAILED;
    }
    subtree = (netsnmp_subtree *)calloc(1, sizeof(netsnmp_subtree));
    if (subtree == NULL) {
        netsnmp_handler_registration_free(reginfo);
        return MIB_REGISTRATION_FAILED;
    }

    DEBUGMSGTL(("register_mib", "registering \"%s\" at ", moduleName));
    DEBUGMSGOIDRANGE(("register_mib", mibloc, mibloclen, range_subid,
                      range_ubound));
    DEBUGMSG(("register_mib", " with context \"%s\"\n",
              SNMP_STRORNULL(context)));

    /*
     * verify that the passed context is equal to the context
     * in the reginfo.
     * (which begs the question, why do we have both? It appears that the
     *  reginfo item didn't appear til 5.2)
     */
    if( ((NULL == context) && (NULL != reginfo->contextName)) ||
        ((NULL != context) && (NULL == reginfo->contextName)) ||
        ( ((NULL != context) && (NULL != reginfo->contextName)) &&
          (0 != strcmp(context, reginfo->contextName))) ) {
        snmp_log(LOG_WARNING,"context passed during registration does not "
                 "equal the reginfo contextName! ('%s' != '%s')\n",
                 context, reginfo->contextName);
        netsnmp_assert(!"register context == reginfo->contextName"); /* always false */
    }

    /*  Create the new subtree node being registered.  */

    subtree->reginfo = reginfo;
    subtree->name_a  = snmp_duplicate_objid(mibloc, mibloclen);
    subtree->start_a = snmp_duplicate_objid(mibloc, mibloclen);
    subtree->end_a   = snmp_duplicate_objid(mibloc, mibloclen);
    subtree->label_a = strdup(moduleName);
    if (subtree->name_a == NULL || subtree->start_a == NULL || 
	subtree->end_a  == NULL || subtree->label_a == NULL) {
	netsnmp_subtree_free(subtree); /* also frees reginfo */
	return MIB_REGISTRATION_FAILED;
    }
    subtree->namelen   = (u_char)mibloclen;
    subtree->start_len = (u_char)mibloclen;
    subtree->end_len   = (u_char)mibloclen;
    subtree->end_a[mibloclen - 1]++;

    if (var != NULL) {
	subtree->variables = (struct variable *)malloc(varsize*numvars);
	if (subtree->variables == NULL) {
	    netsnmp_subtree_free(subtree); /* also frees reginfo */
	    return MIB_REGISTRATION_FAILED;
	}
	memcpy(subtree->variables, var, numvars*varsize);
	subtree->variables_len = numvars;
	subtree->variables_width = varsize;
    }
    subtree->priority = priority;
    subtree->timeout = timeout;
    subtree->range_subid = range_subid;
    subtree->range_ubound = range_ubound;
    subtree->session = ss;
    subtree->flags = (u_char)flags;    /*  used to identify instance oids  */
    subtree->flags |= SUBTREE_ATTACHED;
    subtree->global_cacheid = reginfo->global_cacheid;

    netsnmp_set_lookup_cache_size(0);
    res = netsnmp_subtree_load(subtree, context);

    /*  If registering a range, use the first subtree as a template for the
	rest of the range.  */

    if (res == MIB_REGISTERED_OK && range_subid != 0) {
        int i;
	for (i = mibloc[range_subid - 1] + 1; i <= (int)range_ubound; i++) {
	    sub2 = netsnmp_subtree_deepcopy(subtree);

	    if (sub2 == NULL) {
                unregister_mib_context(mibloc, mibloclen, priority,
                                       range_subid, range_ubound, context);
                netsnmp_set_lookup_cache_size(old_lookup_cache_val);
                invalidate_lookup_cache(context);
                return MIB_REGISTRATION_FAILED;
            }

            sub2->name_a[range_subid - 1]  = i;
            sub2->start_a[range_subid - 1] = i;
            sub2->end_a[range_subid - 1]   = i;     /* XXX - ???? */
            if (range_subid == (int)mibloclen) {
                ++sub2->end_a[range_subid - 1];
            }
            sub2->flags |= SUBTREE_ATTACHED;
            sub2->global_cacheid = reginfo->global_cacheid;
            /* FRQ This is essential for requests to succeed! */
            sub2->reginfo->rootoid[range_subid - 1]  = i;

            res = netsnmp_subtree_load(sub2, context);
            if (res != MIB_REGISTERED_OK) {
                unregister_mib_context(mibloc, mibloclen, priority,
                                       range_subid, range_ubound, context);
                netsnmp_remove_subtree(sub2);
		netsnmp_subtree_free(sub2);
                netsnmp_set_lookup_cache_size(old_lookup_cache_val);
                invalidate_lookup_cache(context);
                return res;
            }
        }
    } else if (res == MIB_DUPLICATE_REGISTRATION ||
               res == MIB_REGISTRATION_FAILED) {
        netsnmp_set_lookup_cache_size(old_lookup_cache_val);
        invalidate_lookup_cache(context);
        netsnmp_subtree_free(subtree);
        return res;
    }

    /*
     * mark the MIB as detached, if there's no master agent present as of now 
     */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_ROLE) != MASTER_AGENT) {
        extern struct snmp_session *main_session;
        if (main_session == NULL) {
            register_mib_detach_node(subtree);
	}
    }

    if (res == MIB_REGISTERED_OK && perform_callback) {
        memset(&reg_parms, 0x0, sizeof(reg_parms));
        reg_parms.name = mibloc;
        reg_parms.namelen = mibloclen;
        reg_parms.priority = priority;
        reg_parms.range_subid = range_subid;
        reg_parms.range_ubound = range_ubound;
        reg_parms.timeout = timeout;
        reg_parms.flags = (u_char) flags;
        reg_parms.contextName = context;
        reg_parms.session = ss;
        reg_parms.reginfo = reginfo;
        reg_parms.contextName = context;
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_REGISTER_OID, &reg_parms);
    }

    netsnmp_set_lookup_cache_size(old_lookup_cache_val);
    invalidate_lookup_cache(context);
    return res;
}

/** @private
 *  Reattach a particular node.  
 */
static void
register_mib_reattach_node(netsnmp_subtree *s)
{
    if ((s != NULL) && (s->namelen > 1) && !(s->flags & SUBTREE_ATTACHED)) {
        struct register_parameters reg_parms;
        /*
         * only do registrations that are not the top level nodes 
         */
        memset(&reg_parms, 0x0, sizeof(reg_parms));

        /*
         * XXX: do this better 
         */
        reg_parms.name = s->name_a;
        reg_parms.namelen = s->namelen;
        reg_parms.priority = s->priority;
        reg_parms.range_subid = s->range_subid;
        reg_parms.range_ubound = s->range_ubound;
        reg_parms.timeout = s->timeout;
        reg_parms.flags = s->flags;
        reg_parms.session = s->session;
        reg_parms.reginfo = s->reginfo;
        /* XXX: missing in subtree: reg_parms.contextName = s->context; */
        if ((NULL != s->reginfo) && (NULL != s->reginfo->contextName))
            reg_parms.contextName = s->reginfo->contextName;
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_REGISTER_OID, &reg_parms);
        s->flags |= SUBTREE_ATTACHED;
    }
}

/** Call callbacks to reattach all our nodes.  
 */
void
register_mib_reattach(void)
{
    netsnmp_subtree *s, *t;
    subtree_context_cache *ptr;

    for (ptr = context_subtrees; ptr; ptr = ptr->next) {
        for (s = ptr->first_subtree; s != NULL; s = s->next) {
            register_mib_reattach_node(s);
            for (t = s->children; t != NULL; t = t->children) {
                register_mib_reattach_node(t);
            }
        }
    }
}

/** @private
 *  Mark a node as detached.
 *
 *  @param s The note to be marked
 */
static void
register_mib_detach_node(netsnmp_subtree *s)
{
    if (s != NULL) {
        s->flags = s->flags & ~SUBTREE_ATTACHED;
    }
}

/** Mark all our registered OIDs as detached.
 *  This is only really useful for subagent protocols, when
 *  a connection is lost or the subagent is being shut down.  
 */
void
register_mib_detach(void)
{
    netsnmp_subtree *s, *t;
    subtree_context_cache *ptr;
    for (ptr = context_subtrees; ptr; ptr = ptr->next) {
        for (s = ptr->first_subtree; s != NULL; s = s->next) {
            register_mib_detach_node(s);
            for (t = s->children; t != NULL; t = t->children) {
                register_mib_detach_node(t);
            }
        }
    }
}

/** Register a new module into the MIB database, with all possible custom options
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct variable),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @param range_subid If non-zero, the module is registered against a range
 *                     of OIDs, with this parameter identifying the relevant
 *                     subidentifier - see RFC 2741 for details.
 *                     Typically used to register a single row of a table.
 *                     If zero, then register the module against the full OID subtree.
 *
 *  @param range_ubound The end of the range being registered (see RFC 2741)
 *                     If range_subid is zero, then this parameter is ignored.
 *
 *  @param ss 
 *  @param context
 *  @param timeout 
 *  @param flags 
 *
 *  @return gives SNMPERR_SUCCESS or SNMPERR_* error code.
 *
 *  @see register_mib()
 *  @see register_mib_priority()
 *  @see register_mib_range()
 *  @see unregister_mib()
 */
int
register_mib_context(const char *moduleName,
                     struct variable *var,
                     size_t varsize,
                     size_t numvars,
                     const oid * mibloc,
                     size_t mibloclen,
                     int priority,
                     int range_subid,
                     oid range_ubound,
                     netsnmp_session * ss,
                     const char *context, int timeout, int flags)
{
    return netsnmp_register_old_api(moduleName, var, varsize, numvars,
                                    mibloc, mibloclen, priority,
                                    range_subid, range_ubound, ss, context,
                                    timeout, flags);
}

/** Register a new module into the MIB database, as being responsible
 *   for a range of OIDs (typically a single row of a table).
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct variable),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @param range_subid If non-zero, the module is registered against a range
 *                     of OIDs, with this parameter identifying the relevant
 *                     subidentifier - see RFC 2741 for details.
 *                     Typically used to register a single row of a table.
 *                     If zero, then register the module against the full OID subtree.
 *
 *  @param range_ubound The end of the range being registered (see RFC 2741)
 *                     If range_subid is zero, then this parameter is ignored.
 *
 *  @param ss 
 *
 *  @return gives SNMPERR_SUCCESS or SNMPERR_* error code.
 *
 *  @see register_mib()
 *  @see register_mib_priority()
 *  @see register_mib_context()
 *  @see unregister_mib()
 */
int
register_mib_range(const char *moduleName,
                   struct variable *var,
                   size_t varsize,
                   size_t numvars,
                   const oid * mibloc,
                   size_t mibloclen,
                   int priority,
                   int range_subid, oid range_ubound, netsnmp_session * ss)
{
    return register_mib_context(moduleName, var, varsize, numvars,
                                mibloc, mibloclen, priority,
                                range_subid, range_ubound, ss, "", -1, 0);
}

/** Register a new module into the MIB database, with a non-default priority
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct variable),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @return gives SNMPERR_SUCCESS or SNMPERR_* error code.
 *
 *  @see register_mib()
 *  @see register_mib_range()
 *  @see register_mib_context()
 *  @see unregister_mib()
 */
int
register_mib_priority(const char *moduleName,
                      struct variable *var,
                      size_t varsize,
                      size_t numvars,
                      const oid * mibloc, size_t mibloclen, int priority)
{
    return register_mib_range(moduleName, var, varsize, numvars,
                              mibloc, mibloclen, priority, 0, 0, NULL);
}

/** Register a new module into the MIB database, using default priority and context
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct variable),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @return gives SNMPERR_SUCCESS or SNMPERR_* error code.
 *
 *  @see register_mib_priority()
 *  @see register_mib_range()
 *  @see register_mib_context()
 *  @see unregister_mib()
 */
int
register_mib(const char *moduleName,
             struct variable *var,
             size_t varsize,
             size_t numvars, const oid * mibloc, size_t mibloclen)
{
    return register_mib_priority(moduleName, var, varsize, numvars,
                                 mibloc, mibloclen, DEFAULT_MIB_PRIORITY);
}

/** @private
 *  Unloads a subtree from MIB tree.
 *
 *  @param  sub     The sub-tree which is being removed.
 *
 *  @param  prev    Previous entry, before the unloaded one.
 *
 *  @param  context Name of the context which is being removed.
 *
 *  @see unregister_mib_context()
 */
void
netsnmp_subtree_unload(netsnmp_subtree *sub, netsnmp_subtree *prev, const char *context)
{
    netsnmp_subtree *ptr;

    DEBUGMSGTL(("register_mib", "unload("));
    if (sub != NULL) {
        DEBUGMSGOID(("register_mib", sub->start_a, sub->start_len));
    } else {
        DEBUGMSG(("register_mib", "[NIL]"));
        return;
    }
    DEBUGMSG(("register_mib", ", "));
    if (prev != NULL) {
        DEBUGMSGOID(("register_mib", prev->start_a, prev->start_len));
    } else {
        DEBUGMSG(("register_mib", "[NIL]"));
    }
    DEBUGMSG(("register_mib", ")\n"));

    if (prev != NULL) {         /* non-leading entries are easy */
        prev->children = sub->children;
        invalidate_lookup_cache(context);
        return;
    }
    /*
     * otherwise, we need to amend our neighbours as well 
     */

    if (sub->children == NULL) {        /* just remove this node completely */
        for (ptr = sub->prev; ptr; ptr = ptr->children) {
            netsnmp_subtree_change_next(ptr, sub->next);
        }
        for (ptr = sub->next; ptr; ptr = ptr->children) {
            netsnmp_subtree_change_prev(ptr, sub->prev);
        }

	if (sub->prev == NULL) {
	    netsnmp_subtree_replace_first(sub->next, context);
	}

    } else {
        for (ptr = sub->prev; ptr; ptr = ptr->children)
            netsnmp_subtree_change_next(ptr, sub->children);
        for (ptr = sub->next; ptr; ptr = ptr->children)
            netsnmp_subtree_change_prev(ptr, sub->children);

	if (sub->prev == NULL) {
	    netsnmp_subtree_replace_first(sub->children, context);
	}
    }
    invalidate_lookup_cache(context);
}

/**
 * Unregisters a module registered against a given OID (or range) in a specified context. 
 * Typically used when a module has multiple contexts defined.
 * The parameters priority, range_subid, range_ubound and context
 * should match those used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @param range_subid  permits specifying a range in place of one of a subtree
 *                     sub-identifiers.  When this value is zero, no range is
 *                     being specified.
 *
 * @param range_ubound  the upper bound of a sub-identifier's range.
 *                      This field is present only if range_subid is not 0.
 *
 * @param context  a context name that has been created
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 * 
 * @see unregister_mib()
 * @see unregister_mib_priority()
 * @see unregister_mib_range()
 */
int
unregister_mib_context(oid * name, size_t len, int priority,
                       int range_subid, oid range_ubound,
                       const char *context)
{
    netsnmp_subtree *list, *myptr = NULL;
    netsnmp_subtree *prev, *child, *next; /* loop through children */
    struct register_parameters reg_parms;
    int old_lookup_cache_val = netsnmp_get_lookup_cache_size();
    int unregistering = 1;
    int orig_subid_val = -1;

    netsnmp_set_lookup_cache_size(0);

    if ((range_subid > 0) &&  ((size_t)range_subid <= len))
        orig_subid_val = name[range_subid-1];

    while(unregistering){
        DEBUGMSGTL(("register_mib", "unregistering "));
        DEBUGMSGOIDRANGE(("register_mib", name, len, range_subid, range_ubound));
        DEBUGMSG(("register_mib", "\n"));

        list = netsnmp_subtree_find(name, len, netsnmp_subtree_find_first(context),
                    context);
        if (list == NULL) {
            return MIB_NO_SUCH_REGISTRATION;
        }

        for (child = list, prev = NULL; child != NULL;
            prev = child, child = child->children) {
            if (netsnmp_oid_equals(child->name_a, child->namelen, name, len) == 0 &&
                child->priority == priority) {
                break;              /* found it */
             }
        }

        if (child == NULL) {
            return MIB_NO_SUCH_REGISTRATION;
        }

        netsnmp_subtree_unload(child, prev, context);
        myptr = child;              /* remember this for later */

        /*
        *  Now handle any occurances in the following subtrees,
        *      as a result of splitting this range.  Due to the
        *      nature of the way such splits work, the first
        *      subtree 'slice' that doesn't refer to the given
        *      name marks the end of the original region.
        *
        *  This should also serve to register ranges.
        */

        for (list = myptr->next; list != NULL; list = next) {
            next = list->next; /* list gets freed sometimes; cache next */
            for (child = list, prev = NULL; child != NULL;
                prev = child, child = child->children) {
                if ((netsnmp_oid_equals(child->name_a, child->namelen,
                    name, len) == 0) &&
            (child->priority == priority)) {
                    netsnmp_subtree_unload(child, prev, context);
                    netsnmp_subtree_free(child);
                    break;
                }
            }
            if (child == NULL)      /* Didn't find the given name */
                break;
        }

        /* Maybe we are in a range... */
        if (orig_subid_val != -1){
            if (++name[range_subid-1] >= orig_subid_val+range_ubound)
                {
                unregistering=0;
                name[range_subid-1] = orig_subid_val;
                }
        }
        else {
            unregistering=0;
        }
    }

    memset(&reg_parms, 0x0, sizeof(reg_parms));
    reg_parms.name = name;
    reg_parms.namelen = len;
    reg_parms.priority = priority;
    reg_parms.range_subid = range_subid;
    reg_parms.range_ubound = range_ubound;
    reg_parms.flags = 0x00;     /*  this is okay I think  */
    reg_parms.contextName = context;
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_UNREGISTER_OID, &reg_parms);

    netsnmp_subtree_free(myptr);
    netsnmp_set_lookup_cache_size(old_lookup_cache_val);
    invalidate_lookup_cache(context);
    return MIB_UNREGISTERED_OK;
}

#ifndef NETSNMP_FEATURE_REMOVE_UNREGISTER_MIB_TABLE_ROW
int
netsnmp_unregister_mib_table_row(oid * name, size_t len, int priority,
                                 int var_subid, oid range_ubound,
                                 const char *context)
{
    netsnmp_subtree *list, *myptr, *futureptr;
    netsnmp_subtree *prev, *child;       /* loop through children */
    struct register_parameters reg_parms;
    oid             range_lbound = name[var_subid - 1];

    DEBUGMSGTL(("register_mib", "unregistering "));
    DEBUGMSGOIDRANGE(("register_mib", name, len, var_subid, range_ubound));
    DEBUGMSG(("register_mib", "\n"));

    for (; name[var_subid - 1] <= range_ubound; name[var_subid - 1]++) {
        list = netsnmp_subtree_find(name, len, 
				netsnmp_subtree_find_first(context), context);

        if (list == NULL) {
            continue;
        }

        for (child = list, prev = NULL; child != NULL;
             prev = child, child = child->children) {

            if (netsnmp_oid_equals(child->name_a, child->namelen, 
				 name, len) == 0 && 
		(child->priority == priority)) {
                break;          /* found it */
            }
        }

        if (child == NULL) {
            continue;
        }

        netsnmp_subtree_unload(child, prev, context);
        myptr = child;          /* remember this for later */

        for (list = myptr->next; list != NULL; list = futureptr) {
            /* remember the next spot in the list in case we free this node */
            futureptr = list->next;

            /* check each child */
            for (child = list, prev = NULL; child != NULL;
                 prev = child, child = child->children) {

                if (netsnmp_oid_equals(child->name_a, child->namelen, 
				      name, len) == 0 &&
                    (child->priority == priority)) {
                    netsnmp_subtree_unload(child, prev, context);
                    netsnmp_subtree_free(child);
                    break;
                }
            }

            /* XXX: wjh: not sure why we're bailing here */
            if (child == NULL) {        /* Didn't find the given name */
                break;
            }
        }
        netsnmp_subtree_free(myptr);
    }

    name[var_subid - 1] = range_lbound;
    memset(&reg_parms, 0x0, sizeof(reg_parms));
    reg_parms.name = name;
    reg_parms.namelen = len;
    reg_parms.priority = priority;
    reg_parms.range_subid = var_subid;
    reg_parms.range_ubound = range_ubound;
    reg_parms.flags = 0x00;     /*  this is okay I think  */
    reg_parms.contextName = context;
    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                        SNMPD_CALLBACK_UNREGISTER_OID, &reg_parms);

    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_UNREGISTER_MIB_TABLE_ROW */

/**
 * Unregisters a module registered against a given OID (or range) in the default context. 
 * Typically used when a module has multiple contexts defined.
 * The parameters priority, range_subid, and range_ubound should
 * match those used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @param range_subid  permits specifying a range in place of one of a subtree
 *                     sub-identifiers.  When this value is zero, no range is
 *                     being specified.
 *
 * @param range_ubound  the upper bound of a sub-identifier's range.
 *                      This field is present only if range_subid is not 0.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 * 
 * @see unregister_mib()
 * @see unregister_mib_priority()
 * @see unregister_mib_context()
 */
int
unregister_mib_range(oid * name, size_t len, int priority,
                     int range_subid, oid range_ubound)
{
    return unregister_mib_context(name, len, priority, range_subid,
                                  range_ubound, "");
}

/**
 * Unregisters a module registered against a given OID at the specified priority.
 * The priority parameter should match that used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 * 
 * @see unregister_mib()
 * @see unregister_mib_range()
 * @see unregister_mib_context()
 */
int
unregister_mib_priority(oid * name, size_t len, int priority)
{
    return unregister_mib_range(name, len, priority, 0, 0);
}

/**
 * Unregisters a module registered against a given OID at the default priority.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  OID_LENGTH macro.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 * 
 * @see unregister_mib_priority()
 * @see unregister_mib_context()
 * @see unregister_mib_range()
 * @see unregister_agentx_list()
 */
int
unregister_mib(oid * name, size_t len)
{
    return unregister_mib_priority(name, len, DEFAULT_MIB_PRIORITY);
}

/** Unregisters subtree of OIDs bounded to given session.
 *
 *  @param ss Session which OIDs will be removed from tree.
 *
 *  @see unregister_mib()
 *  @see unregister_agentx_list()
 */
void
unregister_mibs_by_session(netsnmp_session * ss)
{
    netsnmp_subtree *list, *list2;
    netsnmp_subtree *child, *prev, *next_child;
    struct register_parameters rp;
    subtree_context_cache *contextptr;

    DEBUGMSGTL(("register_mib", "unregister_mibs_by_session(%p) ctxt \"%s\"\n",
		ss, (ss && ss->contextName) ? ss->contextName : "[NIL]"));

    for (contextptr = get_top_context_cache(); contextptr != NULL;
         contextptr = contextptr->next) {
        for (list = contextptr->first_subtree; list != NULL; list = list2) {
            list2 = list->next;

            for (child = list, prev = NULL; child != NULL; child = next_child){
                next_child = child->children;

                if (((!ss || ss->flags & SNMP_FLAGS_SUBSESSION) &&
		     child->session == ss) ||
                    (!(!ss || ss->flags & SNMP_FLAGS_SUBSESSION) && child->session &&
                     child->session->subsession == ss)) {

                    memset(&rp,0x0,sizeof(rp));
                    rp.name = child->name_a;
		    child->name_a = NULL;
                    rp.namelen = child->namelen;
                    rp.priority = child->priority;
                    rp.range_subid = child->range_subid;
                    rp.range_ubound = child->range_ubound;
                    rp.timeout = child->timeout;
                    rp.flags = child->flags;
                    if ((NULL != child->reginfo) &&
                        (NULL != child->reginfo->contextName))
                        rp.contextName = child->reginfo->contextName;

                    if (child->reginfo != NULL) {
                        /*
                         * Don't let's free the session pointer just yet!  
                         */
                        child->reginfo->handler->myvoid = NULL;
                        netsnmp_handler_registration_free(child->reginfo);
			child->reginfo = NULL;
                    }

                    netsnmp_subtree_unload(child, prev, contextptr->context_name);
                    netsnmp_subtree_free(child);

                    snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                                        SNMPD_CALLBACK_UNREGISTER_OID, &rp);
		    SNMP_FREE(rp.name);
                } else {
                    prev = child;
                }
            }
        }
        netsnmp_subtree_join(contextptr->first_subtree);
    }
}

/** Determines if given PDU is allowed to see (or update) a given OID.
 *
 * @param name    The OID to check access for.
 *                On return, this parameter holds the OID actually matched
 *
 * @param namelen Number of sub-identifiers in the OID.
 *                On return, this parameter holds the length of the matched OID
 *
 * @param pdu     PDU requesting access to the OID.
 *
 * @param type    ANS.1 type of the value at given OID.
 *                (Used for catching SNMPv1 requests for SMIv2-only objects)
 *
 * @return gives VACM_SUCCESS if the OID is in the PDU, otherwise error code.
 */
int
in_a_view(oid *name, size_t *namelen, netsnmp_pdu *pdu, int type)
{
    struct view_parameters view_parms;

    if (pdu->flags & UCD_MSG_FLAG_ALWAYS_IN_VIEW) {
	/* Enable bypassing of view-based access control */
        return VACM_SUCCESS;
    }

    /*
     * check for v1 and counter64s, since snmpv1 doesn't support it 
     */
#ifndef NETSNMP_DISABLE_SNMPV1
    if (pdu->version == SNMP_VERSION_1 && type == ASN_COUNTER64) {
        return VACM_NOTINVIEW;
    }
#endif

    view_parms.pdu = pdu;
    view_parms.name = name;
    if (namelen != NULL) {
        view_parms.namelen = *namelen;
    } else {
        view_parms.namelen = 0;
    }
    view_parms.errorcode = 0;
    view_parms.check_subtree = 0;

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
    case SNMP_VERSION_3:
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_ACM_CHECK, &view_parms);
        return view_parms.errorcode;
    }
    return VACM_NOSECNAME;
}

/** Determines if the given PDU request could potentially succeed.
 *  (Preliminary, OID-independent validation)
 *
 * @param pdu     PDU requesting access
 *
 * @return gives VACM_SUCCESS   if the entire MIB tree is accessible
 *               VACM_NOTINVIEW if the entire MIB tree is inaccessible
 *               VACM_SUBTREE_UNKNOWN if some portions are accessible
 *               other codes may returned on error
 */
int
check_access(netsnmp_pdu *pdu)
{                               /* IN - pdu being checked */
    struct view_parameters view_parms;
    view_parms.pdu = pdu;
    view_parms.name = NULL;
    view_parms.namelen = 0;
    view_parms.errorcode = 0;
    view_parms.check_subtree = 0;

    if (pdu->flags & UCD_MSG_FLAG_ALWAYS_IN_VIEW) {
	/* Enable bypassing of view-based access control */
        return 0;
    }

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
    case SNMP_VERSION_3:
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_ACM_CHECK_INITIAL, &view_parms);
        return view_parms.errorcode;
    }
    return 1;
}

/** Determines if the given PDU request could potentially access
 *   the specified MIB subtree
 *
 * @param pdu     PDU requesting access
 *
 * @param name    The OID to check access for.
 *
 * @param namelen Number of sub-identifiers in the OID.
 *
 * @return gives VACM_SUCCESS   if the entire MIB tree is accessible
 *               VACM_NOTINVIEW if the entire MIB tree is inaccessible
 *               VACM_SUBTREE_UNKNOWN if some portions are accessible
 *               other codes may returned on error
 */
int
netsnmp_acm_check_subtree(netsnmp_pdu *pdu, oid *name, size_t namelen)
{                               /* IN - pdu being checked */
    struct view_parameters view_parms;
    view_parms.pdu = pdu;
    view_parms.name = name;
    view_parms.namelen = namelen;
    view_parms.errorcode = 0;
    view_parms.check_subtree = 1;

    if (pdu->flags & UCD_MSG_FLAG_ALWAYS_IN_VIEW) {
	/* Enable bypassing of view-based access control */
        return 0;
    }

    switch (pdu->version) {
#ifndef NETSNMP_DISABLE_SNMPV1
    case SNMP_VERSION_1:
#endif
#ifndef NETSNMP_DISABLE_SNMPV2C
    case SNMP_VERSION_2c:
#endif
    case SNMP_VERSION_3:
        snmp_call_callbacks(SNMP_CALLBACK_APPLICATION,
                            SNMPD_CALLBACK_ACM_CHECK_SUBTREE, &view_parms);
        return view_parms.errorcode;
    }
    return 1;
}

netsnmp_feature_child_of(get_session_for_oid,netsnmp_unused)
#ifndef NETSNMP_FEATURE_REMOVE_GET_SESSION_FOR_OID
netsnmp_session *
get_session_for_oid(const oid *name, size_t len, const char *context_name)
{
    netsnmp_subtree *myptr;

    myptr = netsnmp_subtree_find_prev(name, len, 
				      netsnmp_subtree_find_first(context_name),
				      context_name);

    while (myptr && myptr->variables == NULL) {
        myptr = myptr->next;
    }

    if (myptr == NULL) {
        return NULL;
    } else {
        return myptr->session;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_GET_SESSION_FOR_OID */

void
setup_tree(void)
{
    oid ccitt[1]           = { 0 };
    oid iso[1]             = { 1 };
    oid joint_ccitt_iso[1] = { 2 };

#ifdef USING_AGENTX_SUBAGENT_MODULE
    int role =  netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
				       NETSNMP_DS_AGENT_ROLE);

    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 
			   MASTER_AGENT);
#endif

    /* 
     * we need to have the oid's in the heap, that we can *free* it for every case, 
     * thats the purpose of the duplicate_objid's
     */
    netsnmp_register_null(snmp_duplicate_objid(ccitt, 1), 1);
    netsnmp_register_null(snmp_duplicate_objid(iso, 1), 1);
    netsnmp_register_null(snmp_duplicate_objid(joint_ccitt_iso, 1), 1);

#ifdef USING_AGENTX_SUBAGENT_MODULE
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 
			   role);
#endif
}

int 
remove_tree_entry (oid *name, size_t len) {

    netsnmp_subtree *sub = NULL;

    if ((sub = netsnmp_subtree_find(name, len, NULL, "")) == NULL) {
	return MIB_NO_SUCH_REGISTRATION;
    }

    return unregister_mib_context(name, len, sub->priority,
				  sub->range_subid, sub->range_ubound, "");

}


void
shutdown_tree(void) {
    oid ccitt[1]           = { 0 };
    oid iso[1]             = { 1 };
    oid joint_ccitt_iso[1] = { 2 };

    DEBUGMSGTL(("agent_registry", "shut down tree\n"));

    remove_tree_entry(joint_ccitt_iso, 1);
    remove_tree_entry(iso, 1);
    remove_tree_entry(ccitt, 1);

}

extern void     dump_idx_registry(void);
void
dump_registry(void)
{
    struct variable *vp = NULL;
    netsnmp_subtree *myptr, *myptr2;
    u_char *s = NULL, *e = NULL, *v = NULL;
    size_t sl = 256, el = 256, vl = 256, sl_o = 0, el_o = 0, vl_o = 0;
    int i = 0;

    if ((s = (u_char *) calloc(sl, 1)) != NULL &&
        (e = (u_char *) calloc(sl, 1)) != NULL &&
        (v = (u_char *) calloc(sl, 1)) != NULL) {

        subtree_context_cache *ptr;
        for (ptr = context_subtrees; ptr; ptr = ptr->next) {
            printf("Subtrees for Context: %s\n", ptr->context_name);
            for (myptr = ptr->first_subtree; myptr != NULL;
                 myptr = myptr->next) {
                sl_o = el_o = vl_o = 0;

                if (!sprint_realloc_objid(&s, &sl, &sl_o, 1,
                                          myptr->start_a,
                                          myptr->start_len)) {
                    break;
                }
                if (!sprint_realloc_objid(&e, &el, &el_o, 1,
                                          myptr->end_a,
					  myptr->end_len)) {
                    break;
                }

                if (myptr->variables) {
                    printf("%02x ( %s - %s ) [", myptr->flags, s, e);
                    for (i = 0, vp = myptr->variables;
                         i < myptr->variables_len; i++) {
                        vl_o = 0;
                        if (!sprint_realloc_objid
                            (&v, &vl, &vl_o, 1, vp->name, vp->namelen)) {
                            break;
                        }
                        printf("%s, ", v);
                        vp = (struct variable *) ((char *) vp +
                                                  myptr->variables_width);
                    }
                    printf("]\n");
                } else {
                    printf("%02x   %s - %s  \n", myptr->flags, s, e);
                }
                for (myptr2 = myptr; myptr2 != NULL;
                     myptr2 = myptr2->children) {
                    if (myptr2->label_a && myptr2->label_a[0]) {
                        if (strcmp(myptr2->label_a, "old_api") == 0) {
                            struct variable *vp =
                                (struct variable*)myptr2->reginfo->handler->myvoid;

                            if (!sprint_realloc_objid(&s, &sl, &sl_o, 1,
                                                 vp->name, vp->namelen)) {
                                continue;
                            }
                            printf("\t%s[%s] %p var %s\n", myptr2->label_a,
                                   myptr2->reginfo->handlerName ?
                                   myptr2->reginfo->handlerName : "no-name",
                                   myptr2->reginfo, s);
                        } else {
                            printf("\t%s %s %p\n", myptr2->label_a,
                                   myptr2->reginfo->handlerName ?
                                   myptr2->reginfo->handlerName : "no-handler-name",
                                   myptr2->reginfo);
                        }
                    }
                }
            }
        }
    }

    SNMP_FREE(s);
    SNMP_FREE(e);
    SNMP_FREE(v);

    dump_idx_registry();
}

/**  @} */
/* End of MIB registration code */


/** @defgroup agent_signals POSIX signals support for agents.
 *     Registering and unregistering signal handlers.
 *   @ingroup agent_registry
 *
 * @{
 */

int             external_signal_scheduled[NUM_EXTERNAL_SIGS];
void            (*external_signal_handler[NUM_EXTERNAL_SIGS]) (int);

#ifndef WIN32

/*
 * TODO: add agent_SIGXXX_handler functions and `case SIGXXX: ...' lines
 *       below for every single that might be handled by register_signal().
 */

RETSIGTYPE
agent_SIGCHLD_handler(int sig)
{
    external_signal_scheduled[SIGCHLD]++;
#ifndef HAVE_SIGACTION
    /*
     * signal() sucks. It *might* have SysV semantics, which means that
     * * a signal handler is reset once it gets called. Ensure that it
     * * remains active.
     */
    signal(SIGCHLD, agent_SIGCHLD_handler);
#endif
}

/** Registers a POSIX Signal handler.
 *  Implements the signal registering process for POSIX and non-POSIX
 *  systems. Also, unifies the way signals work.
 *  Note that the signal handler should register itself again with
 *  signal() call before end of execution to prevent possible problems.
 *
 *  @param sig POSIX Signal ID number, as defined in signal.h.
 *
 *  @param func New signal handler function.
 *
 *  @return value is SIG_REGISTERED_OK for success and
 *        SIG_REGISTRATION_FAILED if the registration can't
 *        be handled.
 */
int
register_signal(int sig, void (*func) (int))
{

    switch (sig) {
#if defined(SIGCHLD)
    case SIGCHLD:
#ifdef HAVE_SIGACTION
        {
            static struct sigaction act;
            act.sa_handler = agent_SIGCHLD_handler;
            sigemptyset(&act.sa_mask);
            act.sa_flags = 0;
            sigaction(SIGCHLD, &act, NULL);
        }
#else
        signal(SIGCHLD, agent_SIGCHLD_handler);
#endif
        break;
#endif
    default:
        snmp_log(LOG_CRIT,
                 "register_signal: signal %d cannot be handled\n", sig);
        return SIG_REGISTRATION_FAILED;
    }

    external_signal_handler[sig] = func;
    external_signal_scheduled[sig] = 0;

    DEBUGMSGTL(("register_signal", "registered signal %d\n", sig));
    return SIG_REGISTERED_OK;
}

/** Unregisters a POSIX Signal handler.
 *
 *  @param sig POSIX Signal ID number, as defined in signal.h.
 *
 *  @return value is SIG_UNREGISTERED_OK for success, or error code.
 */
int
unregister_signal(int sig)
{
    signal(sig, SIG_DFL);
    DEBUGMSGTL(("unregister_signal", "unregistered signal %d\n", sig));
    return SIG_UNREGISTERED_OK;
}

#endif                          /* !WIN32 */

/**  @} */
/* End of signals support code */

/**  @} */

