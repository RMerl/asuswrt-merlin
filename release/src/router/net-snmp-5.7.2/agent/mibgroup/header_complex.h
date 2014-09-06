/*
 *  header_complex.h:  More complex storage and data sorting for mib modules
 */
#ifndef _MIBGROUP_HEADER_COMPLEX_H
#define _MIBGROUP_HEADER_COMPLEX_H

struct header_complex_index {
    oid            *name;
    size_t          namelen;
    void           *data;
    struct header_complex_index *next;
    struct header_complex_index *prev;
};

/*
 * Function pointer called by the header_comlpex functions when a client pointer (void * to us) needs to be cleaned. 
 */

typedef void    (HeaderComplexCleaner) (void *);
void           *header_complex(struct header_complex_index *datalist,
                               struct variable *vp, oid * name,
                               size_t * length, int exact,
                               size_t * var_len,
                               WriteMethod ** write_method);

int             header_complex_generate_varoid(netsnmp_variable_list *
                                               var);
int             header_complex_parse_oid(oid * oidIndex, size_t oidLen,
                                         netsnmp_variable_list * data);
void            header_complex_generate_oid(oid * name, size_t * length,
                                            oid * prefix,
                                            size_t prefix_len,
                                            netsnmp_variable_list * data);
void            header_complex_free_all(struct header_complex_index
                                        *thestuff,
                                        HeaderComplexCleaner * cleaner);
void            header_complex_free_entry(struct header_complex_index
                                          *theentry,
                                          HeaderComplexCleaner * cleaner);
void           *header_complex_extract_entry(struct header_complex_index
                                             **thetop,
                                             struct header_complex_index
                                             *thespot);
struct header_complex_index *header_complex_find_entry(struct
                                                       header_complex_index
                                                       *thestuff,
                                                       void *entry);

void           *header_complex_get(struct header_complex_index *datalist,
                                   netsnmp_variable_list * index);
void           *header_complex_get_from_oid(struct header_complex_index
                                            *datalist, oid * searchfor,
                                            size_t searchfdor_len);

struct header_complex_index *header_complex_add_data(struct
                                                     header_complex_index
                                                     **thedata,
                                                     netsnmp_variable_list
                                                     * var, void *data);
struct header_complex_index *header_complex_maybe_add_data(struct header_complex_index **thedata,
                                                           netsnmp_variable_list * var,
                                                           void *data,
                                                           int dont_allow_duplicates);

/*
 * Note: newoid is copied/cloned for you 
 */
struct header_complex_index *header_complex_add_data_by_oid(struct
                                                            header_complex_index
                                                            **thedata,
                                                            oid * newoid,
                                                            size_t
                                                            newoid_len,
                                                            void *data);
struct header_complex_index *header_complex_maybe_add_data_by_oid(struct header_complex_index **thedata,
                                                                  oid * newoid,
                                                                  size_t newoid_len,
                                                                  void *data,
                                                                  int dont_allow_duplicates);

#endif                          /* _MIBGROUP_HEADER_COMPLEX_H */
