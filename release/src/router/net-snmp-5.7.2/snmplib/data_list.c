/*
 * netsnmp_data_list.c
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

netsnmp_feature_child_of(data_list_all, libnetsnmp)

netsnmp_feature_child_of(data_list_add_data, data_list_all)
netsnmp_feature_child_of(data_list_get_list_node, data_list_all)

/** @defgroup data_list generic linked-list data handling with a string as a key.
 * @ingroup library
 * @{
*/

/** frees the data and a name at a given data_list node.
 * Note that this doesn't free the node itself.
 * @param node the node for which the data should be freed
 */
NETSNMP_INLINE void
netsnmp_free_list_data(netsnmp_data_list *node)
{
    Netsnmp_Free_List_Data *beer;
    if (!node)
        return;

    beer = node->free_func;
    if (beer)
        (beer) (node->data);
    SNMP_FREE(node->name);
}

/** frees all data and nodes in a list.
 * @param head the top node of the list to be freed.
 */
NETSNMP_INLINE void
netsnmp_free_all_list_data(netsnmp_data_list *head)
{
    netsnmp_data_list *tmpptr;
    for (; head;) {
        netsnmp_free_list_data(head);
        tmpptr = head;
        head = head->next;
        SNMP_FREE(tmpptr);
    }
}

/** adds creates a data_list node given a name, data and a free function ptr.
 * @param name the name of the node to cache the data.
 * @param data the data to be stored under that name
 * @param beer A function that can free the data pointer (in the future)
 * @return a newly created data_list node which can be given to the netsnmp_add_list_data function.
 */
NETSNMP_INLINE netsnmp_data_list *
netsnmp_create_data_list(const char *name, void *data,
                         Netsnmp_Free_List_Data * beer)
{
    netsnmp_data_list *node;
    
    if (!name)
        return NULL;
    node = SNMP_MALLOC_TYPEDEF(netsnmp_data_list);
    if (!node)
        return NULL;
    node->name = strdup(name);
    if (!node->name) {
        free(node);
        return NULL;
    }
    node->data = data;
    node->free_func = beer;
    return node;
}

/** adds data to a datalist
 * @param head a pointer to the head node of a data_list
 * @param node a node to stash in the data_list
 */
NETSNMP_INLINE void
netsnmp_data_list_add_node(netsnmp_data_list **head, netsnmp_data_list *node)
{
    netsnmp_data_list *ptr;

    netsnmp_assert(NULL != head);
    netsnmp_assert(NULL != node);
    netsnmp_assert(NULL != node->name);

    DEBUGMSGTL(("data_list","adding key '%s'\n", node->name));

    if (!*head) {
        *head = node;
        return;
    }

    if (0 == strcmp(node->name, (*head)->name)) {
        netsnmp_assert(!"list key == is unique"); /* always fail */
        snmp_log(LOG_WARNING,
                 "WARNING: adding duplicate key '%s' to data list\n",
                 node->name);
    }

    for (ptr = *head; ptr->next != NULL; ptr = ptr->next) {
        netsnmp_assert(NULL != ptr->name);
        if (0 == strcmp(node->name, ptr->name)) {
            netsnmp_assert(!"list key == is unique"); /* always fail */
            snmp_log(LOG_WARNING,
                     "WARNING: adding duplicate key '%s' to data list\n",
                     node->name);
        }
    }

    netsnmp_assert(NULL != ptr);
    if (ptr)                    /* should always be true */
        ptr->next = node;
}

/** adds data to a datalist
 * @note netsnmp_data_list_add_node is preferred
 * @param head a pointer to the head node of a data_list
 * @param node a node to stash in the data_list
 */
/**  */
NETSNMP_INLINE void
netsnmp_add_list_data(netsnmp_data_list **head, netsnmp_data_list *node)
{
    netsnmp_data_list_add_node(head, node);
}

/** adds data to a datalist
 * @param head a pointer to the head node of a data_list
 * @param name the name of the node to cache the data.
 * @param data the data to be stored under that name
 * @param beer A function that can free the data pointer (in the future)
 * @return a newly created data_list node which was inserted in the list
 */
