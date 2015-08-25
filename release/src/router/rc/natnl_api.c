
#include "rc.h"
#ifdef RTCONFIG_TUNNEL
void start_aae()
{
	if( getpid()!=1 ) {
		notify_rc("start_aae");
		return;
	}

	if ( !pids("aaews" )){
		nvram_set("aae_enable", "1");
		system("aaews &");
		logmessage("AAE", "AAE Service is started");
	}
}

void stop_aae()
{
	if( getpid()!=1 ) {
		notify_rc("stop_aae");
		return;
	}
	
	nvram_set("aae_enable","0");	
	killall_tk("aaews");
	logmessage("NAT Tunnel", "AAE Service is stopped");
}

void start_mastiff()
{
	stop_aae();
	
	if( getpid()!=1 ) {
		notify_rc("start_mastiff");
		return;
	}

	if ( !pids("mastiff" )){
		system("mastiff &");
		logmessage("AAE", "AAE Service is started");
		//start_aae();
	}

}

void stop_mastiff()
{
	if( getpid()!=1 ) {
		notify_rc("stop_mastiff");
		return;
	}
	
	killall_tk("mastiff");
	logmessage("NAT Tunnel", "AAE Service is stopped");
	
	stop_aae();
}
#endif
