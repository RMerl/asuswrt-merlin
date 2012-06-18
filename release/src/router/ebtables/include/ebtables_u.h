/*
 * $Id: ebtables.c,v 1.03 2002/01/19
 *
 * Copyright (C) 2001-2002 Bart De Schuymer
 *
 *  This code is stongly inspired on the iptables code which is
 *  Copyright (C) 1999 Paul `Rusty' Russell & Michael J. Neuling
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef EBTABLES_U_H
#define EBTABLES_U_H
#include <netinet/in.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter/x_tables.h>

#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP		132
#endif
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP		33
#endif

#define EXEC_STYLE_PRG		0
#define EXEC_STYLE_DAEMON	1

#ifndef EBT_MIN_ALIGN
#define EBT_MIN_ALIGN (__alignof__(struct _xt_align))
#endif
#define EBT_ALIGN(s) (((s) + (EBT_MIN_ALIGN-1)) & ~(EBT_MIN_ALIGN-1))
#define ERRORMSG_MAXLEN 128

struct ebt_u_entries
{
	int policy;
	unsigned int nentries;
	/* counter offset for this chain */
	unsigned int counter_offset;
	/* used for udc */
	unsigned int hook_mask;
	char *kernel_start;
	char name[EBT_CHAIN_MAXNAMELEN];
	struct ebt_u_entry *entries;
};

struct ebt_cntchanges
{
	unsigned short type;
	unsigned short change; /* determines incremental/decremental/change */
	struct ebt_cntchanges *prev;
	struct ebt_cntchanges *next;
};

#define EBT_ORI_MAX_CHAINS 10
struct ebt_u_replace
{
	char name[EBT_TABLE_MAXNAMELEN];
	unsigned int valid_hooks;
	/* nr of rules in the table */
	unsigned int nentries;
	unsigned int num_chains;
	unsigned int max_chains;
	struct ebt_u_entries **chains;
	/* nr of counters userspace expects back */
	unsigned int num_counters;
	/* where the kernel will put the old counters */
	struct ebt_counter *counters;
	/*
	 * can be used e.g. to know if a standard option
	 * has been specified twice
	 */
	unsigned int flags;
	/* we stick the specified command (e.g. -A) in here */
	char command;
	/*
	 * here we stick the chain to do our thing on (can be -1 if unspecified)
	 */
	int selected_chain;
	/* used for the atomic option */
	char *filename;
	/* tells what happened to the old rules (counter changes) */
	struct ebt_cntchanges *cc;
};

struct ebt_u_table
{
	char name[EBT_TABLE_MAXNAMELEN];
	void (*check)(struct ebt_u_replace *repl);
	void (*help)(const char **);
	struct ebt_u_table *next;
};

struct ebt_u_match_list
{
	struct ebt_u_match_list *next;
	struct ebt_entry_match *m;
};

struct ebt_u_watcher_list
{
	struct ebt_u_watcher_list *next;
	struct ebt_entry_watcher *w;
};

struct ebt_u_entry
{
	unsigned int bitmask;
	unsigned int invflags;
	uint16_t ethproto;
	char in[IFNAMSIZ];
	char logical_in[IFNAMSIZ];
	char out[IFNAMSIZ];
	char logical_out[IFNAMSIZ];
	unsigned char sourcemac[ETH_ALEN];
	unsigned char sourcemsk[ETH_ALEN];
	unsigned char destmac[ETH_ALEN];
	unsigned char destmsk[ETH_ALEN];
	struct ebt_u_match_list *m_list;
	struct ebt_u_watcher_list *w_list;
	struct ebt_entry_target *t;
	struct ebt_u_entry *prev;
	struct ebt_u_entry *next;
	struct ebt_counter cnt;
	struct ebt_counter cnt_surplus; /* for increasing/decreasing a counter and for option 'C' */
	struct ebt_cntchanges *cc;
	/* the standard target needs this to know the name of a udc when
	 * printing out rules. */
	struct ebt_u_replace *replace;
};

struct ebt_u_match
{
	char name[EBT_FUNCTION_MAXNAMELEN];
	/* size of the real match data */
	unsigned int size;
	void (*help)(void);
	void (*init)(struct ebt_entry_match *m);
	int (*parse)(int c, char **argv, int argc,
	        const struct ebt_u_entry *entry, unsigned int *flags,
	        struct ebt_entry_match **match);
	void (*final_check)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_match *match,
	   const char *name, unsigned int hookmask, unsigned int time);
	void (*print)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_match *match);
	int (*compare)(const struct ebt_entry_match *m1,
	   const struct ebt_entry_match *m2);
	const struct option *extra_ops;
	/*
	 * can be used e.g. to check for multiple occurance of the same option
	 */
	unsigned int flags;
	unsigned int option_offset;
	struct ebt_entry_match *m;
	/*
	 * if used == 1 we no longer have to add it to
	 * the match chain of the new entry
	 * be sure to put it back on 0 when finished
	 */
	unsigned int used;
	struct ebt_u_match *next;
};

