/*
 * Zebra configuration command interface routine
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 * 
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _ZEBRA_COMMAND_H
#define _ZEBRA_COMMAND_H

#include "vector.h"
#include "vty.h"
#include "lib/route_types.h"

/* Host configuration variable */
struct host
{
  /* Host name of this router. */
  char *name;

  /* Password for vty interface. */
  char *password;
  char *password_encrypt;

  /* Enable password */
  char *enable;
  char *enable_encrypt;

  /* System wide terminal lines. */
  int lines;

  /* Log filename. */
  char *logfile;

  /* config file name of this host */
  char *config;

  /* Flags for services */
  int advanced;
  int encrypt;

  /* Banner configuration. */
  const char *motd;
  char *motdfile;
};

/* There are some command levels which called from command node. */
enum node_type 
{
  AUTH_NODE,			/* Authentication mode of vty interface. */
  RESTRICTED_NODE,		/* Restricted view mode */ 
  VIEW_NODE,			/* View node. Default mode of vty interface. */
  AUTH_ENABLE_NODE,		/* Authentication mode for change enable. */
  ENABLE_NODE,			/* Enable node. */
  CONFIG_NODE,			/* Config node. Default mode of config file. */
  SERVICE_NODE, 		/* Service node. */
  DEBUG_NODE,			/* Debug node. */
  AAA_NODE,			/* AAA node. */
  KEYCHAIN_NODE,		/* Key-chain node. */
  KEYCHAIN_KEY_NODE,		/* Key-chain key node. */
  INTERFACE_NODE,		/* Interface mode node. */
  ZEBRA_NODE,			/* zebra connection node. */
  TABLE_NODE,			/* rtm_table selection node. */
  RIP_NODE,			/* RIP protocol mode node. */ 
  RIPNG_NODE,			/* RIPng protocol mode node. */
  BABEL_NODE,			/* Babel protocol mode node. */
  BGP_NODE,			/* BGP protocol mode which includes BGP4+ */
  BGP_VPNV4_NODE,		/* BGP MPLS-VPN PE exchange. */
  BGP_IPV4_NODE,		/* BGP IPv4 unicast address family.  */
  BGP_IPV4M_NODE,		/* BGP IPv4 multicast address family.  */
  BGP_IPV6_NODE,		/* BGP IPv6 address family */
  BGP_IPV6M_NODE,		/* BGP IPv6 multicast address family. */
  OSPF_NODE,			/* OSPF protocol mode */
  OSPF6_NODE,			/* OSPF protocol for IPv6 mode */
  ISIS_NODE,			/* ISIS protocol mode */
  PIM_NODE,			/* PIM protocol mode */
  MASC_NODE,			/* MASC for multicast.  */
  IRDP_NODE,			/* ICMP Router Discovery Protocol mode. */ 
  IP_NODE,			/* Static ip route node. */
  ACCESS_NODE,			/* Access list node. */
  PREFIX_NODE,			/* Prefix list node. */
  ACCESS_IPV6_NODE,		/* Access list node. */
  PREFIX_IPV6_NODE,		/* Prefix list node. */
  AS_LIST_NODE,			/* AS list node. */
  COMMUNITY_LIST_NODE,		/* Community list node. */
  RMAP_NODE,			/* Route map node. */
  SMUX_NODE,			/* SNMP configuration node. */
  DUMP_NODE,			/* Packet dump node. */
  FORWARDING_NODE,		/* IP forwarding node. */
  PROTOCOL_NODE,                /* protocol filtering node */
  VTY_NODE,			/* Vty node. */
};

/* Node which has some commands and prompt string and configuration
   function pointer . */
struct cmd_node 
{
  /* Node index. */
  enum node_type node;		

  /* Prompt character at vty interface. */
  const char *prompt;			

  /* Is this node's configuration goes to vtysh ? */
  int vtysh;
  
  /* Node's configuration write function */
  int (*func) (struct vty *);

  /* Vector of this node's command list. */
  vector cmd_vector;	
};

