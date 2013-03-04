/* Copyright 2000-2002 Joakim Axelsson (gozem@linux.nu)
 *                     Patrick Schaaf (bof@bof.de)
 * Copyright 2003-2004 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 */

#include <stdio.h>			/* *printf, perror, sscanf, fdopen */
#include <string.h>			/* mem*, str* */
#include <errno.h>			/* errno, perror */
#include <time.h>			/* time, ctime */
#include <netdb.h>			/* gethostby*, getnetby*, getservby* */
#include <stdlib.h>			/* exit, malloc, free, strtol, getenv, mkstemp */
#include <unistd.h>			/* read, close, fork, exec*, unlink */
#include <sys/types.h>			/* open, wait, socket, *sockopt, umask */
#include <sys/stat.h>			/* open, umask */
#include <sys/wait.h>			/* wait */
#include <sys/socket.h>			/* socket, *sockopt, gethostby*, inet_* */
#include <netinet/in.h>			/* inet_* */
#include <fcntl.h>			/* open */
#include <arpa/inet.h>			/* htonl, inet_* */
#include <stdarg.h>			/* va_* */
#include <dlfcn.h>			/* dlopen */

#include "ipset.h"

#ifndef PROC_SYS_MODPROBE
#define PROC_SYS_MODPROBE "/proc/sys/kernel/modprobe"
#endif

char program_name[] = "ipset";
char program_version[] = IPSET_VERSION;
static int protocol_version = 0;

#define STREQ(a,b)	(strncmp(a,b,IP_SET_MAXNAMELEN) == 0)
#define DONT_ALIGN	(protocol_version == IP_SET_PROTOCOL_UNALIGNED)
#define ALIGNED(len)	IPSET_VALIGN(len, DONT_ALIGN)

/* The list of loaded set types */
static struct settype *all_settypes = NULL;

/* Array of sets */
struct set **set_list = NULL;
ip_set_id_t max_sets = 0;

/* Suppress output to stdout and stderr? */
static int option_quiet = 0;

/* Data for restore mode */
static int restore = 0;
void *restore_data = NULL;
struct ip_set_restore *restore_set = NULL;
size_t restore_offset = 0;
socklen_t restore_size;
unsigned restore_line = 0;
unsigned warn_once = 0;

#define TEMPFILE_PATTERN	"/ipsetXXXXXX"

#ifdef IPSET_DEBUG
int option_debug = 0;
#endif

#define OPTION_OFFSET 256
static unsigned int global_option_offset = 0;

/* Most of these command parsing functions are borrowed from iptables.c */

static const char cmdflags[] = { ' ',			/* CMD_NONE */ 
	'N', 'X', 'F', 'E', 'W', 'L', 'S', 'R', 
	'A', 'D', 'T', 'H', 'V',
};

/* Options */
#define OPT_NONE		0x0000U
#define OPT_NUMERIC		0x0001U		/* -n */
#define OPT_SORTED		0x0002U		/* -s */
#define OPT_QUIET		0x0004U		/* -q */
#define OPT_DEBUG		0x0008U		/* -z */
#define OPT_RESOLVE		0x0020U		/* -r */
#define NUMBER_OF_OPT 5
static const char optflags[] =
    { 'n', 's', 'q', 'z', 'r' };

static struct option opts_long[] = {
	/* set operations */
	{"create",  1, 0, 'N'},
	{"destroy", 2, 0, 'X'},
	{"flush",   2, 0, 'F'},
	{"rename",  1, 0, 'E'},
	{"swap",    1, 0, 'W'},
	{"list",    2, 0, 'L'},

	{"save",    2, 0, 'S'},
	{"restore", 0, 0, 'R'},

	/* ip in set operations */
	{"add",     1, 0, 'A'},
	{"del",     1, 0, 'D'},
	{"test",    1, 0, 'T'},
	
	/* free options */
	{"numeric", 0, 0, 'n'},
	{"sorted",  0, 0, 's'},
	{"quiet",   0, 0, 'q'},
	{"resolve", 0, 0, 'r'},

#ifdef IPSET_DEBUG
	/* debug (if compiled with it) */
	{"debug",   0, 0, 'z'},
#endif

	/* version and help */
	{"version", 0, 0, 'V'},
	{"help",    2, 0, 'H'},

	/* end */
	{NULL},
};

static char opts_short[] =
    "-N:X::F::E:W:L::S::RA:D:T:nrsqzvVh::H::";

/* Table of legal combinations of commands and options. If any of the
 * given commands make an option legal, that option is legal.
 * Key:
 *  +  compulsory
 *  x  illegal
 *     optional
 */

static char commands_v_options[NUMBER_OF_CMD][NUMBER_OF_OPT] = {
	/*            -n   -s   -q   -z   -r */
	 /*CREATE*/  {'x', 'x', ' ', ' ', 'x'},
	 /*DESTROY*/ {'x', 'x', ' ', ' ', 'x'},
	 /*FLUSH*/   {'x', 'x', ' ', ' ', 'x'},
	 /*RENAME*/  {'x', 'x', ' ', ' ', 'x'},
	 /*SWAP*/    {'x', 'x', ' ', ' ', 'x'},
	 /*LIST*/    {' ', ' ', 'x', ' ', ' '},
	 /*SAVE*/    {'x', 'x', ' ', ' ', 'x'},
	 /*RESTORE*/ {'x', 'x', ' ', ' ', 'x'},
	 /*ADD*/     {'x', 'x', ' ', ' ', 'x'},
	 /*DEL*/     {'x', 'x', ' ', ' ', 'x'},
	 /*TEST*/    {'x', 'x', ' ', ' ', 'x'},
	 /*HELP*/    {'x', 'x', 'x', ' ', 'x'},
	 /*VERSION*/ {'x', 'x', 'x', ' ', 'x'},
};

/* Main parser function */
int parse_commandline(int argc, char *argv[]);

static void exit_tryhelp(int status)
{
	fprintf(stderr,
		"Try `%s -H' or '%s --help' for more information.\n",
		program_name, program_name);
	exit(status);
}

void exit_error(int status, const char *msg, ...)
{
	if (!option_quiet) {
		va_list args;

		va_start(args, msg);
		fprintf(stderr, "%s v%s: ", program_name, program_version);
		vfprintf(stderr, msg, args);
		va_end(args);
		fprintf(stderr, "\n");
		if (restore_line)
			fprintf(stderr, "Restore failed at line %u:\n", restore_line);
		if (status == PARAMETER_PROBLEM)
			exit_tryhelp(status);
		if (status == VERSION_PROBLEM)
			fprintf(stderr,
				"Perhaps %s or your kernel needs to be upgraded.\n",
				program_name);
	}

	exit(status);
}

static void ipset_printf(const char *msg, ...)
{
	if (!option_quiet) {
		va_list args;

		va_start(args, msg);
		vfprintf(stdout, msg, args);
		va_end(args);
		fprintf(stdout, "\n");
	}
}

static void generic_opt_check(int command, unsigned int options)
{
	int i, j, legal = 0;

	/* Check that commands are valid with options.  Complicated by the
	 * fact that if an option is legal with *any* command given, it is
	 * legal overall (ie. -z and -l).
	 */
	for (i = 0; i < NUMBER_OF_OPT; i++) {
		legal = 0;	/* -1 => illegal, 1 => legal, 0 => undecided. */

		for (j = 1; j <= NUMBER_OF_CMD; j++) {
			if (command != j)
				continue;

			if (!(options & (1 << i))) {
				if (commands_v_options[j-1][i] == '+')
					exit_error(PARAMETER_PROBLEM,
						   "You need to supply the `-%c' "
						   "option for this command\n",
						   optflags[i]);
			} else {
				if (commands_v_options[j-1][i] != 'x')
					legal = 1;
				else if (legal == 0)
					legal = -1;
			}
		}
		if (legal == -1)
			exit_error(PARAMETER_PROBLEM,
				   "Illegal option `-%c' with this command\n",
				   optflags[i]);
	}
}

