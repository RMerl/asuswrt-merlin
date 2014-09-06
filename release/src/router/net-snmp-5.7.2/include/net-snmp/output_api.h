#ifndef NET_SNMP_OUTPUT_API_H
#define NET_SNMP_OUTPUT_API_H

    /**
     *  Library API routines concerned with logging and message output
     *    (including error handling and debugging).
     */

#include <net-snmp/types.h>
#include <stdarg.h>	/* for va_list */

#ifdef __cplusplus
extern "C" {
#endif

    /* Error reporting */
    NETSNMP_IMPORT
    void    snmp_error(netsnmp_session *sess, int *clib_errorno,
                           int *snmp_errorno, char **errstring);
    NETSNMP_IMPORT
    void    snmp_sess_error(      void *sess, int *clib_errorno,
                           int *snmp_errorno, char **errstring);

    NETSNMP_IMPORT
    const char *snmp_api_errstring(int snmp_errorno);  /*  library errors */
    NETSNMP_IMPORT
    const char     *snmp_errstring(int snmp_errorno);  /* protocol errors */

    NETSNMP_IMPORT
    void    snmp_perror(const char *msg);   /* for parsing errors only */

    NETSNMP_IMPORT
    void    snmp_sess_perror(const char *msg, netsnmp_session *sess);
                                       /* for all other SNMP library errors */
    NETSNMP_IMPORT
    void    snmp_log_perror(const char *msg);
                                       /* for system library errors */

    /* Logging messages */

#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define _LOG_ATTR
#else
#define _LOG_ATTR   __attribute__ ((__format__ (__printf__, 2, 3)))
#endif

    NETSNMP_IMPORT
    int  snmp_log( int priority, const char *format, ...) _LOG_ATTR;
    NETSNMP_IMPORT
    int  snmp_vlog(int priority, const char *format, va_list ap);
    NETSNMP_IMPORT
    int  snmp_get_do_logging(    void);
    NETSNMP_IMPORT
    void netsnmp_logging_restart(void);
    NETSNMP_IMPORT
    void snmp_disable_log(       void);
    NETSNMP_IMPORT
    void shutdown_snmp_logging(  void);

#undef _LOG_ATTR

    /* Debug messages */
#ifndef NETSNMP_NO_DEBUGGING
#include <net-snmp/library/snmp_debug.h>	/* for internal macros */
#define DEBUGMSG(x)        do {if (_DBG_IF_) {debugmsg x;} }while(0)
#define DEBUGMSGT(x)       do {if (_DBG_IF_) {__DBGMSGT(x);} }while(0)
#define DEBUGTRACE         do {if (_DBG_IF_) {__DBGTRACE;} }while(0)
#define DEBUGTRACETOK(x)   do {if (_DBG_IF_) {__DBGTRACETOK(x);} }while(0)
#define DEBUGMSGL(x)       do {if (_DBG_IF_) {__DBGMSGL(x);} }while(0)
#define DEBUGMSGTL(x)      do {if (_DBG_IF_) {__DBGMSGTL(x);} }while(0)
#define DEBUGMSGOID(x)     do {if (_DBG_IF_) {__DBGMSGOID(x);} }while(0)
#define DEBUGMSGSUBOID(x)  do {if (_DBG_IF_) {__DBGMSGSUBOID(x);} }while(0)
#define DEBUGMSGVAR(x)     do {if (_DBG_IF_) {__DBGMSGVAR(x);} }while(0)
#define DEBUGMSGOIDRANGE(x) do {if (_DBG_IF_) {__DBGMSGOIDRANGE(x);} }while(0)
#define DEBUGMSGHEX(x)     do {if (_DBG_IF_) {__DBGMSGHEX(x);} }while(0)
#define DEBUGMSGHEXTLI(x)  do {if (_DBG_IF_) {__DBGMSGHEXTLI(x);} }while(0)
#define DEBUGINDENTADD(x)  do {if (_DBG_IF_) {__DBGINDENTADD(x);} }while(0)
#define DEBUGINDENTMORE()  do {if (_DBG_IF_) {__DBGINDENTMORE();} }while(0)
#define DEBUGINDENTLESS()  do {if (_DBG_IF_) {__DBGINDENTLESS();} }while(0)
#define DEBUGPRINTINDENT(token) \
	do {if (_DBG_IF_) {__DBGPRINTINDENT(token);} }while(0)
#define DEBUGDUMPHEADER(token,x) \
	do {if (_DBG_IF_) {__DBGDUMPHEADER(token,x);} }while(0)
#define DEBUGDUMPSECTION(token,x) \
	do {if (_DBG_IF_) {__DBGDUMPSECTION(token,x);} }while(0)
#define DEBUGDUMPSETUP(token,buf,len) \
	do {if (_DBG_IF_) {__DBGDUMPSETUP(token,buf,len);} }while(0)
#define DEBUGMSG_NC(x)  do { __DBGMSG_NC(x); }while(0)
#define DEBUGMSGT_NC(x) do { __DBGMSGT_NC(x); }while(0)

#else        /* NETSNMP_NO_DEBUGGING := enable streamlining of the code */

#define DEBUGMSG(x)
#define DEBUGMSGT(x)
#define DEBUGTRACE
#define DEBUGTRACETOK(x)
#define DEBUGMSGL(x)
#define DEBUGMSGTL(x)
#define DEBUGMSGOID(x)
#define DEBUGMSGSUBOID(x)
#define DEBUGMSGVAR(x)
#define DEBUGMSGOIDRANGE(x)
#define DEBUGMSGHEX(x)
#define DEBUGIF(x)        if(0)
#define DEBUGDUMP(t,b,l,p)
#define DEBUGINDENTMORE()
#define DEBUGINDENTLESS()
#define DEBUGINDENTADD(x)
#define DEBUGMSGHEXTLI(x)
#define DEBUGPRINTINDENT(token)
#define DEBUGDUMPHEADER(token,x)
#define DEBUGDUMPSECTION(token,x)
#define DEBUGDUMPSETUP(token, buf, len)

#define DEBUGMSG_NC(x)
#define DEBUGMSGT_NC(x)

#endif    /* NETSNMP_NO_DEBUGGING */

    NETSNMP_IMPORT
    void            debug_register_tokens(const char *tokens);
    NETSNMP_IMPORT
    int             debug_is_token_registered(const char *token);
    NETSNMP_IMPORT
    void            snmp_set_do_debugging(int);
    NETSNMP_IMPORT
    int             snmp_get_do_debugging(void);

    /*
     *    Having extracted the main ("public API") calls relevant
     *  to this area of the Net-SNMP project, the next step is to
     *  identify the related "public internal API" routines.
     *
     *    In due course, these should probably be gathered
     *  together into a companion 'library/output_api.h' header file.
     *  [Or some suitable name]
     *
     *    But for the time being, the expectation is that the
     *  traditional headers that provided the above definitions
     *  will probably also cover the relevant internal API calls.
     *  Hence they are listed here:
     */

#ifdef __cplusplus
}
#endif

#include <net-snmp/library/snmp_api.h>
#include <net-snmp/library/snmp_client.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/snmp_logging.h>

#ifndef ERROR_MSG
#define ERROR_MSG(string)	snmp_set_detail(string)
#endif

#endif                          /* NET_SNMP_OUTPUT_API_H */
