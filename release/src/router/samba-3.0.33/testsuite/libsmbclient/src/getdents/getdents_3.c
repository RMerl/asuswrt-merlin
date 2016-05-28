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
	int dh = 0; 
	int entry_num = 0;
	int i = 0;
	int j = 0;
	char *file_name;
	char *tmp_file_ptr;
	int direntsize = 0;
	int diramount = 0;

	struct smbc_dirent *dirptr;


	char buff[MAX_BUFF_SIZE];
	char url[MAX_BUFF_SIZE];
	char file_url[MAX_BUFF_SIZE];
	char dir_url[MAX_BUFF_SIZE];
	char dirbuff[MAX_BUFF_SIZE];

	bzero(g_workgroup,MAX_BUFF_SIZE);
	bzero(url,MAX_BUFF_SIZE);
	bzero(file_url,MAX_BUFF_SIZE);
	bzero(dir_url,MAX_BUFF_SIZE);
	bzero(buff,MAX_BUFF_SIZE);

	if ( argc == 6 )
	{

		dirptr = (struct smbc_dirent *) dirbuff;

		strncpy(g_workgroup,argv[1],strlen(argv[1]));
		strncpy(g_username,argv[2],strlen(argv[2]));
		strncpy(g_password,argv[3],strlen(argv[3]));
		strncpy(url,argv[4],strlen(argv[4]));
		smbc_init(auth_fn, 0);

		strncpy(file_url,"tempfile-",9);
		tmp_file_ptr = file_url;
		tmp_file_ptr += 9;

		smbc_rmdir(url);
		smbc_mkdir(url,0666);

		entry_num = atoi(argv[5]);
		strcat(dir_url,url);
		strcat(dir_url,"/");

		file_name = dir_url;
		file_name += strlen(dir_url);

		for ( i = 0; i < entry_num; i++ )
		{
			sprintf(buff,"%d",i);
			memcpy(tmp_file_ptr,buff,strlen(buff)+4);
			strncat(tmp_file_ptr,".txt",4);
			strcpy(file_name,file_url);
			fd = smbc_open(dir_url,O_RDWR | O_CREAT, 0666);
			smbc_close(fd);

		}

		dh = smbc_opendir(url);
		diramount = smbc_getdents( dh, dirptr, sizeof(dirbuff));

		err = 0;
		i = 0;
		bzero(buff,MAX_BUFF_SIZE);
		bzero(tmp_file_ptr,MAX_BUFF_SIZE-9);

		while ( diramount > 0 )
		{
			direntsize = dirptr->dirlen;
			/* printf("Name: %s\n",dirptr->name); */
			if ( j == 0 )
			{
				if ( !(( strncmp(dirptr->name,".",1) == 0 )) ) 
				{
					break;
					err = 1; 
				}

			} else if ( j == 1 ) {	

				if ( !(( strncmp(dirptr->name,"..",2) == 0 )) )
			       	{
					break;
					err = 1; 
				} 
			
			} else if ( j > 1 ) {

					sprintf(buff,"%d",i);
					memcpy(tmp_file_ptr,buff,strlen(buff)+4);
					strncat(tmp_file_ptr,".txt",4);

					if ( !((strcmp(dirptr->name,file_url) == 0 )) ) /* make sure entries match */
					{
						err = 1;
						break;
					}

					i++;

			}

			dirptr = (struct smbc_dirent *) ((char *) dirptr + direntsize);
			diramount -= direntsize;
			j++;

		}
	
		if ( ! err )
		{
			if ( (j - 2) != entry_num ) /* Make sure that all entries created are counted and returned - minus . and .. */
				err = 1;
		}	

	}

	return err;

}

