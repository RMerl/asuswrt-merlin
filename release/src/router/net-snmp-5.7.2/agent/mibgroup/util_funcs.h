/*
 *  util_funcs.h:  utilitiy functions for extensible groups.
 */
#ifndef _MIBGROUP_UTIL_FUNCS_H
#define _MIBGROUP_UTIL_FUNCS_H

config_require(util_funcs/header_generic)
config_require(util_funcs/header_simple_table)

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#include "util_funcs/header_generic.h"
#include "util_funcs/header_simple_table.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "struct.h"

typedef struct prefix_info
{
   struct prefix_info *next_info;
   unsigned long ipAddressPrefixOnLinkFlag;
   unsigned long ipAddressPrefixAutonomousFlag;
   char in6p[40];
}prefix_cbx;
#ifndef WIN32
typedef struct 
{
 prefix_cbx **list_head;
}netsnmp_prefix_listen_info;
#endif
int             shell_command(struct extensible *);
int             exec_command(struct extensible *);
struct extensible *get_exten_instance(struct extensible *, size_t);
int             get_exec_output(struct extensible *);
#if defined(WIN32) && !defined(cygwin)
int             get_exec_pipes(char *cmd, int *fdIn, int *fdOut, HANDLE *pid);
#else
int             get_exec_pipes(char *cmd, int *fdIn, int *fdOut, pid_t *pid);
#endif
WriteMethod     clear_cache;
void            print_mib_oid(oid *, size_t);
void            sprint_mib_oid(char *, const oid *, size_t);
int             checkmib(struct variable *, oid *, size_t *, int, size_t *,
                         WriteMethod ** write_method, int);
char           *find_field(char *, int);
int             parse_miboid(const char *, oid *);
void            string_append_int(char *, int);
void            wait_on_exec(struct extensible *);
const char     *make_tempfile(void);

prefix_cbx *net_snmp_create_prefix_info(unsigned long OnLinkFlag,
                                        unsigned long AutonomousFlag,
                                        char *in6ptr);
#ifndef WIN32
int net_snmp_find_prefix_info(prefix_cbx **head,
                              char *address,
                              prefix_cbx *node_to_find);
int net_snmp_update_prefix_info(prefix_cbx **head,
                                prefix_cbx *node_to_update);
int net_snmp_search_update_prefix_info(prefix_cbx **head,
                                       prefix_cbx *node_to_use,
                                       int functionality);
int net_snmp_delete_prefix_info(prefix_cbx **head,
                                char *address);
#endif
#define NIP6(addr) \
        ntohs((addr).s6_addr16[0]), \
        ntohs((addr).s6_addr16[1]), \
        ntohs((addr).s6_addr16[2]), \
        ntohs((addr).s6_addr16[3]), \
        ntohs((addr).s6_addr16[4]), \
        ntohs((addr).s6_addr16[5]), \
        ntohs((addr).s6_addr16[6]), \
        ntohs((addr).s6_addr16[7])

#define     satosin(x)      ((struct sockaddr_in *) &(x))
#define     SOCKADDR(x)     (satosin(x)->sin_addr.s_addr)

#include "util_funcs/MIB_STATS_CACHE_TIMEOUT.h"

typedef void   *mib_table_t;
typedef int     (RELOAD) (mib_table_t);
typedef int     (COMPARE) (const void *, const void *);
mib_table_t     Initialise_Table(int, int, RELOAD*, COMPARE*);
int             Search_Table(mib_table_t, void *, int);
int             Add_Entry(mib_table_t, void *);
void           *Retrieve_Table_Data(mib_table_t, int *);

#ifdef __cplusplus
}
#endif

#endif                          /* _MIBGROUP_UTIL_FUNCS_H */
