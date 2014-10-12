#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
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
	char url[MAX_BUFF_SIZE];
	struct stat st;
	char *user;
	SMBCCTX *ctx;

	bzero(g_workgroup,MAX_BUFF_SIZE);
	bzero(url,MAX_BUFF_SIZE);

	if ( argc == 2)
	{
		char *p;
		user = getenv("USER");
		if (!user) {
			printf("no user??\n");
			return 0;
		}

		printf("username: %s\n", user);

		p = strchr(user, '\\');
		if (! p) {
			printf("BAD username??\n");
			return 0;
		}
		strncpy(g_workgroup, user, strlen(user));
		g_workgroup[p - user] = 0;
		strncpy(g_username, p + 1, strlen(p + 1));
		memset(g_password, 0, sizeof(char) * MAX_BUFF_SIZE);
		strncpy(url,argv[1],strlen(argv[1]));

		err = smbc_init(auth_fn, 10);
		if (err) {
			printf("init smbclient context failed!!\n");
			return err;
		}
		/* Using null context actually get the old context. */
		ctx = smbc_set_context(NULL);
		smbc_setOptionUseKerberos(ctx, 1);
		smbc_setOptionFallbackAfterKerberos(ctx, 1);
		smbc_setWorkgroup(ctx, g_workgroup);
		smbc_setUser(ctx, g_username);
		err = smbc_stat(url, &st);

		if ( err < 0 ) {
			err = 1;
			printf("stat failed!!\n");
		}
		else {
			err = 0;
			printf("stat succeeded!!\n");
		}


	}

	return err;

}