enum
{
  CMD_ATTR_DEPRECATED = 1,
  CMD_ATTR_HIDDEN,
};

/* Structure of command element. */
struct cmd_element 
{
  const char *string;			/* Command specification by string. */
  int (*func) (struct cmd_element *, struct vty *, int, const char *[]);
  const char *doc;			/* Documentation of this command. */
  int daemon;                   /* Daemon to which this command belong. */
  vector tokens;		/* Vector of cmd_tokens */
  u_char attr;			/* Command attributes */
};


enum cmd_token_type
{
  TOKEN_TERMINAL = 0,
  TOKEN_MULTIPLE,
  TOKEN_KEYWORD,
};

/* Command description structure. */
struct cmd_token
{
  enum cmd_token_type type;

  /* Used for type == MULTIPLE */
  vector multiple; /* vector of cmd_token, type == FINAL */

  /* Used for type == KEYWORD */
  vector keyword; /* vector of vector of cmd_tokens */

  /* Used for type == TERMINAL */
  char *cmd;                    /* Command string. */
  char *desc;                    /* Command's description. */
};

/* Return value of the commands. */
#define CMD_SUCCESS              0
#define CMD_WARNING              1
#define CMD_ERR_NO_MATCH         2
#define CMD_ERR_AMBIGUOUS        3
#define CMD_ERR_INCOMPLETE       4
#define CMD_ERR_EXEED_ARGC_MAX   5
#define CMD_ERR_NOTHING_TODO     6
#define CMD_COMPLETE_FULL_MATCH  7
#define CMD_COMPLETE_MATCH       8
#define CMD_COMPLETE_LIST_MATCH  9
#define CMD_SUCCESS_DAEMON      10

/* Argc max counts. */
#define CMD_ARGC_MAX   25

/* Turn off these macros when uisng cpp with extract.pl */
#ifndef VTYSH_EXTRACT_PL  

/* helper defines for end-user DEFUN* macros */
#define DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
  struct cmd_element cmdname = \
  { \
    .string = cmdstr, \
    .func = funcname, \
    .doc = helpstr, \
    .attr = attrs, \
    .daemon = dnum, \
  };

#define DEFUN_CMD_FUNC_DECL(funcname) \
  static int funcname (struct cmd_element *, struct vty *, int, const char *[]);

#define DEFUN_CMD_FUNC_TEXT(funcname) \
  static int funcname \
    (struct cmd_element *self __attribute__ ((unused)), \
     struct vty *vty __attribute__ ((unused)), \
     int argc __attribute__ ((unused)), \
     const char *argv[] __attribute__ ((unused)) )

