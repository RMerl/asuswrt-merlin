#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/library/check_varbind.h>

netsnmp_feature_child_of(check_varbind_all, libnetsnmp)

netsnmp_feature_child_of(check_vb_range, check_varbind_all)
netsnmp_feature_child_of(check_vb_size_range, check_varbind_all)
netsnmp_feature_child_of(check_vb_uint, check_varbind_all)
netsnmp_feature_child_of(check_vb_storagetype, check_varbind_all)
netsnmp_feature_child_of(check_vb_oid, check_varbind_all)
netsnmp_feature_child_of(check_vb_type_and_max_size, check_varbind_all)
netsnmp_feature_child_of(check_vb_type_and_max_size, check_varbind_all)
netsnmp_feature_child_of(check_vb_rowstatus_with_storagetype, check_varbind_all)
netsnmp_feature_child_of(check_vb_truthvalue, check_varbind_all)

#ifdef NETSNMP_FEATURE_REQUIRE_CHECK_VB_ROWSTATUS_WITH_STORAGETYPE
netsnmp_feature_require(check_rowstatus_with_storagetype_transition)
#endif /* NETSNMP_FEATURE_REQUIRES_CHECK_VB_ROWSTATUS_WITH_STORAGETYPE */


NETSNMP_INLINE int
netsnmp_check_vb_type(const netsnmp_variable_list *var, int type )
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if (var->type != type) {
        rc = SNMP_ERR_WRONGTYPE;
    }

    return rc;
}

NETSNMP_INLINE int
netsnmp_check_vb_size(const netsnmp_variable_list *var, size_t size )
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    else if (var->val_len != size) {
        rc = SNMP_ERR_WRONGLENGTH;
    }

    return rc;
}

NETSNMP_INLINE int
netsnmp_check_vb_max_size(const netsnmp_variable_list *var, size_t size )
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    else if (var->val_len > size) {
        rc = SNMP_ERR_WRONGLENGTH;
    }

    return rc;
}

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_RANGE
NETSNMP_INLINE int
netsnmp_check_vb_range(const netsnmp_variable_list *var,
                       size_t low, size_t high )
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if (((size_t)*var->val.integer < low) || ((size_t)*var->val.integer > high)) {
        rc = SNMP_ERR_WRONGVALUE;
    }

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_RANGE */

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_SIZE_RANGE
NETSNMP_INLINE int
netsnmp_check_vb_size_range(const netsnmp_variable_list *var,
                            size_t low, size_t high )
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((var->val_len < low) || (var->val_len > high)) {
        rc = SNMP_ERR_WRONGLENGTH;
    }

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_SIZE_RANGE */

NETSNMP_INLINE int
netsnmp_check_vb_type_and_size(const netsnmp_variable_list *var,
                               int type, size_t size)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type(var,type)))
        ;
    else
        rc = netsnmp_check_vb_size(var, size);

    return rc;
}

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_TYPE_AND_MAX_SIZE
NETSNMP_INLINE int
netsnmp_check_vb_type_and_max_size(const netsnmp_variable_list *var,
                               int type, size_t size)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type(var,type)))
        ;
    else
        rc = netsnmp_check_vb_max_size(var, size);

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_TYPE_AND_MAX_SIZE */

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_OID
NETSNMP_INLINE int
netsnmp_check_vb_oid(const netsnmp_variable_list *var)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type(var,ASN_OBJECT_ID)))
        ;
    else
        rc = netsnmp_check_vb_max_size(var, MAX_OID_LEN*sizeof(oid));

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_OID */

NETSNMP_INLINE int
netsnmp_check_vb_int(const netsnmp_variable_list *var)
{
    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    return netsnmp_check_vb_type_and_size(var, ASN_INTEGER, sizeof(long));
}

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_UINT
NETSNMP_INLINE int
netsnmp_check_vb_uint(const netsnmp_variable_list *var)
{
    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    return netsnmp_check_vb_type_and_size(var, ASN_UNSIGNED, sizeof(long));
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_UINT */

NETSNMP_INLINE int
netsnmp_check_vb_int_range(const netsnmp_variable_list *var, int low, int high)
{
    register int rc = SNMP_ERR_NOERROR;
    
    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type_and_size(var, ASN_INTEGER, sizeof(long))))
        return rc;
    
    if ((*var->val.integer < low) || (*var->val.integer > high)) {
        rc = SNMP_ERR_WRONGVALUE;
    }

    return rc;
}

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_TRUTHVALUE
int
netsnmp_check_vb_truthvalue(const netsnmp_variable_list *var)
{
    register int rc = SNMP_ERR_NOERROR;
    
    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type_and_size(var, ASN_INTEGER, sizeof(long))))
        return rc;
    
    return netsnmp_check_vb_int_range(var, 1, 2);
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_TRUTHVALUE */

NETSNMP_INLINE int
netsnmp_check_vb_rowstatus_value(const netsnmp_variable_list *var)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type_and_size(var, ASN_INTEGER, sizeof(long))))
        return rc;
    
    if (*var->val.integer == RS_NOTREADY)
        return SNMP_ERR_WRONGVALUE;

    return netsnmp_check_vb_int_range(var, SNMP_ROW_NONEXISTENT,
                                      SNMP_ROW_DESTROY);
}

int
netsnmp_check_vb_rowstatus(const netsnmp_variable_list *var, int old_value)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_rowstatus_value(var)))
        return rc;

    return check_rowstatus_transition(old_value, *var->val.integer);
}

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_ROWSTATUS_WITH_STORAGETYPE
int
netsnmp_check_vb_rowstatus_with_storagetype(const netsnmp_variable_list *var,
                                            int old_value, int old_storage)
{
    register int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;

    if ((rc = netsnmp_check_vb_rowstatus_value(var)))
        return rc;

    return check_rowstatus_with_storagetype_transition(old_value,
                                                       *var->val.integer,
                                                       old_storage);
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_ROWSTATUS_WITH_STORAGETYPE */

#ifndef NETSNMP_FEATURE_REMOVE_CHECK_VB_STORAGETYPE
int
netsnmp_check_vb_storagetype(const netsnmp_variable_list *var, int old_value)
{
    int rc = SNMP_ERR_NOERROR;

    if (NULL == var)
        return SNMP_ERR_GENERR;
    
    if ((rc = netsnmp_check_vb_type_and_size(var, ASN_INTEGER, sizeof(long))))
        return rc;
    
    if ((rc = netsnmp_check_vb_int_range(var, SNMP_STORAGE_NONE,
                                        SNMP_STORAGE_READONLY)))
        return rc;
        
    return check_storage_transition(old_value, *var->val.integer);
}
#endif /* NETSNMP_FEATURE_REMOVE_CHECK_VB_STORAGETYPE */
