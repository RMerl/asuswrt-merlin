#ifndef _IPTABLES_USER_H
#define _IPTABLES_USER_H

#include <netinet/ip.h>
#include <xtables.h>
#include <libiptc/libiptc.h>
#include <iptables/internal.h>

/* Your shared library should call one of these. */
extern int do_command4(int argc, char *argv[], char **table,
		      struct xtc_handle **handle);
extern int delete_chain4(const xt_chainlabel chain, int verbose,
			struct xtc_handle *handle);
extern int flush_entries4(const xt_chainlabel chain, int verbose, 
			struct xtc_handle *handle);
extern int for_each_chain4(int (*fn)(const xt_chainlabel, int, struct xtc_handle *),
		int verbose, int builtinstoo, struct xtc_handle *handle);
extern void print_rule4(const struct ipt_entry *e,
		struct xtc_handle *handle, const char *chain, int counters);

extern struct xtables_globals iptables_globals;

#endif /*_IPTABLES_USER_H*/
