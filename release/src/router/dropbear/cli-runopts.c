/*
 * Dropbear - a SSH2 server
 * 
 * Copyright (c) 2002,2003 Matt Johnston
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */

#include "includes.h"
#include "runopts.h"
#include "signkey.h"
#include "buffer.h"
#include "dbutil.h"
#include "algo.h"
#include "tcpfwd.h"
#include "list.h"

cli_runopts cli_opts; /* GLOBAL */

static void printhelp(void);
static void parse_hostname(const char* orighostarg);
static void parse_multihop_hostname(const char* orighostarg, const char* argv0);
static void fill_own_user(void);
#ifdef ENABLE_CLI_PUBKEY_AUTH
static void loadidentityfile(const char* filename, int warnfail);
#endif
#ifdef ENABLE_CLI_ANYTCPFWD
static void addforward(const char* str, m_list *fwdlist);
#endif
#ifdef ENABLE_CLI_NETCAT
static void add_netcat(const char *str);
#endif
static void add_extendedopt(const char *str);

static void printhelp() {

	fprintf(stderr, "Dropbear SSH client v%s https://matt.ucc.asn.au/dropbear/dropbear.html\n"
#ifdef ENABLE_CLI_MULTIHOP
					"Usage: %s [options] [user@]host[/port][,[user@]host/port],...] [command]\n"
#else
					"Usage: %s [options] [user@]host[/port] [command]\n"
#endif
					"-p <remoteport>\n"
					"-l <username>\n"
					"-t    Allocate a pty\n"
					"-T    Don't allocate a pty\n"
					"-N    Don't run a remote command\n"
					"-f    Run in background after auth\n"
					"-y    Always accept remote host key if unknown\n"
					"-y -y Don't perform any remote host key checking (caution)\n"
					"-s    Request a subsystem (use by external sftp)\n"
					"-o option     Set option in OpenSSH-like format ('-o help' to list options)\n"
#ifdef ENABLE_CLI_PUBKEY_AUTH
					"-i <identityfile>   (multiple allowed, default %s)\n"
#endif
#ifdef ENABLE_CLI_AGENTFWD
					"-A    Enable agent auth forwarding\n"
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
					"-L <[listenaddress:]listenport:remotehost:remoteport> Local port forwarding\n"
					"-g    Allow remote hosts to connect to forwarded ports\n"
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
					"-R <[listenaddress:]listenport:remotehost:remoteport> Remote port forwarding\n"
#endif
					"-W <receive_window_buffer> (default %d, larger may be faster, max 1MB)\n"
					"-K <keepalive>  (0 is never, default %d)\n"
					"-I <idle_timeout>  (0 is never, default %d)\n"
#ifdef ENABLE_CLI_NETCAT
					"-B <endhost:endport> Netcat-alike forwarding\n"
#endif				
#ifdef ENABLE_CLI_PROXYCMD
					"-J <proxy_program> Use program pipe rather than TCP connection\n"
#endif
#ifdef ENABLE_USER_ALGO_LIST
					"-c <cipher list> Specify preferred ciphers ('-c help' to list options)\n"
					"-m <MAC list> Specify preferred MACs for packet verification (or '-m help')\n"
#endif
					"-V    Version\n"
#ifdef DEBUG_TRACE
					"-v    verbose (compiled with DEBUG_TRACE)\n"
#endif
					,DROPBEAR_VERSION, cli_opts.progname,
#ifdef ENABLE_CLI_PUBKEY_AUTH
					DROPBEAR_DEFAULT_CLI_AUTHKEY,
#endif
					DEFAULT_RECV_WINDOW, DEFAULT_KEEPALIVE, DEFAULT_IDLE_TIMEOUT);
					
}

