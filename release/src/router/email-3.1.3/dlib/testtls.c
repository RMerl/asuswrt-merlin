#include <stdio.h>

#include <dstring.h>
#include <dnet.h>

void
getSmtpCommands(dsocket *sd)
{
	int err;
	size_t wlen;
	char wbuf[4096]={0}, rbuf[4096]={0};

	while (1) {
		printf("smtp> ");
		fgets(wbuf, sizeof(wbuf), stdin);
		if (strncasecmp(wbuf, "quit", 4) == 0) {
			break;
		}
		wlen = strlen(wbuf);
		wbuf[wlen-1] = '\0';
		strncat(wbuf, "\r\n", sizeof(wbuf));
		wlen += 1;
		if (dnetWrite(sd, wbuf, wlen) == -1) {
			printf("SSL Write error.\n\t");
			break;
		}
		err = dnetRead(sd, rbuf, sizeof(rbuf)-1);
		if (err == -1) {
			printf("dnetRecv Error.\n");
			break;
		}
		rbuf[err] = '\0';
		printf("<-- %s\n", rbuf);
		memset(rbuf, '\0', sizeof(rbuf));
		memset(wbuf, '\0', sizeof(wbuf));
	}
}

int main(int argc, char **argv)
{
	dsocket *sd=NULL;
	char *put=NULL;
	char *server=NULL;
	uint port;
	char buf[4096] = {0};

	if (argc != 3) {
		printf("%s server port\n", argv[0]);
		exit(1);
	} else {
		server = argv[1];
		port = atoi(argv[2]);
	}

	// The next function doesn't have to be called unless we want
	// to specify our own get password function. Otherwise OpenSSL
	// will prompt the user on the cmd line.
	// SSL_CTX_set_default_passwd_cb(ctx, getPassword);
	
	/* END Init SSL/TLS */

	sd = dnetConnect(server, port);
	if (!sd) {
		perror("Coulnd't connect to server.");
		return 1;
	}

	dnetRead(sd, buf, sizeof(buf)-1);
	printf("<-- %s", buf);
	put = "EHLO snaghosting.com\r\n";
	dnetWrite(sd, put, strlen(put));
	printf("--> %s", put);
	memset(buf, '\0', sizeof(buf));
	dnetRead(sd, buf, sizeof(buf)-1);
	printf("<-- %s\n", buf);
	put = "STARTTLS\r\n";
	dnetWrite(sd, put, strlen(put));
	printf("--> %s", put);
	memset(buf, '\0', sizeof(buf));
	dnetRead(sd, buf, sizeof(buf)-1);
	printf("--> %s", buf);
	memset(buf, '\0', sizeof(buf));

	if (dnetUseTls(sd) == ERROR) {
		printf("TLS Connection failed miserably.\n");
		dnetClose(sd);
		return 1;
	}
	if (dnetVerifyCert(sd) == ERROR) {
		printf("Couldn't verify peer certificate.\n");
		dnetClose(sd);
		return 1;
	}
	/* Do cert verification stuff here. */

	getSmtpCommands(sd);
	dnetWrite(sd, "QUIT\r\n", 6);
	dnetClose(sd);

	return 0;
}