static char opt2char(unsigned int option)
{
	const char *ptr;
	for (ptr = optflags; option > 1; option >>= 1, ptr++);

	return *ptr;
}

static char cmd2char(int cmd)
{
	if (cmd <= CMD_NONE || cmd > NUMBER_OF_CMD)
		return ' '; 

	return cmdflags[cmd];
}

/* From iptables.c ... */
static char *get_modprobe(void)
{
	int procfile;
	char *ret;

#define PROCFILE_BUFSIZ	1024
	procfile = open(PROC_SYS_MODPROBE, O_RDONLY);
	if (procfile < 0)
		return NULL;

	ret = (char *) malloc(PROCFILE_BUFSIZ);
	if (ret) {
		memset(ret, 0, PROCFILE_BUFSIZ);
		switch (read(procfile, ret, PROCFILE_BUFSIZ)) {
		case -1: goto fail;
		case PROCFILE_BUFSIZ: goto fail; /* Partial read.  Wierd */
		default: ; /* nothing */
		}
		if (ret[strlen(ret)-1]=='\n') 
			ret[strlen(ret)-1]=0;
		close(procfile);
		return ret;
	}
 fail:
	free(ret);
	close(procfile);
	return NULL;
}

static int ipset_insmod(const char *modname, const char *modprobe)
{
	char *buf = NULL;
	char *argv[3];
	struct stat junk;
	int status;
	
	if (!stat(modprobe, &junk)) {
		/* Try to read out of the kernel */
		buf = get_modprobe();
		if (!buf)
			return -1;
		modprobe = buf;
	}
	
	switch (fork()) {
	case 0:
		argv[0] = (char *) modprobe;
		argv[1] = (char *) modname;
		argv[2] = NULL;
		execv(argv[0], argv);
		
		/* Should not reach */
		exit(1);
	case -1:
		return -1;
	
	default: /* parent */
		wait(&status);
	}
	
	free(buf);
	
	if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return 0;
	return -1;
}

static int kernel_getsocket(void)
{
	int sockfd = -1;

	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (sockfd < 0)
		exit_error(OTHER_PROBLEM,
			   "You need to be root to perform this command.");

	return sockfd;
}

static void kernel_error(unsigned cmd, int err)
{
	unsigned int i;
	struct translate_error {
		int err;
		unsigned cmd;
		const char *message;
	} table[] =
	{ /* Generic error codes */
	  { EPERM, 0, "Missing capability" },
	  { EBADF, 0, "Invalid socket option" },
	  { EINVAL, 0, "Size mismatch for expected socket data" },
	  { ENOMEM, 0, "Not enough memory" },
	  { EFAULT, 0, "Failed to copy data" },
	  { EPROTO, 0, "ipset kernel/userspace version mismatch" },
	  { EBADMSG, 0, "Unknown command" },
	  /* Per command error codes */
	  /* Reserved ones for add/del/test to handle internally: 
	   * 	EEXIST
	   */
	  { ENOENT, CMD_CREATE, "Unknown set type" },
	  { ENOENT, 0, "Unknown set" },
	  { EAGAIN, 0, "Sets are busy, try again later" },
	  { ERANGE, CMD_CREATE, "No free slot remained to add a new set" },
	  { ERANGE, 0, "IP/port/element is outside of the set or set is full" },
	  { ENOEXEC, CMD_CREATE, "Invalid parameters to create a set" },
	  { ENOEXEC, CMD_SWAP, "Sets with different types cannot be swapped" },
	  { EEXIST, CMD_CREATE, "Set already exists" },
	  { EEXIST, CMD_RENAME, "Set with new name already exists" },
	  { EEXIST, 0, "Set specified as element does not exist" },
	  { EBUSY, 0, "Set is in use, operation not permitted" },
	  };
	for (i = 0; i < sizeof(table)/sizeof(struct translate_error); i++) {
		if ((table[i].cmd == cmd || table[i].cmd == 0)
		    && table[i].err == err)
		    	exit_error(err == EPROTO ? VERSION_PROBLEM
		    				 : OTHER_PROBLEM, 
				   table[i].message);
	}
	exit_error(OTHER_PROBLEM, "Error from kernel: %s", strerror(err));
}

static inline int wrapped_getsockopt(void *data, socklen_t *size)
{
	int res;
	int sockfd = kernel_getsocket();

	/* Send! */
	res = getsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
	if (res != 0 
	    && errno == ENOPROTOOPT 
	    && ipset_insmod("ip_set", "/sbin/modprobe") == 0)
		res = getsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
	DP("res=%d errno=%d", res, errno);
	
	return res;
}

static inline int wrapped_setsockopt(void *data, socklen_t size)
{
	int res;
	int sockfd = kernel_getsocket();

	/* Send! */
	res = setsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
	if (res != 0 
	    && errno == ENOPROTOOPT 
	    && ipset_insmod("ip_set", "/sbin/modprobe") == 0)
		res = setsockopt(sockfd, SOL_IP, SO_IP_SET, data, size);
	DP("res=%d errno=%d", res, errno);
	
	return res;
}

static void kernel_getfrom(unsigned cmd, void *data, socklen_t * size)
{
	int res = wrapped_getsockopt(data, size);

	if (res != 0)
		kernel_error(cmd, errno);
}

static int kernel_sendto_handleerrno(unsigned cmd,
				     void *data, socklen_t size)
{
	int res = wrapped_setsockopt(data, size);

	if (res != 0) {
		if (errno == EEXIST)
			return -1;
		else
			kernel_error(cmd, errno);
	}

	return 0; /* all ok */
}

static void kernel_sendto(unsigned cmd, void *data, size_t size)
{
	int res = wrapped_setsockopt(data, size);

	if (res != 0)
		kernel_error(cmd, errno);
}

static int kernel_getfrom_handleerrno(unsigned cmd, void *data, socklen_t *size)
{
	int res = wrapped_getsockopt(data, size);

	if (res != 0) {
		if (errno == EAGAIN)
			return -1;
		else
			kernel_error(cmd, errno);
	}

	return 0; /* all ok */
}

static void check_protocolversion(void)
{
	struct ip_set_req_version req_version;
	socklen_t size = sizeof(struct ip_set_req_version);
	int res;

	if (protocol_version)
		return;
	
	req_version.op = IP_SET_OP_VERSION;
	res = wrapped_getsockopt(&req_version, &size);

	if (res != 0)
		exit_error(OTHER_PROBLEM,
			   "Couldn't verify kernel module version!");

	if (!(req_version.version == IP_SET_PROTOCOL_VERSION
	      || req_version.version == IP_SET_PROTOCOL_UNALIGNED))
		exit_error(OTHER_PROBLEM,
			   "Kernel ip_set module is of protocol version %u."
			   "I'm of protocol version %u.\n"
			   "Please upgrade your kernel and/or ipset(8) utillity.",
			   req_version.version, IP_SET_PROTOCOL_VERSION);
	protocol_version = req_version.version;
}

static void set_command(int *cmd, int newcmd)
{
	if (*cmd != CMD_NONE)
		exit_error(PARAMETER_PROBLEM, "Can't use -%c with -%c\n",
			   cmd2char(*cmd), cmd2char(newcmd));
	*cmd = newcmd;
}

