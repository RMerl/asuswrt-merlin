#ifndef _IP6TABLES_USER_H
#define _IP6TABLES_USER_H

#include <netinet/ip.h>
#include <xtables.h>
#include <libiptc/libip6tc.h>
#include <iptables/internal.h>

/* Your shared library should call one of these. */
extern int do_command6(int argc, char *argv[], char **table,
		       struct xtc_handle **handle);

extern int for_each_chain6(int (*fn)(const xt_chainlabel, int, struct xtc_handle *), int verbose, int builtinstoo, struct xtc_handle *handle);
extern int flush_entries6(const xt_chainlabel chain, int verbose, struct xtc_handle *handle);
extern int delete_chain6(const xt_chainlabel chain, int verbose, struct xtc_handle *handle);
void print_rule6(const struct ip6t_entry *e, struct xtc_handle *h, const char *chain, int counters);

extern struct xtables_globals ip6tables_globals;

#endif /*_IP6TABLES_USER_H*/