/* DEFUN for vty command interafce. Little bit hacky ;-).
 *
 * DEFUN(funcname, cmdname, cmdstr, helpstr)
 *
 * funcname
 * ========
 *
 * Name of the function that will be defined.
 *
 * cmdname
 * =======
 *
 * Name of the struct that will be defined for the command.
 *
 * cmdstr
 * ======
 *
 * The cmdstr defines the command syntax. It is used by the vty subsystem
 * and vtysh to perform matching and completion in the cli. So you have to take
 * care to construct it adhering to the following grammar. The names used
 * for the production rules losely represent the names used in lib/command.c
 *
 * cmdstr = cmd_token , { " " , cmd_token } ;
 *
 * cmd_token = cmd_terminal
 *           | cmd_multiple
 *           | cmd_keyword ;
 *
 * cmd_terminal_fixed = fixed_string
 *                    | variable
 *                    | range
 *                    | ipv4
 *                    | ipv4_prefix
 *                    | ipv6
 *                    | ipv6_prefix ;
 *
 * cmd_terminal = cmd_terminal_fixed
 *              | option
 *              | vararg ;
 *
 * multiple_part = cmd_terminal_fixed ;
 * cmd_multiple = "(" , multiple_part , ( "|" | { "|" , multiple_part } ) , ")" ;
 *
 * keyword_part = fixed_string , { " " , ( cmd_terminal_fixed | cmd_multiple ) } ;
 * cmd_keyword = "{" , keyword_part , { "|" , keyword_part } , "}" ;
 *
 * lowercase = "a" | ... | "z" ;
 * uppercase = "A" | ... | "Z" ;
 * digit = "0" | ... | "9" ;
 * number = digit , { digit } ;
 *
 * fixed_string = (lowercase | digit) , { lowercase | digit | uppercase | "-" | "_" } ;
 * variable = uppercase , { uppercase | "_" } ;
 * range = "<" , number , "-" , number , ">" ;
 * ipv4 = "A.B.C.D" ;
 * ipv4_prefix = "A.B.C.D/M" ;
 * ipv6 = "X:X::X:X" ;
 * ipv6_prefix = "X:X::X:X/M" ;
 * option = "[" , variable , "]" ;
 * vararg = "." , variable ;
 *
 * To put that all in a textual description: A cmdstr is a sequence of tokens,
 * separated by spaces.
 *
 * Terminal Tokens:
 *
 * A very simple cmdstring would be something like: "show ip bgp". It consists
 * of three Terminal Tokens, each containing a fixed string. When this command
 * is called, no arguments will be passed down to the function implementing it,
 * as it only consists of fixed strings.
 *
 * Apart from fixed strings, Terminal Tokens can also contain variables:
 * An example would be "show ip bgp A.B.C.D". This command expects an IPv4
 * as argument. As this is a variable, the IP address entered by the user will
 * be passed down as an argument. Apart from two exceptions, the other options
 * for Terminal Tokens behave exactly as we just discussed and only make a
 * difference for the CLI. The two exceptions will be discussed in the next
 * paragraphs.
 *
 * A Terminal Token can contain a so called option match. This is a simple
 * string variable that the user may omit. An example would be:
 * "show interface [IFNAME]". If the user calls this without an interface as
 * argument, no arguments will be passed down to the function implementing
 * this command. Otherwise, the interface name will be provided to the function
 * as a regular argument.

 * Also, a Terminal Token can contain a so called vararg. This is used e.g. in
 * "show ip bgp regexp .LINE". The last token is a vararg match and will
 * consume all the arguments the user inputs on the command line and append
 * those to the list of arguments passed down to the function implementing this
 * command. (Therefore, it doesn't make much sense to have any tokens after a
 * vararg because the vararg will already consume all the words the user entered
 * in the CLI)
 *
 * Multiple Tokens:
 *
 * The Multiple Token type can be used if there are multiple possibilities what
 * arguments may be used for a command, but it should map to the same function
 * nonetheless. An example would be "ip route A.B.C.D/M (reject|blackhole)"
 * In that case both "reject" and "blackhole" would be acceptable as last
 * arguments. The words matched by Multiple Tokens are always added to the
 * argument list, even if they are matched by fixed strings. Such a Multiple
 * Token can contain almost any type of token that would also be acceptable
 * for a Terminal Token, the exception are optional variables and varag.
 *
 * There is one special case that is used in some places of Quagga that should be
 * pointed out here shortly. An example would be "password (8|) WORD". This
 * construct is used to have fixed strings communicated as arguments. (The "8"
 * will be passed down as an argument in this case) It does not mean that
 * the "8" is optional. Another historic and possibly surprising property of
 * this construct is that it consumes two parts of helpstr. (Help
 * strings will be explained later)
 *
 * Keyword Tokens:
 *
 * There are commands that take a lot of different and possibly optional arguments.
 * An example from ospf would be the "default-information originate" command. This
 * command takes a lot of optional arguments that may be provided in any order.
 * To accomodate such commands, the Keyword Token has been implemented.
 * Using the keyword token, the "default-information originate" command and all
 * its possible options can be represented using this single cmdstr:
 * "default-information originate \
 *  {always|metric <0-16777214>|metric-type (1|2)|route-map WORD}"
 *
 * Keywords always start with a fixed string and may be followed by arguments.
 * Except optional variables and vararg, everything is permitted here.
 *
 * For the special case of a keyword without arguments, either NULL or the
 * keyword itself will be pushed as an argument, depending on whether the
 * keyword is present.
 * For the other keywords, arguments will be only pushed for
 * variables/Multiple Tokens. If the keyword is not present, the arguments that
 * would have been pushed will be substituted by NULL.
 *
 * A few examples:
 *   "default information originate metric-type 1 metric 1000"
 * would yield the following arguments:
 *   { NULL, "1000", "1", NULL }
 *
 *   "default information originate always route-map RMAP-DEFAULT"
 * would yield the following arguments:
 *   { "always", NULL, NULL, "RMAP-DEFAULT" }
 *
 * helpstr
 * =======
 *
 * The helpstr is used to show a short explantion for the commands that
 * are available when the user presses '?' on the CLI. It is the concatenation
 * of the helpstrings for all the tokens that make up the command.
 *
 * There should be one helpstring for each token in the cmdstr except those
 * containing other tokens, like Multiple or Keyword Tokens. For those, there
 * will only be the helpstrings of the contained tokens.
 *
 * The individual helpstrings are expected to be in the same order as their
 * respective Tokens appear in the cmdstr. They should each be terminated with
 * a linefeed. The last helpstring should be terminated with a linefeed as well.
 *
 * Care should also be taken to avoid having similar tokens with different
 * helpstrings. Imagine e.g. the commands "show ip ospf" and "show ip bgp".
 * they both contain a helpstring for "show", but only one will be displayed
 * when the user enters "sh?". If those two helpstrings differ, it is not
 * defined which one will be shown and the behavior is therefore unpredictable.
 */