static void add_option(unsigned int *options, unsigned int option)
{
	if (*options & option)
		exit_error(PARAMETER_PROBLEM,
			   "multiple -%c flags not allowed",
			   opt2char(option));
	*options |= option;
}

void *ipset_malloc(size_t size)
{
	void *p;

	if (size == 0)
		return NULL;

	if ((p = malloc(size)) == NULL) {
		perror("ipset: not enough memory");
		exit(1);
	}
	return p;
}

char *ipset_strdup(const char *s)
{
	char *p;

	if ((p = strdup(s)) == NULL) {
		perror("ipset: not enough memory");
		exit(1);
	}
	return p;
}

void ipset_free(void *data)
{
	if (data == NULL)
		return;

	free(data);
}

static struct option *merge_options(struct option *oldopts,
				    const struct option *newopts,
				    int *option_offset)
{
	unsigned int num_old, num_new, i;
	struct option *merge;

	for (num_old = 0; oldopts[num_old].name; num_old++);
	for (num_new = 0; newopts[num_new].name; num_new++);

	global_option_offset += OPTION_OFFSET;
	*option_offset = global_option_offset;

	merge = ipset_malloc(sizeof(struct option) * (num_new + num_old + 1));
	memcpy(merge, oldopts, num_old * sizeof(struct option));
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *option_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof(struct option));

	return merge;
}

static char *ip_tohost(const struct in_addr *addr)
{
	struct hostent *host;

	if ((host = gethostbyaddr((char *) addr,
				  sizeof(struct in_addr),
				  AF_INET)) != NULL) {
		DP("%s", host->h_name);
		return (char *) host->h_name;
	}

	return (char *) NULL;
}

static char *ip_tonetwork(const struct in_addr *addr)
{
	struct netent *net;

	if ((net = getnetbyaddr(ntohl(addr->s_addr), 
				AF_INET)) != NULL) {
		DP("%s", net->n_name);
		return (char *) net->n_name;
	}

	return (char *) NULL;
}

/* Return a string representation of an IP address.
 * Please notice that a pointer to static char* area is returned.
 */
char *ip_tostring(ip_set_ip_t ip, unsigned options)
{
	struct in_addr addr;
	addr.s_addr = htonl(ip);

	if (!(options & OPT_NUMERIC)) {
		char *name;
		if ((name = ip_tohost(&addr)) != NULL ||
		    (name = ip_tonetwork(&addr)) != NULL)
			return name;
	}
	
	return inet_ntoa(addr);
}

char *ip_tostring_numeric(ip_set_ip_t ip)
{
	return ip_tostring(ip, OPT_NUMERIC);
}

/* Fills the 'ip' with the parsed ip or host in host byte order */
void parse_ip(const char *str, ip_set_ip_t * ip)
{
	struct hostent *host;
	struct in_addr addr;

	DP("%s", str);
	
	if (inet_aton(str, &addr) != 0) {
		*ip = ntohl(addr.s_addr);	/* We want host byte order */
		return;
	}

	host = gethostbyname(str);
	if (host != NULL) {
		if (host->h_addrtype != AF_INET ||
		    host->h_length != sizeof(struct in_addr))
			exit_error(PARAMETER_PROBLEM,
				   "host/network `%s' not an internet name",
				   str);
		if (host->h_addr_list[1] != 0)
			exit_error(PARAMETER_PROBLEM,
				   "host/network `%s' resolves to serveral ip-addresses. "
				   "Please specify one.", str);

		memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
		*ip = ntohl(addr.s_addr);
		return;
	}

	exit_error(PARAMETER_PROBLEM, "host/network `%s' not found", str);
}

/* Fills 'mask' with the parsed mask in host byte order */
void parse_mask(const char *str, ip_set_ip_t * mask)
{
	struct in_addr addr;
	int bits;

	DP("%s", str);

	if (str == NULL) {
		/* no mask at all defaults to 32 bits */
		*mask = 0xFFFFFFFF;
		return;
	}
	if (strchr(str, '.') && inet_aton(str, &addr) != 0) {
		*mask = ntohl(addr.s_addr);	/* We want host byte order */
		return;
	}
	if (sscanf(str, "%d", &bits) != 1 || bits < 0 || bits > 32)
		exit_error(PARAMETER_PROBLEM,
			   "invalid mask `%s' specified", str);

	DP("bits: %d", bits);

	*mask = bits != 0 ? 0xFFFFFFFF << (32 - bits) : 0L;
}

/* Combines parse_ip and parse_mask */
void
parse_ipandmask(const char *str, ip_set_ip_t * ip, ip_set_ip_t * mask)
{
	char buf[256];
	char *p;

	strncpy(buf, str, sizeof(buf) - 1);
	buf[255] = '\0';

	if ((p = strrchr(buf, '/')) != NULL) {
		*p = '\0';
		parse_mask(p + 1, mask);
	} else
		parse_mask(NULL, mask);

	/* if a null mask is given, the name is ignored, like in "any/0" */
	if (*mask == 0U)
		*ip = 0U;
	else
		parse_ip(buf, ip);

	DP("%s ip: %08X (%s) mask: %08X",
	   str, *ip, ip_tostring_numeric(*ip), *mask);

	/* Apply the netmask */
	*ip &= *mask;

	DP("%s ip: %08X (%s) mask: %08X",
	   str, *ip, ip_tostring_numeric(*ip), *mask);
}

/* Return a string representation of a port
 * Please notice that a pointer to static char* area is returned
 * and we assume TCP protocol.
 */
char *port_tostring(ip_set_ip_t port, unsigned options)
{
	struct servent *service;
	static char name[] = "65535";
	
	if (!(options & OPT_NUMERIC)) {
		if ((service = getservbyport(htons(port), "tcp")))
			return service->s_name;
	}
	sprintf(name, "%u", port);
	return name;
}

int
string_to_number(const char *str, unsigned int min, unsigned int max,
		 ip_set_ip_t *port)
{
	unsigned long number;
	char *end;

	/* Handle hex, octal, etc. */
	errno = 0;
	number = strtoul(str, &end, 0);
	if (*end == '\0' && end != str) {
		/* we parsed a number, let's see if we want this */
		if (errno != ERANGE && min <= number && number <= max) {
			*port = number;
			return 0;
		}
	}
	return -1;
}

static int
string_to_port(const char *str, ip_set_ip_t *port)
{
	struct servent *service;
	
	if ((service = getservbyname(str, "tcp")) != NULL) {
		*port = ntohs((uint16_t) service->s_port);
		return 0;
	}
	return -1;
}

/* Fills the 'ip' with the parsed port in host byte order */
void parse_port(const char *str, ip_set_ip_t *port)
{	
	if ((string_to_number(str, 0, 65535, port) != 0)
	      && (string_to_port(str, port) != 0))
		exit_error(PARAMETER_PROBLEM, 
		           "Invalid TCP port `%s' specified", str);	
}

/* 
 * Settype functions
 */
static struct settype *settype_find(const char *typename)
{
	struct settype *runner = all_settypes;

	DP("%s", typename);

	while (runner != NULL) {
		if (STREQ(runner->typename, typename))
			return runner;

		runner = runner->next;
	}

	return NULL;		/* not found */
}

static struct settype *settype_load(const char *typename)
{
	char path[sizeof(IPSET_LIB_DIR) + sizeof(IPSET_LIB_NAME) +
		  strlen(typename)];
	struct settype *settype;

	/* do some search in list */
	settype = settype_find(typename);
	if (settype != NULL)
		return settype;	/* found */

	/* Else we have to load it */
	sprintf(path, IPSET_LIB_DIR IPSET_LIB_NAME, typename);

