/*
	bwdpi_check.c for check AiStream Live status
*/

#include <rc.h>
#include <bwdpi.h>

static int count = 0; // real count

void stop_bwdpi_check()
{
	eval("killall", "-9", "bwdpi_check");
}

void start_bwdpi_check()
{
	char *bwdpi_check_argv[] = {"bwdpi_check", NULL};
	pid_t pid;

	_eval(bwdpi_check_argv, NULL, 0, &pid);
}

static void run_engine()
{
	if(!f_exists("/dev/detector") || !f_exists("/dev/idpfw")){
		run_dpi_engine_service();
	}

	if(!f_exists("/tmp/bwdpi/bwdpi.app.db")){
		stop_dpi_engine_service();
	}
}

static void check_dpi_alive()
{
	int enabled = 0;
	debug = nvram_get_int("bwdpi_debug");

	// check no qos service
	if(nvram_get_int("wrs_enable") == 0 && nvram_get_int("wrs_app_enable") == 0 && 
		nvram_get_int("wrs_vp_enable") == 0 && nvram_get_int("wrs_cc_enable") == 0 &&
		nvram_get_int("qos_enable") == 0)
		enabled = 1;

	// check traditional qos service
	if(nvram_get_int("wrs_enable") == 0 && nvram_get_int("wrs_app_enable") == 0 && 
		nvram_get_int("wrs_vp_enable") == 0 && nvram_get_int("wrs_cc_enable") == 0 &&
		nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		enabled = 1;

	if(debug) dbg("[bwdpi check] enabled= %d\n", enabled);

	if(enabled)
	{
		if(debug) dbg("[bwdpi check] count=%2d\n", count);
		run_engine();

		// Re-apply rules flushed by closed-source bwdpi code
		start_firewall(wan_primary_ifunit(), 0);

		count -= 3;
		if(count <= 0){
			stop_dpi_engine_service();
			pause();
		}
		else 
			alarm(3);
	}
}

static int sig_cur = -1;

static void catch_sig(int sig)
{
	sig_cur = sig;
	
	if(sig == SIGUSR1)
	{
		//dbg("[bwdpi check] reset alive count...\n");
		count = nvram_get_int("bwdpi_alive");
		check_dpi_alive();
	}
	else if(sig == SIGALRM)
	{
		//dbg("[bwdpi check] check alive...\n");
		check_dpi_alive();
	}
	else if (sig == SIGTERM)
	{
		//dbg("[bwdpi check] killall...\n");
		remove("/var/run/bwdpi_check.pid");
		exit(0);
	}
}

int bwdpi_check_main(int argc, char **argv)
{
	dbg("[bwdpi check] starting...\n");

	FILE *fp;
	sigset_t sigs_to_catch;

	/* write pid */
	if ((fp = fopen("/var/run/bwdpi_check.pid", "w")) != NULL)
	{
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	/* set the signal handler */
	sigemptyset(&sigs_to_catch);
	sigaddset(&sigs_to_catch, SIGTERM);
	sigaddset(&sigs_to_catch, SIGUSR1);
	sigaddset(&sigs_to_catch, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &sigs_to_catch, NULL);

	signal(SIGTERM, catch_sig);
	signal(SIGUSR1, catch_sig);
	signal(SIGALRM, catch_sig);

	while(1)
	{
		pause();
	}

	return 0;
}
