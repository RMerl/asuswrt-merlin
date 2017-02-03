/*
 * ebtables.c, v2.0 July 2002
 *
 * Author: Bart De Schuymer
 *
 *  This code was stongly inspired on the iptables code which is
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

#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <signal.h>
#include "include/ebtables_u.h"
#include "include/ethernetdb.h"

/* Checks whether a command has already been specified */
#define OPT_COMMANDS (replace->flags & OPT_COMMAND || replace->flags & OPT_ZERO)

#define OPT_COMMAND	0x01
#define OPT_TABLE	0x02
#define OPT_IN		0x04
#define OPT_OUT		0x08
#define OPT_JUMP	0x10
#define OPT_PROTOCOL	0x20
#define OPT_SOURCE	0x40
#define OPT_DEST	0x80
#define OPT_ZERO	0x100
#define OPT_LOGICALIN	0x200
#define OPT_LOGICALOUT	0x400
#define OPT_KERNELDATA	0x800 /* This value is also defined in ebtablesd.c */
#define OPT_COUNT	0x1000 /* This value is also defined in libebtc.c */
#define OPT_CNT_INCR	0x2000 /* This value is also defined in libebtc.c */
#define OPT_CNT_DECR	0x4000 /* This value is also defined in libebtc.c */

/* Default command line options. Do not mess around with the already
 * assigned numbers unless you know what you are doing */
static struct option ebt_original_options[] =
{
	{ "append"         , required_argument, 0, 'A' },
	{ "insert"         , required_argument, 0, 'I' },
	{ "delete"         , required_argument, 0, 'D' },
	{ "list"           , optional_argument, 0, 'L' },
	{ "Lc"             , no_argument      , 0, 4   },
	{ "Ln"             , no_argument      , 0, 5   },
	{ "Lx"             , no_argument      , 0, 6   },
	{ "Lmac2"          , no_argument      , 0, 12  },
	{ "zero"           , optional_argument, 0, 'Z' },
	{ "flush"          , optional_argument, 0, 'F' },
	{ "policy"         , required_argument, 0, 'P' },
	{ "in-interface"   , required_argument, 0, 'i' },
	{ "in-if"          , required_argument, 0, 'i' },
	{ "logical-in"     , required_argument, 0, 2   },
	{ "logical-out"    , required_argument, 0, 3   },
	{ "out-interface"  , required_argument, 0, 'o' },
	{ "out-if"         , required_argument, 0, 'o' },
	{ "version"        , no_argument      , 0, 'V' },
	{ "help"           , no_argument      , 0, 'h' },
	{ "jump"           , required_argument, 0, 'j' },
	{ "set-counters"   , required_argument, 0, 'c' },
	{ "change-counters", required_argument, 0, 'C' },
	{ "proto"          , required_argument, 0, 'p' },
	{ "protocol"       , required_argument, 0, 'p' },
	{ "db"             , required_argument, 0, 'b' },
	{ "source"         , required_argument, 0, 's' },
	{ "src"            , required_argument, 0, 's' },
	{ "destination"    , required_argument, 0, 'd' },
	{ "dst"            , required_argument, 0, 'd' },
	{ "table"          , required_argument, 0, 't' },
	{ "modprobe"       , required_argument, 0, 'M' },
	{ "new-chain"      , required_argument, 0, 'N' },
	{ "rename-chain"   , required_argument, 0, 'E' },
	{ "delete-chain"   , optional_argument, 0, 'X' },
	{ "atomic-init"    , no_argument      , 0, 7   },
	{ "atomic-commit"  , no_argument      , 0, 8   },
	{ "atomic-file"    , required_argument, 0, 9   },
	{ "atomic-save"    , no_argument      , 0, 10  },
	{ "init-table"     , no_argument      , 0, 11  },
	{ "concurrent"     , no_argument      , 0, 13  },
	{ 0 }
};

static struct option *ebt_options = ebt_original_options;

/* Holds all the data */
static struct ebt_u_replace *replace;

/* The chosen table */
static struct ebt_u_table *table;

/* The pointers in here are special:
 * The struct ebt_target pointer is actually a struct ebt_u_target pointer.
 * I do not feel like using a union.
 * We need a struct ebt_u_target pointer because we know the address of the data
 * they point to won't change. We want to allow that the struct ebt_u_target.t
 * member can change.
 * The same holds for the struct ebt_match and struct ebt_watcher pointers */
static struct ebt_u_entry *new_entry;


static int global_option_offset;
#define OPTION_OFFSET 256
static struct option *merge_options(struct option *oldopts,
   const struct option *newopts, unsigned int *options_offset)
{
	unsigned int num_old, num_new, i;
	struct option *merge;

	if (!newopts || !oldopts || !options_offset)
		ebt_print_bug("merge wrong");
	for (num_old = 0; oldopts[num_old].name; num_old++);
	for (num_new = 0; newopts[num_new].name; num_new++);

	global_option_offset += OPTION_OFFSET;
	*options_offset = global_option_offset;

	merge = malloc(sizeof(struct option) * (num_new + num_old + 1));
	if (!merge)
		ebt_print_memory();
	memcpy(merge, oldopts, num_old * sizeof(struct option));
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *options_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof(struct option));
	/* Only free dynamically allocated stuff */
	if (oldopts != ebt_original_options)
		free(oldopts);

	return merge;
}

static void merge_match(struct ebt_u_match *m)
{
	ebt_options = merge_options
	   (ebt_options, m->extra_ops, &(m->option_offset));
}

static void merge_watcher(struct ebt_u_watcher *w)
{
	ebt_options = merge_options
	   (ebt_options, w->extra_ops, &(w->option_offset));
}

static void merge_target(struct ebt_u_target *t)
{
	ebt_options = merge_options
	   (ebt_options, t->extra_ops, &(t->option_offset));
}

