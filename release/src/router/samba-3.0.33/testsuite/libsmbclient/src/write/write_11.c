#include <stdio.h>
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


void auth_fn(const char *server, const char *share, char *workgroup, int wgmaxlen, 
		char *username, int unmaxlen, char *password, int pwmaxlen)
{

	strncpy(workgroup, g_workgroup, wgmaxlen - 1);

	strncpy(username, g_username, unmaxlen - 1);

	strncpy(password, g_password, pwmaxlen - 1);

	strcpy(g_server, server);
	strcpy(g_share, share);

}

int main(int argc, char** argv)
{
	int err = -1;
	int fd = 0;
	int msg_len = 0;
	char url[MAX_BUFF_SIZE];
	char* message;
	char* response;

	bzero(g_workgroup,MAX_BUFF_SIZE);
	bzero(url,MAX_BUFF_SIZE);

	if ( argc == 6 )
	{
		
		strncpy(g_workgroup,argv[1],strlen(argv[1]));
		strncpy(g_username,argv[2],strlen(argv[2]));
		strncpy(g_password,argv[3],strlen(argv[3]));
		strncpy(url,argv[4],strlen(argv[4]));

		msg_len = strlen(argv[5])+1;
		message = malloc(msg_len);
		response = malloc(msg_len);
		message[msg_len - 1] = 0;	
		strncpy(message,argv[5],msg_len);

		smbc_init(auth_fn, 0);
		smbc_unlink(url);
		fd = smbc_open(url,O_RDWR | O_CREAT, 0666);
		smbc_close(fd);

		fd = smbc_open(url, O_RDWR, 0666);
		smbc_write(fd, message, msg_len);
		smbc_close(fd);

		fd = smbc_open(url, O_RDONLY, 0666);
		smbc_read(fd,response,msg_len);
		smbc_close(fd);

		if ( memcmp ( message, response, msg_len) == 0 )
			err = 0;

		else
			err = 1;

		free(message);
		free(response);
		
	}

	return err;

}