	if (dlopen(path, RTLD_NOW)) {
		/* Found library. */

		settype = settype_find(typename);

		if (settype != NULL)
			return settype;
	}

	/* Can't load the settype */
	exit_error(PARAMETER_PROBLEM,
		   "Couldn't load settype `%s':%s\n",
		   typename, dlerror());

	return NULL;		/* Never executed, but keep compilers happy */
}

static char *check_set_name(char *setname)
{
	if (strlen(setname) > IP_SET_MAXNAMELEN - 1)
		exit_error(PARAMETER_PROBLEM,
			   "Setname '%s' too long, max %d characters.",
			   setname, IP_SET_MAXNAMELEN - 1);

	return setname;
}

static struct settype *check_set_typename(const char *typename)
{
	if (strlen(typename) > IP_SET_MAXNAMELEN - 1)
		exit_error(PARAMETER_PROBLEM,
			   "Typename '%s' too long, max %d characters.",
			   typename, IP_SET_MAXNAMELEN - 1);

	return settype_load(typename);
}

#define MAX(a,b)	((a) > (b) ? (a) : (b))

/* Register a new set type */
void settype_register(struct settype *settype)
{
	struct settype *chk;
	size_t size;

	DP("%s", settype->typename);

	/* Check if this typename already exists */
	chk = settype_find(settype->typename);

	if (chk != NULL)
		exit_error(OTHER_PROBLEM,
			   "Set type '%s' already registered!\n",
			   settype->typename);

	/* Check version */
	if (settype->protocol_version != IP_SET_PROTOCOL_VERSION)
		exit_error(OTHER_PROBLEM,
			   "Set type %s is of wrong protocol version %u!"
			   " I'm of version %u.\n", settype->typename,
			   settype->protocol_version,
			   IP_SET_PROTOCOL_VERSION);

	/* Initialize internal data */
	settype->header = ipset_malloc(settype->header_size);
	size = MAX(settype->create_size, settype->adt_size);
	settype->data = ipset_malloc(size);

	/* Insert first */
	settype->next = all_settypes;
	all_settypes = settype;

	DP("%s registered", settype->typename);
}

/* Find set functions */
struct set *set_find_byid(ip_set_id_t id)
{
	struct set *set = NULL;
	ip_set_id_t i;
	
	for (i = 0; i < max_sets; i++)
		if (set_list[i] && set_list[i]->id == id) {
			set = set_list[i];
			break;
		}
			
	if (set == NULL)
	    	exit_error(PARAMETER_PROBLEM,
	    		   "Set identified by id %u is not found", id);
	return set;
}

struct set *set_find_byname(const char *name)
{
	struct set *set = NULL;
	ip_set_id_t i;
	
	for (i = 0; i < max_sets; i++)
		if (set_list[i] != NULL && STREQ(set_list[i]->name, name)) {
			set = set_list[i];
			break;
		}
	if (set == NULL)
	    	exit_error(PARAMETER_PROBLEM,
	    		   "Set %s is not found", name);
	return set;
}

static ip_set_id_t set_find_free_index(const char *name)
{
	ip_set_id_t i, idx = IP_SET_INVALID_ID;

	for (i = 0; i < max_sets; i++) {
		if (idx == IP_SET_INVALID_ID
		    && set_list[i] == NULL)
			idx = i;
		if (set_list[i] != NULL && STREQ(set_list[i]->name, name))
			exit_error(PARAMETER_PROBLEM,
   				   "Set %s is already defined, cannot be restored",
   				   name);
	}
			
	if (idx == IP_SET_INVALID_ID)		
		exit_error(PARAMETER_PROBLEM,
	   		   "Set %s cannot be restored, "
	   		   "max number of set %u reached",
	   		   name, max_sets);

	return idx;
}

/* 
 * Send create set order to kernel
 */
static void set_create(const char *name, struct settype *settype)
{
	struct ip_set_req_create req_create;
	size_t size;
	void *data;

	DP("%s %s", name, settype->typename);

	req_create.op = IP_SET_OP_CREATE;
	req_create.version = protocol_version;
	strcpy(req_create.name, name);
	strcpy(req_create.typename, settype->typename);

	/* Final checks */
	settype->create_final(settype->data, settype->flags);

	/* Alloc memory for the data to send */
	size = sizeof(struct ip_set_req_create) + settype->create_size;
	data = ipset_malloc(size);

	/* Add up ip_set_req_create and the settype data */
	memcpy(data, &req_create, sizeof(struct ip_set_req_create));
	memcpy(data + sizeof(struct ip_set_req_create),
	       settype->data, settype->create_size);

	kernel_sendto(CMD_CREATE, data, size);
	free(data);
}

static void set_restore_create(const char *name, struct settype *settype)
{
	struct set *set;
	
	DP("%s %s %zu %zu %u %u", name, settype->typename,
	   restore_offset, sizeof(struct ip_set_restore),
	   settype->create_size, restore_size);

	/* Sanity checking */
	if (restore_offset
	    + ALIGNED(sizeof(struct ip_set_restore))
	    + ALIGNED(settype->create_size) > restore_size)
	    	exit_error(PARAMETER_PROBLEM,
	    		   "Giving up, restore file is screwed up!");
	    		   
	/* Final checks */
	settype->create_final(settype->data, settype->flags);

	/* Fill out restore_data */
	restore_set = (struct ip_set_restore *) 
			(restore_data + restore_offset);
	strcpy(restore_set->name, name);
	strcpy(restore_set->typename, settype->typename);
	restore_set->index = set_find_free_index(name);
	restore_set->header_size = settype->create_size;
	restore_set->members_size = 0;

	DP("name %s, restore index %u", restore_set->name, restore_set->index);
	/* Add settype data */
	
	restore_offset += ALIGNED(sizeof(struct ip_set_restore));
	memcpy(restore_data + restore_offset, settype->data, settype->create_size);

	restore_offset += ALIGNED(settype->create_size);
	DP("restore_offset: %zu", restore_offset);
	
	/* Add set to set_list */
	set = ipset_malloc(sizeof(struct set));
	strcpy(set->name, name);
	set->settype = settype;
	set->index = restore_set->index;
	set_list[restore_set->index] = set;
}

/*
 * Send destroy/flush order to kernel for one or all sets
 */
static void set_destroy(const char *name, unsigned op, unsigned cmd)
{
	struct ip_set_req_std req;

	DP("%s %s", cmd == CMD_DESTROY ? "destroy" : "flush", name);

	req.op = op;
	req.version = protocol_version;
	strcpy(req.name, name);

	kernel_sendto(cmd, &req, sizeof(struct ip_set_req_std));
}

/*
 * Send rename/swap order to kernel
 */
static void set_rename(const char *name, const char *newname,
		       unsigned op, unsigned cmd)
{
	struct ip_set_req_create req;

	DP("%s %s %s", cmd == CMD_RENAME ? "rename" : "swap",
		       name, newname);

	req.op = op;
	req.version = protocol_version;
	strcpy(req.name, name);
	strcpy(req.typename, newname);

	kernel_sendto(cmd, &req,
		      sizeof(struct ip_set_req_create));
}

/*
 * Send MAX_SETS, LIST_SIZE and/or SAVE_SIZE orders to kernel
 */
