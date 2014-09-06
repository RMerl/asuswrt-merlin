/*
 * For compatibility with applications built using
 * previous versions of the UCD library only.
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>

#include <net-snmp/types.h>
#include <net-snmp/session_api.h>
#include <net-snmp/config_api.h>
#include <net-snmp/library/mib.h>	/* for OID O/P format enums */
#include <net-snmp/library/ucd_compat.h>

netsnmp_feature_child_of(ucd_compatibility, libnetsnmp)

#ifndef NETSNMP_FEATURE_REMOVE_UCD_COMPATIBILITY
/*
 * use <netsnmp_session *)->s_snmp_errno instead 
 */
int
snmp_get_errno(void)
{
    return SNMPERR_SUCCESS;
}

/*
 * synch_reset and synch_setup are no longer used. 
 */
NETSNMP_IMPORT void snmp_synch_reset(netsnmp_session * notused);
void
snmp_synch_reset(netsnmp_session * notused)
{
}
NETSNMP_IMPORT void snmp_synch_setup(netsnmp_session * notused);
void
snmp_synch_setup(netsnmp_session * notused)
{
}


void
snmp_set_dump_packet(int x)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_DUMP_PACKET, x);
}

int
snmp_get_dump_packet(void)
{
    return netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_DUMP_PACKET);
}

void
snmp_set_quick_print(int x)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_QUICK_PRINT, x);
}

int
snmp_get_quick_print(void)
{
    return netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_QUICK_PRINT);
}


void
snmp_set_suffix_only(int x)
{
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID,
		       NETSNMP_DS_LIB_OID_OUTPUT_FORMAT, x);
}

int
snmp_get_suffix_only(void)
{
    return netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
			      NETSNMP_DS_LIB_OID_OUTPUT_FORMAT);
}

void
snmp_set_full_objid(int x)
{
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT,
                                              NETSNMP_OID_OUTPUT_FULL);
}

int
snmp_get_full_objid(void)
{
    return (NETSNMP_OID_OUTPUT_FULL ==
        netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_OID_OUTPUT_FORMAT));
}

void
snmp_set_random_access(int x)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_RANDOM_ACCESS, x);
}

int
snmp_get_random_access(void)
{
    return netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
				  NETSNMP_DS_LIB_RANDOM_ACCESS);
}

void
snmp_set_mib_errors(int err)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_MIB_ERRORS, err);
}

void
snmp_set_mib_warnings(int warn)
{
    netsnmp_ds_set_int(NETSNMP_DS_LIBRARY_ID, 
		       NETSNMP_DS_LIB_MIB_WARNINGS, warn);
}

void
snmp_set_save_descriptions(int save)
{
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_SAVE_MIB_DESCRS, save);
}

void
snmp_set_mib_comment_term(int save)
{
    /*
     * 0=strict, 1=EOL terminated 
     */
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_MIB_COMMENT_TERM, save);
}

void
snmp_set_mib_parse_label(int save)
{
    /*
     * 0=strict, 1=underscore OK in label 
     */
    netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
			   NETSNMP_DS_LIB_MIB_PARSE_LABEL, save);
}

int
ds_set_boolean		(int storeid, int which, int value)
{
  return netsnmp_ds_set_boolean(storeid, which, value);
}

int
ds_get_boolean		(int storeid, int which)
{
  return netsnmp_ds_get_boolean(storeid, which);
}

int
ds_toggle_boolean	(int storeid, int which)
{
  return netsnmp_ds_toggle_boolean(storeid, which);
}

int
ds_set_int		(int storeid, int which, int value)
{
  return netsnmp_ds_set_int(storeid, which, value);
}

int
ds_get_int		(int storeid, int which)
{
  return netsnmp_ds_get_int(storeid, which);
}


int
ds_set_string		(int storeid, int which, const char *value)
{
  return netsnmp_ds_set_string(storeid, which, value);
}

char *
ds_get_string		(int storeid, int which)
{
  return netsnmp_ds_get_string(storeid, which);
}

int
ds_set_void		(int storeid, int which, void *value)
{
  return netsnmp_ds_set_void(storeid, which, value);
}

void *
ds_get_void		(int storeid, int which)
{
  return netsnmp_ds_get_void(storeid, which);
}

int
ds_register_config	(u_char type, const char *ftype,
			 const char *token, int storeid, int which)
{
  return netsnmp_ds_register_config(type, ftype, token, storeid, which);
}

int
ds_register_premib	(u_char type, const char *ftype,
			 const char *token, int storeid, int which)
{
  return netsnmp_ds_register_premib(type, ftype, token, storeid, which);
}

void
ds_shutdown		(void)
{
  netsnmp_ds_shutdown();
}
#else /* !NETSNMP_FEATURE_REMOVE_UCD_COMPATIBILITY */
netsnmp_feature_unused(ucd_compatibility);
#endif /* !NETSNMP_FEATURE_REMOVE_UCD_COMPATIBILITY */
