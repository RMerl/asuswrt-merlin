/*
	bwdpi_check.c for check AiStream Live status
*/

#include <rc.h>
#include <bwdpi.h>

static void check_wred_alive()
{
	debug = nvram_get_int("bwdpi_debug");
	if(debug) dbg("[bwdpi_wred_alive] check_wred_alive...\n");

	int enabled = check_bwdpi_nvram_setting();

	if(enabled){
		if(debug) dbg("[bwdpi_wred_alive] start_wrs and start_data_colld...\n");
		// start wrs
		start_wrs();
		// start data_colld
		start_dc(NULL);
		alarm(60);
	}
}

static int sig_cur = -1;

static void catch_sig(int sig)
{
	sig_cur = sig;
	debug = nvram_get_int("bwdpi_debug");
	
	if (sig == SIGTERM)
	{
		remove("/var/run/bwdpi_wred_check.pid");
		exit(0);
	}
	else if(sig == SIGALRM)
	{
		if(debug) dbg("[bwdpi_wred_alive] SIGALRM ...\n");
		check_wred_alive();
	}
}

int bwdpi_wred_alive_main(int argc, char **argv)
{
	debug = nvram_get_int("bwdpi_debug");
	if(debug) dbg("[bwdpi_wred_alive] starting...\n");

	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp = fopen("/var/run/bwdpi_wred_alive.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGTERM, catch_sig);
	signal(SIGALRM, catch_sig);

	while(1)
	{
		if(debug) dbg("[bwdpi_wred_alive] set alarm 60 secs and pause...\n");
		alarm(60);
		pause();
	}

	return 0;
}