static size_t load_set_list(const char name[IP_SET_MAXNAMELEN],
			    ip_set_id_t *idx,
			    unsigned op, unsigned cmd)
{
	void *data = NULL;
	struct ip_set_req_max_sets req_max_sets;
	struct ip_set_name_list *name_list;
	struct set *set;
	ip_set_id_t i;
	socklen_t size, req_size;
	int repeated = 0, res = 0;

	DP("%s %s", cmd == CMD_MAX_SETS ? "MAX_SETS"
		    : cmd == CMD_LIST_SIZE ? "LIST_SIZE"
		    : "SAVE_SIZE",
		    name);
	
tryagain:
	if (set_list) {
		for (i = 0; i < max_sets; i++)
			if (set_list[i])
				free(set_list[i]);
		free(set_list);
		set_list = NULL;
	}
	/* Get max_sets */
	req_max_sets.op = IP_SET_OP_MAX_SETS;
	req_max_sets.version = protocol_version;
	strcpy(req_max_sets.set.name, name);
	size = sizeof(req_max_sets);
	kernel_getfrom(CMD_MAX_SETS, &req_max_sets, &size);

	DP("got MAX_SETS: sets %d, max_sets %d",
	   req_max_sets.sets, req_max_sets.max_sets);

	max_sets = req_max_sets.max_sets;
	set_list = ipset_malloc(max_sets * sizeof(struct set *));
	memset(set_list, 0, max_sets * sizeof(struct set *));
	*idx = req_max_sets.set.index;

	if (req_max_sets.sets == 0)
		/* No sets in kernel */
		return 0;

	/* Get setnames */
	size = req_size = ALIGNED(sizeof(struct ip_set_req_setnames)) 
			  + req_max_sets.sets * ALIGNED(sizeof(struct ip_set_name_list));
	data = ipset_malloc(size);
	((struct ip_set_req_setnames *) data)->op = op;
	((struct ip_set_req_setnames *) data)->index = *idx;

	res = kernel_getfrom_handleerrno(cmd, data, &size);

	if (res != 0 || size != req_size) {
		free(data);
		if (repeated++ < LIST_TRIES)
			goto tryagain;
		exit_error(OTHER_PROBLEM,
			   "Tried to get sets from kernel %d times"
			   " and failed. Please try again when the load on"
			   " the sets has gone down.", LIST_TRIES);
	}
		
	/* Load in setnames */
	size = ALIGNED(sizeof(struct ip_set_req_setnames));			
	while (size + ALIGNED(sizeof(struct ip_set_name_list)) <= req_size) {
		name_list = (struct ip_set_name_list *)
			(data + size);
		set = ipset_malloc(sizeof(struct set));
		strcpy(set->name, name_list->name);
		set->index = name_list->index;
		set->id = name_list->id;
		set->settype = settype_load(name_list->typename);
		set_list[name_list->index] = set;
		DP("loaded %s, type %s, index %u",
		   set->name, set->settype->typename, set->index);
		size += ALIGNED(sizeof(struct ip_set_name_list));
	}
	/* Size to get set members  */
	size = ((struct ip_set_req_setnames *)data)->size;
	free(data);
	
	return size;
}

/*
 * Save operation
 */
static size_t save_set(void *data, size_t offset, size_t len)
{
	struct ip_set_save *set_save =
		(struct ip_set_save *) (data + offset);
	struct set *set;
	struct settype *settype;
	size_t used;
	
	DP("offset %zu (%zu/%u/%u), len %zu", offset,
	   sizeof(struct ip_set_save), 
	   set_save->header_size, set_save->members_size, 
	   len);
	if (offset + ALIGNED(sizeof(struct ip_set_save)) > len
	    || offset + ALIGNED(sizeof(struct ip_set_save))
	       + set_save->header_size + set_save->members_size > len)
		exit_error(OTHER_PROBLEM,
			   "Save operation failed, try again later.");

	DP("index: %u", set_save->index);
	if (set_save->index == IP_SET_INVALID_ID) {
		/* Marker */
		return ALIGNED(sizeof(struct ip_set_save));
	}
	set = set_list[set_save->index];
	if (!set)
		exit_error(OTHER_PROBLEM,
			   "Save set failed, try again later.");
	settype = set->settype;

	/* Init set header */
	used = ALIGNED(sizeof(struct ip_set_save));
	settype->initheader(set, data + offset + used);

	/* Print create set */
	settype->saveheader(set, OPT_NUMERIC);

	/* Print add IPs */
	used += set_save->header_size;
	settype->saveips(set, data + offset + used,
			 set_save->members_size, OPT_NUMERIC,
			 DONT_ALIGN);

	return (used + set_save->members_size);
}

static int try_save_sets(const char name[IP_SET_MAXNAMELEN])
{
	void *data = NULL;
	socklen_t size, req_size = 0;
	ip_set_id_t idx;
	int res = 0;
	time_t now = time(NULL);

	/* Load set_list from kernel */
	size = load_set_list(name, &idx,
			     IP_SET_OP_SAVE_SIZE, CMD_SAVE);
	
	if (size) {
		/* Get sets and print them */
		/* Take into account marker */
		req_size = (size += ALIGNED(sizeof(struct ip_set_save)));
		data = ipset_malloc(size);
		((struct ip_set_req_list *) data)->op = IP_SET_OP_SAVE;
		((struct ip_set_req_list *) data)->index = idx;
		res = kernel_getfrom_handleerrno(CMD_SAVE, data, &size);

		if (res != 0 || size != req_size) {
			DP("Try again: res: %i, size %u, req_size: %u",
			   res, size, req_size);
			free(data);
			return -EAGAIN;
		}
	}

	printf("# Generated by ipset %s on %s", IPSET_VERSION, ctime(&now));
	size = 0;
	while (size < req_size) {
		DP("size: %u, req_size: %u", size, req_size);
		size += save_set(data, size, req_size);
	}
	printf("COMMIT\n");
	now = time(NULL);
	printf("# Completed on %s", ctime(&now));
	ipset_free(data);
	return res;
}

/*
 * Performs a save to stdout
 */
static void set_save(const char name[IP_SET_MAXNAMELEN])
{
	int i;

	DP("%s", name);
	for (i = 0; i < LIST_TRIES; i++)
		if (try_save_sets(name) == 0)
			return;

	if (errno == EAGAIN)
		exit_error(OTHER_PROBLEM,
			   "Tried to save sets from kernel %d times"
			   " and failed. Please try again when the load on"
			   " the sets has gone down.", LIST_TRIES);
	else
		kernel_error(CMD_SAVE, errno);
}

/*
 * Restore operation
 */

/* global new argv and argc */
static char *newargv[255];
static int newargc = 0;

/* Build faked argv from parsed line */
static void build_argv(unsigned line, char *buffer) {
	char *ptr;
	int i;

	/* Reset */	
	for (i = 1; i < newargc; i++)
		free(newargv[i]);
	newargc = 1;

	ptr = strtok(buffer, " \t\n");
	newargv[newargc++] = ipset_strdup(ptr);
	while ((ptr = strtok(NULL, " \t\n")) != NULL) {
		if ((newargc + 1) < (int)(sizeof(newargv)/sizeof(char *)))
			newargv[newargc++] = ipset_strdup(ptr);
		else
			exit_error(PARAMETER_PROBLEM,
				   "Line %d is too long to restore\n", line);
	}
}

static FILE *create_tempfile(void)
{
	char buffer[1024], __tmpdir[] = "/tmp";	
	char *tmpdir = NULL;
	char *filename;
	int fd;
	FILE *file;
	
	if (!(tmpdir = getenv("TMPDIR")) && !(tmpdir = getenv("TMP")))
		tmpdir = __tmpdir;
	filename = ipset_malloc(strlen(tmpdir) + strlen(TEMPFILE_PATTERN) + 1);
	strcpy(filename, tmpdir);
	strcat(filename, TEMPFILE_PATTERN);
	
	(void) umask(077);	/* Create with restrictive permissions */
	fd = mkstemp(filename);
	if (fd == -1)
		exit_error(OTHER_PROBLEM, "Could not create temporary file.");
	if (!(file = fdopen(fd, "r+")))
		exit_error(OTHER_PROBLEM, "Could not open temporary file.");
	if (unlink(filename) == -1)
		exit_error(OTHER_PROBLEM, "Could not unlink temporary file.");
	free(filename);

	while (fgets(buffer, sizeof(buffer), stdin)) {
		fputs(buffer, file);
	}
	fseek(file, 0L, SEEK_SET);

	return file;
}