struct ebt_u_watcher
{
	char name[EBT_FUNCTION_MAXNAMELEN];
	unsigned int size;
	void (*help)(void);
	void (*init)(struct ebt_entry_watcher *w);
	int (*parse)(int c, char **argv, int argc,
	   const struct ebt_u_entry *entry, unsigned int *flags,
	   struct ebt_entry_watcher **watcher);
	void (*final_check)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_watcher *watch, const char *name,
	   unsigned int hookmask, unsigned int time);
	void (*print)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_watcher *watcher);
	int (*compare)(const struct ebt_entry_watcher *w1,
	   const struct ebt_entry_watcher *w2);
	const struct option *extra_ops;
	unsigned int flags;
	unsigned int option_offset;
	struct ebt_entry_watcher *w;
	unsigned int used;
	struct ebt_u_watcher *next;
};

struct ebt_u_target
{
	char name[EBT_FUNCTION_MAXNAMELEN];
	unsigned int size;
	void (*help)(void);
	void (*init)(struct ebt_entry_target *t);
	int (*parse)(int c, char **argv, int argc,
	   const struct ebt_u_entry *entry, unsigned int *flags,
	   struct ebt_entry_target **target);
	void (*final_check)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_target *target, const char *name,
	   unsigned int hookmask, unsigned int time);
	void (*print)(const struct ebt_u_entry *entry,
	   const struct ebt_entry_target *target);
	int (*compare)(const struct ebt_entry_target *t1,
	   const struct ebt_entry_target *t2);
	const struct option *extra_ops;
	unsigned int option_offset;
	unsigned int flags;
	struct ebt_entry_target *t;
	unsigned int used;
	struct ebt_u_target *next;
};

/* libebtc.c */

extern struct ebt_u_table *ebt_tables;
extern struct ebt_u_match *ebt_matches;
extern struct ebt_u_watcher *ebt_watchers;
extern struct ebt_u_target *ebt_targets;

void ebt_register_table(struct ebt_u_table *);
void ebt_register_match(struct ebt_u_match *);
void ebt_register_watcher(struct ebt_u_watcher *);
void ebt_register_target(struct ebt_u_target *t);
int ebt_get_kernel_table(struct ebt_u_replace *replace, int init);
struct ebt_u_target *ebt_find_target(const char *name);
struct ebt_u_match *ebt_find_match(const char *name);
struct ebt_u_watcher *ebt_find_watcher(const char *name);
struct ebt_u_table *ebt_find_table(const char *name);
int ebtables_insmod(const char *modname);
void ebt_list_extensions();
void ebt_initialize_entry(struct ebt_u_entry *e);
void ebt_cleanup_replace(struct ebt_u_replace *replace);
void ebt_reinit_extensions();
void ebt_double_chains(struct ebt_u_replace *replace);
void ebt_free_u_entry(struct ebt_u_entry *e);
struct ebt_u_entries *ebt_name_to_chain(const struct ebt_u_replace *replace,
				    const char* arg);
struct ebt_u_entries *ebt_name_to_chain(const struct ebt_u_replace *replace,
				    const char* arg);
int ebt_get_chainnr(const struct ebt_u_replace *replace, const char* arg);
/**/
void ebt_change_policy(struct ebt_u_replace *replace, int policy);
void ebt_flush_chains(struct ebt_u_replace *replace);
int ebt_check_rule_exists(struct ebt_u_replace *replace,
			  struct ebt_u_entry *new_entry);
void ebt_add_rule(struct ebt_u_replace *replace, struct ebt_u_entry *new_entry,
		  int rule_nr);
void ebt_delete_rule(struct ebt_u_replace *replace,
		     struct ebt_u_entry *new_entry, int begin, int end);
void ebt_zero_counters(struct ebt_u_replace *replace);
void ebt_change_counters(struct ebt_u_replace *replace,
		     struct ebt_u_entry *new_entry, int begin, int end,
		     struct ebt_counter *cnt, int mask);
void ebt_new_chain(struct ebt_u_replace *replace, const char *name, int policy);
void ebt_delete_chain(struct ebt_u_replace *replace);
void ebt_rename_chain(struct ebt_u_replace *replace, const char *name);
/**/
void ebt_do_final_checks(struct ebt_u_replace *replace, struct ebt_u_entry *e,
			 struct ebt_u_entries *entries);
int ebt_check_for_references(struct ebt_u_replace *replace, int print_err);
int ebt_check_for_references2(struct ebt_u_replace *replace, int chain_nr,
			      int print_err);