void cli_getopts(int argc, char ** argv) {
	unsigned int i, j;
	char ** next = 0;
	enum {
		OPT_EXTENDED_OPTIONS,
#ifdef ENABLE_CLI_PUBKEY_AUTH
		OPT_AUTHKEY,
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
		OPT_LOCALTCPFWD,
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
		OPT_REMOTETCPFWD,
#endif
#ifdef ENABLE_CLI_NETCAT
		OPT_NETCAT,
#endif
		/* a flag (no arg) if 'next' is NULL, a string-valued option otherwise */
		OPT_OTHER
	} opt;
	unsigned int cmdlen;
	char* dummy = NULL; /* Not used for anything real */

	char* recv_window_arg = NULL;
	char* keepalive_arg = NULL;
	char* idle_timeout_arg = NULL;
	char *host_arg = NULL;
	char c;

	/* see printhelp() for options */
	cli_opts.progname = argv[0];
	cli_opts.remotehost = NULL;
	cli_opts.remoteport = NULL;
	cli_opts.username = NULL;
	cli_opts.cmd = NULL;
	cli_opts.no_cmd = 0;
	cli_opts.backgrounded = 0;
	cli_opts.wantpty = 9; /* 9 means "it hasn't been touched", gets set later */
	cli_opts.always_accept_key = 0;
	cli_opts.no_hostkey_check = 0;
	cli_opts.is_subsystem = 0;
#ifdef ENABLE_CLI_PUBKEY_AUTH
	cli_opts.privkeys = list_new();
#endif
#ifdef ENABLE_CLI_ANYTCPFWD
	cli_opts.exit_on_fwd_failure = 0;
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
	cli_opts.localfwds = list_new();
	opts.listen_fwd_all = 0;
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
	cli_opts.remotefwds = list_new();
#endif
#ifdef ENABLE_CLI_AGENTFWD
	cli_opts.agent_fwd = 0;
	cli_opts.agent_fd = -1;
	cli_opts.agent_keys_loaded = 0;
#endif
#ifdef ENABLE_CLI_PROXYCMD
	cli_opts.proxycmd = NULL;
#endif
#ifndef DISABLE_ZLIB
	opts.compress_mode = DROPBEAR_COMPRESS_ON;
#endif
#ifdef ENABLE_USER_ALGO_LIST
	opts.cipher_list = NULL;
	opts.mac_list = NULL;
#endif
#ifndef DISABLE_SYSLOG
	opts.usingsyslog = 0;
#endif
	/* not yet
	opts.ipv4 = 1;
	opts.ipv6 = 1;
	*/
	opts.recv_window = DEFAULT_RECV_WINDOW;
	opts.keepalive_secs = DEFAULT_KEEPALIVE;
	opts.idle_timeout_secs = DEFAULT_IDLE_TIMEOUT;

	fill_own_user();

	for (i = 1; i < (unsigned int)argc; i++) {
		/* Handle non-flag arguments such as hostname or commands for the remote host */
		if (argv[i][0] != '-')
		{
			if (host_arg == NULL) {
				host_arg = argv[i];
				continue;
			}
			/* Commands to pass to the remote host. No more flag handling,
			commands are consumed below */
			break;
		}

		/* Begins with '-' */
		opt = OPT_OTHER;
		for (j = 1; (c = argv[i][j]) != '\0' && !next && opt == OPT_OTHER; j++) {
			switch (c) {
				case 'y': /* always accept the remote hostkey */
					if (cli_opts.always_accept_key) {
						/* twice means no checking at all */
						cli_opts.no_hostkey_check = 1;
					}
					cli_opts.always_accept_key = 1;
					break;
				case 'p': /* remoteport */
					next = &cli_opts.remoteport;
					break;
#ifdef ENABLE_CLI_PUBKEY_AUTH
				case 'i': /* an identityfile */
					opt = OPT_AUTHKEY;
					break;
#endif
				case 't': /* we want a pty */
					cli_opts.wantpty = 1;
					break;
				case 'T': /* don't want a pty */
					cli_opts.wantpty = 0;
					break;
				case 'N':
					cli_opts.no_cmd = 1;
					break;
				case 'f':
					cli_opts.backgrounded = 1;
					break;
				case 's':
					cli_opts.is_subsystem = 1;
					break;
				case 'o':
					opt = OPT_EXTENDED_OPTIONS;
					break;
#ifdef ENABLE_CLI_LOCALTCPFWD
				case 'L':
					opt = OPT_LOCALTCPFWD;
					break;
				case 'g':
					opts.listen_fwd_all = 1;
					break;
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
				case 'R':
					opt = OPT_REMOTETCPFWD;
					break;
#endif
#ifdef ENABLE_CLI_NETCAT
				case 'B':
					opt = OPT_NETCAT;
					break;
#endif
#ifdef ENABLE_CLI_PROXYCMD
				case 'J':
					next = &cli_opts.proxycmd;
					break;
#endif
				case 'l':
					next = &cli_opts.username;
					break;
				case 'h':
					printhelp();
					exit(EXIT_SUCCESS);
					break;
				case 'u':
					/* backwards compatibility with old urandom option */
					break;
				case 'W':
					next = &recv_window_arg;
					break;
				case 'K':
					next = &keepalive_arg;
					break;
				case 'I':
					next = &idle_timeout_arg;
					break;
#ifdef ENABLE_CLI_AGENTFWD
				case 'A':
					cli_opts.agent_fwd = 1;
					break;
#endif
#ifdef ENABLE_USER_ALGO_LIST
				case 'c':
					next = &opts.cipher_list;
					break;
				case 'm':
					next = &opts.mac_list;
					break;
#endif
#ifdef DEBUG_TRACE
				case 'v':
					debug_trace = 1;
					break;
#endif
				case 'F':
				case 'e':
#ifndef ENABLE_USER_ALGO_LIST
				case 'c':
				case 'm':
#endif
				case 'D':
#ifndef ENABLE_CLI_REMOTETCPFWD
				case 'R':
#endif
#ifndef ENABLE_CLI_LOCALTCPFWD
				case 'L':
#endif
				case 'V':
					print_version();
					exit(EXIT_SUCCESS);
					break;
				case 'b':
					next = &dummy;
					/* FALLTHROUGH */
				default:
					fprintf(stderr,
						"WARNING: Ignoring unknown option -%c\n", c);
					break;
			} /* Switch */
		}

		if (!next && opt == OPT_OTHER) /* got a flag */
			continue;

		if (c == '\0') {
			i++;
			j = 0;
			if (!argv[i])
				dropbear_exit("Missing argument");
		}

		if (opt == OPT_EXTENDED_OPTIONS) {
			TRACE(("opt extended"))
			add_extendedopt(&argv[i][j]);
		}
		else
#ifdef ENABLE_CLI_PUBKEY_AUTH
		if (opt == OPT_AUTHKEY) {
			TRACE(("opt authkey"))
			loadidentityfile(&argv[i][j], 1);
		}
		else
#endif
#ifdef ENABLE_CLI_REMOTETCPFWD
		if (opt == OPT_REMOTETCPFWD) {
			TRACE(("opt remotetcpfwd"))
			addforward(&argv[i][j], cli_opts.remotefwds);
		}
		else
#endif
#ifdef ENABLE_CLI_LOCALTCPFWD
		if (opt == OPT_LOCALTCPFWD) {
			TRACE(("opt localtcpfwd"))
			addforward(&argv[i][j], cli_opts.localfwds);
		}
		else
#endif
#ifdef ENABLE_CLI_NETCAT
		if (opt == OPT_NETCAT) {
			TRACE(("opt netcat"))
			add_netcat(&argv[i][j]);
		}
		else
#endif
		if (next) {
			/* The previous flag set a value to assign */
			*next = &argv[i][j];
			if (*next == NULL)
				dropbear_exit("Invalid null argument");
			next = NULL;
		}
	}

	/* Done with options/flags; now handle the hostname (which may not
	 * start with a hyphen) and optional command */

	if (host_arg == NULL) { /* missing hostname */
		printhelp();
		exit(EXIT_FAILURE);
	}
	TRACE(("host is: %s", host_arg))

	if (i < (unsigned int)argc) {
		/* Build the command to send */
		cmdlen = 0;
		for (j = i; j < (unsigned int)argc; j++)
			cmdlen += strlen(argv[j]) + 1; /* +1 for spaces */

		/* Allocate the space */
		cli_opts.cmd = (char*)m_malloc(cmdlen);
		cli_opts.cmd[0] = '\0';

		/* Append all the bits */
		for (j = i; j < (unsigned int)argc; j++) {
			strlcat(cli_opts.cmd, argv[j], cmdlen);
			strlcat(cli_opts.cmd, " ", cmdlen);
		}
		/* It'll be null-terminated here */
		TRACE(("cmd is: %s", cli_opts.cmd))
	}

	/* And now a few sanity checks and setup */

#ifdef ENABLE_USER_ALGO_LIST
	parse_ciphers_macs();
#endif

#ifdef ENABLE_CLI_PROXYCMD                                                                                                                                   
	if (cli_opts.proxycmd) {
		/* To match the common path of m_freeing it */
		cli_opts.proxycmd = m_strdup(cli_opts.proxycmd);
	}
#endif

	if (cli_opts.remoteport == NULL) {
		cli_opts.remoteport = "22";
	}

	/* If not explicitly specified with -t or -T, we don't want a pty if
	 * there's a command, but we do otherwise */
	if (cli_opts.wantpty == 9) {
		if (cli_opts.cmd == NULL) {
			cli_opts.wantpty = 1;
		} else {
			cli_opts.wantpty = 0;
		}
	}

	if (cli_opts.backgrounded && cli_opts.cmd == NULL
			&& cli_opts.no_cmd == 0) {
		dropbear_exit("Command required for -f");
	}
	
	if (recv_window_arg) {
		opts.recv_window = atol(recv_window_arg);
		if (opts.recv_window == 0 || opts.recv_window > MAX_RECV_WINDOW) {
			dropbear_exit("Bad recv window '%s'", recv_window_arg);
		}
	}
	if (keepalive_arg) {
		unsigned int val;
		if (m_str_to_uint(keepalive_arg, &val) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad keepalive '%s'", keepalive_arg);
		}
		opts.keepalive_secs = val;
	}

	if (idle_timeout_arg) {
		unsigned int val;
		if (m_str_to_uint(idle_timeout_arg, &val) == DROPBEAR_FAILURE) {
			dropbear_exit("Bad idle_timeout '%s'", idle_timeout_arg);
		}
		opts.idle_timeout_secs = val;
	}

#ifdef ENABLE_CLI_NETCAT
	if (cli_opts.cmd && cli_opts.netcat_host) {
		dropbear_log(LOG_INFO, "Ignoring command '%s' in netcat mode", cli_opts.cmd);
	}
#endif

#if defined(DROPBEAR_DEFAULT_CLI_AUTHKEY) && defined(ENABLE_CLI_PUBKEY_AUTH)
	{
		char *expand_path = expand_homedir_path(DROPBEAR_DEFAULT_CLI_AUTHKEY);
		loadidentityfile(expand_path, 0);
		m_free(expand_path);
	}
#endif

	/* The hostname gets set up last, since
	 * in multi-hop mode it will require knowledge
	 * of other flags such as -i */
#ifdef ENABLE_CLI_MULTIHOP
	parse_multihop_hostname(host_arg, argv[0]);
#else
	parse_hostname(host_arg);
#endif
}