/*
 * Performs a restore from a file
 */
static void set_restore(char *argv0)
{
	char buffer[1024];	
	char *ptr, *name = NULL;
	char cmd = ' ';
	int first_pass, i;
	struct settype *settype = NULL;
	struct ip_set_req_setnames *header;
	ip_set_id_t idx;
	FILE *in;
	int res;
	
	/* Create and store stdin in temporary file */
	in = create_tempfile();
	
	/* Load existing sets from kernel */
	load_set_list(IPSET_TOKEN_ALL, &idx,
		      IP_SET_OP_LIST_SIZE, CMD_RESTORE);
	
	restore_line = 0;
	restore_size = ALIGNED(sizeof(struct ip_set_req_setnames)); /* header */
	DP("restore_size: %u", restore_size);
	/* First pass: calculate required amount of data */
	while (fgets(buffer, sizeof(buffer), in)) {
		restore_line++;

		if (buffer[0] == '\n')
			continue;
		else if (buffer[0] == '#')
			continue;
		else if (strcmp(buffer, "COMMIT\n") == 0) {
			/* Enable restore mode */
			restore = 1;
			break;
		}
			
		/* -N, -A or -B */
		ptr = strtok(buffer, " \t\n");
		DP("ptr: %s", ptr);
		if (ptr == NULL
		    || ptr[0] != '-'
		    || !(ptr[1] == 'N'
		         || ptr[1] == 'A'
			 || ptr[1] == 'B')
		    || ptr[2] != '\0') {
			exit_error(PARAMETER_PROBLEM,
				   "Line %u does not start as a valid restore command\n",
				   restore_line);
		}
		cmd = ptr[1];		
		/* setname */
		ptr = strtok(NULL, " \t\n");
		DP("setname: %s", ptr);
		if (ptr == NULL)
		        exit_error(PARAMETER_PROBLEM,
		        	   "Missing set name in line %u\n",
		        	   restore_line);
		DP("cmd %c", cmd);
		switch (cmd) {
		case 'N': {
			name = check_set_name(ptr);
			/* settype */
			ptr = strtok(NULL, " \t\n");
			if (ptr == NULL)
			        exit_error(PARAMETER_PROBLEM,
			        	   "Missing settype in line %u\n",
		        		   restore_line);
			settype = check_set_typename(ptr);
			restore_size += ALIGNED(sizeof(struct ip_set_restore))
					+ ALIGNED(settype->create_size);
			DP("restore_size (N): %u", restore_size);
			break; 
		}
		case 'A': {
			if (name == NULL
			    || strncmp(name, ptr, sizeof(name)) != 0)
			        exit_error(PARAMETER_PROBLEM,
			        	   "Add IP to set %s in line %u without "
					   "preceding corresponding create set line\n",
		        		   ptr, restore_line);
			restore_size += ALIGNED(settype->adt_size);
			DP("restore_size (A): %u", restore_size);
			break;
		}
		default: {
			exit_error(PARAMETER_PROBLEM,
		       		   "Unrecognized restore command in line %u\n",
				   restore_line);
		}
		} /* end of switch */
	}			
	/* Sanity checking */
	if (!restore)
		exit_error(PARAMETER_PROBLEM,
		      	   "Missing COMMIT line\n");
	restore_size += ALIGNED(sizeof(struct ip_set_restore)); /* marker */
	DP("restore_size: %u", restore_size);
	restore_data = ipset_malloc(restore_size);
	header = (struct ip_set_req_setnames *) restore_data;
	header->op = IP_SET_OP_RESTORE; 
	header->size = restore_size; 
	restore_offset = ALIGNED(sizeof(struct ip_set_req_setnames));

	/* Rewind to scan the file again */
	fseek(in, 0L, SEEK_SET);
	first_pass = restore_line;
	restore_line = 0;
	
	/* Initialize newargv/newargc */
	newargv[newargc++] = ipset_strdup(argv0);
	
	/* Second pass: build up restore request */
	while (fgets(buffer, sizeof(buffer), in)) {		
		restore_line++;

		if (buffer[0] == '\n')
			continue;
		else if (buffer[0] == '#')
			continue;
		else if (strcmp(buffer, "COMMIT\n") == 0)
			goto do_restore;
		DP("restoring: %s", buffer);
		/* Build faked argv, argc */
		build_argv(restore_line, buffer);
		for (i = 0; i < newargc; i++)
			DP("argv[%u]: %s", i, newargv[i]);
		
		/* Parse line */
		parse_commandline(newargc, newargv);
	}
	exit_error(PARAMETER_PROBLEM,
	      	   "Broken restore file\n");
   do_restore:
   	if (restore_size == (restore_offset + ALIGNED(sizeof(struct ip_set_restore)))) {
   		/* No bindings */
		struct ip_set_restore *marker = 
			(struct ip_set_restore *) (restore_data + restore_offset);

		marker->index = IP_SET_INVALID_ID;
		marker->header_size = marker->members_size = 0;
		restore_offset += ALIGNED(sizeof(struct ip_set_restore));
		DP("restore marker, restore_offset: %zu", restore_offset);
	}
	if (restore_size != restore_offset)
		exit_error(PARAMETER_PROBLEM,
		    	   "Giving up, restore file is screwed up!");
	res = kernel_getfrom_handleerrno(CMD_RESTORE, restore_data, &restore_size);

	if (res != 0) {
		if (restore_size != sizeof(struct ip_set_req_setnames))
			exit_error(PARAMETER_PROBLEM,
			    	   "Communication with kernel failed (%u %u)!",
			    	   restore_size, sizeof(struct ip_set_req_setnames));
		/* Check errors  */
		header = (struct ip_set_req_setnames *) restore_data;
		if (header->size != 0) 
			exit_error(PARAMETER_PROBLEM,
			    	   "Committing restoring failed at line %u!",
		    		   header->size);
	}
}

/*
 * Send ADT_GET order to kernel for a set
 */
static struct set *set_adt_get(const char *name)
{
	struct ip_set_req_adt_get req_adt_get;
	struct set *set;
	socklen_t size;

	DP("%s", name);

	check_protocolversion();

	req_adt_get.op = IP_SET_OP_ADT_GET;
	req_adt_get.version = protocol_version;
	strcpy(req_adt_get.set.name, name);
	size = sizeof(struct ip_set_req_adt_get);

	kernel_getfrom(CMD_ADT_GET, (void *) &req_adt_get, &size);

	set = ipset_malloc(sizeof(struct set));
	strcpy(set->name, name);
	set->index = req_adt_get.set.index;	
	set->settype = settype_load(req_adt_get.typename);

	return set;
}	

/*
 * Send add/del/test order to kernel for a set
 */