void ebt_check_for_loops(struct ebt_u_replace *replace);
void ebt_add_match(struct ebt_u_entry *new_entry, struct ebt_u_match *m);
void ebt_add_watcher(struct ebt_u_entry *new_entry, struct ebt_u_watcher *w);
void ebt_iterate_matches(void (*f)(struct ebt_u_match *));
void ebt_iterate_watchers(void (*f)(struct ebt_u_watcher *));
void ebt_iterate_targets(void (*f)(struct ebt_u_target *));
void __ebt_print_bug(char *file, int line, char *format, ...);
void __ebt_print_error(char *format, ...);

/* communication.c */

int ebt_get_table(struct ebt_u_replace *repl, int init);
void ebt_deliver_counters(struct ebt_u_replace *repl);
void ebt_deliver_table(struct ebt_u_replace *repl);

/* useful_functions.c */

extern int ebt_invert;
void ebt_check_option(unsigned int *flags, unsigned int mask);
#define ebt_check_inverse(arg) _ebt_check_inverse(arg, argc, argv)
int _ebt_check_inverse(const char option[], int argc, char **argv);
void ebt_print_mac(const unsigned char *mac);
void ebt_print_mac_and_mask(const unsigned char *mac, const unsigned char *mask);
int ebt_get_mac_and_mask(const char *from, unsigned char *to, unsigned char *mask);
void ebt_parse_ip_address(char *address, uint32_t *addr, uint32_t *msk);
char *ebt_mask_to_dotted(uint32_t mask);
void ebt_parse_ip6_address(char *address, struct in6_addr *addr, 
						   struct in6_addr *msk);
char *ebt_ip6_to_numeric(const struct in6_addr *addrp);


int do_command(int argc, char *argv[], int exec_style,
               struct ebt_u_replace *replace_);

struct ethertypeent *parseethertypebynumber(int type);

#define ebt_to_chain(repl)				\
({struct ebt_u_entries *_ch = NULL;			\
if (repl->selected_chain != -1)				\
	_ch = repl->chains[repl->selected_chain];	\
_ch;})
#define ebt_print_bug(format, args...) \
   __ebt_print_bug(__FILE__, __LINE__, format, ##args)
#define ebt_print_error(format,args...) __ebt_print_error(format, ##args);
#define ebt_print_error2(format, args...) do {__ebt_print_error(format, ##args); \
   return -1;} while (0)
#define ebt_check_option2(flags,mask)	\
({ebt_check_option(flags,mask);		\
 if (ebt_errormsg[0] != '\0')		\
	return -1;})
#define ebt_check_inverse2(option)					\
({int __ret = ebt_check_inverse(option);				\
if (ebt_errormsg[0] != '\0')						\
	return -1;							\
if (!optarg) {								\
	__ebt_print_error("Option without (mandatory) argument");	\
	return -1;							\
}									\
__ret;})
#define ebt_print_memory() do {printf("Ebtables: " __FILE__ \
   " %s %d :Out of memory.\n", __FUNCTION__, __LINE__); exit(-1);} while (0)

/* used for keeping the rule counters right during rule adds or deletes */
#define CNT_NORM 	0
#define CNT_DEL 	1
#define CNT_ADD 	2
#define CNT_CHANGE 	3

extern const char *ebt_hooknames[NF_BR_NUMHOOKS];
extern const char *ebt_standard_targets[NUM_STANDARD_TARGETS];
extern char ebt_errormsg[ERRORMSG_MAXLEN];
extern char *ebt_modprobe;
extern int ebt_silent;
extern int ebt_printstyle_mac;

/*
 * Transforms a target string into the right integer,
 * returns 0 on success.
 */
#define FILL_TARGET(_str, _pos) ({                            \
	int _i, _ret = 0;                                     \
	for (_i = 0; _i < NUM_STANDARD_TARGETS; _i++)         \
		if (!strcmp(_str, ebt_standard_targets[_i])) {\
			_pos = -_i - 1;                       \
			break;                                \
		}                                             \
	if (_i == NUM_STANDARD_TARGETS)                       \
		_ret = 1;                                     \
	_ret;                                                 \
})

/* Transforms the target value to an index into standard_targets[] */
#define TARGET_INDEX(_value) (-_value - 1)
/* Returns a target string corresponding to the value */
#define TARGET_NAME(_value) (ebt_standard_targets[TARGET_INDEX(_value)])
/* True if the hook mask denotes that the rule is in a base chain */
#define BASE_CHAIN (hookmask & (1 << NF_BR_NUMHOOKS))
/* Clear the bit in the hook_mask that tells if the rule is on a base chain */
#define CLEAR_BASE_CHAIN_BIT (hookmask &= ~(1 << NF_BR_NUMHOOKS))
#define PRINT_VERSION printf(PROGNAME" v"PROGVERSION" ("PROGDATE")\n")
#ifndef PROC_SYS_MODPROBE
#define PROC_SYS_MODPROBE "/proc/sys/kernel/modprobe"
#endif
#define ATOMIC_ENV_VARIABLE "EBTABLES_ATOMIC_FILE"
#endif /* EBTABLES_U_H */
