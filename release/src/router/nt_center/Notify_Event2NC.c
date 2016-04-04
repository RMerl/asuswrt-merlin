#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libnt.h>

int main(int argc, char *argv[]) {
	
	
	if(argc < 2) {
		printf("Parameter Num Error.\nUsage: %s \"EVENT ID\" \"Message\"\n", argv[0]);
		return 0;
	}
	
	/* Do Initial first */
	NOTIFY_EVENT_T *event_t = initial_nt_event();
	
	/* Input event data */
	event_t->event = strtol(argv[1], NULL, 16);
	
	if(argv[2] != NULL) {
		snprintf(event_t->msg, sizeof(event_t->msg), "%s", argv[2]);
	} else {
		snprintf(event_t->msg, sizeof(event_t->msg), "%s", "");
	}
	
	/* Send Event to NC */
	send_trigger_event(event_t);
	
	/* Free malloc via initial_nt_event() */
	nt_event_free(event_t);
	
	return 0;
}
