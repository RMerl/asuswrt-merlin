#include <stdio.h>
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
	int fd1 = 0;
	int fd2 = 0;
	char url[MAX_BUFF_SIZE];

	bzero(g_workgroup,MAX_BUFF_SIZE);
	bzero(url,MAX_BUFF_SIZE);

	if ( argc == 7 )
	{
		strncpy( g_workgroup, argv[1], strlen(argv[1]) );
		strncpy( g_username, argv[2], strlen(argv[2]) );
		strncpy( g_password, argv[3], strlen(argv[3]) );
		strncpy( url, argv[4], strlen(argv[4]) );

		smbc_init( auth_fn, 0 );
		fd1 = smbc_open( url, O_RDWR | O_CREAT, 0666 );
		smbc_close( fd1 );	

		smbc_mkdir( argv[5], 0666 );
		smbc_rename( url, argv[6] );

		fd1 = smbc_open( url, O_RDWR, 0666 );
		fd2 = smbc_open( argv[6], O_RDWR, 0666 );

		if ( fd1 == -1 && fd2 != -1 )
			err = 0;

		else
			err = 1;	


	}

	return err;

}

