#ifndef AGENT_INDEX_H
#define AGENT_INDEX_H

#ifdef __cplusplus
extern          "C" {
#endif

#define ALLOCATE_THIS_INDEX		0x0
#define ALLOCATE_ANY_INDEX		0x1
#define ALLOCATE_NEW_INDEX		0x3
        /*
         * N.B: it's deliberate that NEW_INDEX & ANY_INDEX == ANY_INDEX 
         */

#define ANY_INTEGER_INDEX		-1
#define ANY_STRING_INDEX		NULL
#define ANY_OID_INDEX			NULL

#define	INDEX_ERR_GENERR		-1
#define	INDEX_ERR_WRONG_TYPE		-2
#define	INDEX_ERR_NOT_ALLOCATED		-3
#define	INDEX_ERR_WRONG_SESSION		-4

char           *register_string_index(oid *, size_t, char *);
int             register_int_index(oid *, size_t, int);
netsnmp_variable_list *register_oid_index(oid *, size_t, oid *, size_t);
netsnmp_variable_list *register_index(netsnmp_variable_list *, int,
                                      netsnmp_session *);

int             unregister_string_index(oid *, size_t, char *);
int             unregister_int_index(oid *, size_t, int);
int             unregister_oid_index(oid *, size_t, oid *, size_t);

int             release_index(netsnmp_variable_list *);
int             remove_index(netsnmp_variable_list *, netsnmp_session *);
void            unregister_index_by_session(netsnmp_session *);
int             unregister_index(netsnmp_variable_list *, int,
                                 netsnmp_session *);

unsigned long   count_indexes(oid * name, size_t namelen,
                              int include_unallocated);

#ifdef __cplusplus
}
#endif
#endif                          /* AGENT_INDEX_H */
