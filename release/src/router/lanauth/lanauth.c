/*
 * lanauth client
 * (c) visir
 * (c) 2009 theMIROn
 * "THE BEER-WARE LICENSE" (Revision 43):
 * <visir@telenet.ru> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.                    
 * compile: gcc -O2 -Wall -s -o lanauth lanauth.c
 * run: ./lanauth -p yourpassword
 */

#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<signal.h>
#include<errno.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/time.h>
#include<sys/socket.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<string.h>
#include<syslog.h>

#define LTC_NO_CIPHERS
#define LTC_NO_MODES
#define LTC_NO_HASHES
#define LTC_NO_MACS
#define LTC_NO_PRNGS
#define LTC_NO_PK
#define LTC_NO_PKCS
#define LTC_NO_TEST
#define MD5
#define RIPEMD160
#include<tomcrypt.h>

static int nodaemon = 0;		/* don't become daemon */
static int nobind = 1;			/* don't really bind, just assume we binded */
static int nolip = 0;			/* don't touch localip */
static int ver = 2;			/* protocol version */
static char level = 2;			/* access level */
static int sock = -1;			/* socket */
static char localip[17] = "";		/* local ip adress */
static char gateip[17] = "10.0.0.1";	/* gateway ip adress */
static char pass[21] = "";		/* password */
static unsigned char challenge[256], digest[256];

static void opensock();		/* connect to server */
static void auth1();			/* generate response for v1 protocol */
static void auth2();			/* generate response for v2 protocol */
static void sigusr(int sig);		/* change access level */
static int tmread(unsigned char *buf, int size, int timeout);	/* read with timeout */

static void usage() {
	printf("Usage: lanauth [-i] [-v 1|2] [-b localip] [-n] [-g gid] [-u uid] [-s gateip] [-l accesslevel] -p password\n");
	exit(0);
}

static void fatal(char *s, ...) {
	va_list ap;
	va_start(ap, s);
	vsyslog(LOG_ERR, s, ap);
	va_end(ap);
	exit(1);
}

