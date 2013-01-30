/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Taken from http://smarden.sunsite.dk/runit/sv.8.html:

sv - control and manage services monitored by runsv

sv [-v] [-w sec] command services
/etc/init.d/service [-w sec] command

The sv program reports the current status and controls the state of services
monitored by the runsv(8) supervisor.

services consists of one or more arguments, each argument naming a directory
service used by runsv(8). If service doesn't start with a dot or slash,
it is searched in the default services directory /var/service/, otherwise
relative to the current directory.

command is one of up, down, status, once, pause, cont, hup, alarm, interrupt,
1, 2, term, kill, or exit, or start, stop, restart, shutdown, force-stop,
force-reload, force-restart, force-shutdown.

The sv program can be sym-linked to /etc/init.d/ to provide an LSB init
script interface. The service to be controlled then is specified by the
base name of the "init script".

status
    Report the current status of the service, and the appendant log service
    if available, to standard output.
up
    If the service is not running, start it. If the service stops, restart it.
down
    If the service is running, send it the TERM signal, and the CONT signal.
    If ./run exits, start ./finish if it exists. After it stops, do not
    restart service.
once
    If the service is not running, start it. Do not restart it if it stops.
pause cont hup alarm interrupt quit 1 2 term kill
    If the service is running, send it the STOP, CONT, HUP, ALRM, INT, QUIT,
    USR1, USR2, TERM, or KILL signal respectively.
exit
    If the service is running, send it the TERM signal, and the CONT signal.
    Do not restart the service. If the service is down, and no log service
    exists, runsv(8) exits. If the service is down and a log service exists,
    send the TERM signal to the log service. If the log service is down,
    runsv(8) exits. This command is ignored if it is given to an appendant
    log service.

sv actually looks only at the first character of above commands.

Commands compatible to LSB init script actions:

status
    Same as status.
start
    Same as up, but wait up to 7 seconds for the command to take effect.
    Then report the status or timeout. If the script ./check exists in
    the service directory, sv runs this script to check whether the service
    is up and available; it's considered to be available if ./check exits
    with 0.
stop
    Same as down, but wait up to 7 seconds for the service to become down.
    Then report the status or timeout.
restart
    Send the commands term, cont, and up to the service, and wait up to
    7 seconds for the service to restart. Then report the status or timeout.
    If the script ./check exists in the service directory, sv runs this script
    to check whether the service is up and available again; it's considered
    to be available if ./check exits with 0.
shutdown
    Same as exit, but wait up to 7 seconds for the runsv(8) process
    to terminate. Then report the status or timeout.
force-stop
    Same as down, but wait up to 7 seconds for the service to become down.
    Then report the status, and on timeout send the service the kill command.
force-reload
    Send the service the term and cont commands, and wait up to
    7 seconds for the service to restart. Then report the status,
    and on timeout send the service the kill command.
force-restart
    Send the service the term, cont and up commands, and wait up to
    7 seconds for the service to restart. Then report the status, and
    on timeout send the service the kill command. If the script ./check
    exists in the service directory, sv runs this script to check whether
    the service is up and available again; it's considered to be available
    if ./check exits with 0.
force-shutdown
    Same as exit, but wait up to 7 seconds for the runsv(8) process to
    terminate. Then report the status, and on timeout send the service
    the kill command.

Additional Commands

check
    Check for the service to be in the state that's been requested. Wait up to
    7 seconds for the service to reach the requested state, then report
    the status or timeout. If the requested state of the service is up,
    and the script ./check exists in the service directory, sv runs
    this script to check whether the service is up and running;
    it's considered to be up if ./check exits with 0.

Options

-v
    wait up to 7 seconds for the command to take effect.
    Then report the status or timeout.
-w sec
    Override the default timeout of 7 seconds with sec seconds. Implies -v.

Environment

SVDIR
    The environment variable $SVDIR overrides the default services directory
    /var/service.
SVWAIT
    The environment variable $SVWAIT overrides the default 7 seconds to wait
    for a command to take effect. It is overridden by the -w option.

Exit Codes
    sv exits 0, if the command was successfully sent to all services, and,
    if it was told to wait, the command has taken effect to all services.

    For each service that caused an error (e.g. the directory is not
    controlled by a runsv(8) process, or sv timed out while waiting),
    sv increases the exit code by one and exits non zero. The maximum
    is 99. sv exits 100 on error.
