#ifndef OID_STASH_H
#define OID_STASH_H

/*
 * designed to store/retrieve information associated with a given oid.
 * Storage is done in an efficient tree manner for fast lookups.
 */

#define OID_STASH_CHILDREN_SIZE 31

#ifdef __cplusplus
extern          "C" {
#endif

    struct netsnmp_oid_stash_node_s;

    /* args: buffer, sizeof(buffer), yourdata, stashnode */
    typedef int     (NetSNMPStashDump) (char *, size_t,
                                        void *,
                                        struct netsnmp_oid_stash_node_s *);

    typedef void    (NetSNMPStashFreeNode) (void *);
    
    typedef struct netsnmp_oid_stash_node_s {
        oid             value;
        struct netsnmp_oid_stash_node_s **children;     /* array of children */
        size_t          children_size;
        struct netsnmp_oid_stash_node_s *next_sibling;  /* cache too small links */
        struct netsnmp_oid_stash_node_s *prev_sibling;
        struct netsnmp_oid_stash_node_s *parent; 

        void           *thedata;
    } netsnmp_oid_stash_node;

    typedef struct netsnmp_oid_stash_save_info_s {
       const char *token;
       netsnmp_oid_stash_node **root;
       NetSNMPStashDump *dumpfn;
    } netsnmp_oid_stash_save_info;

    NETSNMP_IMPORT
    int             netsnmp_oid_stash_add_data(netsnmp_oid_stash_node **root,
					       const oid * lookup,
                                               size_t lookup_len,
                                               void *mydata);
    SNMPCallback netsnmp_oid_stash_store_all;


    netsnmp_oid_stash_node
        *netsnmp_oid_stash_get_node(netsnmp_oid_stash_node *root,
                                    const oid * lookup, size_t lookup_len);
    NETSNMP_IMPORT
    void           *netsnmp_oid_stash_get_data(netsnmp_oid_stash_node *root,
					       const oid * lookup,
                                               size_t lookup_len);
    NETSNMP_IMPORT
    netsnmp_oid_stash_node *
    netsnmp_oid_stash_getnext_node(netsnmp_oid_stash_node *root,
                                   oid * lookup, size_t lookup_len);

    netsnmp_oid_stash_node *netsnmp_oid_stash_create_sized_node(size_t
                                                                mysize);
    netsnmp_oid_stash_node *netsnmp_oid_stash_create_node(void);        /* returns a malloced node */

    void netsnmp_oid_stash_store(netsnmp_oid_stash_node *root,
                                 const char *tokenname,
                                 NetSNMPStashDump *dumpfn,
                                 oid *curoid, size_t curoid_len);

    /* frees all data in the stash and cleans it out.  Sets root = NULL */
    NETSNMP_IMPORT
    void netsnmp_oid_stash_free(netsnmp_oid_stash_node **root,
                                NetSNMPStashFreeNode *freefn);
                                

    /* a noop function that can be passed to netsnmp_oid_stash_node to
       NOT free the data */
    NetSNMPStashFreeNode netsnmp_oid_stash_no_free;
#ifdef __cplusplus
}
#endif
#endif                          /* OID_STASH_H */
