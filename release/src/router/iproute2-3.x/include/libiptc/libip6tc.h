#ifndef _LIBIP6TC_H
#define _LIBIP6TC_H
/* Library which manipulates firewall rules. Version 0.2. */

#include <libiptc/ipt_kernel_headers.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

#ifndef IP6T_MIN_ALIGN
#define IP6T_MIN_ALIGN (__alignof__(struct ip6t_entry))
#endif
#define IP6T_ALIGN(s) (((s) + (IP6T_MIN_ALIGN-1)) & ~(IP6T_MIN_ALIGN-1))

typedef char ip6t_chainlabel[32];

#define IP6TC_LABEL_ACCEPT "ACCEPT"
#define IP6TC_LABEL_DROP "DROP"
#define IP6TC_LABEL_QUEUE   "QUEUE"
#define IP6TC_LABEL_RETURN "RETURN"

/* Transparent handle type. */
typedef struct ip6tc_handle *ip6tc_handle_t;

/* Does this chain exist? */
int ip6tc_is_chain(const char *chain, const ip6tc_handle_t handle);

/* Take a snapshot of the rules. Returns NULL on error. */
ip6tc_handle_t ip6tc_init(const char *tablename);

/* Cleanup after ip6tc_init(). */
void ip6tc_free(ip6tc_handle_t *h);

/* Iterator functions to run through the chains.  Returns NULL at end. */
const char *ip6tc_first_chain(ip6tc_handle_t *handle);
const char *ip6tc_next_chain(ip6tc_handle_t *handle);

/* Get first rule in the given chain: NULL for empty chain. */
const struct ip6t_entry *ip6tc_first_rule(const char *chain,
					  ip6tc_handle_t *handle);

/* Returns NULL when rules run out. */
const struct ip6t_entry *ip6tc_next_rule(const struct ip6t_entry *prev,
					 ip6tc_handle_t *handle);

/* Returns a pointer to the target name of this position. */
const char *ip6tc_get_target(const struct ip6t_entry *e,
			     ip6tc_handle_t *handle);

/* Is this a built-in chain? */
int ip6tc_builtin(const char *chain, const ip6tc_handle_t handle);

/* Get the policy of a given built-in chain */
const char *ip6tc_get_policy(const char *chain,
			     struct ip6t_counters *counters,
			     ip6tc_handle_t *handle);

/* These functions return TRUE for OK or 0 and set errno. If errno ==
   0, it means there was a version error (ie. upgrade libiptc). */
/* Rule numbers start at 1 for the first rule. */

/* Insert the entry `fw' in chain `chain' into position `rulenum'. */
int ip6tc_insert_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *e,
		       unsigned int rulenum,
		       ip6tc_handle_t *handle);

/* Atomically replace rule `rulenum' in `chain' with `fw'. */
int ip6tc_replace_entry(const ip6t_chainlabel chain,
			const struct ip6t_entry *e,
			unsigned int rulenum,
			ip6tc_handle_t *handle);

/* Append entry `fw' to chain `chain'. Equivalent to insert with
   rulenum = length of chain. */
int ip6tc_append_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *e,
		       ip6tc_handle_t *handle);

/* Delete the first rule in `chain' which matches `fw'. */
int ip6tc_delete_entry(const ip6t_chainlabel chain,
		       const struct ip6t_entry *origfw,
		       unsigned char *matchmask,
		       ip6tc_handle_t *handle);

/* Delete the rule in position `rulenum' in `chain'. */
int ip6tc_delete_num_entry(const ip6t_chainlabel chain,
			   unsigned int rulenum,
			   ip6tc_handle_t *handle);

/* Check the packet `fw' on chain `chain'. Returns the verdict, or
   NULL and sets errno. */
const char *ip6tc_check_packet(const ip6t_chainlabel chain,
			       struct ip6t_entry *,
			       ip6tc_handle_t *handle);

/* Flushes the entries in the given chain (ie. empties chain). */
int ip6tc_flush_entries(const ip6t_chainlabel chain,
			ip6tc_handle_t *handle);

/* Zeroes the counters in a chain. */
int ip6tc_zero_entries(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

/* Creates a new chain. */
int ip6tc_create_chain(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

/* Deletes a chain. */
int ip6tc_delete_chain(const ip6t_chainlabel chain,
		       ip6tc_handle_t *handle);

/* Renames a chain. */
int ip6tc_rename_chain(const ip6t_chainlabel oldname,
		       const ip6t_chainlabel newname,
		       ip6tc_handle_t *handle);

/* Sets the policy on a built-in chain. */
int ip6tc_set_policy(const ip6t_chainlabel chain,
		     const ip6t_chainlabel policy,
		     struct ip6t_counters *counters,
		     ip6tc_handle_t *handle);

/* Get the number of references to this chain */
int ip6tc_get_references(unsigned int *ref, const ip6t_chainlabel chain,
			 ip6tc_handle_t *handle);

/* read packet and byte counters for a specific rule */
struct ip6t_counters *ip6tc_read_counter(const ip6t_chainlabel chain,
					unsigned int rulenum,
					ip6tc_handle_t *handle);

/* zero packet and byte counters for a specific rule */
int ip6tc_zero_counter(const ip6t_chainlabel chain,
		       unsigned int rulenum,
		       ip6tc_handle_t *handle);

/* set packet and byte counters for a specific rule */
int ip6tc_set_counter(const ip6t_chainlabel chain,
		      unsigned int rulenum,
		      struct ip6t_counters *counters,
		      ip6tc_handle_t *handle);

/* Makes the actual changes. */
int ip6tc_commit(ip6tc_handle_t *handle);

/* Get raw socket. */
int ip6tc_get_raw_socket();

/* Translates errno numbers into more human-readable form than strerror. */
const char *ip6tc_strerror(int err);

/* Return prefix length, or -1 if not contiguous */
int ipv6_prefix_length(const struct in6_addr *a);

#endif /* _LIBIP6TC_H */
