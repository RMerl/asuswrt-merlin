#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif
#include <sys/types.h>

#include <net-snmp/types.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_enum.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/snmp_assert.h>

netsnmp_feature_child_of(snmp_enum_all, libnetsnmp)

netsnmp_feature_child_of(se_find_free_value_in_slist, snmp_enum_all)
netsnmp_feature_child_of(snmp_enum_store_list, snmp_enum_all)
netsnmp_feature_child_of(snmp_enum_store_slist, snmp_enum_all)
netsnmp_feature_child_of(snmp_enum_clear, snmp_enum_all)

struct snmp_enum_list_str {
    char           *name;
    struct snmp_enum_list *list;
    struct snmp_enum_list_str *next;
};

static struct snmp_enum_list ***snmp_enum_lists;
unsigned int    current_maj_num;
unsigned int    current_min_num;
static struct snmp_enum_list_str *sliststorage;

static void
free_enum_list(struct snmp_enum_list *list);

int
init_snmp_enum(const char *type)
{
    int             i;

    if (NULL != snmp_enum_lists)
        return SE_OK;

    snmp_enum_lists = (struct snmp_enum_list ***)
        calloc(1, sizeof(struct snmp_enum_list **) * SE_MAX_IDS);
    if (!snmp_enum_lists)
        return SE_NOMEM;
    current_maj_num = SE_MAX_IDS;

    for (i = 0; i < SE_MAX_IDS; i++) {
        if (!snmp_enum_lists[i])
            snmp_enum_lists[i] = (struct snmp_enum_list **)
                calloc(1, sizeof(struct snmp_enum_list *) * SE_MAX_SUBIDS);
        if (!snmp_enum_lists[i])
            return SE_NOMEM;
    }
    current_min_num = SE_MAX_SUBIDS;

    register_config_handler(type, "enum", se_read_conf, NULL, NULL);
    return SE_OK;
}

int
se_store_in_list(struct snmp_enum_list *new_list,
              unsigned int major, unsigned int minor)
{
    int             ret = SE_OK;

    if (major > current_maj_num || minor > current_min_num) {
        /*
         * XXX: realloc 
         */
        return SE_NOMEM;
    }
    netsnmp_assert(NULL != snmp_enum_lists);

    if (snmp_enum_lists[major][minor] != NULL)
        ret = SE_ALREADY_THERE;

    snmp_enum_lists[major][minor] = new_list;

    return ret;
}

void
se_read_conf(const char *word, char *cptr)
{
    int major, minor;
    int value;
    char *cp, *cp2;
    char e_name[BUFSIZ];
    char e_enum[  BUFSIZ];

    if (!cptr || *cptr=='\0')
        return;

    /*
     * Extract the first token
     *   (which should be the name of the list)
     */
    cp = copy_nword(cptr, e_name, sizeof(e_name));
    cp = skip_white(cp);
    if (!cp || *cp=='\0')
        return;


    /*
     * Add each remaining enumeration to the list,
     *   using the appropriate style interface
     */
    if (sscanf(e_name, "%d:%d", &major, &minor) == 2) {
        /*
         *  Numeric major/minor style
         */
        while (1) {
            cp = copy_nword(cp, e_enum, sizeof(e_enum));
            if (sscanf(e_enum, "%d:", &value) != 1) {
                break;
            }
            cp2 = e_enum;
            while (*(cp2++) != ':')
                ;
            se_add_pair(major, minor, strdup(cp2), value);
            if (!cp)
                break;
        }
    } else {
        /*
         *  Named enumeration
         */
        while (1) {
            cp = copy_nword(cp, e_enum, sizeof(e_enum));
            if (sscanf(e_enum, "%d:", &value) != 1) {
                break;
            }
            cp2 = e_enum;
            while (*(cp2++) != ':')
                ;
            se_add_pair_to_slist(e_name, strdup(cp2), value);
            if (!cp)
                break;
        }
    }
}

