/* vi: set sw=4 ts=4: */
/*
 * bare bones sendmail
 *
 * Copyright (C) 2008 by Vladimir Dronnikov <dronnikov@gmail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//kbuild:lib-$(CONFIG_SENDMAIL) += sendmail.o mail.o

//usage:#define sendmail_trivial_usage
//usage:       "[OPTIONS] [RECIPIENT_EMAIL]..."
//usage:#define sendmail_full_usage "\n\n"
//usage:       "Read email from stdin and send it\n"
//usage:     "\nStandard options:"
//usage:     "\n	-t		Read additional recipients from message body"
//usage:     "\n	-f SENDER	Sender (required)"
//usage:     "\n	-o OPTIONS	Various options. -oi implied, others are ignored"
//usage:     "\n	-i		-oi synonym. implied and ignored"
//usage:     "\n"
//usage:     "\nBusybox specific options:"
//usage:     "\n	-v		Verbose"
//usage:     "\n	-w SECS		Network timeout"
//usage:     "\n	-H 'PROG ARGS'	Run connection helper"
//usage:     "\n			Examples:"
//usage:     "\n			-H 'exec openssl s_client -quiet -tls1 -starttls smtp"
//usage:     "\n				-connect smtp.gmail.com:25' <email.txt"
//usage:     "\n				[4<username_and_passwd.txt | -auUSER -apPASS]"
//usage:     "\n			-H 'exec openssl s_client -quiet -tls1"
//usage:     "\n				-connect smtp.gmail.com:465' <email.txt"
//usage:     "\n				[4<username_and_passwd.txt | -auUSER -apPASS]"
//usage:     "\n	-S HOST[:PORT]	Server"
//usage:     "\n	-auUSER		Username for AUTH LOGIN"
//usage:     "\n	-apPASS 	Password for AUTH LOGIN"
////usage:     "\n	-amMETHOD	Authentication method. Ignored. LOGIN is implied"
//usage:     "\n"
//usage:     "\nOther options are silently ignored; -oi -t is implied"
//usage:	IF_MAKEMIME(
//usage:     "\nUse makemime to create emails with attachments"
//usage:	)

#include "libbb.h"
#include "mail.h"

// limit maximum allowed number of headers to prevent overflows.
// set to 0 to not limit
#define MAX_HEADERS 256

static void send_r_n(const char *s)
{
	if (verbose)
		bb_error_msg("send:'%s'", s);
	printf("%s\r\n", s);
}

static int smtp_checkp(const char *fmt, const char *param, int code)
{
	char *answer;
	char *msg = send_mail_command(fmt, param);
	// read stdin
	// if the string has a form NNN- -- read next string. E.g. EHLO response
	// parse first bytes to a number
	// if code = -1 then just return this number
	// if code != -1 then checks whether the number equals the code
	// if not equal -> die saying msg
	while ((answer = xmalloc_fgetline(stdin)) != NULL) {
		if (verbose)
			bb_error_msg("recv:'%.*s'", (int)(strchrnul(answer, '\r') - answer), answer);
		if (strlen(answer) <= 3 || '-' != answer[3])
			break;
		free(answer);
	}
	if (answer) {
		int n = atoi(answer);
		if (timeout)
			alarm(0);
		free(answer);
		if (-1 == code || n == code) {
			free(msg);
			return n;
		}
	}
	bb_error_msg_and_die("%s failed", msg);
}

static int smtp_check(const char *fmt, int code)
{
	return smtp_checkp(fmt, NULL, code);
}

// strip argument of bad chars
static char *sane_address(char *str)
{
	char *s = str;
	char *p = s;
	while (*s) {
		if (isalnum(*s) || '_' == *s || '-' == *s || '.' == *s || '@' == *s) {
			*p++ = *s;
		}
		s++;
	}
	*p = '\0';
	return str;
}

static void rcptto(const char *s)
{
	// N.B. we don't die if recipient is rejected, for the other recipients may be accepted
	if (250 != smtp_checkp("RCPT TO:<%s>", s, -1))
		bb_error_msg("Bad recipient: <%s>", s);
}

int sendmail_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sendmail_main(int argc UNUSED_PARAM, char **argv)
{
	char *opt_connect = opt_connect;
	char *opt_from;
	char *s;
	llist_t *list = NULL;
	char *domain = sane_address(safe_getdomainname());
	unsigned nheaders = 0;
	int code;

	enum {
	//--- standard options
		OPT_t = 1 << 0,         // read message for recipients, append them to those on cmdline
		OPT_f = 1 << 1,         // sender address
		OPT_o = 1 << 2,         // various options. -oi IMPLIED! others are IGNORED!
		OPT_i = 1 << 3,         // IMPLIED!
	//--- BB specific options
		OPT_w = 1 << 4,         // network timeout
		OPT_H = 1 << 5,         // use external connection helper
		OPT_S = 1 << 6,         // specify connection string
		OPT_a = 1 << 7,         // authentication tokens
		OPT_v = 1 << 8,         // verbosity
	};

	// init global variables
	INIT_G();

	// save initial stdin since body is piped!
	xdup2(STDIN_FILENO, 3);
	G.fp0 = xfdopen_for_read(3);

	// parse options
	// -v is a counter, -f is required. -H and -S are mutually exclusive, -a is a list
	opt_complementary = "vv:f:w+:H--S:S--H:a::";
	// N.B. since -H and -S are mutually exclusive they do not interfere in opt_connect
	// -a is for ssmtp (http://downloads.openwrt.org/people/nico/man/man8/ssmtp.8.html) compatibility,
	// it is still under development.
	opts = getopt32(argv, "tf:o:iw:H:S:a::v", &opt_from, NULL,
			&timeout, &opt_connect, &opt_connect, &list, &verbose);
	//argc -= optind;
	argv += optind;

	// process -a[upm]<token> options
	if ((opts & OPT_a) && !list)
		bb_show_usage();
	while (list) {
		char *a = (char *) llist_pop(&list);
		if ('u' == a[0])
			G.user = xstrdup(a+1);
		if ('p' == a[0])
			G.pass = xstrdup(a+1);
		// N.B. we support only AUTH LOGIN so far
		//if ('m' == a[0])
		//	G.method = xstrdup(a+1);
	}
	// N.B. list == NULL here
	//bb_info_msg("OPT[%x] AU[%s], AP[%s], AM[%s], ARGV[%s]", opts, au, ap, am, *argv);

	// connect to server

	// connection helper ordered? ->
	if (opts & OPT_H) {
		const char *args[] = { "sh", "-c", opt_connect, NULL };
		// plug it in
		launch_helper(args);
		// Now:
		// our stdout will go to helper's stdin,
		// helper's stdout will be available on our stdin.

		// Wait for initial server message.
		// If helper (such as openssl) invokes STARTTLS, the initial 220
		// is swallowed by helper (and not repeated after TLS is initiated).
		// We will send NOOP cmd to server and check the response.
		// We should get 220+250 on plain connection, 250 on STARTTLSed session.
		//
		// The problem here is some servers delay initial 220 message,
		// and consider client to be a spammer if it starts sending cmds
		// before 220 reached it. The code below is unsafe in this regard:
		// in non-STARTTLSed case, we potentially send NOOP before 220
		// is sent by server.
		// Ideas? (--delay SECS opt? --assume-starttls-helper opt?)
		code = smtp_check("NOOP", -1);
		if (code == 220)
			// we got 220 - this is not STARTTLSed connection,
			// eat 250 response to our NOOP
			smtp_check(NULL, 250);
		else
		if (code != 250)
			bb_error_msg_and_die("SMTP init failed");
	} else {
		// vanilla connection
		int fd;
		// host[:port] not explicitly specified? -> use $SMTPHOST
		// no $SMTPHOST? -> use localhost
		if (!(opts & OPT_S)) {
			opt_connect = getenv("SMTPHOST");
			if (!opt_connect)
				opt_connect = (char *)"127.0.0.1";
		}
		// do connect
		fd = create_and_connect_stream_or_die(opt_connect, 25);
		// and make ourselves a simple IO filter
		xmove_fd(fd, STDIN_FILENO);
		xdup2(STDIN_FILENO, STDOUT_FILENO);

		// Wait for initial server 220 message
		smtp_check(NULL, 220);
	}

	// we should start with modern EHLO
	if (250 != smtp_checkp("EHLO %s", domain, -1))
		smtp_checkp("HELO %s", domain, 250);

	// perform authentication
	if (opts & OPT_a) {
		smtp_check("AUTH LOGIN", 334);
		// we must read credentials unless they are given via -a[up] options
		if (!G.user || !G.pass)
			get_cred_or_die(4);
		encode_base64(NULL, G.user, NULL);
		smtp_check("", 334);
		encode_base64(NULL, G.pass, NULL);
		smtp_check("", 235);
	}

	// set sender
	// N.B. we have here a very loosely defined algorythm
	// since sendmail historically offers no means to specify secrets on cmdline.
	// 1) server can require no authentication ->
	//	we must just provide a (possibly fake) reply address.
	// 2) server can require AUTH ->
	//	we must provide valid username and password along with a (possibly fake) reply address.
	//	For the sake of security username and password are to be read either from console or from a secured file.
	//	Since reading from console may defeat usability, the solution is either to read from a predefined
	//	file descriptor (e.g. 4), or again from a secured file.

	// got no sender address? -> use system username as a resort
	// N.B. we marked -f as required option!
	//if (!G.user) {
	//	// N.B. IMHO getenv("USER") can be way easily spoofed!
	//	G.user = xuid2uname(getuid());
	//	opt_from = xasprintf("%s@%s", G.user, domain);
	//}
	smtp_checkp("MAIL FROM:<%s>", opt_from, 250);

	// process message

	// read recipients from message and add them to those given on cmdline.
	// this means we scan stdin for To:, Cc:, Bcc: lines until an empty line
	// and then use the rest of stdin as message body
	code = 0; // set "analyze headers" mode
	while ((s = xmalloc_fgetline(G.fp0)) != NULL) {
 dump:
		// put message lines doubling leading dots
		if (code) {
			// escape leading dots
			// N.B. this feature is implied even if no -i (-oi) switch given
			// N.B. we need to escape the leading dot regardless of
			// whether it is single or not character on the line
			if ('.' == s[0] /*&& '\0' == s[1] */)
				printf(".");
			// dump read line
			send_r_n(s);
			free(s);
			continue;
		}

		// analyze headers
		// To: or Cc: headers add recipients
		if (opts & OPT_t) {
			if (0 == strncasecmp("To:", s, 3) || 0 == strncasecmp("Bcc:" + 1, s, 3)) {
				rcptto(sane_address(s+3));
				goto addheader;
			}
			// Bcc: header adds blind copy (hidden) recipient
			if (0 == strncasecmp("Bcc:", s, 4)) {
				rcptto(sane_address(s+4));
				free(s);
				continue; // N.B. Bcc: vanishes from headers!
			}
		}
		if (strchr(s, ':') || (list && isspace(s[0]))) {
			// other headers go verbatim
			// N.B. RFC2822 2.2.3 "Long Header Fields" allows for headers to occupy several lines.
			// Continuation is denoted by prefixing additional lines with whitespace(s).
			// Thanks (stefan.seyfried at googlemail.com) for pointing this out.
 addheader:
			// N.B. we allow MAX_HEADERS generic headers at most to prevent attacks
			if (MAX_HEADERS && ++nheaders >= MAX_HEADERS)
				goto bail;
			llist_add_to_end(&list, s);
		} else {
			// a line without ":" (an empty line too, by definition) doesn't look like a valid header
			// so stop "analyze headers" mode
 reenter:
			// put recipients specified on cmdline
			while (*argv) {
				char *t = sane_address(*argv);
				rcptto(t);
				//if (MAX_HEADERS && ++nheaders >= MAX_HEADERS)
				//	goto bail;
				llist_add_to_end(&list, xasprintf("To: %s", t));
				argv++;
			}
			// enter "put message" mode
			// N.B. DATA fails iff no recipients were accepted (or even provided)
			// in this case just bail out gracefully
			if (354 != smtp_check("DATA", -1))
				goto bail;
			// dump the headers
			while (list) {
				send_r_n((char *) llist_pop(&list));
			}
			// stop analyzing headers
			code++;
			// N.B. !s means: we read nothing, and nothing to be read in the future.
			// just dump empty line and break the loop
			if (!s) {
				send_r_n("");
				break;
			}
			// go dump message body
			// N.B. "s" already contains the first non-header line, so pretend we read it from input
			goto dump;
		}
	}
	// odd case: we didn't stop "analyze headers" mode -> message body is empty. Reenter the loop
	// N.B. after reenter code will be > 0
	if (!code)
		goto reenter;

	// finalize the message
	smtp_check(".", 250);
 bail:
	// ... and say goodbye
	smtp_check("QUIT", 221);
	// cleanup
	if (ENABLE_FEATURE_CLEAN_UP)
		fclose(G.fp0);

	return EXIT_SUCCESS;
}