#ifndef NETSNMP_FEATURE_REMOVE_DATA_LIST_ADD_DATA
NETSNMP_INLINE netsnmp_data_list *
netsnmp_data_list_add_data(netsnmp_data_list **head, const char *name,
                           void *data, Netsnmp_Free_List_Data * beer)
{
    netsnmp_data_list *node;
    if (!name) {
        snmp_log(LOG_ERR,"no name provided.");
        return NULL;
    }
    node = netsnmp_create_data_list(name, data, beer);
    if(NULL == node) {
        snmp_log(LOG_ERR,"could not allocate memory for node.");
        return NULL;
    }
    
    netsnmp_add_list_data(head, node);

    return node;
}
#endif /* NETSNMP_FEATURE_REMOVE_DATA_LIST_ADD_DATA */

/** returns a data_list node's data for a given name within a data_list
 * @param head the head node of a data_list
 * @param name the name to find
 * @return a pointer to the data cached at that node
 */
NETSNMP_INLINE void    *
netsnmp_get_list_data(netsnmp_data_list *head, const char *name)
{
    if (!name)
        return NULL;
    for (; head; head = head->next)
        if (head->name && strcmp(head->name, name) == 0)
            break;
    if (head)
        return head->data;
    return NULL;
}

/** returns a data_list node for a given name within a data_list
 * @param head the head node of a data_list
 * @param name the name to find
 * @return a pointer to the data_list node
 */
#ifndef NETSNMP_FEATURE_REMOVE_DATA_LIST_GET_LIST_NODE
NETSNMP_INLINE netsnmp_data_list    *
netsnmp_get_list_node(netsnmp_data_list *head, const char *name)
{
    if (!name)
        return NULL;
    for (; head; head = head->next)
        if (head->name && strcmp(head->name, name) == 0)
            break;
    if (head)
        return head;
    return NULL;
}
#endif /* NETSNMP_FEATURE_REMOVE_DATA_LIST_GET_LIST_NODE */

/** Removes a named node from a data_list (and frees it)
 * @param realhead a pointer to the head node of a data_list
 * @param name the name to find and remove
 * @return 0 on successful find-and-delete, 1 otherwise.
 */
int
netsnmp_remove_list_node(netsnmp_data_list **realhead, const char *name)
{
    netsnmp_data_list *head, *prev;
    if (!name)
        return 1;
    for (head = *realhead, prev = NULL; head;
         prev = head, head = head->next) {
        if (head->name && strcmp(head->name, name) == 0) {
            if (prev)
                prev->next = head->next;
            else
                *realhead = head->next;
            netsnmp_free_list_data(head);
            free(head);
            return 0;
        }
    }
    return 1;
}

/** used to store registered save/parse handlers (specifically, parsing info) */
static netsnmp_data_list *saveHead;

/** registers to store a data_list set of data at persistent storage time
 *
 * @param datalist the data to be saved
 * @param type the name of the application to save the data as.  If left NULL the default application name that was registered during the init_snmp call will be used (recommended).
 * @param token the unique token identifier string to use as the first word in the persistent file line.
 * @param data_list_save_ptr a function pointer which will be called to save the rest of the data to a buffer.
 * @param data_list_read_ptr a function pointer which can read the remainder of a saved line and return the application specific void * pointer.
 * @param data_list_free_ptr a function pointer which will be passed to the data node for freeing it in the future when/if the list/node is cleaned up or destroyed.
 */