/* Be backwards compatible, so don't use '+' in kernel */
#define IF_WILDCARD 1
static void print_iface(const char *iface)
{
	char *c;

	if ((c = strchr(iface, IF_WILDCARD)))
		*c = '+';
	printf("%s ", iface);
	if (c)
		*c = IF_WILDCARD;
}

/* We use replace->flags, so we can't use the following values:
 * 0x01 == OPT_COMMAND, 0x02 == OPT_TABLE, 0x100 == OPT_ZERO */
#define LIST_N    0x04
#define LIST_C    0x08
#define LIST_X    0x10
#define LIST_MAC2 0x20

/* Helper function for list_rules() */
static void list_em(struct ebt_u_entries *entries)
{
	int i, j, space = 0, digits;
	struct ebt_u_entry *hlp;
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;
	struct ebt_u_match *m;
	struct ebt_u_watcher *w;
	struct ebt_u_target *t;

	if (replace->flags & LIST_MAC2)
		ebt_printstyle_mac = 2;
	else
		ebt_printstyle_mac = 0;
	hlp = entries->entries->next;
	if (replace->flags & LIST_X && entries->policy != EBT_ACCEPT) {
		printf("ebtables -t %s -P %s %s\n", replace->name,
		   entries->name, ebt_standard_targets[-entries->policy - 1]);
	} else if (!(replace->flags & LIST_X)) {
		printf("\nBridge chain: %s, entries: %d, policy: %s\n",
		   entries->name, entries->nentries,
		   ebt_standard_targets[-entries->policy - 1]);
	}

	if (replace->flags & LIST_N) {
		i = entries->nentries;
		while (i > 9) {
			space++;
			i /= 10;
		}
	}

	for (i = 0; i < entries->nentries; i++) {
		if (replace->flags & LIST_N) {
			digits = 0;
			/* A little work to get nice rule numbers. */
			j = i + 1;
			while (j > 9) {
				digits++;
				j /= 10;
			}
			for (j = 0; j < space - digits; j++)
				printf(" ");
			printf("%d. ", i + 1);
		}
		if (replace->flags & LIST_X)
			printf("ebtables -t %s -A %s ",
			   replace->name, entries->name);

		/* The standard target's print() uses this to find out
		 * the name of a udc */
		hlp->replace = replace;

		/* Don't print anything about the protocol if no protocol was
		 * specified, obviously this means any protocol will do. */
		if (!(hlp->bitmask & EBT_NOPROTO)) {
			printf("-p ");
			if (hlp->invflags & EBT_IPROTO)
				printf("! ");
			if (hlp->bitmask & EBT_802_3)
				printf("Length ");
			else {
				struct ethertypeent *ent;

				ent = getethertypebynumber(ntohs(hlp->ethproto));
				if (!ent)
					printf("0x%x ", ntohs(hlp->ethproto));
				else
					printf("%s ", ent->e_name);
			}
		}
		if (hlp->bitmask & EBT_SOURCEMAC) {
			printf("-s ");
			if (hlp->invflags & EBT_ISOURCE)
				printf("! ");
			ebt_print_mac_and_mask(hlp->sourcemac, hlp->sourcemsk);
			printf(" ");
		}
		if (hlp->bitmask & EBT_DESTMAC) {
			printf("-d ");
			if (hlp->invflags & EBT_IDEST)
				printf("! ");
			ebt_print_mac_and_mask(hlp->destmac, hlp->destmsk);
			printf(" ");
		}
		if (hlp->in[0] != '\0') {
			printf("-i ");
			if (hlp->invflags & EBT_IIN)
				printf("! ");
			print_iface(hlp->in);
		}
		if (hlp->logical_in[0] != '\0') {
			printf("--logical-in ");
			if (hlp->invflags & EBT_ILOGICALIN)
				printf("! ");
			print_iface(hlp->logical_in);
		}
		if (hlp->logical_out[0] != '\0') {
			printf("--logical-out ");
			if (hlp->invflags & EBT_ILOGICALOUT)
				printf("! ");
			print_iface(hlp->logical_out);
		}
		if (hlp->out[0] != '\0') {
			printf("-o ");
			if (hlp->invflags & EBT_IOUT)
				printf("! ");
			print_iface(hlp->out);
		}

		m_l = hlp->m_list;
		while (m_l) {
			m = ebt_find_match(m_l->m->u.name);
			if (!m)
				ebt_print_bug("Match not found");
			m->print(hlp, m_l->m);
			m_l = m_l->next;
		}
		w_l = hlp->w_list;
		while (w_l) {
			w = ebt_find_watcher(w_l->w->u.name);
			if (!w)
				ebt_print_bug("Watcher not found");
			w->print(hlp, w_l->w);
			w_l = w_l->next;
		}

		printf("-j ");
		if (strcmp(hlp->t->u.name, EBT_STANDARD_TARGET))
			printf("%s ", hlp->t->u.name);
		t = ebt_find_target(hlp->t->u.name);
		if (!t)
			ebt_print_bug("Target '%s' not found", hlp->t->u.name);
		t->print(hlp, hlp->t);
		if (replace->flags & LIST_C) {
			uint64_t pcnt = hlp->cnt.pcnt;
			uint64_t bcnt = hlp->cnt.bcnt;

			if (replace->flags & LIST_X)
				printf("-c %"PRIu64" %"PRIu64, pcnt, bcnt);
			else
				printf(", pcnt = %"PRIu64" -- bcnt = %"PRIu64, pcnt, bcnt);
		}
		printf("\n");
		hlp = hlp->next;
	}
}