#ifdef ENABLE_CLI_PUBKEY_AUTH
static void loadidentityfile(const char* filename, int warnfail) {
	sign_key *key;
	enum signkey_type keytype;

	TRACE(("loadidentityfile %s", filename))

	key = new_sign_key();
	keytype = DROPBEAR_SIGNKEY_ANY;
	if ( readhostkey(filename, key, &keytype) != DROPBEAR_SUCCESS ) {
		if (warnfail) {
			dropbear_log(LOG_WARNING, "Failed loading keyfile '%s'\n", filename);
		}
		sign_key_free(key);
	} else {
		key->type = keytype;
		key->source = SIGNKEY_SOURCE_RAW_FILE;
		key->filename = m_strdup(filename);
		list_append(cli_opts.privkeys, key);
	}
}
#endif

#ifdef ENABLE_CLI_MULTIHOP

static char*
multihop_passthrough_args() {
	char *ret;
	int total;
	unsigned int len = 0;
	m_list_elem *iter;
	/* Fill out -i, -y, -W options that make sense for all
	 * the intermediate processes */
#ifdef ENABLE_CLI_PUBKEY_AUTH
	for (iter = cli_opts.privkeys->first; iter; iter = iter->next)
	{
		sign_key * key = (sign_key*)iter->item;
		len += 3 + strlen(key->filename);
	}
#endif /* ENABLE_CLI_PUBKEY_AUTH */

	len += 30; /* space for -W <size>, terminator. */
	ret = m_malloc(len);
	total = 0;

	if (cli_opts.no_hostkey_check)
	{
		int written = snprintf(ret+total, len-total, "-y -y ");
		total += written;
	}
	else if (cli_opts.always_accept_key)
	{
		int written = snprintf(ret+total, len-total, "-y ");
		total += written;
	}

	if (opts.recv_window != DEFAULT_RECV_WINDOW)
	{
		int written = snprintf(ret+total, len-total, "-W %u ", opts.recv_window);
		total += written;
	}

#ifdef ENABLE_CLI_PUBKEY_AUTH
	for (iter = cli_opts.privkeys->first; iter; iter = iter->next)
	{
		sign_key * key = (sign_key*)iter->item;
		const size_t size = len - total;
		int written = snprintf(ret+total, size, "-i %s ", key->filename);
		dropbear_assert((unsigned int)written < size);
		total += written;
	}
#endif /* ENABLE_CLI_PUBKEY_AUTH */

	/* if args were passed, total will be not zero, and it will have a space at the end, so remove that */
	if (total > 0) 
	{
		total--;
	}

	return ret;
}

