/* $Id: testifacewatcher.c,v 1.1 2011/05/20 09:34:25 nanard Exp $ */

#include <syslog.h>

int
OpenAndConfInterfaceWatchSocket(void);

void
ProcessInterfaceWatchNotify(int s);

const char * ext_if_name;
volatile int should_send_public_address_change_notif = 0;

int main(int argc, char * * argv)
{
	int s;

	ext_if_name = "ep0";
	openlog("testifacewatcher", LOG_CONS|LOG_PERROR, LOG_USER);

	syslog(LOG_DEBUG, "test");
	s = OpenAndConfInterfaceWatchSocket();
	for(;;) {
		if(should_send_public_address_change_notif) {
			syslog(LOG_DEBUG, "should_send_public_address_change_notif !");
			should_send_public_address_change_notif = 0;
		}
		ProcessInterfaceWatchNotify(s);
	}
	closelog();
	return 0;
}