static void print_help()
{
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;

	PRINT_VERSION;
	printf(
"Usage:\n"
"ebtables -[ADI] chain rule-specification [options]\n"
"ebtables -P chain target\n"
"ebtables -[LFZ] [chain]\n"
"ebtables -[NX] [chain]\n"
"ebtables -E old-chain-name new-chain-name\n\n"
"Commands:\n"
"--append -A chain             : append to chain\n"
"--delete -D chain             : delete matching rule from chain\n"
"--delete -D chain rulenum     : delete rule at position rulenum from chain\n"
"--change-counters -C chain\n"
"          [rulenum] pcnt bcnt : change counters of existing rule\n"
"--insert -I chain rulenum     : insert rule at position rulenum in chain\n"
"--list   -L [chain]           : list the rules in a chain or in all chains\n"
"--flush  -F [chain]           : delete all rules in chain or in all chains\n"
"--init-table                  : replace the kernel table with the initial table\n"
"--zero   -Z [chain]           : put counters on zero in chain or in all chains\n"
"--policy -P chain target      : change policy on chain to target\n"
"--new-chain -N chain          : create a user defined chain\n"
"--rename-chain -E old new     : rename a chain\n"
"--delete-chain -X [chain]     : delete a user defined chain\n"
"--atomic-commit               : update the kernel w/t table contained in <FILE>\n"
"--atomic-init                 : put the initial kernel table into <FILE>\n"
"--atomic-save                 : put the current kernel table into <FILE>\n"
"--atomic-file file            : set <FILE> to file\n\n"
"Options:\n"
"--proto  -p [!] proto         : protocol hexadecimal, by name or LENGTH\n"
"--src    -s [!] address[/mask]: source mac address\n"
"--dst    -d [!] address[/mask]: destination mac address\n"
"--in-if  -i [!] name[+]       : network input interface name\n"
"--out-if -o [!] name[+]       : network output interface name\n"
"--logical-in  [!] name[+]     : logical bridge input interface name\n"
"--logical-out [!] name[+]     : logical bridge output interface name\n"
"--set-counters -c chain\n"
"          pcnt bcnt           : set the counters of the to be added rule\n"
"--modprobe -M program         : try to insert modules using this program\n"
"--concurrent                  : use a file lock to support concurrent scripts\n"
"--version -V                  : print package version\n\n"
"Environment variable:\n"
ATOMIC_ENV_VARIABLE "          : if set <FILE> (see above) will equal its value"
"\n\n");
	m_l = new_entry->m_list;
	while (m_l) {
		((struct ebt_u_match *)m_l->m)->help();
		printf("\n");
		m_l = m_l->next;
	}
	w_l = new_entry->w_list;
	while (w_l) {
		((struct ebt_u_watcher *)w_l->w)->help();
		printf("\n");
		w_l = w_l->next;
	}
	((struct ebt_u_target *)new_entry->t)->help();
	printf("\n");
	if (table->help)
		table->help(ebt_hooknames);
}

/* Execute command L */
static void list_rules()
{
	int i;

	if (!(replace->flags & LIST_X))
		printf("Bridge table: %s\n", table->name);
	if (replace->selected_chain != -1)
		list_em(ebt_to_chain(replace));
	else {
		/* Create new chains and rename standard chains when necessary */
		if (replace->flags & LIST_X && replace->num_chains > NF_BR_NUMHOOKS) {
			for (i = NF_BR_NUMHOOKS; i < replace->num_chains; i++)
				printf("ebtables -t %s -N %s\n", replace->name, replace->chains[i]->name);
			for (i = 0; i < NF_BR_NUMHOOKS; i++)
				if (replace->chains[i] && strcmp(replace->chains[i]->name, ebt_hooknames[i]))
					printf("ebtables -t %s -E %s %s\n", replace->name, ebt_hooknames[i], replace->chains[i]->name);
		}
		for (i = 0; i < replace->num_chains; i++)
			if (replace->chains[i])
				list_em(replace->chains[i]);
	}
}

static int parse_rule_range(const char *argv, int *rule_nr, int *rule_nr_end)
{
	char *colon = strchr(argv, ':'), *buffer;

	if (colon) {
		*colon = '\0';
		if (*(colon + 1) == '\0')
			*rule_nr_end = -1; /* Until the last rule */
		else {
			*rule_nr_end = strtol(colon + 1, &buffer, 10);
			if (*buffer != '\0' || *rule_nr_end == 0)
				return -1;
		}
	}
	if (colon == argv)
		*rule_nr = 1; /* Beginning with the first rule */
	else {
		*rule_nr = strtol(argv, &buffer, 10);
		if (*buffer != '\0' || *rule_nr == 0)
			return -1;
	}
	if (!colon)
		*rule_nr_end = *rule_nr;
	return 0;
}

/* Incrementing or decrementing rules in daemon mode is not supported as the
 * involved code overload is not worth it (too annoying to take the increased
 * counters in the kernel into account). */
static int parse_change_counters_rule(int argc, char **argv, int *rule_nr, int *rule_nr_end, int exec_style)
{
	char *buffer;
	int ret = 0;

	if (optind + 1 >= argc || (argv[optind][0] == '-' && (argv[optind][1] < '0' || argv[optind][1] > '9')) ||
	    (argv[optind + 1][0] == '-' && (argv[optind + 1][1] < '0'  && argv[optind + 1][1] > '9')))
		ebt_print_error2("The command -C needs at least 2 arguments");
	if (optind + 2 < argc && (argv[optind + 2][0] != '-' || (argv[optind + 2][1] >= '0' && argv[optind + 2][1] <= '9'))) {
		if (optind + 3 != argc)
			ebt_print_error2("No extra options allowed with -C start_nr[:end_nr] pcnt bcnt");
		if (parse_rule_range(argv[optind], rule_nr, rule_nr_end))
			ebt_print_error2("Something is wrong with the rule number specification '%s'", argv[optind]);
		optind++;
	}

	if (argv[optind][0] == '+') {
		if (exec_style == EXEC_STYLE_DAEMON)
daemon_incr:
			ebt_print_error2("Incrementing rule counters (%s) not allowed in daemon mode", argv[optind]);
		ret += 1;
		new_entry->cnt_surplus.pcnt = strtoull(argv[optind] + 1, &buffer, 10);
	} else if (argv[optind][0] == '-') {
		if (exec_style == EXEC_STYLE_DAEMON)
daemon_decr:
			ebt_print_error2("Decrementing rule counters (%s) not allowed in daemon mode", argv[optind]);
		ret += 2;
		new_entry->cnt_surplus.pcnt = strtoull(argv[optind] + 1, &buffer, 10);
	} else
		new_entry->cnt_surplus.pcnt = strtoull(argv[optind], &buffer, 10);

	if (*buffer != '\0')
		goto invalid;
	optind++;
	if (argv[optind][0] == '+') {
		if (exec_style == EXEC_STYLE_DAEMON)
			goto daemon_incr;
		ret += 3;
		new_entry->cnt_surplus.bcnt = strtoull(argv[optind] + 1, &buffer, 10);
	} else if (argv[optind][0] == '-') {
		if (exec_style == EXEC_STYLE_DAEMON)
			goto daemon_decr;
		ret += 6;
		new_entry->cnt_surplus.bcnt = strtoull(argv[optind] + 1, &buffer, 10);
	} else
		new_entry->cnt_surplus.bcnt = strtoull(argv[optind], &buffer, 10);

	if (*buffer != '\0')
		goto invalid;
	optind++;
	return ret;
invalid:
	ebt_print_error2("Packet counter '%s' invalid", argv[optind]);
}

