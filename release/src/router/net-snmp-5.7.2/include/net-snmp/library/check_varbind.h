#ifndef SNMP_CHECK_VARBIND_H
#define SNMP_CHECK_VARBIND_H

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * Assorted convience routines to check the contents of a
     * netsnmp_variable_list instance.
     */

    int netsnmp_check_vb_type(const netsnmp_variable_list *var, int type);
    int netsnmp_check_vb_size(const netsnmp_variable_list *var, size_t size );
    int netsnmp_check_vb_max_size(const netsnmp_variable_list *var, size_t size );
    int netsnmp_check_vb_range(const netsnmp_variable_list *var,
                               size_t low, size_t high );
    int netsnmp_check_vb_size_range(const netsnmp_variable_list *var,
                                    size_t low, size_t high );

    NETSNMP_IMPORT
    int netsnmp_check_vb_type_and_size(const netsnmp_variable_list *var,
                                    int type, size_t size);
    NETSNMP_IMPORT
    int netsnmp_check_vb_type_and_max_size(const netsnmp_variable_list *var,
                                    int type, size_t size);

    NETSNMP_IMPORT
    int netsnmp_check_vb_oid(const netsnmp_variable_list *var);
    NETSNMP_IMPORT
    int netsnmp_check_vb_int(const netsnmp_variable_list *var);
    NETSNMP_IMPORT
    int netsnmp_check_vb_uint(const netsnmp_variable_list *var);
    NETSNMP_IMPORT
    int netsnmp_check_vb_int_range(const netsnmp_variable_list *var, int low,
                                   int high);

    NETSNMP_IMPORT
    int netsnmp_check_vb_truthvalue(const netsnmp_variable_list *var);

    NETSNMP_IMPORT
    int netsnmp_check_vb_rowstatus_value(const netsnmp_variable_list *var);
    NETSNMP_IMPORT
    int netsnmp_check_vb_rowstatus(const netsnmp_variable_list *var, int old_val);
    int netsnmp_check_vb_rowstatus_with_storagetype(const netsnmp_variable_list *var, int old_val, int old_storage);

    int netsnmp_check_vb_storagetype(const netsnmp_variable_list *var, int old_val);


#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_CHECK_VARBIND_H */