#define DEFUN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUN_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED) \

/* DEFUN_NOSH for commands that vtysh should ignore */
#define DEFUN_NOSH(funcname, cmdname, cmdstr, helpstr) \
  DEFUN(funcname, cmdname, cmdstr, helpstr)

/* DEFSH for vtysh. */
#define DEFSH(daemon, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(NULL, cmdname, cmdstr, helpstr, 0, daemon) \

/* DEFUN + DEFSH */
#define DEFUNSH(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon) \
  DEFUN_CMD_FUNC_TEXT(funcname)

/* DEFUN + DEFSH with attributes */
#define DEFUNSH_ATTR(daemon, funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, daemon) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUNSH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUNSH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED)

/* ALIAS macro which define existing command's alias. */
#define ALIAS(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0)

#define ALIAS_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0)

#define ALIAS_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, 0)

#define ALIAS_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, 0)

#define ALIAS_SH(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon)

#define ALIAS_SH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, daemon)

#define ALIAS_SH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, daemon)

#endif /* VTYSH_EXTRACT_PL */

/* Some macroes */
#define CMD_OPTION(S)   ((S[0]) == '[')
#define CMD_VARIABLE(S) (((S[0]) >= 'A' && (S[0]) <= 'Z') || ((S[0]) == '<'))
#define CMD_VARARG(S)   ((S[0]) == '.')
#define CMD_RANGE(S)	((S[0] == '<'))

#define CMD_IPV4(S)	   ((strcmp ((S), "A.B.C.D") == 0))
#define CMD_IPV4_PREFIX(S) ((strcmp ((S), "A.B.C.D/M") == 0))
#define CMD_IPV6(S)        ((strcmp ((S), "X:X::X:X") == 0))
#define CMD_IPV6_PREFIX(S) ((strcmp ((S), "X:X::X:X/M") == 0))