static int parse_iface(char *iface, char *option)
{
	char *c;

	if ((c = strchr(iface, '+'))) {
		if (*(c + 1) != '\0') {
			ebt_print_error("Spurious characters after '+' wildcard for '%s'", option);
			return -1;
		} else
			*c = IF_WILDCARD;
	}
	return 0;
}

void ebt_early_init_once()
{
	ebt_iterate_matches(merge_match);
	ebt_iterate_watchers(merge_watcher);
	ebt_iterate_targets(merge_target);
}

/* signal handler, installed when the option --concurrent is specified. */
static void sighandler(int signum)
{
	exit(-1);
}

/* We use exec_style instead of #ifdef's because ebtables.so is a shared object. */
int do_command(int argc, char *argv[], int exec_style,
               struct ebt_u_replace *replace_)
{
	char *buffer;
	int c, i;
	int zerochain = -1; /* Needed for the -Z option (we can have -Z <this> -L <that>) */
	int chcounter = 0; /* Needed for -C */
	int policy = 0;
	int rule_nr = 0;
	int rule_nr_end = 0;
	struct ebt_u_target *t;
	struct ebt_u_match *m;
	struct ebt_u_watcher *w;
	struct ebt_u_match_list *m_l;
	struct ebt_u_watcher_list *w_l;
	struct ebt_u_entries *entries;

	opterr = 0;
	ebt_modprobe = NULL;

	replace = replace_;

	/* The daemon doesn't use the environment variable */
	if (exec_style == EXEC_STYLE_PRG) {
		buffer = getenv(ATOMIC_ENV_VARIABLE);
		if (buffer) {
			replace->filename = malloc(strlen(buffer) + 1);
			if (!replace->filename)
				ebt_print_memory();
			strcpy(replace->filename, buffer);
			buffer = NULL;
		}
	}

	replace->flags &= OPT_KERNELDATA; /* ebtablesd needs OPT_KERNELDATA */
	replace->selected_chain = -1;
	replace->command = 'h';

	if (!new_entry) {
		new_entry = (struct ebt_u_entry *)malloc(sizeof(struct ebt_u_entry));
		if (!new_entry)
			ebt_print_memory();
	}
	/* Put some sane values in our new entry */
	ebt_initialize_entry(new_entry);
	new_entry->replace = replace;

	/* The scenario induced by this loop makes that:
	 * '-t'  ,'-M' and --atomic (if specified) have to come
	 * before '-A' and the like */

