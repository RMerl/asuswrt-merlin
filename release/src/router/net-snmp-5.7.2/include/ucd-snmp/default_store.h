#ifdef UCD_COMPATIBLE

#include <net-snmp/library/default_store.h>

/*  Compatibility definitions -- see above header for meaningful comments.  */

#define DS_MAX_IDS			NETSNMP_DS_MAX_IDS
#define DS_MAX_SUBIDS			NETSNMP_DS_MAX_SUBIDS

#define DS_LIBRARY_ID			NETSNMP_DS_LIBRARY_ID
#define DS_APPLICATION_ID		NETSNMP_DS_APPLICATION_ID
#define DS_TOKEN_ID			NETSNMP_DS_TOKEN_ID

#define DS_LIB_MIB_ERRORS		NETSNMP_DS_LIB_MIB_ERRORS
#define DS_LIB_SAVE_MIB_DESCRS		NETSNMP_DS_LIB_SAVE_MIB_DESCRS
#define DS_LIB_MIB_COMMENT_TERM		NETSNMP_DS_LIB_MIB_COMMENT_TERM
#define DS_LIB_MIB_PARSE_LABEL		NETSNMP_DS_LIB_MIB_PARSE_LABEL
#define DS_LIB_DUMP_PACKET		NETSNMP_DS_LIB_DUMP_PACKET
#define DS_LIB_LOG_TIMESTAMP		NETSNMP_DS_LIB_LOG_TIMESTAMP
#define DS_LIB_DONT_READ_CONFIGS	NETSNMP_DS_LIB_DONT_READ_CONFIGS
#define DS_LIB_MIB_REPLACE		NETSNMP_DS_LIB_MIB_REPLACE
#define DS_LIB_PRINT_NUMERIC_ENUM	NETSNMP_DS_LIB_PRINT_NUMERIC_ENUM
#define DS_LIB_PRINT_NUMERIC_OIDS	NETSNMP_DS_LIB_PRINT_NUMERIC_OIDS
#define DS_LIB_DONT_BREAKDOWN_OIDS	NETSNMP_DS_LIB_DONT_BREAKDOWN_OIDS
#define DS_LIB_ALARM_DONT_USE_SIG 	NETSNMP_DS_LIB_ALARM_DONT_USE_SIG
#define DS_LIB_PRINT_FULL_OID 		NETSNMP_DS_LIB_PRINT_FULL_OID
#define DS_LIB_QUICK_PRINT 		NETSNMP_DS_LIB_QUICK_PRINT
#define DS_LIB_RANDOM_ACCESS 		NETSNMP_DS_LIB_RANDOM_ACCESS
#define DS_LIB_REGEX_ACCESS 		NETSNMP_DS_LIB_REGEX_ACCESS
#define DS_LIB_DONT_CHECK_RANGE 	NETSNMP_DS_LIB_DONT_CHECK_RANGE
#define DS_LIB_NO_TOKEN_WARNINGS 	NETSNMP_DS_LIB_NO_TOKEN_WARNINGS
#define DS_LIB_NUMERIC_TIMETICKS 	NETSNMP_DS_LIB_NUMERIC_TIMETICKS
#define DS_LIB_ESCAPE_QUOTES 		NETSNMP_DS_LIB_ESCAPE_QUOTES
#define DS_LIB_REVERSE_ENCODE 		NETSNMP_DS_LIB_REVERSE_ENCODE
#define DS_LIB_PRINT_BARE_VALUE 	NETSNMP_DS_LIB_PRINT_BARE_VALUE
#define DS_LIB_EXTENDED_INDEX 		NETSNMP_DS_LIB_EXTENDED_INDEX
#define DS_LIB_PRINT_HEX_TEXT 		NETSNMP_DS_LIB_PRINT_HEX_TEXT

#define DS_LIB_MIB_WARNINGS		NETSNMP_DS_LIB_MIB_WARNINGS
#define DS_LIB_SECLEVEL			NETSNMP_DS_LIB_SECLEVEL
#define DS_LIB_SNMPVERSION		NETSNMP_DS_LIB_SNMPVERSION
#define DS_LIB_DEFAULT_PORT		NETSNMP_DS_LIB_DEFAULT_PORT
#define DS_LIB_PRINT_SUFFIX_ONLY	NETSNMP_DS_LIB_PRINT_SUFFIX_ONLY

#define DS_LIB_SECNAME			NETSNMP_DS_LIB_SECNAME
#define DS_LIB_CONTEXT			NETSNMP_DS_LIB_CONTEXT
#define DS_LIB_PASSPHRASE		NETSNMP_DS_LIB_PASSPHRASE
#define DS_LIB_AUTHPASSPHRASE		NETSNMP_DS_LIB_AUTHPASSPHRASE
#define DS_LIB_PRIVPASSPHRASE		NETSNMP_DS_LIB_PRIVPASSPHRASE
#define DS_LIB_OPTIONALCONFIG		NETSNMP_DS_LIB_OPTIONALCONFIG
#define DS_LIB_APPTYPE			NETSNMP_DS_LIB_APPTYPE
#define DS_LIB_COMMUNITY		NETSNMP_DS_LIB_COMMUNITY
#define DS_LIB_PERSISTENT_DIR		NETSNMP_DS_LIB_PERSISTENT_DIR
#define DS_LIB_CONFIGURATION_DIR	NETSNMP_DS_LIB_CONFIGURATION_DIR

#ifdef __cplusplus
extern "C" {
#endif

int	ds_set_boolean		(int storeid, int which, int value);
int	ds_get_boolean		(int storeid, int which);
int	ds_toggle_boolean	(int storeid, int which);

int	ds_set_int		(int storeid, int which, int value);
int	ds_get_int		(int storeid, int which);

int	ds_set_string		(int storeid, int which, const char *value);
char   *ds_get_string		(int storeid, int which);

int 	ds_set_void		(int storeid, int which, void *value);
void   *ds_get_void		(int storeid, int which);

int	ds_register_config	(u_char type, const char *ftype,
				 const char *token, int storeid, int which);
int	ds_register_premib	(u_char type, const char *ftype,
				 const char *token, int storeid, int which);

void	ds_shutdown		(void);

#ifdef __cplusplus
}
#endif

#else /* UCD_COMPATIBLE */

#error "Please update your headers or configure using --enable-ucd-snmp-compatibility"

#endif
