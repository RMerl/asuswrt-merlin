#ifndef AGENT_REGISTRY_H
#define AGENT_REGISTRY_H

/***********************************************************************/
/*
 * new version2 agent handler API structures 
 */
/***********************************************************************/

#include <net-snmp/agent/snmp_agent.h>
#include <net-snmp/library/fd_event_manager.h>

#ifdef __cplusplus
extern          "C" {
#endif

/***********************************************************************/
    /*
     * requests api definitions 
     */
/***********************************************************************/

    /*
     * the structure of parameters passed to registered ACM modules 
     */
struct view_parameters {
    netsnmp_pdu    *pdu;
    oid            *name;
    size_t          namelen;
    int             test;
    int             errorcode;		/*  Do not change unless you're
					    specifying an error, as it starts
					    in a success state.  */
    int             check_subtree;
};

struct register_parameters {
    oid                          *name;
    size_t                        namelen;
    int                           priority;
    int                           range_subid;
    oid                           range_ubound;
    int                           timeout;
    u_char                        flags;
    const char                   *contextName;
    netsnmp_session              *session;
    netsnmp_handler_registration *reginfo;
};

typedef struct subtree_context_cache_s {
    const char				*context_name;
    struct netsnmp_subtree_s		*first_subtree;
    struct subtree_context_cache_s	*next;
} subtree_context_cache;



void             setup_tree		  (void);
void             shutdown_tree    (void);


netsnmp_subtree *netsnmp_subtree_find	  (const oid *, size_t,
					   netsnmp_subtree *,
					   const char *context_name);

netsnmp_subtree *netsnmp_subtree_find_next(const oid *, size_t,
					   netsnmp_subtree *,
					   const char *context_name);

netsnmp_subtree *netsnmp_subtree_find_prev(const oid *, size_t,
					   netsnmp_subtree *,
					   const char *context_name);

netsnmp_subtree *netsnmp_subtree_find_first(const char *context_name);

netsnmp_session *get_session_for_oid	   (const oid *, size_t, 
					    const char *context_name);

subtree_context_cache *get_top_context_cache(void);

void netsnmp_set_lookup_cache_size(int newsize);
int netsnmp_get_lookup_cache_size(void);

#define MIB_REGISTERED_OK		 0
#define MIB_DUPLICATE_REGISTRATION	-1
#define MIB_REGISTRATION_FAILED		-2
#define MIB_UNREGISTERED_OK		 0
#define MIB_NO_SUCH_REGISTRATION	-1
#define MIB_UNREGISTRATION_FAILED	-2
#define DEFAULT_MIB_PRIORITY		127

int             register_mib		   (const char *, struct variable *,
					    size_t, size_t, const oid *,
					    size_t);

int             register_mib_priority	   (const char *, struct variable *,
					    size_t, size_t, const oid *, size_t,
					    int);

int             register_mib_range	   (const char *, struct variable *,
					    size_t, size_t, const oid *,
					    size_t, int, int, oid,
					    netsnmp_session *);

int		register_mib_context	   (const char *, struct variable *,
					    size_t, size_t, const oid *, size_t,
					    int, int, oid, netsnmp_session *,
					    const char *, int, int);

int	netsnmp_register_mib_table_row	   (const char *, struct variable *,
					    size_t, size_t, oid *,
					    size_t, int, int, netsnmp_session *,
					    const char *, int, int);

int		unregister_mib		   (oid *, size_t);

int             unregister_mib_priority	   (oid *, size_t, int);
int             unregister_mib_range	   (oid *, size_t, int, int, oid);
int             unregister_mib_context	   (oid *, size_t, int, int, oid,
					    const char *);
void            clear_context              (void);
void            unregister_mibs_by_session (netsnmp_session *);
int     netsnmp_unregister_mib_table_row   (oid *mibloc, size_t mibloclen,
					    int priority, int var_subid,
					    oid range_ubound,
					    const char *context);

int             compare_tree		   (const oid *, size_t, 
					    const oid *, size_t);
int             in_a_view		   (oid *, size_t *, 
					    netsnmp_pdu *, int);
int             check_access		   (netsnmp_pdu *pdu);
int             netsnmp_acm_check_subtree  (netsnmp_pdu *, oid *, size_t);
void            register_mib_reattach	   (void);
void            register_mib_detach	   (void);

/*
 * REGISTER_MIB(): This macro simply loads register_mib with less pain:
 * 
 * descr:   A short description of the mib group being loaded.
 * var:     The variable structure to load.
 * vartype: The variable structure used to define it (variable[2, 4, ...])
 * theoid:  An *initialized* *exact length* oid pointer.
 *          (sizeof(theoid) *must* return the number of elements!) 
 */

#define REGISTER_MIB(descr, var, vartype, theoid)                      \
  if (register_mib(descr, (struct variable *) var, sizeof(struct vartype), \
               sizeof(var)/sizeof(struct vartype),                     \
               theoid, sizeof(theoid)/sizeof(oid)) != MIB_REGISTERED_OK ) \
	DEBUGMSGTL(("register_mib", "%s registration failed\n", descr));


#define NUM_EXTERNAL_SIGS 32
#define SIG_REGISTERED_OK		 0
#define SIG_REGISTRATION_FAILED		-2
#define SIG_UNREGISTERED_OK		 0

extern int      external_signal_scheduled[NUM_EXTERNAL_SIGS];
extern void     (*external_signal_handler[NUM_EXTERNAL_SIGS])(int);

int             register_signal(int, void (*func)(int));
int             unregister_signal(int);



/*
 * internal API.  Don't use this.  Use netsnmp_register_handler instead 
 */

struct netsnmp_handler_registration_s;

int             netsnmp_register_mib(const char *, struct variable *,
				     size_t, size_t, oid *, size_t,
				     int, int, oid, netsnmp_session *,
				     const char *, int, int,
				     struct netsnmp_handler_registration_s *,
				     int);

#ifdef __cplusplus
}
#endif

#endif                          /* AGENT_REGISTRY_H */
