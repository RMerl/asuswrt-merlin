#ifndef SNMP_DEBUG_H
#define SNMP_DEBUG_H

#ifdef __cplusplus
extern          "C" {
#endif

    /*
     * snmp_debug.h:
     * 
     * - prototypes for snmp debugging routines.
     * - easy to use macros to wrap around the functions.  This also provides
     * the ability to remove debugging code easily from the applications at
     * compile time.
     */


#if !defined(__GNUC__) || __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define NETSNMP_ATTRIBUTE_FORMAT(type, formatArg, firstArg)
#else
#define NETSNMP_ATTRIBUTE_FORMAT(type, formatArg, firstArg) \
  __attribute__((__format__( __ ## type ## __, formatArg, firstArg )))
#endif

    /*
     * These functions should not be used, if at all possible.  Instead, use
     * the macros below. 
     */
    NETSNMP_IMPORT
    void            debugmsg(const char *token, const char *format, ...)
                        NETSNMP_ATTRIBUTE_FORMAT(printf, 2, 3);
    NETSNMP_IMPORT
    void            debugmsgtoken(const char *token, const char *format,
                                  ...)
                        NETSNMP_ATTRIBUTE_FORMAT(printf, 2, 3);
    void            debug_combo_nc(const char *token, const char *format,
                                   ...)
                        NETSNMP_ATTRIBUTE_FORMAT(printf, 2, 3);

#undef NETSNMP_ATTRIBUTE_FORMAT

    NETSNMP_IMPORT
    void            debugmsg_oid(const char *token, const oid * theoid,
                                 size_t len);
    NETSNMP_IMPORT
    void            debugmsg_suboid(const char *token, const oid * theoid,
                                    size_t len);
    NETSNMP_IMPORT
    void            debugmsg_var(const char *token,
                                 netsnmp_variable_list * var);
    NETSNMP_IMPORT
    void            debugmsg_oidrange(const char *token,
                                      const oid * theoid, size_t len,
                                      size_t var_subid, oid range_ubound);
    NETSNMP_IMPORT
    void            debugmsg_hex(const char *token, const u_char * thedata,
                                 size_t len);
    NETSNMP_IMPORT
    void            debugmsg_hextli(const char *token, const u_char * thedata,
                                    size_t len);
    NETSNMP_IMPORT
    void            debug_indent_add(int amount);
    NETSNMP_IMPORT
    int             debug_indent_get(void);
    /*
     * What is said above is true for this function as well. Further this
     * function is deprecated and only provided for backwards compatibility.
     * Please use "%*s", debug_indent_get(), "" if you used this one before.
     */
    NETSNMP_IMPORT
    const char     *debug_indent(void);

    /*
     * Use these macros instead of the functions above to allow them to be
     * re-defined at compile time to NOP for speed optimization.
     * 
     * They need to be called enclosing all the arguments in a single set of ()s.
     * Example:
     * DEBUGMSGTL(("token", "debugging of something %s related\n", "snmp"));
     * 
     * Usage:
     * All of the functions take a "token" argument that helps determine when
     * the output in question should be printed.  See the snmpcmd.1 manual page
     * on the -D flag to turn on/off output for a given token on the command line.
     * 
     * DEBUGMSG((token, format, ...)):      equivalent to printf(format, ...)
     * (if "token" debugging output
     * is requested by the user)
     * 
     * DEBUGMSGT((token, format, ...)):     equivalent to DEBUGMSG, but prints
     * "token: " at the beginning of the
     * line for you.
     * 
     * DEBUGTRACE                           Insert this token anywhere you want
     * tracing output displayed when the
     * "trace" debugging token is selected.
     * 
     * DEBUGMSGL((token, format, ...)):     equivalent to DEBUGMSG, but includes
     * DEBUGTRACE debugging line just before
     * yours.
     * 
     * DEBUGMSGTL((token, format, ...)):    Same as DEBUGMSGL and DEBUGMSGT
     * combined.
     * 
     * Important:
     * It is considered best if you use DEBUGMSGTL() everywhere possible, as it
     * gives the nicest format output and provides tracing support just before
     * every debugging statement output.
     * 
     * To print multiple pieces to a single line in one call, use:
     * 
     * DEBUGMSGTL(("token", "line part 1"));
     * DEBUGMSG  (("token", " and part 2\n"));
     * 
     * to get:
     * 
     * token: line part 1 and part 2
     * 
     * as debugging output.
     *
     *
     * Each of these macros also have a version with a suffix of '_NC'. The
     * NC suffix stands for 'No Check', which means that no check will be
     * performed to see if debug is enabled or if the token has been turned
     * on. These NC versions are intended for use within a DEBUG_IF {} block,
     * where the debug/token check has already been performed.
     */

#ifndef NETSNMP_NO_DEBUGGING       /* make sure we're wanted */

    /*
     * define two macros : one macro with, one without,
     *                     a test if debugging is enabled.
     * 
     * Generally, use the macro with _DBG_IF_
     */

/******************* Start private macros ************************/
#define _DBG_IF_            snmp_get_do_debugging()
#define DEBUGIF(x)         if (_DBG_IF_ && debug_is_token_registered(x) == SNMPERR_SUCCESS)

#define __DBGMSGT(x)     debugmsgtoken x,  debugmsg x
#define __DBGMSG_NC(x)   debugmsg x
#define __DBGMSGT_NC(x)  debug_combo_nc x
#define __DBGMSGL_NC(x)  __DBGTRACE; debugmsg x
#define __DBGMSGTL_NC(x) __DBGTRACE; debug_combo_nc x

#ifdef  NETSNMP_FUNCTION
#define __DBGTRACE       __DBGMSGT(("trace","%s(): %s, %d:\n",\
				NETSNMP_FUNCTION,__FILE__,__LINE__))
#define __DBGTRACETOK(x) __DBGMSGT((x,"%s(): %s, %d:\n",       \
                                    NETSNMP_FUNCTION,__FILE__,__LINE__))