void
se_store_enum_list(struct snmp_enum_list *new_list,
                   const char *token, const char *type)
{
    struct snmp_enum_list *listp = new_list;
    char line[2048];
    char buf[512];
    int  len;

    snprintf(line, sizeof(line), "enum %s", token);
    while (listp) {
        snprintf(buf, sizeof(buf), " %d:%s", listp->value, listp->label);
        /*
         * Calculate the space left in the buffer.
         * If this is not sufficient to include the next enum,
         *   then save the line so far, and start again.
         */
	len = sizeof(line) - strlen(line);
	if ((int)strlen(buf) > len) {
	    read_config_store(type, line);
            snprintf(line, sizeof(line), "enum %s", token);
	    len = sizeof(line) - strlen(line);
	}

	strncat(line, buf, len);
        listp = listp->next;
    }

    read_config_store(type, line);
}

#ifndef NETSNMP_FEATURE_REMOVE_SNMP_ENUM_STORE_LIST
void
se_store_list(unsigned int major, unsigned int minor, const char *type)
{
    char token[32];

    snprintf(token, sizeof(token), "%d:%d", major, minor);
    se_store_enum_list(se_find_list(major, minor), token, type);
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_ENUM_STORE_LIST */

struct snmp_enum_list *
se_find_list(unsigned int major, unsigned int minor)
{
    if (major > current_maj_num || minor > current_min_num)
        return NULL;
    netsnmp_assert(NULL != snmp_enum_lists);

    return snmp_enum_lists[major][minor];
}

int
se_find_value_in_list(struct snmp_enum_list *list, const char *label)
{
    if (!list)
        return SE_DNE;          /* XXX: um, no good solution here */
    while (list) {
        if (strcmp(list->label, label) == 0)
            return (list->value);
        list = list->next;
    }

    return SE_DNE;              /* XXX: um, no good solution here */
}

int
se_find_free_value_in_list(struct snmp_enum_list *list)
{
    int max_value = 0;
    if (!list)
        return SE_DNE;

    for (;list; list=list->next) {
        if (max_value < list->value)
            max_value = list->value;
    }
    return max_value+1;
}

int
se_find_value(unsigned int major, unsigned int minor, const char *label)
{
    return se_find_value_in_list(se_find_list(major, minor), label);
}

int
se_find_free_value(unsigned int major, unsigned int minor)
{
    return se_find_free_value_in_list(se_find_list(major, minor));
}

char           *
se_find_label_in_list(struct snmp_enum_list *list, int value)
{
    if (!list)
        return NULL;
    while (list) {
        if (list->value == value)
            return (list->label);
        list = list->next;
    }
    return NULL;
}

char           *
se_find_label(unsigned int major, unsigned int minor, int value)
{
    return se_find_label_in_list(se_find_list(major, minor), value);
}

int
se_add_pair_to_list(struct snmp_enum_list **list, char *label, int value)
{
    struct snmp_enum_list *lastnode = NULL, *tmp;

    if (!list)
        return SE_DNE;

    tmp = *list;
    while (tmp) {
        if (tmp->value == value) {
            free(label);
            return (SE_ALREADY_THERE);
        }
        lastnode = tmp;
        tmp = tmp->next;
    }

    if (lastnode) {
        lastnode->next = SNMP_MALLOC_STRUCT(snmp_enum_list);
        lastnode = lastnode->next;
    } else {
        (*list) = SNMP_MALLOC_STRUCT(snmp_enum_list);
        lastnode = (*list);
    }
    if (!lastnode) {
        free(label);
        return (SE_NOMEM);
    }
    lastnode->label = label;
    lastnode->value = value;
    lastnode->next = NULL;
    return (SE_OK);
}

int
se_add_pair(unsigned int major, unsigned int minor, char *label, int value)
{
    struct snmp_enum_list *list = se_find_list(major, minor);
    int             created = (list) ? 1 : 0;
    int             ret = se_add_pair_to_list(&list, label, value);
    if (!created)
        se_store_in_list(list, major, minor);
    return ret;
}

/*
 * remember a list of enums based on a lookup name.
 */
static struct snmp_enum_list **
se_find_slist_ptr(const char *listname)
{
    struct snmp_enum_list_str *sptr;
    if (!listname)
        return NULL;

    for (sptr = sliststorage; sptr != NULL; sptr = sptr->next)
        if (sptr->name && strcmp(sptr->name, listname) == 0)
            return &sptr->list;

    return NULL;
}

struct snmp_enum_list *
se_find_slist(const char *listname)
{
    struct snmp_enum_list **ptr = se_find_slist_ptr(listname);
    return ptr ? *ptr : NULL;
}

char           *
se_find_label_in_slist(const char *listname, int value)
{
    return (se_find_label_in_list(se_find_slist(listname), value));
}

int
se_find_value_in_slist(const char *listname, const char *label)
{
    return (se_find_value_in_list(se_find_slist(listname), label));
}

#ifndef NETSNMP_FEATURE_REMOVE_SE_FIND_FREE_VALUE_IN_SLIST
int
se_find_free_value_in_slist(const char *listname)
{
    return (se_find_free_value_in_list(se_find_slist(listname)));
}
#endif /* NETSNMP_FEATURE_REMOVE_SE_FIND_FREE_VALUE_IN_SLIST */

int
se_add_pair_to_slist(const char *listname, char *label, int value)
{
    struct snmp_enum_list *list = se_find_slist(listname);
    int             created = (list) ? 1 : 0;
    int             ret = se_add_pair_to_list(&list, label, value);

    if (!created) {
        struct snmp_enum_list_str *sptr =
            SNMP_MALLOC_STRUCT(snmp_enum_list_str);
        if (!sptr) {
            free_enum_list(list);
            return SE_NOMEM;
        }
        sptr->next = sliststorage;
        sptr->name = strdup(listname);
        sptr->list = list;
        sliststorage = sptr;
    }
    return ret;
}

static void
free_enum_list(struct snmp_enum_list *list)
{
    struct snmp_enum_list *next;

    while (list) {
        next = list->next;
        SNMP_FREE(list->label);
        SNMP_FREE(list);
        list = next;
    }
}

void
clear_snmp_enum(void)
{
    struct snmp_enum_list_str *sptr = sliststorage, *next = NULL;
    int i, j;

    while (sptr != NULL) {
	next = sptr->next;
	free_enum_list(sptr->list);
	SNMP_FREE(sptr->name);
	SNMP_FREE(sptr);
	sptr = next;
    }
    sliststorage = NULL;

    if (snmp_enum_lists) {
        for (i = 0; i < SE_MAX_IDS; i++) {
            if (snmp_enum_lists[i]) {
                for (j = 0; j < SE_MAX_SUBIDS; j++) {
                    if (snmp_enum_lists[i][j])
                        free_enum_list(snmp_enum_lists[i][j]);
                }
                SNMP_FREE(snmp_enum_lists[i]);
            }
        }
        SNMP_FREE(snmp_enum_lists);
    }
}

void
se_clear_list(struct snmp_enum_list **list)
{
    struct snmp_enum_list *this_entry, *next_entry;

    if (!list)
        return;

    this_entry = *list;
    while (this_entry) {
        next_entry = this_entry->next;
        SNMP_FREE(this_entry->label);
        SNMP_FREE(this_entry);
        this_entry = next_entry;
    }
    *list = NULL;
    return;
}

#ifndef NETSNMP_FEATURE_REMOVE_SNMP_ENUM_STORE_SLIST
void
se_store_slist(const char *listname, const char *type)
{
    struct snmp_enum_list *list = se_find_slist(listname);
    se_store_enum_list(list, listname, type);
}

int
se_store_slist_callback(int majorID, int minorID,
                        void *serverargs, void *clientargs)
{
    char *appname = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                          NETSNMP_DS_LIB_APPTYPE);
    se_store_slist((char *)clientargs, appname);
    return SNMPERR_SUCCESS;
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_ENUM_STORE_SLIST */

#ifndef NETSNMP_FEATURE_REMOVE_SNMP_ENUM_CLEAR
void
se_clear_slist(const char *listname)
{
    se_clear_list(se_find_slist_ptr(listname));
}

void
se_clear_all_lists(void)
{
    struct snmp_enum_list_str *sptr = NULL;

    for (sptr = sliststorage; sptr != NULL; sptr = sptr->next)
        se_clear_list(&(sptr->list));
}
#endif /* NETSNMP_FEATURE_REMOVE_SNMP_ENUM_CLEAR */