/* Sets up 'onion-forwarding' connections. This will spawn
 * a separate dbclient process for each hop.
 * As an example, if the cmdline is
 *   dbclient wrt,madako,canyons
 * then we want to run:
 *   dbclient -J "dbclient -B canyons:22 wrt,madako" canyons
 * and then the inner dbclient will recursively run:
 *   dbclient -J "dbclient -B madako:22 wrt" madako
 * etc for as many hosts as we want.
 *
 * Ports for hosts can be specified as host/port.
 */
static void parse_multihop_hostname(const char* orighostarg, const char* argv0) {
	char *userhostarg = NULL;
	char *hostbuf = NULL;
	char *last_hop = NULL;
	char *remainder = NULL;

	/* both scp and rsync parse a user@host argument
	 * and turn it into "-l user host". This breaks
	 * for our multihop syntax, so we suture it back together.
	 * This will break usernames that have both '@' and ',' in them,
	 * though that should be fairly uncommon. */
	if (cli_opts.username 
			&& strchr(cli_opts.username, ',') 
			&& strchr(cli_opts.username, '@')) {
		unsigned int len = strlen(orighostarg) + strlen(cli_opts.username) + 2;
		hostbuf = m_malloc(len);
		snprintf(hostbuf, len, "%s@%s", cli_opts.username, orighostarg);
	} else {
		hostbuf = m_strdup(orighostarg);
	}
	userhostarg = hostbuf;

	last_hop = strrchr(userhostarg, ',');
	if (last_hop) {
		if (last_hop == userhostarg) {
			dropbear_exit("Bad multi-hop hostnames");
		}
		*last_hop = '\0';
		last_hop++;
		remainder = userhostarg;
		userhostarg = last_hop;
	}

	parse_hostname(userhostarg);

	if (last_hop) {
		/* Set up the proxycmd */
		unsigned int cmd_len = 0;
		char *passthrough_args = multihop_passthrough_args();
		if (cli_opts.proxycmd) {
			dropbear_exit("-J can't be used with multihop mode");
		}
		if (cli_opts.remoteport == NULL) {
			cli_opts.remoteport = "22";
		}
		cmd_len = strlen(argv0) + strlen(remainder) 
			+ strlen(cli_opts.remotehost) + strlen(cli_opts.remoteport)
			+ strlen(passthrough_args)
			+ 30;
		cli_opts.proxycmd = m_malloc(cmd_len);
		snprintf(cli_opts.proxycmd, cmd_len, "%s -B %s:%s %s %s", 
				argv0, cli_opts.remotehost, cli_opts.remoteport, 
				passthrough_args, remainder);
#ifndef DISABLE_ZLIB
		/* The stream will be incompressible since it's encrypted. */
		opts.compress_mode = DROPBEAR_COMPRESS_OFF;
#endif
		m_free(passthrough_args);
	}
	m_free(hostbuf);
}
#endif /* !ENABLE_CLI_MULTIHOP */