static int set_adtip(struct set *set, const char *adt, 
		     unsigned op, unsigned cmd)
{
	struct ip_set_req_adt *req_adt;
	size_t size;
	void *data;
	int res = 0;

	DP("%s -> %s", set->name, adt);

	/* Alloc memory for the data to send */
	size = ALIGNED(sizeof(struct ip_set_req_adt)) + set->settype->adt_size ;
	DP("alloc size %zu", size);
	data = ipset_malloc(size);

	/* Fill out the request */
	req_adt = (struct ip_set_req_adt *) data;
	req_adt->op = op;
	req_adt->index = set->index;
	memcpy(data + ALIGNED(sizeof(struct ip_set_req_adt)),
	       set->settype->data, set->settype->adt_size);
	
	if (kernel_sendto_handleerrno(cmd, data, size) == -1)
		switch (op) {
		case IP_SET_OP_ADD_IP:
			exit_error(OTHER_PROBLEM, "%s is already in set %s.",
				   adt, set->name);
			break;
		case IP_SET_OP_DEL_IP:
			exit_error(OTHER_PROBLEM, "%s is not in set %s.",
				   adt, set->name);
			break;
		case IP_SET_OP_TEST_IP:
			ipset_printf("%s is in set %s.", adt, set->name);
			res = 0;
			break;
		default:
			break;
		}
	else
		switch (op) {
		case IP_SET_OP_TEST_IP:
			ipset_printf("%s is NOT in set %s.", adt, set->name);
			res = 1;
			break;
		default:
			break;
		}
	free(data);

	return res;
}

static void set_restore_add(struct set *set, const char *adt UNUSED)
{
	DP("%s %s", set->name, adt);
	/* Sanity checking */
	if (restore_offset + ALIGNED(set->settype->adt_size) > restore_size)
	    	exit_error(PARAMETER_PROBLEM,
	    		   "Giving up, restore file is screwed up!");
	    		   
	memcpy(restore_data + restore_offset,
	       set->settype->data, set->settype->adt_size);
	restore_set->members_size += ALIGNED(set->settype->adt_size);
	restore_offset += ALIGNED(set->settype->adt_size);

	DP("restore_offset: %zu", restore_offset);
}

/*
 * Print operation
 */

/* Help function to set_list() */
static size_t print_set(void *data, unsigned options)
{
	struct ip_set_list *setlist = data;
	struct set *set = set_list[setlist->index];
	struct settype *settype = set->settype;
	size_t offset;

	/* Pretty print the set */
	DP("header size: %u, members size: %u",
	   setlist->header_size, setlist->members_size);
	printf("Name: %s\n", set->name);
	printf("Type: %s\n", settype->typename);
	printf("References: %d\n", setlist->ref);

	/* Init header */
	offset = ALIGNED(sizeof(struct ip_set_list));
	settype->initheader(set, data + offset);

	/* Pretty print the type header */
	printf("Header:");
	settype->printheader(set, options);

	/* Pretty print all IPs */
	printf("Members:\n");
	offset += setlist->header_size;
	DP("Aligned: %u, offset: %zu, members_size %u\n", !DONT_ALIGN, offset,
	   setlist->members_size);
	if (options & OPT_SORTED)
		settype->printips_sorted(set, data + offset,
					 setlist->members_size, options,
					 DONT_ALIGN);
	else
		settype->printips(set, data + offset,
				  setlist->members_size, options,
				  DONT_ALIGN);

	printf("\n");		/* One newline between sets */
	
	return (offset + setlist->members_size);
}

static int try_list_sets(const char name[IP_SET_MAXNAMELEN],
			 unsigned options)
{
	void *data = NULL;
	ip_set_id_t idx;
	socklen_t size, req_size;
	int res = 0;

	/* Default is numeric listing */
	if (!(options & (OPT_RESOLVE|OPT_NUMERIC)))
		options |= OPT_NUMERIC;

	DP("%s", name);
	/* Load set_list from kernel */
	size = req_size = load_set_list(name, &idx,
					IP_SET_OP_LIST_SIZE, CMD_LIST);

	if (size) {
		/* Get sets and print them */
		data = ipset_malloc(size);
		((struct ip_set_req_list *) data)->op = IP_SET_OP_LIST;
		((struct ip_set_req_list *) data)->index = idx;
		res = kernel_getfrom_handleerrno(CMD_LIST, data, &size);
		DP("get_lists getsockopt() res=%d errno=%d", res, errno);

		if (res != 0 || size != req_size) {
			free(data);
			return -EAGAIN;
		}
		size = 0;
	}
	while (size != req_size)
		size += print_set(data + size, options);

	ipset_free(data);
	return res;
}

/* Print a set or all sets
 * All sets: name = NULL
 */
static void list_sets(const char name[IP_SET_MAXNAMELEN], unsigned options)
{
	int i;

	DP("%s", name);
	for (i = 0; i < LIST_TRIES; i++)
		if (try_list_sets(name, options) == 0)
			return;

	if (errno == EAGAIN)
		exit_error(OTHER_PROBLEM,
			   "Tried to list sets from kernel %d times"
			   " and failed. Please try again when the load on"
			   " the sets has gone down.", LIST_TRIES);
	else
		kernel_error(CMD_LIST, errno);
}

/* Prints help
 * If settype is non null help for that type is printed as well
 */
static void set_help(const struct settype *settype)
{
	printf("%s v%s\n\n"
	       "Usage: %s -N new-set settype [options]\n"
	       "       %s -[XFLSH] [set] [options]\n"
	       "       %s -[EW] from-set to-set\n"
	       "       %s -[ADT] set IP\n"
	       "       %s -R\n"
	       "       %s -v\n"
	       "       %s -h (print this help information)\n\n",
	       program_name, program_version, 
	       program_name, program_name, program_name,
	       program_name, program_name, program_name,
	       program_name);

	printf("Commands:\n"
	       "Either long or short options are allowed.\n"
	       "  --create  -N setname settype <options>\n"
	       "                    Create a new set\n"
	       "  --destroy -X [setname]\n"
	       "                    Destroy a set or all sets\n"
	       "  --flush   -F [setname]\n"
	       "                    Flush a set or all sets\n"
	       "  --rename  -E from-set to-set\n"
	       "                    Rename from-set to to-set\n"
	       "  --swap    -W from-set to-set\n"
	       "                    Swap the content of two existing sets\n"
	       "  --list    -L [setname] [options]\n"
	       "                    List the IPs in a set or all sets\n"
	       "  --save    -S [setname]\n"
	       "                    Save the set or all sets to stdout\n"
	       "  --restore -R [option]\n"
	       "                    Restores a saved state\n"
	       "  --add     -A setname IP\n"
	       "                    Add an IP to a set\n"
	       "  --del     -D setname IP\n"
	       "                    Deletes an IP from a set\n"
	       "  --test    -T setname IP \n"
	       "                    Tests if an IP exists in a set.\n"
	       "  --help    -H [settype]\n"
	       "                    Prints this help, and settype specific help\n"
	       "  --version -V\n"
	       "                    Prints version information\n\n"
	       "Options:\n"
	       "  --sorted     -s   Numeric sort of the IPs in -L\n"
	       "  --numeric    -n   Numeric output of addresses in a -L (default)\n"
	       "  --resolve    -r   Try to resolve addresses in a -L\n"
	       "  --quiet      -q   Suppress any output to stdout and stderr.\n");
#ifdef IPSET_DEBUG
	printf("  --debug      -z   Enable debugging\n\n");
#else
	printf("\n");
#endif

	if (settype != NULL) {
		printf("Type '%s' specific:\n", settype->typename);
		settype->usage();
	}
}

static int find_cmd(int option)
{
	int i;
	
	for (i = 1; i <= NUMBER_OF_CMD; i++)
		if (cmdflags[i] == option)
			return i;
			
	return CMD_NONE;
}

static int parse_adt_cmdline(int command,
			     const char *name,
			     char *adt,
			     struct set **set,
			     struct settype **settype)
{
	int res = 0;

	*set = restore ? set_find_byname(name) : set_adt_get(name);
					
