#ifndef SCTP_TABLES_H
#define SCTP_TABLES_H
config_require(sctp-mib/sctpTables_common)
config_require(sctp-mib/sctpAssocRemAddrTable)
config_require(sctp-mib/sctpAssocLocalAddrTable)
config_require(sctp-mib/sctpLookupLocalPortTable)
config_require(sctp-mib/sctpLookupRemPortTable)
config_require(sctp-mib/sctpLookupRemHostNameTable)
config_require(sctp-mib/sctpLookupRemPrimIPAddrTable)
config_require(sctp-mib/sctpLookupRemIPAddrTable)
/*
 * this one must be last to ensure proper initialization ordering 
 */
config_require(sctp-mib/sctpAssocTable)
#if defined( linux )
config_require(sctp-mib/sctpTables_linux)
config_require(util_funcs/get_pid_from_inode)
#elif defined( freebsd7 ) || defined( freebsd8 )
config_require(sctp-mib/sctpTables_freebsd)
#elif defined( solaris2 )
config_require(sctp-mib/sctpTables_solaris2)
#else
config_error(SCTP-MIB is not available in tihs environment)
#endif
#endif                          /* SCTP_TABLES_H */