	/* Getopt saves the day */
	while ((c = getopt_long(argc, argv,
	   "-A:D:C:I:N:E:X::L::Z::F::P:Vhi:o:j:c:p:s:d:t:M:", ebt_options, NULL)) != -1) {
		switch (c) {

		case 'A': /* Add a rule */
		case 'D': /* Delete a rule */
		case 'C': /* Change counters */
		case 'P': /* Define policy */
		case 'I': /* Insert a rule */
		case 'N': /* Make a user defined chain */
		case 'E': /* Rename chain */
		case 'X': /* Delete chain */
			/* We allow -N chainname -P policy */
			if (replace->command == 'N' && c == 'P') {
				replace->command = c;
				optind--; /* No table specified */
				goto handle_P;
			}
			if (OPT_COMMANDS)
				ebt_print_error2("Multiple commands are not allowed");

			replace->command = c;
			replace->flags |= OPT_COMMAND;
			if (!(replace->flags & OPT_KERNELDATA))
				ebt_get_kernel_table(replace, 0);
			if (optarg && (optarg[0] == '-' || !strcmp(optarg, "!")))
				ebt_print_error2("No chain name specified");
			if (c == 'N') {
				if (ebt_get_chainnr(replace, optarg) != -1)
					ebt_print_error2("Chain %s already exists", optarg);
				else if (ebt_find_target(optarg))
					ebt_print_error2("Target with name %s exists", optarg);
				else if (strlen(optarg) >= EBT_CHAIN_MAXNAMELEN)
					ebt_print_error2("Chain name length can't exceed %d",
							EBT_CHAIN_MAXNAMELEN - 1);
				else if (strchr(optarg, ' ') != NULL)
					ebt_print_error2("Use of ' ' not allowed in chain names");
				ebt_new_chain(replace, optarg, EBT_ACCEPT);
				/* This is needed to get -N x -P y working */
				replace->selected_chain = ebt_get_chainnr(replace, optarg);
				break;
			} else if (c == 'X') {
				if (optind >= argc) {
					replace->selected_chain = -1;
					ebt_delete_chain(replace);
					break;
				}

				if (optind < argc - 1)
					ebt_print_error2("No extra options allowed with -X");

				if ((replace->selected_chain = ebt_get_chainnr(replace, argv[optind])) == -1)
					ebt_print_error2("Chain '%s' doesn't exist", argv[optind]);
				ebt_delete_chain(replace);
				if (ebt_errormsg[0] != '\0')
					return -1;
				optind++;
				break;
			}

			if ((replace->selected_chain = ebt_get_chainnr(replace, optarg)) == -1)
				ebt_print_error2("Chain '%s' doesn't exist", optarg);
			if (c == 'E') {
				if (optind >= argc)
					ebt_print_error2("No new chain name specified");
				else if (optind < argc - 1)
					ebt_print_error2("No extra options allowed with -E");
				else if (strlen(argv[optind]) >= EBT_CHAIN_MAXNAMELEN)
					ebt_print_error2("Chain name length can't exceed %d characters", EBT_CHAIN_MAXNAMELEN - 1);
				else if (ebt_get_chainnr(replace, argv[optind]) != -1)
					ebt_print_error2("Chain '%s' already exists", argv[optind]);
				else if (ebt_find_target(argv[optind]))
					ebt_print_error2("Target with name '%s' exists", argv[optind]);
				else if (strchr(argv[optind], ' ') != NULL)
					ebt_print_error2("Use of ' ' not allowed in chain names");
				ebt_rename_chain(replace, argv[optind]);
				optind++;
				break;
			} else if (c == 'D' && optind < argc && (argv[optind][0] != '-' || (argv[optind][1] >= '0' && argv[optind][1] <= '9'))) {
				if (optind != argc - 1)
					ebt_print_error2("No extra options allowed with -D start_nr[:end_nr]");
				if (parse_rule_range(argv[optind], &rule_nr, &rule_nr_end))
					ebt_print_error2("Problem with the specified rule number(s) '%s'", argv[optind]);
				optind++;
			} else if (c == 'C') {
				if ((chcounter = parse_change_counters_rule(argc, argv, &rule_nr, &rule_nr_end, exec_style)) == -1)
					return -1;
			} else if (c == 'I') {
				if (optind >= argc || (argv[optind][0] == '-' && (argv[optind][1] < '0' || argv[optind][1] > '9')))
					rule_nr = 1;
				else {
					rule_nr = strtol(argv[optind], &buffer, 10);
					if (*buffer != '\0')
						ebt_print_error2("Problem with the specified rule number '%s'", argv[optind]);
					optind++;
				}
			} else if (c == 'P') {
handle_P:
				if (optind >= argc)
					ebt_print_error2("No policy specified");
				for (i = 0; i < NUM_STANDARD_TARGETS; i++)
					if (!strcmp(argv[optind], ebt_standard_targets[i])) {
						policy = -i -1;
						if (policy == EBT_CONTINUE)
							ebt_print_error2("Wrong policy '%s'", argv[optind]);
						break;
					}
				if (i == NUM_STANDARD_TARGETS)
					ebt_print_error2("Unknown policy '%s'", argv[optind]);
				optind++;
			}
			break;
		case 'L': /* List */
		case 'F': /* Flush */
		case 'Z': /* Zero counters */
			if (c == 'Z') {
				if ((replace->flags & OPT_ZERO) || (replace->flags & OPT_COMMAND && replace->command != 'L'))
print_zero:
					ebt_print_error2("Command -Z only allowed together with command -L");
				replace->flags |= OPT_ZERO;
			} else {
				if (replace->flags & OPT_COMMAND)
					ebt_print_error2("Multiple commands are not allowed");
				replace->command = c;
				replace->flags |= OPT_COMMAND;
				if (replace->flags & OPT_ZERO && c != 'L')
					goto print_zero;
			}

#ifdef SILENT_DAEMON
			if (c== 'L' && exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("-L not supported in daemon mode");
#endif

			if (!(replace->flags & OPT_KERNELDATA))
				ebt_get_kernel_table(replace, 0);
			i = -1;
			if (optind < argc && argv[optind][0] != '-') {
				if ((i = ebt_get_chainnr(replace, argv[optind])) == -1)
					ebt_print_error2("Chain '%s' doesn't exist", argv[optind]);
				optind++;
			}
			if (i != -1) {
				if (c == 'Z')
					zerochain = i;
				else
					replace->selected_chain = i;
			}
			break;
		case 'V': /* Version */
			if (OPT_COMMANDS)
				ebt_print_error2("Multiple commands are not allowed");
			replace->command = 'V';
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2(PROGNAME" v"PROGVERSION" ("PROGDATE")\n");
			PRINT_VERSION;
			exit(0);
		case 'M': /* Modprobe */
			if (OPT_COMMANDS)
				ebt_print_error2("Please put the -M option earlier");
			free(ebt_modprobe);
			ebt_modprobe = optarg;
			break;
		case 'h': /* Help */
#ifdef SILENT_DAEMON
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("-h not supported in daemon mode");
#endif
			if (OPT_COMMANDS)
				ebt_print_error2("Multiple commands are not allowed");
			replace->command = 'h';

			/* All other arguments should be extension names */
			while (optind < argc) {
				struct ebt_u_match *m;
				struct ebt_u_watcher *w;

				if (!strcasecmp("list_extensions", argv[optind])) {
					ebt_list_extensions();
					exit(0);
				}
				if ((m = ebt_find_match(argv[optind])))
					ebt_add_match(new_entry, m);
				else if ((w = ebt_find_watcher(argv[optind])))
					ebt_add_watcher(new_entry, w);
				else {
					if (!(t = ebt_find_target(argv[optind])))
						ebt_print_error2("Extension '%s' not found", argv[optind]);
					if (replace->flags & OPT_JUMP)
						ebt_print_error2("Sorry, you can only see help for one target extension at a time");
					replace->flags |= OPT_JUMP;
					new_entry->t = (struct ebt_entry_target *)t;
				}
				optind++;
			}
			break;
		case 't': /* Table */
			if (OPT_COMMANDS)
				ebt_print_error2("Please put the -t option first");
			ebt_check_option2(&(replace->flags), OPT_TABLE);
			if (strlen(optarg) > EBT_TABLE_MAXNAMELEN - 1)
				ebt_print_error2("Table name length cannot exceed %d characters", EBT_TABLE_MAXNAMELEN - 1);
			strcpy(replace->name, optarg);
			break;
		case 'i': /* Input interface */
		case 2  : /* Logical input interface */
		case 'o': /* Output interface */
		case 3  : /* Logical output interface */
		case 'j': /* Target */
		case 'p': /* Net family protocol */
		case 's': /* Source mac */
		case 'd': /* Destination mac */
		case 'c': /* Set counters */
			if (!OPT_COMMANDS)
				ebt_print_error2("No command specified");
			if (replace->command != 'A' && replace->command != 'D' && replace->command != 'I' && replace->command != 'C')
				ebt_print_error2("Command and option do not match");
			if (c == 'i') {
				ebt_check_option2(&(replace->flags), OPT_IN);
				if (replace->selected_chain > 2 && replace->selected_chain < NF_BR_BROUTING)
					ebt_print_error2("Use -i only in INPUT, FORWARD, PREROUTING and BROUTING chains");
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_IIN;

				if (strlen(optarg) >= IFNAMSIZ)
big_iface_length:
					ebt_print_error2("Interface name length cannot exceed %d characters", IFNAMSIZ - 1);
				strcpy(new_entry->in, optarg);
				if (parse_iface(new_entry->in, "-i"))
					return -1;
				break;
			} else if (c == 2) {
				ebt_check_option2(&(replace->flags), OPT_LOGICALIN);
				if (replace->selected_chain > 2 && replace->selected_chain < NF_BR_BROUTING)
					ebt_print_error2("Use --logical-in only in INPUT, FORWARD, PREROUTING and BROUTING chains");
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_ILOGICALIN;

				if (strlen(optarg) >= IFNAMSIZ)
					goto big_iface_length;
				strcpy(new_entry->logical_in, optarg);
				if (parse_iface(new_entry->logical_in, "--logical-in"))
					return -1;
				break;
			} else if (c == 'o') {
				ebt_check_option2(&(replace->flags), OPT_OUT);
				if (replace->selected_chain < 2 || replace->selected_chain == NF_BR_BROUTING)
					ebt_print_error2("Use -o only in OUTPUT, FORWARD and POSTROUTING chains");
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_IOUT;

				if (strlen(optarg) >= IFNAMSIZ)
					goto big_iface_length;
				strcpy(new_entry->out, optarg);
				if (parse_iface(new_entry->out, "-o"))
					return -1;
				break;
			} else if (c == 3) {
				ebt_check_option2(&(replace->flags), OPT_LOGICALOUT);
				if (replace->selected_chain < 2 || replace->selected_chain == NF_BR_BROUTING)
					ebt_print_error2("Use --logical-out only in OUTPUT, FORWARD and POSTROUTING chains");
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_ILOGICALOUT;

				if (strlen(optarg) >= IFNAMSIZ)
					goto big_iface_length;
				strcpy(new_entry->logical_out, optarg);
				if (parse_iface(new_entry->logical_out, "--logical-out"))
					return -1;    
				break;
			} else if (c == 'j') {
				ebt_check_option2(&(replace->flags), OPT_JUMP);
				for (i = 0; i < NUM_STANDARD_TARGETS; i++)
					if (!strcmp(optarg, ebt_standard_targets[i])) {
						t = ebt_find_target(EBT_STANDARD_TARGET);
						((struct ebt_standard_target *) t->t)->verdict = -i - 1;
						break;
					}
				if (-i - 1 == EBT_RETURN && replace->selected_chain < NF_BR_NUMHOOKS) {
					ebt_print_error2("Return target only for user defined chains");
				} else if (i != NUM_STANDARD_TARGETS)
					break;

				if ((i = ebt_get_chainnr(replace, optarg)) != -1) {
					if (i < NF_BR_NUMHOOKS)
						ebt_print_error2("Don't jump to a standard chain");
					t = ebt_find_target(EBT_STANDARD_TARGET);
					((struct ebt_standard_target *) t->t)->verdict = i - NF_BR_NUMHOOKS;
					break;
				} else {
					/* Must be an extension then */
					struct ebt_u_target *t;

					t = ebt_find_target(optarg);
					/* -j standard not allowed either */
					if (!t || t == (struct ebt_u_target *)new_entry->t)
						ebt_print_error2("Illegal target name '%s'", optarg);
					new_entry->t = (struct ebt_entry_target *)t;
					ebt_find_target(EBT_STANDARD_TARGET)->used = 0;
					t->used = 1;
				}
				break;
			} else if (c == 's') {
				ebt_check_option2(&(replace->flags), OPT_SOURCE);
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_ISOURCE;

				if (ebt_get_mac_and_mask(optarg, new_entry->sourcemac, new_entry->sourcemsk))
					ebt_print_error2("Problem with specified source mac '%s'", optarg);
				new_entry->bitmask |= EBT_SOURCEMAC;
				break;
			} else if (c == 'd') {
				ebt_check_option2(&(replace->flags), OPT_DEST);
				if (ebt_check_inverse2(optarg))
					new_entry->invflags |= EBT_IDEST;

				if (ebt_get_mac_and_mask(optarg, new_entry->destmac, new_entry->destmsk))
					ebt_print_error2("Problem with specified destination mac '%s'", optarg);
				new_entry->bitmask |= EBT_DESTMAC;
				break;
			} else if (c == 'c') {
				ebt_check_option2(&(replace->flags), OPT_COUNT);
				if (ebt_check_inverse2(optarg))
					ebt_print_error2("Unexpected '!' after -c");
				if (optind >= argc || optarg[0] == '-' || argv[optind][0] == '-')
					ebt_print_error2("Option -c needs 2 arguments");

				new_entry->cnt.pcnt = strtoull(optarg, &buffer, 10);
				if (*buffer != '\0')
					ebt_print_error2("Packet counter '%s' invalid", optarg);
				new_entry->cnt.bcnt = strtoull(argv[optind], &buffer, 10);
				if (*buffer != '\0')
					ebt_print_error2("Packet counter '%s' invalid", argv[optind]);
				optind++;
				break;
			}
			ebt_check_option2(&(replace->flags), OPT_PROTOCOL);
			if (ebt_check_inverse2(optarg))
				new_entry->invflags |= EBT_IPROTO;

			new_entry->bitmask &= ~((unsigned int)EBT_NOPROTO);
			i = strtol(optarg, &buffer, 16);
			if (*buffer == '\0' && (i < 0 || i > 0xFFFF))
				ebt_print_error2("Problem with the specified protocol");
			if (*buffer != '\0') {
				struct ethertypeent *ent;

				if (!strcasecmp(optarg, "LENGTH")) {
					new_entry->bitmask |= EBT_802_3;
					break;
				}
				ent = getethertypebyname(optarg);
				if (!ent)
					ebt_print_error2("Problem with the specified Ethernet protocol '%s', perhaps "_PATH_ETHERTYPES " is missing", optarg);
				new_entry->ethproto = ent->e_ethertype;
			} else
				new_entry->ethproto = i;

			if (new_entry->ethproto < 0x0600)
				ebt_print_error2("Sorry, protocols have values above or equal to 0x0600");
			break;
		case 4  : /* Lc */
#ifdef SILENT_DAEMON
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("--Lc is not supported in daemon mode");
#endif
			ebt_check_option2(&(replace->flags), LIST_C);
			if (replace->command != 'L')
				ebt_print_error("Use --Lc with -L");
			replace->flags |= LIST_C;
			break;
		case 5  : /* Ln */
#ifdef SILENT_DAEMON
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("--Ln is not supported in daemon mode");
#endif
			ebt_check_option2(&(replace->flags), LIST_N);
			if (replace->command != 'L')
				ebt_print_error2("Use --Ln with -L");
			if (replace->flags & LIST_X)
				ebt_print_error2("--Lx is not compatible with --Ln");
			replace->flags |= LIST_N;
			break;
		case 6  : /* Lx */
#ifdef SILENT_DAEMON
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("--Lx is not supported in daemon mode");
#endif
			ebt_check_option2(&(replace->flags), LIST_X);
			if (replace->command != 'L')
				ebt_print_error2("Use --Lx with -L");
			if (replace->flags & LIST_N)
				ebt_print_error2("--Lx is not compatible with --Ln");
			replace->flags |= LIST_X;
			break;
		case 12 : /* Lmac2 */
#ifdef SILENT_DAEMON
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error("--Lmac2 is not supported in daemon mode");
#endif
			ebt_check_option2(&(replace->flags), LIST_MAC2);
			if (replace->command != 'L')
				ebt_print_error2("Use --Lmac2 with -L");
			replace->flags |= LIST_MAC2;
			break;
		case 8 : /* atomic-commit */
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("--atomic-commit is not supported in daemon mode");
			replace->command = c;
			if (OPT_COMMANDS)
				ebt_print_error2("Multiple commands are not allowed");
			replace->flags |= OPT_COMMAND;
			if (!replace->filename)
				ebt_print_error2("No atomic file specified");
			/* Get the information from the file */
			ebt_get_table(replace, 0);
			/* We don't want the kernel giving us its counters,
			 * they would overwrite the counters extracted from
			 * the file */
			replace->num_counters = 0;
			/* Make sure the table will be written to the kernel */
			free(replace->filename);
			replace->filename = NULL;
			break;
		case 7 : /* atomic-init */
		case 10: /* atomic-save */
		case 11: /* init-table */
			if (exec_style == EXEC_STYLE_DAEMON) {
				if (c == 7) {
					ebt_print_error2("--atomic-init is not supported in daemon mode");
				} else if (c == 10)
					ebt_print_error2("--atomic-save is not supported in daemon mode");
				ebt_print_error2("--init-table is not supported in daemon mode");
			}
			replace->command = c;
			if (OPT_COMMANDS)
				ebt_print_error2("Multiple commands are not allowed");
			if (c != 11 && !replace->filename)
				ebt_print_error2("No atomic file specified");
			replace->flags |= OPT_COMMAND;
			{
				char *tmp = replace->filename;

				/* Get the kernel table */
				replace->filename = NULL;
				ebt_get_kernel_table(replace, c == 10 ? 0 : 1);
				replace->filename = tmp;
			}
			break;
		case 9 : /* atomic */
			if (exec_style == EXEC_STYLE_DAEMON)
				ebt_print_error2("--atomic is not supported in daemon mode");
			if (OPT_COMMANDS)
				ebt_print_error2("--atomic has to come before the command");
			/* A possible memory leak here, but this is not
			 * executed in daemon mode */
			replace->filename = (char *)malloc(strlen(optarg) + 1);
			strcpy(replace->filename, optarg);
			break;
		case 13 : /* concurrent */
			signal(SIGINT, sighandler);
			signal(SIGTERM, sighandler);
			use_lockfd = 1;
			break;
		case 1 :
			if (!strcmp(optarg, "!"))
				ebt_check_inverse2(optarg);
			else
				ebt_print_error2("Bad argument : '%s'", optarg);
			/* ebt_check_inverse() did optind++ */
			optind--;
			continue;
		default:
			/* Is it a target option? */
			t = (struct ebt_u_target *)new_entry->t;
			if ((t->parse(c - t->option_offset, argv, argc, new_entry, &t->flags, &t->t))) {
				if (ebt_errormsg[0] != '\0')
					return -1;
				goto check_extension;
			}

			/* Is it a match_option? */
			for (m = ebt_matches; m; m = m->next)
				if (m->parse(c - m->option_offset, argv, argc, new_entry, &m->flags, &m->m))
					break;

			if (m != NULL) {
				if (ebt_errormsg[0] != '\0')
					return -1;
				if (m->used == 0) {
					ebt_add_match(new_entry, m);
					m->used = 1;
				}
				goto check_extension;
			}

			/* Is it a watcher option? */
			for (w = ebt_watchers; w; w = w->next)
				if (w->parse(c - w->option_offset, argv, argc, new_entry, &w->flags, &w->w))
					break;

			if (w == NULL && c == '?')
				ebt_print_error2("Unknown argument: '%s'", argv[optind - 1], (char)optopt, (char)c);
			else if (w == NULL) {
				if (!strcmp(t->name, "standard"))
					ebt_print_error2("Unknown argument: don't forget the -t option");
				else
					ebt_print_error2("Target-specific option does not correspond with specified target");
			}
			if (ebt_errormsg[0] != '\0')
				return -1;
			if (w->used == 0) {
				ebt_add_watcher(new_entry, w);
				w->used = 1;
			}
check_extension:
			if (replace->command != 'A' && replace->command != 'I' &&
			    replace->command != 'D' && replace->command != 'C')
				ebt_print_error2("Extensions only for -A, -I, -D and -C");
		}
		ebt_invert = 0;
	}

	/* Just in case we didn't catch an error */
	if (ebt_errormsg[0] != '\0')
		return -1;

	if (!(table = ebt_find_table(replace->name)))
		ebt_print_error2("Bad table name");

	if (replace->command == 'h' && !(replace->flags & OPT_ZERO)) {
		print_help();
		if (exec_style == EXEC_STYLE_PRG)
			exit(0);
	}

	/* Do the final checks */
	if (replace->command == 'A' || replace->command == 'I' ||
	   replace->command == 'D' || replace->command == 'C') {
		/* This will put the hook_mask right for the chains */
		ebt_check_for_loops(replace);
		if (ebt_errormsg[0] != '\0')
			return -1;
		entries = ebt_to_chain(replace);
		m_l = new_entry->m_list;
		w_l = new_entry->w_list;
		t = (struct ebt_u_target *)new_entry->t;
		while (m_l) {
			m = (struct ebt_u_match *)(m_l->m);
			m->final_check(new_entry, m->m, replace->name,
			   entries->hook_mask, 0);
			if (ebt_errormsg[0] != '\0')
				return -1;
			m_l = m_l->next;
		}
		while (w_l) {
			w = (struct ebt_u_watcher *)(w_l->w);
			w->final_check(new_entry, w->w, replace->name,
			   entries->hook_mask, 0);
			if (ebt_errormsg[0] != '\0')
				return -1;
			w_l = w_l->next;
		}
		t->final_check(new_entry, t->t, replace->name,
		   entries->hook_mask, 0);
		if (ebt_errormsg[0] != '\0')
			return -1;
	}
	/* So, the extensions can work with the host endian.
	 * The kernel does not have to do this of course */
	new_entry->ethproto = htons(new_entry->ethproto);

	if (replace->command == 'P') {
		if (replace->selected_chain < NF_BR_NUMHOOKS && policy == EBT_RETURN)
			ebt_print_error2("Policy RETURN only allowed for user defined chains");
		ebt_change_policy(replace, policy);
		if (ebt_errormsg[0] != '\0')
			return -1;
	} else if (replace->command == 'L') {
		list_rules();
		if (!(replace->flags & OPT_ZERO) && exec_style == EXEC_STYLE_PRG)
			exit(0);
	}
	if (replace->flags & OPT_ZERO) {
		replace->selected_chain = zerochain;
		ebt_zero_counters(replace);
	} else if (replace->command == 'F') {
		ebt_flush_chains(replace);
	} else if (replace->command == 'A' || replace->command == 'I') {
		ebt_add_rule(replace, new_entry, rule_nr);
		if (ebt_errormsg[0] != '\0')
			return -1;
		/* Makes undoing the add easier (jumps to delete_the_rule) */
		if (rule_nr <= 0)
			rule_nr--;
		rule_nr_end = rule_nr;

		/* a jump to a udc requires checking for loops */
		if (!strcmp(new_entry->t->u.name, EBT_STANDARD_TARGET) &&
		((struct ebt_standard_target *)(new_entry->t))->verdict >= 0) {
			/* FIXME: this can be done faster */
			ebt_check_for_loops(replace);
			if (ebt_errormsg[0] != '\0')
				goto delete_the_rule;
		}

		/* Do the final_check(), for all entries.
		 * This is needed when adding a rule that has a chain target */
		i = -1;
		while (++i != replace->num_chains) {
			struct ebt_u_entry *e;

			entries = replace->chains[i];
			if (!entries) {
				if (i < NF_BR_NUMHOOKS)
					continue;
				else
					ebt_print_bug("whoops\n");
			}
			e = entries->entries->next;
			while (e != entries->entries) {
				/* Userspace extensions use host endian */
				e->ethproto = ntohs(e->ethproto);
				ebt_do_final_checks(replace, e, entries);
				if (ebt_errormsg[0] != '\0')
					goto delete_the_rule;
				e->ethproto = htons(e->ethproto);
				e = e->next;
			}
		}
		/* Don't reuse the added rule */
		new_entry = NULL;
	} else if (replace->command == 'D') {
delete_the_rule:
		ebt_delete_rule(replace, new_entry, rule_nr, rule_nr_end);
		if (ebt_errormsg[0] != '\0')
			return -1;
	} else if (replace->command == 'C') {
		ebt_change_counters(replace, new_entry, rule_nr, rule_nr_end, &(new_entry->cnt_surplus), chcounter);
		if (ebt_errormsg[0] != '\0')
			return -1;
	}
	/* Commands -N, -E, -X, --atomic-commit, --atomic-commit, --atomic-save,
	 * --init-table fall through */

	if (ebt_errormsg[0] != '\0')
		return -1;
	if (table->check)
		table->check(replace);

	if (exec_style == EXEC_STYLE_PRG) {/* Implies ebt_errormsg[0] == '\0' */
		ebt_deliver_table(replace);

		if (replace->nentries)
			ebt_deliver_counters(replace);
	}
	return 0;
}