void
netsnmp_register_save_list(netsnmp_data_list **datalist,
                           const char *type, const char *token,
                           Netsnmp_Save_List_Data *data_list_save_ptr,
                           Netsnmp_Read_List_Data *data_list_read_ptr,
                           Netsnmp_Free_List_Data *data_list_free_ptr)
{
    netsnmp_data_list_saveinfo *info;

    if (!data_list_save_ptr && !data_list_read_ptr)
        return;

    info = SNMP_MALLOC_TYPEDEF(netsnmp_data_list_saveinfo);

    if (!info) {
        snmp_log(LOG_ERR, "couldn't malloc a netsnmp_data_list_saveinfo typedef");
        return;
    }

    info->datalist = datalist;
    info->token = token;
    info->type = type;
    if (!info->type) {
        info->type = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                           NETSNMP_DS_LIB_APPTYPE);
    }

    /* function which will save the data */
    info->data_list_save_ptr = data_list_save_ptr;
    if (data_list_save_ptr)
        snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                               netsnmp_save_all_data_callback, info);

    /* function which will read the data back in */
    info->data_list_read_ptr = data_list_read_ptr;
    if (data_list_read_ptr) {
        /** @todo netsnmp_register_save_list should handle the same token name being saved from different types? */
        netsnmp_add_list_data(&saveHead,
                              netsnmp_create_data_list(token, info, NULL));
        register_config_handler(type, token, netsnmp_read_data_callback,
                                NULL /* XXX */, NULL);
    }

    info->data_list_free_ptr = data_list_free_ptr;
}


/** intended to be registerd as a callback operation.
 * It should be registered using:
 *
 * snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA, netsnmp_save_all_data_callback, INFO_POINTER);
 *
 * where INFO_POINTER is a pointer to a netsnmp_data_list_saveinfo object containing apporpriate registration information
 */
int
netsnmp_save_all_data_callback(int major, int minor,
                               void *serverarg, void *clientarg) {
    netsnmp_data_list_saveinfo *info = (netsnmp_data_list_saveinfo *)clientarg;

    if (!clientarg) {
        snmp_log(LOG_WARNING, "netsnmp_save_all_data_callback called with no passed data");
        return SNMP_ERR_NOERROR;
    }

    netsnmp_save_all_data(*(info->datalist), info->type, info->token,
                          info->data_list_save_ptr);
    return SNMP_ERR_NOERROR;
}    

/** intended to be called as a callback during persistent save operations.
 * See the netsnmp_save_all_data_callback for where this is typically used. */
int
netsnmp_save_all_data(netsnmp_data_list *head,
                      const char *type, const char *token,
                      Netsnmp_Save_List_Data * data_list_save_ptr)
{
    char buf[SNMP_MAXBUF], *cp;

    for (; head; head = head->next) {
        if (head->name) {
            /* save begining of line */
            snprintf(buf, sizeof(buf), "%s ", token);
            cp = buf + strlen(buf);
            cp = read_config_save_octet_string(cp, (u_char*)head->name,
                                               strlen(head->name));
            *cp++ = ' ';

            /* call registered function to save the rest */
            if (!(data_list_save_ptr)(cp,
                                      sizeof(buf) - strlen(buf),
                                      head->data)) {
                read_config_store(type, buf);
            }
        }
    }
    return SNMP_ERR_NOERROR;
}

/** intended to be registerd as a .conf parser
 * It should be registered using:
 *
 * register_app_config_handler("token", netsnmp_read_data_callback, XXX)
 *
 * where INFO_POINTER is a pointer to a netsnmp_data_list_saveinfo object
 * containing apporpriate registration information
 * @todo make netsnmp_read_data_callback deal with a free routine
 */
void
netsnmp_read_data_callback(const char *token, char *line) {
    netsnmp_data_list_saveinfo *info;
    char *dataname = NULL;
    size_t dataname_len;
    void *data = NULL;

    /* find the stashed information about what we're parsing */
    info = (netsnmp_data_list_saveinfo *) netsnmp_get_list_data(saveHead, token);
    if (!info) {
        snmp_log(LOG_WARNING, "netsnmp_read_data_callback called without previously registered subparser");
        return;
    }

    /* read in the token */
    line =
        read_config_read_data(ASN_OCTET_STR, line,
                              &dataname, &dataname_len);

    if (!line || !dataname)
        return;

    /* call the sub-parser to read the rest */
    data = (info->data_list_read_ptr)(line, strlen(line));

    if (!data) {
        free(dataname);
        return;
    }

    /* add to the datalist */
    netsnmp_add_list_data(info->datalist,
                          netsnmp_create_data_list(dataname, data,
                                                   info->data_list_free_ptr));

    return;
}
/**  @} */