/* Common descriptions. */
#define SHOW_STR "Show running system information\n"
#define IP_STR "IP information\n"
#define IPV6_STR "IPv6 information\n"
#define NO_STR "Negate a command or set its defaults\n"
#define REDIST_STR "Redistribute information from another routing protocol\n"
#define CLEAR_STR "Reset functions\n"
#define RIP_STR "RIP information\n"
#define BGP_STR "BGP information\n"
#define OSPF_STR "OSPF information\n"
#define NEIGHBOR_STR "Specify neighbor router\n"
#define DEBUG_STR "Debugging functions (see also 'undebug')\n"
#define UNDEBUG_STR "Disable debugging functions (see also 'debug')\n"
#define ROUTER_STR "Enable a routing process\n"
#define AS_STR "AS number\n"
#define MBGP_STR "MBGP information\n"
#define MATCH_STR "Match values from routing table\n"
#define SET_STR "Set values in destination routing protocol\n"
#define OUT_STR "Filter outgoing routing updates\n"
#define IN_STR  "Filter incoming routing updates\n"
#define V4NOTATION_STR "specify by IPv4 address notation(e.g. 0.0.0.0)\n"
#define OSPF6_NUMBER_STR "Specify by number\n"
#define INTERFACE_STR "Interface infomation\n"
#define IFNAME_STR "Interface name(e.g. ep0)\n"
#define IP6_STR "IPv6 Information\n"
#define OSPF6_STR "Open Shortest Path First (OSPF) for IPv6\n"
#define OSPF6_ROUTER_STR "Enable a routing process\n"
#define OSPF6_INSTANCE_STR "<1-65535> Instance ID\n"
#define SECONDS_STR "<1-65535> Seconds\n"
#define ROUTE_STR "Routing Table\n"
#define PREFIX_LIST_STR "Build a prefix list\n"
#define OSPF6_DUMP_TYPE_LIST \
"(neighbor|interface|area|lsa|zebra|config|dbex|spf|route|lsdb|redistribute|hook|asbr|prefix|abr)"
#define ISIS_STR "IS-IS information\n"
#define AREA_TAG_STR "[area tag]\n"

#define CONF_BACKUP_EXT ".sav"

/* IPv4 only machine should not accept IPv6 address for peer's IP
   address.  So we replace VTY command string like below. */
#ifdef HAVE_IPV6
#define NEIGHBOR_CMD       "neighbor (A.B.C.D|X:X::X:X) "
#define NO_NEIGHBOR_CMD    "no neighbor (A.B.C.D|X:X::X:X) "
#define NEIGHBOR_ADDR_STR  "Neighbor address\nIPv6 address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor IPv6 address\nNeighbor tag\n"
#else
#define NEIGHBOR_CMD       "neighbor A.B.C.D "
#define NO_NEIGHBOR_CMD    "no neighbor A.B.C.D "
#define NEIGHBOR_ADDR_STR  "Neighbor address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor tag\n"
#endif /* HAVE_IPV6 */

/* Prototypes. */
extern void install_node (struct cmd_node *, int (*) (struct vty *));
extern void install_default (enum node_type);
extern void install_element (enum node_type, struct cmd_element *);

/* Concatenates argv[shift] through argv[argc-1] into a single NUL-terminated
   string with a space between each element (allocated using
   XMALLOC(MTYPE_TMP)).  Returns NULL if shift >= argc. */
extern char *argv_concat (const char **argv, int argc, int shift);

extern vector cmd_make_strvec (const char *);
extern void cmd_free_strvec (vector);
extern vector cmd_describe_command (vector, struct vty *, int *status);
extern char **cmd_complete_command (vector, struct vty *, int *status);
extern const char *cmd_prompt (enum node_type);
extern int config_from_file (struct vty *, FILE *, unsigned int *line_num);
extern enum node_type node_parent (enum node_type);
extern int cmd_execute_command (vector, struct vty *, struct cmd_element **, int);
extern int cmd_execute_command_strict (vector, struct vty *, struct cmd_element **);
extern void cmd_init (int);
extern void cmd_terminate (void);

/* Export typical functions. */
extern struct cmd_element config_end_cmd;
extern struct cmd_element config_exit_cmd;
extern struct cmd_element config_quit_cmd;
extern struct cmd_element config_help_cmd;
extern struct cmd_element config_list_cmd;
extern char *host_config_file (void);
extern void host_config_set (char *);

extern void print_version (const char *);

/* struct host global, ick */
extern struct host host; 

/* "<cr>" global */
extern char *command_cr;
#endif /* _ZEBRA_COMMAND_H */