int main(int argc, char **argv) {
	char *s;
	int op;
	unsigned char ch;
	unsigned char curlevel = 0xff;
	/* process command line arguments */
	while(1) {
		op = getopt(argc, argv, "iv:b:ns:p:l:u:g:");
		if (op == -1) break;
		switch(op) {
			case 'i':
				nodaemon = 1;
				break;
			case 'v':
				ver = atoi(optarg);
				if (ver != 1 && ver != 2) usage();
				break;
			case 'b':
				strncpy(localip, optarg, sizeof(localip)-1);
				localip[sizeof(localip)-1] = 0;
				nobind = 0;
				nolip = 1;
				break;
			case 'n':
				nobind = 1;
				if (!nolip) usage();
				break;
			case 's':
				strncpy(gateip, optarg, sizeof(gateip)-1);
				gateip[sizeof(gateip)-1] = 0;
				break;
			case 'p':
				strncpy(pass, optarg, sizeof(pass)-1);
				pass[sizeof(pass)-1] = 0;
				/* hide password */
				for (s = optarg; *s; s++) *s = '*';
				break;
			case 'l':
				level = atoi(optarg);
				break;
			case 'u':
				setuid(atoi(optarg));
				break;
			case 'g':
				setgid(atoi(optarg));
				break;
			default:
				usage();
		}
	}

	if (!*pass) usage();

	openlog("lanauth", nodaemon ? LOG_PERROR|LOG_PID : LOG_PID, LOG_DAEMON);

	signal(SIGUSR1, sigusr);
	signal(SIGUSR2, sigusr);
	if (!nodaemon) {
		signal(SIGPIPE, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
		daemon(0, 0);
	}

	/* main loop */
	while(1) {
		curlevel = 0xff;
		ch = 0;
		/* connect to server */
		opensock();
		/* start conversation */
		if (!tmread(&ch, 1, 10)) {
			close(sock);
			sleep(5);
			continue;
		}
		switch(ch) {
			case 0:	/* access closed */
				syslog(LOG_NOTICE, "access closed for us");
				close(sock);
				sleep(60);
			case 1: /* continue authorization */
				break;
			case 2: /* redirect to real server */
				read(sock, (char*)&ch, 1);
				if(ch < 7 || ch > 15)
				fatal("redirect: invalid gateway lenght %d", ch);
				read(sock, gateip, ch);
				gateip[ch] = 0;
				syslog(LOG_NOTICE, "gate changed to %s", gateip);
				close(sock);
				break;
			default:
				close(sock);
				syslog(LOG_NOTICE, "unknown protocol %d", ch);
				sleep(60);
				break;
		}
		if (ch != 1) continue;
		/* main loop of challenge-response authorization */
		while(1) {
			if (!tmread(challenge, sizeof(challenge), 240)) {
				close(sock);
				if (curlevel == 0xff) sleep(5);
				break;
			}
			switch(ver) {
				case 1: auth1(); break;
				case 2: auth2(); break;
			}
			write(sock, digest, (ver == 1) ? 16 : 256);
			if (!tmread(&ch, 1, 10)) {
				close(sock);
				syslog(LOG_NOTICE, "auth unsuccessful");
				sleep((curlevel == 0xff) ? 60 : 5);
				break;
			}
			if (curlevel != ch) {
				curlevel = ch;
				syslog(LOG_NOTICE, "auth successful, access level = %d", curlevel);
			}
		}
	}
}

static void sigusr(int sig) {
	switch(sig) {
		case SIGUSR1: level = 1; break;
		case SIGUSR2: level = 2; break;
	}
}

static void opensock() {
	struct sockaddr_in sin;
	socklen_t len;
	while (1) {
		/* create socket */
		if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		    fatal("socket: %m");

		/* bind it to specified ip, if needed */
		if (*localip && !nobind) {
			bzero(&sin, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = inet_addr(localip);
			sin.sin_port = 0;
			if(sin.sin_addr.s_addr == INADDR_NONE)
				fatal("%s: invalid ip address", localip);
			if(bind(sock, (struct sockaddr*)&sin, sizeof(sin)) == -1)
				fatal("bind: %m");
		}
	
		/* connect to server */
		bzero(&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(gateip);
		sin.sin_port = htons((ver == 1) ? 8899 : 8314);
		if (sin.sin_addr.s_addr == INADDR_NONE)
			fatal("%s: invalid ip address", gateip);

		if (connect(sock, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
			syslog(LOG_NOTICE, "connect: %m");
			sleep(30);
			close(sock);
			continue;
		}
		break;
	}

	/* save localip, if needed */
	len = sizeof(sin);
	getsockname(sock, (struct sockaddr*)&sin, &len);
	if (!nolip)
		strcpy(localip, inet_ntoa(sin.sin_addr));
	syslog(LOG_NOTICE, "connected, gateway %s, local ip %s", gateip, localip);
}

static int tmread(unsigned char *buf, int size, int timeout) {
	fd_set fds;
	struct timeval tv;

	/* wait */
	FD_ZERO(&fds);
	FD_SET(sock, &fds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	int n = select(sock+1, &fds, NULL ,NULL, &tv);
	if (n < 0 && errno == EINTR) {
		syslog(LOG_DEBUG, "reconnecting by request");
		return 0;
	}
	if (n < 0) fatal("select: %m");
	if (!n) {
		syslog(LOG_DEBUG, "no response from server, reconnecting");
		return 0;
	}
	/* wait a moment */
	if (size > 1) {
		tv.tv_sec = 3;
		tv.tv_usec = 0;
		select(0, NULL, NULL, NULL, &tv);
	}
	/* read */
	n = read(sock, buf, size);
	if (n < 0) {
		syslog(LOG_DEBUG, "read: %m");
		return 0;
	}
	if (!n) {
		syslog(LOG_DEBUG, "server closed connection");
		return 0;
	}
	return n;
}

static void auth1() {
	hash_state ctx;
	md5_init(&ctx);
	md5_process(&ctx, challenge+1, challenge[0]);
	md5_process(&ctx, (unsigned char *)localip, strlen(localip));
	level += '0';
	md5_process(&ctx, (unsigned char *)&level, 1);
	level -= '0';
	md5_process(&ctx, (unsigned char *)pass, strlen(pass));
	md5_done(&ctx, digest);
}

static void auth2() {
	hash_state ctx;
	int i;
	for(i = 0; i < 255; i++)digest[i] = rand() % 256;
	digest[0] = level - 1;
	digest[1] = 2 + rand() % 230;
	rmd160_init(&ctx);
	rmd160_process(&ctx, challenge+1, challenge[0]);
	rmd160_process(&ctx, (unsigned char *)pass, strlen(pass));
	rmd160_done(&ctx, digest+digest[1]);
}
