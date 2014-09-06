/*
 * module to include the modules
 */
/*config_require(tcp-mib/tcpConnTable)*/
config_require(tcp-mib/tcpConnectionTable)
config_require(tcp-mib/tcpListenerTable)
config_add_mib(TCP-MIB)