/* Parses a [user@]hostname[/port] argument. */
static void parse_hostname(const char* orighostarg) {
	char *userhostarg = NULL;
	char *port = NULL;

	userhostarg = m_strdup(orighostarg);

	cli_opts.remotehost = strchr(userhostarg, '@');
	if (cli_opts.remotehost == NULL) {
		/* no username portion, the cli-auth.c code can figure the
		 * local user's name */
		cli_opts.remotehost = userhostarg;
	} else {
		cli_opts.remotehost[0] = '\0'; /* Split the user/host */
		cli_opts.remotehost++;
		cli_opts.username = userhostarg;
	}

	if (cli_opts.username == NULL) {
		cli_opts.username = m_strdup(cli_opts.own_user);
	}

	port = strchr(cli_opts.remotehost, '^');
	if (!port)  {
		/* legacy separator */
		port = strchr(cli_opts.remotehost, '/');
	}
	if (port) {
		*port = '\0';
		cli_opts.remoteport = port+1;
	}

	if (cli_opts.remotehost[0] == '\0') {
		dropbear_exit("Bad hostname");
	}
}

#ifdef ENABLE_CLI_NETCAT
static void add_netcat(const char* origstr) {
	char *portstr = NULL;
	
	char * str = m_strdup(origstr);
	
	portstr = strchr(str, ':');
	if (portstr == NULL) {
		TRACE(("No netcat port"))
		goto fail;
	}
	*portstr = '\0';
	portstr++;
	
	if (strchr(portstr, ':')) {
		TRACE(("Multiple netcat colons"))
		goto fail;
	}
	
	if (m_str_to_uint(portstr, &cli_opts.netcat_port) == DROPBEAR_FAILURE) {
		TRACE(("bad netcat port"))
		goto fail;
	}
	
	if (cli_opts.netcat_port > 65535) {
		TRACE(("too large netcat port"))
		goto fail;
	}
	
	cli_opts.netcat_host = str;
	return;
	
fail:
	dropbear_exit("Bad netcat endpoint '%s'", origstr);
}
#endif

