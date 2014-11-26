#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <shutils.h>
#include <shared.h>
#include <bcmnvram.h>

#include <unistd.h>
#include <syslog.h>
#include <string.h>

#define SEND_CONTENT	"/tmp/send.tmp"
#define EMAIL_CONF	"/tmp/email.conf"

int sendm_main(int argc, char **argv)
{
	FILE *fp;
	char *np, *ns, *mta, *mport, *myname, *mymail, *auth, *auth_user, *auth_pass, *to_mail, cmd_buf[128];
	
	mta = nvram_safe_get("smtp_gw");
	mport = nvram_safe_get("smtp_port");
	myname = nvram_safe_get("myname");
	mymail = nvram_safe_get("mymail");
	auth = nvram_get_int("smtp_auth")==1?"LOGIN":"PLAIN";
	auth_user = nvram_safe_get("auth_user");
	auth_pass = nvram_safe_get("auth_pass");
	to_mail = nvram_safe_get("to_mail");

	if ((fp = fopen(EMAIL_CONF, "w+")) != NULL) {
		fprintf(fp, "SMTP_SERVER = '%s'\n", mta);
		fprintf(fp, "SMTP_PORT = '%s'\n", mport);
		fprintf(fp, "MY_NAME = '%s'\n", myname);
		fprintf(fp, "MY_EMAIL = '%s'\n", mymail);
		fprintf(fp, "USE_TLS = 'true'\n");
		fprintf(fp, "SMTP_AUTH = '%s'\n", auth);
		fprintf(fp, "SMTP_AUTH_USER = '%s'\n", auth_user);
		fprintf(fp, "SMTP_AUTH_PASS = '%s'\n", auth_pass);
	}
	fclose(fp);

	np = nvram_safe_get("sendmsg");
	ns = nvram_safe_get("sendsub");

	if(!np || !*np)
		np = "no report";
	if(!ns || !*ns)
		ns = "report";

	if((fp = fopen(SEND_CONTENT, "w+")) != NULL){
		fprintf(fp, "%s", np);
	}
	fclose(fp);

	memset(cmd_buf, 0, sizeof(cmd_buf));
	sprintf(cmd_buf, "cat %s | email -c %s -s '%s' '%s'", SEND_CONTENT, EMAIL_CONF, ns, to_mail);

	return system(cmd_buf);
}