*/

/* Busyboxed by Denys Vlasenko <vda.linux@googlemail.com> */
/* TODO: depends on runit_lib.c - review and reduce/eliminate */

//usage:#define sv_trivial_usage
//usage:       "[-v] [-w SEC] CMD SERVICE_DIR..."
//usage:#define sv_full_usage "\n\n"
//usage:       "Control services monitored by runsv supervisor.\n"
//usage:       "Commands (only first character is enough):\n"
//usage:       "\n"
//usage:       "status: query service status\n"
//usage:       "up: if service isn't running, start it. If service stops, restart it\n"
//usage:       "once: like 'up', but if service stops, don't restart it\n"
//usage:       "down: send TERM and CONT signals. If ./run exits, start ./finish\n"
//usage:       "	if it exists. After it stops, don't restart service\n"
//usage:       "exit: send TERM and CONT signals to service and log service. If they exit,\n"
//usage:       "	runsv exits too\n"
//usage:       "pause, cont, hup, alarm, interrupt, quit, 1, 2, term, kill: send\n"
//usage:       "STOP, CONT, HUP, ALRM, INT, QUIT, USR1, USR2, TERM, KILL signal to service"

#include <sys/poll.h>
#include <sys/file.h>
#include "libbb.h"
#include "runit_lib.h"