static void fill_own_user() {
	uid_t uid;
	struct passwd *pw = NULL; 

	uid = getuid();

	pw = getpwuid(uid);
	if (pw && pw->pw_name != NULL) {
		cli_opts.own_user = m_strdup(pw->pw_name);
	} else {
		dropbear_log(LOG_INFO, "Warning: failed to identify current user. Trying anyway.");
		cli_opts.own_user = m_strdup("unknown");
	}

}

#ifdef ENABLE_CLI_ANYTCPFWD
/* Turn a "[listenaddr:]listenport:remoteaddr:remoteport" string into into a forwarding
 * set, and add it to the forwarding list */
static void addforward(const char* origstr, m_list *fwdlist) {

	char *part1 = NULL, *part2 = NULL, *part3 = NULL, *part4 = NULL;
	char * listenaddr = NULL;
	char * listenport = NULL;
	char * connectaddr = NULL;
	char * connectport = NULL;
	struct TCPFwdEntry* newfwd = NULL;
	char * str = NULL;

	TRACE(("enter addforward"))

	/* We need to split the original argument up. This var
	   is never free()d. */ 
	str = m_strdup(origstr);

	part1 = str;

	part2 = strchr(str, ':');
	if (part2 == NULL) {
		TRACE(("part2 == NULL"))
		goto fail;
	}
	*part2 = '\0';
	part2++;

	part3 = strchr(part2, ':');
	if (part3 == NULL) {
		TRACE(("part3 == NULL"))
		goto fail;
	}
	*part3 = '\0';
	part3++;

	part4 = strchr(part3, ':');
	if (part4) {
		*part4 = '\0';
		part4++;
	}

	if (part4) {
		listenaddr = part1;
		listenport = part2;
		connectaddr = part3;
		connectport = part4;
	} else {
		listenaddr = NULL;
		listenport = part1;
		connectaddr = part2;
		connectport = part3;
	}

	newfwd = m_malloc(sizeof(struct TCPFwdEntry));

	/* Now we check the ports - note that the port ints are unsigned,
	 * the check later only checks for >= MAX_PORT */
	if (m_str_to_uint(listenport, &newfwd->listenport) == DROPBEAR_FAILURE) {
		TRACE(("bad listenport strtoul"))
		goto fail;
	}

	if (m_str_to_uint(connectport, &newfwd->connectport) == DROPBEAR_FAILURE) {
		TRACE(("bad connectport strtoul"))
		goto fail;
	}

	newfwd->listenaddr = listenaddr;
	newfwd->connectaddr = connectaddr;

	if (newfwd->listenport > 65535) {
		TRACE(("listenport > 65535"))
		goto badport;
	}
		
	if (newfwd->connectport > 65535) {
		TRACE(("connectport > 65535"))
		goto badport;
	}

	newfwd->have_reply = 0;
	list_append(fwdlist, newfwd);

	TRACE(("leave addforward: done"))
	return;

fail:
	dropbear_exit("Bad TCP forward '%s'", origstr);

badport:
	dropbear_exit("Bad TCP port in '%s'", origstr);
}
#endif

