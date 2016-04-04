/*
	test linked list function
*/

#include <libnt.h>

/* global */
struct list *event_list = NULL;

void NT_DBPrintf(struct list *event_list)
{
	NOTIFY_DATABASE_T *listevent;
	struct listnode *ln;

	LIST_LOOP(event_list, listevent, ln)
	{
		printf("[db_test][\"%ld\", \"%8x\", \"%20s\"][%s]\n", listevent->tstamp, listevent->event, listevent->msg, eInfo_get_eName(listevent->event));
	}
}

int main(int argc, char **argv)
{
	int c;
	char *action = NULL, *t = NULL, *e = NULL, *f = NULL;

	/* initial */
	NOTIFY_DATABASE_T *input = initial_db_input();

	if (argc == 1) return 0;

	while ((c = getopt(argc, argv, "rt:e:f:")) != -1)
	{
		switch(c)
		{
			case 'r':
				action = "read";
				break;
			case 't':
				t = optarg;
				break;
			case 'e':
				e = optarg;
				break;
			case 'f':
				f = optarg;
				break;
			case '?':
				printf("[db_test] option %c has wrong command\n", optopt);
				return 0;
			default:
				break;
		}
	}
	
	printf("[db_test] action=%s, t=%s, e=%s, f=%s\n", action, t, e, f);

	if(t == NULL) input->tstamp = 0;
	else input->tstamp = atoi(t);
	
	if(e == NULL) input->event = 0;
	else input->event = strtol(e, NULL, 16);

	if(f == NULL) input->flag = 0;
	else input->flag = atoi(f);

	strcpy(input->msg, "");

	printf("[db_test] t=%ld, e=%d, f=%d, m=%s\n", input->tstamp, input->event, input->flag, input->msg);

	/* initial linked list */
	event_list = list_new();

	/* database API */
	NT_DBAction(event_list, action, input);

	/* free input */
	db_input_free(input);

	/* print all linked list */
	NT_DBPrintf(event_list);

	/* free memory */
	NT_DBFree(event_list);

	return 1;
}
