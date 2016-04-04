/*
	NOTIFY_DATABASE_T *input;
	notification center database
	- command API
	- write / read / delete
*/

#include <libnt.h>

#define MyDBG(fmt,args...) \
	if(isFileExist(NOTIFY_DB_DEBUG) > 0) { \
		Debug2Console("[nt_db][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

static void show_help()
{
	printf("Usage :\n");
	printf("  nt_db -[w/r/d], option : t/e/f/m\n");
	printf("  nt_db -w -t [timestamp] -e [event] -f [flag] -m [msg]\n");
	printf("  nt_db -r -t [timestamp] -e [event] -f [flag] -m [msg]\n");
	printf("  nt_db -d -t [timestamp] -e [event] -f [flag] -m [msg]\n");
}

int main(int argc, char **argv)
{
	int c;
	char *action = NULL, *t = NULL, *e = NULL, *f = NULL, *m = NULL;

	/* initial */
	NOTIFY_DATABASE_T *input = initial_db_input();

	if (argc == 1){
		show_help();
		return 0;
	}

	while ((c = getopt(argc, argv, "wrdt:e:f:m:")) != -1)
	{
		switch(c)
		{
			case 'w':
				action = "write";
				break;
			case 'r':
				action = "read";
				break;
			case 'd':
				action = "delete";
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
			case 'm':
				m = optarg;
				break;
			case '?':
				printf("[nt_db] option %c has wrong command\n", optopt);
				return -1;
			default:
				show_help();
				break;
		}
	}

	if(t == NULL) input->tstamp = 0;
	else input->tstamp = atoi(t);
	
	if(e == NULL) input->event = 0;
	else input->event = strtol(e, NULL, 16);

	if(f == NULL) input->flag = 0;
	else input->flag = atoi(f);

	if(m == NULL) strcpy(input->msg, "");
	else strcpy(input->msg, m);

	//printf("[nt_db] t=%ld, e=%d, f=%d, m=%s\n", input->tstamp, input->event, input->flag, input->msg);
	
	/* call API */
	NT_DBCommand(action, input);

	/* free input */
	db_input_free(input);

	return 1;
}
