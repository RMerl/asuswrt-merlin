#ifndef _IPTABLES_USER_H
#define _IPTABLES_USER_H

#include "iptables_common.h"
#include "libiptc/libiptc.h"

#ifndef IPT_LIB_DIR
#define IPT_LIB_DIR "/usr/local/lib/iptables"
#endif

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#ifndef IPT_SO_GET_REVISION_MATCH /* Old kernel source. */
#define IPT_SO_GET_REVISION_MATCH	(IPT_BASE_CTL + 2)
#define IPT_SO_GET_REVISION_TARGET	(IPT_BASE_CTL + 3)

struct ipt_get_revision
{
	char name[IPT_FUNCTION_MAXNAMELEN-1];

	u_int8_t revision;
};
#endif /* IPT_SO_GET_REVISION_MATCH   Old kernel source */

struct iptables_rule_match
{
	struct iptables_rule_match *next;

	struct iptables_match *match;
};

/* Include file for additions: new matches and targets. */
struct iptables_match
{
	struct iptables_match *next;

	ipt_chainlabel name;

	/* Revision of match (0 by default). */
	u_int8_t revision;

	const char *version;

	/* Size of match data. */
	size_t size;

	/* Size of match data relevent for userspace comparison purposes */
	size_t userspacesize;

	/* Function which prints out usage message. */
	void (*help)(void);

	/* Initialize the match. */
	void (*init)(struct ipt_entry_match *m, unsigned int *nfcache);

	/* Function which parses command options; returns true if it
           ate an option */
	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ipt_entry *entry,
		     unsigned int *nfcache,
		     struct ipt_entry_match **match);

	/* Final check; exit if not ok. */
	void (*final_check)(unsigned int flags);

	/* Prints out the match iff non-NULL: put space at end */
	void (*print)(const struct ipt_ip *ip,
		      const struct ipt_entry_match *match, int numeric);

	/* Saves the match info in parsable form to stdout. */
	void (*save)(const struct ipt_ip *ip,
		     const struct ipt_entry_match *match);

	/* Pointer to list of extra command-line options */
	const struct option *extra_opts;

	/* Ignore these men behind the curtain: */
	unsigned int option_offset;
	struct ipt_entry_match *m;
	unsigned int mflags;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; /* simulate loading so options are merged properly */
#endif
};

struct iptables_target
{
	struct iptables_target *next;

	ipt_chainlabel name;

	/* Revision of target (0 by default). */
	u_int8_t revision;

	const char *version;

	/* Size of target data. */
	size_t size;

	/* Size of target data relevent for userspace comparison purposes */
	size_t userspacesize;

	/* Function which prints out usage message. */
	void (*help)(void);

	/* Initialize the target. */
	void (*init)(struct ipt_entry_target *t, unsigned int *nfcache);

	/* Function which parses command options; returns true if it
           ate an option */
	int (*parse)(int c, char **argv, int invert, unsigned int *flags,
		     const struct ipt_entry *entry,
		     struct ipt_entry_target **target);

	/* Final check; exit if not ok. */
	void (*final_check)(unsigned int flags);

	/* Prints out the target iff non-NULL: put space at end */
	void (*print)(const struct ipt_ip *ip,
		      const struct ipt_entry_target *target, int numeric);

	/* Saves the targinfo in parsable form to stdout. */
	void (*save)(const struct ipt_ip *ip,
		     const struct ipt_entry_target *target);

	/* Pointer to list of extra command-line options */
	struct option *extra_opts;

	/* Ignore these men behind the curtain: */
	unsigned int option_offset;
	struct ipt_entry_target *t;
	unsigned int tflags;
	unsigned int used;
#ifdef NO_SHARED_LIBS
	unsigned int loaded; /* simulate loading so options are merged properly */
#endif
};

extern int line;

/* Your shared library should call one of these. */
extern void register_match(struct iptables_match *me);
extern void register_target(struct iptables_target *me);

extern struct in_addr *dotted_to_addr(const char *dotted);
extern char *addr_to_dotted(const struct in_addr *addrp);
extern char *addr_to_anyname(const struct in_addr *addr);
extern char *mask_to_dotted(const struct in_addr *mask);

extern void parse_hostnetworkmask(const char *name, struct in_addr **addrpp,
                      struct in_addr *maskp, unsigned int *naddrs);
extern u_int16_t parse_protocol(const char *s);

extern int do_command(int argc, char *argv[], char **table,
		      iptc_handle_t *handle);
/* Keeping track of external matches and targets: linked lists.  */
extern struct iptables_match *iptables_matches;
extern struct iptables_target *iptables_targets;

enum ipt_tryload {
	DONT_LOAD,
	TRY_LOAD,
	LOAD_MUST_SUCCEED
};

extern struct iptables_target *find_target(const char *name, enum ipt_tryload);
extern struct iptables_match *find_match(const char *name, enum ipt_tryload, struct iptables_rule_match **match);

extern int delete_chain(const ipt_chainlabel chain, int verbose,
			iptc_handle_t *handle);
extern int flush_entries(const ipt_chainlabel chain, int verbose,
			iptc_handle_t *handle);
extern int for_each_chain(int (*fn)(const ipt_chainlabel, int, iptc_handle_t *),
		int verbose, int builtinstoo, iptc_handle_t *handle);
#endif /*_IPTABLES_USER_H*/