	/* Reset space for adt data */
	*settype = (*set)->settype;
	memset((*settype)->data, 0, (*settype)->adt_size);

	res = (*settype)->adt_parser(command, adt, (*settype)->data);

	return res;
}

/* Main worker function */
int parse_commandline(int argc, char *argv[])
{
	int res = 0;
	int command = CMD_NONE;
	unsigned options = 0;
	int c;
	
	char *name = NULL;		/* All except -H, -R */
	char *newname = NULL;		/* -E, -W */
	char *adt = NULL;		/* -A, -D, -T */
	struct set *set = NULL;		/* -A, -D, -T */
	struct settype *settype = NULL;	/* -N, -H */
	char all_sets[] = IPSET_TOKEN_ALL;
	
	struct option *opts = opts_long;

	/* Suppress error messages: we may add new options if we
	   demand-load a protocol. */
	opterr = 0;
	/* Reset optind to 0 for restore */
	optind = 0;
	
	while ((c = getopt_long(argc, argv, opts_short, opts, NULL)) != -1) {

		DP("commandline parsed: opt %c (%s)", c, argv[optind]);

		switch (c) {
			/*
			 * Command selection
			 */
		case 'h':
		case 'H':{	/* Help: -H [typename [options]] */
				check_protocolversion();
				set_command(&command, CMD_HELP);
				
				if (optarg)
					settype = check_set_typename(optarg);
				else if (optind < argc
					 && argv[optind][0] != '-')
					settype = check_set_typename(argv[optind++]);
				
				break;
			}

		case 'V':
		case 'v': {	/* Version */
				printf("%s v%s, protocol version %u.\n",
				       program_name, program_version,
				       IP_SET_PROTOCOL_VERSION);
				check_protocolversion();
				printf("Kernel module protocol version %u.\n",
				       protocol_version);
				exit(0);
			}

		case 'N':{	/* Create: -N name typename options */
				set_command(&command, CMD_CREATE);

				name = check_set_name(optarg);
				
				/* Protect reserved names */
				if (name[0] == ':')
					exit_error(PARAMETER_PROBLEM,
						   "setname might not start with colon",
						   cmd2char(CMD_CREATE));
				
				if (optind < argc
				    && argv[optind][0] != '-')
					settype = check_set_typename(argv[optind++]);
				else
					exit_error(PARAMETER_PROBLEM,
						   "-%c requires setname and settype",
						   cmd2char(CMD_CREATE));

				DP("merge options");
				/* Merge the create options */
				opts = merge_options(opts,
					     settype->create_opts,
					     &settype->option_offset);

				/* Reset space for create data */
				memset(settype->data, 0, settype->create_size);

				/* Zero the flags */
				settype->flags = 0;

				DP("call create_init");
				/* Call the settype create_init */
				settype->create_init(settype->data);

				break;
			}

		case 'X': 	/* Destroy */
		case 'F':	/* Flush */
		case 'L':	/* List */
		case 'S':{	/* Save */
				set_command(&command, find_cmd(c));

				if (optarg)
					name = check_set_name(optarg);
				else if (optind < argc
					   && argv[optind][0] != '-')
					name = check_set_name(argv[optind++]);
				else
					name = all_sets;

				break;
			}

		case 'R':{	/* Restore */
				set_command(&command, find_cmd(c));

				break;
			}

		case 'E':	/* Rename */
		case 'W':{	/* Swap */
				set_command(&command, find_cmd(c));
				name = check_set_name(optarg);

				if (optind < argc
				    && argv[optind][0] != '-')
					newname = check_set_name(argv[optind++]);
				else
					exit_error(PARAMETER_PROBLEM,
						   "-%c requires a setname "
						   "and the new name for that set",
						   cmd2char(CMD_RENAME));

				break;
			}

		case 'A':	/* Add IP */
		case 'D':	/* Del IP */
		case 'T':{	/* Test IP */
				set_command(&command, find_cmd(c));

				name = check_set_name(optarg);

				/* IP */
				if (optind < argc
				    && argv[optind][0] != '-')
					adt = argv[optind++];
				else
					exit_error(PARAMETER_PROBLEM,
						   "-%c requires setname and IP",
						   c);

				res = parse_adt_cmdline(command, name, adt,
							&set, &settype);

				if (!res)
					exit_error(PARAMETER_PROBLEM,
						   "Unknown arg `%s'",
						   argv[optind - 1]);

				res = 0;
				break;
			}

			/* options */

		case 'n':
			add_option(&options, OPT_NUMERIC);
			break;

		case 'r':
			if (!(options & OPT_NUMERIC))
				add_option(&options, OPT_RESOLVE);
			break;

		case 's':
			add_option(&options, OPT_SORTED);
			break;

		case 'q':
			add_option(&options, OPT_QUIET);
			option_quiet = 1;
			break;

#ifdef IPSET_DEBUG
		case 'z':	/* debug */
			add_option(&options, OPT_DEBUG);
			option_debug = 1;
			break;
#endif

		case 1:	/* non option */
			printf("Bad argument `%s'\n", optarg);
			exit_tryhelp(PARAMETER_PROBLEM);
			break;	/*always good */

		default:{
				DP("default");

				switch (command) {
				case CMD_CREATE:
					res = settype->create_parse(
					    		c - settype->option_offset,
							argv,
							settype->data,
							&settype->flags);
					break;

				default:
					res = 0;	/* failed */
				}	/* switch (command) */


				if (!res)
					exit_error(PARAMETER_PROBLEM,
						   "Unknown arg `%s'",
						   argv[optind - 1]);
				
				res = 0;
			}

			DP("next arg");
		}	/* switch */

	}	/* while( getopt_long() ) */


	if (optind < argc)
		exit_error(PARAMETER_PROBLEM,
			   "unknown arguments found on commandline");
	if (command == CMD_NONE)
		exit_error(PARAMETER_PROBLEM, "no command specified");

	/* Check options */
	generic_opt_check(command, options);

	DP("cmd: %c", cmd2char(command));
	
	check_protocolversion();

	switch (command) {
	case CMD_CREATE:
		DP("CMD_CREATE");
		if (restore)
			set_restore_create(name, settype);
		else
			set_create(name, settype);
		break;

	case CMD_DESTROY:
		set_destroy(name, IP_SET_OP_DESTROY, CMD_DESTROY);
		break;

	case CMD_FLUSH:
		set_destroy(name, IP_SET_OP_FLUSH, CMD_FLUSH);
		break;

	case CMD_RENAME:
		set_rename(name, newname, IP_SET_OP_RENAME, CMD_RENAME);
		break;

	case CMD_SWAP:
		set_rename(name, newname, IP_SET_OP_SWAP, CMD_SWAP);
		break;

	case CMD_LIST:
		list_sets(name, options);
		break;

	case CMD_SAVE:
		set_save(name);
		break;

	case CMD_RESTORE:
		set_restore(argv[0]);
		break;

	case CMD_ADD:
		if (restore)
			set_restore_add(set, adt);
		else
			set_adtip(set, adt, IP_SET_OP_ADD_IP, CMD_ADD);
		break;

	case CMD_DEL:
		set_adtip(set, adt, IP_SET_OP_DEL_IP, CMD_DEL);
		break;

	case CMD_TEST:
		res = set_adtip(set, adt, IP_SET_OP_TEST_IP, CMD_TEST);
		break;

	case CMD_HELP:
		set_help(settype);
		break;

	default:
		/* Will never happen */
		break; /* Keep the compiler happy */

	}	/* switch( command ) */

	return res;
}


int main(int argc, char *argv[])
{	
	return parse_commandline(argc, argv);

}