#else
#define __DBGTRACE       __DBGMSGT(("trace"," %s, %d:\n", __FILE__,__LINE__))
#define __DBGTRACETOK(x) __DBGMSGT((x," %s, %d:\n", __FILE__,__LINE__))
#endif

#define __DBGMSGL(x)     __DBGTRACE, debugmsg x
#define __DBGMSGTL(x)    __DBGTRACE, debugmsgtoken x, debugmsg x
#define __DBGMSGOID(x)     debugmsg_oid x
#define __DBGMSGSUBOID(x)  debugmsg_suboid x
#define __DBGMSGVAR(x)     debugmsg_var x
#define __DBGMSGOIDRANGE(x) debugmsg_oidrange x
#define __DBGMSGHEX(x)     debugmsg_hex x
#define __DBGMSGHEXTLI(x)  debugmsg_hextli x
#define __DBGINDENT()      debug_indent_get()
#define __DBGINDENTADD(x)  debug_indent_add(x)
#define __DBGINDENTMORE()  debug_indent_add(2)
#define __DBGINDENTLESS()  debug_indent_add(-2)
#define __DBGPRINTINDENT(token) __DBGMSGTL((token, "%*s", __DBGINDENT(), ""))

#define __DBGDUMPHEADER(token,x) \
        __DBGPRINTINDENT("dumph_" token); \
        debugmsg("dumph_" token,x); \
        if (debug_is_token_registered("dumpx" token) == SNMPERR_SUCCESS ||    \
            debug_is_token_registered("dumpv" token) == SNMPERR_SUCCESS ||    \
            (debug_is_token_registered("dumpx_" token) != SNMPERR_SUCCESS &&  \
             debug_is_token_registered("dumpv_" token) != SNMPERR_SUCCESS)) { \
            debugmsg("dumph_" token,"\n"); \
        } else { \
            debugmsg("dumph_" token,"  "); \
        } \
        __DBGINDENTMORE()

#define __DBGDUMPSECTION(token,x) \
        __DBGPRINTINDENT("dumph_" token); \
        debugmsg("dumph_" token,"%s\n",x);\
        __DBGINDENTMORE()

#define __DBGDUMPSETUP(token,buf,len) \
        debugmsg("dumpx" token, "dumpx_%s:%*s", token, __DBGINDENT(), ""); \
        __DBGMSGHEX(("dumpx_" token,buf,len)); \
        if (debug_is_token_registered("dumpv" token) == SNMPERR_SUCCESS || \
            debug_is_token_registered("dumpv_" token) != SNMPERR_SUCCESS) { \
            debugmsg("dumpx_" token,"\n"); \
        } else { \
            debugmsg("dumpx_" token,"  "); \
        } \
        debugmsg("dumpv" token, "dumpv_%s:%*s", token, __DBGINDENT(), "");

/******************* End   private macros ************************/
/*****************************************************************/
#endif /* NETSNMP_NO_DEBUGGING */

#ifdef __cplusplus
}
#endif

    /* Public macros moved to top-level API header file */
#include <net-snmp/output_api.h>

#ifdef __cplusplus
extern          "C" {
#endif

    void            snmp_debug_init(void);

#define MAX_DEBUG_TOKENS 256
#define MAX_DEBUG_TOKEN_LEN 128
#define DEBUG_TOKEN_DELIMITER ","
#define DEBUG_ALWAYS_TOKEN "all"

#ifndef NETSNMP_NO_DEBUGGING

/*
 * internal:
 * You probably shouldn't be using this information unless the word
 * "expert" applies to you.  I know it looks tempting.
 */
typedef struct netsnmp_token_descr_s {
    char *token_name;
    char  enabled;
} netsnmp_token_descr;

NETSNMP_IMPORT int                 debug_num_tokens;
NETSNMP_IMPORT netsnmp_token_descr dbg_tokens[MAX_DEBUG_TOKENS];

#endif /* NETSNMP_NO_DEBUGGING */

#ifdef __cplusplus
}
#endif
#endif                          /* SNMP_DEBUG_H */
