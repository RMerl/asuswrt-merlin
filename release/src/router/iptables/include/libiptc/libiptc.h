#ifndef _LIBIPTC_H
#define _LIBIPTC_H
/* Library which manipulates filtering rules. */

#include <libiptc/ipt_kernel_headers.h>
#include <linux/netfilter_ipv4/ip_tables.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IPT_MIN_ALIGN
/* ipt_entry has pointers and u_int64_t's in it, so if you align to
   it, you'll also align to any crazy matches and targets someone
   might write */
#define IPT_MIN_ALIGN (__alignof__(struct ipt_entry))
#endif

#define IPT_ALIGN(s) (((s) + ((IPT_MIN_ALIGN)-1)) & ~((IPT_MIN_ALIGN)-1))

typedef char ipt_chainlabel[32];

#define IPTC_LABEL_ACCEPT  "ACCEPT"
#define IPTC_LABEL_DROP    "DROP"
#define IPTC_LABEL_QUEUE   "QUEUE"
#define IPTC_LABEL_RETURN  "RETURN"

/* Transparent handle type. */
typedef struct iptc_handle *iptc_handle_t;

/* Does this chain exist? */
int iptc_is_chain(const char *chain, const iptc_handle_t handle);

/* Take a snapshot of the rules.  Returns NULL on error. */
iptc_handle_t iptc_init(const char *tablename);

/* Cleanup after iptc_init(). */
void iptc_free(iptc_handle_t *h);

/* Iterator functions to run through the chains.  Returns NULL at end. */
const char *iptc_first_chain(iptc_handle_t *handle);
const char *iptc_next_chain(iptc_handle_t *handle);

/* Get first rule in the given chain: NULL for empty chain. */
const struct ipt_entry *iptc_first_rule(const char *chain,
					iptc_handle_t *handle);

/* Returns NULL when rules run out. */
const struct ipt_entry *iptc_next_rule(const struct ipt_entry *prev,
				       iptc_handle_t *handle);

/* Returns a pointer to the target name of this entry. */
const char *iptc_get_target(const struct ipt_entry *e,
			    iptc_handle_t *handle);

/* Is this a built-in chain? */
int iptc_builtin(const char *chain, const iptc_handle_t handle);

/* Get the policy of a given built-in chain */
const char *iptc_get_policy(const char *chain,
			    struct ipt_counters *counter,
			    iptc_handle_t *handle);

/* These functions return TRUE for OK or 0 and set errno.  If errno ==
   0, it means there was a version error (ie. upgrade libiptc). */
/* Rule numbers start at 1 for the first rule. */

/* Insert the entry `e' in chain `chain' into position `rulenum'. */
int iptc_insert_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      unsigned int rulenum,
		      iptc_handle_t *handle);

/* Atomically replace rule `rulenum' in `chain' with `e'. */
int iptc_replace_entry(const ipt_chainlabel chain,
		       const struct ipt_entry *e,
		       unsigned int rulenum,
		       iptc_handle_t *handle);

/* Append entry `e' to chain `chain'.  Equivalent to insert with
   rulenum = length of chain. */
int iptc_append_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *e,
		      iptc_handle_t *handle);

/* Delete the first rule in `chain' which matches `e', subject to
   matchmask (array of length == origfw) */
int iptc_delete_entry(const ipt_chainlabel chain,
		      const struct ipt_entry *origfw,
		      unsigned char *matchmask,
		      iptc_handle_t *handle);

/* Delete the rule in position `rulenum' in `chain'. */
int iptc_delete_num_entry(const ipt_chainlabel chain,
			  unsigned int rulenum,
			  iptc_handle_t *handle);

/* Check the packet `e' on chain `chain'.  Returns the verdict, or
   NULL and sets errno. */
const char *iptc_check_packet(const ipt_chainlabel chain,
			      struct ipt_entry *entry,
			      iptc_handle_t *handle);

/* Flushes the entries in the given chain (ie. empties chain). */
int iptc_flush_entries(const ipt_chainlabel chain,
		       iptc_handle_t *handle);

/* Zeroes the counters in a chain. */
int iptc_zero_entries(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

/* Creates a new chain. */
int iptc_create_chain(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

/* Deletes a chain. */
int iptc_delete_chain(const ipt_chainlabel chain,
		      iptc_handle_t *handle);

/* Renames a chain. */
int iptc_rename_chain(const ipt_chainlabel oldname,
		      const ipt_chainlabel newname,
		      iptc_handle_t *handle);

/* Sets the policy on a built-in chain. */
int iptc_set_policy(const ipt_chainlabel chain,
		    const ipt_chainlabel policy,
		    struct ipt_counters *counters,
		    iptc_handle_t *handle);

/* Get the number of references to this chain */
int iptc_get_references(unsigned int *ref,
			const ipt_chainlabel chain,
			iptc_handle_t *handle);

/* read packet and byte counters for a specific rule */
struct ipt_counters *iptc_read_counter(const ipt_chainlabel chain,
				       unsigned int rulenum,
				       iptc_handle_t *handle);

/* zero packet and byte counters for a specific rule */
int iptc_zero_counter(const ipt_chainlabel chain,
		      unsigned int rulenum,
		      iptc_handle_t *handle);

/* set packet and byte counters for a specific rule */
int iptc_set_counter(const ipt_chainlabel chain,
		     unsigned int rulenum,
		     struct ipt_counters *counters,
		     iptc_handle_t *handle);

/* Makes the actual changes. */
int iptc_commit(iptc_handle_t *handle);

/* Get raw socket. */
int iptc_get_raw_socket();

/* Translates errno numbers into more human-readable form than strerror. */
const char *iptc_strerror(int err);

#ifdef __cplusplus
}
#endif


#endif /* _LIBIPTC_H */
