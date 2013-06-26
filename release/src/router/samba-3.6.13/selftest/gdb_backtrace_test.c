/*

add a useful tool to test the gdb_backtrace script

just compile it with
cc -g -o gdb_backtrace_test gdb_backtrace_test.c

and run it in the same directory where your gdb_backtrace script is.

2006 - Stefan Metzmacher <metze@samba.org>

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static const char *prog;

static void sig_fault(int sig)
{
	int ret;
	char cmdstr[200];

	snprintf(cmdstr, sizeof(cmdstr),
		 "./gdb_backtrace %u %s",
		 getpid(), prog);
	printf("sig_fault start: %s\n", cmdstr);
	ret = system(cmdstr);
	printf("sig_fault end: %d\n", ret);
}

int main(int argc, const char **argv)
{
	prog = argv[0];

	signal(SIGABRT, sig_fault);

	abort();
	return 0;
}
