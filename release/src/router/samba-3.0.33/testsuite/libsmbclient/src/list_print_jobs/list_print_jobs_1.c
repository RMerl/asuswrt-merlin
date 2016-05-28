#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libsmbclient.h>

#define	MAX_BUFF_SIZE	255
char g_workgroup[MAX_BUFF_SIZE];
char g_username[MAX_BUFF_SIZE];
char g_password[MAX_BUFF_SIZE];
char g_server[MAX_BUFF_SIZE];
char g_share[MAX_BUFF_SIZE];

char g_print_user[MAX_BUFF_SIZE];
char g_print_name[MAX_BUFF_SIZE];
unsigned int g_print_id;
unsigned int g_print_priority;
unsigned int g_print_size;

int call_back_flag;
int print_queue_empty;

void auth_fn(const char *server, const char *share, char *workgroup, int wgmaxlen, 
		char *username, int unmaxlen, char *password, int pwmaxlen)
{

	strncpy(workgroup, g_workgroup, wgmaxlen - 1);

	strncpy(username, g_username, unmaxlen - 1);

	strncpy(password, g_password, pwmaxlen - 1);

	strcpy(g_server, server);
	strcpy(g_share, share);

}

void print_list_fn_2(struct print_job_info *pji)
{
	print_queue_empty = 0;
	g_print_id = pji->id;
}

void print_list_fn(struct print_job_info *pji)
{

	call_back_flag = 1;

	g_print_id = pji->id;
	g_print_priority = pji->priority;
	g_print_size = pji->size;
	strcpy(g_print_user,pji->user);
	strcpy(g_print_name,pji->name);

	/* fprintf(stdout, "Print job: ID: %u, Prio: %u, Size: %u, User: %s, Name: %s\n",
			          pji->id, pji->priority, pji->size, pji->user, pji->name); */
	
}

int main(int argc, char** argv)
{

	int err = -1;
	char url[MAX_BUFF_SIZE];

	bzero(g_workgroup,MAX_BUFF_SIZE);
	bzero(url,MAX_BUFF_SIZE);
	bzero(g_print_user,MAX_BUFF_SIZE);
	bzero(g_print_name,MAX_BUFF_SIZE);

	g_print_id = 0;
	g_print_priority = 0;
	g_print_size = 0;
	call_back_flag = 0;
	print_queue_empty = 0;

	if ( argc == 5 )
	{
		
		strncpy(g_workgroup,argv[1],strlen(argv[1]));
		strncpy(g_username,argv[2],strlen(argv[2]));
		strncpy(g_password,argv[3],strlen(argv[3]));
		strncpy(url,argv[4],strlen(argv[4]));

		smbc_init(auth_fn, 0);

		while ( ! print_queue_empty ) /* Wait until the queue is empty */
		{
			sleep(1);
			print_queue_empty = 1;
	 		smbc_list_print_jobs(url,print_list_fn_2);
		}

	 	smbc_list_print_jobs(url,print_list_fn);

		if ( call_back_flag )

			err = 0;

		else
			err = 1;
		
	}

	return err;

}