static int match_extendedopt(const char** strptr, const char *optname) {
	int seen_eq = 0;
	int optlen = strlen(optname);
	const char *str = *strptr;

	while (isspace(*str)) {
		++str;
	}

	if (strncasecmp(str, optname, optlen) != 0) {
		return DROPBEAR_FAILURE;
	}

	str += optlen;

	while (isspace(*str) || (!seen_eq && *str == '=')) {
		if (*str == '=') {
			seen_eq = 1;
		}
		++str;
	}

	if (str-*strptr == optlen) {
		/* matched just a prefix of optname */
		return DROPBEAR_FAILURE;
	}

	*strptr = str;
	return DROPBEAR_SUCCESS;
}

static int parse_flag_value(const char *value) {
	if (strcmp(value, "yes") == 0 || strcmp(value, "true") == 0) {
		return 1;
	} else if (strcmp(value, "no") == 0 || strcmp(value, "false") == 0) {
		return 0;
	}

	dropbear_exit("Bad yes/no argument '%s'", value);
}

static void add_extendedopt(const char* origstr) {
	const char *optstr = origstr;

	if (strcmp(origstr, "help") == 0) {
		dropbear_log(LOG_INFO, "Available options:\n"
#ifdef ENABLE_CLI_ANYTCPFWD
			"\tExitOnForwardFailure\n"
#endif
#ifndef DISABLE_SYSLOG
			"\tUseSyslog\n"
#endif
		);
		exit(EXIT_SUCCESS);
	}

#ifdef ENABLE_CLI_ANYTCPFWD
	if (match_extendedopt(&optstr, "ExitOnForwardFailure") == DROPBEAR_SUCCESS) {
		cli_opts.exit_on_fwd_failure = parse_flag_value(optstr);
		return;
	}
#endif

#ifndef DISABLE_SYSLOG
	if (match_extendedopt(&optstr, "UseSyslog") == DROPBEAR_SUCCESS) {
		opts.usingsyslog = parse_flag_value(optstr);
		return;
	}
#endif

	dropbear_log(LOG_WARNING, "Ignoring unknown configuration option '%s'", origstr);
}