struct globals {
	const char *acts;
	char **service;
	unsigned rc;
/* "Bernstein" time format: unix + 0x400000000000000aULL */
	uint64_t tstart, tnow;
	svstatus_t svstatus;
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define acts         (G.acts        )
#define service      (G.service     )
#define rc           (G.rc          )
#define tstart       (G.tstart      )
#define tnow         (G.tnow        )
#define svstatus     (G.svstatus    )
#define INIT_G() do { } while (0)


#define str_equal(s,t) (!strcmp((s), (t)))


static void fatal_cannot(const char *m1) NORETURN;
static void fatal_cannot(const char *m1)
{
	bb_perror_msg("fatal: can't %s", m1);
	_exit(151);
}

static void out(const char *p, const char *m1)
{
	printf("%s%s: %s", p, *service, m1);
	if (errno) {
		printf(": %s", strerror(errno));
	}
	bb_putchar('\n'); /* will also flush the output */
}

#define WARN    "warning: "
#define OK      "ok: "

static void fail(const char *m1)
{
	++rc;
	out("fail: ", m1);
}
static void failx(const char *m1)
{
	errno = 0;
	fail(m1);
}
static void warn(const char *m1)
{
	++rc;
	/* "warning: <service>: <m1>\n" */
	out("warning: ", m1);
}
static void ok(const char *m1)
{
	errno = 0;
	out(OK, m1);
}

static int svstatus_get(void)
{
	int fd, r;

	fd = open("supervise/ok", O_WRONLY|O_NDELAY);
	if (fd == -1) {
		if (errno == ENODEV) {
			*acts == 'x' ? ok("runsv not running")
			             : failx("runsv not running");
			return 0;
		}
		warn("can't open supervise/ok");
		return -1;
	}
	close(fd);
	fd = open("supervise/status", O_RDONLY|O_NDELAY);
	if (fd == -1) {
		warn("can't open supervise/status");
		return -1;
	}
	r = read(fd, &svstatus, 20);
	close(fd);
	switch (r) {
	case 20:
		break;
	case -1:
		warn("can't read supervise/status");
		return -1;
	default:
		errno = 0;
		warn("can't read supervise/status: bad format");
		return -1;
	}
	return 1;
}

static unsigned svstatus_print(const char *m)
{
	int diff;
	int pid;
	int normallyup = 0;
	struct stat s;
	uint64_t timestamp;

	if (stat("down", &s) == -1) {
		if (errno != ENOENT) {
			bb_perror_msg(WARN"can't stat %s/down", *service);
			return 0;
		}
		normallyup = 1;
	}
	pid = SWAP_LE32(svstatus.pid_le32);
	timestamp = SWAP_BE64(svstatus.time_be64);
	if (pid) {
		switch (svstatus.run_or_finish) {
		case 1: printf("run: "); break;
		case 2: printf("finish: "); break;
		}
		printf("%s: (pid %d) ", m, pid);
	} else {
		printf("down: %s: ", m);
	}
	diff = tnow - timestamp;
	printf("%us", (diff < 0 ? 0 : diff));
	if (pid) {
		if (!normallyup) printf(", normally down");
		if (svstatus.paused) printf(", paused");
		if (svstatus.want == 'd') printf(", want down");
		if (svstatus.got_term) printf(", got TERM");
	} else {
		if (normallyup) printf(", normally up");
		if (svstatus.want == 'u') printf(", want up");
	}
	return pid ? 1 : 2;
}

static int status(const char *unused UNUSED_PARAM)
{
	int r;

	if (svstatus_get() <= 0)
		return 0;

	r = svstatus_print(*service);
	if (chdir("log") == -1) {
		if (errno != ENOENT) {
			printf("; log: "WARN"can't change to log service directory: %s",
					strerror(errno));
		}
	} else if (svstatus_get()) {
		printf("; ");
		svstatus_print("log");
	}
	bb_putchar('\n'); /* will also flush the output */
	return r;
}

static int checkscript(void)
{
	char *prog[2];
	struct stat s;
	int pid, w;

	if (stat("check", &s) == -1) {
		if (errno == ENOENT) return 1;
		bb_perror_msg(WARN"can't stat %s/check", *service);
		return 0;
	}
	/* if (!(s.st_mode & S_IXUSR)) return 1; */
	prog[0] = (char*)"./check";
	prog[1] = NULL;
	pid = spawn(prog);
	if (pid <= 0) {
		bb_perror_msg(WARN"can't %s child %s/check", "run", *service);
		return 0;
	}
	while (safe_waitpid(pid, &w, 0) == -1) {
		bb_perror_msg(WARN"can't %s child %s/check", "wait for", *service);
		return 0;
	}
	return WEXITSTATUS(w) == 0;
}

static int check(const char *a)
{
	int r;
	unsigned pid_le32;
	uint64_t timestamp;

	r = svstatus_get();
	if (r == -1)
		return -1;
	if (r == 0) {
		if (*a == 'x')
			return 1;
		return -1;
	}
	pid_le32 = svstatus.pid_le32;
	switch (*a) {
	case 'x':
		return 0;
	case 'u':
		if (!pid_le32 || svstatus.run_or_finish != 1) return 0;
		if (!checkscript()) return 0;
		break;
	case 'd':
		if (pid_le32) return 0;
		break;
	case 'c':
		if (pid_le32 && !checkscript()) return 0;
		break;
	case 't':
		if (!pid_le32 && svstatus.want == 'd') break;
		timestamp = SWAP_BE64(svstatus.time_be64);
		if ((tstart > timestamp) || !pid_le32 || svstatus.got_term || !checkscript())
			return 0;
		break;
	case 'o':
		timestamp = SWAP_BE64(svstatus.time_be64);
		if ((!pid_le32 && tstart > timestamp) || (pid_le32 && svstatus.want != 'd'))
			return 0;
	}
	printf(OK);
	svstatus_print(*service);
	bb_putchar('\n'); /* will also flush the output */
	return 1;
}

static int control(const char *a)
{
	int fd, r, l;

/* Is it an optimization?
   It causes problems with "sv o SRV; ...; sv d SRV"
   ('d' is not passed to SRV because its .want == 'd'):
	if (svstatus_get() <= 0)
		return -1;
	if (svstatus.want == *a)
		return 0;
*/
	fd = open("supervise/control", O_WRONLY|O_NDELAY);
	if (fd == -1) {
		if (errno != ENODEV)
			warn("can't open supervise/control");
		else
			*a == 'x' ? ok("runsv not running") : failx("runsv not running");
		return -1;
	}
	l = strlen(a);
	r = write(fd, a, l);
	close(fd);
	if (r != l) {
		warn("can't write to supervise/control");
		return -1;
	}
	return 1;
}

int sv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sv_main(int argc UNUSED_PARAM, char **argv)
{
	char *x;
	char *action;
	const char *varservice = CONFIG_SV_DEFAULT_SERVICE_DIR;
	unsigned waitsec = 7;
	smallint kll = 0;
	int verbose = 0;
	int (*act)(const char*);
	int (*cbk)(const char*);
	int curdir;

	INIT_G();

	xfunc_error_retval = 100;

	x = getenv("SVDIR");
	if (x) varservice = x;
	x = getenv("SVWAIT");
	if (x) waitsec = xatou(x);

	opt_complementary = "w+:vv"; /* -w N, -v is a counter */
	getopt32(argv, "w:v", &waitsec, &verbose);
	argv += optind;
	action = *argv++;
	if (!action || !*argv) bb_show_usage();

	tnow = time(NULL) + 0x400000000000000aULL;
	tstart = tnow;
	curdir = open(".", O_RDONLY|O_NDELAY);
	if (curdir == -1)
		fatal_cannot("open current directory");

	act = &control;
	acts = "s";
	cbk = &check;

	switch (*action) {
	case 'x':
	case 'e':
		acts = "x";
		if (!verbose) cbk = NULL;
		break;
	case 'X':
	case 'E':
		acts = "x";
		kll = 1;
		break;
	case 'D':
		acts = "d";
		kll = 1;
		break;
	case 'T':
		acts = "tc";
		kll = 1;
		break;
	case 'c':
		if (str_equal(action, "check")) {
			act = NULL;
			acts = "c";
			break;
		}
	case 'u': case 'd': case 'o': case 't': case 'p': case 'h':
	case 'a': case 'i': case 'k': case 'q': case '1': case '2':
		action[1] = '\0';
		acts = action;
		if (!verbose) cbk = NULL;
		break;
	case 's':
		if (str_equal(action, "shutdown")) {
			acts = "x";
			break;
		}
		if (str_equal(action, "start")) {
			acts = "u";
			break;
		}
		if (str_equal(action, "stop")) {
			acts = "d";
			break;
		}
		/* "status" */
		act = &status;
		cbk = NULL;
		break;
	case 'r':
		if (str_equal(action, "restart")) {
			acts = "tcu";
			break;
		}
		bb_show_usage();
	case 'f':
		if (str_equal(action, "force-reload")) {
			acts = "tc";
			kll = 1;
			break;
		}
		if (str_equal(action, "force-restart")) {
			acts = "tcu";
			kll = 1;
			break;
		}
		if (str_equal(action, "force-shutdown")) {
			acts = "x";
			kll = 1;
			break;
		}
		if (str_equal(action, "force-stop")) {
			acts = "d";
			kll = 1;
			break;
		}
	default:
		bb_show_usage();
	}

	service = argv;
	while ((x = *service) != NULL) {
		if (x[0] != '/' && x[0] != '.') {
			if (chdir(varservice) == -1)
				goto chdir_failed_0;
		}
		if (chdir(x) == -1) {
 chdir_failed_0:
			fail("can't change to service directory");
			goto nullify_service_0;
		}
		if (act && (act(acts) == -1)) {
 nullify_service_0:
			*service = (char*) -1L; /* "dead" */
		}
		if (fchdir(curdir) == -1)
			fatal_cannot("change to original directory");
		service++;
	}

	if (cbk) while (1) {
		int want_exit;
		int diff;

		diff = tnow - tstart;
		service = argv;
		want_exit = 1;
		while ((x = *service) != NULL) {
			if (x == (char*) -1L) /* "dead" */
				goto next;
			if (x[0] != '/' && x[0] != '.') {
				if (chdir(varservice) == -1)
					goto chdir_failed;
			}
			if (chdir(x) == -1) {
 chdir_failed:
				fail("can't change to service directory");
				goto nullify_service;
			}
			if (cbk(acts) != 0)
				goto nullify_service;
			want_exit = 0;
			if (diff >= waitsec) {
				printf(kll ? "kill: " : "timeout: ");
				if (svstatus_get() > 0) {
					svstatus_print(x);
					++rc;
				}
				bb_putchar('\n'); /* will also flush the output */
				if (kll)
					control("k");
 nullify_service:
				*service = (char*) -1L; /* "dead" */
			}
			if (fchdir(curdir) == -1)
				fatal_cannot("change to original directory");
 next:
			service++;
		}
		if (want_exit) break;
		usleep(420000);
		tnow = time(NULL) + 0x400000000000000aULL;
	}
	return rc > 99 ? 99 : rc;
}
